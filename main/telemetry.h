#pragma once
#include "esp_http_server.h"
#include <stdbool.h>
#include <stdint.h>
void telemetry_start(void);
void telemetry_register(httpd_handle_t server);

typedef struct {bool ready,sustained,sensor_warning,lit;uint32_t count,low_ms;} telemetry_clip_t;
void telemetry_clip_state(telemetry_clip_t *out);
