#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
typedef struct {double warn_v,critical_v,dip_warn_v,dip_critical_v;} battery_config_t;
typedef struct {double slow_sum,slow[30];unsigned next,count,warn_s,critical_s,clear_s,level;uint64_t last_ms,dip_until;unsigned dip_level;} battery_policy_t;
static inline bool battery_config_valid(const battery_config_t *c){return isfinite(c->warn_v)&&isfinite(c->critical_v)&&isfinite(c->dip_warn_v)&&isfinite(c->dip_critical_v)&&c->critical_v>=8&&c->warn_v<=14&&c->critical_v<c->warn_v&&c->dip_critical_v>=7&&c->dip_warn_v<=14&&c->dip_critical_v<c->dip_warn_v;}
static inline void battery_sample(battery_policy_t *s,const battery_config_t *c,uint64_t now,bool valid,double avg,double min){
 if(!valid||!isfinite(avg)||!isfinite(min)){*s=(battery_policy_t){0};return;}
 if(s->last_ms&&now-s->last_ms>2500)*s=(battery_policy_t){0};
 s->last_ms=now;
 if(s->count==30)s->slow_sum-=s->slow[s->next];else s->count++;
 s->slow[s->next]=avg;s->slow_sum+=avg;s->next=(s->next+1)%30;
 double mean=s->slow_sum/s->count;
 s->warn_s=mean<c->warn_v?s->warn_s+1:0;
 s->critical_s=mean<c->critical_v?s->critical_s+1:0;
 unsigned demand=s->count>=10&&s->critical_s>=5?2:s->count>=10&&s->warn_s>=10?1:0;
 if(demand>s->level){s->level=demand;s->clear_s=0;}
 if(mean>(s->level==2?c->critical_v:c->warn_v)+.2){if(++s->clear_s>=5){s->level=demand;s->clear_s=0;}}else s->clear_s=0;
 unsigned dip=min<c->dip_critical_v?2:min<c->dip_warn_v?1:0;
 if(now>=s->dip_until)s->dip_level=0;
 if(dip){if(dip>s->dip_level)s->dip_level=dip;s->dip_until=now+5000;}
}
static inline unsigned battery_level(const battery_policy_t *s,uint64_t now){if(!s->last_ms||now-s->last_ms>2500)return 0;unsigned dip=now<s->dip_until?s->dip_level:0;return s->level>dip?s->level:dip;}
