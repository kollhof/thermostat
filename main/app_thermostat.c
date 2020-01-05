#include <stdio.h>
#include <math.h>
#include <string.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"

#include "unistd.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/adc.h"

#include "slow_pwm.h"
#include "temp_sensor.h"

#include "./app_thermostat.h"
#include "./app_events.h"


static const char* TAG = "app-thermostat";


static void simple_thermostat_loop(temp_sensor_t * temp_sensor, slow_pwm_t * pwm, app_thermostat_state_t * state) {
  ESP_LOGI(TAG, "starting simple thermostat algorithm");

  while (true) {
    // TODO: writes should be atomic as we access state from multiple tasks
    state->current_temp = get_temperature(temp_sensor);

    if (state->current_temp < state->target_temp) {
      state->heat_power = 50;
    } else {
      state->heat_power = 0;
    }
    set_pwm_duty(pwm, state->heat_power);

    post_changed_event(state);

    sleep(1);
  }
}


static void handle_set_state(void *arg, esp_event_base_t evt_base, int32_t id, void *data) {
  app_thermostat_state_t * current_state = (app_thermostat_state_t*) arg;
  app_thermostat_state_t * new_state = (app_thermostat_state_t*) data;

  if (new_state->target_temp > 0) {
    current_state->target_temp = new_state->target_temp;
    post_changed_event(current_state);
  }
}


static void handle_change(void *arg, esp_event_base_t evt_base, int32_t id, void *data) {
  app_thermostat_state_t * state = (app_thermostat_state_t*) data;
  ESP_LOGI(TAG, "c: %0.3f t: %0.3f h: %d",
    state->current_temp, state->target_temp, state->heat_power
  );
}


void app_start_thermostat(gpio_num_t gpio_pwm, gpio_num_t gpio_temp) {
  app_thermostat_state_t state = {
    .current_temp = 20,
    .target_temp = 20,
    .heat_power = 0
  };
  const uint64_t temp_read_interval = 1000*1000;
  const uint64_t pwm_freq = 20* 1000;
  const uint32_t pwm_resolution = 100;
  const uint32_t pwm_duty = 0;

  app_register_evt_handler(APP_EVENT_THERMOSTAT_SET, handle_set_state, &state);
  app_register_evt_handler(APP_EVENT_THERMOSTAT_CHANGED, handle_change, NULL);

  ESP_LOGI(TAG, "starting temp sensors and heating PWM driver");
  temp_sensor_t * temp_sensor = start_temp_sensors(gpio_temp, temp_read_interval);
  slow_pwm_t * pwm = start_pwm(pwm_freq, pwm_resolution, pwm_duty, gpio_pwm);

  simple_thermostat_loop(temp_sensor, pwm, &state);

  // TODO: will never reach this point
  stop_temp_sensors(temp_sensor);
  stop_pwm(pwm);
}

