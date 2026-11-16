#pragma once
#include <stdbool.h>
#include <stdint.h>
#define SOURCE_USB_AUDIO_PID 0x1719
#define SOURCE_BLUETOOTH_PID 0x171e
#define SOURCE_MAX_ATTEMPTS 3
typedef struct {
    bool present;
    uint16_t pid;
    unsigned attempts;
    int64_t connected_at, next_attempt;
} source_policy_t;
void source_policy_connected(source_policy_t *p,uint16_t pid,int64_t now);
void source_policy_disconnected(source_policy_t *p);
bool source_policy_target(const source_policy_t *p);
bool source_policy_due(const source_policy_t *p,int64_t now);
void source_policy_pulsed(source_policy_t *p,int64_t now);
