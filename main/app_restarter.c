#include "esp_log.h"

#include "hap.h"
#include "iot_button.h"

#include "./app_events.h"


static const char* TAG = "app-restarter";


#define RESET_SHORT_BUTTON_TIMEOUT   3

#define RESET_LONG_BUTTON_TIMEOUT   10

#define RESET_GPIO GPIO_NUM_0



static void handle_short_reset_press(void *arg) {
  app_post_event(APP_EVENT_RESET_NETWORK, NULL, 0);
}



static void handle_long_reset_press(void *arg) {
  app_post_event(APP_EVENT_RESET_FACTORY, NULL, 0);
}



static void handle_app_evt(void* arg, esp_event_base_t base, int32_t id, void* data) {
  if (id == APP_EVENT_RESET_FACTORY) {
    hap_reset_to_factory();
  } else if (id == APP_EVENT_RESET_HOMEKIT) {
    hap_reset_homekit_data();
  } else if (id == APP_EVENT_RESET_NETWORK) {
    hap_reset_network();
  } else if (id == APP_EVENT_RESET_PAIRING) {
    hap_reset_pairings();
  } else if (id == APP_EVENT_RESTART) {
    hap_reboot_accessory();
  }
}




void app_start_restart_handler() {
  ESP_LOGI(TAG, "starting restart/reset handler");

  app_register_evt_handler(APP_EVENT_RESTART, handle_app_evt, NULL);
  app_register_evt_handler(APP_EVENT_RESET_FACTORY, handle_app_evt, NULL);
  app_register_evt_handler(APP_EVENT_RESET_HOMEKIT, handle_app_evt, NULL);
  app_register_evt_handler(APP_EVENT_RESET_NETWORK, handle_app_evt, NULL);
  app_register_evt_handler(APP_EVENT_RESET_PAIRING, handle_app_evt, NULL);

  button_handle_t handle = iot_button_create(RESET_GPIO, BUTTON_ACTIVE_LOW);
  iot_button_add_on_release_cb(handle, RESET_SHORT_BUTTON_TIMEOUT, handle_short_reset_press, NULL);
  iot_button_add_on_press_cb(handle, RESET_LONG_BUTTON_TIMEOUT, handle_long_reset_press, NULL);
}

