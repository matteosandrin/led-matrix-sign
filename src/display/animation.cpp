#include "animation.h"

#include "../../common.h"

void Animations::setup(GFXcanvas16 *canvas) { this->canvas = canvas; }

void Animations::start_music_animations(CurrentlyPlaying song) {
  this->start_music_animation(ANIMATION_ID_MUSIC_TITLE, song.title,
                              SCREEN_WIDTH);
  this->start_music_animation(ANIMATION_ID_MUSIC_ARTIST, song.artist,
                              SCREEN_WIDTH);
}

void Animations::start_music_animation(AnimationId id, char *text,
                                       uint16_t bbox_width) {
  Animation animation;
  Rect text_bbox;
  this->canvas->getTextBounds(text, 0, 0, &text_bbox.x, &text_bbox.y,
                              &text_bbox.w, &text_bbox.h);
  if (text_bbox.w > bbox_width) {
    animation.speed = -10;
  } else {
    // static text
    animation.speed = 0;
  }
  animation.type = ANIMATION_TYPE_TEXT_SCROLL;
  if (id == ANIMATION_ID_MUSIC_TITLE) {
    animation.bbox = {0, 0, bbox_width, 8};
  }
  if (id == ANIMATION_ID_MUSIC_ARTIST) {
    animation.bbox = {0, 8, bbox_width, 8};
  }
  animation.id = id;
  strncpy(animation.content.text_scroll.text, text, 128);
  this->animations[id] = animation;
}

void Animations::stop_music_animations() {
  this->animations.erase(ANIMATION_ID_MUSIC_TITLE);
  this->animations.erase(ANIMATION_ID_MUSIC_ARTIST);
}

void Animations::draw(QueueHandle_t render_queue) {
  if (animations.empty()) {
    return;
  }
  for (auto const &[type, a] : this->animations) {
    RenderMessage message;
    message.type = RENDER_TYPE_ANIMATION;
    message.content.animation = a;
    xQueueSend(render_queue, (void *)&message, TEN_MILLIS);
  }
}