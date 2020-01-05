#pragma once

#include "freertos/task.h"
#include "driver/gpio.h"
#include "stdatomic.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
  gpio_num_t gpio_num;
  uint64_t read_interval;

  TaskHandle_t task_handle;
  atomic_uint_fast32_t curr_temp;
} temp_sensor_t;


temp_sensor_t * start_temp_sensors(gpio_num_t gpio_num, uint64_t read_interval);

void stop_temp_sensors(temp_sensor_t * temp_sensor);

double get_temperature(temp_sensor_t * temp_sensor);


#ifdef __cplusplus
}
#endif