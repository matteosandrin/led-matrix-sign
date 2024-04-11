#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

#ifndef SPOTIFY_H
#define SPOTIFY_H

#define SPOTIFY_TOKEN_REFRESH_RATE 30 * 60 * 1000 // 30 min in millis

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

class Spotify {
  char access_token[256];
  unsigned long last_refresh_time;
  WiFiClientSecure *wifi_client;
  DynamicJsonDocument *refresh_token_response;
  DynamicJsonDocument *currently_playing_response;
  SpotifyResponse fetch_currently_playing(JsonDocument *dst);
  SpotifyResponse fetch_refresh_token(char *dst);
  void check_refresh_token();
  void get_refresh_bearer_token(char *dst);
  void get_api_bearer_token(char *dst);

 public:
  void setup();
  SpotifyResponse refresh_token();
  SpotifyResponse get_currently_playing(CurrentlyPlaying *dst);
};

#endif /* SPOTIFY_H */
