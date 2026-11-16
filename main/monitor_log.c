#include "monitor_log.h"
void monitor_log_raw(monitor_log_t *log,uint64_t at_ms,int raw){
    log->raw[log->raw_head]=(monitor_raw_t){at_ms,(int16_t)raw};log->raw_head=(log->raw_head+1)%MONITOR_RAW_CAPACITY;
    if(log->raw_count<MONITOR_RAW_CAPACITY)log->raw_count++;
}
const monitor_raw_t *monitor_log_raw_at(const monitor_log_t *log,size_t index){
    if(index>=log->raw_count)return NULL;
    return &log->raw[(log->raw_head+MONITOR_RAW_CAPACITY-log->raw_count+index)%MONITOR_RAW_CAPACITY];
}
void monitor_log_sample(monitor_log_t *log,monitor_sample_t sample){
    log->samples[log->sample_head]=sample;log->sample_head=(log->sample_head+1)%MONITOR_SAMPLE_CAPACITY;
    if(log->sample_count<MONITOR_SAMPLE_CAPACITY)log->sample_count++;
}
void monitor_log_event(monitor_log_t *log,uint64_t at_ms,monitor_event_code_t code,int detail){
    log->events[log->event_head]=(monitor_event_t){at_ms,code,detail};log->event_head=(log->event_head+1)%MONITOR_EVENT_CAPACITY;
    if(log->event_count<MONITOR_EVENT_CAPACITY)log->event_count++;
}
const monitor_sample_t *monitor_log_sample_at(const monitor_log_t *log,size_t index){
    if(index>=log->sample_count)return NULL;
    return &log->samples[(log->sample_head+MONITOR_SAMPLE_CAPACITY-log->sample_count+index)%MONITOR_SAMPLE_CAPACITY];
}
const monitor_event_t *monitor_log_event_at(const monitor_log_t *log,size_t index){
    if(index>=log->event_count)return NULL;
    return &log->events[(log->event_head+MONITOR_EVENT_CAPACITY-log->event_count+index)%MONITOR_EVENT_CAPACITY];
}
const char *monitor_event_name(monitor_event_code_t code){
    static const char *names[]={"started","clip_on","clip_off","ntc_valid","ntc_invalid","ldr_valid","ldr_invalid","cal_off","cal_on","adc_error"};
    return code>=MONITOR_STARTED&&code<=MONITOR_ADC_ERROR?names[code]:"unknown";
}
