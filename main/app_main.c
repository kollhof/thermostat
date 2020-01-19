#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_https_ota.h"
#include "nvs_flash.h"
#include "mqtt_client.h"

#include "./app_wifi.h"
#include "./app_mqtt.h"
#include "./app_timekeeper.h"
#include "./app_thermostat.h"
#include "./app_ota.h"
#include "./app_events.h"
#include "./app_restarter.h"
#include "./app_stats.h"


static const char* TAG = "app";

extern const uint8_t ota_ca_cert_pem[] asm("_binary_ota_ca_cert_pem_start");
extern const uint8_t ota_ca_cert_pem_end[] asm("_binary_ota_ca_cert_pem_start");

extern const uint8_t aws_root_ca_pem[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_ca_pem_end");

extern const uint8_t aws_certificate_pem_crt[] asm("_binary_aws_certificate_pem_crt_start");
extern const uint8_t aws_certificate_pem_crt_end[] asm("_binary_aws_certificate_pem_crt_end");

extern const uint8_t aws_private_pem_key[] asm("_binary_aws_private_pem_key_start");
extern const uint8_t aws_private_pem_key_end[] asm("_binary_aws_private_pem_key_end");


static void init_logging() {
  esp_log_level_set("*", ESP_LOG_VERBOSE);
  esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
  esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);
}


static void init_nvs() {
  ESP_LOGI(TAG, "init nvs");

  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}


static void init_system() {
  init_logging();

  ESP_LOGI(TAG, "starting app...");
  ESP_LOGI(TAG, "free memory: %d bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "IDF version: %s", esp_get_idf_version());

  esp_event_loop_create_default();
  init_nvs();
}


void app_main(void) {
  ESP_LOGI(TAG, "starting...");
  init_system();

  const char * device_id = CONFIG_APP_DEVICE_ID;

  wifi_config_t wifi_config = {
    .sta = {
      .ssid = CONFIG_APP_WIFI_SSID,
      .password = CONFIG_APP_WIFI_PASSWORD
    },
  };

  esp_mqtt_client_config_t mqtt_config = {
    .uri = CONFIG_APP_MQTT_URI,
    .cert_pem = (const char *) aws_root_ca_pem,
    .client_cert_pem = (const char *) aws_certificate_pem_crt,
    .client_key_pem = (const char *) aws_private_pem_key,
  };

  esp_http_client_config_t ota_config = {
    .url = CONFIG_APP_OTA_URI,
    .cert_pem = (char *)ota_ca_cert_pem,
  };

  // TODO: pull from board config
  const gpio_num_t gpio_pwm = CONFIG_APP_PWM_GPIO;
  const gpio_num_t gpio_temp = CONFIG_APP_TEMP_GPIO;

  app_start_network(&wifi_config);
  app_start_mqtt(&mqtt_config, device_id);

  app_start_restart_handler();
  app_start_ota_handler(&ota_config);
  app_start_timekeeper();
  app_start_stats_handler();

  app_start_thermostat(gpio_pwm, gpio_temp);

  // TODO: should never reach this point
  ESP_LOGI(TAG, "main task returned unexpectedly, restarting ...");
  fflush(stdout);
  esp_restart();
}
