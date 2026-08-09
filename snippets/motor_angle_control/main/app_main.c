/*
 * test_esp - MG90S 位置舵机角度控制
 * GPIO4 PWM, 串口发送 0-180 设角度
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"

#define SERVO_GPIO      GPIO_NUM_4
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define LEDC_DUTY_RES   LEDC_TIMER_14_BIT
#define LEDC_FREQ       50
#define DUTY_MAX        ((1 << LEDC_DUTY_RES) - 1)

#define PULSE_MIN_US    500
#define PULSE_MAX_US    2400
#define PWM_PERIOD_US   20000

static void servo_write(int angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    float pulse_us = PULSE_MIN_US + (float)angle / 180.0f * (PULSE_MAX_US - PULSE_MIN_US);
    uint32_t duty = (uint32_t)(pulse_us / PWM_PERIOD_US * DUTY_MAX);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

static void servo_init(void) {
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_MODE, .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER, .freq_hz = LEDC_FREQ, .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t ch = {
        .gpio_num = SERVO_GPIO, .speed_mode = LEDC_MODE, .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER, .duty = 0, .hpoint = 0, .intr_type = LEDC_INTR_DISABLE,
    };
    ledc_channel_config(&ch);
}

void app_main(void) {
    servo_init();
    servo_write(90);

    usb_serial_jtag_driver_config_t usb = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_jtag_driver_install(&usb);

    printf("Servo ready. Send 0-180:\n");

    char buf[16];
    int idx = 0;
    while (1) {
        uint8_t ch;
        int n = usb_serial_jtag_read_bytes(&ch, 1, pdMS_TO_TICKS(50));
        if (n > 0) {
            if (ch == '\n' || ch == '\r') {
                if (idx > 0) {
                    buf[idx] = '\0';
                    servo_write(atoi(buf));
                    printf("angle=%d\n", atoi(buf));
                    idx = 0;
                }
            } else if (idx < (int)sizeof(buf) - 1) {
                buf[idx++] = (char)ch;
            }
        }
    }
}
