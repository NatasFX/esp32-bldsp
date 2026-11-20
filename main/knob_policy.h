#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {uint8_t module,selector;int16_t min_q8,max_q8;} knob_binding_t;
typedef struct {bool seen,picked;uint16_t previous;int16_t observed,last_sent;} knob_state_t;
bool knob_binding_valid(const knob_binding_t *b);
uint16_t knob_control_position(uint16_t adc_position);
int16_t knob_gain(const knob_binding_t *b,uint16_t position);
bool knob_update(knob_state_t *s,const knob_binding_t *b,uint16_t position,int16_t current,int16_t *target);

// New link session: accept physical position immediately, preserving retry state.
bool knob_claim(knob_state_t *s,const knob_binding_t *b,uint16_t position,int16_t current,int16_t *target);
