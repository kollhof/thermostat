#pragma once

#include "driver/gpio.h"
#include "esp_timer.h"
#include "stdatomic.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    uint64_t freq;
    uint32_t cycle_ticks;
    atomic_uint_fast32_t duty;
    uint32_t tick_cntr;
    gpio_num_t gpio_num;
    esp_timer_handle_t timer;
    esp_timer_create_args_t timer_args;
} slow_pwm_t;


slow_pwm_t * start_pwm(uint64_t freq, uint32_t resolution, uint32_t duty, gpio_num_t gpio_num);

void stop_pwm(slow_pwm_t * foo);

void set_pwm_duty(slow_pwm_t * foo, uint32_t duty);


#ifdef __cplusplus
}
#endif