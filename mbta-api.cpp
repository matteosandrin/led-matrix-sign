#include "mbta-api.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define MBTA_REQUEST "https://api-v3.mbta.com/predictions?"                 \
                     "api_key=***REMOVED***&"            \
                     "filter[stop]=place-harsq&"                            \
                     "filter[route]=Red&"                                   \
                     "fields[prediction]=arrival_time,status,direction_id&" \
                     "include=trip&"                                        \
                     "sort=arrival_time"

DynamicJsonDocument *prediction_data = new DynamicJsonDocument(8192);
WiFiClientSecure *wifi_client = new WiFiClientSecure;

int get_mbta_predictions(Prediction dst[2])
{

    int status = fetch_predictions(prediction_data);
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
                    filter["data"][0]["attributes"]["direction_id"] = true;
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

void format_prediction(JsonObject prediction, JsonObject trip, Prediction *dst)
{
    String headsign = trip["attributes"]["headsign"];
    String arrival_time = prediction["attributes"]["arrival_time"];
    headsign.toCharArray(dst->label, 16);
    arrival_time.substring(11, 19).toCharArray(dst->value, 16);
}