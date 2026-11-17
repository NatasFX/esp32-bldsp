#pragma once
#include <stdbool.h>
#include <stdint.h>

#define THERMAL_OBSERVER_SAMPLES 10

// Stable inverse of joint_v2 from NTC only. No voltage/power assumption.
typedef struct {
    double ambient, ring[THERMAL_OBSERVER_SAMPLES], mean, filtered, z, coil;
    unsigned count, head;
    uint64_t last_ms;
    bool initialized, valid;
} thermal_observer_t;

void thermal_observer_init(thermal_observer_t *s, double ambient);
bool thermal_observer_sample(thermal_observer_t *s, uint64_t ms, bool valid, double ntc);
