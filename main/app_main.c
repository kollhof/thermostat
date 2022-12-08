#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_event.h"
#include "esp_https_ota.h"
#include "esp_gatt_defs.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "unistd.h"
#include "driver/gpio.h"

#include "./app_wifi.h"
#include "./app_homekit.h"
#include "./app_mqtt.h"
#include "./app_timekeeper.h"
#include "./app_thermostat.h"
#include "./app_thermometer.h"
#include "./app_ota.h"
#include "./app_events.h"
#include "./app_restarter.h"
#include "./app_stats.h"


static const char* TAG = "app";


typedef struct {
  char * hw_model;
  char * hw_serial;
  char * hw_rev;

  uint8_t gpio_pwm;
  uint8_t gpio_temp;
  uint8_t gpio_led;
  uint8_t heat_min;
  uint8_t heat_normal;
  uint8_t heat_max;
  uint8_t heat_cycle_sec;
  float target_temp;

  esp_bd_addr_t ble_themometer_addr;

  char * mqtt_uri;
  char * mqtt_root_ca;
  char * mqtt_client_cert;
  char * mqtt_client_key;

  char * ota_uri;
  char * ota_cert;

} app_config_t;




static void init_logging() {
  esp_log_level_set("*", ESP_LOG_VERBOSE);
  esp_log_level_set("nvs", ESP_LOG_VERBOSE);
  esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
  esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);
  esp_log_level_set("BTDM_INIT", ESP_LOG_VERBOSE);
  esp_log_level_set("simple_ble", ESP_LOG_VERBOSE);

  ESP_LOGI(TAG, "logging initialized");
}



static void init_nvs() {
  ESP_LOGI(TAG, "init nvs");

  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_ERROR_CHECK(nvs_flash_init_partition("factory_nvs"));
}



static void init_system() {
  init_logging();

  ESP_LOGI(TAG, "starting app...");
  ESP_LOGI(TAG, "free memory: %d bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "IDF version: %s", esp_get_idf_version());

  esp_event_loop_create_default();

  init_nvs();
}



static esp_err_t get_u8(nvs_handle_t handle, const char *key, uint8_t *out_value) {
  esp_err_t err = nvs_get_u8(handle, key, out_value);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error reading %s: %s NVS", key, esp_err_to_name(err));
  } else {
    ESP_LOGI(TAG, "Loaded %s: %u", key, *out_value);
  }
  return err;
}


static esp_err_t get_str(nvs_handle_t handle, const char *key, char **out_value) {
  size_t required_size;

  esp_err_t err = nvs_get_str(handle, key, NULL, &required_size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error reading %s: %s NVS", key, esp_err_to_name(err));
  } else {
    char* value = malloc(required_size);
    nvs_get_str(handle, key, value, &required_size);
    ESP_LOGI(TAG, "Loaded %s: %s", key, value);
    *out_value = value;
  }
  return err;
}


static esp_err_t get_blob(nvs_handle_t handle, const char *key, char *out_value) {
  size_t required_size;

  esp_err_t err = nvs_get_blob(handle, key, NULL, &required_size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error reading %s: %s NVS", key, esp_err_to_name(err));
  } else {
    nvs_get_blob(handle, key, out_value, &required_size);
    ESP_LOGI(TAG, "Loaded %s: %d", key, required_size);
    esp_log_buffer_hex(TAG, (char *) out_value, required_size);
  }
  return err;
}



char * get_serial_number() {
  uint8_t mac[6] = {0};
  esp_efuse_mac_get_default(mac);

  char * str = malloc(16);
  snprintf(str, 16, "%02x%02x-%02x%02x-%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return str;
}



static esp_err_t init_app_config(app_config_t * config) {
  ESP_LOGI(TAG, "init config...");


  config->hw_serial = get_serial_number();

  nvs_handle handle;
  esp_err_t err = nvs_open_from_partition("factory_nvs", "app", NVS_READONLY, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle for 'factory_nvs:app'", esp_err_to_name(err));
    return err;
  }

  err = get_str(handle, "hw_model", &config->hw_model);
  err = get_str(handle, "hw_rev", &config->hw_rev);

  err = get_u8(handle, "gpio_pwm", &config->gpio_pwm);
  err = get_u8(handle, "gpio_temp", &config->gpio_temp);
  err = get_u8(handle, "gpio_led", &config->gpio_led);

  err = get_u8(handle, "heat_min", &config->heat_min);
  err = get_u8(handle, "heat_normal", &config->heat_normal);
  err = get_u8(handle, "heat_max", &config->heat_max);
  err = get_u8(handle, "heat_cycle", &config->heat_cycle_sec);

  err = get_blob(handle, "ble_thermo_addr", (char *) &config->ble_themometer_addr);

  err = get_str(handle, "mqtt_uri", &config->mqtt_uri);
  err = get_str(handle, "root_cert_pem", &config->mqtt_root_ca);
  err = get_str(handle, "client_cert_pem", &config->mqtt_client_cert);
  err = get_str(handle, "client_key_pem", &config->mqtt_client_key);

  err = get_str(handle, "ota_uri", &config->ota_uri);
  err = get_str(handle, "ota_cert", &config->ota_cert);

  nvs_close(handle);

  return err;
}



static float load_target_temp(float default_target_temp) {
  float target_temp = default_target_temp;

  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
  } else {

    err = nvs_get_u32(nvs_handle, "target_temp", (uint32_t*)&target_temp);
    switch (err) {
      case ESP_OK:
        ESP_LOGI(TAG, "Read target_temp %0.3f from storage", target_temp);
        break;

      case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGE(TAG, "The target_temp is not in storage yet!");
        target_temp = default_target_temp;
        break;

      default :
        ESP_LOGE(TAG, "Error (%s) reading NVS", esp_err_to_name(err));
        target_temp = default_target_temp;
        break;
    }

    nvs_close(nvs_handle);
  }

  return target_temp;
}


static void patch_config() {

  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open_from_partition("factory_nvs", "app", NVS_READWRITE, &nvs_handle);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
  } else {
    nvs_set_u8(nvs_handle, "heat_min", 0);
    nvs_set_u8(nvs_handle, "heat_normal", 50);
    nvs_set_u8(nvs_handle, "heat_max", 80);

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error (%s) committing storage", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
  }
}



void app_main(void) {
  ESP_LOGI(TAG, "starting...");
  init_system();

  patch_config();

  app_config_t conf = {
    .gpio_pwm = 25,
    .gpio_temp = 26,
    .gpio_led = 27,
    .heat_min = 0,
    .heat_normal = 50,
    .heat_max = 70,
    .heat_cycle_sec = 1,
    .target_temp = load_target_temp(15),
    .ble_themometer_addr = {0},
  };
  init_app_config(&conf);

  esp_http_client_config_t ota_config = {
    .url = conf.ota_uri,
    .cert_pem = conf.ota_cert,
  };

  esp_mqtt_client_config_t mqtt_config = {
    .uri = conf.mqtt_uri,
    .cert_pem = conf.mqtt_root_ca,
    .client_cert_pem = conf.mqtt_client_cert,
    .client_key_pem = conf.mqtt_client_key,
  };

  app_init_networking();

  app_start_mqtt(&mqtt_config, conf.hw_serial);

  app_start_networking(portMAX_DELAY);

  app_start_homekit(conf.hw_model, conf.hw_rev, conf.hw_serial, conf.target_temp);

  app_start_thermometer(conf.gpio_temp, conf.ble_themometer_addr);

  app_start_restart_handler();

  app_start_ota_handler(&ota_config);

  app_start_timekeeper();

  app_start_stats_handler(conf.gpio_led);

  app_start_thermostat(
    conf.gpio_pwm,
    conf.heat_min, conf.heat_normal, conf.heat_max,
    conf.heat_cycle_sec,
    conf.target_temp
  );

  app_post_event(APP_EVENT_STARTED, NULL, 0);

  // TODO: should wait for a restart event
  ESP_LOGI(TAG, "all tasks started");
  while (true) {
    sleep(1);
  }
  // fflush(stdout);
  // esp_restart();
}
