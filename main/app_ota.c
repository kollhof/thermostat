#include "unistd.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_task.h"
#include "esp_log.h"
#include "esp_https_ota.h"
#include "esp_log.h"

#include "./app_events.h"


#define STACK_SIZE 5048

static const char* TAG = "app-ota";


static void ota_task(void * arg) {
  ESP_LOGI(TAG, "OTA task started ...");
  app_post_event(APP_EVENT_OTA_STARTED, NULL, 0);

  esp_err_t ret = esp_https_ota((esp_http_client_config_t *)arg);

  if (ret == ESP_OK) {
    app_post_event(APP_EVENT_OTA_SUCCESS, NULL, 0);
    ESP_LOGI(TAG, "OTA upgrade success.");
  } else {
    app_post_event(APP_EVENT_OTA_FAILED, &ret, sizeof(ret));
    ESP_LOGE(TAG, "OTA upgrade failed.");
  }

  app_post_event(APP_EVENT_RESTART, NULL, 0);
  ESP_LOGI(TAG, "OTA waiting for restart...");
  sleep(5);
}


static void handle_ota(void* arg, esp_event_base_t base, int32_t id, void* data) {
  ESP_LOGI(TAG, "OTA requested ...");

  TaskHandle_t task_handle;
  xTaskCreate(ota_task, "ota-task", STACK_SIZE, arg, ESP_TASK_MAIN_PRIO, &task_handle);
  // TODO: vTaskDelete(task_handle);
}


void app_start_ota_handler(esp_http_client_config_t * config) {
  app_register_evt_handler(APP_EVENT_OTA, handle_ota, config);
  ESP_LOGI(TAG, "OTA updater started");
}

