
/*
 * This supports 3 ways of connecting to the Wi-Fi Network
 * 1. Hard coded credentials
 * 2. Unified Provisioning
 *
 * Unified Provisioning has 2 options
 * 1. BLE Provisioning
 * 2. SoftAP Provisioning
 * */
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "esp_netif.h"

#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>
// #include "wifi_provisioning/manager.h"
// #include "wifi_provisioning/scheme_ble.h"

#include "qrcode.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "./app_wifi.h"


static const char *TAG = "app_wifi";
static const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;


#define PROV_QR_VERSION "v1"

#define PROV_TRANSPORT_SOFTAP   "softap"
#define PROV_TRANSPORT_BLE    "ble"
#define QRCODE_BASE_URL   "https://espressif.github.io/esp-jumpstart/qrcode.html"

#define CREDENTIALS_NAMESPACE   "rmaker_creds"
#define RANDOM_NVS_KEY      "random"



static void app_wifi_print_qr(const char *name, const char *pop, const char *transport) {
  if (!name || !pop || !transport) {
    ESP_LOGW(TAG, "Cannot generate QR code payload. Data missing.");
    return;
  }
  char payload[150];
  snprintf(
    payload, sizeof(payload),
    "{\"ver\":\"%s\",\"name\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}",
    PROV_QR_VERSION, name, pop, transport
  );
  ESP_LOGI(TAG, "-----QR Code for ESP Provisioning-----");
  ESP_LOGI(TAG, "Scan this QR code from the phone app for Provisioning.");
  qrcode_display(payload);
}



static void get_device_service_name(char *service_name, size_t max) {
  uint8_t eth_mac[6];
  const char *ssid_prefix = "PROV_";
  esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
  snprintf(service_name, max, "%s%02X%02X%02X", ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}



static esp_err_t get_device_pop(char *pop, size_t max) {
  if (!pop || !max) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t eth_mac[6];
  esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
  if (err == ESP_OK) {
    snprintf(pop, max, "%02x%02x%02x%02x", eth_mac[2], eth_mac[3], eth_mac[4], eth_mac[5]);
    return ESP_OK;
  } else {
    return err;
  }
}



/* Event handler for catching system events */
static void event_handler(void* arg, esp_event_base_t event_base, int event_id, void* event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();

  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
    /* Signal main application to continue execution */
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);

  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGI(TAG, "Disconnected. Connecting to the AP again...");
    esp_wifi_connect();
  } else if (event_base == WIFI_PROV_EVENT) {
    switch (event_id) {
      case WIFI_PROV_START:
        ESP_LOGI(TAG, "Provisioning started");
        break;
      case WIFI_PROV_CRED_RECV: {
        wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
        ESP_LOGI(
          TAG, "Received Wi-Fi credentials"
          "\n\tSSID   : %s\n\tPassword : %s",
          (const char *) wifi_sta_cfg->ssid,
          (const char *) wifi_sta_cfg->password
        );
        break;
      }
      case WIFI_PROV_CRED_FAIL: {
        wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
        ESP_LOGE(
          TAG, "Provisioning failed!\n\tReason : %s"
          "\n\tPlease reset to factory and retry provisioning",
          (*reason == WIFI_PROV_STA_AUTH_ERROR) ?
          "Wi-Fi station authentication failed" : "Wi-Fi access-point not found"
        );
        break;
      }
      case WIFI_PROV_CRED_SUCCESS:
        ESP_LOGI(TAG, "Provisioning successful");
        break;
      case WIFI_PROV_END:
        /* De-initialize manager once provisioning is finished */
        wifi_prov_mgr_deinit();
        break;
      default:
        break;
    }
  }
}


void app_init_networking(const char * hw_serial) {
  /* Initialize TCP/IP */
  esp_netif_init();

  /* Initialize the event loop */
  // ESP_ERROR_CHECK(esp_event_loop_create_default());
  wifi_event_group = xEventGroupCreate();

  /* Register our event handler for Wi-Fi, IP and Provisioning related events */
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

  /* Initialize Wi-Fi including netif with default config */
  esp_netif_t * netif = esp_netif_create_default_wifi_sta();

  char hostname[strlen(hw_serial) + 8];
  strcpy(hostname, "thermo-");
  strcat(hostname, hw_serial);
  esp_netif_set_hostname(netif, hostname);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}


static void wifi_init_sta() {
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());
}


esp_err_t app_start_networking(TickType_t ticks_to_wait) {
  /* Configuration for the provisioning manager */
  wifi_prov_mgr_config_t config = {
    /* What is the Provisioning Scheme that we want ?
     * wifi_prov_scheme_softap or wifi_prov_scheme_ble */
    .scheme = wifi_prov_scheme_ble,

    /* Any default scheme specific event handler that you would
     * like to choose. Since our example application requires
     * neither BT nor BLE, we can choose to release the associated
     * memory once provisioning is complete, or not needed
     * (in case when device is already provisioned). Choosing
     * appropriate scheme specific event handler allows the manager
     * to take care of this automatically. This can be set to
     * WIFI_PROV_EVENT_HANDLER_NONE when using wifi_prov_scheme_softap*/
    .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
  };

  /* Initialize provisioning manager with the
   * configuration parameters set above */
  ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

  /* Let's find out if the device is provisioned */
  bool provisioned = false;
  ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));
  // TODO: if error then   esp_wifi_restore();

  /* If device is not yet provisioned start provisioning service */
  if (!provisioned) {
    ESP_LOGI(TAG, "Starting provisioning");

    esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);
    /* What is the Device Service Name that we want
     * This translates to :
     *   - Wi-Fi SSID when scheme is wifi_prov_scheme_softap
     *   - device name when scheme is wifi_prov_scheme_ble
     */
    char service_name[12];
    get_device_service_name(service_name, sizeof(service_name));

    /* What is the security level that we want (0 or 1):
     *    - WIFI_PROV_SECURITY_0 is simply plain text communication.
     *    - WIFI_PROV_SECURITY_1 is secure communication which consists of secure handshake
     *      using X25519 key exchange and proof of possession (pop) and AES-CTR
     *      for encryption/decryption of messages.
     */
    wifi_prov_security_t security = WIFI_PROV_SECURITY_1;

    /* Do we want a proof-of-possession (ignored if Security 0 is selected):
     *    - this should be a string with length > 0
     *    - NULL if not used
     */
    char pop[9];
    esp_err_t err = get_device_pop(pop, sizeof(pop));
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error: %d. Failed to get PoP from NVS, Please perform Claiming.", err);
      return err;
    }

    /* What is the service key (Wi-Fi password)
     * NULL = Open network
     * This is ignored when scheme is wifi_prov_scheme_ble
     */
    const char *service_key = NULL;


    /* This step is only useful when scheme is wifi_prov_scheme_ble. This will
     * set a custom 128 bit UUID which will be included in the BLE advertisement
     * and will correspond to the primary GATT service that provides provisioning
     * endpoints as GATT characteristics. Each GATT characteristic will be
     * formed using the primary service UUID as base, with different auto assigned
     * 12th and 13th bytes (assume counting starts from 0th byte). The client side
     * applications must identify the endpoints by reading the User Characteristic
     * Description descriptor (0x2901) for each characteristic, which contains the
     * endpoint name of the characteristic */
    uint8_t custom_service_uuid[] = {
      /* This is a random uuid. This can be modified if you want to change the BLE uuid. */
      /* 12th and 13th bit will be replaced by internal bits. */
      0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
      0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
    };
    err = wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "wifi_prov_scheme_ble_set_service_uuid failed %d", err);
      return err;
    }

    /* Start provisioning service */
    ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, pop, service_name, service_key));

    app_wifi_print_qr(service_name, pop, PROV_TRANSPORT_BLE);

    ESP_LOGI(TAG, "Provisioning Started. Name : %s, POP : %s", service_name, pop);

    ESP_LOGI(TAG, "Waiting for  Wi-Fi connectd");

    /* Wait for Wi-Fi connection */
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, false, true, ticks_to_wait);
  } else {
    ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");
    /* We don't need the manager as device is already provisioned,
     * so let's release it's resources */

    // TODO: deinit breaks BLE later
    // wifi_prov_mgr_deinit();

    /* Start Wi-Fi station */
    wifi_init_sta();
  }

  return ESP_OK;
}


