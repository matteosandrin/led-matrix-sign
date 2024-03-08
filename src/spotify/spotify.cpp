#include "spotify.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <base64.h>

#include "spotify-api-key.h"

#define SPOTIFY_REFRESH_TOKEN_URL "https://accounts.spotify.com/api/token"
#define SPOTIFY_CURRENTLY_PLAYING_URL \
  "https://api.spotify.com/v1/me/player/currently-playing"
#define SPOTIFY_REFRESH_TOKEN_PAYLOAD \
  "grant_type=refresh_token&"         \
  "refresh_token=" SPOTIFY_REFRESH_TOKEN

Spotify::Spotify() {
  this->wifi_client = new WiFiClientSecure;
  this->refresh_token_response = new DynamicJsonDocument(2048);
  this->currently_playing_response = new DynamicJsonDocument(2048);
}

void Spotify::setup() {
  //   Preferences preferences;
  //   preferences.begin("spotify");
  this->refresh_token();
}

SpotifyResponse Spotify::get_currently_playing(CurrentlyPlaying *dst) {
  SpotifyResponse status =
     this->fetch_currently_playing(currently_playing_response);
  if (status != SPOTIFY_RESPONSE_OK) {
    return status;
  }
  serializeJsonPretty(*this->currently_playing_response, Serial);
  JsonObject currently_playing = (*this->currently_playing_response)["item"];
  String artist = currently_playing["artists"][0]["name"];
  String name = currently_playing["name"];
  artist.toCharArray(dst->artist, 64);
  name.toCharArray(dst->title, 64);
  dst->duration_ms = currently_playing["duration_ms"];
  dst->progress_ms = currently_playing["progress_ms"];
  return SPOTIFY_RESPONSE_OK;
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
    this->wifi_client->setInsecure();
    HTTPClient https;
    if (https.begin(*this->wifi_client, SPOTIFY_REFRESH_TOKEN_URL)) {
      char bearer[256];
      this->get_refresh_bearer_token(bearer);
      https.addHeader("Authorization", bearer);
      https.addHeader("content-type", "application/x-www-form-urlencoded");
      int httpCode = https.POST(SPOTIFY_REFRESH_TOKEN_PAYLOAD);
      Serial.printf("[HTTPS] POST... code: %d\n", httpCode);
      if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK ||
            httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          DeserializationError error = deserializeJson(
              *this->refresh_token_response, https.getStream());
          if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return SPOTIFY_RESPONSE_ERROR;
          }
          String access_code_str =
              (*this->refresh_token_response)["access_token"];
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
    this->wifi_client->setInsecure();
    HTTPClient https;
    if (https.begin(*this->wifi_client, SPOTIFY_CURRENTLY_PLAYING_URL)) {
      char bearer[256];
      this->get_api_bearer_token(bearer);
      https.addHeader("Authorization", bearer);
      https.addHeader("content-type", "application/x-www-form-urlencoded");
      int httpCode = https.GET();
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
      if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK ||
            httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          StaticJsonDocument<1024> filter;
          filter["item"]["type"] = true;
          filter["item"]["name"] = true;
          filter["item"]["duration_ms"] = true;
          filter["item"]["artists"][0]["name"] = true;
          filter["progress_ms"] = true;
          filter["timestamp"] = true;
          DeserializationError error = deserializeJson(
              *dst, https.getStream(), DeserializationOption::Filter(filter));
          if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return SPOTIFY_RESPONSE_ERROR;
          }
          return SPOTIFY_RESPONSE_OK;
        } else if (httpCode == HTTP_CODE_NO_CONTENT) {
          return SPOTIFY_RESPONSE_EMPTY;
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