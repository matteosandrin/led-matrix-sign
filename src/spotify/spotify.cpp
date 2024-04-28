#include "spotify.h"

#include <base64.h>
#include <freertos/FreeRTOS.h>

#include "spotify-api-key.h"
#include "spotify-cert.h"

#define SPOTIFY_REFRESH_TOKEN_URL "https://accounts.spotify.com/api/token"
#define SPOTIFY_CURRENTLY_PLAYING_URL \
  "https://api.spotify.com/v1/me/player/currently-playing"
#define SPOTIFY_REFRESH_TOKEN_PAYLOAD \
  "grant_type=refresh_token&"         \
  "refresh_token=" SPOTIFY_REFRESH_TOKEN

void Spotify::setup() {
  lms::Client::setup(2048);
  this->wifi_client->setCACert(spotify_certificate);
  this->refresh_token();
  this->album_cover_jpg = new uint8_t[ALBUM_COVER_IMG_BUF_SIZE];
  this->clear_current_song();
}

SpotifyResponse Spotify::get_currently_playing(CurrentlyPlaying *dst) {
  dst->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
  SpotifyResponse status = this->fetch_currently_playing(data);
  if (status == SPOTIFY_RESPONSE_EMPTY && this->current_song.timestamp_ms > 0) {
    return SPOTIFY_RESPONSE_OK_SHOW_CACHED;
  }
  if (status != SPOTIFY_RESPONSE_OK) {
    return status;
  }
  JsonObject currently_playing = (*this->data)["item"];
  String name = currently_playing["name"];
  this->format_artists(dst->artist, this->data);
  name.toCharArray(dst->title, 128);
  dst->duration_ms = currently_playing["duration_ms"];
  dst->progress_ms = (*this->data)["progress_ms"];
  this->format_album_cover(&dst->cover, this->data);
  return SPOTIFY_RESPONSE_OK;
}

void Spotify::format_artists(char *dst, JsonDocument *data) {
  JsonArray artists = (*data)["item"]["artists"];
  char artist[128] = "";
  if (artists.size() > 1) {
    for (JsonObject a : artists) {
      strncat(artist, a["name"], 128 - strlen(artist));
      if (a != artists[artists.size() - 1]) {
        strncat(artist, ", ", 128 - strlen(artist));
      }
    }
  } else {
    strncpy(artist, artists[0]["name"], 128);
  }
  strncpy(dst, artist, 128);
}

void Spotify::format_album_cover(AlbumCover *dst, JsonDocument *data) {
  JsonArray imgs = (*data)["item"]["album"]["images"];
  if (imgs.size() == 0) {
    return;
  }
  JsonObject smallest_img = imgs[0];
  for (JsonObject img : imgs) {
    if (img["width"] < smallest_img["width"]) {
      smallest_img = img;
    }
  }
  String url = smallest_img["url"];
  url.toCharArray(dst->url, 128);
  dst->width = smallest_img["width"];
  dst->height = smallest_img["height"];
}

void Spotify::get_refresh_bearer_token(char *dst) {
  char bearer[128];
  sprintf(bearer, "%s:%s", SPOTIFY_CLIENT_ID, SPOTIFY_CLIENT_SECRET);
  String bearer_header = "Basic " + base64::encode(bearer);
  bearer_header.toCharArray(dst, 256);
}

void Spotify::get_api_bearer_token(char *dst) {
  sprintf(dst, "Bearer %s", this->access_token);
}

SpotifyResponse Spotify::refresh_token() {
  SpotifyResponse status = this->fetch_refresh_token(access_token);
  if (status != SPOTIFY_RESPONSE_OK) {
    Serial.printf("Failed to refresh spotify token: %d\n", status);
  }
  this->last_refresh_time = millis();
  return status;
}

SpotifyResponse Spotify::fetch_refresh_token(char *dst) {
  if (this->wifi_client) {
    this->wifi_client->setCACert(spotify_certificate);
    HTTPClient https;
    if (https.begin(*this->wifi_client, SPOTIFY_REFRESH_TOKEN_URL)) {
      char bearer[256];
      this->get_refresh_bearer_token(bearer);
      https.addHeader("Authorization", bearer);
      https.addHeader("content-type", "application/x-www-form-urlencoded");
      int http_code = https.POST(SPOTIFY_REFRESH_TOKEN_PAYLOAD);
      Serial.printf("[HTTPS] POST... code: %d\n", http_code);
      if (http_code > 0) {
        if (http_code == HTTP_CODE_OK ||
            http_code == HTTP_CODE_MOVED_PERMANENTLY) {
          DeserializationError error =
              deserializeJson(*this->data, https.getStream());
          if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return SPOTIFY_RESPONSE_ERROR;
          }
          String access_code_str = (*this->data)["access_token"];
          access_code_str.toCharArray(dst, 256);
          return SPOTIFY_RESPONSE_OK;
        }
      }
    }
  }
  return SPOTIFY_RESPONSE_ERROR;
}

SpotifyResponse Spotify::fetch_currently_playing(JsonDocument *dst) {
  this->check_refresh_token();
  if (this->wifi_client) {
    this->wifi_client->setCACert(spotify_certificate);
    if (!this->http_client.connected()) {
      Serial.println("Starting new http connection to spotify api");
      char bearer[256];
      this->get_api_bearer_token(bearer);
      this->http_client.addHeader("Authorization", bearer);
      this->http_client.addHeader("content-type",
                                  "application/x-www-form-urlencoded");
      if (!this->http_client.begin(*this->wifi_client,
                                   SPOTIFY_CURRENTLY_PLAYING_URL)) {
        return SPOTIFY_RESPONSE_ERROR;
      }
    }
    int http_code = this->http_client.GET();
    Serial.printf("[HTTPS] GET... code: %d\n", http_code);
    if (http_code > 0) {
      if (http_code == HTTP_CODE_OK ||
          http_code == HTTP_CODE_MOVED_PERMANENTLY) {
        StaticJsonDocument<1024> filter;
        filter["item"]["type"] = true;
        filter["item"]["name"] = true;
        filter["item"]["duration_ms"] = true;
        filter["item"]["artists"][0]["name"] = true;
        filter["item"]["album"]["images"][0]["url"] = true;
        filter["item"]["album"]["images"][0]["width"] = true;
        filter["item"]["album"]["images"][0]["height"] = true;
        filter["progress_ms"] = true;
        filter["timestamp"] = true;
        DeserializationError error =
            deserializeJson(*dst, this->http_client.getStream(),
                            DeserializationOption::Filter(filter));
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return SPOTIFY_RESPONSE_ERROR;
        }
        return SPOTIFY_RESPONSE_OK;
      } else if (http_code == HTTP_CODE_NO_CONTENT) {
        return SPOTIFY_RESPONSE_EMPTY;
      }
    }
  }
  return SPOTIFY_RESPONSE_ERROR;
}

SpotifyResponse Spotify::get_album_cover(CurrentlyPlaying *src) {
  return this->fetch_album_cover(src->cover.url, this->album_cover_jpg);
}

SpotifyResponse Spotify::fetch_album_cover(char *url, uint8_t *dst) {
  memset(dst, 0, ALBUM_COVER_IMG_BUF_SIZE);
  Serial.printf("url: %s\n", url);
  Serial.printf("img: %d %d %d %d\n", dst[0], dst[1], dst[2], dst[3]);
  if (this->wifi_client) {
    this->wifi_client->setInsecure();
    HTTPClient https;
    if (https.begin(*this->wifi_client, url)) {
      int http_code = https.GET();
      Serial.printf("[HTTPS] GET... code: %d\n", http_code);
      if (http_code > 0) {
        if (http_code == HTTP_CODE_OK ||
            http_code == HTTP_CODE_MOVED_PERMANENTLY) {
          int size = https.getSize();
          int i = 0;
          uint8_t buff[128];
          WiFiClient *stream = https.getStreamPtr();
          while (https.connected() && (size > 0 || size == -1)) {
            size_t stream_size = stream->available();
            if (stream_size > 0) {
              int c = stream->readBytes(
                  buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
              memcpy(dst + i, buff, c);
              if (size > 0) {
                size -= c;
                i += c;
              }
            }
            delay(1);
          }
          Serial.printf("img: %d %d %d %d\n", dst[0], dst[1], dst[2], dst[3]);
          return SPOTIFY_RESPONSE_OK;
        }
      }
    }
  }
  return SPOTIFY_RESPONSE_ERROR;
}

void Spotify::check_refresh_token() {
  if (millis() - this->last_refresh_time > SPOTIFY_TOKEN_REFRESH_RATE) {
    Serial.println("refreshing spotify token after 30min");
    this->refresh_token();
  }
}

void Spotify::update_current_song(CurrentlyPlaying *src) {
  this->current_song = *src;
  this->current_song.progress_ms = 0;
}

void Spotify::clear_current_song() {
  memset(&this->current_song, 0, sizeof(this->current_song));
}

bool Spotify::is_current_song_new(const CurrentlyPlaying *cmp) {
  return strcmp(cmp->artist, this->current_song.artist) != 0 ||
         strcmp(cmp->title, this->current_song.title) != 0;
}

CurrentlyPlaying Spotify::get_current_song() { return this->current_song; }