#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_http_server.h"
void panel_ota_init(void);
bool panel_ota_receive(const uint8_t *data,int len);
bool panel_ota_busy(void);
void panel_ota_register(httpd_handle_t server);
