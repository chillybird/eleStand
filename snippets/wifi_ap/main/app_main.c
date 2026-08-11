#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#define AP_SSID     "eleStand"
#define AP_PASSWORD ""
#define AP_CHANNEL  6
#define AP_MAX_CONN 4

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data) {
    if (id == WIFI_EVENT_AP_STACONNECTED)
        ESP_LOGI("ap", "Station connected");
    else if (id == WIFI_EVENT_AP_STADISCONNECTED)
        ESP_LOGI("ap", "Station disconnected");
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_flash_init();
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));

    wifi_config_t ap = {
        .ap = { .ssid = AP_SSID, .ssid_len = strlen(AP_SSID),
                .channel = AP_CHANNEL, .password = AP_PASSWORD,
                .max_connection = AP_MAX_CONN, .authmode = WIFI_AUTH_OPEN }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_max_tx_power(8);

    ESP_LOGI("ap", "Started: SSID=%s (no password)", AP_SSID);

    while (1) vTaskDelay(pdMS_TO_TICKS(10000));
}
