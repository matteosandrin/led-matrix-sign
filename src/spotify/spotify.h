#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

#include "../client/client.h"

#ifndef SPOTIFY_H
#define SPOTIFY_H

#define SPOTIFY_TOKEN_REFRESH_RATE 30 * 60 * 1000  // 30 min in millis
#define ALBUM_COVER_IMG_BUF_SIZE 4096

enum SpotifyResponse {
  SPOTIFY_RESPONSE_OK,
  SPOTIFY_RESPONSE_ERROR,
  SPOTIFY_RESPONSE_EMPTY
};

struct AlbumCover {
  char url[128];
  int32_t width;
  int32_t height;
};

struct CurrentlyPlaying {
  char title[128];
  char artist[128];
  int32_t duration_ms;
  int32_t progress_ms;
  uint32_t timestamp;
  AlbumCover cover;
};

class Spotify : lms::Client {
  char access_token[256];
  unsigned long last_refresh_time;
  SpotifyResponse fetch_currently_playing(JsonDocument *dst);
  SpotifyResponse fetch_refresh_token(char *dst);
  void check_refresh_token();
  void get_refresh_bearer_token(char *dst);
  void get_api_bearer_token(char *dst);
  void format_artists(char *dst, JsonDocument *data);
  void format_album_cover(AlbumCover *dst, JsonDocument *data);
  SpotifyResponse fetch_album_cover(char *url, uint8_t *dst);

 public:
  CurrentlyPlaying current_song;
  uint8_t *album_cover_jpg;
  void setup();
  SpotifyResponse refresh_token();
  SpotifyResponse get_currently_playing(CurrentlyPlaying *dst);
  SpotifyResponse get_album_cover(CurrentlyPlaying *src);
  void update_current_song(CurrentlyPlaying *src);
  void clear_current_song();
  bool is_current_song_new(const CurrentlyPlaying *cmp);
};

#endif /* SPOTIFY_H */
