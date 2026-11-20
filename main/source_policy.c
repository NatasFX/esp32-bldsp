#include "source_policy.h"
void source_policy_connected(source_policy_t *p,uint16_t pid,int64_t now) {
    p->present=true;p->pid=pid;p->connected_at=now;
    if(p->cycle_wait_usb && pid==SOURCE_USB_AUDIO_PID){p->cycle_wait_usb=false;p->attempts=0;p->next_attempt=0;}
    if(pid==SOURCE_BLUETOOTH_PID){p->attempts=0;p->next_attempt=0;}
}
void source_policy_disconnected(source_policy_t *p){p->present=false;}
bool source_policy_target(const source_policy_t *p){return p->present && p->pid==SOURCE_BLUETOOTH_PID && !p->cycle_wait_usb;}
bool source_policy_due(const source_policy_t *p,int64_t now) {
    return !p->cycle_wait_usb && p->present && p->pid==SOURCE_USB_AUDIO_PID && p->attempts<SOURCE_MAX_ATTEMPTS &&
        now-p->connected_at>=1000000 && now>=p->next_attempt;
}
void source_policy_pulsed(source_policy_t *p,int64_t now){p->attempts++;p->next_attempt=now+5000000;}

bool source_policy_begin_cycle(source_policy_t *p,int64_t now){
    if(!source_policy_target(p))return false;
    p->cycle_wait_usb=true;p->cycle_deadline=now+15000000;return true;
}
