#pragma once
#include "esp_http_server.h"
#include <stdint.h>
esp_err_t presets_init(void);
esp_err_t presets_read(int slot,uint16_t sub[53],uint16_t stereo[53]);
void presets_register(httpd_handle_t server);
