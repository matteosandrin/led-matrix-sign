#include "display.h"

#include "../fonts/MBTASans.h"
#include "display-pins.h"

Display::Display() : canvas(SCREEN_WIDTH, SCREEN_HEIGHT) {
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
}

void Display::log(char *message) {
  this->dma_display->clearScreen();
  this->dma_display->setCursor(0, 0);
  this->dma_display->setTextWrap(true);
  this->dma_display->print(message);
  this->dma_display->setTextWrap(false);
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

void Display::render_text_content(TextRenderContent content, uint16_t color) {
  Serial.println("Rendering text content");
  this->canvas.fillScreen(BLACK);
  this->canvas.setFont(NULL);
  this->canvas.setTextColor(color);
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
  this->canvas.fillScreen(BLACK);
  this->canvas.setFont(NULL);
  this->canvas.setTextColor(SPOTIFY_GREEN);
  this->canvas.setTextWrap(false);
  this->canvas.setCursor(0, 0);
  if (content.status == SPOTIFY_RESPONSE_OK) {
    CurrentlyPlaying playing = content.data;
    this->canvas.println(playing.title);
    this->canvas.println(playing.artist);
    // draw progress bar
    int progress_bar_width = SCREEN_WIDTH;
    double progress = (double)playing.progress_ms / (double)playing.duration_ms;
    int current_bar_width = progress_bar_width * progress;
    this->canvas.drawRect(0, SCREEN_HEIGHT - 2, progress_bar_width, 2, WHITE);
    if (current_bar_width > 0) {
      this->canvas.drawRect(0, SCREEN_HEIGHT - 2, current_bar_width, 2,
                            SPOTIFY_GREEN);
    }
  } else if (content.status == SPOTIFY_RESPONSE_EMPTY) {
    this->canvas.print("Nothing is playing");
  } else {
    this->canvas.print("Error querying the spotify API");
  }
  this->dma_display->drawRGBBitmap(0, 0, this->canvas.getBuffer(),
                                   this->canvas.width(), this->canvas.height());
}