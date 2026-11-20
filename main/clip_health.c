#include "clip_health.h"
void clip_health_sample(clip_health_t *s,uint32_t ms,bool valid,int raw){
    if(!valid){s->timing_rail=false;s->timing_clear=false;return;}
    if(raw>=4080){
        s->timing_clear=false;
        if(!s->timing_rail){s->timing_rail=true;s->rail_since=ms;}
        if((uint32_t)(ms-s->rail_since)>=2000)s->rail_high=true;
    }else{
        s->timing_rail=false;
        if(s->rail_high&&raw<=4000){
            if(!s->timing_clear){s->timing_clear=true;s->clear_since=ms;}
            if((uint32_t)(ms-s->clear_since)>=500){s->rail_high=false;s->timing_clear=false;}
        }else s->timing_clear=false;
    }
}
