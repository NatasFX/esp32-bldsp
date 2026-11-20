#pragma once
#include <stdbool.h>
#include <stdint.h>
void panel_ota_start(void);
bool panel_ota_receive(const uint8_t *data,int len);
void panel_ota_health(bool healthy);
bool panel_ota_busy(void);
