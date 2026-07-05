#include "servo.h"

#include <stdio.h>
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL        LEDC_CHANNEL_0
#define LEDC_DUTY_RES       LEDC_TIMER_14_BIT
#define LEDC_FREQUENCY      50
#define DUTY_MAX            ((1 << LEDC_DUTY_RES) - 1)

#define STOP_PULSE_US       1500
#define CW_PULSE_US         1100
#define CCW_PULSE_US        1900
#define PWM_PERIOD_US       20000

static int s_servo_gpio = -1;

static uint32_t pulse_to_duty(uint32_t pulse_us)
{
    return (uint32_t)(((float)pulse_us / PWM_PERIOD_US) * DUTY_MAX);
}

static void write_pulse(uint32_t pulse_us)
{
    uint32_t duty = pulse_to_duty(pulse_us);
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

void servo_init(int gpio_num)
{
    s_servo_gpio = gpio_num;

    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .gpio_num       = s_servo_gpio,
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .duty           = 0,
        .hpoint         = 0,
        .intr_type      = LEDC_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    write_pulse(STOP_PULSE_US);
    ESP_LOGI("servo", "Initialized on GPIO %d", s_servo_gpio);
}

void servo_start_cw(void)
{
    write_pulse(CW_PULSE_US);
}

void servo_start_ccw(void)
{
    write_pulse(CCW_PULSE_US);
}

void servo_stop(void)
{
    write_pulse(STOP_PULSE_US);
}
