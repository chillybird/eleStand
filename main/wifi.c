#include "wifi.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif_ip_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

#define AP_SSID     "eleStand"
#define AP_PASSWORD ""
#define AP_CHANNEL  6
#define AP_MAX_CONN 4
#define STA_MAX_RETRY 5
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi";
static char s_ip[16] = {0};
static bool s_is_sta = false;
static EventGroupHandle_t s_event_group;

static void event_handler(void *arg, esp_event_base_t base,
                          int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "AP client connected");
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGI(TAG, "AP client left");
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "STA lost connection");
        xEventGroupSetBits(s_event_group, WIFI_FAIL_BIT);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&ev->ip_info.ip));
        ESP_LOGI(TAG, "STA IP: %s", s_ip);
        xEventGroupSetBits(s_event_group, WIFI_CONNECTED_BIT);
    }
}

static bool try_sta_connect(const char *ssid, const char *pass) {
    ESP_LOGI(TAG, "Connecting to %s...", ssid);

    wifi_config_t cfg = { .sta = { .threshold.authmode = WIFI_AUTH_WPA2_PSK } };
    strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid) - 1);
    if (pass[0] != '\0') {
        strncpy((char *)cfg.sta.password, pass, sizeof(cfg.sta.password) - 1);
    } else {
        cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }

    xEventGroupClearBits(s_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_start();
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(s_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE,
        pdMS_TO_TICKS(15000));

    if (bits & WIFI_CONNECTED_BIT) {
        s_is_sta = true;
        return true;
    }
    esp_wifi_stop();
    return false;
}

static void start_ap(void) {
    wifi_config_t cfg = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .channel = AP_CHANNEL,
            .password = AP_PASSWORD,
            .max_connection = AP_MAX_CONN,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &cfg);
    esp_wifi_start();
    esp_wifi_set_max_tx_power(8);

    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(ap_netif, &ip_info);
    snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&ip_info.ip));
    s_is_sta = false;

    ESP_LOGI(TAG, "AP: SSID=%s IP=%s", AP_SSID, s_ip);
}

static void save_wifi(const char *ssid, const char *pass) {
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "ssid", ssid);
        nvs_set_str(nvs, "pass", pass);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

static bool load_wifi(char *ssid, size_t ssid_len, char *pass, size_t pass_len) {
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READONLY, &nvs) != ESP_OK) return false;
    bool ok = (nvs_get_str(nvs, "ssid", ssid, &ssid_len) == ESP_OK &&
               nvs_get_str(nvs, "pass", pass, &pass_len) == ESP_OK);
    nvs_close(nvs);
    return ok && ssid[0] != '\0';
}

void wifi_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    s_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, event_handler, NULL, NULL));

    char ssid[33] = {0}, pass[65] = {0};
    if (load_wifi(ssid, sizeof(ssid), pass, sizeof(pass))) {
        if (try_sta_connect(ssid, pass)) return;
    }
    start_ap();
}

void wifi_connect(const char *ssid, const char *pass) {
    save_wifi(ssid, pass);
    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(500));
    if (!try_sta_connect(ssid, pass)) {
        ESP_LOGW(TAG, "STA failed, restarting AP...");
        start_ap();
    }
}

const char *wifi_get_ip(void) { return s_ip; }
bool wifi_is_sta(void) { return s_is_sta; }
