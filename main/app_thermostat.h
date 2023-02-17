#pragma once

#include "driver/gpio.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
  APP_THERMOSTAT_TEMP_OK,
  APP_THERMOSTAT_TEMP_ERROR
} app_thermostat_temp_state_t;


typedef struct {
  app_thermostat_temp_state_t temp_state;
  float current_temp;
  float target_temp;
  float current_humid;
  uint8_t heat;
  uint8_t heat_min;
  uint8_t heat_normal;
  uint8_t heat_max;
} app_thermostat_state_t;



void app_start_thermostat(
  gpio_num_t gpio_pwm,
  uint8_t heat_min,
  uint8_t heat_normal,
  uint8_t heat_max,
  uint8_t cycle_len,
  float target_temp
);


#ifdef __cplusplus
}
#endif