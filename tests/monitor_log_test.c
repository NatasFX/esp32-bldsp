#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "monitor_log.h"
static monitor_log_t log_data;
int main(void){
    assert(sizeof(log_data)<16384);
    assert(!monitor_log_sample_at(&log_data,0));assert(!monitor_log_event_at(&log_data,0));
    for(unsigned i=0;i<MONITOR_SAMPLE_CAPACITY+10;i++){
        monitor_sample_t sample={.at_ms=50000000000ULL+i*1000ULL,.temperature_c=(float)i,.temperature_valid=i!=15,.clip=-1,.ldr_mv=-1};
        monitor_log_sample(&log_data,sample);
    }
    assert(log_data.sample_count==MONITOR_SAMPLE_CAPACITY);
    for(size_t i=0;i<log_data.sample_count;i++){
        const monitor_sample_t *s=monitor_log_sample_at(&log_data,i);
        assert(s->at_ms==50000000000ULL+(i+10)*1000ULL);assert(s->temperature_c==(float)(i+10));
    }
    assert(!monitor_log_sample_at(&log_data,5)->temperature_valid);
    assert(!monitor_log_sample_at(&log_data,MONITOR_SAMPLE_CAPACITY));
    for(unsigned i=0;i<MONITOR_EVENT_CAPACITY+6;i++)monitor_log_event(&log_data,i*1000,MONITOR_CLIP_ON,(int)i);
    assert(log_data.event_count==MONITOR_EVENT_CAPACITY);
    for(size_t i=0;i<log_data.event_count;i++)assert(monitor_log_event_at(&log_data,i)->detail==(int)(i+6));
    assert(!monitor_log_event_at(&log_data,MONITOR_EVENT_CAPACITY));
    assert(!strcmp(monitor_event_name(MONITOR_CLIP_ON),"clip_on"));
    assert(!strcmp(monitor_event_name((monitor_event_code_t)999),"unknown"));
    for(unsigned i=0;i<MONITOR_RAW_CAPACITY+12;i++)monitor_log_raw(&log_data,i*20,i==20?-1:(int)i);
    assert(log_data.raw_count==MONITOR_RAW_CAPACITY);
    for(size_t i=0;i<log_data.raw_count;i++)assert(monitor_log_raw_at(&log_data,i)->at_ms==(i+12)*20);
    assert(monitor_log_raw_at(&log_data,8)->raw==-1);
    assert(!monitor_log_raw_at(&log_data,MONITOR_RAW_CAPACITY));
    puts("Monitoring log: bounded RAM, chronological wraparound, gaps and independent event window OK");return 0;
}
