#include <ArduinoJson.h>

#ifndef SPOTIFY_H
#define SPOTIFY_H

enum SpotifyResponse {
  SPOTIFY_RESPONSE_OK,
  SPOTIFY_RESPONSE_ERROR,
  SPOTIFY_RESPONSE_EMPTY
};

struct CurrentlyPlaying {
  char title[64];
  char artist[64];
  int32_t duration_ms;
  int32_t progress_ms;
};

void setup_spotify();
SpotifyResponse refresh_token(char *dst);
SpotifyResponse get_currently_playing(CurrentlyPlaying *dst);
SpotifyResponse fetch_currently_playing(JsonDocument *dst);

#endif /* SPOTIFY_H */
