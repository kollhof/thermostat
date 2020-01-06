#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "mqtt_client.h"

#include "cJSON.h"

#include "./app_events.h"
#include "./app_stats.h"
#include "./app_thermostat.h"


static const char* TAG = "app-mqtt";


typedef struct {
  esp_mqtt_client_handle_t client;
  const char * topic_prefix;
} ctx_t;


static bool topic_matches(esp_mqtt_event_handle_t event, ctx_t * ctx, const char * topic) {
  const size_t max_char = 50;
  char * full_topic = malloc(max_char);
  const size_t len = snprintf(full_topic, 50, "%s%s", ctx->topic_prefix, topic);

  const bool result = (
    (len == event->topic_len)
    && (strncmp(full_topic, event->topic, len) == 0)
  );

  free(full_topic);
  return result;
}


static void subscribe(ctx_t * ctx, const char * topic) {
  char * full_topic = malloc(50);
  snprintf(full_topic, 50, "%s%s", ctx->topic_prefix, topic);

  ESP_LOGI(TAG, "subscribing to MQTT topic %s", full_topic);

  esp_mqtt_client_subscribe(ctx->client, full_topic, 0);
  free(full_topic);
}


static void publish(ctx_t * ctx, const char *topic, const char *data, int len, int qos, int retain) {
  char * full_topic = malloc(50);
  snprintf(full_topic, 50, "%s%s", ctx->topic_prefix, topic);

  esp_mqtt_client_publish(ctx->client, full_topic, data, len, qos, retain);
  free(full_topic);
}



static void handle_ip_event(void* arg, esp_event_base_t evt_base, int32_t evt_id, void* data) {
  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) arg;
  ESP_LOGI(TAG, "startinfg MQTT client");

  esp_mqtt_client_start(client);
}


static void handle_connected(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  ctx_t * ctx = (ctx_t *) arg;
  subscribe(ctx, "/target-temp/set");
  subscribe(ctx, "/stats/get");
  subscribe(ctx, "/system/ota");
  subscribe(ctx, "/system/restart");
}


static void handle_message(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;
  ctx_t * ctx = (ctx_t *) arg;

  cJSON *root = cJSON_Parse(event->data);

  char *msg = cJSON_PrintUnformatted(root);
  ESP_LOGI(TAG, "received message %.*s: %s", event->topic_len, event->topic, msg);
  free(msg);

  if (topic_matches(event, ctx, "/system/ota")) {
    app_post_event(APP_EVENT_OTA, NULL, 0);

  } else if (topic_matches(event, ctx, "/system/restart")) {
    app_post_event(APP_EVENT_RESTART, NULL, 0);

  } else if (topic_matches(event, ctx, "/stats/get")) {
    app_post_event(APP_EVENT_STATS_GET, NULL, 0);

  } else if (topic_matches(event, ctx, "/target-temp/set")) {
    float target_temp = cJSON_GetObjectItem(root, "value")->valuedouble;

    app_post_event(APP_EVENT_TARGET_TEMP_SET, &target_temp, sizeof(target_temp));

  } else {
    ESP_LOGI(TAG, "no handler for topic %.*s", event->topic_len, event->topic);
  }

  cJSON_Delete(root);
}


static void handle_stats(void* arg, esp_event_base_t evt_base, int32_t evt_id, void* data) {
  ctx_t * ctx = (ctx_t *) arg;
  app_stats_t * stats = (app_stats_t*) data;

  cJSON *json = cJSON_CreateObject();
  cJSON_AddNumberToObject(json, "current_temp", stats->current_temp);
  cJSON_AddNumberToObject(json, "target_temp", stats->target_temp);
  cJSON_AddNumberToObject(json, "heat_power", stats->heat_power);
  char * msg = cJSON_Print(json);
  cJSON_Delete(json);

  // ESP_LOGI(TAG, "posting stats: %s", msg);
  // TODO: QOS = 1 crash when not connected
  publish(ctx, "/stats/report", msg, 0, 0, 0);
  free(msg);
}


void app_start_mqtt(esp_mqtt_client_config_t * config, const char* topic_prefix) {
  ESP_LOGI(TAG, "setting up MQTT client for %s", config->uri);

  ctx_t * ctx = malloc(sizeof(ctx_t));

  ctx->client = esp_mqtt_client_init(config);
  ctx->topic_prefix = topic_prefix;

  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, handle_ip_event, ctx->client);
  esp_mqtt_client_register_event(ctx->client, MQTT_EVENT_CONNECTED, handle_connected, ctx);
  esp_mqtt_client_register_event(ctx->client, MQTT_EVENT_DATA, handle_message, ctx);

  app_register_evt_handler(APP_EVENT_STATS_REPORT, handle_stats, ctx);
}

