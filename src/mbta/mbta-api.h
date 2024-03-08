#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

#ifndef MBTA_API_H
#define MBTA_API_H

#define DIRECTION_SOUTHBOUND 0
#define DIRECTION_NORTHBOUND 1

struct Prediction {
  char label[16];
  char value[16];
};

enum PredictionStatus {
  PREDICTION_STATUS_OK,
  PREDICTION_STATUS_ERROR,
  PREDICTION_STATUS_SKIP
};

class MBTA {
  DynamicJsonDocument *prediction_data;
  WiFiClientSecure *wifi_client;
  PredictionStatus get_predictions(Prediction *dst, int num_predictions,
                                   int directions[], int nth_positions[]);
  int fetch_predictions(JsonDocument *prediction_data);

  JsonObject find_nth_prediction_for_direction(
      JsonDocument *prediction_data_ptr, int direction, int n);

  JsonObject find_trip_for_prediction(JsonDocument *prediction_data_ptr,
                                      JsonObject prediction);

  void format_prediction(JsonObject prediction, JsonObject trip,
                         Prediction *dst);

  int diff_with_local_time(String timestring);

  int datetime_diff(struct tm time1, struct tm time2);

  int datetime_to_epoch(struct tm dt);

  void determine_display_string(int arr_diff, int dep_diff, String status,
                                char *dst);

 public:
  MBTA();
  PredictionStatus get_predictions_both_directions(Prediction dst[2]);

  PredictionStatus get_predictions_one_direction(Prediction dst[2],
                                                 int direction);

  void get_placeholder_predictions(Prediction dst[2]);
};

#endif /* MBTA_API_H */
