#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint64_t bins[6], first_hard, last_hard, last_severe;
    bool occupied[6], series, severe_seen, flash;
} clip_alert_t;
void clip_alert_sample(clip_alert_t *s,uint64_t ms,bool ready,bool hard_pulse,bool hard_sustained);
