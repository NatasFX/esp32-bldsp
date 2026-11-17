#define _CRT_SECURE_NO_WARNINGS
#include "thermal_observer.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static void replay(const char *path,double ambient){
 FILE *f=fopen(path,"r");assert(f);char header[128];assert(fgets(header,sizeof(header),f));
 thermal_observer_t s;thermal_observer_init(&s,ambient);unsigned long long ms;double ntc,ref;unsigned count=0;
 while(fscanf(f,"%llu,%lf,%lf",&ms,&ntc,&ref)==3){
  thermal_observer_sample(&s,(uint64_t)ms,true,ntc);assert(fabs(s.coil-ref)<.03);count++;
 }
 fclose(f);assert(count>1000);
}
int main(void){
 replay("observer_run1.csv",27.86);replay("observer_run2.csv",27.8);
 thermal_observer_t s;thermal_observer_init(&s,27.8);
 for(uint64_t i=1;i<=259200;i++){assert(thermal_observer_sample(&s,i*1000,true,50)||i<10);assert(isfinite(s.coil));}
 // Constant NTC produces bounded state, not an ever increasing hidden body.
 assert(fabs(s.coil-116.375)<.2);
 double steady=s.coil;
 for(uint64_t i=259201;i<=262800;i++){thermal_observer_sample(&s,i*1000,true,50+(i%2?.8:-.8));assert(fabs(s.coil-steady)<8);}
 assert(!thermal_observer_sample(&s,262801000,false,NAN));assert(s.count==0);
 for(unsigned i=0;i<10;i++)thermal_observer_sample(&s,262802000+i*1000,true,28);
 assert(s.valid);assert(!thermal_observer_sample(&s,262820000,true,28));assert(s.count==1);
 puts("observer: both recorded runs match Python; 72h bounded, noise, invalid sensor and gap recovery passed");return 0;
}
