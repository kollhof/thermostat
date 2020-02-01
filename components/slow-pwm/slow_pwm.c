#include <stdio.h>
#include "stdatomic.h"

#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "./slow_pwm.h"


static const char* TAG = "slow_pwm";


static void periodic_timer_callback(void * arg) {
  slow_pwm_t * pmw = (slow_pwm_t *)arg;


  uint32_t duty = atomic_load(&(pmw->duty));

  if (pmw->tick_cntr < duty) {
    // ESP_LOGI(TAG, "tick %u, duty %u: on", pmw->tick_cntr, duty);
    gpio_set_level(pmw->gpio_num, 1);
  } else if (duty < pmw->cycle_ticks) {
    // ESP_LOGI(TAG, "tick %u, duty %u: off", pmw->tick_cntr, duty);
    gpio_set_level(pmw->gpio_num, 0);
  }

  pmw->tick_cntr += 1;
  if (pmw->tick_cntr >= pmw->cycle_ticks) {
    pmw->tick_cntr = 0;
  }
}


slow_pwm_t * start_pwm(
  uint64_t freq, uint32_t resolution, uint32_t duty, gpio_num_t gpio_num
) {
  ESP_LOGI(TAG, "starting slow-pwm ...");

  gpio_set_direction(gpio_num, GPIO_MODE_OUTPUT);
  gpio_set_level(gpio_num, 0);

  slow_pwm_t * pwm = malloc(sizeof(slow_pwm_t));
  *pwm = (slow_pwm_t) {
    .freq = freq,
    .cycle_ticks = resolution,
    .duty =  duty,
    .gpio_num = gpio_num,
    .tick_cntr = 0,
    .timer_args = {
      .callback = &periodic_timer_callback,
      .arg = pwm
    }
  };
  esp_timer_create(&(pwm->timer_args), &(pwm->timer));
  esp_timer_start_periodic(pwm->timer, freq / resolution);

  ESP_LOGI(TAG, "started slow-pwm");
  return pwm;
};


void stop_pwm(slow_pwm_t * pwm) {
  ESP_LOGI(TAG, "stopping slow-pwm");
  esp_timer_stop(pwm->timer);
  esp_timer_delete(pwm->timer);
  free(&(pwm->timer_args));
  free(pwm);
  ESP_LOGI(TAG, "stopped slow-pwm");
};


void set_pwm_duty(slow_pwm_t * pwm, uint32_t duty) {
  ESP_LOGI(TAG, "update duty cycle: %u", duty);
  atomic_store(&(pwm->duty), duty);
};




