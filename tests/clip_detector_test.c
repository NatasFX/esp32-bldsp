#include "clip_detector.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static clip_detector_t s;
static void warm(uint32_t origin,int raw){clip_detector_reset(&s);for(unsigned i=0;i<3000;i++){clip_detector_sample(&s,origin+i*20,true,raw);assert(!s.ready);}}
int main(void){
 // Short pulse: at least two low samples, count only upon sharp recovery.
 warm(0,4000);clip_detector_sample(&s,60000,true,2000);assert(s.candidate&&!s.active);
 clip_detector_sample(&s,60020,true,2100);assert(!s.active&&s.pulse_qualified);
 clip_detector_sample(&s,60040,true,4000);assert(s.active&&!s.candidate);
 clip_detector_sample(&s,60060,true,4000);assert(!s.active);
 // One-sample dip is rejected.
 warm(0,4000);clip_detector_sample(&s,60000,true,1900);clip_detector_sample(&s,60020,true,4000);assert(!s.active);
 // Exactly 20 percent is not enough; reference excludes the current sample.
 warm(0,4000);clip_detector_sample(&s,60000,true,3200);assert(!s.candidate&&fabs(s.baseline-4000)<.001);
 // More than 30 consecutive low samples confirms a sustained event at #31.
 warm(0,4000);
 for(unsigned i=0;i<30;i++){clip_detector_sample(&s,60000+i*20,true,2000);assert(!s.active);}
 clip_detector_sample(&s,60600,true,2000);assert(s.active&&s.long_confirmed);
 clip_detector_sample(&s,60620,true,2000);assert(s.active);
 clip_detector_sample(&s,60640,true,4000);assert(!s.active&&!s.candidate);
 // Slow flashlight movement cannot start a candidate, even held low for >30 samples.
 warm(0,4000);
 for(uint32_t t=60000;t<=64000;t+=20){clip_detector_sample(&s,t,true,4000-(int)((t-60000)/2));assert(!s.active&&!s.candidate);}
 for(uint32_t t=64020;t<=70000;t+=20){clip_detector_sample(&s,t,true,2000);assert(!s.active);}
 // Sharp fall followed by a slow rise is not a validated short pulse.
 warm(0,4000);clip_detector_sample(&s,60000,true,2000);clip_detector_sample(&s,60020,true,2000);
 for(unsigned i=1;i<=20;i++){clip_detector_sample(&s,60020+i*20,true,2000+(int)i*100);assert(!s.active);}
 // A data gap cannot be interpreted as an edge; invalid ADC resets warmup.
 warm(0,4000);clip_detector_sample(&s,60100,true,2000);assert(!s.candidate&&!s.active);
 clip_detector_sample(&s,60120,false,0);assert(!s.ready&&s.count==0);
 // Timestamp rollover and reference expiry.
 warm(0xffff0000u,3200);clip_detector_sample(&s,0xffff0000u+60000,true,1800);assert(s.candidate&&!s.active);
 clip_detector_sample(&s,0xffff0000u+60020,true,1800);clip_detector_sample(&s,0xffff0000u+60040,true,3200);assert(s.active);
 clip_detector_sample(&s,0xffff0000u+61000,true,3200);assert(!s.ready&&s.count==1);
 warm(0,4000);for(uint32_t t=60000;t<125000;t+=20)clip_detector_sample(&s,t,true,2000);
 assert(s.ready&&fabs(s.baseline-2000)<.001&&s.long_confirmed);
 puts("CLIP: two-sample pulse + both fast edges, 31-sample sustained event, slow-light rejection, gaps, rollover and 60s mean passed");return 0;
}
