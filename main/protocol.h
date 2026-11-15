#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define DSP_REPORT_SIZE 256
#define DSP_EQ_WORDS 53
size_t dsp_frame(uint8_t *out, uint8_t module, const uint8_t *payload, size_t len);
bool dsp_valid_frame(const uint8_t *p, size_t n, uint8_t module);
bool dsp_decode_eq(const uint8_t *p, size_t n, uint16_t words[DSP_EQ_WORDS]);
size_t dsp_encode_eq(uint8_t *p, const uint16_t words[DSP_EQ_WORDS]);
bool dsp_decode_eq_module(const uint8_t *p, size_t n, uint8_t module, uint16_t words[DSP_EQ_WORDS]);
size_t dsp_encode_eq_module(uint8_t *p, uint8_t module, const uint16_t words[DSP_EQ_WORDS]);
bool dsp_valid_eq_words(const uint16_t words[DSP_EQ_WORDS]);
