#pragma once
#include "esp_http_server.h"
#include <stdbool.h>
#include <stdint.h>
void source_control_init(void);
void source_control_register(httpd_handle_t server);
void source_control_usb_connected(uint16_t pid);
void source_control_usb_disconnected(void);
void source_control_poll(void);
bool source_control_ready(void);
