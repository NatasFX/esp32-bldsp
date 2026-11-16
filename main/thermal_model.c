#include "thermal_model.h"
#include <math.h>
void thermal_model_init(thermal_model_t *s,double ambient){*s=(thermal_model_t){.ambient=ambient};}
void thermal_model_init_observed(thermal_model_t *s,double ambient,double coil,double ntc){
    thermal_model_init(s,ambient);s->coil_delta=coil-ambient;s->ntc_delta=ntc-ambient;
    // Initial sensor is assumed approximately stationary, not instantly at ambient.
    // H is the body contribution, not measured frame temperature.
    s->body_effect=fmax(0,s->ntc_delta-.12847667377287805*s->coil_delta);
}
double thermal_model_current(const thermal_model_t *s,double voltage,double limit){
    double r=3.596662911673227+.011856911913765731*(s->ambient+s->coil_delta-27.86);
    if(!isfinite(r)||r<=0||!isfinite(voltage)||voltage<0)return NAN;
    double i=voltage/r;return limit>0&&i>limit?limit:i;
}
static void derivative(const thermal_model_t *s,double v,double limit,bool on,double *d){
    double i=on?thermal_model_current(s,v,limit):0;
    double r=3.2057936565596066+.011856911913765731*(s->ambient+s->coil_delta-27.86);
    double p=i*i*r;
    d[0]=p/45.28945929537856-.011712475216921526*s->coil_delta+.009684306578637078*s->body_effect;
    d[1]=.00046138761184564897*s->coil_delta-.0039747387754912895*s->body_effect;
    d[2]=(.12847667377287805*s->coil_delta+s->body_effect-s->ntc_delta)/13.023514572513946;
}
static thermal_model_t offset(const thermal_model_t *s,const double *d,double dt){
    thermal_model_t a=*s;a.coil_delta+=d[0]*dt;a.body_effect+=d[1]*dt;a.ntc_delta+=d[2]*dt;return a;
}
bool thermal_model_step(thermal_model_t *s,double v,double limit,bool powered,double dt){
    if(!s||!isfinite(dt)||dt<0||dt>5||!isfinite(v)||v<0||v>24||!isfinite(limit)||limit<0||limit>10)return false;
    while(dt>1e-9){double h=dt>.25?.25:dt,a[3],b[3],c[3],d[3];
        derivative(s,v,limit,powered,a);thermal_model_t t=offset(s,a,h/2);derivative(&t,v,limit,powered,b);
        t=offset(s,b,h/2);derivative(&t,v,limit,powered,c);t=offset(s,c,h);derivative(&t,v,limit,powered,d);
        s->coil_delta+=h*(a[0]+2*b[0]+2*c[0]+d[0])/6;
        s->body_effect+=h*(a[1]+2*b[1]+2*c[1]+d[1])/6;
        s->ntc_delta+=h*(a[2]+2*b[2]+2*c[2]+d[2])/6;dt-=h;
    }
    return isfinite(s->coil_delta)&&isfinite(s->body_effect)&&isfinite(s->ntc_delta);
}
