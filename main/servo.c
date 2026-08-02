#include "servo.h"
#include "driver/ledc.h"
#include "esp_log.h"

#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define LEDC_DUTY_RES   LEDC_TIMER_14_BIT
#define LEDC_FREQUENCY  50
#define DUTY_MAX        ((1 << LEDC_DUTY_RES) - 1)

#define STOP_PULSE_US   1500
#define DEFAULT_CW_US   1400
#define DEFAULT_CCW_US  1600
#define PWM_PERIOD_US   20000

static int s_servo_gpio = -1;
static uint32_t s_cw_pulse_us  = DEFAULT_CW_US;
static uint32_t s_ccw_pulse_us = DEFAULT_CCW_US;

static uint32_t pulse_to_duty(uint32_t pulse_us) {
    return (uint32_t)(((float)pulse_us / PWM_PERIOD_US) * DUTY_MAX);
}

static void write_pulse(uint32_t pulse_us) {
    uint32_t duty = pulse_to_duty(pulse_us);
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

void servo_init(int gpio_num) {
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

    servo_stop();
    ESP_LOGI("servo", "Initialized on GPIO %d", s_servo_gpio);
}

void servo_start_cw(void)       { write_pulse(s_cw_pulse_us);  }
void servo_start_ccw(void)      { write_pulse(s_ccw_pulse_us); }
void servo_stop(void)           { write_pulse(STOP_PULSE_US);  }

void servo_set_speed(int percent) {
    if (percent < 0)   percent = 0;
    if (percent > 100) percent = 100;
    if (percent == 0) {
        s_cw_pulse_us  = STOP_PULSE_US;
        s_ccw_pulse_us = STOP_PULSE_US;
    } else {
        s_cw_pulse_us  = STOP_PULSE_US - (percent * 10);
        s_ccw_pulse_us = STOP_PULSE_US + (percent * 10);
    }
    ESP_LOGI("servo", "Speed %d%% (CW=%lu CCW=%lu us)",
             percent, s_cw_pulse_us, s_ccw_pulse_us);
}
