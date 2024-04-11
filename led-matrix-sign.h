#include <Button2.h>

#include "common.h"
#include "src/mbta/mbta-api.h"
#include "src/spotify/spotify.h"

#ifndef LED_MATRIX_SIGN_H
#define LED_MATRIX_SIGN_H

QueueHandle_t ui_queue;
QueueHandle_t render_request_queue;
QueueHandle_t render_response_queue;

TaskHandle_t system_task_handle;
TaskHandle_t render_task_handle;
TaskHandle_t test_provider_task_handle;
TaskHandle_t mbta_provider_task_handle;
TaskHandle_t clock_provider_task_handle;
TaskHandle_t music_provider_task_handle;

TimerHandle_t mbta_provider_timer_handle;
TimerHandle_t clock_provider_timer_handle;
TimerHandle_t music_provider_timer_handle;
TimerHandle_t wifi_reconnect_timer_handle;
TimerHandle_t button_loop_timer_handle;

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

void system_task(void *params);
void display_task(void *params);
void test_provider_task(void *params);
void mbta_provider_task(void *params);
void clock_provider_task(void *params);

void button_tapped(Button2 &btn);
void mbta_provider_timer(TimerHandle_t timer);
void clock_provider_timer(TimerHandle_t timer);
void check_wifi_and_reconnect_timer(TimerHandle_t timer);

SignMode shift_sign_mode(SignMode current_sign_mode);
SignMode read_sign_mode();
void write_sign_mode(SignMode sign_mode);
char *sign_mode_to_str(SignMode sign_mode);

#endif /* LED_MATRIX_SIGN_H */
