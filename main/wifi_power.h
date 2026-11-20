#pragma once
#include "esp_http_server.h"
#include "cJSON.h"
void wifi_power_start(void);
void wifi_power_register(httpd_handle_t server);
void wifi_power_add_status(cJSON *root);
