#pragma once

#include "esp_https_ota.h"

#ifdef __cplusplus
extern "C" {
#endif


void app_start_ota_handler(esp_http_client_config_t * config);


#ifdef __cplusplus
}
#endif