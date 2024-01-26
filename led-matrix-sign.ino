#include <ArduinoJson.h>
#include <ESP.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <WiFi.h>

#include "MBTASans.h"
#include "esp32-custom-pin-mapping.h"
#include "mbta-api.h"
#include "sntp.h"
#include "time.h"

#define PANEL_RES_X \
  32  // Number of pixels wide of each INDIVIDUAL panel module.
#define PANEL_RES_Y \
  32                   // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 5  // Total number of panels chained one to another

#define SCREEN_WIDTH PANEL_RES_X *PANEL_CHAIN
#define SCREEN_HEIGHT PANEL_RES_Y

#define SIGN_MODE_MBTA 0
uint8_t SIGN_MODE = SIGN_MODE_MBTA;

// MatrixPanel_I2S_DMA dma_display;
MatrixPanel_I2S_DMA *dma_display = nullptr;
uint16_t AMBER = dma_display->color565(255, 191, 0);

const char *ssid = "OliveBranch2.4GHz";
const char *password = "Breadstick_lover_68";
unsigned long wifi_previous_millis = millis();
unsigned long wifi_check_interval = 30000;  // 30s
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const char *time_zone = "EST5EDT,M3.2.0,M11.1.0";  // TZ_America_New_York

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

  // Module configuration
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
}

void loop() {
  switch (SIGN_MODE) {
    case SIGN_MODE_MBTA:
      mbta_sign_mode_loop();
      break;
    default:
      break;
  }
  check_wifi_and_reconnect();
}

void mbta_sign_mode_loop() {
  Serial.printf("[cycle %d] updating LED matrix\n", cycle);
  cycle++;
  dma_display->setFont(&MBTASans);
  dma_display->setTextSize(1);
  dma_display->setTextWrap(false);
  dma_display->setTextColor(AMBER);

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

    int cursorX_01 =
        min(110, (int)(SCREEN_WIDTH - strlen(predictions[0].value) * 10));
    dma_display->setCursor(0, 15);
    dma_display->print(predictions[0].label);
    dma_display->setCursor(cursorX_01, 15);
    dma_display->print(predictions[0].value);

    int cursorX_02 =
        min(110, (int)(SCREEN_WIDTH - strlen(predictions[1].value) * 10));
    dma_display->setCursor(0, 31);
    dma_display->print(predictions[1].label);
    dma_display->setCursor(cursorX_02, 31);
    dma_display->print(predictions[1].value);
  }
  print_ram_info();
  delay(5000);
  Serial.print("\n\n\n");
}
