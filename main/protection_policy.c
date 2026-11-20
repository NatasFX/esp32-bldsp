#include "protection_policy.h"
#include <string.h>
protection_config_t protection_defaults(void) {
    return (protection_config_t){.enabled=false,.recovery_enabled=true,.event_count=3,
        .window_ms=2000,.interval_ms=1000,.quiet_ms=8000,.recovery_interval_ms=2000,
        .step_q8=128,.max_q8=3072,.recovery_step_q8=64};
}
bool protection_config_valid(const protection_config_t *c) {
    return c && c->event_count>=2 && c->event_count<=PROTECTION_EVENT_CAPACITY
        && c->window_ms>=500 && c->window_ms<=10000 && c->interval_ms>=250 && c->interval_ms<=5000
        && c->quiet_ms>=2000 && c->quiet_ms<=60000 && c->recovery_interval_ms>=500 && c->recovery_interval_ms<=10000
        && c->step_q8>=26 && c->step_q8<=768 && c->max_q8>=128 && c->max_q8<=6144
        && c->max_q8>=c->step_q8 && c->recovery_step_q8>=26 && c->recovery_step_q8<=256;
}
void protection_policy_init(protection_policy_t *s, protection_config_t config) {
    memset(s,0,sizeof(*s));s->config=config;
}
void protection_policy_configure(protection_policy_t *s, protection_config_t config) {
    s->config=config;s->event_size=0;
    // Disabling or lowering the configured cap never raises the volume abruptly.
}
void protection_policy_restore(protection_policy_t *s) {s->restoring=true;}
unsigned protection_policy_tick(protection_policy_t *s, uint64_t now, bool ready, uint32_t count, bool sustained) {
    if(!s->initialized){s->last_count=count;s->last_clip=now;s->initialized=true;}
    uint32_t delta=count-s->last_count;s->last_count=count;
    if(!ready){s->last_clip=now;s->event_size=0;return s->attenuation_q8;}
    unsigned keep=0;
    for(unsigned i=0;i<s->event_size;i++)if(now-s->events[i]<=s->config.window_ms)s->events[keep++]=s->events[i];
    s->event_size=keep;
    if(delta||sustained){
        s->last_clip=now;s->restoring=false;
        unsigned add=delta>PROTECTION_EVENT_CAPACITY?PROTECTION_EVENT_CAPACITY:delta;
        while(add--){
            if(s->event_size==PROTECTION_EVENT_CAPACITY){memmove(s->events,s->events+1,(PROTECTION_EVENT_CAPACITY-1)*sizeof(s->events[0]));s->event_size--;}
            s->events[s->event_size++]=now;
        }
    }
    bool repeated=s->event_size>=s->config.event_count;
    if(s->config.enabled&&(repeated||sustained)){
        if(!s->last_step||now-s->last_step>=s->config.interval_ms){
            unsigned target=s->attenuation_q8+s->config.step_q8;
            if(target>s->config.max_q8)target=s->config.max_q8;
            if(target>s->attenuation_q8)s->attenuation_q8=target;
            s->last_step=now;s->event_size=0;
        }
    }else if(!s->recovery_inhibited&&s->attenuation_q8&&((s->config.recovery_enabled&&s->config.enabled)||s->restoring)
             &&now-s->last_clip>=s->config.quiet_ms&&now-s->last_recovery>=s->config.recovery_interval_ms){
        s->attenuation_q8=s->attenuation_q8>s->config.recovery_step_q8?s->attenuation_q8-s->config.recovery_step_q8:0;
        s->last_recovery=now;if(!s->attenuation_q8)s->restoring=false;
    }
    return s->attenuation_q8;
}
