#pragma once
#include "esp_err.h"
#include "protocol.h"
void dsp_usb_start(void);
// Serialized entire transactions; callers must not hold stack-owned requests after timeout.
esp_err_t dsp_read_eq(uint8_t module, uint16_t words[DSP_EQ_WORDS]);
esp_err_t dsp_apply_eq(uint8_t module, const uint16_t words[DSP_EQ_WORDS], uint16_t actual[DSP_EQ_WORDS]);

// Runtime offset only; scalar write is serialized with all EQ transactions.
esp_err_t dsp_set_sub_attenuation(unsigned attenuation_q8);

// Atomic gain-only update, serialized with all other USB requests.
esp_err_t dsp_set_gain(uint8_t module,uint8_t selector,int16_t gain_q8);
bool dsp_eq_snapshot(uint8_t module,uint16_t words[DSP_EQ_WORDS],uint32_t *revision);
