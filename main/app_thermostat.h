#pragma once

#include "driver/gpio.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
  float current_temp;
  float target_temp;
  uint8_t heat_power;
} app_thermostat_state_t;


void app_start_thermostat(gpio_num_t gpio_pwm, gpio_num_t gpio_temp);


#ifdef __cplusplus
}
#endif