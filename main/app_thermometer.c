#include <string.h>

#include "esp_task_wdt.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

// #include "temp_sensor.h"

#include "./app_events.h"


#define TAG "app-thermometer"


static void post_curr_temp_change_event(float temp) {
  app_post_event(APP_EVENT_CURRENT_TEMP_CHANGED, &temp, sizeof(temp));
}



static void post_ble_temp_change_event(float temp) {
  app_post_event(APP_EVENT_BLE_TEMP_CHANGED, &temp, sizeof(temp));
}


static void post_ble_humid_change_event(float humid) {
  app_post_event(APP_EVENT_CURRENT_HUMID_CHANGED, &humid, sizeof(humid));
}


static esp_ble_scan_params_t ble_scan_params = {
  .scan_type        = BLE_SCAN_TYPE_PASSIVE,
  .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
  .scan_filter_policy   = BLE_SCAN_FILTER_ALLOW_ONLY_WLST,
  .scan_interval      = 0x50, // N = 0x0800 (1.28 second) Time = N * 0.625 msec
  .scan_window      = 0x30, // N = 0x0800 (1.28 second) Time = N * 0.625 msec
  .scan_duplicate     = BLE_SCAN_DUPLICATE_DISABLE
};



static void gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (event == ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT) {
    ESP_LOGI(TAG, "GAP: ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT");
    // TODO: the unit of the duration is second
    uint32_t duration = 60 * 60 * 24 * 356;
    esp_ble_gap_start_scanning(duration);
  }

  if (event == ESP_GAP_BLE_SCAN_START_COMPLETE_EVT) {
    ESP_LOGI(TAG, "GAP: ESP_GAP_BLE_SCAN_START_COMPLETE_EVT");
    if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
      ESP_LOGE(TAG, "scan start failed, error status = %x", param->scan_start_cmpl.status);
    }
  }

  if (event == ESP_GAP_BLE_SCAN_RESULT_EVT) {
    esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
    if (scan_result->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
      if (scan_result->scan_rst.adv_data_len == 29) {
        // esp_ble_gap_stop_scanning();

        // Len,Type,Value
        // 02 01 06  - Advertising flags
        // 03 02 F0 FF
        // .. .. .. ID ID ID ID ID ID ID ID ID ..
        // 15 FF 10 00 00 00 37 01 00 00 15 6F 7C
        // .. TEMPE HUMID .. .. .. ..
        // 0B BA 01 39 02 29 51 09 00  - Manufacturer Specific Data
        // 0D 09 54 68 65 72 6D 6F 42 65 61 63 6F 6E
        // 05 12 18 00 38 01
        // 02 0A 00
        // Temprature: 0x01BA = 442(dec) => 442/16 = 27,625 Celsius
        // Humidity: 0x0239 = 569(dec) => 569/16 = 35,5625 %

        // ESP_LOGI(TAG, "adv data:");
        // esp_log_buffer_hex(TAG, &scan_result->scan_rst.ble_adv[0], scan_result->scan_rst.adv_data_len);
        // esp_log_buffer_hex(TAG, &scan_result->scan_rst.ble_adv[21], 4);
        uint16_t temp_dec = 0;
        memcpy(&temp_dec, &scan_result->scan_rst.ble_adv[21], 2);
        float temp = temp_dec / 16.0;

        uint16_t humid_dec = 0;
        memcpy(&humid_dec, &scan_result->scan_rst.ble_adv[23], 2);
        float humid = humid_dec / 16.0;
        post_ble_temp_change_event(temp);
        post_ble_humid_change_event(humid);
      }
    }
  }

  if (event == ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT) {
    ESP_LOGI(TAG, "GAP ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT");
    if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
      ESP_LOGE(TAG, "scan stop failed, error status = %x", param->scan_stop_cmpl.status);
    }
  }
}



static void start_ble_thermometer(esp_bd_addr_t addr) {
  ESP_LOGI(TAG, "starting BLE thermometer");

  // ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

  esp_bt_controller_status_t s = esp_bt_controller_get_status();
  ESP_LOGE(TAG, "BT status: %u", s);

  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  esp_err_t ret = esp_bt_controller_init(&bt_cfg);
  if (ret) {
    ESP_LOGE(TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
    return;
  }

  ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
  if (ret) {
    ESP_LOGE(TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
    return;
  }

  ret = esp_bluedroid_init();
  if (ret) {
    ESP_LOGE(TAG, "%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
    return;
  }

  ret = esp_bluedroid_enable();
  if (ret) {
    ESP_LOGE(TAG, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
    return;
  }

  //register the  callback function to the gap module
  ret = esp_ble_gap_register_callback(gap_callback);
  if (ret){
    ESP_LOGE(TAG, "%s gap register failed, error code = %x\n", __func__, ret);
    return;
  }

  esp_ble_gap_update_whitelist(ESP_BLE_WHITELIST_ADD, addr, BLE_WL_ADDR_TYPE_PUBLIC);

  esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
  if (scan_ret){
    ESP_LOGE(TAG, "set scan params error, error code = %x", scan_ret);
  }
}



static void handle_ble_temp_changed(void *arg, esp_event_base_t evt_base, int32_t id, void *data) {
  float temp = * (float*) data;

  ESP_LOGI(TAG, "BLE temp: %f", temp);

  esp_task_wdt_reset();

  post_curr_temp_change_event(temp);
}



static void handle_app_started(void *arg, esp_event_base_t evt_base, int32_t id, void *data) {
  // watchdog for BLE, careful not to set it lower than OTA update time
  ESP_LOGI(TAG, "adding watchdog");
  esp_task_wdt_init(5*60, true);
  esp_task_wdt_add(NULL);
}


static void handle_ota_request(void *arg, esp_event_base_t evt_base, int32_t id, void *data) {
  ESP_LOGI(TAG, "removing watchdog");
  esp_task_wdt_delete(NULL);
}


void app_start_thermometer(gpio_num_t gpio_temp, esp_bd_addr_t addr) {
  app_register_evt_handler(APP_EVENT_BLE_TEMP_CHANGED, handle_ble_temp_changed, 0);
  app_register_evt_handler(APP_EVENT_STARTED, handle_app_started, NULL);
  app_register_evt_handler(APP_EVENT_OTA, handle_ota_request, NULL);

  start_ble_thermometer(addr);
}


