#include "servo.h"
#include "cmd_queue.h"
#include "serial_in.h"
#include "wifi.h"
#include "http_api.h"
#include "hal/gpio_types.h"

#define SERVO_GPIO  GPIO_NUM_4

void app_main(void) {
    cmd_usb_init();
    servo_init(SERVO_GPIO);
    cmd_queue_init();
    serial_in_start();
    wifi_init();
    http_server_start();

    while (1) {
        int cmd = cmd_drain_latest();
        if (cmd == CMD_DOWN) {
            servo_set_angle(0);
            cmd_usb_printf("> DOWN (0 deg)\n");
        } else if (cmd == CMD_UP) {
            servo_set_angle(90);
            cmd_usb_printf("> UP (90 deg)\n");
        }
    }
}
