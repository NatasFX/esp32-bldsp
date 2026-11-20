#include "light_policy.h"
#include <math.h>
#include <stdlib.h>
bool light_config_valid(const light_config_t *c){
 if(c->version!=1||c->enabled>1||c->smooth>1||c->count<2||c->count>LIGHT_MAX_STEPS||!isfinite(c->latitude)||!isfinite(c->longitude)||fabs(c->latitude)>=90||fabs(c->longitude)>180)return false;
 for(unsigned i=0;i<c->count;i++){
  light_step_t s=c->steps[i];if(s.anchor>2||s.brightness>100||(s.anchor==0?(s.minute<0||s.minute>=1440):(abs(s.minute)>720)))return false;
  for(unsigned j=0;j<i;j++)if(c->steps[j].anchor==s.anchor&&c->steps[j].minute==s.minute)return false;
 }return true;
}
bool light_solar(int day,int days,double lat,double lon,int offset,double *rise,double *set){
 if(day<1||day>days||(days!=365&&days!=366)||!isfinite(lat)||!isfinite(lon)||fabs(lat)>=90||fabs(lon)>180)return false;
 const double pi=3.141592653589793,g=2*pi/days*(day-1),r=pi/180;
 // NOAA fractional-year approximation, zenith 90.833 degrees.
 double eq=229.18*(.000075+.001868*cos(g)-.032077*sin(g)-.014615*cos(2*g)-.040849*sin(2*g));
 double dec=.006918-.399912*cos(g)+.070257*sin(g)-.006758*cos(2*g)+.000907*sin(2*g)-.002697*cos(3*g)+.00148*sin(3*g);
 double h=cos(90.833*r)/(cos(lat*r)*cos(dec))-tan(lat*r)*tan(dec);
 if(h< -1||h>1)return false;
 double noon=720-4*lon-eq+offset,span=4*acos(h)/r;
 *rise=noon-span;*set=noon+span;return true;
}
bool light_evaluate(const light_config_t *c,double now,double rise,double set,unsigned *brightness){
 if(!light_config_valid(c)||!isfinite(now)||now<0||now>=1440||!isfinite(rise)||!isfinite(set))return false;
 double t[LIGHT_MAX_STEPS];unsigned v[LIGHT_MAX_STEPS];
 for(unsigned i=0;i<c->count;i++){
  double at=c->steps[i].minute+(c->steps[i].anchor==1?rise:c->steps[i].anchor==2?set:0);
  at=fmod(at,1440);if(at<0)at+=1440;
  unsigned j=i;while(j&&t[j-1]>at){t[j]=t[j-1];v[j]=v[j-1];j--;}
  t[j]=at;v[j]=c->steps[i].brightness;
 }
 for(unsigned i=1;i<c->count;i++)if(fabs(t[i]-t[i-1])<.01)return false;
 unsigned prev=c->count-1,next=0;double before=t[prev]-1440,after=t[0];
 for(unsigned i=0;i<c->count;i++)if(t[i]<=now){prev=i;before=t[i];next=(i+1)%c->count;after=t[next]+(next==0?1440:0);}
 double value=v[prev];if(c->smooth)value+=(v[next]-(double)v[prev])*(now-before)/(after-before);
 *brightness=(unsigned)lround(value);return true;
}
