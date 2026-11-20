#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "vcc_stats.h"
#include "battery_policy.h"
int main(void){
 vcc_stats_t w={0};for(int i=0;i<100;i++)vcc_stats_add(&w,true,i==50?1800:2000);
 assert(w.count==100&&w.min_mv==1800&&vcc_stats_mean(&w)==1998);
 vcc_stats_add(&w,false,0);assert(w.missed==1&&w.min_mv==1800);
 battery_config_t c={12.2,11.8,11,10.5};assert(battery_config_valid(&c));
 battery_config_t bad=c;bad.critical_v=13;assert(!battery_config_valid(&bad));
 battery_policy_t p={0};battery_sample(&p,&c,1000,true,12.7,10.8);assert(battery_level(&p,1000)==1);
 battery_sample(&p,&c,2000,true,12.7,10.3);assert(battery_level(&p,2000)==2);
 for(unsigned t=3000;t<=8000;t+=1000)battery_sample(&p,&c,t,true,12.7,12.5);
 assert(battery_level(&p,8000)==0);
 p=(battery_policy_t){0};for(unsigned t=1000;t<=10000;t+=1000)battery_sample(&p,&c,t,true,12.,12.);assert(battery_level(&p,10000)==1);
 p=(battery_policy_t){0};for(unsigned t=1000;t<=10000;t+=1000)battery_sample(&p,&c,t,true,11.5,11.5);assert(battery_level(&p,10000)==2);
 assert(battery_level(&p,13000)==0);
 battery_sample(&p,&c,14000,false,0,0);assert(p.count==0&&battery_level(&p,14000)==0);
 p=(battery_policy_t){0};for(unsigned t=1000;t<=40000;t+=1000)battery_sample(&p,&c,t,true,12.7,12.7);
 assert(p.count==30&&fabs(p.slow_sum/30-12.7)<1e-8);
 puts("Battery: sample mean/min, invalid exclusion, transient/sustained thresholds and stale handling OK");return 0;
}
