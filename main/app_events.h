#pragma once

#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif


ESP_EVENT_DECLARE_BASE(APP_EVENT_BASE);

typedef enum {
  APP_EVENT_OTA,
  APP_EVENT_OTA_STARTED,
  APP_EVENT_OTA_SUCCESS,
  APP_EVENT_OTA_FAILED,

  APP_EVENT_RESTART,

  APP_EVENT_THERMOSTAT_GET,
  APP_EVENT_THERMOSTAT_SET,
  APP_EVENT_THERMOSTAT_CHANGED,

  APP_EVENT_TIME_UPDATED
} app_event_t;


void app_post_event(app_event_t evt_id, void *evt_data, size_t evt_data_size);

void app_register_evt_handler(app_event_t evt_id, esp_event_handler_t handler, void *handler_arg);


#ifdef __cplusplus
}
#endif