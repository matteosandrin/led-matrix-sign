#include <ArduinoJson.h>

#ifndef SPOTIFY_H
#define SPOTIFY_H

void setup_spotify();
int refresh_token(char *dst);
int fetch_currently_playing(JsonDocument *dst);

#endif /* SPOTIFY_H */
