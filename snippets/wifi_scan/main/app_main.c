#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "scan";

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_flash_init();
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_wifi_set_max_tx_power(34);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    uint16_t number = 15;
    wifi_ap_record_t *records = calloc(number, sizeof(wifi_ap_record_t));
    if (!records) return;

    uint16_t ap_count = 0;
    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, records));
    ESP_LOGI(TAG, "Found %d AP(s), showing top %d", ap_count, number);

    for (int i = 0; i < number; i++) {
        ESP_LOGI(TAG, "  [%d] %-20s RSSI=%d CH=%d",
                 i + 1, records[i].ssid, records[i].rssi, records[i].primary);
    }
    free(records);

    while (1) vTaskDelay(pdMS_TO_TICKS(10000));
}
