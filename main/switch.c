#include "switch.h"
#include "servo.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SW_DOWN_GPIO    GPIO_NUM_3
#define SW_UP_GPIO      GPIO_NUM_5

#define CAL_POLL_MS     10
#define CAL_TIMEOUT_MS  10000

// 中断标志: ISR 置位, 主循环读取/清除
static volatile bool s_irq_down = false;
static volatile bool s_irq_up   = false;

// 校准结果: CW 方向碰到的开关 GPIO (0=未校准)
static int s_cw_hits_gpio = 0;

static void IRAM_ATTR isr_down(void *arg) { s_irq_down = true; }
static void IRAM_ATTR isr_up(void *arg)   { s_irq_up   = true; }

void sw_init(void)
{
    gpio_config_t sw_cfg = {
        .pin_bit_mask = (1ULL << SW_DOWN_GPIO) | (1ULL << SW_UP_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&sw_cfg);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(SW_DOWN_GPIO, isr_down, NULL);
    gpio_isr_handler_add(SW_UP_GPIO,   isr_up,   NULL);

    ESP_LOGI("sw", "DOWN GPIO%d level=%d irq=on", SW_DOWN_GPIO, sw_is_down());
    ESP_LOGI("sw", "UP   GPIO%d level=%d irq=on", SW_UP_GPIO,   sw_is_up());
}

bool sw_is_down(void) { return gpio_get_level(SW_DOWN_GPIO) == 0; }
bool sw_is_up(void)   { return gpio_get_level(SW_UP_GPIO)   == 0; }
int sw_down_gpio(void) { return SW_DOWN_GPIO; }
int sw_up_gpio(void)   { return SW_UP_GPIO;   }
int sw_cw_hits_gpio(void) { return s_cw_hits_gpio; }

bool sw_wait_down(uint32_t timeout_ms)
{
    if (sw_is_down()) return true;
    s_irq_down = false;
    uint32_t elapsed = 0;
    while (!s_irq_down) {
        vTaskDelay(1);
        elapsed += portTICK_PERIOD_MS;
        if (elapsed >= timeout_ms) return false;
    }
    vTaskDelay(1);                       // 去抖
    return sw_is_down();
}

bool sw_wait_up(uint32_t timeout_ms)
{
    if (sw_is_up()) return true;
    s_irq_up = false;
    uint32_t elapsed = 0;
    while (!s_irq_up) {
        vTaskDelay(1);
        elapsed += portTICK_PERIOD_MS;
        if (elapsed >= timeout_ms) return false;
    }
    vTaskDelay(1);
    return sw_is_up();
}

// 校准: 阶段0→离开开关 / 阶段1→CW 识别 / 释放 / 阶段2→CCW 识别
bool sw_calibrate(void)
{
    int down_gpio = sw_down_gpio();
    int up_gpio   = sw_up_gpio();

    ESP_LOGI("sw", "===== Calibrating =====");

    // 阶段0: 若开机时已有开关按下, 先双向离开
    if (sw_is_down() || sw_is_up()) {
        ESP_LOGI("sw", "Starting on switch, moving off...");
        servo_start_cw();
        TickType_t t0 = xTaskGetTickCount();
        while ((sw_is_down() || sw_is_up()) &&
               (xTaskGetTickCount() - t0 < pdMS_TO_TICKS(3000))) {
            vTaskDelay(pdMS_TO_TICKS(CAL_POLL_MS));
        }
        servo_stop();
        if (sw_is_down() || sw_is_up()) {
            servo_start_ccw();
            t0 = xTaskGetTickCount();
            while ((sw_is_down() || sw_is_up()) &&
                   (xTaskGetTickCount() - t0 < pdMS_TO_TICKS(3000))) {
                vTaskDelay(pdMS_TO_TICKS(CAL_POLL_MS));
            }
            servo_stop();
        }
        if (sw_is_down() || sw_is_up()) {
            ESP_LOGE("sw", "Cannot move off switch");
            return false;
        }
        ESP_LOGI("sw", "Moved off switch");
    }

    // 阶段1: CW 旋转, 记录首先碰到的开关
    ESP_LOGI("sw", "Phase 1: CW...");
    servo_start_cw();
    int hit1 = 0;
    TickType_t start = xTaskGetTickCount();
    while (!hit1) {
        if (sw_is_down()) hit1 = down_gpio;
        if (sw_is_up())   hit1 = up_gpio;
        if (xTaskGetTickCount() - start > pdMS_TO_TICKS(CAL_TIMEOUT_MS)) {
            servo_stop();
            ESP_LOGE("sw", "CW timeout");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(CAL_POLL_MS));
    }
    servo_stop();
    s_cw_hits_gpio = hit1;
    ESP_LOGI("sw", "CW -> GPIO%d", hit1);

    // 双向尝试释放开关
    ESP_LOGI("sw", "Releasing GPIO%d...", hit1);
    bool released = false;
    for (int try_dir = 0; try_dir < 2; try_dir++) {
        if (try_dir == 0) servo_start_ccw();
        else              servo_start_cw();
        start = xTaskGetTickCount();
        while (1) {
            bool still = (hit1 == down_gpio) ? sw_is_down() : sw_is_up();
            if (!still) { released = true; break; }
            if (xTaskGetTickCount() - start > pdMS_TO_TICKS(2000)) break;
            vTaskDelay(pdMS_TO_TICKS(CAL_POLL_MS));
        }
        servo_stop();
        if (released) break;
    }
    if (!released) {
        ESP_LOGE("sw", "Cannot release switch GPIO%d", hit1);
        return false;
    }
    ESP_LOGI("sw", "Released");

    // 阶段2: CCW 旋转, 记录碰到的开关
    ESP_LOGI("sw", "Phase 2: CCW...");
    servo_start_ccw();
    int hit2 = 0;
    start = xTaskGetTickCount();
    while (!hit2) {
        if (sw_is_down()) hit2 = down_gpio;
        if (sw_is_up())   hit2 = up_gpio;
        if (xTaskGetTickCount() - start > pdMS_TO_TICKS(CAL_TIMEOUT_MS)) {
            servo_stop();
            ESP_LOGE("sw", "CCW timeout");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(CAL_POLL_MS));
    }
    servo_stop();

    if (hit2 == hit1) ESP_LOGW("sw", "Both directions hit GPIO%d", hit1);
    else              ESP_LOGI("sw", "CCW -> GPIO%d", hit2);

    ESP_LOGI("sw", "===== Calibration OK =====");
    ESP_LOGI("sw", "%s -> 放下  GPIO%d",
             (s_cw_hits_gpio == down_gpio) ? "CW" : "CCW", down_gpio);
    ESP_LOGI("sw", "%s -> 立起  GPIO%d",
             (s_cw_hits_gpio == down_gpio) ? "CCW" : "CW", up_gpio);
    return true;
}
