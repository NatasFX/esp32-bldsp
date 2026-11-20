#include "strip_visual.h"
#include "strip_palette.h"
#include <math.h>
#include <string.h>
static double clamp(double v,double lo,double hi){return v<lo?lo:v>hi?hi:v;}
void strip_visual_observe(strip_visual_t *s,uint64_t now,uint32_t boot,uint32_t ms,uint32_t total,uint32_t count,bool lit){
    if(!s->initialized||s->boot!=boot){
        double rainbow=s->rainbow_phase;uint64_t last=s->last_update;strip_motion_t motion=s->motion;uint8_t palette=s->palette;
        memset(s,0,sizeof(*s));s->motion=motion;s->palette=palette;s->rainbow_phase=rainbow;s->last_update=last;s->initialized=true;s->boot=boot;s->source_ms=ms;s->total_ms=total;s->count=count;if(!s->last_update)s->last_update=now;
        if(lit){s->occupancy=.3;s->envelope=.65;s->hold_until=now+700;s->alert_seen=true;}
        return;
    }
    uint32_t dt=ms-s->source_ms,low=total-s->total_ms,events=count-s->count;
    s->source_ms=ms;s->total_ms=total;s->count=count;
    if(!dt||dt>5000||low>dt+100||events>250)return;
    double duty=clamp((double)low/dt,0,1),alpha=1-exp(-(double)dt/1800.);
    s->occupancy+=alpha*(duty-s->occupancy);
    if(low||events||lit){
        double target=clamp(.28+1.8*s->occupancy+low/700.+fmin(events,4)*.035,.3,1);
        if(target>s->envelope)s->envelope=target;
        s->hold_until=now+(uint64_t)clamp(180+low*2.+s->occupancy*1500.,200,2500);
        s->alert_seen=true;if(events)s->phase=0;
    }
}
void strip_visual_tick(strip_visual_t *s,uint64_t now,bool online){
    uint64_t elapsed=s->last_update?now-s->last_update:0;s->last_update=now;
    double dt=(double)(elapsed>200?200:elapsed)/1000.;
    if(!online){s->envelope*=exp(-dt/.7);s->occupancy*=exp(-dt/1.8);}
    else if(now>s->hold_until){s->envelope*=exp(-dt/.65);s->occupancy*=exp(-dt/1.8);}
    if(s->envelope<.002)s->envelope=0;
    s->phase=fmod(s->phase+dt*(2.+3.*s->envelope),1.);
    // Increasing LED indices define right; retain phase when the link changes.
    s->rainbow_phase+=dt*strip_motion_tick(&s->motion,now,online);
    s->rainbow_phase-=floor(s->rainbow_phase);
}
static strip_rgb_t hsv(double hue,double value){
    double x=(hue-floor(hue))*6.;int sector=(int)x;double f=x-sector;
    double r=0,g=0,b=0;
    switch(sector){case 0:r=1;g=f;break;case 1:r=1-f;g=1;break;case 2:g=1;b=f;break;case 3:g=1-f;b=1;break;case 4:r=f;b=1;break;default:r=1;b=1-f;break;}
    return (strip_rgb_t){(uint8_t)lround(255*r*value),(uint8_t)lround(255*g*value),(uint8_t)lround(255*b*value)};
}
strip_rgb_t strip_visual_pixel(const strip_visual_t *s,uint64_t now,unsigned pixel,unsigned length,unsigned mode,unsigned brightness,bool online){
    double t=now/1000.,pos=length>1?(double)pixel/length:0;
    double travel=pos+s->rainbow_phase;
    double hue=travel,value=.7+.2*sin(6.28318530718*travel);
    if(mode==1)value=.85;
    if(mode==2)value=.45+.45*(.5+.5*sin(t*.8));
    double intensity=value*strip_motion_dim(&s->motion,now);
    // Four-second breathing cycle, from 30% to 100% of decorative brightness.
    if(!online)intensity*=.65+.35*cos(6.28318530718*t/4.);
    strip_rgb_t c=hsv(hue,intensity);
    if(s->palette>0&&s->palette<STRIP_PALETTE_COUNT){
        uint8_t rgb[3];strip_palette_sample(s->palette,travel,rgb);
        c=(strip_rgb_t){(uint8_t)lround(rgb[0]*intensity),(uint8_t)lround(rgb[1]*intensity),(uint8_t)lround(rgb[2]*intensity)};
    }
    if(online&&s->battery_level){
        uint64_t period=s->battery_level>=2?2000:5000,phase=now%period;
        if(phase<300){double pulse=sin(3.141592653589793*phase/300.);pulse*=pulse;
            c.r=(uint8_t)lround(c.r*(1-pulse)+255*pulse);
            c.g=(uint8_t)lround(c.g*(1-pulse)+(s->battery_level>=2?0:190)*pulse);
            c.b=(uint8_t)lround(c.b*(1-pulse));
        }
    }
    double duty=.20+.55*s->envelope;
    // Alarm pulses are full red regardless of the user's decorative brightness.
    // Occupancy still controls cadence/duty; fade only after the alert hold ends.
    double alarm=s->envelope>0&&s->phase<duty?1.:0.;
    if(now>s->hold_until)alarm*=exp(-(double)(now-s->hold_until)/650.);
    double scale=clamp(brightness,0,100)/100.;
    return (strip_rgb_t){(uint8_t)lround(c.r*scale*(1-alarm)+255*alarm),(uint8_t)lround(c.g*scale*(1-alarm)),(uint8_t)lround(c.b*scale*(1-alarm))};
}
