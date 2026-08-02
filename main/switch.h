/*
 * 双限位开关模块 (微动开关 → GND, 内部上拉)
 *
 * 放下=GPIO3, 立起=GPIO5
 * CW=放下, CCW=立起 (方向固定, 无需校准)
 */

#ifndef SWITCH_H
#define SWITCH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void sw_init(void);
bool sw_is_down(void);
bool sw_is_up(void);
int  sw_down_gpio(void);
int  sw_up_gpio(void);

#ifdef __cplusplus
}
#endif

#endif
