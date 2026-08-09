#include "serial_in.h"
#include "cmd_queue.h"
#include "servo.h"
#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

static void handle_cmd(const char *buf) {
    if (strcmp(buf, "down") == 0 || strcmp(buf, "d") == 0) {
        cmd_post(CMD_DOWN);
    } else if (strcmp(buf, "up") == 0 || strcmp(buf, "u") == 0) {
        cmd_post(CMD_UP);
    } else if (strncmp(buf, "angle ", 6) == 0) {
        servo_set_angle(atoi(buf + 6));
        cmd_usb_printf("angle=%d\n", servo_get_angle());
    } else if (strcmp(buf, "status") == 0) {
        cmd_usb_printf("angle=%d\n", servo_get_angle());
    } else if (strcmp(buf, "help") == 0 || strcmp(buf, "?") == 0) {
        cmd_usb_printf("down/up  angle 0-180  status  help\n");
    } else {
        cmd_usb_printf("? '%s'\n", buf);
    }
}

static void serial_task(void *arg) {
    cmd_usb_printf("\n=== Servo Ready ===\n");
    cmd_usb_printf("down/up  angle 0-180  status  help\n\n");

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
                    handle_cmd(buf);
                }
            } else if (idx < (int)sizeof(buf) - 1) {
                buf[idx++] = (char)ch;
            }
        }
    }
}

void serial_in_start(void) {
    xTaskCreate(serial_task, "serial", 4096, NULL, 5, NULL);
}
