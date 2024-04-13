#include "client.h"

namespace lms {

void Client::setup(int json_doc_size) {
  this->data = new DynamicJsonDocument(json_doc_size);
  this->wifi_client = new WiFiClientSecure;
}


} /* namespace lms */
