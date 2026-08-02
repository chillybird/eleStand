#include "cmd_queue.h"
#include "switch.h"
#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include <stdarg.h>
#include <stdio.h>

static QueueHandle_t s_queue;

void cmd_usb_init(void) {
    usb_serial_jtag_driver_config_t cfg =
        USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_jtag_driver_install(&cfg);
}

void cmd_usb_printf(const char *fmt, ...) {
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

void cmd_queue_init(void) {
    s_queue = xQueueCreate(8, sizeof(int));
}

void cmd_post(int cmd) {
    xQueueSend(s_queue, &cmd, 0);
}

int cmd_drain_latest(void) {
    int cmd;
    if (xQueueReceive(s_queue, &cmd, portMAX_DELAY) != pdTRUE) return -1;
    int latest;
    while (xQueueReceive(s_queue, &latest, 0) == pdTRUE) cmd = latest;
    return cmd;
}

void cmd_drain_all(void) {
    int cmd;
    while (xQueueReceive(s_queue, &cmd, 0) == pdTRUE) {}
}

int cmd_poll(void) {
    int cmd;
    if (xQueueReceive(s_queue, &cmd, 0) == pdTRUE) return cmd;
    return -1;
}

static void status_timer_cb(TimerHandle_t xTimer) {
    cmd_usb_printf("S: DOWN=%s UP=%s\n",
                   sw_is_down() ? "1" : "0",
                   sw_is_up()   ? "1" : "0");
}

void cmd_status_start(int interval_ms) {
    TimerHandle_t timer = xTimerCreate("status", pdMS_TO_TICKS(interval_ms),
                                       pdTRUE, NULL, status_timer_cb);
    xTimerStart(timer, 0);
}
