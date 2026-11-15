#pragma once
#include "esp_err.h"
#include "protocol.h"
void dsp_usb_start(void);
// Serialized entire transactions; callers must not hold stack-owned requests after timeout.
esp_err_t dsp_read_eq(uint8_t module, uint16_t words[DSP_EQ_WORDS]);
esp_err_t dsp_apply_eq(uint8_t module, const uint16_t words[DSP_EQ_WORDS], uint16_t actual[DSP_EQ_WORDS]);
