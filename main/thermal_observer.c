#include "thermal_observer.h"
#include "thermal_parameters.h"
#include <math.h>
#include <string.h>

#define OBSERVER_FILTER_S 12.0
#define OBSERVER_MAX_GAP_MS 2500u
#define OBSERVER_MAX_COIL_C 500.0

void thermal_observer_init(thermal_observer_t *s, double ambient) {
    memset(s, 0, sizeof(*s));
    s->ambient = ambient;
    s->coil = NAN;
}

bool thermal_observer_sample(thermal_observer_t *s, uint64_t ms, bool valid, double ntc) {
    if (!isfinite(s->ambient) || !valid || !isfinite(ntc) || ntc < -20 || ntc > 180) {
        thermal_observer_init(s, s->ambient);
        return false;
    }
    if (s->initialized && (ms <= s->last_ms || ms - s->last_ms > OBSERVER_MAX_GAP_MS)) {
        thermal_observer_init(s, s->ambient);
    }

    s->ring[s->head] = ntc;
    s->head = (s->head + 1) % THERMAL_OBSERVER_SAMPLES;
    if (s->count < THERMAL_OBSERVER_SAMPLES) s->count++;
    s->mean = 0;
    for (unsigned i = 0; i < s->count; i++) s->mean += s->ring[i];
    s->mean /= s->count;

    const double wc = THERMAL_SENSOR_COIL_WEIGHT;
    const double tau = THERMAL_SENSOR_TAU_S;
    const double k = THERMAL_COIL_TO_BODY_PER_S / wc;
    const double lambda = THERMAL_BODY_LOSS_PER_S + k;
    const double n = s->mean - s->ambient;
    if (!s->initialized) {
        s->filtered = n;
        s->z = (1 - wc - k * tau) * n;
        s->coil = ntc;
        s->initialized = true;
    } else {
        double dt = (ms - s->last_ms) / 1000.;
        double previous = s->filtered;
        // Rolling mean plus low pass controls inverse-derivative noise.
        s->filtered += -expm1(-dt / OBSERVER_FILTER_S) * (n - s->filtered);
        double slope = (s->filtered - previous) / dt;
        // z=H-k*tau*n removes differentiation from hidden-state integration.
        // lambda>0: exact exponential update dissipates initial state errors.
        double target = k * (1 - lambda * tau) * s->filtered / lambda;
        s->z = target + (s->z - target) * exp(-lambda * dt);
        double x = ((1 - k * tau) * s->filtered + tau * slope - s->z) / wc;
        s->coil = s->ambient + fmax(0, x);
    }
    s->last_ms = ms;
    s->valid = s->count == THERMAL_OBSERVER_SAMPLES && isfinite(s->coil)
        && s->coil <= OBSERVER_MAX_COIL_C;
    return s->valid;
}
