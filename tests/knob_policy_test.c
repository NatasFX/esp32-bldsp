#include "knob_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 knob_binding_t volume={0x93,1,-60*256,0},band={0x91,7,-12*256,12*256};
 assert(knob_control_position(0)==65535&&knob_control_position(65535)==0);
 assert(knob_gain(&volume,knob_control_position(0))==0);
 assert(knob_gain(&volume,knob_control_position(65535))==-60*256);
 assert(knob_gain(&band,knob_control_position(0))==12*256);
 assert(knob_gain(&band,knob_control_position(65535))==-12*256);
 assert(knob_control_position(196)==65535); // Physical ceiling observed at 99.7%.
 assert(knob_control_position(65535-196)==0);
 assert(knob_control_position(328)==65535);
 assert(knob_control_position(329)==65535-329);
 assert(knob_control_position(65535-328)==0);
 assert(knob_control_position(65535-329)==329);
 assert(knob_gain(&band,knob_control_position(196))==band.max_q8);
 assert(knob_gain(&volume,knob_control_position(65535-196))==volume.min_q8);
 for(unsigned adc=1;adc<=65535;adc++)assert(knob_control_position(adc)<=knob_control_position(adc-1));
 assert(knob_binding_valid(&volume)&&knob_binding_valid(&band));
 assert(knob_gain(&volume,0)==-60*256&&knob_gain(&volume,65535)==0);
 assert(knob_gain(&band,32768)==0);
 knob_binding_t invalid=band;invalid.selector=6;assert(!knob_binding_valid(&invalid));invalid=volume;invalid.max_q8=256;assert(!knob_binding_valid(&invalid));
 knob_state_t s={0};int16_t target=0;
 assert(!knob_update(&s,&volume,0,0,&target)&&!s.picked);
 assert(!knob_update(&s,&volume,30000,0,&target)&&!s.picked);
 assert(!knob_update(&s,&volume,65535,0,&target)&&s.picked);
 assert(knob_update(&s,&volume,60000,0,&target)&&target<0);
 s.observed=target;s.last_sent=target;int16_t applied=target;
 assert(!knob_update(&s,&volume,60000,applied,&target));
 // A phone/preset change releases ownership; unchanged knob cannot undo it.
 assert(!knob_update(&s,&volume,60000,-20*256,&target)&&!s.picked);
 assert(!knob_update(&s,&volume,40000,-20*256,&target)&&s.picked);
 assert(knob_update(&s,&volume,45000,-20*256,&target));
 memset(&s,0,sizeof(s));assert(!knob_update(&s,&band,32768,0,&target)&&s.picked);
 assert(knob_update(&s,&band,40000,0,&target)&&target>0);
 // Startup/reconnection takes the physical value even when stationary and far from preset.
 memset(&s,0,sizeof(s));assert(knob_claim(&s,&volume,0,0,&target)&&target==-60*256&&s.picked);
 // USB failure: unchanged input must still request the target on retry.
 assert(knob_claim(&s,&volume,0,0,&target)&&target==-60*256);
 s.observed=target;s.last_sent=target;
 assert(!knob_update(&s,&volume,0,target,&target));
 // Disconnect leaves the DSP untouched (no calls). Reconnection wins over the retained gain.
 memset(&s,0,sizeof(s));assert(knob_claim(&s,&volume,65535,-60*256,&target)&&target==0);
 assert(!knob_claim(&s,&volume,65535,0,&target)&&s.picked);
 // A later phone edit still releases ownership until physical pickup.
 assert(!knob_update(&s,&volume,65535,-20*256,&target)&&!s.picked);
 puts("Knobs: gain-only bindings, bounds, zero detent, startup pickup, phone takeover and crossing passed");
 return 0;
}
