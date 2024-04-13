#ifndef COMMON_DEFS_H
#define COMMON_DEFS_H

#include "src/mbta/mbta-api.h"

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
#define REFRESH_RATE 17 / portTICK_PERIOD_MS  // 60 FPS
#define SIGN_MODE_BUTTON_PIN 32

#define SIGN_MODE_KEY "sign-mode-key"
#define DEFAULT_SIGN_MODE SIGN_MODE_MBTA

enum SignMode {
  SIGN_MODE_TEST,
  SIGN_MODE_MBTA,
  SIGN_MODE_CLOCK,
  SIGN_MODE_MUSIC,
  SIGN_MODE_MAX
};

enum UIMessageType {
  UI_MESSAGE_TYPE_MODE_CHANGE,  // change to a specified sign mode
  UI_MESSAGE_TYPE_MODE_SHIFT,   // shift to the next available sign mode
  UI_MESSAGE_TYPE_MBTA_CHANGE_STATION,
};

struct UIMessage {
  UIMessageType type;
  SignMode next_sign_mode;
  TrainStation next_station;
};

#endif /* COMMON_DEFS_H */
