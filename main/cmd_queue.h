#ifndef CMD_QUEUE_H
#define CMD_QUEUE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CMD_DOWN = 0,
    CMD_UP,
    CMD_STOP,
    CMD_CW,
    CMD_CCW,
};

enum { MP_REACHED, MP_TIMEOUT, MP_NEWCMD };

void cmd_usb_init(void);
void cmd_usb_printf(const char *fmt, ...);
void cmd_queue_init(void);
void cmd_post(int cmd);
int  cmd_drain_latest(void);
void cmd_drain_all(void);
int  cmd_poll(void);
void cmd_status_start(int interval_ms);

#ifdef __cplusplus
}
#endif

#endif
