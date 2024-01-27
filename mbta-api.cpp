#include "mbta-api.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "config.h"
#include "time.h"

#define MBTA_REQUEST                                                    \
  "https://api-v3.mbta.com/predictions?"                                \
  "api_key=" MBTA_API_KEY                                               \
  "&"                                                                   \
  "filter[stop]=place-harsq&"                                           \
  "filter[route]=Red&"                                                  \
  "fields[prediction]=arrival_time,departure_time,status,direction_id&" \
  "include=trip"

DynamicJsonDocument *prediction_data = new DynamicJsonDocument(8192);
WiFiClientSecure *wifi_client = new WiFiClientSecure;

PredictionStatus get_mbta_predictions(Prediction dst[2]) {
  int status = fetch_predictions(prediction_data);
  if (status != 0) {
    return PREDICTION_STATUS_ERROR;
  }
  JsonObject prediction1 = find_first_prediction_for_direction(
      prediction_data, DIRECTION_SOUTHBOUND);
  JsonObject prediction2 = find_first_prediction_for_direction(
      prediction_data, DIRECTION_NORTHBOUND);
  if (prediction1.isNull() || prediction2.isNull()) {
    return PREDICTION_STATUS_ERROR;
  }
  JsonObject trip1 = find_trip_for_prediction(prediction_data, prediction1);
  JsonObject trip2 = find_trip_for_prediction(prediction_data, prediction2);
  if (trip1.isNull() || trip2.isNull()) {
    return PREDICTION_STATUS_ERROR;
  }
  format_prediction(prediction1, trip1, &dst[0]);
  format_prediction(prediction2, trip2, &dst[1]);
  prediction_data->clear();
  return PREDICTION_STATUS_OK;
}

int fetch_predictions(JsonDocument *prediction_data) {
  if (wifi_client) {
    wifi_client->setInsecure();
    HTTPClient https;
    if (https.begin(*wifi_client, MBTA_REQUEST)) {
      int httpCode = https.GET();
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
      if (httpCode > 0) {
        // file found at server
        if (httpCode == HTTP_CODE_OK ||
            httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
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
              deserializeJson(*prediction_data, https.getStream(),
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
  }
  return 1;
}

JsonObject find_first_prediction_for_direction(
    JsonDocument *prediction_data_ptr, int direction) {
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
        return prediction;
      } else {
        int arr_diff = diff_with_local_time(arr_time);
        if (arr_diff > -30) {
          return prediction;
        }
      }
    }
  }
  return prediction;
}

JsonObject find_trip_for_prediction(JsonDocument *prediction_data_ptr,
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

int diff_with_local_time(String timestring) {
  struct tm time;
  struct tm local_time;
  char timestring_char[32];
  timestring.toCharArray(timestring_char, 32);
  strptime(timestring_char, "%Y-%m-%dT%H:%M:%S", &time);
  getLocalTime(&local_time);
  return datetime_diff(local_time, time);
}

int datetime_diff(struct tm time1, struct tm time2) {
  return datetime_to_epoch(time2) - datetime_to_epoch(time1);
}

// source:
// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html#tag_04_15
int datetime_to_epoch(struct tm dt) {
  return dt.tm_sec + dt.tm_min * 60 + dt.tm_hour * 3600 + dt.tm_yday * 86400 +
         (dt.tm_year - 70) * 31536000 + ((dt.tm_year - 69) / 4) * 86400 -
         ((dt.tm_year - 1) / 100) * 86400 + ((dt.tm_year + 299) / 400) * 86400;
}

void determine_display_string(int arr_diff, int dep_diff, char *dst) {
  if (arr_diff > 0) {
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
      strcpy(dst, "ERR");
    }
  }
}

void format_prediction(JsonObject prediction, JsonObject trip,
                       Prediction *dst) {
  String headsign = trip["attributes"]["headsign"];
  String arr_time = prediction["attributes"]["arrival_time"];
  String dep_time = prediction["attributes"]["departure_time"];
  String status = prediction["attributes"]["status"];
  headsign.toCharArray(dst->label, 16);
  Serial.printf("status: %s %d\n", status, status == NULL);
  if (!status.equals("null")) {
    status.substring(0, 7).toCharArray(dst->value, 16);
    return;
  }
  struct tm local_time;
  getLocalTime(&local_time);
  char display_string[16];
  if (arr_time && arr_time.length() > 0 && dep_time && dep_time.length() > 0) {
    int arr_diff = diff_with_local_time(arr_time);
    int dep_diff = diff_with_local_time(dep_time);
    determine_display_string(arr_diff, dep_diff, display_string);
  } else {
    strcpy(display_string, "");
  }
  Serial.printf("display string: %s\n", display_string);
  strcpy(dst->value, display_string);
}