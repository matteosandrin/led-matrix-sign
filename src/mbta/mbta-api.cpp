#include "mbta-api.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "mbta-api-key.h"
#include "mbta-cert.h"

#define MBTA_REQUEST                                                    \
  "https://api-v3.mbta.com/predictions?"                                \
  "api_key=%s&"                                                         \
  "filter[stop]=%s&"                                                    \
  "filter[route]=Red&"                                                  \
  "fields[prediction]=arrival_time,departure_time,status,direction_id&" \
  "include=trip"

#define DEFAULT_TRAIN_STATION TRAIN_STATION_HARVARD

void MBTA::setup() {
  this->prediction_data = new DynamicJsonDocument(8192);
  this->wifi_client = new WiFiClientSecure;
  this->wifi_client->setCACert(mbta_certificate);
  this->get_placeholder_predictions(this->latest_predictions);
  this->current_station = DEFAULT_TRAIN_STATION;
}

PredictionStatus MBTA::get_predictions(Prediction *dst, int num_predictions,
                                       int directions[], int nth_positions[]) {
  int status = this->fetch_predictions(this->prediction_data);
  if (status != 0) {
    return PREDICTION_STATUS_ERROR;
  }
  for (int i = 0; i < num_predictions; i++) {
    JsonObject prediction = this->find_nth_prediction_for_direction(
        this->prediction_data, directions[i], nth_positions[i]);
    if (prediction.isNull()) {
      return PREDICTION_STATUS_ERROR;
    }
    JsonObject trip =
        this->find_trip_for_prediction(this->prediction_data, prediction);
    if (trip.isNull()) {
      return PREDICTION_STATUS_ERROR;
    }
    this->format_prediction(prediction, trip, &dst[i]);
  }
  PredictionStatus prediction_status = PREDICTION_STATUS_OK;
  if (this->show_arriving_banner(&dst[0], directions[0])) {
    prediction_status = PREDICITON_STATUS_OK_SHOW_ARR_BANNER_SLOT_1;
  } else if (this->show_arriving_banner(&dst[1], directions[1])) {
    prediction_status = PREDICITON_STATUS_OK_SHOW_ARR_BANNER_SLOT_2;
  }
  this->update_latest_predictions(dst, directions);
  this->prediction_data->clear();
  return prediction_status;
}

PredictionStatus MBTA::get_predictions_both_directions(Prediction dst[2]) {
  int directions[2] = {DIRECTION_SOUTHBOUND, DIRECTION_NORTHBOUND};
  int nth_positions[2] = {0, 0};
  return this->get_predictions(dst, 2, directions, nth_positions);
}

PredictionStatus MBTA::get_predictions_one_direction(Prediction dst[2],
                                                     int direction) {
  int directions[2] = {direction, direction};
  int nth_positions[2] = {0, 1};
  return this->get_predictions(dst, 2, directions, nth_positions);
}

void MBTA::get_placeholder_predictions(Prediction dst[2]) {
  strcpy(dst[0].label, "Ashmont");
  strcpy(dst[0].value, "");
  strcpy(dst[1].label, "Alewife");
  strcpy(dst[1].value, "");
}

void MBTA::set_station(TrainStation station) {
  this->current_station = station;
  this->get_placeholder_predictions(this->latest_predictions);
}

int MBTA::fetch_predictions(JsonDocument *prediction_data) {
  if (this->wifi_client) {
    if (!this->http_client.connected()) {
      Serial.println("Starting new http connection to mbta api");
      char request_url[256];
      snprintf(request_url, 256, MBTA_REQUEST, MBTA_API_KEY,
              this->train_station_codes[this->current_station]);
      if (!this->http_client.begin(*this->wifi_client, request_url)) {
        return 1;
      }
    }
    int httpCode = this->http_client.GET();
    Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
    if (httpCode > 0) {
      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        // filter down the response so less memory is used
        StaticJsonDocument<1024> filter;
        filter["data"][0]["attributes"]["arrival_time"] = true;
        filter["data"][0]["attributes"]["departure_time"] = true;
        filter["data"][0]["attributes"]["direction_id"] = true;
        filter["data"][0]["attributes"]["status"] = true;
        filter["data"][0]["relationships"]["trip"]["data"]["id"] = true;
        filter["included"][0]["id"] = true;
        filter["included"][0]["attributes"]["headsign"] = true;

        DeserializationError error =
            deserializeJson(*prediction_data, this->http_client.getStream(),
                            DeserializationOption::Filter(filter));
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return 1;
        }
        return 0;
      }
    }
  }
  return 1;
}

JsonObject MBTA::find_nth_prediction_for_direction(
    JsonDocument *prediction_data_ptr, int direction, int n) {
  JsonArray prediction_array = (*prediction_data_ptr)["data"];
  JsonObject prediction;

  for (int i = 0; i < prediction_array.size(); i++) {
    prediction = prediction_array[i];
    int d = prediction["attributes"]["direction_id"];
    String arr_time = prediction["attributes"]["arrival_time"];
    String dep_time = prediction["attributes"]["departure_time"];
    String status = prediction["attributes"]["status"];
    if (d == direction) {
      if (!status.equals("null")) {
        if (n == 0) {
          return prediction;
        } else {
          n--;
        }
      } else {
        int arr_diff = this->diff_with_local_time(arr_time);
        if (arr_diff > -30) {
          if (n == 0) {
            return prediction;
          } else {
            n--;
          }
        }
      }
    }
  }
  return prediction;
}

JsonObject MBTA::find_trip_for_prediction(JsonDocument *prediction_data_ptr,
                                          JsonObject prediction) {
  JsonArray trip_array = (*prediction_data_ptr)["included"];
  JsonObject trip;

  for (int i = 0; i < trip_array.size(); i++) {
    trip = trip_array[i];
    if (trip["id"] == prediction["relationships"]["trip"]["data"]["id"]) {
      return trip;
    }
  }
  return trip;
}

int MBTA::diff_with_local_time(String timestring) {
  struct tm time;
  struct tm local_time;
  char timestring_char[32];
  timestring.toCharArray(timestring_char, 32);
  strptime(timestring_char, "%Y-%m-%dT%H:%M:%S", &time);
  getLocalTime(&local_time);
  return this->datetime_diff(local_time, time);
}

int MBTA::datetime_diff(struct tm time1, struct tm time2) {
  return this->datetime_to_epoch(time2) - this->datetime_to_epoch(time1);
}

// source:
// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html#tag_04_15
int MBTA::datetime_to_epoch(struct tm dt) {
  return dt.tm_sec + dt.tm_min * 60 + dt.tm_hour * 3600 + dt.tm_yday * 86400 +
         (dt.tm_year - 70) * 31536000 + ((dt.tm_year - 69) / 4) * 86400 -
         ((dt.tm_year - 1) / 100) * 86400 + ((dt.tm_year + 299) / 400) * 86400;
}

void MBTA::determine_display_string(int arr_diff, int dep_diff, String status,
                                    char *dst) {
  if (!status.equals("null")) {
    status.toLowerCase();
    if (status.indexOf("stopped") != -1) {
      strcpy(dst, "STOP");
    } else {
      status.substring(0, 6).toCharArray(dst, 16);
    }
  } else if (arr_diff > 0) {
    if (arr_diff > 60) {
      int minutes = floor(arr_diff / 60.0);
      sprintf(dst, "%d min", minutes);
    } else {
      strcpy(dst, "ARR");
    }
  } else {
    if (dep_diff > 0) {
      strcpy(dst, "BRD");
    } else {
      strcpy(dst, "ERROR");
    }
  }
}

void MBTA::format_prediction(JsonObject prediction, JsonObject trip,
                             Prediction *dst) {
  char display_string[16];
  char status_char[32];
  struct tm local_time;
  String headsign = trip["attributes"]["headsign"];
  String arr_time = prediction["attributes"]["arrival_time"];
  String dep_time = prediction["attributes"]["departure_time"];
  String status = prediction["attributes"]["status"];
  headsign.toCharArray(dst->label, 16);
  status.toCharArray(status_char, 32);
  getLocalTime(&local_time);
  Serial.printf("status: %s\n", status_char);
  if (!status.equals("null")) {
    this->determine_display_string(-1, -1, status, display_string);
  } else if (arr_time && arr_time.length() > 0 && dep_time &&
             dep_time.length() > 0) {
    int arr_diff = this->diff_with_local_time(arr_time);
    int dep_diff = this->diff_with_local_time(dep_time);
    this->determine_display_string(arr_diff, dep_diff, status, display_string);
  } else {
    strcpy(display_string, "ERROR");
  }
  Serial.printf("display string: %s\n", display_string);
  strcpy(dst->value, display_string);
}

void MBTA::update_latest_predictions(Prediction latest[2], int directions[2]) {
  if (directions[0] == directions[1]) {
    this->latest_predictions[directions[0]] = latest[0];
  } else {
    this->latest_predictions[directions[0]] = latest[0];
    this->latest_predictions[directions[1]] = latest[1];
  }
}

bool MBTA::show_arriving_banner(Prediction *prediction, int direction) {
  return strcmp(prediction->value, "ARR") == 0 &&
         strcmp(this->latest_predictions[direction].value, "ARR") != 0;
}

char *train_station_to_str(TrainStation station) {
  switch (station) {
    case TRAIN_STATION_ALEWIFE:
      return "Alewife";
    case TRAIN_STATION_DAVIS:
      return "Davis";
    case TRAIN_STATION_PORTER:
      return "Porter";
    case TRAIN_STATION_HARVARD:
      return "Harvard";
    case TRAIN_STATION_CENTRAL:
      return "Central";
    case TRAIN_STATION_KENDALL:
      return "Kendall/MIT";
    case TRAIN_STATION_CHARLES_MGH:
      return "Charles/MGH";
    case TRAIN_STATION_PARK_STREET:
      return "Park Street";
    case TRAIN_STATION_DOWNTOWN_CROSSING:
      return "Downtown Crossing";
    case TRAIN_STATION_SOUTH_STATION:
      return "South Station";
  }
  return "TRAIN_STATION_UNKNOWN";
}
