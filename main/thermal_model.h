#pragma once
#include <stdbool.h>
typedef struct { double ambient,coil_delta,body_effect,ntc_delta; } thermal_model_t;
void thermal_model_init(thermal_model_t *s,double ambient);
void thermal_model_init_observed(thermal_model_t *s,double ambient,double coil,double ntc);
bool thermal_model_step(thermal_model_t *s,double voltage,double current_limit,bool powered,double seconds);
double thermal_model_current(const thermal_model_t *s,double voltage,double current_limit);
