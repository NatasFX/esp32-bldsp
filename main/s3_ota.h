#pragma once
#include "esp_http_server.h"
void s3_ota_register(httpd_handle_t server);
void s3_ota_ready(void);
