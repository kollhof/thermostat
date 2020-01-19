
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"


static const char* TAG = "app-wifi";


static int s_retry_num = 0;
// TODO: retry forvever
static const int MAXIMUM_NET_RETRY = -1;


static void handle_network_disconnect() {
  if (s_retry_num < MAXIMUM_NET_RETRY || MAXIMUM_NET_RETRY == -1) {
    esp_wifi_connect();
    s_retry_num++;
    ESP_LOGW(TAG, "retry to connect to the AP");
  }
  ESP_LOGE(TAG,"connecting to the AP failed");

  // TODO: restart or keep trying?
}


static void handle_ip_event(
  void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data
) {

  if (event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "network connected:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
  }
}


static void handle_wifi_event(
  void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data
) {
  if (event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();

  } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
    handle_network_disconnect();

  } else {
    ESP_LOGW(TAG, "network event: %d", event_id );
  }
}


void app_start_network(wifi_config_t * config) {
  ESP_LOGI(TAG, "init network stack");
  esp_netif_init();

  ESP_LOGI(TAG, "create wifi sta");
  esp_netif_create_default_wifi_sta();

  ESP_LOGI(TAG, "init wifi");
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);

  esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &handle_wifi_event, NULL);
  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &handle_ip_event, NULL);

  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(ESP_IF_WIFI_STA, config);
  esp_wifi_start();

  ESP_LOGI(TAG, "wating for wifi to connect ...");
}

