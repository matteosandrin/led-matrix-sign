#include <Button2.h>

#include "src/mbta/mbta-api.h"

#ifndef LED_MATRIX_SIGN_H
#define LED_MATRIX_SIGN_H

#define PANEL_RES_X \
  32  // Number of pixels wide of each INDIVIDUAL panel module.
#define PANEL_RES_Y \
  32                   // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 5  // Total number of panels chained one to another

#define SCREEN_WIDTH PANEL_RES_X *PANEL_CHAIN
#define SCREEN_HEIGHT PANEL_RES_Y

#define ESP32_CORE_0 0
#define ESP32_CORE_1 1

#define TEN_MILLIS 10 / portTICK_PERIOD_MS
#define REFRESH_RATE 17 / portTICK_PERIOD_MS // 60 FPS

enum SignMode {
  SIGN_MODE_TEST,
  SIGN_MODE_MBTA,
  SIGN_MODE_CLOCK,
  SIGN_MODE_MAX
};

struct MBTARenderContent {
  PredictionStatus status;
  Prediction predictions[2];
};

struct TextRenderContent {
  char text[128];
};

struct RenderMessage {
  SignMode sign_mode;
  MBTARenderContent mbta_content;
  TextRenderContent text_content;
};

struct RenderRequest {
  SignMode sign_mode;
};

enum UIMessageType {
  UI_MESSAGE_TYPE_MODE_CHANGE,  // change to a specified sign mode
  UI_MESSAGE_TYPE_MODE_SHIFT    // shift to the next available sign mode
};

struct UIMessage {
  UIMessageType type;
  SignMode next_sign_mode;
};

#define SIGN_MODE_BUTTON_PIN 32

void system_task(void *params);
void display_task(void *params);
void test_provider_task(void *params);
void mbta_provider_task(void *params);
void clock_provider_task(void *params);

void button_tapped(Button2 &btn);
void mbta_provider_timer(TimerHandle_t timer);
void clock_provider_timer(TimerHandle_t timer);
void check_wifi_and_reconnect_timer(TimerHandle_t timer);

void web_server_index();
void web_server_mode();

#endif /* LED_MATRIX_SIGN_H */
