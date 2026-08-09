#ifndef SERVO_H
#define SERVO_H

#ifdef __cplusplus
extern "C" {
#endif

void servo_init(int gpio_num);
void servo_set_angle(int degree);
int  servo_get_angle(void);

#ifdef __cplusplus
}
#endif

#endif
