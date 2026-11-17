#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "telemetry_math.h"
int main(void){
    double r,t;
    assert(telemetry_ntc_estimate(1650,&r,&t));assert(fabs(r-10000)<0.001);assert(fabs(t-25)<0.001);
    assert(telemetry_ntc_estimate(1000,&r,&t));assert(r<10000&&t>25);
    assert(telemetry_ntc_estimate(2300,&r,&t));assert(r>10000&&t<25);
    assert(!telemetry_ntc_estimate(0,&r,&t));assert(!telemetry_ntc_estimate(3300,&r,&t));
    double previous=1000;
    for(int mv=20;mv<=3000;mv++){assert(telemetry_ntc_estimate(mv,&r,&t));assert(isfinite(t)&&t<previous);previous=t;}
    puts("Telemetry: B3950 reference, grounded NTC direction, finite range and rail rejection OK");return 0;
}
