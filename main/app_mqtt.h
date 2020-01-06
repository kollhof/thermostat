#pragma once

#ifdef __cplusplus
extern "C" {
#endif


void app_start_mqtt(esp_mqtt_client_config_t * config, const char* topic_prefix);


#ifdef __cplusplus
}
#endif