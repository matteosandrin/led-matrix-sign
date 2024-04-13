#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#ifndef LMS_CLIENT_H
#define LMS_CLIENT_H

namespace lms {

class Client {
 protected:
  DynamicJsonDocument *data;
  WiFiClientSecure *wifi_client;
  HTTPClient http_client;

 public:
  void setup(int json_doc_size);
};

} /* namespace lms */

#endif /* LMS_CLIENT_H */
