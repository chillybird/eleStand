#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void wifi_init(void);
void wifi_connect(const char *ssid, const char *pass);
const char *wifi_get_ip(void);
bool wifi_is_sta(void);

#ifdef __cplusplus
}
#endif

#endif
