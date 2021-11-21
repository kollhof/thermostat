#include <stdio.h>
#include <math.h>
#include <string.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"

#include "unistd.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/adc.h"
#include "nvs_flash.h"

#include "slow_pwm.h"
#include "temp_sensor.h"

#include "./app_thermostat.h"
#include "./app_events.h"

#define sec 1000000

static const char* TAG = "app-thermostat";



static void persist_target_temp(float target_temp) {
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
  } else {
    const uint32_t store_value = *(uint32_t *)&target_temp;

    err = nvs_set_u32(nvs_handle, "target_temp", store_value);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error (%s) storing target temp", esp_err_to_name(err));
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error (%s) committing storage", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
  }
}



static void post_changed_event(app_thermostat_state_t * state) {
  app_post_event(APP_EVENT_THERMOSTAT_CHANGED, state, sizeof(app_thermostat_state_t));
}



static void handle_temp_change(app_thermostat_state_t * state){
  ESP_LOGI(TAG, "calculating new heat: %f -> %f", state->current_temp, state->target_temp);

  float temp_diff = state->current_temp - state->target_temp;

  uint8_t heat = state->heat;

  if (state->temp_state != APP_THERMOSTAT_TEMP_OK) {
    ESP_LOGE(TAG, "temp error ... min heat");
    heat = state->heat_min;

  } else if (temp_diff <= -2) {
    heat = state->heat_max;

  } else if (temp_diff < 0) {
    heat = state->heat_normal;

  } else {
    heat = 0;
  }

  state->heat = heat;
  post_changed_event(state);
}



static void handle_traget_temp_changed(void *arg, esp_event_base_t evt_base, int32_t id, void *data) {

  app_thermostat_state_t * state = (app_thermostat_state_t*) arg;
  float target_temp = *((float*) data);

  state->target_temp = target_temp;
  // TODO: should not live here
  persist_target_temp(target_temp);
  handle_temp_change(state);
}



static void handle_current_temp_changed(void *arg, esp_event_base_t evt_base, int32_t id, void *data) {
  app_thermostat_state_t * state = (app_thermostat_state_t*) arg;
  float temp = *((float*) data);
  state->current_temp = temp;
  handle_temp_change(state);
}


static void handle_temp_read_state_changed(void *arg, esp_event_base_t evt_base, int32_t id, void *data) {
  app_thermostat_state_t * state = (app_thermostat_state_t*) arg;
  bool err = *((bool*) data);

  state->temp_state = err
    ? APP_THERMOSTAT_TEMP_ERROR
    : APP_THERMOSTAT_TEMP_OK;

  handle_temp_change(state);
}



static void handle_heat_changed(void *arg, esp_event_base_t evt_base, int32_t id, void *data) {
  slow_pwm_t * pwm = (slow_pwm_t*) arg;
  app_thermostat_state_t * state = (app_thermostat_state_t*) data;
  set_pwm_duty(pwm, state->heat);
}



void app_start_thermostat(gpio_num_t gpio_pwm, uint8_t heat_min, uint8_t heat_normal, uint8_t heat_max, uint8_t cycle_len, float target_temp) {
  ESP_LOGI(TAG, "starting thermostat min, normal, max: %d, %d, %d -> %f", heat_min, heat_normal, heat_max, target_temp);

  const uint64_t pwm_freq = cycle_len * sec;
  const uint32_t pwm_resolution = 100;
  const uint32_t pwm_duty = 0;

  slow_pwm_t * pwm = start_pwm(pwm_freq, pwm_resolution, pwm_duty, gpio_pwm);

  app_thermostat_state_t * state = malloc(sizeof(app_thermostat_state_t));
  *state = (app_thermostat_state_t) {
    .temp_state = APP_THERMOSTAT_TEMP_OK,
    .current_temp = 20,
    .target_temp = target_temp,
    .heat = 0,
    .heat_min = heat_min,
    .heat_max = heat_max,
    .heat_normal = heat_normal,
  };

  app_register_evt_handler(APP_EVENT_TARGET_TEMP_CHANGED, handle_traget_temp_changed, state);
  app_register_evt_handler(APP_EVENT_CURRENT_TEMP_CHANGED, handle_current_temp_changed, state);
  app_register_evt_handler(APP_EVENT_TEMP_READ_STATE, handle_temp_read_state_changed, state);
  app_register_evt_handler(APP_EVENT_THERMOSTAT_CHANGED, handle_heat_changed, pwm);
}




