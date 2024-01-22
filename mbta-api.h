#include <ArduinoJson.h>

struct Prediction
{
    char label[16];
    char value[16];
};

int get_mbta_predictions(Prediction dst[2]);

int fetch_predictions(DynamicJsonDocument *prediction_data);

JsonObject find_first_prediction_for_direction(
    DynamicJsonDocument *prediction_data_ptr, int direction);

JsonObject find_trip_for_prediction(
    DynamicJsonDocument *prediction_data_ptr, JsonObject prediction);

void format_prediction(JsonObject prediction, JsonObject trip, Prediction *dst);
