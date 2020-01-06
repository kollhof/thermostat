#pragma once

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
  float current_temp;
  float target_temp;
  uint8_t heat_power;
} app_stats_t;


void app_start_stats_handler();


#ifdef __cplusplus
}
#endif