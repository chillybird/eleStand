/*
 * MG90S 360度舵机 + 双限位开关控制 (ESP32-C3 SuperMini)
 *
 * 硬件:
 *   舵机 PWM     → GPIO4
 *   放下限位开关 → GPIO3 (微动开关到GND, 按下=放下)
 *   立起限位开关 → GPIO5 (微动开关到GND, 按下=立起)
 *   通信         → USB Serial/JTAG (CDC)
 *
 * 工作流程:
 *   1. 上电自动校准 → sw_calibrate()
 *   2. 校准完成回到"放下"位置
 *   3. 启动串口命令监听 + 定时状态上报
 *
 * 串口命令: down(d) up(u) cw ccw stop(s) status help
 */

#include "servo.h"
#include "switch.h"
#include "hal/gpio_types.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <stdio.h>
#include <string.h>

#define SERVO_GPIO  GPIO_NUM_4
#define STATUS_MS   2000

// USB CDC 直写 (不经 UART/stdio)
static void usb_printf(const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) {
        usb_serial_jtag_write_bytes((const uint8_t *)buf, len,
                                     pdMS_TO_TICKS(50));
    }
}

// 非阻塞移动: 中断驱动, 碰开关即停
static void go_to(int target_gpio, const char *label)
{
    int cw_hits = sw_cw_hits_gpio();
    if (cw_hits == 0) {
        ESP_LOGW("cmd", "Not calibrated");
        return;
    }

    // 已在目标位置
    if ((target_gpio == sw_down_gpio() && sw_is_down()) ||
        (target_gpio == sw_up_gpio()   && sw_is_up())) {
        usb_printf("OK %s (already)\n", label);
        return;
    }

    if (cw_hits == target_gpio) servo_start_cw();
    else                        servo_start_ccw();

    bool ok = (target_gpio == sw_down_gpio()) ? sw_wait_down(30000)
                                              : sw_wait_up(30000);
    servo_stop();

    if (!ok) {
        ESP_LOGE("cmd", "Timeout reaching %s", label);
        return;
    }
    usb_printf("OK %s\n", label);
    ESP_LOGI("cmd", "Done: %s", label);
}

static void handle_cmd(const char *cmd)
{
    if (strcmp(cmd, "down") == 0 || strcmp(cmd, "d") == 0) {
        usb_printf("> DOWN\n");
        go_to(sw_down_gpio(), "放下");
    } else if (strcmp(cmd, "up") == 0 || strcmp(cmd, "u") == 0) {
        usb_printf("> UP\n");
        go_to(sw_up_gpio(), "立起");
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
        usb_printf("DOWN(GPIO%d): %s\n", sw_down_gpio(),
                    sw_is_down() ? "DOWN" : "--");
        usb_printf("UP  (GPIO%d): %s\n", sw_up_gpio(),
                    sw_is_up()   ? "UP"   : "--");
    } else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        usb_printf("down(d) up(u) cw ccw stop(s) status help\n");
    } else if (strlen(cmd) > 0) {
        usb_printf("? '%s'\n", cmd);
    }
}

static void status_timer_cb(TimerHandle_t xTimer)
{
    usb_printf("S: DOWN=%s UP=%s\n",
               sw_is_down() ? "1" : "0",
               sw_is_up()   ? "1" : "0");
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

void app_main(void)
{
    usb_serial_jtag_driver_config_t usb_cfg =
        USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_jtag_driver_install(&usb_cfg);

    servo_init(SERVO_GPIO);
    sw_init();

    if (!sw_calibrate()) {
        usb_printf("ERROR: Calibration failed\n");
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    go_to(sw_down_gpio(), "放下");

    xTaskCreate(serial_task, "serial", 4096, NULL, 5, NULL);

    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
}
