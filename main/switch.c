#include "switch.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define SW_DOWN_GPIO    GPIO_NUM_3
#define SW_UP_GPIO      GPIO_NUM_5

void sw_init(void) {
    gpio_config_t sw_cfg = {
        .pin_bit_mask = (1ULL << SW_DOWN_GPIO) | (1ULL << SW_UP_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&sw_cfg);

    ESP_LOGI("sw", "DOWN GPIO%d level=%d", SW_DOWN_GPIO, sw_is_down());
    ESP_LOGI("sw", "UP   GPIO%d level=%d", SW_UP_GPIO,   sw_is_up());
}

bool sw_is_down(void)   { return gpio_get_level(SW_DOWN_GPIO) == 0; }
bool sw_is_up(void)     { return gpio_get_level(SW_DOWN_GPIO) == 1
                             && gpio_get_level(SW_UP_GPIO) == 1; }
int  sw_down_gpio(void) { return SW_DOWN_GPIO; }
int  sw_up_gpio(void)   { return SW_UP_GPIO; }
