#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include "../../common.h"
#include "common.h"

#ifndef RENDER_H
#define RENDER_H

class Display {
  MatrixPanel_I2S_DMA *dma_display;
  // Using a GFXcanvas reduces the flicker when redrawing the screen, but uses a
  // lot of memory. (160 * 32 * 2 = 10240 bytes)
  // https://learn.adafruit.com/adafruit-gfx-graphics-library/minimizing-redraw-flicker
  GFXcanvas16 canvas;

  int justify_right(char *str, int char_width, int min_x);
  int justify_center(char *str, int char_width);
  void render_text_scrolling(char *text, Rect bbox, int16_t speed, bool draw);

 public:
  Display();
  void setup();
  void log(char *message);
  GFXcanvas16 *get_canvas();
  Rect get_text_bbox(char *text, int16_t x, int16_t y);
  void render_text_content(TextRenderContent content);
  void render_mbta_content(MBTARenderContent content);
  void render_music_content(MusicRenderContent content);
  void render_animation_content(AnimationRenderContent content);

  uint16_t AMBER;
  uint16_t WHITE;
  uint16_t BLACK;
  uint16_t SPOTIFY_GREEN;
};

#endif /* RENDER_H */
