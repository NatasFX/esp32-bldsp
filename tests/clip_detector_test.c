#include "clip_detector.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static clip_detector_t s;
static void warm(uint32_t origin,int raw){clip_detector_reset(&s);for(unsigned i=0;i<3000;i++){clip_detector_sample(&s,origin+i*20,true,raw);assert(!s.ready);}}
int main(void){
 clip_config_t custom=clip_config_default();assert(clip_config_valid(&custom));
 custom.release_drop_percent=21;assert(!clip_config_valid(&custom));custom.release_drop_percent=5;custom.drop_percent=10;custom.window_ms=5000;custom.min_low_samples=1;
 clip_detector_configure(&s,&custom);
 for(unsigned i=0;i<250;i++)clip_detector_sample(&s,i*20,true,4000);
 clip_detector_sample(&s,5000,true,3500);assert(s.candidate&&s.active);
 clip_detector_sample(&s,5020,true,4000);assert(!s.active);
 clip_detector_sample(&s,5060,false,0);assert(s.config.window_ms==5000&&s.config.drop_percent==10);

 // Confirm on the second low sample, not on recovery.
 warm(0,4000);clip_detector_sample(&s,60000,true,2000);assert(s.candidate&&!s.active);
 clip_detector_sample(&s,60020,true,2100);assert(s.active&&s.pulse_qualified);
 clip_detector_sample(&s,60040,true,4000);assert(!s.active&&!s.candidate);
 clip_detector_sample(&s,60060,true,4000);assert(!s.active);
 // One-sample dip is rejected.
 warm(0,4000);clip_detector_sample(&s,60000,true,1900);clip_detector_sample(&s,60020,true,4000);assert(!s.active);
 // Exactly 20 percent is not enough; reference excludes the current sample.
 warm(0,4000);clip_detector_sample(&s,60000,true,3200);assert(!s.candidate&&fabs(s.baseline-4000)<.001);
 // More than 30 consecutive low samples confirms a sustained event at #31.
 warm(0,4000);
 for(unsigned i=0;i<30;i++){clip_detector_sample(&s,60000+i*20,true,2000);assert(s.active==(i>=1));}
 clip_detector_sample(&s,60600,true,2000);assert(s.active&&s.long_confirmed);
 clip_detector_sample(&s,60620,true,2000);assert(s.active);
 clip_detector_sample(&s,60640,true,4000);assert(!s.active&&!s.candidate);
 // Slow flashlight movement cannot start a candidate, even held low for >30 samples.
 warm(0,4000);
 for(uint32_t t=60000;t<=64000;t+=20){clip_detector_sample(&s,t,true,4000-(int)((t-60000)/2));assert(!s.active&&!s.candidate);}
 for(uint32_t t=64020;t<=70000;t+=20){clip_detector_sample(&s,t,true,2000);assert(!s.active);}
 // Sharp fall and qualified samples remain active during a slow recovery.
 warm(0,4000);clip_detector_sample(&s,60000,true,2000);clip_detector_sample(&s,60020,true,2000);
 for(unsigned i=1;i<=20;i++){clip_detector_sample(&s,60020+i*20,true,2000+(int)i*100);assert(s.active==(i<16));}
 // A data gap cannot be interpreted as an edge; invalid ADC resets warmup.
 warm(0,4000);clip_detector_sample(&s,60100,true,2000);assert(!s.candidate&&!s.active);
 clip_detector_sample(&s,60120,false,0);assert(!s.ready&&s.count==0);
 // Timestamp rollover and reference expiry.
 warm(0xffff0000u,3200);clip_detector_sample(&s,0xffff0000u+60000,true,1800);assert(s.candidate&&!s.active);
 clip_detector_sample(&s,0xffff0000u+60020,true,1800);clip_detector_sample(&s,0xffff0000u+60040,true,3200);assert(!s.active);
 clip_detector_sample(&s,0xffff0000u+61000,true,3200);assert(!s.ready&&s.count==1);
 warm(0,4000);for(uint32_t t=60000;t<125000;t+=20)clip_detector_sample(&s,t,true,2000);
 assert(s.ready&&fabs(s.baseline-4000)<.001&&s.long_confirmed&&s.active);
 assert(s.reference_count==0); // all clean samples expired, reference held
 clip_detector_sample(&s,125000,true,4000);assert(!s.candidate&&!s.active&&s.ready);
 clip_detector_sample(&s,125020,true,4000);assert(fabs(s.baseline-4000)<.001);
 // Repeated pulses never contaminate the mean, even across several windows.
 warm(0,4000);
 for(uint32_t t=60000;t<240000;t+=20){
   unsigned phase=(t/20)%10;int raw=phase<4?2000:4000;
   clip_detector_sample(&s,t,true,raw);
   assert(s.ready&&fabs(s.baseline-4000)<.001);
 }
 // A rejected single-sample candidate also cannot bias the reference.
 warm(0,4000);clip_detector_sample(&s,60000,true,1900);
 clip_detector_sample(&s,60020,true,4000);assert(!s.active&&s.sum==s.reference_count*4000u);
 // Gradual ambient changes still update the clean mean.
 warm(0,4000);for(uint32_t t=60000;t<180000;t+=20){int raw=t<100000?4000-(int)((t-60000)/40):3000;clip_detector_sample(&s,t,true,raw);assert(!s.active);}
 assert(fabs(s.baseline-3000)<.001);
 // Six low samples classify a hard pulse; five remain a brief pulse.
 warm(0,4000);for(unsigned i=0;i<6;i++)clip_detector_sample(&s,60000+i*20,true,2000);
 assert(s.active&&s.hard_pulse);clip_detector_sample(&s,60120,true,4000);assert(!s.active&&!s.hard_pulse);
 warm(0,4000);for(unsigned i=0;i<5;i++)clip_detector_sample(&s,60000+i*20,true,2000);
 assert(s.active&&!s.hard_pulse);clip_detector_sample(&s,60100,true,4000);assert(!s.active&&!s.hard_pulse);
 // Captured LDR waveform: 63% dip with a gradual recovery previously rejected.
 warm(0,3817);
 const int captured[]={3750,2146,1767,1623,1571,2514,2549,2636,2400,2138,1902,1573,1417,2535,2981,3212,3322,3277,3464,3627};
 unsigned starts=0;bool previous=false;
 for(unsigned i=0;i<sizeof(captured)/sizeof(captured[0]);i++){
    clip_detector_sample(&s,60000+i*20,true,captured[i]);
    if(s.active&&!previous)starts++;previous=s.active;
    if(i>=2&&i<=17)assert(s.active);
    assert(fabs(s.baseline-3817)<.1);
 }
 assert(starts==1&&!s.active);
 puts("CLIP: two-sample pulse + fast fall and slow recovery, 31-sample sustained event, slow-light rejection, gaps, rollover and 60s mean passed");return 0;
}
