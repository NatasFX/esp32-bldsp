#pragma once
#include <stdbool.h>
#include <stdint.h>
#define LIGHT_MAX_STEPS 12
typedef struct {int16_t minute;uint8_t anchor,brightness;} light_step_t; // 0: midnight, 1: sunrise, 2: sunset
typedef struct {uint32_t version;uint8_t enabled,smooth,count,reserved;double latitude,longitude;light_step_t steps[LIGHT_MAX_STEPS];} light_config_t;
bool light_config_valid(const light_config_t *c);
bool light_solar(int day_of_year,int days_in_year,double lat,double lon,int offset,double *rise,double *set);
bool light_evaluate(const light_config_t *c,double minute,double rise,double set,unsigned *brightness);
