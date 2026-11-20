#include "clip_alert.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 clip_alert_t s={0};
 for(unsigned t=0;t<10000;t+=20){clip_alert_sample(&s,t,true,false,false);assert(!s.flash);}
 s=(clip_alert_t){0};clip_alert_sample(&s,1000,true,true,false);
 for(unsigned t=1020;t<10000;t+=20){clip_alert_sample(&s,t,true,false,false);assert(!s.flash);}
 s=(clip_alert_t){0};
 for(unsigned t=1000;t<=6000;t+=20){clip_alert_sample(&s,t,true,t%1000==0,false);if(t<6000)assert(!s.flash);}
 assert(s.flash);clip_alert_sample(&s,20000,true,false,false);assert(!s.flash);
 s=(clip_alert_t){0};
 for(unsigned t=0;t<=5000;t+=20)clip_alert_sample(&s,t,true,false,true);
 assert(s.flash);clip_alert_sample(&s,5020,false,false,false);assert(!s.flash);
 s=(clip_alert_t){0};
 for(unsigned t=1000;t<=7000;t+=20){clip_alert_sample(&s,t,true,t%1000==0&&t!=3000,false);assert(!s.flash);}
 puts("Clip alert: isolated pulses stay pill-only; five completed hard-clip seconds, sustained input, gaps and expiry passed");
}
