#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "mqtt_client.h"

#include "cJSON.h"

#include "./app_events.h"
#include "./app_thermostat.h"


static const char* TAG = "app-mqtt";


bool topic_matches(esp_mqtt_event_handle_t event, const char * topic) {
  if (strlen(topic) != event->topic_len) {
    return false;
  }
  return strncmp(event->topic, topic, event->topic_len) == 0;
}


static void handle_connected(esp_mqtt_event_handle_t event) {
  ESP_LOGI(TAG, "connected to MQTT, subscribing to topics...");
  esp_mqtt_client_subscribe(event->client, "/state/set", 0);
  esp_mqtt_client_subscribe(event->client, "/state/get", 0);
  esp_mqtt_client_subscribe(event->client, "/system/ota", 0);
  esp_mqtt_client_subscribe(event->client, "/system/restart", 0);
}


static void handle_msg(esp_mqtt_event_handle_t event) {
  cJSON *root = cJSON_Parse(event->data);

  char *msg = cJSON_PrintUnformatted(root);
  ESP_LOGI(TAG, "received message %s", msg);
  free(msg);

  if (topic_matches(event, "/system/ota")) {
    app_post_event(APP_EVENT_OTA, NULL, 0);

  } else if (topic_matches(event, "/system/restart")) {
    app_post_event(APP_EVENT_RESTART, NULL, 0);

  } else if (topic_matches(event, "/state/get")) {
    app_post_event(APP_EVENT_THERMOSTAT_GET, NULL, 0);

  } else if (topic_matches(event, "/state/set")) {
     app_thermostat_state_t data = {
      .current_temp = 0,
      .target_temp = cJSON_GetObjectItem(root, "target_temp")->valuedouble,
      .heat_power = 0
    };

    app_post_event(APP_EVENT_THERMOSTAT_SET, &data, sizeof(data));
  }

  cJSON_Delete(root);
}


static void handle_mqtt_evt(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;

  switch (event_id) {
    case MQTT_EVENT_CONNECTED:
      handle_connected(event);
      break;
    case MQTT_EVENT_DATA:
      handle_msg(event);
      break;
  }
}


static void handle_ip_event(void* arg, esp_event_base_t evt_base, int32_t evt_id, void* data) {
  if (evt_id == IP_EVENT_STA_GOT_IP) {
    esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) arg;
    ESP_LOGI(TAG, "startinfg MQTT client");

    esp_mqtt_client_start(client);
  }
}


static void handle_thermostat_changed(void* arg, esp_event_base_t evt_base, int32_t evt_id, void* data) {
  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) arg;
  app_thermostat_state_t * state = (app_thermostat_state_t*) data;

  cJSON *json = cJSON_CreateObject();
  cJSON_AddNumberToObject(json, "current_temp", state->current_temp);
  cJSON_AddNumberToObject(json, "target_temp", state->target_temp);
  cJSON_AddNumberToObject(json, "heat_power", state->heat_power);
  char *msg = cJSON_Print(json);
  cJSON_Delete(json);

  // ESP_LOGI(TAG, "state: %s", msg);
  // TODO: QOS = 1 crash when not connected
  // esp_mqtt_client_publish(client, "/foo-status/state", msg, 0, 0, 0);

  free(msg);
}


void app_start_mqtt(esp_mqtt_client_config_t * config) {
  ESP_LOGI(TAG, "setting up MQTT client for %s", config->uri);

  esp_mqtt_client_handle_t client = esp_mqtt_client_init(config);

  esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, handle_mqtt_evt, client);
  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, handle_ip_event, client);

  app_register_evt_handler(APP_EVENT_THERMOSTAT_CHANGED, handle_thermostat_changed, client);
}

