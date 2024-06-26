#include <Button2.h>

#include "common.h"
#include "src/mbta/mbta.h"
#include "src/spotify/spotify.h"

#ifndef LED_MATRIX_SIGN_H
#define LED_MATRIX_SIGN_H

struct ProviderRequest {
  SignMode sign_mode;
};

QueueHandle_t ui_queue;
QueueHandle_t provider_queue;
QueueHandle_t render_queue;

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
TimerHandle_t animation_timer_handle;

void system_task(void *params);
void render_task(void *params);
void test_provider_task(void *params);
void mbta_provider_task(void *params);
void clock_provider_task(void *params);

void button_tapped(Button2 &btn);
void mbta_provider_timer(TimerHandle_t timer);
void clock_provider_timer(TimerHandle_t timer);
void check_wifi_and_reconnect_timer(TimerHandle_t timer);

SignMode shift_sign_mode(SignMode current_sign_mode);
SignMode read_sign_mode();
int write_sign_mode(SignMode sign_mode);
bool draw_jpg_image(int16_t x, int16_t y, uint16_t w, uint16_t h,
                    uint16_t *data);

#endif /* LED_MATRIX_SIGN_H */
