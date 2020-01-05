#include <time.h>

#include "esp_log.h"
#include "esp_sntp.h"

#include "./app_events.h"


static const char* TAG = "app-timekeeper";


static void log_curr_time() {
  struct tm timeinfo;
  time_t now = 0;

  time(&now);
  localtime_r(&now, &timeinfo);

  char strftime_buf[64];
  strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
  ESP_LOGI(TAG, "current date/time is: %s UTC", strftime_buf);
}


static void time_sync_notification_cb(struct timeval *tv) {
  app_post_event(APP_EVENT_TIME_UPDATED, NULL, 0);
  log_curr_time();
}


void app_start_timekeeper(void) {
  ESP_LOGI(TAG, "initializing SNTP...");
  log_curr_time();
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, "pool.ntp.org");
  sntp_set_time_sync_notification_cb(time_sync_notification_cb);
  // sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH;)
  sntp_init();
  ESP_LOGI(TAG, "initialized SNTP");
}

