#include "protection_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 protection_config_t c=protection_defaults();assert(protection_config_valid(&c));
 protection_policy_t s;protection_policy_init(&s,c);
 protection_policy_tick(&s,1000,true,0,false);
 assert(protection_policy_tick(&s,1100,true,3,true)==0); // opt-in only
 c.enabled=true;protection_policy_configure(&s,c);
 assert(protection_policy_tick(&s,2000,true,4,false)==0);
 assert(protection_policy_tick(&s,2200,true,5,false)==0);
 assert(protection_policy_tick(&s,2400,true,6,false)==128);
 assert(protection_policy_tick(&s,2500,true,7,true)==128); // cooldown
 assert(protection_policy_tick(&s,3400,true,7,true)==256);
 for(unsigned i=0;i<100;i++)protection_policy_tick(&s,4400+i*1000,true,7,true);
 assert(s.attenuation_q8==c.max_q8);
 c.enabled=false;protection_policy_configure(&s,c);
 assert(protection_policy_tick(&s,120000,true,7,false)==c.max_q8); // no volume jump
 protection_policy_restore(&s);
 assert(protection_policy_tick(&s,122000,false,7,false)==c.max_q8); // loss of sensor cannot raise volume
 assert(protection_policy_tick(&s,129999,true,7,false)==c.max_q8);
 assert(protection_policy_tick(&s,130000,true,7,false)==c.max_q8-64);
 protection_policy_tick(&s,130100,true,8,false);assert(!s.restoring);
 assert(protection_policy_tick(&s,150000,true,8,false)==c.max_q8-64);
 c.recovery_enabled=true;c.enabled=true;protection_policy_configure(&s,c);
 for(unsigned i=0;i<100;i++)protection_policy_tick(&s,160000+i*2000,true,8,false);
 assert(s.attenuation_q8==0);
 protection_policy_init(&s,c);protection_policy_tick(&s,1000,true,0,false);
 protection_policy_tick(&s,2000,true,1,false);protection_policy_tick(&s,5001,true,2,false);
 assert(protection_policy_tick(&s,6000,true,3,false)==0); // expired first event
 c.event_count=0;assert(!protection_config_valid(&c));c=protection_defaults();c.step_q8=999;assert(!protection_config_valid(&c));
 puts("Protection: opt-in, repeated/sustained clip, cooldown, cap, held reduction, quiet recovery and sensor loss passed");
}
