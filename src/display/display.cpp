#include "display.h"

#include "../fonts/MBTASans.h"
#include "../fonts/Picopixel.h"
#include "display-pins.h"

Display::Display()
    : canvas(SCREEN_WIDTH, SCREEN_HEIGHT),
      scratch_canvas(SCREEN_WIDTH, SCREEN_HEIGHT),
      mask(SCREEN_WIDTH, SCREEN_HEIGHT),
      image_canvas(32, 32) {
  this->AMBER = dma_display->color565(255, 191, 0);
  this->WHITE = dma_display->color565(255, 255, 255);
  this->BLACK = dma_display->color565(0, 0, 0);
  this->SPOTIFY_GREEN = dma_display->color565(29, 185, 84);
}

void Display::setup() {
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

  this->dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  this->dma_display->begin();
  this->dma_display->setBrightness8(90);  // 0-255
  this->dma_display->clearScreen();
  this->animations.setup(&this->canvas);
}

void Display::log(char *message) {
  this->dma_display->clearScreen();
  this->dma_display->setCursor(0, 0);
  this->dma_display->setTextWrap(true);
  this->dma_display->print(message);
  this->dma_display->setTextWrap(false);
}

GFXcanvas16 *Display::get_canvas() { return &this->canvas; }

void Display::render_canvas_to_display() {
  this->dma_display->drawRGBBitmap(0, 0, this->canvas.getBuffer(),
                                   this->canvas.width(), this->canvas.height());
}

Rect Display::get_text_bbox(char *text, int16_t x, int16_t y) {
  int16_t x0, y0;
  uint16_t w0, h0;
  Rect bbox;
  this->canvas.getTextBounds(text, x, y, &bbox.x, &bbox.y, &bbox.w, &bbox.h);
  return bbox;
}

// Calculate the cursor position that aligns the given string to the right edge
// of the screen. If the cursor position is left of min_x, then min_x is
// returned instead.
int Display::justify_right(char *str, int char_width, int min_x) {
  int num_characters = strlen(str);
  int cursor_x = SCREEN_WIDTH - (num_characters * char_width);
  return max(cursor_x, min_x);
}

int Display::justify_center(char *str, int char_width) {
  int num_characters = strlen(str);
  int cursor_x = (SCREEN_WIDTH - (num_characters * char_width)) / 2;
  return cursor_x;
}

void Display::render_text_content(TextRenderContent content) {
  Serial.println("Rendering text content");
  this->canvas.fillScreen(BLACK);
  this->canvas.setFont(NULL);
  this->canvas.setTextColor(content.color);
  this->canvas.setCursor(0, 0);
  this->canvas.print(content.text);
  this->dma_display->drawRGBBitmap(0, 0, this->canvas.getBuffer(),
                                   this->canvas.width(), this->canvas.height());
}

void Display::render_mbta_content(MBTARenderContent content) {
  Serial.println("Rendering mbta content");
  this->canvas.fillScreen(BLACK);
  this->canvas.setTextSize(1);
  this->canvas.setTextWrap(false);
  this->canvas.setTextColor(AMBER);

  if (content.status == PREDICTION_STATUS_OK) {
    Prediction *predictions = content.predictions;
    this->canvas.setFont(&MBTASans);

    Serial.printf("%s: %s\n", predictions[0].label, predictions[0].value);
    Serial.printf("%s: %s\n", predictions[1].label, predictions[1].value);

    int cursor_x_1 = justify_right(predictions[0].value, 10, PANEL_RES_X * 3);
    this->canvas.setCursor(0, 15);
    this->canvas.print(predictions[0].label);
    this->canvas.setCursor(cursor_x_1, 15);
    this->canvas.print(predictions[0].value);

    int cursor_x_2 = justify_right(predictions[1].value, 10, PANEL_RES_X * 3);
    this->canvas.setCursor(0, 31);
    this->canvas.print(predictions[1].label);
    this->canvas.setCursor(cursor_x_2, 31);
    this->canvas.print(predictions[1].value);
  } else if (content.status == PREDICITON_STATUS_OK_SHOW_ARR_BANNER_SLOT_1 ||
             content.status == PREDICITON_STATUS_OK_SHOW_ARR_BANNER_SLOT_2) {
    Prediction *predictions = content.predictions;
    this->canvas.setFont(&MBTASans);

    Serial.printf("%s: %s\n", predictions[0].label, predictions[0].value);
    Serial.printf("%s: %s\n", predictions[1].label, predictions[1].value);

    int slot =
        (int)content.status - (int)PREDICITON_STATUS_OK_SHOW_ARR_BANNER_SLOT_1;
    char arr_banner_message_line1[32];
    Serial.println(predictions[slot].label);
    snprintf(arr_banner_message_line1, 32, "%s train", predictions[slot].label);
    Serial.println(arr_banner_message_line1);
    char arr_banner_message_line2[] = "is now arriving.";

    int cursor_x_1 = justify_center(arr_banner_message_line1, 10);
    this->canvas.setCursor(cursor_x_1, 15);
    this->canvas.print(arr_banner_message_line1);
    int cursor_x_2 = justify_center(arr_banner_message_line2, 10);
    this->canvas.setCursor(cursor_x_2, 31);
    this->canvas.print(arr_banner_message_line2);
  } else if (content.status == PREDICTION_STATUS_OK_SHOW_STATION_BANNER) {
    Prediction *predictions = content.predictions;
    this->canvas.setFont(NULL);
    this->canvas.setCursor(0, 0);
    this->canvas.print(predictions[0].label);
  } else {
    this->canvas.setFont(NULL);
    this->canvas.setCursor(0, 0);
    this->canvas.print("Failed to fetch MBTA data");
    Serial.println("Failed to fetch MBTA data");
  }
  this->dma_display->drawRGBBitmap(0, 0, this->canvas.getBuffer(),
                                   this->canvas.width(), this->canvas.height());
}

void Display::render_music_content(MusicRenderContent content) {
  Serial.println("Rendering music content");
  this->canvas.setFont(NULL);
  this->canvas.setTextColor(SPOTIFY_GREEN);
  this->canvas.setTextWrap(false);
  this->canvas.setCursor(0, 0);
  this->dma_display->setFont(NULL);
  this->dma_display->setTextColor(SPOTIFY_GREEN);
  this->dma_display->setTextWrap(false);
  this->dma_display->setCursor(0, 0);
  if (content.status == SPOTIFY_RESPONSE_OK ||
      content.status == SPOTIFY_RESPONSE_OK_SHOW_CACHED) {
    CurrentlyPlaying playing = content.data;
    int progress_bar_width = SCREEN_WIDTH - 32;
    this->canvas.fillRect(32, SCREEN_HEIGHT - 2, progress_bar_width, 2,
                          this->BLACK);
    // draw progress bar
    double progress = (double)playing.progress_ms / (double)playing.duration_ms;
    int current_bar_width = progress_bar_width * progress;
    this->canvas.drawRect(32, SCREEN_HEIGHT - 2, progress_bar_width, 2,
                          this->WHITE);
    if (current_bar_width > 0) {
      this->canvas.drawRect(32, SCREEN_HEIGHT - 2, current_bar_width, 2,
                            SPOTIFY_GREEN);
    }
    // draw time progress
    Rect progress_time_bounds;
    int16_t progress_time_x = SCREEN_HEIGHT + 1;
    int16_t progress_time_y = SCREEN_HEIGHT - 4;
    char progress_time[16];
    millis_to_timestring(playing.progress_ms, progress_time);
    this->canvas.setFont(&Picopixel);
    this->canvas.getTextBounds(progress_time, progress_time_x, progress_time_y,
                               &progress_time_bounds.x, &progress_time_bounds.y,
                               &progress_time_bounds.w,
                               &progress_time_bounds.h);
    this->canvas.fillRect(progress_time_bounds.x, progress_time_bounds.y,
                          progress_time_bounds.w + 8, progress_time_bounds.h,
                          this->BLACK);
    this->canvas.setCursor(progress_time_x, progress_time_y);
    this->canvas.print(progress_time);
    // draw time to end
    int32_t time_to_end_millis = -(playing.duration_ms - playing.progress_ms);
    Rect time_to_end_bounds;
    int16_t time_to_end_x = 0;
    int16_t time_to_end_y = progress_time_y;
    char time_to_end[16];
    millis_to_timestring(time_to_end_millis, time_to_end);
    this->canvas.setFont(&Picopixel);
    this->canvas.getTextBounds(time_to_end, time_to_end_x, time_to_end_y,
                               &time_to_end_bounds.x, &time_to_end_bounds.y,
                               &time_to_end_bounds.w, &time_to_end_bounds.h);
    time_to_end_x = SCREEN_WIDTH - time_to_end_bounds.w - 1;
    this->canvas.fillRect(time_to_end_x - 8, time_to_end_bounds.y,
                          time_to_end_bounds.w + 16, time_to_end_bounds.h,
                          this->BLACK);
    this->canvas.setCursor(time_to_end_x, time_to_end_y);
    this->canvas.print(time_to_end);
    // draw image
    this->canvas.drawRGBBitmap(0, 0, this->image_canvas.getBuffer(),
                               this->image_canvas.width(),
                               this->image_canvas.height());

    this->render_canvas_to_display();
  } else if (content.status == SPOTIFY_RESPONSE_EMPTY) {
    this->dma_display->fillScreen(this->BLACK);
    this->dma_display->print("Nothing is playing");
  } else {
    this->dma_display->fillScreen(this->BLACK);
    this->dma_display->print("Error querying the spotify API");
  }
}

void Display::render_animation_content(AnimationRenderContent content) {
  if (content.type == ANIMATION_TYPE_TEXT_SCROLL) {
    this->render_text_scrolling(content, false);
  }
}

void Display::render_text_scrolling(AnimationRenderContent content, bool draw) {
  int16_t x0, y0;
  uint16_t w0, h0;
  char *text = content.content.text_scroll.text;
  uint32_t start = content.content.text_scroll.start_timestamp;
  Rect bbox = content.bbox;
  int16_t speed = content.speed;
  this->mask.fillScreen(this->BLACK);
  this->mask.fillRect(bbox.x, bbox.y, bbox.w, bbox.h, this->WHITE);
  this->scratch_canvas.setFont(NULL);
  this->scratch_canvas.setTextColor(this->SPOTIFY_GREEN);
  this->scratch_canvas.fillScreen(this->BLACK);
  this->scratch_canvas.setTextWrap(false);
  this->scratch_canvas.setCursor(0, 0);
  this->scratch_canvas.getTextBounds(text, bbox.x, bbox.y, &x0, &y0, &w0, &h0);
  int32_t ticks_delta = xTaskGetTickCount() - start;
  int wrap_width = max(bbox.w, w0) + 8;
  int shift_x = (int)(round(speed * ticks_delta / 1000)) % wrap_width;
  int new_x = bbox.x + shift_x;
  this->scratch_canvas.setCursor(new_x, bbox.y);
  this->scratch_canvas.print(text);
  this->scratch_canvas.setCursor(wrap_width + new_x, bbox.y);
  this->scratch_canvas.print(text);
  this->scratch_canvas.setCursor(-wrap_width + new_x, bbox.y);
  this->scratch_canvas.print(text);
  this->canvas.drawRGBBitmap(0, 0, this->scratch_canvas.getBuffer(),
                             mask.getBuffer(), this->scratch_canvas.width(),
                             this->scratch_canvas.height());
  if (draw) {
    this->render_canvas_to_display();
  }
}

void millis_to_timestring(int32_t delta, char *dst) {
  bool negative = delta < 0;
  delta = abs(delta);
  int seconds = floor(delta / 1000.0);
  int hours = floor(seconds / 3600.0);
  int minutes = floor((seconds % 3600) / 60.0);
  seconds = seconds % 60;
  if (hours > 0) {
    sniprintf(dst, 16, "%s%02d:%02d:%02d", negative ? "-" : "", hours, minutes,
              seconds);
  } else {
    sniprintf(dst, 16, "%s%02d:%02d", negative ? "-" : "", minutes, seconds);
  }
}
