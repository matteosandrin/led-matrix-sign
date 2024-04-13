#include <ESPAsyncWebServer.h>
#include <freertos/queue.h>

namespace lms {

class Server {
  AsyncWebServer server;
  QueueSetHandle_t ui_queue;
  void setup_index();
  void setup_mode();
  void setup_set();

 public:
  Server() : server(80) {}
  void setup(QueueHandle_t ui_queue);
};

}; /* namespace lms */