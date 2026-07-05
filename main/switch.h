/*
 * 双限位开关模块 (微动开关 → GND, 内部上拉)
 *
 * 硬件接线:
 *   放下开关: 微动开关一端接 GPIO3, 另一端接 GND
 *   立起开关: 微动开关一端接 GPIO5, 另一端接 GND
 *
 * 电平逻辑: 内部上拉, 开关按下 = GPIO 低电平 (0)
 *
 * 功能:
 *   1. 开关状态实时读取 (sw_is_down / sw_is_up)
 *   2. 中断驱动阻塞等待 (sw_wait_down / sw_wait_up)
 *   3. 自动校准 CW/CCW 与开关的方向映射 (sw_calibrate)
 */

#ifndef SWITCH_H
#define SWITCH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 初始化限位开关
 *
 * 配置 GPIO3/GPIO5 为输入模式 + 内部上拉 + 下降沿中断.
 * 注册 ISR 服务并绑定两路中断处理函数.
 *
 * 注意:
 *   - GPIO3/GPIO5 被独占, 不可在其他模块中使用
 *   - ISR 仅置 volatile 标志, 不含阻塞操作
 *
 * 前提: 必须在其他 sw_* 函数之前调用
 */
void sw_init(void);

/*
 * 放下开关是否按下 (实时读取 GPIO 电平)
 *
 * 返回: true=按下(低电平), false=释放(高电平)
 *
 * 无缓存, 直接读 IO 寄存器, 可在 ISR 中使用
 * 前提: 已调用 sw_init()
 */
bool sw_is_down(void);

/*
 * 立起开关是否按下 (实时读取 GPIO 电平)
 *
 * 返回: true=按下(低电平), false=释放(高电平)
 *
 * 无缓存, 直接读 IO 寄存器, 可在 ISR 中使用
 * 前提: 已调用 sw_init()
 */
bool sw_is_up(void);

/*
 * 获取放下开关的 GPIO 编号
 * 返回: 当前为 GPIO_NUM_3
 */
int sw_down_gpio(void);

/*
 * 获取立起开关的 GPIO 编号
 * 返回: 当前为 GPIO_NUM_5
 */
int sw_up_gpio(void);

/*
 * 阻塞等待放下开关按下 (中断驱动 + 去抖)
 *
 * 流程:
 *   1) 若已按下 → 立即返回 true
 *   2) 清除 IRQ 标志, 循环等待 ISR 置位 (每 tick 检查一次)
 *   3) ISR 触发后等待 1 tick 去抖, 再次读 GPIO 确认
 *
 * 参数:
 *   timeout_ms  最大等待毫秒数
 *
 * 返回:
 *   true   开关已稳定按下
 *   false  超时未按下 (舵机可能卡住或方向错误)
 *
 * 注意: 阻塞期间通过 vTaskDelay(1) 让出 CPU
 * 前提: 已调用 sw_init() 且 GPIO 中断已配置
 */
bool sw_wait_down(uint32_t timeout_ms);

/*
 * 阻塞等待立起开关按下 (中断驱动 + 去抖)
 *
 * 参数/返回值/行为: 同 sw_wait_down, 目标为立起开关 (GPIO5)
 * 前提: 已调用 sw_init() 且 GPIO 中断已配置
 */
bool sw_wait_up(uint32_t timeout_ms);

/*
 * 自动校准 CW/CCW 与两个限位开关的方向映射
 *
 * 流程 (3 阶段, 各有独立超时):
 *   0) 开机时若已有开关按下 → 双向尝试离开, 确保从中间开始
 *   1) CW 旋转, 记录首先碰到的开关 → 双向释放 → 离开
 *   2) CCW 旋转, 记录碰到的开关 (可能与阶段1相同)
 *
 * 映射结论 (通过 sw_cw_hits_gpio() 获取):
 *   返回值 == 3 → CW=放下, CCW=立起
 *   返回值 == 5 → CW=立起, CCW=放下
 *
 * 返回:
 *   true   校准成功
 *   false  校准失败 (超时或无法释放开关), 舵机已停止
 *
 * 副作用: 驱动舵机旋转 (CW/CCW)
 * 前提: 已调用 servo_init() 和 sw_init()
 */
bool sw_calibrate(void);

/*
 * 获取校准结果: CW 方向碰到的开关 GPIO
 *
 * 返回: 开关 GPIO 编号 (3 或 5), 0 表示尚未校准
 * 用途: 主程序根据此值判断向目标开关应走 CW 还是 CCW
 *
 * 前提: 已调用 sw_calibrate() 且返回 true
 */
int sw_cw_hits_gpio(void);

#ifdef __cplusplus
}
#endif

#endif
