#include <ArduinoJson.h>
#include <ESP.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <WiFi.h>

#include "Button2.h"
#include "FreeRTOSConfig.h"
#include "MBTASans.h"
#include "esp32-custom-pin-mapping.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "led-matrix-sign.h"
#include "mbta-api.h"
#include "sntp.h"
#include "time.h"

// MatrixPanel_I2S_DMA dma_display;
MatrixPanel_I2S_DMA *dma_display = nullptr;
uint16_t AMBER = dma_display->color565(255, 191, 0);
uint16_t WHITE = dma_display->color565(255, 255, 255);

const char *ssid = "OliveBranch2.4GHz";
const char *password = "Breadstick_lover_68";
unsigned long wifi_previous_millis = millis();
unsigned long wifi_check_interval = 30000;  // 30s
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const char *time_zone = "EST5EDT,M3.2.0,M11.1.0";  // TZ_America_New_York

Button2 button;
TaskHandle_t system_task_handle;
TaskHandle_t button_task_handle;
TaskHandle_t display_task_handle;
TaskHandle_t test_provider_task_handle;
TaskHandle_t mbta_provider_task_handle;
QueueHandle_t button_queue;
QueueHandle_t sign_mode_queue;
QueueHandle_t render_request_queue;
QueueHandle_t render_response_queue;

int cycle = 0;

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

void check_wifi_and_reconnect() {
  unsigned long current_millis = millis();
  if ((WiFi.status() != WL_CONNECTED) &&
      (current_millis - wifi_previous_millis >= wifi_check_interval)) {
    Serial.println("Wifi disconnected. Attempting to reconnect...");
    WiFi.disconnect();
    WiFi.reconnect();
    wifi_previous_millis = current_millis;
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

void setup() {
  Serial.begin(115200);
  while (!Serial) continue;
  setup_wifi();
  setup_time();

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

  // Display Setup
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(90);  // 0-255
  dma_display->clearScreen();

  // Queue setup
  button_queue = xQueueCreate(32, sizeof(bool));
  sign_mode_queue = xQueueCreate(1, sizeof(SignMode));
  render_request_queue = xQueueCreate(4, sizeof(RenderRequest));
  render_response_queue = xQueueCreate(4, sizeof(RenderMessage));

  // Task setup
  //
  //  * The system task has highest priority (3)
  //  * The button and display tasks have medium priority (2)
  //  * The provider tasks have low priority (1)
  //
  // The mbta_provider_task needs a deeper stack because it passes around a lot
  // of JSON object as function arguments.
  //
  // The display_task has its own reserved core, because I always want the
  // the display to be ready to draw when it receives a new message.
  xTaskCreatePinnedToCore(system_task, "system_task",
                          2048,  // stack size
                          NULL,  // task parameters
                          3,     // task priority
                          &system_task_handle, ESP32_CORE_0);
  xTaskCreatePinnedToCore(button_task, "button_task",
                          1024,  // stack size
                          NULL,  // task parameters
                          2,     // task priority
                          &button_task_handle, ESP32_CORE_0);
  xTaskCreatePinnedToCore(display_task, "display_task",
                          2048,  // stack size
                          NULL,  // task parameters
                          2,     // task priority
                          &display_task_handle, ESP32_CORE_1);
  // xTaskCreatePinnedToCore(mbta_provider_task, "mbta_provider_task",
  //                         8192,  // stack size
  //                         NULL,  // task parameters
  //                         1,     // task priority
  //                         &mbta_provider_task_handle, ESP32_CORE_0);
  xTaskCreatePinnedToCore(test_provider_task, "test_provider_task",
                          2048,  // stack size
                          NULL,  // task parameters
                          1,     // task priority
                          &test_provider_task_handle, ESP32_CORE_0);
}

void loop() {
  // switch (SIGN_MODE) {
  //   case SIGN_MODE_TEST:
  //     test_sign_mode_loop();
  //     break;
  //   case SIGN_MODE_MBTA:
  //     mbta_sign_mode_loop();
  //     break;
  //   default:
  //     break;
  // }
  // int now = millis();
  // int original_sign_mode = SIGN_MODE;
  // while (millis() - now < 5000 && original_sign_mode == SIGN_MODE) {
  //   delay(100);
  // }
  check_wifi_and_reconnect();
}

// Calculate the cursor position that aligns the given string to the right edge
// of the screen. If the cursor position is left of min_x, then min_x is
// returned instead.
int justify_right(char *str, int char_width, int min_x) {
  int num_characters = strlen(str);
  int cursor_x = SCREEN_WIDTH - (num_characters * char_width);
  return max(cursor_x, min_x);
}

void render_text_content(char text[128], uint16_t color) {
  Serial.println("Rendering [0] SIGN_MODE_TEST");
  dma_display->clearScreen();
  dma_display->setFont(NULL);
  dma_display->setTextColor(color);
  dma_display->setCursor(0, 0);
  dma_display->print(text);
}

void render_mbta_sign_mode() {
  Serial.println("Rendering [1] SIGN_MODE_MBTA");
  Serial.printf("[cycle %d] updating LED matrix\n", cycle);
  cycle++;
  dma_display->setFont(&MBTASans);
  dma_display->setTextSize(1);
  dma_display->setTextWrap(false);
  dma_display->setTextColor(AMBER);

  // Two predictions, one for southbound trains and one for northbound trains
  Prediction predictions[2];
  int status = get_mbta_predictions(predictions);

  if (status != PREDICTION_STATUS_OK) {
    Serial.println("Failed to fetch MBTA data");
    dma_display->setCursor(0, 0);
    dma_display->print("mbta api failed.");
  } else {
    Serial.printf("%s: %s\n", predictions[0].label, predictions[0].value);
    Serial.printf("%s: %s\n", predictions[1].label, predictions[1].value);

    dma_display->clearScreen();

    int cursor_x_1 = justify_right(predictions[0].value, 10, PANEL_RES_X * 3);
    dma_display->setCursor(0, 15);
    dma_display->print(predictions[0].label);
    dma_display->setCursor(cursor_x_1, 15);
    dma_display->print(predictions[0].value);

    int cursor_x_2 = justify_right(predictions[1].value, 10, PANEL_RES_X * 3);
    dma_display->setCursor(0, 31);
    dma_display->print(predictions[1].label);
    dma_display->setCursor(cursor_x_2, 31);
    dma_display->print(predictions[1].value);
  }
}

void system_task(void *params) {
  SignMode current_sign_mode = SIGN_MODE_TEST;
  RenderRequest initial_request{current_sign_mode};
  xQueueOverwrite(sign_mode_queue, (void *)&current_sign_mode);
  xQueueSend(render_request_queue, &initial_request, portMAX_DELAY);

  while (1) {
    bool is_button_tapped;
    if (xQueueReceive(button_queue, &is_button_tapped, TEN_MILLIS)) {
      // New message from the button queue. This means the button has been
      // pressed
      Serial.println("message received on the button_queue");
      current_sign_mode = (SignMode)((current_sign_mode + 1) % SIGN_MODE_MAX);
      // Notify all other tasks that the sign mode has changed
      xQueueOverwrite(sign_mode_queue, (void *)&current_sign_mode);
      // Request new render messages from the appropriate provider
      RenderRequest request;
      request.sign_mode = current_sign_mode;
      if (current_sign_mode == SIGN_MODE_TEST) {
        if (xQueueSend(render_request_queue, (void *)&request, TEN_MILLIS)) {
          Serial.println("sending render_request to render_request_queue");
        }
      }
    }
  }
}

void button_task(void *params) {
  button.begin(SIGN_MODE_BUTTON_PIN);
  button.setTapHandler(button_tapped);
  while (1) {
    button.loop();
    vTaskDelay(TEN_MILLIS);
  }
}

void display_task(void *params) {
  SignMode current_sign_mode = SIGN_MODE_TEST;
  while (1) {
    int new_sign_mode = -1;
    if (xQueuePeek(sign_mode_queue, &new_sign_mode, TEN_MILLIS)) {
      if (new_sign_mode != current_sign_mode) {
        Serial.printf("New sign mode message: %d\n", new_sign_mode);
        current_sign_mode = (SignMode)new_sign_mode;
      }
    }
    RenderMessage message;
    if (xQueueReceive(render_response_queue, &message, TEN_MILLIS)) {
      if (message.sign_mode != current_sign_mode) {
        Serial.println(
            "message.sign_type is different from current_sign_type. "
            "Dropping the render message");
      } else {
        if (message.sign_mode == SIGN_MODE_TEST) {
          render_text_content(message.text_content.text, WHITE);
        }
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
    SignMode current_sign_mode = SIGN_MODE_TEST;
    if (xQueuePeek(sign_mode_queue, &current_sign_mode, TEN_MILLIS)) {
      if (current_sign_mode == SIGN_MODE_TEST) {
        RenderRequest request;
        if (xQueuePeek(render_request_queue, &request, TEN_MILLIS)) {
          if (request.sign_mode == SIGN_MODE_TEST) {
            xQueueReceive(render_request_queue, &request, TEN_MILLIS);
            RenderMessage message;
            message.sign_mode = SIGN_MODE_TEST;
            strcpy(message.text_content.text, test_text);
            if (xQueueSend(render_response_queue, &message, TEN_MILLIS)) {
              Serial.println("sending render_message to render_response_queue");
            }
          }
        }
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void button_tapped(Button2 &btn) {
  Serial.println("button_tapped function");
  bool is_button_tapped = true;
  xQueueSend(button_queue, (void *)&is_button_tapped, TEN_MILLIS);
}
