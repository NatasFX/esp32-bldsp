#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct {unsigned attempts;uint64_t last_attempt;} vcc_recovery_t;
// Return 1=recreate ADC2, 2=restart C3, 0=wait/healthy.
static inline unsigned vcc_recovery_step(vcc_recovery_t *s,uint64_t uptime,bool valid){
 if(valid){s->attempts=0;s->last_attempt=0;return 0;}
 if(s->attempts&&uptime-s->last_attempt<5000)return 0;
 if(s->attempts>=2&&uptime<30000)return 0;
 s->last_attempt=uptime;
 return ++s->attempts<=2?1:2;
}
