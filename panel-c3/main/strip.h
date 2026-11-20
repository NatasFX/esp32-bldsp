#pragma once
#include "panel_protocol.h"
void panel_strip_start(void);
void panel_strip_receive(const panel_reply_t *reply);

void panel_strip_positions(const uint16_t adc_position[5],uint8_t valid);
