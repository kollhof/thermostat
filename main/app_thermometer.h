#pragma once

#include "esp_bt_defs.h"
#include "driver/gpio.h"


#ifdef __cplusplus
extern "C" {
#endif


void app_start_thermometer(gpio_num_t gpio_temp, esp_bd_addr_t addr);


#ifdef __cplusplus
}
#endif