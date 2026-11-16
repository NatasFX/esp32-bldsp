#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MONITOR_SAMPLE_CAPACITY 300
#define MONITOR_EVENT_CAPACITY 64
#define MONITOR_RAW_CAPACITY 128
typedef struct {uint64_t at_ms;int16_t raw;} monitor_raw_t;
typedef struct {uint64_t at_ms;float temperature_c;int16_t ldr_mv;int8_t clip;bool temperature_valid;} monitor_sample_t;
typedef enum {MONITOR_STARTED,MONITOR_CLIP_ON,MONITOR_CLIP_OFF,MONITOR_NTC_VALID,MONITOR_NTC_INVALID,MONITOR_LDR_VALID,MONITOR_LDR_INVALID,MONITOR_CAL_OFF,MONITOR_CAL_ON,MONITOR_ADC_ERROR} monitor_event_code_t;
typedef struct {uint64_t at_ms;monitor_event_code_t code;int detail;} monitor_event_t;
typedef struct {
    monitor_sample_t samples[MONITOR_SAMPLE_CAPACITY];size_t sample_head,sample_count;
    monitor_event_t events[MONITOR_EVENT_CAPACITY];size_t event_head,event_count;
    monitor_raw_t raw[MONITOR_RAW_CAPACITY];size_t raw_head,raw_count;
} monitor_log_t;
void monitor_log_sample(monitor_log_t *log,monitor_sample_t sample);
void monitor_log_event(monitor_log_t *log,uint64_t at_ms,monitor_event_code_t code,int detail);
const monitor_sample_t *monitor_log_sample_at(const monitor_log_t *log,size_t index);
const monitor_event_t *monitor_log_event_at(const monitor_log_t *log,size_t index);
const char *monitor_event_name(monitor_event_code_t code);
void monitor_log_raw(monitor_log_t *log,uint64_t at_ms,int raw);
const monitor_raw_t *monitor_log_raw_at(const monitor_log_t *log,size_t index);
