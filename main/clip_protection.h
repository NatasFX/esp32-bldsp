#pragma once
#include "esp_http_server.h"
#include "cJSON.h"
void clip_protection_start(void);
void clip_protection_register(httpd_handle_t server);
void clip_protection_add_status(cJSON *parent);
