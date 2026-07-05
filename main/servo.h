#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void servo_init(int gpio_num);
void servo_start_cw(void);
void servo_start_ccw(void);
void servo_stop(void);

#ifdef __cplusplus
}
#endif

#endif
