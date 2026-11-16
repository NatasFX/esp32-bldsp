#include "thermal_model.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void){
    thermal_model_t s;thermal_model_init(&s,27.86);
    assert(fabs(thermal_model_current(&s,17,0)-4.7266)<.01);
    for(int i=0;i<776;i++)assert(thermal_model_step(&s,17,0,true,1));
    // Frozen independent SciPy solve_ivp prediction from the 15 V data.
    assert(fabs(s.ambient+s.coil_delta-139.423707)<.02);
    assert(fabs(s.ambient+s.ntc_delta-54.121011)<.02);
    double hot=s.coil_delta;for(int i=0;i<120;i++)assert(thermal_model_step(&s,17,0,false,1));assert(s.coil_delta<hot);
    thermal_model_init(&s,27.86);for(int i=0;i<60;i++)assert(thermal_model_step(&s,17,4,true,1));
    assert(fabs(s.ambient+s.coil_delta-82.68187)<.03);
    assert(thermal_model_current(&s,17,4)<=4);
    thermal_model_t before=s;assert(!thermal_model_step(&s,17,4,true,6));assert(s.coil_delta==before.coil_delta);
    assert(!thermal_model_step(&s,NAN,4,true,1));
    thermal_model_init_observed(&s,27.8,30,30);
    assert(fabs(s.ntc_delta-2.2)<1e-9&&s.body_effect>1.9);
    for(int i=0;i<120;i++)assert(thermal_model_step(&s,17,0,true,1));
    assert(fabs(s.ambient+s.coil_delta-115.851)<.05);
    thermal_model_init(&s,27.86);for(int i=0;i<776;i++)assert(thermal_model_step(&s,11,0,true,1));assert(fabs(s.ambient+s.coil_delta-81.357)<.03);
    puts("thermal_model: frozen SciPy curves, current limit, cooling and invalid time checks passed");return 0;
}
