#include "esp_log.h"

#include "./app_events.h"


static const char* TAG = "app-restarter";


static void handle_restart(void* arg, esp_event_base_t base, int32_t id, void* data) {
  ESP_LOGI(TAG, "restarting ...");
  fflush(stdout);
  esp_restart();
}


void app_start_restart_handler() {
  ESP_LOGI(TAG, "starting restart handler");
  app_register_evt_handler(APP_EVENT_RESTART, handle_restart, NULL);
}

