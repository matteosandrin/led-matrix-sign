#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include <map>

#include "../../common.h"
#include "common.h"

#ifndef ANIMATION_H
#define ANIMATION_H

#define ANIMATION_FONT_HEIGHT 8
#define ANIMATION_IMAGE_WIDTH 32

class Animations {
  std::map<AnimationId, Animation> animations;
  GFXcanvas16 *canvas;
  void start_music_animation(AnimationId id, char *text, uint16_t bbox_width);

 public:
  void setup(GFXcanvas16 *canvas);
  void start_music_animations(CurrentlyPlaying song);
  void stop_music_animations();
  void draw(QueueHandle_t render_queue);
};

#endif
