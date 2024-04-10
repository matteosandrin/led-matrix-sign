#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <Button2.h>
#include <ESP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <sntp.h>
#include <time.h>
#include <Preferences.h>

#include "led-matrix-sign.h"
#include "src/display/display.h"
#include "src/mbta/mbta-api.h"
#include "src/spotify/spotify.h"

AsyncWebServer server(80);
Preferences preferences;
const char *ssid = "OliveBranch2.4GHz";
const char *password = "Breadstick_lover_68";
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
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

void setup_webserver() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    char response[] = R"(
      <!DOCTYPE html/>
      <head>
        <meta name="viewport" content="width=device-width, initial-scale=1" />
        <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
        <meta http-equiv="Content-Type" content="text/html; charset=ISO-8859-1" />
        <style>
          body {
            font-family: system-ui, Roboto, Helvetica;
          }
        </style>
        <title>LED Matrix Display</title>
      </head>
      <body>
        <h1>LED Matrix Display</h1>
        <h2>Set sign mode</h2>
        <ul>
          <li><a href="/mode?id=0">SIGN_MODE_TEST</a></li>
          <li><a href="/mode?id=1">SIGN_MODE_MBTA</a></li>
          <li><a href="/mode?id=2">SIGN_MODE_CLOCK</a></li>
          <li><a href="/mode?id=3">SIGN_MODE_MUSIC</a></li>
        </ul>
        <h2>Set MBTA station</h2>
        <ul>
          <li><a href="/set?key=station&value=0">Alewife</a></li>
          <li><a href="/set?key=station&value=1">Davis</a></li>
          <li><a href="/set?key=station&value=2">Porter</a></li>
          <li><a href="/set?key=station&value=3">Harvard</a></li>
          <li><a href="/set?key=station&value=4">Central</a></li>
          <li><a href="/set?key=station&value=5">Kendall/MIT</a></li>
          <li><a href="/set?key=station&value=6">Charles/MGH</a></li>
          <li><a href="/set?key=station&value=7">Park Street</a></li>
          <li><a href="/set?key=station&value=8">Downtown Crossing</a></li>
          <li><a href="/set?key=station&value=9">South Station</a></li>
        </ul>
      </body>
    )";
    request->send(200, "text/html", response);
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
  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("key") && request->hasParam("value")) {
      String key = request->getParam("key")->value();
      String value = request->getParam("value")->value();

      if (key == "station") {
        int station = value.toInt();
        if (0 <= station < TRAIN_STATION_MAX) {
          UIMessage message;
          message.type = UI_MESSAGE_TYPE_MBTA_CHANGE_STATION;
          message.next_station = (TrainStation)station;
          if (xQueueSend(ui_queue, (void *)&message, TEN_MILLIS)) {
            request->redirect("/");
            return;
          }
        } else {
          request->send(500, "text/plain", "invalid station id: " + value);
        }
      } else {
        request->send(500, "text/plain", "unknown key '" + key + "'");
      }
    }
    request->send(500, "text/plain", "missing query parameter 'id'");
  });
  server.begin();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) continue;
  setup_wifi();
  display.setup();

  display.log("Sync with NTP server");
  setup_time();

  display.log("Refresh Spotify token");
  spotify.setup();

  // Webserver setup
  display.log("Setup webserver");
  setup_webserver();

  // Button setup
  display.log("Setup button");
  button.begin(SIGN_MODE_BUTTON_PIN);
  button.setTapHandler(button_tapped);

  // Preferences setup
  preferences.begin("default");

  // Queue setup
  display.log("Setup RTOS queues");
  ui_queue = xQueueCreate(16, sizeof(UIMessage));
  sign_mode_queue = xQueueCreate(1, sizeof(SignMode));
  render_request_queue = xQueueCreate(32, sizeof(RenderRequest));
  render_response_queue = xQueueCreate(32, sizeof(RenderMessage));

  // Timer setup
  display.log("Setup RTOS timers");
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
  display.log("Setup RTOS tasks");
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
          current_sign_mode = shift_sign_mode(current_sign_mode)
        } else if (ui_message.type == UI_MESSAGE_TYPE_MODE_CHANGE) {
          current_sign_mode = ui_message.next_sign_mode;
        }
        // save new sign mode, then restart
        write_sign_mode(current_sign_mode);
        ESP.restart();
      } else if (ui_message.type == UI_MESSAGE_TYPE_MBTA_CHANGE_STATION) {
        Serial.printf("updating mbta station to %s\n",
                      train_station_to_str(ui_message.next_station));
        mbta.set_station(ui_message.next_station);
        if (current_sign_mode == SIGN_MODE_MBTA) {
          RenderMessage message;
          message.sign_mode = SIGN_MODE_MBTA;
          message.mbta_content.status =
              PREDICTION_STATUS_OK_SHOW_STATION_BANNER;
          strcpy(message.mbta_content.predictions[0].label,
                 train_station_to_str(ui_message.next_station));
          if (xQueueSend(render_response_queue, (void *)&message, TEN_MILLIS)) {
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
    int current_sign_mode = -1;
    if (!xQueuePeek(sign_mode_queue, &current_sign_mode, TEN_MILLIS)) {
      continue;
    }
    RenderMessage message;
    if (xQueueReceive(render_response_queue, &message, TEN_MILLIS)) {
      if (message.sign_mode == current_sign_mode) {
        if (message.sign_mode == SIGN_MODE_TEST) {
          display.render_text_content(message.text_content, display.WHITE);
        } else if (message.sign_mode == SIGN_MODE_MBTA) {
          display.render_mbta_content(message.mbta_content);
        } else if (message.sign_mode == SIGN_MODE_CLOCK) {
          display.render_text_content(message.text_content, display.WHITE);
        } else if (message.sign_mode == SIGN_MODE_MUSIC) {
          display.render_music_content(message.music_content);
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
        PredictionStatus status =
            mbta.get_predictions_both_directions(predictions);
        message.mbta_content.status = status;
        if (status == PREDICTION_STATUS_OK ||
            status == PREDICITON_STATUS_OK_SHOW_ARR_BANNER_SLOT_1 ||
            status == PREDICITON_STATUS_OK_SHOW_ARR_BANNER_SLOT_2 ||
            status == PREDICTION_STATUS_OK_SHOW_STATION_BANNER) {
          message.mbta_content.predictions[0] = predictions[0];
          message.mbta_content.predictions[1] = predictions[1];
        }
        if (xQueueSend(render_response_queue, &message, TEN_MILLIS)) {
          Serial.println(
              "sending mbta render_message to render_response_queue");
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
              "sending music render_message to render_response_queue");
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
  int current_sign_mode = preferences.getInt(SIGN_MODE_KEY, DEFAULT_SIGN_MODE);
  return (SignMode)current_sign_mode;
}

void write_sign_mode(SignMode sign_mode) {
  preferences.putInt(SIGN_MODE_KEY, (int)sign_mode);
}

void start_sign(SignMode current_sign_mode) {
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

char *sign_mode_to_str(SignMode sign_mode) {
  switch (sign_mode) {
    case SIGN_MODE_TEST:
      return "SIGN_MODE_TEST";
    case SIGN_MODE_MBTA:
      return "SIGN_MODE_MBTA";
    case SIGN_MODE_CLOCK:
      return "SIGN_MODE_CLOCK";
    case SIGN_MODE_MUSIC:
      return "SIGN_MODE_MUSIC";
  }
  return "SIGN_MODE_UNKNOWN";
}
