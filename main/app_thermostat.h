#pragma once

#include "driver/gpio.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
  float current_temp;
  float target_temp;
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