#include "server.h"

#include "../mbta/mbta.h"

namespace lms {

const char index_html[] PROGMEM = R"(
  <!DOCTYPE html/>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
    <meta http-equiv="Content-Type" content="text/html; charset=ISO-8859-1" />
    <style>
      body {
        font-family: system-ui, Roboto, Helvetica;
      }
    </style>
    <title>LED Matrix Display</title>
  </head>
  <body>
    <h1>LED Matrix Display</h1>
    <form method="GET" action="/mode">
      <h2>Set sign mode</h2>
      <select name="id">
        <option value="0">SIGN_MODE_TEST</option>
        <option value="1">SIGN_MODE_MBTA</option>
        <option value="2">SIGN_MODE_CLOCK</option>
        <option value="3">SIGN_MODE_MUSIC</option>
      </select>
      <input type="submit" value="Set sign mode">
    </form>
    <form method="GET" action="/set">
      <h2>Set MBTA station</h2>
      <input name="key" type="hidden" value="station">
      <select name="value">
        <option value="0">Alewife</option>
        <option value="1">Davis</option>
        <option value="2">Porter</option>
        <option value="3">Harvard</option>
        <option value="4">Central</option>
        <option value="5">Kendall/MIT</option>
        <option value="6">Charles/MGH</option>
        <option value="7">Park Street</option>
        <option value="8">Downtown Crossing</option>
        <option value="9">South Station</option>
        <option value="10">Test Station</option>
      </select>
      <input type="submit" value="Set station">
    </form>
  </body>
)";

void Server::setup(SignMode sign_mode, QueueSetHandle_t ui_queue) {
  this->sign_mode = sign_mode;
  this->ui_queue = ui_queue;
  this->setup_index();
  this->setup_mode();
  this->setup_set();
  this->server.begin();
}

void Server::setup_index() {
  this->server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });
}

void Server::setup_mode() {
  this->server.on("/mode", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (request->hasParam("id")) {
      String sign_mode_str = request->getParam("id")->value();
      int sign_mode = sign_mode_str.toInt();
      if (0 <= sign_mode && sign_mode < SIGN_MODE_MAX) {
        UIMessage message;
        message.type = UI_MESSAGE_TYPE_MODE_CHANGE;
        message.next_sign_mode = (SignMode)sign_mode;
        request->redirect("/");
        if (xQueueSend(this->ui_queue, (void *)&message, TEN_MILLIS)) {
          return;
        }
      }
      request->send(500, "text/plain", "invalid sign mode: " + sign_mode_str);
    }
    request->send(500, "text/plain", "missing query parameter 'id'");
  });
}

void Server::setup_set() {
  this->server.on("/set", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (request->hasParam("key") && request->hasParam("value")) {
      String key = request->getParam("key")->value();
      String value = request->getParam("value")->value();

      if (key == "station") {
        int station = value.toInt();
        if (0 <= station < TRAIN_STATION_MAX) {
          UIMessage message;
          message.type = UI_MESSAGE_TYPE_MBTA_CHANGE_STATION;
          message.next_station = (TrainStation)station;
          if (xQueueSend(this->ui_queue, (void *)&message, TEN_MILLIS)) {
            request->redirect("/");
            return;
          }
        } else {
          request->send(500, "text/plain", "invalid station id: " + value);
        }
      } else {
        request->send(500, "text/plain", "unknown key '" + key + "'");
      }
    }
    request->send(500, "text/plain", "missing query parameter 'id'");
  });
}

} /* namespace lms */