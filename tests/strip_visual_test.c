#include "strip_visual.h"
#include "strip_palette.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
int main(void){
 strip_visual_t brief={0},longer={0};
 strip_visual_observe(&brief,1000,1,1000,0,0,false);
 strip_visual_observe(&brief,1250,1,1250,20,1,false);
 assert(brief.envelope>.3&&brief.envelope<.7&&brief.hold_until>1250);
 strip_visual_observe(&longer,1000,1,1000,0,0,false);
 strip_visual_observe(&longer,2000,1,2000,1000,1,true);
 assert(longer.envelope>brief.envelope&&longer.hold_until>brief.hold_until);
 // Pulses fully between packets remain observable via cumulative counters.
 strip_visual_t missed={0};strip_visual_observe(&missed,1000,1,1000,0,0,false);
 strip_visual_observe(&missed,1500,1,1500,40,2,false);assert(missed.envelope>.3);
 double old=brief.envelope;strip_visual_tick(&brief,brief.hold_until+1,true);assert(brief.envelope>0&&brief.envelope<old);
 for(unsigned now=2000;now<9000;now+=40)strip_visual_tick(&brief,now,true);
 assert(brief.envelope<.002);
 strip_visual_observe(&longer,3000,2,100,0,0,false);assert(longer.envelope==0&&longer.occupancy==0);
 strip_visual_t wrap={0};strip_visual_observe(&wrap,1000,1,0xffffff80u,0xfffffff0u,8,false);
 strip_visual_observe(&wrap,1250,1,122,4,9,false);assert(wrap.envelope>.3);
 for(unsigned mode=0;mode<3;mode++)for(unsigned pixel=0;pixel<30;pixel++){
  strip_rgb_t c=strip_visual_pixel(&missed,1750,pixel,30,mode,30,true);assert(c.g<=77&&c.b<=77);
  c=strip_visual_pixel(&missed,1750,pixel,30,mode,0,true);assert(!c.g&&!c.b);
 }
 // A 100ms advance moves the pattern one pixel on a 300-pixel ring.
 for(unsigned mode=0;mode<3;mode++){
  strip_visual_t right={.last_update=1000},left=right;
  strip_visual_tick(&right,1100,true);strip_visual_tick(&left,1100,false);
  strip_visual_t origin={0};
  strip_rgb_t expected=strip_visual_pixel(&origin,1100,100,300,mode,100,true);
  strip_rgb_t r=strip_visual_pixel(&right,1100,101,300,mode,100,true);
  strip_rgb_t l=strip_visual_pixel(&left,1100,7,80,mode,100,false);
  assert(abs((int)r.r-expected.r)<=1&&abs((int)r.g-expected.g)<=1&&abs((int)r.b-expected.b)<=1);
  expected=strip_visual_pixel(&origin,1100,8,80,mode,100,false);
  assert(abs((int)l.r-expected.r)<=1&&abs((int)l.g-expected.g)<=1&&abs((int)l.b-expected.b)<=1);
  double phase=right.rainbow_phase;
  strip_visual_tick(&right,1200,false);
  assert(fabs(right.rainbow_phase-(phase+.0125-1.))<1e-9);
  assert(phase>.99);
 }
 // Offline breath reaches 30% at two seconds and returns at four seconds.
 strip_visual_t breath={0};
 strip_rgb_t high=strip_visual_pixel(&breath,4000,0,22,1,100,false);
 strip_rgb_t low=strip_visual_pixel(&breath,2000,0,22,1,100,false);
 assert(abs((int)low.r-(int)lround(high.r*.3))<=1);
 // Endpoint dim must never dim the CLIP overlay.
 strip_visual_t clip_priority={.envelope=1,.phase=0,.hold_until=2000};clip_priority.motion.endpoint=true;clip_priority.motion.endpoint_at=1000;
 strip_rgb_t alarm=strip_visual_pixel(&clip_priority,1250,0,22,1,30,true);
 assert(alarm.r==255&&alarm.g==0&&alarm.b==0);
 for(unsigned brightness=0;brightness<=100;brightness++){
  clip_priority.envelope=.3;
  strip_rgb_t peak=strip_visual_pixel(&clip_priority,1250,0,22,1,brightness,true);
  assert(peak.r==255&&peak.g==0&&peak.b==0);
 }
 strip_rgb_t tail=strip_visual_pixel(&clip_priority,2400,0,22,1,0,true);
 assert(tail.r>0&&tail.r<255&&tail.g==0&&tail.b==0);
 clip_priority.envelope=0;
 strip_rgb_t dark=strip_visual_pixel(&clip_priority,2400,0,22,1,0,true);
 assert(!dark.r&&!dark.g&&!dark.b);
 for(unsigned palette=0;palette<STRIP_PALETTE_COUNT;palette++){
  uint8_t start[3],wrap[3];strip_palette_sample(palette,0,start);strip_palette_sample(palette,1,wrap);
  for(int k=0;k<3;k++)assert(start[k]==wrap[k]);
  strip_visual_t colored={.palette=palette};
  for(unsigned mode=0;mode<3;mode++)for(unsigned px=0;px<22;px++){
   strip_rgb_t c=strip_visual_pixel(&colored,1750,px,22,mode,30,true);assert(c.r<=77&&c.g<=77&&c.b<=77);
  }
  colored.envelope=1;colored.phase=0;colored.hold_until=2000;colored.motion.endpoint=true;colored.motion.endpoint_at=1000;
  strip_rgb_t c=strip_visual_pixel(&colored,1250,0,22,1,30,true);assert(c.r==255&&c.g==0&&c.b==0);
 }
 uint8_t negative[3],positive[3];strip_palette_sample(1,-.25,negative);strip_palette_sample(1,.75,positive);for(int k=0;k<3;k++)assert(negative[k]==positive[k]);
 // Higher render rate must not accelerate idle motion or the exponential fade.
 strip_visual_t slow={.last_update=1000,.envelope=1,.hold_until=1000},fast=slow;
 for(unsigned now=1040;now<=2000;now+=40)strip_visual_tick(&slow,now,true);
 for(unsigned frame=1;frame<=120;frame++)strip_visual_tick(&fast,1000+(frame*1000)/120,true);
 assert(fabs(slow.rainbow_phase-fast.rainbow_phase)<1e-9);
 assert(fabs(slow.envelope-fast.envelope)<1e-9);
 puts("Strip: pulse counters, duration scaling, fade, reboot/rollover, RGB bounds and bidirectional rainbow passed");
 return 0;
}
