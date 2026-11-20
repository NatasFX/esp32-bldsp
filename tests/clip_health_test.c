#include "clip_health.h"
#include "pregain_control.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 clip_health_t s={0};clip_health_sample(&s,0,true,4095);clip_health_sample(&s,1999,true,4095);assert(!s.rail_high);
 clip_health_sample(&s,2000,true,4095);assert(s.rail_high);
 clip_health_sample(&s,2100,true,3500);clip_health_sample(&s,2599,true,3500);assert(s.rail_high);
 clip_health_sample(&s,2600,true,3500);assert(!s.rail_high);
 clip_health_sample(&s,3000,true,4095);clip_health_sample(&s,3500,true,4070);clip_health_sample(&s,6000,true,4095);assert(!s.rail_high);
 assert((int16_t)pregain_attenuated(0,128)==-128);
 assert((int16_t)pregain_attenuated(768,3072)==-2304);
 assert((int16_t)pregain_attenuated(0x8000,6144)==-32768);
 assert((int16_t)pregain_attenuated((uint16_t)-32000,6144)==-32768);
 puts("Clip health: sustained rail, hysteretic recovery; pregain: signed Q8 attenuation and no cut wraparound passed");
}
