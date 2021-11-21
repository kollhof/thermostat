#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#include "./app_events.h"
#include "./app_thermostat.h"
#include "./app_stats.h"


static const char* TAG = "app-stats";



static void set_led_level(uint8_t lvl){
  // resolution 13 bit = 8192
  uint32_t duty = 20 + lvl * 5;
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
}


static void report_stats(void * arg) {
  app_stats_t * stats = (app_stats_t*) arg;
  app_post_event(APP_EVENT_STATS_REPORT, stats, sizeof(app_stats_t));
}


static void handle_get_stats(void* arg, esp_event_base_t base, int32_t id, void* data) {
  report_stats(arg);
}


static void handle_change(void* arg, esp_event_base_t evt_base, int32_t evt_id, void* data) {
  app_stats_t * stats = (app_stats_t*) arg;
  app_thermostat_state_t * current_state = (app_thermostat_state_t*) data;

  stats->current_temp = current_state->current_temp;
  stats->heat = current_state->heat;
  stats->target_temp = current_state->target_temp;
  stats->error = current_state->temp_state == APP_THERMOSTAT_TEMP_ERROR ? true : false;

  ESP_LOGI(TAG,
    "curr: %f C, target: %f C heat: %d ERR: %d" ,
    stats->current_temp, stats->target_temp, stats->heat, stats->error
  );

  if (stats->heat == 0) {
    set_led_level(0);
  } else if (stats->heat < 50) {
    set_led_level(10);
  } else if (stats->heat < 100) {
    set_led_level(55);
  } else {
    set_led_level(100);
  }
}



static void init_led(gpio_num_t gpio_led) {
  ledc_timer_config_t ledc_timer = {
    .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
    .freq_hz = 5000,                      // frequency of PWM signal
    .speed_mode = LEDC_LOW_SPEED_MODE,    // timer mode
    .timer_num = LEDC_TIMER_1,            // timer index
    .clk_cfg = LEDC_AUTO_CLK,             // Auto select the source clock
  };
  ledc_channel_config_t ledc_channel =  {
    .channel    = LEDC_CHANNEL_2,
    .duty       = 0,
    .gpio_num   = gpio_led,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .hpoint     = 0,
    .timer_sel  = LEDC_TIMER_1
  };

  // Set configuration of timer0 for high speed channels
  ledc_timer_config(&ledc_timer);
  ledc_channel_config(&ledc_channel);
  set_led_level(50);
}


void app_start_stats_handler(gpio_num_t gpio_led) {
  ESP_LOGI(TAG, "starting stats handler");

  const int stats_interval_sec = 60;

  init_led(gpio_led);

  app_stats_t * stats = malloc(sizeof(app_stats_t));
  // TODO: pass values in as args
  *stats = (app_stats_t) {
    .current_temp = 20.0,
    .target_temp = 20.0,
    .heat = 0,
    .error = false
  };

  app_register_evt_handler(APP_EVENT_STATS_GET, handle_get_stats, stats);
  app_register_evt_handler(APP_EVENT_THERMOSTAT_CHANGED, handle_change, stats);

  esp_timer_create_args_t timer_args = {
    .name = "app-stats",
    .callback = &report_stats,
    .arg = stats
  };
  esp_timer_handle_t timer;
  esp_timer_create(&timer_args, &timer);
  esp_timer_start_periodic(timer, stats_interval_sec * 1000*1000);

  // TODO: need a stop func/handler and free(state);
}
