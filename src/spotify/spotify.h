#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

#include "../client/client.h"

#ifndef SPOTIFY_H
#define SPOTIFY_H

#define SPOTIFY_TOKEN_REFRESH_RATE 30 * 60 * 1000  // 30 min in millis

enum SpotifyResponse {
  SPOTIFY_RESPONSE_OK,
  SPOTIFY_RESPONSE_ERROR,
  SPOTIFY_RESPONSE_EMPTY
};

struct CurrentlyPlaying {
  char title[128];
  char artist[128];
  int32_t duration_ms;
  int32_t progress_ms;
};

class Spotify : lms::Client {
  char access_token[256];
  unsigned long last_refresh_time;
  SpotifyResponse fetch_currently_playing(JsonDocument *dst);
  SpotifyResponse fetch_refresh_token(char *dst);
  void check_refresh_token();
  void get_refresh_bearer_token(char *dst);
  void get_api_bearer_token(char *dst);

 public:
  CurrentlyPlaying current_song;
  void setup();
  SpotifyResponse refresh_token();
  SpotifyResponse get_currently_playing(CurrentlyPlaying *dst);
  void update_current_song(CurrentlyPlaying *src);
  void clear_current_song();
  bool is_current_song_new(const CurrentlyPlaying *cmp);
};

#endif /* SPOTIFY_H */
