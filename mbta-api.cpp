#include "mbta-api.h"
#include "time.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define MBTA_REQUEST "https://api-v3.mbta.com/predictions?"                                \
                     "api_key=***REMOVED***&"                           \
                     "filter[stop]=place-harsq&"                                           \
                     "filter[route]=Red&"                                                  \
                     "fields[prediction]=arrival_time,departure_time,status,direction_id&" \
                     "include=trip"

DynamicJsonDocument *prediction_data = new DynamicJsonDocument(8192);
WiFiClientSecure *wifi_client = new WiFiClientSecure;

int get_mbta_predictions(Prediction dst[2])
{

    int status = fetch_predictions(prediction_data);
    if (status != 0)
    {
        return status;
    }
    JsonObject prediction1 = find_first_prediction_for_direction(
        prediction_data, 0);
    JsonObject prediction2 = find_first_prediction_for_direction(
        prediction_data, 1);
    JsonObject trip1 = find_trip_for_prediction(prediction_data, prediction1);
    JsonObject trip2 = find_trip_for_prediction(prediction_data, prediction2);
    format_prediction(prediction1, trip1, &dst[0]);
    format_prediction(prediction2, trip2, &dst[1]);
    prediction_data->clear();
    return 0;
}

int fetch_predictions(JsonDocument *prediction_data)
{
    if (wifi_client)
    {
        wifi_client->setInsecure();
        HTTPClient https;
        if (https.begin(*wifi_client, MBTA_REQUEST))
        {
            int httpCode = https.GET();
            Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
            if (httpCode > 0)
            {
                // file found at server
                if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
                {

                    // filter down the response so less memory is used
                    StaticJsonDocument<1024> filter;
                    filter["data"][0]["attributes"]["arrival_time"] = true;
                    filter["data"][0]["attributes"]["departure_time"] = true;
                    filter["data"][0]["attributes"]["direction_id"] = true;
                    filter["data"][0]["attributes"]["status"] = true;
                    filter["data"][0]["relationships"]["trip"]["data"]["id"] = true;
                    filter["included"][0]["id"] = true;
                    filter["included"][0]["attributes"]["headsign"] = true;

                    DeserializationError error = deserializeJson(
                        *prediction_data, https.getStream(), DeserializationOption::Filter(filter));
                    if (error)
                    {
                        Serial.print(F("deserializeJson() failed: "));
                        Serial.println(error.f_str());
                        return -1;
                    }
                    return 0;
                }
            }
        }
    }
    return -1;
}

JsonObject find_first_prediction_for_direction(
    JsonDocument *prediction_data_ptr, int direction)
{
    DynamicJsonDocument prediction_data = *prediction_data_ptr;

    for (int i = 0; i < prediction_data["data"].size(); i++)
    {
        JsonObject prediction = prediction_data["data"][i];
        int d = prediction["attributes"]["direction_id"];
        if (d == direction)
        {
            return prediction;
        }
    }
    Serial.println("We should never get here");
}

JsonObject find_trip_for_prediction(
    JsonDocument *prediction_data_ptr, JsonObject prediction)
{
    DynamicJsonDocument prediction_data = *prediction_data_ptr;

    for (int i = 0; i < prediction_data["included"].size(); i++)
    {
        JsonObject trip = prediction_data["included"][i];
        if (trip["id"] == prediction["relationships"]["trip"]["data"]["id"])
        {
            return trip;
        }
    }
}

double diff_with_local_time(String timestring)
{
    struct tm time;
    struct tm local_time;
    char timestring_char[32];
    timestring.toCharArray(timestring_char, 32);
    strptime(timestring_char, "%Y-%m-%dT%H:%M:%S", &time);
    getLocalTime(&local_time);
    return time_diff(local_time, time);
}

double time_diff(struct tm time1, struct tm time2)
{
    long start_seconds = time1.tm_sec + time1.tm_min * 60 + time1.tm_hour * 3600 + time1.tm_mday * 86400;
    long end_seconds = time2.tm_sec + time2.tm_min * 60 + time2.tm_hour * 3600 + time2.tm_mday * 86400;
    return end_seconds - start_seconds;
}

void determine_display_string(double arr_diff, double dep_diff, char *dst)
{
    if (arr_diff > 0)
    {
        if (arr_diff > 60)
        {
            int minutes = floor(arr_diff / 60.0);
            sprintf(dst, "%d min", minutes);
        }
        else
        {
            strcpy(dst, "ARR");
        }
    }
    else
    {
        if (dep_diff > 0)
        {
            strcpy(dst, "BRD");
        }
        else
        {
            strcpy(dst, "ERR");
        }
    }
}

void format_prediction(JsonObject prediction, JsonObject trip, Prediction *dst)
{
    String headsign = trip["attributes"]["headsign"];
    String arr_time = prediction["attributes"]["arrival_time"];
    String dep_time = prediction["attributes"]["departure_time"];
    String status = prediction["attributes"]["status"];
    headsign.toCharArray(dst->label, 16);
    Serial.printf("status: %s %d\n", status, status == NULL);
    if (!status.equals("null"))
    {
        status.substring(0, 7).toCharArray(dst->value, 16);
        return;
    }
    struct tm local_time;
    getLocalTime(&local_time);
    char display_string[16];
    if (arr_time && arr_time.length() > 0 && dep_time && dep_time.length() > 0)
    {
        double arr_diff = diff_with_local_time(arr_time);
        double dep_diff = diff_with_local_time(dep_time);
        determine_display_string(arr_diff, dep_diff, display_string);
    }
    else
    {
        strcpy(display_string, "");
    }
    Serial.printf("display string: %s\n", display_string);
    strcpy(dst->value, display_string);
}