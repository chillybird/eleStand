#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif_ip_addr.h"

// 修改为你的 WiFi 名称和密码
#define STA_SSID     "your_wifi_ssid"
#define STA_PASSWORD "your_wifi_password"
#define MAX_RETRY    5

static EventGroupHandle_t s_evg;
#define WIFI_OK  BIT0
#define WIFI_ERR BIT1

static void handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) esp_wifi_connect();
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        static int retry = 0;
        if (retry++ < MAX_RETRY) esp_wifi_connect();
        else xEventGroupSetBits(s_evg, WIFI_ERR);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        ESP_LOGI("sta", "IP: " IPSTR, IP2STR(&ev->ip_info.ip));
        xEventGroupSetBits(s_evg, WIFI_OK);
    }
}

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

    s_evg = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, handler, NULL, NULL));

    wifi_config_t sta = { .sta = {
        .ssid = STA_SSID, .password = STA_PASSWORD,
        .threshold.authmode = WIFI_AUTH_WPA2_PSK } };

    esp_wifi_set_max_tx_power(34);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_evg, WIFI_OK | WIFI_ERR,
        pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));
    if (bits & WIFI_OK) ESP_LOGI("sta", "Connected to %s", STA_SSID);
    else                ESP_LOGW("sta", "Failed to connect");

    while (1) vTaskDelay(pdMS_TO_TICKS(10000));
}
