#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "strip_motion.h"
typedef struct {
    bool initialized,alert_seen;uint32_t boot,source_ms,total_ms,count;
    uint64_t last_update,hold_until;
    double occupancy,envelope,phase,rainbow_phase;
    strip_motion_t motion;uint8_t palette;
} strip_visual_t;
typedef struct {uint8_t r,g,b;} strip_rgb_t;
void strip_visual_observe(strip_visual_t *s,uint64_t now,uint32_t boot,uint32_t source_ms,uint32_t total_ms,uint32_t count,bool lit);
void strip_visual_tick(strip_visual_t *s,uint64_t now,bool online);
strip_rgb_t strip_visual_pixel(const strip_visual_t *s,uint64_t now,unsigned pixel,unsigned length,unsigned mode,unsigned brightness,bool online);
