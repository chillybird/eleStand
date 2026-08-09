#include "servo.h"
#include "driver/ledc.h"
#include "esp_log.h"

#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define LEDC_DUTY_RES   LEDC_TIMER_14_BIT
#define LEDC_FREQ       50
#define DUTY_MAX        ((1 << LEDC_DUTY_RES) - 1)

#define PULSE_MIN_US    500
#define PULSE_MAX_US    2400
#define PWM_PERIOD_US   20000

static int s_angle = 0;

static uint32_t angle_to_duty(int deg) {
    float us = PULSE_MIN_US + (float)deg / 180.0f * (PULSE_MAX_US - PULSE_MIN_US);
    return (uint32_t)(us / PWM_PERIOD_US * DUTY_MAX);
}

void servo_init(int gpio) {
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_MODE, .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER, .freq_hz = LEDC_FREQ, .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t ch = {
        .gpio_num = gpio, .speed_mode = LEDC_MODE, .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER, .duty = 0, .hpoint = 0, .intr_type = LEDC_INTR_DISABLE,
    };
    ledc_channel_config(&ch);

    servo_set_angle(0);
    ESP_LOGI("servo", "Init GPIO%d", gpio);
}

void servo_set_angle(int deg) {
    if (deg < 0) deg = 0;
    if (deg > 180) deg = 180;
    s_angle = deg;
    uint32_t duty = angle_to_duty(deg);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

int servo_get_angle(void) { return s_angle; }
