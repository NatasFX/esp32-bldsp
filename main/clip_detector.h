#pragma once
#include <stdbool.h>
#include <stdint.h>

#define CLIP_WINDOW_SAMPLES 3001
#define CLIP_WINDOW_MS 60000u
#define CLIP_EDGE_MS 60u
#define CLIP_MAX_GAP_MS 500u
#define CLIP_DROP_PERCENT 20
#define CLIP_RELEASE_DROP_PERCENT 10
#define CLIP_MIN_LOW_SAMPLES 2
#define CLIP_SUSTAINED_SAMPLES 31

typedef struct {
    uint32_t window_ms, edge_ms, drop_percent, release_drop_percent;
    uint32_t min_low_samples, sustained_samples, hard_min_samples;
} clip_config_t;
clip_config_t clip_config_default(void);
bool clip_config_valid(const clip_config_t *c);

// Timestamped 60s window at up to 50 Hz; no heap allocation in the sampler.
typedef struct {
    clip_config_t config;
    uint32_t times[CLIP_WINDOW_SAMPLES];
    uint16_t raw[CLIP_WINDOW_SAMPLES];
    bool reference_sample[CLIP_WINDOW_SAMPLES];
    unsigned reference_count;
    uint32_t sum, started, last, last_low;
    unsigned head, count, low_run, max_low_run;
    bool initialized, ready, active, candidate, pulse_qualified, long_confirmed, hard_pulse;
    double baseline, trigger, release, edge_drop, pulse_baseline;
} clip_detector_t;

void clip_detector_reset(clip_detector_t *s);
void clip_detector_sample(clip_detector_t *s, uint32_t ms, bool valid, int raw);

void clip_detector_configure(clip_detector_t *s, const clip_config_t *config);
