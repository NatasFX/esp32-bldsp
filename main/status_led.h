#pragma once
#include <stdbool.h>
#include <stdint.h>
void status_led_init(void);
void status_led_system_ready(void);
void status_led_usb(bool present,bool bluetooth_mode);
void status_led_source_error(bool failed);
void status_led_bootstrap(bool success);
void status_led_request_begin(void);
void status_led_request_end(uint8_t module,bool success);
