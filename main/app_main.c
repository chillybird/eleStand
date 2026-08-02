#include "servo.h"
#include "switch.h"
#include "hal/gpio_types.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERVO_GPIO  GPIO_NUM_4
#define STATUS_MS   2000

enum {
    CMD_DOWN = 0,
    CMD_UP,
    CMD_STOP,
    CMD_CW,
    CMD_CCW,
};

enum { MP_REACHED, MP_TIMEOUT, MP_NEWCMD };

static QueueHandle_t g_cmd_queue;

static void usb_printf(const char *fmt, ...) {
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

static bool sw_at_target(int target) {
    return (target == sw_down_gpio()) ? sw_is_down() : sw_is_up();
}

static void move_to(int target) {
    if (sw_at_target(target)) return;
    if (target == sw_down_gpio()) servo_start_ccw();
    else                          servo_start_cw();
}

static int move_poll(int target, int timeout_ms, int poll_ms) {
    TickType_t start = xTaskGetTickCount();
    while (!sw_at_target(target)) {
        if (xTaskGetTickCount() - start > pdMS_TO_TICKS(timeout_ms)) return MP_TIMEOUT;
        int cmd;
        if (xQueueReceive(g_cmd_queue, &cmd, pdMS_TO_TICKS(poll_ms)) == pdTRUE) {
            xQueueSendToFront(g_cmd_queue, &cmd, 0);
            return MP_NEWCMD;
        }
    }
    return MP_REACHED;
}

static void post_cmd(int cmd) {
    xQueueSend(g_cmd_queue, &cmd, 0);
}

static void status_timer_cb(TimerHandle_t xTimer) {
    usb_printf("S: DOWN=%s UP=%s\n",
               sw_is_down() ? "1" : "0",
               sw_is_up()   ? "1" : "0");
}

static void serial_task(void *arg) {
    usb_printf("\n=== Servo Ready ===\n");
    usb_printf("down(d)/up(u)/cw/ccw/stop(s)/speed(0-100)/help\n\n");

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
                    idx = 0;

                    if (strcmp(buf, "down") == 0 || strcmp(buf, "d") == 0) {
                        usb_printf("> DOWN\n");
                        post_cmd(CMD_DOWN);
                    } else if (strcmp(buf, "up") == 0 || strcmp(buf, "u") == 0) {
                        usb_printf("> UP\n");
                        post_cmd(CMD_UP);
                    } else if (strcmp(buf, "stop") == 0 || strcmp(buf, "s") == 0) {
                        post_cmd(CMD_STOP);
                    } else if (strcmp(buf, "cw") == 0) {
                        post_cmd(CMD_CW);
                    } else if (strcmp(buf, "ccw") == 0) {
                        post_cmd(CMD_CCW);
                    } else if (strncmp(buf, "speed ", 6) == 0) {
                        servo_set_speed(atoi(buf + 6));
                        usb_printf("> SPEED %d%%\n", atoi(buf + 6));
                    } else if (strcmp(buf, "status") == 0) {
                        usb_printf("DOWN(GPIO%d): %s\n", sw_down_gpio(),
                                    sw_is_down() ? "DOWN" : "--");
                        usb_printf("UP  (GPIO%d): %s\n", sw_up_gpio(),
                                    sw_is_up() ? "UP" : "--");
                    } else if (strcmp(buf, "debug") == 0) {
                        usb_printf("=== DEBUG ===\n");
                        usb_printf("CW=放下 GPIO%d, CCW=立起 GPIO%d\n",
                                    sw_down_gpio(), sw_up_gpio());
                        usb_printf("DOWN(GPIO%d): %s\n", sw_down_gpio(),
                                    sw_is_down() ? "PRESSED" : "open");
                        usb_printf("UP  (GPIO%d): %s\n", sw_up_gpio(),
                                    sw_is_up() ? "PRESSED" : "open");
                        usb_printf("queue_waiting: %d\n",
                                    (int)uxQueueMessagesWaiting(g_cmd_queue));
                        usb_printf("==============\n");
                    } else if (strcmp(buf, "help") == 0 || strcmp(buf, "?") == 0) {
                        usb_printf("down(d)/up(u)/cw/ccw/stop(s)/speed(0-100)/help\n");
                    } else {
                        usb_printf("? '%s'\n", buf);
                    }
                }
            } else if (idx < (int)sizeof(buf) - 1) {
                buf[idx++] = (char)ch;
            }
        }
    }
}

void app_main(void) {
    usb_serial_jtag_driver_config_t usb_cfg =
        USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_jtag_driver_install(&usb_cfg);

    servo_init(SERVO_GPIO);
    sw_init();

    g_cmd_queue = xQueueCreate(8, sizeof(int));
    xTaskCreate(serial_task, "serial", 4096, NULL, 5, NULL);

    move_to(sw_down_gpio());
    move_poll(sw_down_gpio(), 30000, 20);
    servo_stop();

    int cmd;
    while (1) {
        if (xQueueReceive(g_cmd_queue, &cmd, portMAX_DELAY) != pdTRUE) continue;

        int latest;
        while (xQueueReceive(g_cmd_queue, &latest, 0) == pdTRUE) cmd = latest;

        servo_stop();

        if (cmd == CMD_STOP) {
            usb_printf("> STOP\n");
            continue;
        }
        if (cmd == CMD_CW) {
            usb_printf("> CW\n");
            servo_start_cw();
            continue;
        }
        if (cmd == CMD_CCW) {
            usb_printf("> CCW\n");
            servo_start_ccw();
            continue;
        }

        int target = (cmd == CMD_DOWN) ? sw_down_gpio() : sw_up_gpio();
        const char *label = (cmd == CMD_DOWN) ? "放下" : "立起";

        if (sw_at_target(target)) {
            usb_printf("OK %s (already)\n", label);
            continue;
        }

        usb_printf("> %s\n", (cmd == CMD_DOWN) ? "DOWN" : "UP");
        move_to(target);
        int result = move_poll(target, 30000, 20);
        servo_stop();

        if (result == MP_REACHED) {
            usb_printf("OK %s\n", label);
        } else {
            // 超时或新命令中断: 清空队列, 不修改状态, 不回滚
            int drain;
            while (xQueueReceive(g_cmd_queue, &drain, 0) == pdTRUE) {}
        }
    }
}
