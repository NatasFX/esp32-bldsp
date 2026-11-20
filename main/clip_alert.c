#include "clip_alert.h"
#include <string.h>
void clip_alert_sample(clip_alert_t *s,uint64_t ms,bool ready,bool hard_pulse,bool hard_sustained){
    if(!ready){memset(s,0,sizeof(*s));return;}
    uint64_t bin=ms/1000;
    if(hard_pulse||hard_sustained){
        if(!s->series||ms-s->last_hard>2000){s->series=true;s->first_hard=ms;}
        s->last_hard=ms;s->bins[bin%6]=bin;s->occupied[bin%6]=true;
    }
    bool covered=s->series&&bin>=5&&ms-s->first_hard>=5000&&ms-s->last_hard<=1000;
    for(unsigned i=1;i<=5&&covered;i++){
        uint64_t expected=bin-i;
        if(!s->occupied[expected%6]||s->bins[expected%6]!=expected)covered=false;
    }
    if(covered){s->severe_seen=true;s->last_severe=ms;}
    s->flash=s->severe_seen&&ms-s->last_severe<2000;
}
