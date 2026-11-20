#pragma once
#include "esp_http_server.h"
#include "cJSON.h"
void lighting_start(void);
void lighting_register(httpd_handle_t server);
unsigned lighting_brightness(unsigned manual);
void lighting_status(cJSON *root,unsigned manual);
