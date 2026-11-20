#pragma once
#include "cJSON.h"
#include "esp_http_server.h"
#include "panel_protocol.h"
void battery_start(void);
void battery_register(httpd_handle_t server);
void battery_ingest(const panel_vcc_t *p,uint32_t primary_boot);
void battery_status(cJSON *root);
unsigned battery_alert(void);
