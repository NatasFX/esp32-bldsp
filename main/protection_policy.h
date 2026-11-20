#pragma once
#include <stdbool.h>
#include <stdint.h>
#define PROTECTION_EVENT_CAPACITY 20

typedef struct {
    bool enabled, recovery_enabled;
    unsigned event_count, window_ms, interval_ms, quiet_ms, recovery_interval_ms;
    unsigned step_q8, max_q8, recovery_step_q8;
} protection_config_t;
typedef struct {
    protection_config_t config;
    uint64_t events[PROTECTION_EVENT_CAPACITY], last_clip, last_step, last_recovery;
    unsigned event_size, attenuation_q8;
    uint32_t last_count;
    bool initialized, restoring, recovery_inhibited;
} protection_policy_t;
protection_config_t protection_defaults(void);
bool protection_config_valid(const protection_config_t *c);
void protection_policy_init(protection_policy_t *s, protection_config_t config);
void protection_policy_configure(protection_policy_t *s, protection_config_t config);
void protection_policy_restore(protection_policy_t *s);
unsigned protection_policy_tick(protection_policy_t *s, uint64_t now_ms, bool ready, uint32_t count, bool sustained);
