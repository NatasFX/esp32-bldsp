#include "knob_policy.h"
#include <stdlib.h>
bool knob_binding_valid(const knob_binding_t *b){
    if(b->module==0)return true;
    if(b->module!=0x91&&b->module!=0x93)return false;
    bool gain=b->selector>=7&&b->selector<=52&&(b->selector-7)%5==0;
    if(b->selector!=1&&!gain)return false;
    int lo=b->selector==1?-60*256:-24*256,hi=b->selector==1?0:12*256;
    return b->min_q8>=lo&&b->max_q8<=hi&&b->max_q8-b->min_q8>=256;
}
// All five physical pots are wired with reversed voltage direction.
uint16_t knob_control_position(uint16_t adc_position){
    // Match the strip's existing 0.5% endpoint indication. Keep raw ADC intact.
    uint16_t p=(uint16_t)(65535u-adc_position);
    return p<=328?0:p>=65535u-328?65535:p;
}
int16_t knob_gain(const knob_binding_t *b,uint16_t p){
    int32_t v=b->min_q8+((int64_t)(b->max_q8-b->min_q8)*p+32767)/65535;
    // Quarter-dB steps, with a small center detent at zero.
    if(abs(v)<=48)v=0;
    else v=v>=0?((v+32)/64)*64:-(((-v+32)/64)*64);
    if(v<b->min_q8)v=b->min_q8;
    if(v>b->max_q8)v=b->max_q8;
    return (int16_t)v;
}
bool knob_update(knob_state_t *s,const knob_binding_t *b,uint16_t p,int16_t current,int16_t *target){
    if(!b->module)return false;
    int16_t value=knob_gain(b,p);
    if(!s->seen||current!=s->observed){
        s->picked=false;s->previous=p;s->last_sent=value;s->observed=current;s->seen=true;
    }
    int16_t old=knob_gain(b,s->previous);
    if(!s->picked&&(abs(value-current)<=64||((old<=current&&value>=current)||(old>=current&&value<=current)))){
        s->picked=true;s->last_sent=value;
    }
    s->previous=p;s->observed=current;
    if(!s->picked||value==s->last_sent||value==current)return false;
    *target=value;return true;
}

bool knob_claim(knob_state_t *s,const knob_binding_t *b,uint16_t p,int16_t current,int16_t *target){
    *s=(knob_state_t){.seen=true,.picked=true,.previous=p,.observed=current,.last_sent=current};
    return knob_update(s,b,p,current,target);
}
