#pragma once

#include "driver/gpio.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
  bool error;
  float current_temp;
  float target_temp;
  float current_humid;
  uint8_t heat;
} app_stats_t;


void app_start_stats_handler(gpio_num_t gpio_led);


#ifdef __cplusplus
}
#endif