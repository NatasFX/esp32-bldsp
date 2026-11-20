#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {uint32_t rail_since,clear_since;bool timing_rail,timing_clear,rail_high;} clip_health_t;
void clip_health_sample(clip_health_t *s,uint32_t ms,bool valid,int raw);
