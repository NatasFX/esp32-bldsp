#pragma once
#include "esp_http_server.h"
void telemetry_start(void);
void telemetry_register(httpd_handle_t server);
