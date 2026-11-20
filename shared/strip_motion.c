#include "strip_motion.h"
#include <math.h>
#include <stdlib.h>
#define MOTION_STEP 328
#define QUIET_MS 2000
#define RELEASE_MS 3000
static const double base_speed=1./30.;
// Preserve the near-limit tolerance, but signal the displayed 0.0/100.0 too.
static int8_t endpoint_zone(uint16_t p){return p<=32?-2:p>=65535-32?2:p<=328?-1:p>=65535-328?1:0;}
void strip_motion_observe(strip_motion_t *s,uint64_t now,const uint16_t p[5],uint8_t valid){
 int winner=-1,largest=0,delta=0;bool hit=false;
 for(int i=0;i<5;i++){
  uint8_t mask=1u<<i;
  if(!(valid&mask)){s->seeded&=~mask;continue;}
  if(!(s->seeded&mask)){s->anchor[i]=p[i];s->endpoint_zone[i]=endpoint_zone(p[i]);s->seeded|=mask;continue;}
  // Endpoint edges must not depend on the movement/noise deadband.
  int8_t zone=endpoint_zone(p[i]);
  if(zone&&(!s->endpoint_zone[i]||zone*s->endpoint_zone[i]<0||abs(zone)>abs(s->endpoint_zone[i])))hit=true;
  s->endpoint_zone[i]=zone;
  int d=(int)p[i]-s->anchor[i];
  if(abs(d)>=MOTION_STEP){
   s->anchor[i]=p[i];
   if(abs(d)>largest){winner=i;largest=abs(d);delta=d;}
  }
 }
 if(hit){
  if(!s->endpoint||now>=s->endpoint_until){s->endpoint_at=now;s->endpoint_until=now+1000;}
  else{
   // Finish the current fade; append two full cycles without resetting phase.
   uint64_t until=s->endpoint_at+((now-s->endpoint_at)/500+3)*500;
   if(until>s->endpoint_until)s->endpoint_until=until;
  }
  s->endpoint=true;
 }
 if(winner<0)return;
 s->last_motion=now;s->moving=true;s->releasing=false;
 double fraction=p[winner]/65535.;
 // Negative phase velocity moves the colors toward increasing LED indices.
 s->target=(delta>0?-1.:1.)*2.5*(base_speed+(.5-base_speed)*fraction);
}
double strip_motion_tick(strip_motion_t *s,uint64_t now,bool online){
 double base=online?-base_speed:1./8.;
 // Disconnected status takes priority over a lingering pot movement.
 if(!online){s->moving=false;s->releasing=false;}
 if(!s->last_tick){s->last_tick=now;s->speed=base;return base;}
 uint64_t elapsed=now-s->last_tick;s->last_tick=now;
 double dt=(elapsed>200?200:elapsed)/1000.;
 if(!s->moving){s->speed=base;return base;}
 uint64_t age=now-s->last_motion;
 if(age<QUIET_MS){s->speed+=(s->target-s->speed)*(1-exp(-dt/.15));}
 else if(age<QUIET_MS+RELEASE_MS){
  if(!s->releasing){s->release_speed=s->speed;s->releasing=true;}
  double t=(age-QUIET_MS)/(double)RELEASE_MS,remaining=(1-t)*(1-t)*(1-t);
  s->speed=base+(s->release_speed-base)*remaining;
 }else{s->speed=base;s->moving=false;}
 return s->speed;
}
double strip_motion_dim(const strip_motion_t *s,uint64_t now){
 uint64_t until=s->endpoint_until?s->endpoint_until:s->endpoint_at+1000;
 if(!s->endpoint||now<s->endpoint_at||now>=until)return 1.;
 double phase=((now-s->endpoint_at)%500)/500.;
 double wave=sin(3.141592653589793*phase);
 return 1.-.8*wave*wave;
}
