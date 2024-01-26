#include <ArduinoJson.h>

struct Prediction {
  char label[16];
  char value[16];
};

enum PredictionStatus {
  PREDICTION_STATUS_OK,
  PREDICTION_STATUS_ERROR,
  PREDICTION_STATUS_SKIP
};

PredictionStatus get_mbta_predictions(Prediction dst[2]);

int fetch_predictions(JsonDocument *prediction_data);

JsonObject find_first_prediction_for_direction(
    JsonDocument *prediction_data_ptr, int direction);

JsonObject find_trip_for_prediction(JsonDocument *prediction_data_ptr,
                                    JsonObject prediction);

void format_prediction(JsonObject prediction, JsonObject trip, Prediction *dst);

double diff_with_local_time(String timestring);

double time_diff(struct tm time1, struct tm time2);

void determine_display_string(double arr_diff, double dep_diff, char *dst);
