#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif


esp_err_t app_start_networking(TickType_t ticks_to_wait);
void app_init_networking();

#ifdef __cplusplus
}
#endif