/*
 * MG90S 360度舵机 + 双限位开关控制 (ESP32-C3 SuperMini)
 *
 * 硬件:
 *   舵机 PWM     → GPIO4
 *   放下限位开关 → GPIO3 (微动开关到GND, 按下=放下)
 *   立起限位开关 → GPIO5 (微动开关到GND, 按下=立起)
 *   通信         → USB Serial/JTAG (CDC)
 *
 * 上电自动校准, 串口命令: down(d)/up(u)/cw/ccw/stop(s)/status/help
 */

#include "servo.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <stdio.h>
#include <string.h>

#define SERVO_GPIO      GPIO_NUM_4
#define SW_DOWN_GPIO    GPIO_NUM_3
#define SW_UP_GPIO      GPIO_NUM_5
#define POLL_MS         20
#define CAL_TIMEOUT_MS  10000
#define BACKOFF_MS      500
#define STATUS_MS       2000

static int g_cw_hits_gpio = 0;

static void usb_printf(const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) {
        usb_serial_jtag_write_bytes((const uint8_t *)buf, len, pdMS_TO_TICKS(50));
    }
}

static bool sw_pressed(int gpio)
{
    return gpio_get_level(gpio) == 0;
}

static void switches_init(void)
{
    gpio_config_t sw_cfg = {
        .pin_bit_mask = (1ULL << SW_DOWN_GPIO) | (1ULL << SW_UP_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&sw_cfg);

    ESP_LOGI("sw", "DOWN GPIO%d level=%d", SW_DOWN_GPIO, sw_pressed(SW_DOWN_GPIO));
    ESP_LOGI("sw", "UP   GPIO%d level=%d", SW_UP_GPIO,   sw_pressed(SW_UP_GPIO));
}

static void go_to(int target_gpio, const char *label)
{
    if (g_cw_hits_gpio == 0) {
        ESP_LOGW("cmd", "Not calibrated");
        return;
    }

    int dir = (g_cw_hits_gpio == target_gpio) ? 0 : 1;
    if (dir == 0) {
        servo_start_cw();
    } else {
        servo_start_ccw();
    }

    uint32_t elapsed = 0;
    while (!sw_pressed(target_gpio)) {
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
        elapsed += POLL_MS;
        if (elapsed > 30000) {
            ESP_LOGE("cmd", "Timeout reaching %s", label);
            servo_stop();
            return;
        }
    }
    servo_stop();

    usb_printf("OK %s\n", label);
    ESP_LOGI("cmd", "Done: %s", label);
}

static void handle_cmd(const char *cmd)
{
    if (strcmp(cmd, "down") == 0 || strcmp(cmd, "d") == 0) {
        usb_printf("> DOWN\n");
        go_to(SW_DOWN_GPIO, "放下");
    } else if (strcmp(cmd, "up") == 0 || strcmp(cmd, "u") == 0) {
        usb_printf("> UP\n");
        go_to(SW_UP_GPIO, "立起");
    } else if (strcmp(cmd, "stop") == 0 || strcmp(cmd, "s") == 0) {
        servo_stop();
        usb_printf("> STOP\n");
    } else if (strcmp(cmd, "cw") == 0) {
        servo_start_cw();
        usb_printf("> CW\n");
    } else if (strcmp(cmd, "ccw") == 0) {
        servo_start_ccw();
        usb_printf("> CCW\n");
    } else if (strcmp(cmd, "status") == 0) {
        usb_printf("DOWN(GPIO%d): %s\n", SW_DOWN_GPIO, sw_pressed(SW_DOWN_GPIO) ? "DOWN" : "--");
        usb_printf("UP  (GPIO%d): %s\n", SW_UP_GPIO,   sw_pressed(SW_UP_GPIO)   ? "UP"   : "--");
    } else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        usb_printf("down(d) up(u) cw ccw stop(s) status help\n");
    } else if (strlen(cmd) > 0) {
        usb_printf("? '%s'\n", cmd);
    }
}

static void status_timer_cb(TimerHandle_t xTimer)
{
    usb_printf("S: DOWN=%s UP=%s\n",
               sw_pressed(SW_DOWN_GPIO) ? "1" : "0",
               sw_pressed(SW_UP_GPIO)   ? "1" : "0");
}

static void serial_task(void *arg)
{
    usb_printf("\n=== Servo Ready ===\n");
    usb_printf("down(d)/up(u)/cw/ccw/stop(s)/status/help\n\n");

    TimerHandle_t timer = xTimerCreate("status", pdMS_TO_TICKS(STATUS_MS),
                                       pdTRUE, NULL, status_timer_cb);
    xTimerStart(timer, 0);

    char buf[64];
    int idx = 0;

    while (1) {
        uint8_t ch;
        int n = usb_serial_jtag_read_bytes(&ch, 1, pdMS_TO_TICKS(50));
        if (n > 0) {
            if (ch == '\n' || ch == '\r') {
                if (idx > 0) {
                    buf[idx] = '\0';
                    handle_cmd(buf);
                    idx = 0;
                }
            } else if (idx < (int)sizeof(buf) - 1) {
                buf[idx++] = (char)ch;
            }
        }
    }
}

static bool calibrate(void)
{
    ESP_LOGI("cal", "===== Calibrating =====");

    ESP_LOGI("cal", "CW...");
    servo_start_cw();

    int hit = 0;
    TickType_t start = xTaskGetTickCount();
    while (!hit) {
        if (sw_pressed(SW_DOWN_GPIO)) hit = SW_DOWN_GPIO;
        if (sw_pressed(SW_UP_GPIO))   hit = SW_UP_GPIO;
        if (xTaskGetTickCount() - start > pdMS_TO_TICKS(CAL_TIMEOUT_MS)) {
            servo_stop();
            ESP_LOGE("cal", "CW timeout");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
    servo_stop();
    g_cw_hits_gpio = hit;
    ESP_LOGI("cal", "CW -> GPIO%d", hit);

    servo_start_ccw();
    vTaskDelay(pdMS_TO_TICKS(BACKOFF_MS));
    servo_stop();

    int other = (g_cw_hits_gpio == SW_DOWN_GPIO) ? SW_UP_GPIO : SW_DOWN_GPIO;
    ESP_LOGI("cal", "CCW, expecting GPIO%d...", other);
    servo_start_ccw();

    hit = 0;
    start = xTaskGetTickCount();
    while (!hit) {
        if (sw_pressed(other)) hit = other;
        if (xTaskGetTickCount() - start > pdMS_TO_TICKS(CAL_TIMEOUT_MS)) {
            servo_stop();
            ESP_LOGE("cal", "CCW timeout");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
    servo_stop();

    ESP_LOGI("cal", "===== Calibration OK =====");
    ESP_LOGI("cal", "%s -> 放下  GPIO%d",
             (g_cw_hits_gpio == SW_DOWN_GPIO) ? "CW" : "CCW", SW_DOWN_GPIO);
    ESP_LOGI("cal", "%s -> 立起  GPIO%d",
             (g_cw_hits_gpio == SW_DOWN_GPIO) ? "CCW" : "CW", SW_UP_GPIO);
    return true;
}

void app_main(void)
{
    usb_serial_jtag_driver_config_t usb_cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_jtag_driver_install(&usb_cfg);

    servo_init(SERVO_GPIO);
    switches_init();

    if (!calibrate()) {
        usb_printf("ERROR: Calibration failed\n");
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    go_to(SW_DOWN_GPIO, "放下");

    xTaskCreate(serial_task, "serial", 4096, NULL, 5, NULL);
}
