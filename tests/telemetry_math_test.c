#include "vcc_calibration.h"
#include "../shared/vcc_recovery.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "telemetry_math.h"
int main(void){
 // ADC millivolts reconstructed from the four old displayed measurements.
 assert(fabs(vcc_from_mv(1911)-8.)<.04);
 assert(fabs(vcc_from_mv(2007)-11.)<.04);
 assert(fabs(vcc_from_mv(2039)-12.)<.04);
 assert(fabs(vcc_from_mv(2072)-13.)<.04);
 assert(vcc_from_mv(2000)>vcc_from_mv(1900));
 assert(!vcc_raw_valid(0)&&!vcc_raw_valid(4095)&&vcc_raw_valid(2500));
    vcc_recovery_t recovery={0};
    assert(vcc_recovery_step(&recovery,1000,false)==1);
    assert(vcc_recovery_step(&recovery,2000,false)==0);
    assert(vcc_recovery_step(&recovery,6000,false)==1);
    assert(vcc_recovery_step(&recovery,11000,false)==0);
    assert(vcc_recovery_step(&recovery,30000,false)==2);
    assert(vcc_recovery_step(&recovery,31000,true)==0&&recovery.attempts==0);
    assert(vcc_recovery_step(&recovery,32000,false)==1);
    double r,t;
    assert(telemetry_ntc_estimate(1650,&r,&t));assert(fabs(r-10000)<0.001);assert(fabs(t-25)<0.001);
    assert(telemetry_ntc_estimate(1000,&r,&t));assert(r<10000&&t>25);
    assert(telemetry_ntc_estimate(2300,&r,&t));assert(r>10000&&t<25);
    assert(!telemetry_ntc_estimate(0,&r,&t));assert(!telemetry_ntc_estimate(3300,&r,&t));
    double previous=1000;
    for(int mv=20;mv<=3000;mv++){assert(telemetry_ntc_estimate(mv,&r,&t));assert(isfinite(t)&&t<previous);previous=t;}
    puts("Telemetry: B3950 reference, grounded NTC direction, finite range and rail rejection OK");return 0;
}
