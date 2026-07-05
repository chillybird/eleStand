/*
 * 舵机控制模块 (360度连续旋转)
 *
 * 使用 LEDC 外设产生 50Hz PWM 信号控制舵机转速和方向:
 *   1.5ms 脉宽 = 停止
 *   <1.5ms    = 顺时针 (CW), 偏离越多转速越快
 *   >1.5ms    = 逆时针 (CCW), 偏离越多转速越快
 *
 * 所有接口均为非阻塞: 函数只设置 PWM 脉宽后立即返回,
 * 不等待舵机完成旋转, 由上层根据限位开关或定时器控制停止时机.
 *
 * 硬件:
 *   MG90S 360度舵机, PWM 输入接任意 GPIO
 *   ESP32-C3: 仅支持 LEDC_LOW_SPEED_MODE
 */

#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * 初始化舵机
     *
     * 配置 LEDC 外设产生 50Hz PWM 信号, 绑定到指定 GPIO.
     * 初始化完成后输出停止脉宽 (1.5ms), 舵机不转动.
     *
     * 参数:
     *   gpio_num  舵机 PWM 信号连接的 GPIO 编号
     *
     * 前提: 必须在其他 servo_* 函数之前调用
     */
    void servo_init(int gpio_num);

    /*
     * 顺时针旋转 (非阻塞)
     *
     * 输出 1100us 脉宽, 舵机持续顺时针旋转.
     * 不自动停止, 需上层调用 servo_stop() 刹车.
     *
     * 速度调节: 修改 CW_PULSE_US 宏 (偏离 1500us 越远越快)
     *
     * 前提: 已调用 servo_init()
     */
    void servo_start_cw(void);

    /*
     * 逆时针旋转 (非阻塞)
     *
     * 输出 1900us 脉宽, 舵机持续逆时针旋转.
     * 不自动停止, 需上层调用 servo_stop() 刹车.
     *
     * 速度调节: 修改 CCW_PULSE_US 宏 (偏离 1500us 越远越快)
     *
     * 前提: 已调用 servo_init()
     */
    void servo_start_ccw(void);

    /*
     * 立即停止 (非阻塞, 幂等)
     *
     * 输出 1500us 停止脉宽, 舵机立即刹车.
     * 可安全重复调用 (不会在已停止时产生副作用).
     *
     * 前提: 已调用 servo_init()
     */
    void servo_stop(void);

#ifdef __cplusplus
}
#endif

#endif
