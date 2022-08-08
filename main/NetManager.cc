#include "NetManager.h"

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_wifi.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "nvs_flash.h"

namespace {
const char* TAG = "NetManager";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static void
startEventHandler(void* arg,
                 esp_event_base_t eventBase,
                 int32_t eventId,
                 void* eventData)
{
    if (eventBase == WIFI_EVENT) {
        switch (eventId) {
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "Attempt reconnect");
            /* Fallthrough */
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;
        default:
            break;
        }
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        EventGroupHandle_t* egroup = static_cast<EventGroupHandle_t*>(arg);
        xEventGroupSetBits(*egroup, WIFI_CONNECTED_BIT);
    }
}

} // namespace

NetManager::NetManager()
{
    esp_err_t rv = nvs_flash_init();
    if (rv == ESP_ERR_NVS_NO_FREE_PAGES || rv == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        rv = nvs_flash_init();
    }
    ESP_ERROR_CHECK(rv);

    esp_read_mac(stationMac_, ESP_MAC_WIFI_STA);
}

void
NetManager::start()
{
    nvs_handle_t nvsHandle;
    esp_err_t rv = nvs_open(TAG, NVS_READONLY, &nvsHandle);
    ESP_ERROR_CHECK(rv);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    wifi_config_t cfgSta = {};
    cfgSta.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;
    cfgSta.sta.pmf_cfg.capable = true;
    cfgSta.sta.pmf_cfg.required = false;

    size_t len = sizeof cfgSta.sta.ssid;
    rv = nvs_get_str(nvsHandle, "ssid", (char*)cfgSta.sta.ssid, &len);
    ESP_ERROR_CHECK(rv);

    len = sizeof cfgSta.sta.password;
    rv = nvs_get_str(nvsHandle, "password", (char*)cfgSta.sta.password, &len);
    ESP_ERROR_CHECK(rv);

    nvs_close(nvsHandle);

    EventGroupHandle_t eventGroup = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t wifiEvent;
    rv = esp_event_handler_instance_register(WIFI_EVENT,
                                             ESP_EVENT_ANY_ID,
                                             &startEventHandler,
                                             &eventGroup,
                                             &wifiEvent);
    ESP_ERROR_CHECK(rv);

    esp_event_handler_instance_t ipEvent;
    rv = esp_event_handler_instance_register(IP_EVENT,
                                             IP_EVENT_STA_GOT_IP,
                                             &startEventHandler,
                                             &eventGroup,
                                             &ipEvent);
    ESP_ERROR_CHECK(rv);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfgSta));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "esp_wifi_start()");

    EventBits_t bits = xEventGroupWaitBits(eventGroup,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, ipEvent));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wifiEvent));
    vEventGroupDelete(eventGroup);
}
