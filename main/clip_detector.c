#include "clip_detector.h"
#include <string.h>

#define TRIGGER_RATIO (1.0 - CLIP_DROP_PERCENT / 100.0)
#define RELEASE_RATIO (1.0 - CLIP_RELEASE_DROP_PERCENT / 100.0)
#define EDGE_DROP_RATIO (CLIP_DROP_PERCENT / 100.0)

typedef struct {
    int peak, minimum;
    bool available;
} recent_levels_t;

void clip_detector_reset(clip_detector_t *s) {
    memset(s, 0, sizeof(*s));
}

static void update_reference(clip_detector_t *s, uint32_t ms, int raw) {
    while (s->count && (uint32_t)(ms - s->times[s->head]) > CLIP_WINDOW_MS) {
        s->sum -= s->raw[s->head];
        s->head = (s->head + 1) % CLIP_WINDOW_SAMPLES;
        s->count--;
    }
    // Current sample is added after classification, not to its own reference.
    s->baseline = s->count ? (double)s->sum / s->count : raw;
    s->trigger = s->baseline * TRIGGER_RATIO;
    s->release = s->baseline * RELEASE_RATIO;
    s->ready = (uint32_t)(ms - s->started) >= CLIP_WINDOW_MS
        && s->count > 0 && s->baseline >= 1;
}

static recent_levels_t recent_levels(const clip_detector_t *s, uint32_t ms, int raw) {
    recent_levels_t levels = {.peak = raw, .minimum = raw};
    for (unsigned j = 0; j < s->count; j++) {
        unsigned i = (s->head + s->count - 1 - j) % CLIP_WINDOW_SAMPLES;
        if ((uint32_t)(ms - s->times[i]) > CLIP_EDGE_MS) break;
        if (s->raw[i] > levels.peak) levels.peak = s->raw[i];
        if (s->raw[i] < levels.minimum) levels.minimum = s->raw[i];
        levels.available = true;
    }
    return levels;
}

static void classify_pulse(clip_detector_t *s, uint32_t ms, int raw, recent_levels_t levels) {
    s->edge_drop = (double)levels.peak - raw;
    s->active = false;
    if (!s->ready) {
        s->candidate = false;
        s->long_confirmed = false;
        s->low_run = 0;
        return;
    }
    if (!s->candidate) {
        bool falling = levels.available && levels.peak >= s->release
            && s->edge_drop > s->baseline * EDGE_DROP_RATIO;
        if (raw < s->trigger && falling) {
            s->candidate = true;
            s->pulse_baseline = s->baseline;
            s->last_low = ms;
            s->low_run = 1;
            s->pulse_qualified = false;
            s->long_confirmed = false;
        }
        return;
    }

    // Freeze only this pulse's reference; the independent long mean keeps moving.
    double base = s->pulse_baseline;
    s->trigger = base * TRIGGER_RATIO;
    s->release = base * RELEASE_RATIO;
    if (raw < s->trigger) {
        s->low_run++;
        s->last_low = ms;
        if (s->low_run >= CLIP_MIN_LOW_SAMPLES) s->pulse_qualified = true;
        if (s->low_run >= CLIP_SUSTAINED_SAMPLES) s->long_confirmed = true;
        s->active = s->long_confirmed;
    } else if (raw >= s->release) {
        bool rising = levels.available && (double)(raw - levels.minimum) > base * EDGE_DROP_RATIO
            && (uint32_t)(ms - s->last_low) <= CLIP_EDGE_MS;
        // Sustained CLIP was already counted; never count its trailing edge again.
        s->active = !s->long_confirmed && s->pulse_qualified && rising;
        s->candidate = false;
        s->long_confirmed = false;
        s->low_run = 0;
    } else {
        s->low_run = 0;
        s->active = s->long_confirmed;
        if (!s->long_confirmed && (uint32_t)(ms - s->last_low) > CLIP_EDGE_MS) {
            s->candidate = false;
        }
    }
}

static void append_sample(clip_detector_t *s, uint32_t ms, int raw) {
    if (s->count == CLIP_WINDOW_SAMPLES) {
        s->sum -= s->raw[s->head];
        s->head = (s->head + 1) % CLIP_WINDOW_SAMPLES;
        s->count--;
    }
    unsigned tail = (s->head + s->count) % CLIP_WINDOW_SAMPLES;
    s->times[tail] = ms;
    s->raw[tail] = (uint16_t)raw;
    s->sum += (uint32_t)raw;
    s->count++;
}

void clip_detector_sample(clip_detector_t *s, uint32_t ms, bool valid, int raw) {
    if (!valid || raw < 0 || raw > 4095) {
        clip_detector_reset(s);
        return;
    }
    if (s->initialized && (uint32_t)(ms - s->last) > CLIP_MAX_GAP_MS) {
        clip_detector_reset(s);
    }
    if (!s->initialized) {
        s->initialized = true;
        s->started = ms;
    }
    s->last = ms;
    update_reference(s, ms, raw);
    classify_pulse(s, ms, raw, recent_levels(s, ms, raw));
    append_sample(s, ms, raw);
}
