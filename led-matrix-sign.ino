#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <Button2.h>
#include <ESP.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <sntp.h>
#include <time.h>

#include "led-matrix-pins.h"
#include "led-matrix-sign.h"
#include "src/mbta/MBTASans.h"
#include "src/mbta/mbta-api.h"
#include "src/spotify/spotify.h"

MatrixPanel_I2S_DMA *dma_display = nullptr;
uint16_t AMBER = dma_display->color565(255, 191, 0);
uint16_t WHITE = dma_display->color565(255, 255, 255);
uint16_t BLACK = dma_display->color565(0, 0, 0);
uint16_t SPOTIFY_GREEN = dma_display->color565(29, 185, 84);
// Using a GFXcanvas reduces the flicker when redrawing the screen, but uses a
// lot of memory. (160 * 32 * 2 = 10240 bytes)
// https://learn.adafruit.com/adafruit-gfx-graphics-library/minimizing-redraw-flicker
GFXcanvas16 canvas(SCREEN_WIDTH, SCREEN_HEIGHT);

AsyncWebServer server(80);
const char *ssid = "OliveBranch2.4GHz";
const char *password = "Breadstick_lover_68";
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const char *time_zone = "EST5EDT,M3.2.0,M11.1.0";  // TZ_America_New_York

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
  configTzTime(time_zone, ntpServer1, ntpServer2);
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

void setup_display() {
  HUB75_I2S_CFG::i2s_pins _pins = {R1_PIN, G1_PIN,  B1_PIN, R2_PIN, G2_PIN,
                                   B2_PIN, A_PIN,   B_PIN,  C_PIN,  D_PIN,
                                   E_PIN,  LAT_PIN, OE_PIN, CLK_PIN};
  HUB75_I2S_CFG mxconfig(PANEL_RES_X,  // module width
                         PANEL_RES_Y,  // module height
                         PANEL_CHAIN,  // Chain length
                         _pins         // pin mapping
  );

  // This is essential to avoid artifacts on the display
  mxconfig.clkphase = false;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(90);  // 0-255
  dma_display->clearScreen();
}

void display_log(char *message) {
  dma_display->clearScreen();
  dma_display->setCursor(0, 0);
  dma_display->setTextWrap(true);
  dma_display->print(message);
  dma_display->setTextWrap(false);
}

void setup_webserver() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", R"(
        <!DOCTYPE html/>
        <head>
          <title>LED Matrix Display</title>
        </head>
        <body>
          <h1>LED Matrix Display</h1>
          <ul>
            <li><a href="/mode?id=0">SIGN_MODE_TEST</a></li>
            <li><a href="/mode?id=1">SIGN_MODE_MBTA</a></li>
            <li><a href="/mode?id=2">SIGN_MODE_CLOCK</a></li>
            <li><a href="/mode?id=3">SIGN_MODE_MUSIC</a></li>
          </ul>
        </body>
      )");
  });
  server.on("/mode", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("id")) {
      String sign_mode_str = request->getParam("id")->value();
      int sign_mode = sign_mode_str.toInt();
      if (0 <= sign_mode && sign_mode < SIGN_MODE_MAX) {
        UIMessage message;
        message.type = UI_MESSAGE_TYPE_MODE_CHANGE;
        message.next_sign_mode = (SignMode)sign_mode;
        if (xQueueSend(ui_queue, (void *)&message, TEN_MILLIS)) {
          request->redirect("/");
          return;
        }
      }
      request->send(500, "text/plain", "invalid sign mode: " + sign_mode_str);
    }
    request->send(500, "text/plain", "missing query parameter 'id'");
  });
  server.begin();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) continue;
  setup_wifi();
  setup_display();

  display_log("Sync with NTP server");
  setup_time();

  display_log("Refresh Spotify token");
  spotify.setup();

  // Webserver setup
  display_log("Setup webserver");
  setup_webserver();

  // Button setup
  display_log("Setup button");
  button.begin(SIGN_MODE_BUTTON_PIN);
  button.setTapHandler(button_tapped);

  // Queue setup
  display_log("Setup RTOS queues");
  ui_queue = xQueueCreate(16, sizeof(UIMessage));
  sign_mode_queue = xQueueCreate(1, sizeof(SignMode));
  render_request_queue = xQueueCreate(32, sizeof(RenderRequest));
  render_response_queue = xQueueCreate(32, sizeof(RenderMessage));

  // Timer setup
  display_log("Setup RTOS timers");
  mbta_provider_timer_handle =
      xTimerCreate("mbta_provider_timer",
                   5000 / portTICK_PERIOD_MS,  // timer interval in millisec
                   true,  // is an autoreload timer (repeats periodically)
                   NULL, mbta_provider_timer);
  clock_provider_timer_handle =
      xTimerCreate("clock_provider_timer",
                   REFRESH_RATE,  // timer interval in millisec
                   true,  // is an autoreload timer (repeats periodically)
                   NULL, clock_provider_timer);
  music_provider_timer_handle =
      xTimerCreate("music_provider_timer",
                   1000 / portTICK_PERIOD_MS,  // timer interval in millisec
                   true,  // is an autoreload timer (repeats periodically)
                   NULL, music_provider_timer);
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
  display_log("Setup RTOS tasks");
  xTaskCreatePinnedToCore(system_task, "system_task",
                          2048,  // stack size
                          NULL,  // task parameters
                          3,     // task priority
                          &system_task_handle, ESP32_CORE_0);
  xTaskCreatePinnedToCore(render_task, "render_task",
                          4096,  // stack size
                          NULL,  // task parameters
                          2,     // task priority
                          &render_task_handle, ESP32_CORE_1);
  xTaskCreatePinnedToCore(mbta_provider_task, "mbta_provider_task",
                          8192,  // stack size
                          NULL,  // task parameters
                          1,     // task priority
                          &mbta_provider_task_handle, ESP32_CORE_0);
  xTaskCreatePinnedToCore(test_provider_task, "test_provider_task",
                          2048,  // stack size
                          NULL,  // task parameters
                          1,     // task priority
                          &test_provider_task_handle, ESP32_CORE_0);
  xTaskCreatePinnedToCore(clock_provider_task, "clock_provider_task",
                          2048,  // stack size
                          NULL,  // task parameters
                          1,     // task priority
                          &clock_provider_task_handle, ESP32_CORE_0);
  xTaskCreatePinnedToCore(music_provider_task, "music_provider_task",
                          8192,  // stack size
                          NULL,  // task parameters
                          1,     // task priority
                          &music_provider_task_handle, ESP32_CORE_0);

  display_log("Setup DONE!");
}

void loop() {}

// Calculate the cursor position that aligns the given string to the right edge
// of the screen. If the cursor position is left of min_x, then min_x is
// returned instead.
int justify_right(char *str, int char_width, int min_x) {
  int num_characters = strlen(str);
  int cursor_x = SCREEN_WIDTH - (num_characters * char_width);
  return max(cursor_x, min_x);
}

void render_text_content(TextRenderContent content, uint16_t color) {
  Serial.println("Rendering text content");
  canvas.fillScreen(BLACK);
  canvas.setFont(NULL);
  canvas.setTextColor(color);
  canvas.setCursor(0, 0);
  canvas.print(content.text);
  dma_display->drawRGBBitmap(0, 0, canvas.getBuffer(), canvas.width(),
                             canvas.height());
}

void render_mbta_content(MBTARenderContent content) {
  Serial.println("Rendering mbta content");
  canvas.fillScreen(BLACK);
  canvas.setTextSize(1);
  canvas.setTextWrap(false);
  canvas.setTextColor(AMBER);

  if (content.status == PREDICTION_STATUS_OK) {
    Prediction *predictions = content.predictions;
    canvas.setFont(&MBTASans);

    Serial.printf("%s: %s\n", predictions[0].label, predictions[0].value);
    Serial.printf("%s: %s\n", predictions[1].label, predictions[1].value);

    int cursor_x_1 = justify_right(predictions[0].value, 10, PANEL_RES_X * 3);
    canvas.setCursor(0, 15);
    canvas.print(predictions[0].label);
    canvas.setCursor(cursor_x_1, 15);
    canvas.print(predictions[0].value);

    int cursor_x_2 = justify_right(predictions[1].value, 10, PANEL_RES_X * 3);
    canvas.setCursor(0, 31);
    canvas.print(predictions[1].label);
    canvas.setCursor(cursor_x_2, 31);
    canvas.print(predictions[1].value);
  } else {
    canvas.setFont(NULL);
    canvas.setCursor(0, 0);
    canvas.print("Failed to fetch MBTA data");
    Serial.println("Failed to fetch MBTA data");
  }
  dma_display->drawRGBBitmap(0, 0, canvas.getBuffer(), canvas.width(),
                             canvas.height());
}

void render_music_content(MusicRenderContent content) {
  Serial.println("Rendering music content");
  canvas.fillScreen(BLACK);
  canvas.setFont(NULL);
  canvas.setTextColor(SPOTIFY_GREEN);
  canvas.setCursor(0, 0);
  if (content.status == SPOTIFY_RESPONSE_OK) {
    CurrentlyPlaying playing = content.data;
    canvas.println(playing.title);
    canvas.println(playing.artist);
  } else if (content.status == SPOTIFY_RESPONSE_EMPTY) {
    canvas.print("Nothing is playing");
  } else {
    canvas.print("Error querying the spotify API");
  }
  dma_display->drawRGBBitmap(0, 0, canvas.getBuffer(), canvas.width(),
                             canvas.height());
}

void system_task(void *params) {
  SignMode current_sign_mode = SIGN_MODE_MBTA;
  UIMessage initial_message{
      UI_MESSAGE_TYPE_MODE_CHANGE,  // type
      SIGN_MODE_MBTA                // next_sign_mode
  };
  xQueueSend(ui_queue, &initial_message, portMAX_DELAY);

  while (1) {
    UIMessage ui_message;
    if (xQueueReceive(ui_queue, &ui_message, TEN_MILLIS)) {
      // New message from the button queue. This means the button has been
      // pressed
      Serial.println("message received on the ui_queue");
      if (ui_message.type == UI_MESSAGE_TYPE_MODE_SHIFT) {
        current_sign_mode = (SignMode)((current_sign_mode + 1) % SIGN_MODE_MAX);
      } else if (ui_message.type == UI_MESSAGE_TYPE_MODE_CHANGE) {
        current_sign_mode = ui_message.next_sign_mode;
      }
      Serial.printf("system task setting sign mode: %d\n", current_sign_mode);
      // empty all rendering queues
      xQueueReset(render_request_queue);
      xQueueReset(render_response_queue);
      // Notify all other tasks that the sign mode has changed
      xQueueOverwrite(sign_mode_queue, (void *)&current_sign_mode);
      // Stop all provider timers
      if (xTimerIsTimerActive(mbta_provider_timer_handle)) {
        if (xTimerStop(mbta_provider_timer_handle, TEN_MILLIS)) {
          Serial.println("stopping mbta provider timer");
        }
      }
      if (xTimerIsTimerActive(clock_provider_timer_handle)) {
        if (xTimerStop(clock_provider_timer_handle, TEN_MILLIS)) {
          Serial.println("stopping clock provider timer");
        }
      }
      if (xTimerIsTimerActive(music_provider_timer_handle)) {
        if (xTimerStop(music_provider_timer_handle, TEN_MILLIS)) {
          Serial.println("stopping music provider timer");
        }
      }
      // Request new render messages from the appropriate provider
      RenderRequest request{current_sign_mode};
      if (xQueueSend(render_request_queue, (void *)&request, TEN_MILLIS)) {
        Serial.println("sending render_request to render_request_queue");
      }
      if (current_sign_mode == SIGN_MODE_MBTA) {
        if (xTimerReset(mbta_provider_timer_handle, TEN_MILLIS)) {
          Serial.println("starting mbta provider timer");
        }
        // Send placeholder predictions while we wait for the real ones
        RenderMessage message;
        message.sign_mode = SIGN_MODE_MBTA;
        message.mbta_content.status = PREDICTION_STATUS_OK;
        mbta.get_placeholder_predictions(
            (Prediction *)&message.mbta_content.predictions);
        xQueueSend(render_response_queue, (void *)&message, TEN_MILLIS);
      } else if (current_sign_mode == SIGN_MODE_CLOCK) {
        if (xTimerReset(clock_provider_timer_handle, TEN_MILLIS)) {
          Serial.println("starting clock provider timer");
        }
      } else if (current_sign_mode == SIGN_MODE_MUSIC) {
        // Send placeholder music info while we wait for the real info
        RenderMessage message;
        message.sign_mode = SIGN_MODE_MUSIC;
        sprintf(message.text_content.text, "Nothing is playing");
        xQueueSend(render_response_queue, (void *)&message, TEN_MILLIS);
        if (xTimerReset(music_provider_timer_handle, TEN_MILLIS)) {
          Serial.println("starting music provider timer");
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
    int current_sign_mode = -1;
    if (!xQueuePeek(sign_mode_queue, &current_sign_mode, TEN_MILLIS)) {
      continue;
    }
    RenderMessage message;
    if (xQueueReceive(render_response_queue, &message, TEN_MILLIS)) {
      if (message.sign_mode == current_sign_mode) {
        if (message.sign_mode == SIGN_MODE_TEST) {
          render_text_content(message.text_content, WHITE);
        } else if (message.sign_mode == SIGN_MODE_MBTA) {
          render_mbta_content(message.mbta_content);
        } else if (message.sign_mode == SIGN_MODE_CLOCK) {
          render_text_content(message.text_content, WHITE);
        } else if (message.sign_mode == SIGN_MODE_MUSIC) {
          render_music_content(message.music_content);
        }
      } else {
        Serial.println(
            "message.sign_type is different from current_sign_type. "
            "Dropping the render message");
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
    int current_sign_mode = -1;
    if (!xQueuePeek(sign_mode_queue, &current_sign_mode, TEN_MILLIS)) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }
    if (current_sign_mode != SIGN_MODE_TEST) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }
    RenderRequest request;
    if (xQueuePeek(render_request_queue, &request, TEN_MILLIS)) {
      if (request.sign_mode == SIGN_MODE_TEST) {
        xQueueReceive(render_request_queue, &request, TEN_MILLIS);
        RenderMessage message;
        message.sign_mode = SIGN_MODE_TEST;
        strcpy(message.text_content.text, test_text);
        if (xQueueSend(render_response_queue, &message, TEN_MILLIS)) {
          Serial.println(
              "sending test render_message to render_response_queue");
        }
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void mbta_provider_task(void *params) {
  while (1) {
    int current_sign_mode = -1;
    if (!xQueuePeek(sign_mode_queue, &current_sign_mode, TEN_MILLIS)) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }
    if (current_sign_mode != SIGN_MODE_MBTA) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }
    RenderRequest request;
    if (xQueuePeek(render_request_queue, &request, TEN_MILLIS)) {
      if (request.sign_mode == SIGN_MODE_MBTA) {
        xQueueReceive(render_request_queue, &request, TEN_MILLIS);
        RenderMessage message;
        message.sign_mode = SIGN_MODE_MBTA;
        // Two predictions, one for southbound trains and one for northbound
        // trains
        Prediction predictions[2];
        PredictionStatus status = mbta.get_predictions_one_direction(
            predictions, DIRECTION_SOUTHBOUND);
        message.mbta_content.status = status;
        if (status == PREDICTION_STATUS_OK) {
          message.mbta_content.predictions[0] = predictions[0];
          message.mbta_content.predictions[1] = predictions[1];
        }
        if (xQueueSend(render_response_queue, &message, TEN_MILLIS)) {
          Serial.println(
              "sending mbta render_message to render_response_queue");
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
    int current_sign_mode = -1;
    if (!xQueuePeek(sign_mode_queue, &current_sign_mode, TEN_MILLIS)) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }
    if (current_sign_mode != SIGN_MODE_CLOCK) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }
    RenderRequest request;
    if (xQueuePeek(render_request_queue, &request, TEN_MILLIS)) {
      if (request.sign_mode == SIGN_MODE_CLOCK) {
        xQueueReceive(render_request_queue, &request, TEN_MILLIS);
        RenderMessage message;
        message.sign_mode = SIGN_MODE_CLOCK;
        struct tm timeinfo;
        getLocalTime(&timeinfo);
        strftime(message.text_content.text, 128, "%A, %B %d %Y\n%H:%M:%S",
                 &timeinfo);
        if (xQueueSend(render_response_queue, &message, TEN_MILLIS)) {
          Serial.println(
              "sending clock render_message to render_response_queue");
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
    int current_sign_mode = -1;
    if (!xQueuePeek(sign_mode_queue, &current_sign_mode, TEN_MILLIS)) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }
    if (current_sign_mode != SIGN_MODE_MUSIC) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }
    RenderRequest request;
    if (xQueuePeek(render_request_queue, &request, TEN_MILLIS)) {
      if (request.sign_mode == SIGN_MODE_MUSIC) {
        xQueueReceive(render_request_queue, &request, TEN_MILLIS);
        RenderMessage message;
        message.sign_mode = SIGN_MODE_MUSIC;
        CurrentlyPlaying currently_playing;
        SpotifyResponse status =
            spotify.get_currently_playing(&currently_playing);
        message.music_content.status = status;
        if (status == SPOTIFY_RESPONSE_OK) {
          message.music_content.data = currently_playing;
        }
        if (xQueueSend(render_response_queue, &message, TEN_MILLIS)) {
          Serial.println(
              "sending clock render_message to render_response_queue");
        }
      }
    }
  }
}

void mbta_provider_timer(TimerHandle_t timer) {
  // Request new render messages from the appropriate provider
  RenderRequest request{SIGN_MODE_MBTA};
  if (xQueueSend(render_request_queue, (void *)&request, TEN_MILLIS)) {
    Serial.println("sending mbta render_request to render_request_queue");
  }
}

void clock_provider_timer(TimerHandle_t timer) {
  // Request new render messages from the appropriate provider
  RenderRequest request{SIGN_MODE_CLOCK};
  if (xQueueSend(render_request_queue, (void *)&request, TEN_MILLIS)) {
    Serial.println("sending clock render_request to render_request_queue");
  }
}

void music_provider_timer(TimerHandle_t timer) {
  // Request new render messages from the appropriate provider
  RenderRequest request{SIGN_MODE_MUSIC};
  if (xQueueSend(render_request_queue, (void *)&request, TEN_MILLIS)) {
    Serial.println("sending music render_request to render_request_queue");
  }
}

void button_tapped(Button2 &btn) {
  Serial.println("button_tapped function");
  UIMessage message;
  message.type = UI_MESSAGE_TYPE_MODE_SHIFT;
  xQueueSend(ui_queue, (void *)&message, TEN_MILLIS);
}
