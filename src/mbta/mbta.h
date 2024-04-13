#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <map>

#ifndef MBTA_API_H
#define MBTA_API_H

#define DIRECTION_SOUTHBOUND 0
#define DIRECTION_NORTHBOUND 1

struct Prediction {
  char label[32];
  char value[16];
};

enum PredictionStatus {
  PREDICTION_STATUS_OK,
  PREDICITON_STATUS_OK_SHOW_ARR_BANNER_SLOT_1,
  PREDICITON_STATUS_OK_SHOW_ARR_BANNER_SLOT_2,
  PREDICTION_STATUS_OK_SHOW_STATION_BANNER,
  PREDICTION_STATUS_ERROR,
  PREDICTION_STATUS_SKIP,
};

enum TrainStation {
  TRAIN_STATION_ALEWIFE,
  TRAIN_STATION_DAVIS,
  TRAIN_STATION_PORTER,
  TRAIN_STATION_HARVARD,
  TRAIN_STATION_CENTRAL,
  TRAIN_STATION_KENDALL,
  TRAIN_STATION_CHARLES_MGH,
  TRAIN_STATION_PARK_STREET,
  TRAIN_STATION_DOWNTOWN_CROSSING,
  TRAIN_STATION_SOUTH_STATION,
  TRAIN_STATION_MAX,
};

class MBTA {
  DynamicJsonDocument *prediction_data;
  WiFiClientSecure *wifi_client;
  HTTPClient http_client;
  Prediction latest_predictions[2];
  std::map<TrainStation, char *> train_station_codes = {
      {TRAIN_STATION_ALEWIFE, "place-alfcl"},
      {TRAIN_STATION_DAVIS, "place-davis"},
      {TRAIN_STATION_PORTER, "place-portr"},
      {TRAIN_STATION_HARVARD, "place-harsq"},
      {TRAIN_STATION_CENTRAL, "place-cntsq"},
      {TRAIN_STATION_KENDALL, "place-knncl"},
      {TRAIN_STATION_CHARLES_MGH, "place-chmnl"},
      {TRAIN_STATION_PARK_STREET, "place-pktrm"},
      {TRAIN_STATION_DOWNTOWN_CROSSING, "place-dwnxg"},
      {TRAIN_STATION_SOUTH_STATION, "place-sstat"},
  };
  TrainStation station;
  bool has_station_changed;

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

  void update_latest_predictions(Prediction latest[], int directions[]);

  bool show_arriving_banner(Prediction *prediction, int direction);

 public:
  void setup();
  PredictionStatus get_predictions_both_directions(Prediction dst[2]);

  PredictionStatus get_predictions_one_direction(Prediction dst[2],
                                                 int direction);

  void get_placeholder_predictions(Prediction dst[2]);
  void set_station(TrainStation station);
};

char *train_station_to_str(TrainStation station);

#endif /* MBTA_API_H */
