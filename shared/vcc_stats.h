#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct {uint32_t sum;uint16_t count,missed,min_mv; } vcc_stats_t;
static inline void vcc_stats_add(vcc_stats_t *s,bool valid,uint16_t mv){
 if(!valid){s->missed++;return;}
 if(!s->count||mv<s->min_mv)s->min_mv=mv;
 s->sum+=mv;s->count++;
}
static inline unsigned vcc_stats_mean(const vcc_stats_t *s){return s->count?(s->sum+s->count/2)/s->count:0;}
