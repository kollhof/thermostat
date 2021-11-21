#include "stdio.h"
#include "string.h"
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"

#include "hap.h"
#include "hap_apple_servs.h"
#include "hap_apple_chars.h"

#include "hap_fw_upgrade.h"

#include "./app_thermostat.h"
#include "./app_events.h"


/*  Required for server verification during OTA, PEM format as string  */
char server_cert[] = {};

static const char *TAG = "app-homekit";

#define THERMO_TASK_PRIORITY  1
#define THERMO_TASK_STACKSIZE 4 * 1024
#define THERMO_TASK_NAME      "hap_thermo"



static void hap_set_float(hap_serv_t * hs, const char * type_uuid, float val) {
  hap_val_t hval = {.f = val};
  hap_char_t * ch = hap_serv_get_char_by_uuid(hs, type_uuid);
  hap_char_update_val(ch, &hval);
}


static void hap_set_uint(hap_serv_t * hs, const char * type_uuid, uint32_t val) {
  hap_val_t hval = {.u = val};
  hap_char_t * ch = hap_serv_get_char_by_uuid(hs, type_uuid);

  if (0 != hap_char_update_val(ch, &hval)) {
    ESP_LOGE(TAG, "Err setting %s, %d", type_uuid, val);
  };
}



static int thermo_identify(hap_acc_t *ha) {
  // TODO: flash status LED a few times, i.e. dispatch app event to do so
  ESP_LOGI(TAG, "accessory identified");
  app_post_event(APP_EVENT_IDENTIFY, NULL, 0);
  return HAP_SUCCESS;
}



static void thermo_hap_event_handler(void* arg, esp_event_base_t event_base, int event, void *data) {
  switch(event) {
    case HAP_EVENT_PAIRING_STARTED :
      ESP_LOGI(TAG, "Pairing Started");
      break;
    case HAP_EVENT_PAIRING_ABORTED :
      ESP_LOGI(TAG, "Pairing Aborted");
      break;
    case HAP_EVENT_CTRL_PAIRED :
      ESP_LOGI(TAG, "Controller %s Paired. Controller count: %d",
        (char *)data, hap_get_paired_controller_count());
      break;
    case HAP_EVENT_CTRL_UNPAIRED :
      ESP_LOGI(TAG, "Controller %s Removed. Controller count: %d",
        (char *)data, hap_get_paired_controller_count());
      break;
    case HAP_EVENT_CTRL_CONNECTED :
      ESP_LOGI(TAG, "Controller %s Connected", (char *)data);
      break;
    case HAP_EVENT_CTRL_DISCONNECTED :
      ESP_LOGI(TAG, "Controller %s Disconnected", (char *)data);
      break;
    case HAP_EVENT_ACC_REBOOTING : {
      char *reason = (char *)data;
      ESP_LOGI(TAG, "Accessory Rebooting (Reason: %s)",  reason ? reason : "null");
      break;
    }
    default:
      /* Silently ignore unknown events */
      break;
  }
}



static int thermo_write(hap_write_data_t write_data[], int count, void *serv_priv, void *write_priv) {
  if (hap_req_get_ctrl_id(write_priv)) {
    ESP_LOGI(TAG, "Received write from %s", hap_req_get_ctrl_id(write_priv));
  }

  ESP_LOGI(TAG, "Write called with %d chars", count);
  int i, ret = HAP_SUCCESS;
  hap_write_data_t *write;

  for (i = 0; i < count; i++) {
    write = &write_data[i];
    *(write->status) = HAP_STATUS_SUCCESS;
    const char * type_uuid = hap_char_get_type_uuid(write->hc);
    ESP_LOGI(TAG, "write char %s", type_uuid);
    hap_char_update_val(write->hc, &(write->val));

    if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_TARGET_TEMPERATURE)) {
      ESP_LOGI(TAG, "write HAP_CHAR_UUID_TARGET_TEMPERATURE");

      float target_temp = write->val.f;
      app_post_event(APP_EVENT_TARGET_TEMP_CHANGED, &target_temp, sizeof(target_temp));
    }
  }
  return ret;
}



static void handle_thermo_change(void* arg, esp_event_base_t evt_base, int32_t evt_id, void* data) {
  hap_serv_t * service = (hap_serv_t *) arg;
  app_thermostat_state_t * state = (app_thermostat_state_t*) data;

  float temp = roundf(state->current_temp * 10) / 10;

  hap_set_float(service, HAP_CHAR_UUID_CURRENT_TEMPERATURE, temp);
  // hap_set_float(service, HAP_CHAR_UUID_TARGET_TEMPERATURE, stats->target_temp);

  if (state->heat < 10) {
    hap_set_uint(service, HAP_CHAR_UUID_CURRENT_HEATING_COOLING_STATE, 0);
  } else {
    hap_set_uint(service, HAP_CHAR_UUID_CURRENT_HEATING_COOLING_STATE, 1);
  }

  if (state->temp_state != APP_THERMOSTAT_TEMP_OK) {
    ESP_LOGE(TAG, "state error %d", state->temp_state);
    hap_set_uint(service, HAP_CHAR_UUID_STATUS_LOW_BATTERY, 1);

  } else {
    ESP_LOGE(TAG, "state OK %d", state->temp_state);
    hap_set_uint(service, HAP_CHAR_UUID_STATUS_LOW_BATTERY, 0);
  }
}



void app_start_homekit(char * model, char * hw_rev, char * serial_num, float target_temp) {

  /* Configure HomeKit core to make the Accessory name (and thus the WAC SSID) unique,
   * instead of the default configuration wherein only the WAC SSID is made unique.
   */
  hap_cfg_t hap_cfg;
  hap_get_config(&hap_cfg);
  hap_cfg.unique_param = UNIQUE_NAME;
  hap_set_config(&hap_cfg);

  hap_init(HAP_TRANSPORT_WIFI);

  // TODO: get as arg
  const esp_app_desc_t * desc = esp_ota_get_app_description();
  char * fw_rev = (char *) desc->version;

  hap_acc_cfg_t cfg = {
    .name = "Heißes Eisen",
    .manufacturer = "kollhof",
    .model = model,
    .serial_num = serial_num,
    .hw_rev = hw_rev,
    .fw_rev = fw_rev,
    .pv = "1.1",
    .identify_routine = thermo_identify,
    .cid = HAP_CID_THERMOSTAT,
  };
  hap_acc_t * accessory = hap_acc_create(&cfg);

  // TODO should this be changed
  uint8_t product_data[] = {'E','S','P','3','2','H','A','P'};
  hap_acc_add_product_data(accessory, product_data, sizeof(product_data));

  // TODO: get initial values from main
  hap_serv_t * service = hap_serv_thermostat_create(
    0 /*0=OFF, 1=HEAT, 2=COOL*/,
    3 /* TODO: 3=AUTO */,
    20, target_temp,
    0 /* 0=Celsius */
  );

  hap_char_t * batt = hap_char_status_low_battery_create(0);
  hap_serv_add_char(service, batt);

  hap_serv_set_write_cb(service, thermo_write);

  hap_acc_add_serv(accessory, service);
  // hap_acc_add_serv(accessory, temp_srv);

  hap_add_accessory(accessory);

  /* Register an event handler for HomeKit specific events */
  esp_event_handler_register(HAP_EVENT, ESP_EVENT_ANY_ID, &thermo_hap_event_handler, NULL);

  // TODO: why is hs != service
  hap_serv_t * hs = hap_acc_get_serv_by_uuid(accessory, HAP_SERV_UUID_THERMOSTAT);

  app_register_evt_handler(APP_EVENT_THERMOSTAT_CHANGED, handle_thermo_change, hs);

  ESP_LOGI(TAG, "Accessory is paired with %d controllers", hap_get_paired_controller_count());

  hap_start();
}


