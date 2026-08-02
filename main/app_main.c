#include "servo.h"
#include "switch.h"
#include "cmd_queue.h"
#include "serial_in.h"
#include "hal/gpio_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SERVO_GPIO      GPIO_NUM_4
#define STATUS_MS       2000
#define MOVE_TIMEOUT_MS 30000
#define POLL_MS         20

static bool at_target(int gpio) {
    return (gpio == sw_down_gpio()) ? sw_is_down() : sw_is_up();
}

static void move_to(int gpio) {
    if (at_target(gpio)) return;
    if (gpio == sw_down_gpio()) servo_start_ccw();
    else                        servo_start_cw();
}

static int move_poll(int gpio, int timeout_ms, int poll_ms) {
    TickType_t start = xTaskGetTickCount();
    while (!at_target(gpio)) {
        if (xTaskGetTickCount() - start > pdMS_TO_TICKS(timeout_ms)) return MP_TIMEOUT;
        int cmd = cmd_poll();
        if (cmd >= 0) {
            cmd_post(cmd);
            return MP_NEWCMD;
        }
        vTaskDelay(pdMS_TO_TICKS(poll_ms));
    }
    return MP_REACHED;
}

void app_main(void) {
    cmd_usb_init();
    servo_init(SERVO_GPIO);
    sw_init();
    cmd_queue_init();
    cmd_status_start(STATUS_MS);
    serial_in_start();

    move_to(sw_down_gpio());
    move_poll(sw_down_gpio(), MOVE_TIMEOUT_MS, POLL_MS);
    servo_stop();

    while (1) {
        int cmd = cmd_drain_latest();
        servo_stop();

        if (cmd == CMD_STOP) {
            cmd_usb_printf("> STOP\n");
            continue;
        }
        if (cmd == CMD_CW) {
            cmd_usb_printf("> CW\n");
            servo_start_cw();
            continue;
        }
        if (cmd == CMD_CCW) {
            cmd_usb_printf("> CCW\n");
            servo_start_ccw();
            continue;
        }

        int target = (cmd == CMD_DOWN) ? sw_down_gpio() : sw_up_gpio();
        const char *label = (cmd == CMD_DOWN) ? "放下" : "立起";

        if (at_target(target)) {
            cmd_usb_printf("OK %s (already)\n", label);
            continue;
        }

        cmd_usb_printf("> %s\n", (cmd == CMD_DOWN) ? "DOWN" : "UP");
        move_to(target);
        int result = move_poll(target, MOVE_TIMEOUT_MS, POLL_MS);
        servo_stop();

        if (result == MP_REACHED) {
            cmd_usb_printf("OK %s\n", label);
        } else {
            cmd_drain_all();
        }
    }
}
