#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#include "./app_events.h"
#include "./app_thermostat.h"
#include "./app_stats.h"


static const char* TAG = "app-stats";


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

  if (stats->target_temp != current_state->target_temp) {
    stats->target_temp = current_state->target_temp;
    report_stats(stats);
  }
}


static void init_led(gpio_num_t gpio_led) {
  gpio_pad_select_gpio(gpio_led);
  gpio_set_direction(gpio_led, GPIO_MODE_OUTPUT);
  gpio_set_level(gpio_led, 1);
}


void app_start_stats_handler(gpio_num_t gpio_led) {
  ESP_LOGI(TAG, "starting stats handler");
  const int stats_interval_sec = 10;

  init_led(gpio_led);

  app_stats_t * stats = malloc(sizeof(app_stats_t));

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
