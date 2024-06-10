#include <ESPAsyncWebServer.h>
#include <freertos/queue.h>

#include "../../common.h"

#ifndef LMS_SERVER_H
#define LMS_SERVER_H
namespace lms {

class Server {
  AsyncWebServer server;
  SignMode sign_mode;
  QueueSetHandle_t ui_queue;
  void setup_index();
  void setup_mode();
  void setup_set();

 public:
  Server() : server(80) {}
  void setup(SignMode sign_mode, QueueHandle_t ui_queue);
};

}; /* namespace lms */

#endif /* LMS_SERVER_H */
