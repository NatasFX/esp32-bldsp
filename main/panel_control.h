#pragma once
#include "esp_http_server.h"
#include "cJSON.h"
void panel_start(void);
void panel_register(httpd_handle_t server);
void panel_add_status(cJSON *root);
