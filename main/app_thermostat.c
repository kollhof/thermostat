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


static void post_changed_event(app_thermostat_state_t * state) {
  app_post_event(APP_EVENT_THERMOSTAT_CHANGED, state, sizeof(app_thermostat_state_t));
}


static void simple_thermostat_loop(
    temp_sensor_t * temp_sensor,
    slow_pwm_t * pwm,
    app_thermostat_state_t * state,
    uint8_t duty_min,
    uint8_t duty_max
) {
  ESP_LOGI(TAG, "starting simple thermostat algorithm");

  while (true) {
    // TODO: writes should be atomic as we access state from multiple tasks
    state->current_temp = get_temperature(temp_sensor);
    float temp_diff = state->current_temp - state->target_temp;

    if (state->current_temp < -100) {
      state->heat = duty_min;
    } else if (temp_diff >= 2) {
      state->heat = 0;
    } else if (temp_diff >= 0) {
      state->heat = duty_min;
    } else if (temp_diff < -2) {
      state->heat = 100;
    } else {
      state->heat = duty_max;
    }
    set_pwm_duty(pwm, state->heat);

    post_changed_event(state);

    sleep(1);
  }
}


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


static void handle_set_target_temp(void *arg, esp_event_base_t evt_base, int32_t id, void *data) {
  app_thermostat_state_t * current_state = (app_thermostat_state_t*) arg;
  float target_temp = *((float*) data);

  current_state->target_temp = target_temp;

  persist_target_temp(target_temp);

  post_changed_event(current_state);
}


void app_start_thermostat(gpio_num_t gpio_pwm, gpio_num_t gpio_temp, uint8_t heat_min, uint8_t heat_max, uint8_t cycle_len) {
  app_thermostat_state_t state = {
    .current_temp = 20,
    .target_temp = load_target_temp(17),
    .heat = 0
  };

  const uint64_t temp_read_interval = 1000;
  const uint64_t pwm_freq = cycle_len * sec;
  const uint32_t pwm_resolution = 100;
  const uint32_t pwm_duty = 0;

  app_register_evt_handler(APP_EVENT_TARGET_TEMP_SET, handle_set_target_temp, &state);

  ESP_LOGI(TAG, "starting temp sensors and heating PWM driver");
  temp_sensor_t * temp_sensor = start_temp_sensors(gpio_temp, temp_read_interval);
  slow_pwm_t * pwm = start_pwm(pwm_freq, pwm_resolution, pwm_duty, gpio_pwm);

  simple_thermostat_loop(temp_sensor, pwm, &state, heat_min, heat_max);

  // TODO: will never reach this point
  stop_temp_sensors(temp_sensor);
  stop_pwm(pwm);
}




