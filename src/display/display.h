#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include "../../common.h"
#include "../mbta/mbta.h"
#include "../spotify/spotify.h"

#ifndef RENDER_H
#define RENDER_H

struct MBTARenderContent {
  PredictionStatus status;
  Prediction predictions[2];
};

struct TextRenderContent {
  char text[128];
};

struct MusicRenderContent {
  SpotifyResponse status;
  CurrentlyPlaying data;
};

struct RenderMessage {
  SignMode sign_mode;
  MBTARenderContent mbta_content;
  TextRenderContent text_content;
  MusicRenderContent music_content;
};

struct RenderRequest {
  SignMode sign_mode;
};

class Display {
  MatrixPanel_I2S_DMA *dma_display;
  // Using a GFXcanvas reduces the flicker when redrawing the screen, but uses a
  // lot of memory. (160 * 32 * 2 = 10240 bytes)
  // https://learn.adafruit.com/adafruit-gfx-graphics-library/minimizing-redraw-flicker
  GFXcanvas16 canvas;

  int justify_right(char *str, int char_width, int min_x);
  int justify_center(char *str, int char_width);

 public:
  Display();
  void setup();
  void log(char *message);
  void render_text_content(TextRenderContent content, uint16_t color);
  void render_mbta_content(MBTARenderContent content);
  void render_music_content(MusicRenderContent content);

  uint16_t AMBER;
  uint16_t WHITE;
  uint16_t BLACK;
  uint16_t SPOTIFY_GREEN;
};

#endif /* RENDER_H */
