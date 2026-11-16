#include "telemetry_math.h"
#include <math.h>
bool telemetry_ldr_active(int raw,bool was_active,int threshold,int hysteresis){return raw<(was_active?threshold+hysteresis:threshold);}
bool telemetry_ntc_estimate(int mv,double *ohms,double *celsius) {
    if(mv<20||mv>3000)return false;
    double r=10000.0*mv/(3300.0-mv);
    double t=1.0/(1.0/298.15+log(r/10000.0)/3950.0)-273.15;
    if(!isfinite(r)||!isfinite(t))return false;
    *ohms=r;*celsius=t;return true;
}
