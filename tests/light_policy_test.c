#include "light_policy.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void){
 light_config_t c={.version=1,.enabled=1,.smooth=1,.count=5,.latitude=-30,.longitude=-53,.steps={{-1,1,15},{0,1,60},{720,0,100},{1020,0,60},{30,2,15}}};
 assert(light_config_valid(&c));double rise,set,other_rise,other_set;
 assert(light_solar(79,365,0,0,0,&rise,&set));assert(rise>350&&rise<375&&set>1080&&set<1100);
 assert(light_solar(324,365,-30,-53,-180,&rise,&set));assert(rise>310&&rise<350&&set>1130&&set<1180);
 assert(light_solar(324,365,-30,-53,0,&other_rise,&other_set));assert(fabs(other_rise-rise-180)<1e-9&&fabs(other_set-set-180)<1e-9);
 double winter_rise,winter_set;assert(light_solar(172,365,-30,-53,-180,&winter_rise,&winter_set));assert(set-rise>winter_set-winter_rise);
 assert(!light_solar(172,365,89,0,0,&other_rise,&other_set));
 unsigned b;assert(light_evaluate(&c,0,rise,set,&b)&&b==15);assert(light_evaluate(&c,rise-2,rise,set,&b)&&b==15);
 assert(light_evaluate(&c,rise,rise,set,&b)&&b==60);assert(light_evaluate(&c,720,rise,set,&b)&&b==100);assert(light_evaluate(&c,1020,rise,set,&b)&&b==60);
 assert(light_evaluate(&c,set+30,rise,set,&b)&&b==15);assert(light_evaluate(&c,1439,rise,set,&b)&&b==15);
 assert(light_evaluate(&c,(rise+720)/2,rise,set,&b)&&b==80);
 c.smooth=0;assert(light_evaluate(&c,800,rise,set,&b)&&b==100);assert(light_evaluate(&c,1020,rise,set,&b)&&b==60);
 c.steps[1]=c.steps[0];assert(!light_config_valid(&c));c.steps[1]=(light_step_t){0,1,101};assert(!light_config_valid(&c));
 c.steps[1]=(light_step_t){0,1,60};c.count=2;c.steps[0]=(light_step_t){720,0,15};c.steps[1]=(light_step_t){0,1,60};assert(!light_evaluate(&c,800,720,1100,&b)); // Resolved collision.
 puts("Lighting: solar seasons, timezone, polar fallback, midnight, anchors, interpolation, steps and validation passed");return 0;
}
