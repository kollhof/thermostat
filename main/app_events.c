#include "esp_event.h"

#include "./app_events.h"


ESP_EVENT_DEFINE_BASE(APP_EVENT_BASE);


void app_post_event(app_event_t evt_id, void *evt_data, size_t evt_data_size) {
  esp_event_post(APP_EVENT_BASE, evt_id, evt_data, evt_data_size, portMAX_DELAY);
}


void app_register_evt_handler(app_event_t evt_id, esp_event_handler_t handler, void *handler_arg) {
  esp_event_handler_register(APP_EVENT_BASE, evt_id, handler, handler_arg);
}
