#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_http_server.h"
void thermal_test_sample(uint64_t at_ms,bool valid,double temperature,int raw,int millivolts);
void thermal_test_register(httpd_handle_t server);
