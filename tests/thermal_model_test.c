#include "thermal_model.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void){
    thermal_model_t s;thermal_model_init(&s,27.86);
    assert(fabs(thermal_model_current(&s,17,0)-4.7266)<.01);
    for(int i=0;i<776;i++)assert(thermal_model_step(&s,17,0,true,1));
    // Independent SciPy solve_ivp references for joint_v2 coefficients.
    assert(fabs(s.ambient+s.coil_delta-138.19807211575025)<.02);
    assert(fabs(s.ambient+s.ntc_delta-54.0879616831193)<.02);
    double hot=s.coil_delta;for(int i=0;i<120;i++)assert(thermal_model_step(&s,17,0,false,1));assert(s.coil_delta<hot);
    thermal_model_init(&s,27.86);for(int i=0;i<60;i++)assert(thermal_model_step(&s,17,4,true,1));
    assert(fabs(s.ambient+s.coil_delta-79.59301370858876)<.03);
    assert(thermal_model_current(&s,17,4)<=4);
    thermal_model_t before=s;assert(!thermal_model_step(&s,17,4,true,6));assert(s.coil_delta==before.coil_delta);
    assert(!thermal_model_step(&s,NAN,4,true,1));
    thermal_model_init_observed(&s,27.8,30,30);
    assert(fabs(s.ntc_delta-2.2)<1e-9&&s.body_effect>1.8);
    for(int i=0;i<120;i++)assert(thermal_model_step(&s,17,0,true,1));
    assert(fabs(s.ambient+s.coil_delta-113.60777338898545)<.05);
    thermal_model_init(&s,27.86);for(int i=0;i<776;i++)assert(thermal_model_step(&s,11,0,true,1));assert(fabs(s.ambient+s.coil_delta-80.71496386250004)<.03);
    puts("thermal_model: joint_v2 SciPy curves, current limit, cooling and invalid time checks passed");return 0;
}
