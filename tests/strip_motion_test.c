#include "strip_motion.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void){
 strip_motion_t s={0};uint16_t p[5]={32768,32768,32768,32768,32768};
 strip_motion_tick(&s,100,true);strip_motion_observe(&s,100,p,31);assert(!s.moving&&!s.endpoint);
 p[0]+=100;strip_motion_observe(&s,120,p,31);assert(!s.moving);
 p[0]=40000;strip_motion_observe(&s,140,p,31);assert(s.moving&&s.target<-.3);
 for(unsigned t=140;t<=2000;t+=40)strip_motion_tick(&s,t,true);
 double fast=s.speed;assert(fast<-.3);
 strip_motion_tick(&s,2140,true);assert(s.releasing&&fabs(s.speed-fast)<.01);
 strip_motion_tick(&s,3640,true);assert(s.speed>fast&&s.speed<-.033);
 strip_motion_tick(&s,5140,true);assert(!s.moving&&fabs(s.speed+1./30.)<1e-8);
 p[0]=20000;strip_motion_observe(&s,5200,p,31);assert(s.target>0);
 for(unsigned t=5200;t<=7000;t+=40)strip_motion_tick(&s,t,true);assert(s.speed>0);
 strip_motion_tick(&s,10200,false);assert(fabs(s.speed-1./8.)<1e-8);
 p[0]=65535;strip_motion_observe(&s,10300,p,31);assert(s.endpoint_at==10300);
 assert(fabs(strip_motion_dim(&s,10550)-.2)<1e-8);assert(fabs(strip_motion_dim(&s,10800)-1)<1e-8);assert(fabs(strip_motion_dim(&s,11050)-.2)<1e-8);assert(strip_motion_dim(&s,11300)==1);
 p[0]=64000;strip_motion_observe(&s,11400,p,31);p[0]=65535;strip_motion_observe(&s,11500,p,31);assert(s.endpoint_at==11500);
 // No pause is needed: leaving and immediately reaching the limit flashes again.
 p[0]=65000;strip_motion_observe(&s,13600,p,31);p[0]=65535;strip_motion_observe(&s,13700,p,31);assert(s.endpoint_at==13700);
 uint64_t edge=s.endpoint_at;p[0]=65530;strip_motion_observe(&s,13740,p,31);assert(s.endpoint_at==edge);
 // Largest simultaneous movement wins; uninitialized/reconnected channels only seed.
 p[0]=60000;p[3]=0;strip_motion_observe(&s,13800,p,31);assert(fabs(s.target-2.5/30.)<1e-8);
 strip_motion_observe(&s,13900,p,0);uint64_t last=s.last_motion;p[3]=65535;strip_motion_observe(&s,14000,p,31);assert(s.last_motion==last);
 // Reproduce missed arrivals: last change is smaller than the movement deadband.
 strip_motion_t slow={0};uint16_t q[5]={65199,20000,20000,20000,20000};
 strip_motion_observe(&slow,20000,q,31);q[0]=65520;strip_motion_observe(&slow,20040,q,31);
 assert(slow.endpoint&&slow.endpoint_at==20040);
 // The opposite end and any channel must also trigger with sub-deadband steps.
 strip_motion_t low={0};q[1]=329;strip_motion_observe(&low,22000,q,31);
 q[1]=320;strip_motion_observe(&low,22040,q,31);assert(low.endpoint_at==22040);
 // Quick new arrivals must not keep restarting the fade before its dimmest point.
 q[0]=64000;strip_motion_observe(&slow,20100,q,31);q[0]=65530;strip_motion_observe(&slow,20120,q,31);
 assert(slow.endpoint_at==20040&&slow.endpoint_until>=21540);
 assert(fabs(strip_motion_dim(&slow,20290)-.2)<1e-8);
 uint64_t until=slow.endpoint_until;strip_motion_observe(&slow,20160,q,31);assert(slow.endpoint_until==until);
 // Entering displayed 100.0% must still flash after lingering near the limit.
 strip_motion_t rounded={0};q[0]=65210;strip_motion_observe(&rounded,30000,q,31);
 q[0]=65520;strip_motion_observe(&rounded,34000,q,31);assert(rounded.endpoint_at==34000);
 puts("Motion: jitter gate, direction/speed, 2s hold + 3s ease-out, 20-percent dual dim, immediate edge retrigger, largest pot and reseeding passed");return 0;
}
