#include "clip_detector.h"
#include <string.h>

#define TRIGGER_RATIO (1.0 - s->config.drop_percent / 100.0)
#define RELEASE_RATIO (1.0 - s->config.release_drop_percent / 100.0)
#define EDGE_DROP_RATIO (s->config.drop_percent / 100.0)
clip_config_t clip_config_default(void) {
    return (clip_config_t){60000,60,20,10,2,31,6};
}
bool clip_config_valid(const clip_config_t *c) {
    return c && c->window_ms>=5000 && c->window_ms<=60000
        && c->edge_ms>=20 && c->edge_ms<=500
        && c->drop_percent>=1 && c->drop_percent<=80
        && c->release_drop_percent<c->drop_percent
        && c->min_low_samples>=1 && c->min_low_samples<=25
        && c->sustained_samples>=c->min_low_samples && c->sustained_samples<=250
        && c->hard_min_samples>=1 && c->hard_min_samples<=250;
}
void clip_detector_configure(clip_detector_t *s,const clip_config_t *c) {
    if (!clip_config_valid(c)) return;
    clip_config_t copy=*c;memset(s,0,sizeof(*s));s->config=copy;
}
typedef struct {
    int peak, minimum;
    bool available;
} recent_levels_t;

void clip_detector_reset(clip_detector_t *s) {
    clip_config_t c=clip_config_default();clip_detector_configure(s,&c);
}

static void update_reference(clip_detector_t *s, uint32_t ms, int raw) {
    while (s->count && (uint32_t)(ms - s->times[s->head]) > s->config.window_ms) {
        if(s->reference_sample[s->head]){s->sum -= s->raw[s->head];s->reference_count--;}
        s->head = (s->head + 1) % CLIP_WINDOW_SAMPLES;
        s->count--;
    }
    // Current sample is added after classification, not to its own reference.
    // Never learn the light from a candidate/confirmed CLIP. If a sustained
    // event outlives the window, retain the last clean reference and readiness.
    if (!s->candidate && s->reference_count)
        s->baseline = (double)s->sum / s->reference_count;
    else if (!s->initialized || s->baseline < 1) s->baseline = raw;
    s->trigger = s->baseline * TRIGGER_RATIO;
    s->release = s->baseline * RELEASE_RATIO;
    s->ready = (uint32_t)(ms - s->started) >= s->config.window_ms
        && (s->reference_count > 0 || s->ready) && s->baseline >= 1;
}

static recent_levels_t recent_levels(const clip_detector_t *s, uint32_t ms, int raw) {
    recent_levels_t levels = {.peak = raw, .minimum = raw};
    for (unsigned j = 0; j < s->count; j++) {
        unsigned i = (s->head + s->count - 1 - j) % CLIP_WINDOW_SAMPLES;
        if ((uint32_t)(ms - s->times[i]) > s->config.edge_ms) break;
        if (s->raw[i] > levels.peak) levels.peak = s->raw[i];
        if (s->raw[i] < levels.minimum) levels.minimum = s->raw[i];
        levels.available = true;
    }
    return levels;
}

static void classify_pulse(clip_detector_t *s, uint32_t ms, int raw, recent_levels_t levels) {
    s->edge_drop=(double)levels.peak-raw;
    s->active=false;s->hard_pulse=false;
    if(!s->ready){s->candidate=false;s->long_confirmed=false;s->low_run=0;return;}
    if(!s->candidate){
        bool falling=levels.available && levels.peak>=s->release
            && s->edge_drop>s->baseline*EDGE_DROP_RATIO;
        if(raw>=s->trigger || !falling)return;
        s->candidate=true;s->pulse_baseline=s->baseline;s->low_run=0;
        s->max_low_run=0;s->pulse_qualified=false;s->long_confirmed=false;
    }
    // Fast falling edge opens the pulse; LDR recovery may be arbitrarily slow.
    // Confirm as soon as enough low samples arrive, with one event per episode.
    double base=s->pulse_baseline;
    s->trigger=base*TRIGGER_RATIO;s->release=base*RELEASE_RATIO;
    if(raw>=s->release){
        s->candidate=false;s->long_confirmed=false;s->pulse_qualified=false;
        s->low_run=0;return;
    }
    if(raw<s->trigger){
        unsigned previous_max=s->max_low_run;
        s->low_run++;s->last_low=ms;
        if(s->low_run>s->max_low_run)s->max_low_run=s->low_run;
        if(s->low_run>=s->config.min_low_samples)s->pulse_qualified=true;
        if(s->low_run>=s->config.sustained_samples)s->long_confirmed=true;
        s->hard_pulse=s->pulse_qualified && previous_max<s->config.hard_min_samples
            && s->max_low_run>=s->config.hard_min_samples;
    }else{
        s->low_run=0;
        // Only unconfirmed candidates can time out between trigger and release.
        if(!s->pulse_qualified && (uint32_t)(ms-s->last_low)>s->config.edge_ms)
            s->candidate=false;
    }
    s->active=s->candidate && s->pulse_qualified;
}

static void append_sample(clip_detector_t *s, uint32_t ms, int raw, bool clean) {
    if (s->count == CLIP_WINDOW_SAMPLES) {
        if(s->reference_sample[s->head]){s->sum -= s->raw[s->head];s->reference_count--;}
        s->head = (s->head + 1) % CLIP_WINDOW_SAMPLES;
        s->count--;
    }
    unsigned tail = (s->head + s->count) % CLIP_WINDOW_SAMPLES;
    s->times[tail] = ms;
    s->raw[tail] = (uint16_t)raw;
    s->reference_sample[tail] = clean;
    if(clean){s->sum += (uint32_t)raw;s->reference_count++;}
    s->count++;
}

void clip_detector_sample(clip_detector_t *s, uint32_t ms, bool valid, int raw) {
    if (!clip_config_valid(&s->config)) clip_detector_reset(s);
    if (!valid || raw < 0 || raw > 4095) {
        clip_config_t c=s->config;clip_detector_configure(s,&c);
        return;
    }
    if (s->initialized && (uint32_t)(ms - s->last) > CLIP_MAX_GAP_MS) {
        clip_config_t c=s->config;clip_detector_configure(s,&c);
    }
    if (!s->initialized) {
        s->initialized = true;
        s->started = ms;
    }
    s->last = ms;
    update_reference(s, ms, raw);
    bool was_candidate=s->candidate;
    classify_pulse(s, ms, raw, recent_levels(s, ms, raw));
    // The edge ring retains all samples; only clean samples feed the mean.
    // Exclude from the FIRST falling sample, including rejected candidates and
    // the recovery sample, so confirmation never arrives too late to exclude it.
    append_sample(s, ms, raw, !was_candidate && !s->candidate && !s->active);
}
