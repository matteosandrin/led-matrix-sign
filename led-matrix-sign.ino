#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <Button2.h>
#include <Esp.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <sntp.h>
#include <time.h>

#include "led-matrix-sign.h"
#include "src/display/animation.h"
#include "src/display/common.h"
#include "src/display/display.h"
#include "src/mbta/mbta.h"
#include "src/server/server.h"
#include "src/spotify/spotify.h"

lms::Server server;
Preferences preferences;
const char *ssid = "OliveBranch2.4GHz";
const char *password = "Breadstick_lover_68";
const char *ntp_server_1 = "north-america.pool.ntp.org";
const char *ntp_server_2 = "time.nist.gov";
const char *time_zone = "EST5EDT,M3.2.0,M11.1.0";  // TZ_America_New_York

SignMode disabled_sign_modes[] = {
    SIGN_MODE_TEST,
    SIGN_MODE_CLOCK,
};

Display display;
Button2 button;
Spotify spotify;
MBTA mbta;

void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.print("RRSI: ");
  Serial.println(WiFi.RSSI());
}

void check_wifi_and_reconnect_timer(TimerHandle_t timer) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wifi disconnected. Attempting to reconnect...");
    WiFi.disconnect();
    WiFi.reconnect();
  }
}

void print_ram_info() {
  float free_heap_percent =
      float(ESP.getFreeHeap()) / float(ESP.getHeapSize()) * 100;
  Serial.printf("Total heap: %u\n", ESP.getHeapSize());
  Serial.printf("Free heap: %u (%.1f%%)\n", ESP.getFreeHeap(),
                free_heap_percent);
}

void setup_time() {
  sntp_servermode_dhcp(1);
  configTzTime(time_zone, ntp_server_1, ntp_server_2);
  Serial.printf("Syncing time with NTP servers...");
  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
    Serial.printf(".");
    delay(1000);
  }
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.printf(".");
    delay(1000);
  }
  Serial.println(" Done!");
  Serial.println("Setting timezone");
  setenv("TZ", time_zone, 1);
  tzset();
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S%z");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) continue;
  setup_wifi();
  display.setup();

  display.log("Sync with NTP server");
  setup_time();

  // Preferences setup
  preferences.begin("default");
  SignMode sign_mode = read_sign_mode();

  // API setup
  if (sign_mode == SIGN_MODE_MBTA) {
    display.log("Setup MBTA API");
    mbta.setup();
  }
  if (sign_mode == SIGN_MODE_MUSIC) {
    display.log("Setup Spotify API");
    spotify.setup();
  }

  // Button setup
  display.log("Setup button");
  button.begin(SIGN_MODE_BUTTON_PIN);
  button.setTapHandler(button_tapped);

  // jpeg setup
  TJpgDec.setJpgScale(2);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(draw_jpg_image);

  // Queue setup
  display.log("Setup RTOS queues");
  ui_queue = xQueueCreate(16, sizeof(UIMessage));
  provider_queue = xQueueCreate(32, sizeof(ProviderRequest));
  render_queue = xQueueCreate(32, sizeof(RenderMessage));

  // Timer setup
  display.log("Setup RTOS timers");
  if (sign_mode == SIGN_MODE_MBTA) {
    mbta_provider_timer_handle =
        xTimerCreate("mbta_provider_timer",
                     5000 / portTICK_PERIOD_MS,  // timer interval in millisec
                     true,  // is an autoreload timer (repeats periodically)
                     NULL, mbta_provider_timer);
  }
  if (sign_mode == SIGN_MODE_CLOCK) {
    clock_provider_timer_handle =
        xTimerCreate("clock_provider_timer",
                     REFRESH_RATE,  // timer interval in millisec
                     true,  // is an autoreload timer (repeats periodically)
                     NULL, clock_provider_timer);
  }
  if (sign_mode == SIGN_MODE_MUSIC) {
    music_provider_timer_handle =
        xTimerCreate("music_provider_timer",
                     1000 / portTICK_PERIOD_MS,  // timer interval in millisec
                     true,  // is an autoreload timer (repeats periodically)
                     NULL, music_provider_timer);
  }
  wifi_reconnect_timer_handle =
      xTimerCreate("wifi_reconnect_timer",
                   30000 / portTICK_PERIOD_MS,  // timer interval in millisec
                   true,  // is an autoreload timer (repeats periodically)
                   NULL, check_wifi_and_reconnect_timer);
  xTimerStart(wifi_reconnect_timer_handle, TEN_MILLIS);
  button_loop_timer_handle =
      xTimerCreate("button_loop_timer",
                   TEN_MILLIS,  // timer interval in millisec
                   true,        // is an autoreload timer (repeats periodically)
                   NULL, [](TimerHandle_t t) { button.loop(); });
  xTimerStart(button_loop_timer_handle, TEN_MILLIS);
  animation_timer_handle =
      xTimerCreate("animation_timer",
                   100 / portTICK_PERIOD_MS,  // timer interval in millisec
                   true,  // is an autoreload timer (repeats periodically)
                   NULL, animation_timer);
  xTimerStart(animation_timer_handle, TEN_MILLIS);

  // Task setup
  //
  //  * The system task has highest priority (3)
  //  * The render task has medium priority (2)
  //  * The provider tasks have low priority (1)
  //
  // The mbta_provider_task needs a deeper stack because it passes around a lot
  // of JSON object as function arguments.
  //
  // The render_task has its own reserved core, because I always want the
  // the display to be ready to draw when it receives a new message.
  display.log("Setup RTOS tasks");
  xTaskCreatePinnedToCore(system_task, "system_task",
                          2048,  // stack size
                          NULL,  // task parameters
                          3,     // task priority
                          &system_task_handle, ESP32_CORE_0);
  xTaskCreatePinnedToCore(render_task, "render_task",
                          8192,  // stack size
                          NULL,  // task parameters
                          2,     // task priority
                          &render_task_handle, ESP32_CORE_1);
  if (sign_mode == SIGN_MODE_MBTA) {
    xTaskCreatePinnedToCore(mbta_provider_task, "mbta_provider_task",
                            8192,  // stack size
                            NULL,  // task parameters
                            1,     // task priority
                            &mbta_provider_task_handle, ESP32_CORE_0);
  }
  if (sign_mode == SIGN_MODE_TEST) {
    xTaskCreatePinnedToCore(test_provider_task, "test_provider_task",
                            2048,  // stack size
                            NULL,  // task parameters
                            1,     // task priority
                            &test_provider_task_handle, ESP32_CORE_0);
  }
  if (sign_mode == SIGN_MODE_CLOCK) {
    xTaskCreatePinnedToCore(clock_provider_task, "clock_provider_task",
                            2048,  // stack size
                            NULL,  // task parameters
                            1,     // task priority
                            &clock_provider_task_handle, ESP32_CORE_0);
  }
  if (sign_mode == SIGN_MODE_MUSIC) {
    xTaskCreatePinnedToCore(music_provider_task, "music_provider_task",
                            8192,  // stack size
                            NULL,  // task parameters
                            1,     // task priority
                            &music_provider_task_handle, ESP32_CORE_0);
  }

  // Webserver setup
  // this needs to happen after the RTOS setup, so we can pass the queue handle
  display.log("Setup webserver");
  SignMode current_sign_mode = read_sign_mode();
  server.setup(current_sign_mode, ui_queue);

  display.log("Setup DONE!");
}

void loop() {}

void system_task(void *params) {
  SignMode current_sign_mode = read_sign_mode();
  start_sign(current_sign_mode);

  while (1) {
    UIMessage ui_message;
    if (xQueueReceive(ui_queue, &ui_message, TEN_MILLIS)) {
      // New message from the ui queue
      Serial.println("message received on the ui_queue");
      // ui message says to change sign mode
      if (ui_message.type == UI_MESSAGE_TYPE_MODE_SHIFT ||
          ui_message.type == UI_MESSAGE_TYPE_MODE_CHANGE) {
        if (ui_message.type == UI_MESSAGE_TYPE_MODE_SHIFT) {
          current_sign_mode = shift_sign_mode(current_sign_mode);
        } else if (ui_message.type == UI_MESSAGE_TYPE_MODE_CHANGE) {
          current_sign_mode = ui_message.next_sign_mode;
        }
        write_sign_mode(current_sign_mode);
        Serial.println("Rebooting ESP32");
        ESP.restart();
      } else if (ui_message.type == UI_MESSAGE_TYPE_MBTA_CHANGE_STATION) {
        Serial.printf("updating mbta station to %s\n",
                      train_station_to_str(ui_message.next_station));
        mbta.set_station(ui_message.next_station);
        if (current_sign_mode == SIGN_MODE_MBTA) {
          RenderMessage message;
          message.type = RENDER_TYPE_MBTA;
          message.content.mbta.status =
              PREDICTION_STATUS_OK_SHOW_STATION_BANNER;
          strcpy(message.content.mbta.predictions[0].label,
                 train_station_to_str(ui_message.next_station));
          if (xQueueSend(render_queue, (void *)&message, TEN_MILLIS)) {
            Serial.println("show updated mbta station on display");
          }
          // manually request a new MBTA frame
          mbta_provider_timer(mbta_provider_timer_handle);
        }
      }
    }
    vTaskDelay(TEN_MILLIS);
  }
}

void render_task(void *params) {
  TickType_t last_wake_time;
  last_wake_time = xTaskGetTickCount();
  while (1) {
    vTaskDelayUntil(&last_wake_time, REFRESH_RATE);
    RenderMessage message;
    if (xQueueReceive(render_queue, &message, TEN_MILLIS)) {
      if (message.type == RENDER_TYPE_MBTA) {
        display.render_mbta_content(message.content.mbta);
      } else if (message.type == RENDER_TYPE_TEXT) {
        display.render_text_content(message.content.text);
      } else if (message.type == RENDER_TYPE_MUSIC) {
        display.render_music_content(message.content.music);
      } else if (message.type == RENDER_TYPE_ANIMATION) {
        display.render_animation_content(message.content.animation);
      } else if (message.type == RENDER_TYPE_CANVAS_TO_DISPLAY) {
        display.render_canvas_to_display();
      }
    }
  }
}

void test_provider_task(void *params) {
  char test_text[] =
      "0123456789\n"
      "abcdefghijklmnopqrstuvwxyz\n"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ\n";
  while (1) {
    ProviderRequest request;
    if (xQueuePeek(provider_queue, &request, TEN_MILLIS)) {
      if (request.sign_mode == SIGN_MODE_TEST) {
        xQueueReceive(provider_queue, &request, TEN_MILLIS);
        RenderMessage message;
        message.type = RENDER_TYPE_TEXT;
        strcpy(message.content.text.text, test_text);
        if (xQueueSend(render_queue, &message, TEN_MILLIS)) {
          Serial.println("sending test render_message to render_queue");
        }
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void mbta_provider_task(void *params) {
  while (1) {
    ProviderRequest request;
    if (xQueuePeek(provider_queue, &request, TEN_MILLIS)) {
      if (request.sign_mode == SIGN_MODE_MBTA) {
        xQueueReceive(provider_queue, &request, TEN_MILLIS);
        RenderMessage message;
        message.type = RENDER_TYPE_MBTA;
        // Two predictions, one for southbound trains and one for northbound
        // trains
        Prediction predictions[2];
        PredictionStatus status =
            mbta.get_predictions_both_directions(predictions);
        message.content.mbta.status = status;
        if (status == PREDICTION_STATUS_OK ||
            status == PREDICITON_STATUS_OK_SHOW_ARR_BANNER_SLOT_1 ||
            status == PREDICITON_STATUS_OK_SHOW_ARR_BANNER_SLOT_2 ||
            status == PREDICTION_STATUS_OK_SHOW_STATION_BANNER) {
          message.content.mbta.predictions[0] = predictions[0];
          message.content.mbta.predictions[1] = predictions[1];
        } else if (status == PREDICTION_STATUS_ERROR_SHOW_CACHED) {
          mbta.get_cached_predictions(message.content.mbta.predictions);
        } else {
          mbta.get_placeholder_predictions(message.content.mbta.predictions);
        }
        if (xQueueSend(render_queue, &message, TEN_MILLIS)) {
          Serial.println("sending mbta render_message to render_queue");
          print_ram_info();
        }
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void clock_provider_task(void *params) {
  TickType_t last_wake_time;
  last_wake_time = xTaskGetTickCount();
  while (1) {
    vTaskDelayUntil(&last_wake_time, REFRESH_RATE);
    ProviderRequest request;
    if (xQueuePeek(provider_queue, &request, TEN_MILLIS)) {
      if (request.sign_mode == SIGN_MODE_CLOCK) {
        xQueueReceive(provider_queue, &request, TEN_MILLIS);
        RenderMessage message;
        message.type = RENDER_TYPE_TEXT;
        struct tm timeinfo;
        getLocalTime(&timeinfo);
        strftime(message.content.text.text, 128, "%A, %B %d %Y\n%H:%M:%S",
                 &timeinfo);
        if (xQueueSend(render_queue, &message, TEN_MILLIS)) {
          Serial.println("sending clock render_message to render_queue");
        }
      }
    }
  }
}

void music_provider_task(void *params) {
  TickType_t last_wake_time;
  last_wake_time = xTaskGetTickCount();
  while (1) {
    vTaskDelayUntil(&last_wake_time, REFRESH_RATE);
    ProviderRequest request;
    if (xQueuePeek(provider_queue, &request, TEN_MILLIS)) {
      if (request.sign_mode == SIGN_MODE_MUSIC) {
        xQueueReceive(provider_queue, &request, TEN_MILLIS);
        RenderMessage message;
        message.type = RENDER_TYPE_MUSIC;
        CurrentlyPlaying currently_playing;
        SpotifyResponse status =
            spotify.get_currently_playing(&currently_playing);
        message.content.music.status = status;
        if (status == SPOTIFY_RESPONSE_OK) {
          if (spotify.is_current_song_new(&currently_playing)) {
            // new song is playing. Update animations to show new info
            display.animations.stop_music_animations();
            display.animations.start_music_animations(currently_playing);
            spotify.update_current_song(&currently_playing);
            // fetch new album cover
            Serial.println("fetch new album cover");
            status = spotify.get_album_cover(&currently_playing);
            if (status == SPOTIFY_RESPONSE_OK) {
              TJpgDec.drawJpg(0, 0, spotify.album_cover_jpg,
                              ALBUM_COVER_IMG_BUF_SIZE);
            }
          }
          message.content.music.data = currently_playing;
        } else if (status == SPOTIFY_RESPONSE_OK_SHOW_CACHED) {
          // If nothing is playing but we still have the last song in memory,
          // let's keep showing that song
          message.content.music.data = spotify.get_current_song();
        } else {
          display.animations.stop_music_animations();
          spotify.clear_current_song();
        }
        if (xQueueSend(render_queue, &message, TEN_MILLIS)) {
          Serial.println("sending music render_message to render_queue");
        }
      }
    }
  }
}

void mbta_provider_timer(TimerHandle_t timer) {
  // Request new render messages from the appropriate provider
  ProviderRequest request{SIGN_MODE_MBTA};
  if (xQueueSend(provider_queue, (void *)&request, TEN_MILLIS)) {
    Serial.println("sending mbta provider_request to provider_queue");
  }
}

void clock_provider_timer(TimerHandle_t timer) {
  // Request new render messages from the appropriate provider
  ProviderRequest request{SIGN_MODE_CLOCK};
  if (xQueueSend(provider_queue, (void *)&request, TEN_MILLIS)) {
    Serial.println("sending clock provider_request to provider_queue");
  }
}

void music_provider_timer(TimerHandle_t timer) {
  // Request new render messages from the appropriate provider
  ProviderRequest request{SIGN_MODE_MUSIC};
  if (xQueueSend(provider_queue, (void *)&request, TEN_MILLIS)) {
    Serial.println("sending music provider_request to provider_queue");
  }
}

void animation_timer(TimerHandle_t timer) {
  display.animations.draw(render_queue);
}

void button_tapped(Button2 &btn) {
  Serial.println("button_tapped function");
  UIMessage message;
  message.type = UI_MESSAGE_TYPE_MODE_SHIFT;
  xQueueSend(ui_queue, (void *)&message, TEN_MILLIS);
}

SignMode shift_sign_mode(SignMode current_sign_mode) {
  SignMode next_sign_mode = (SignMode)((current_sign_mode + 1) % SIGN_MODE_MAX);
  for (SignMode s : disabled_sign_modes) {
    if (s == next_sign_mode) {
      return shift_sign_mode(next_sign_mode);
    }
  }
  return next_sign_mode;
}

SignMode read_sign_mode() {
  int sign_mode = preferences.getInt(SIGN_MODE_KEY, DEFAULT_SIGN_MODE);
  if (sign_mode >= 0 && sign_mode < SIGN_MODE_MAX) {
    return (SignMode)sign_mode;
  } else {
    return DEFAULT_SIGN_MODE;
  }
}

int write_sign_mode(SignMode sign_mode) {
  if (sign_mode >= 0 && sign_mode < SIGN_MODE_MAX) {
    preferences.putInt(SIGN_MODE_KEY, (int)sign_mode);
    return 0;
  }
  return 1;
}

void start_sign(SignMode current_sign_mode) {
  if (current_sign_mode == SIGN_MODE_MBTA) {
    if (xTimerReset(mbta_provider_timer_handle, TEN_MILLIS)) {
      Serial.println("starting mbta provider timer");
    }
    // Send placeholder predictions while we wait for the real ones
    RenderMessage message;
    message.type = RENDER_TYPE_MBTA;
    message.content.mbta.status = PREDICTION_STATUS_OK;
    mbta.get_placeholder_predictions(
        (Prediction *)&message.content.mbta.predictions);
    xQueueSend(render_queue, (void *)&message, TEN_MILLIS);
    // jumpstart timer
    mbta_provider_timer(NULL);
  } else if (current_sign_mode == SIGN_MODE_CLOCK) {
    if (xTimerReset(clock_provider_timer_handle, TEN_MILLIS)) {
      Serial.println("starting clock provider timer");
    }
  } else if (current_sign_mode == SIGN_MODE_MUSIC) {
    // Send placeholder music info while we wait for the real info
    RenderMessage message;
    message.type = RENDER_TYPE_MUSIC;
    sprintf(message.content.text.text, "Nothing is playing");
    xQueueSend(render_queue, (void *)&message, TEN_MILLIS);
    if (xTimerReset(music_provider_timer_handle, TEN_MILLIS)) {
      Serial.println("starting music provider timer");
    }
  }
}

bool draw_jpg_image(int16_t x, int16_t y, uint16_t w, uint16_t h,
                    uint16_t *data) {
  if (y >= SCREEN_HEIGHT) return 0;
  display.image_canvas.drawRGBBitmap(x, y, data, w, h);
  return 1;
}
