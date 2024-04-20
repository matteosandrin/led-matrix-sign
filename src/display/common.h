#include "../mbta/mbta.h"
#include "../spotify/spotify.h"

#ifndef DISPLAY_COMMON_H
#define DISPLAY_COMMON_H

struct Rect {
  int16_t x;
  int16_t y;
  uint16_t w;
  uint16_t h;
};

enum AnimationType {
  ANIMATION_TYPE_TEXT_SCROLL,
};

enum AnimationId {
  ANIMATION_ID_DEFAULT,
  ANIMATION_ID_MUSIC_TITLE,
  ANIMATION_ID_MUSIC_ARTIST,
};

struct TextAnimationContent {
  char text[128];
  uint32_t start_timestamp;
};

union AnimationContent {
  TextAnimationContent text_scroll;
};

struct Animation {
  AnimationType type;
  AnimationId id;
  Rect bbox;
  int16_t speed;  // pixel / s
  AnimationContent content;
};

enum RenderType {
  RENDER_TYPE_MBTA,
  RENDER_TYPE_TEXT,
  RENDER_TYPE_MUSIC,
  RENDER_TYPE_ANIMATION,
};

struct MBTARenderContent {
  PredictionStatus status;
  Prediction predictions[2];
};

struct TextRenderContent {
  char text[128];
  int16_t color;
};

struct MusicRenderContent {
  SpotifyResponse status;
  CurrentlyPlaying data;
};

typedef Animation AnimationRenderContent;

union RenderContent {
  MBTARenderContent mbta;
  TextRenderContent text;
  MusicRenderContent music;
  AnimationRenderContent animation;
};

struct RenderMessage {
  RenderType type;
  RenderContent content;
};

#endif /* DISPLAY_COMMON_H */
