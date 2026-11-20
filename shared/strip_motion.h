#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct {
 uint16_t anchor[5];uint8_t seeded;int8_t endpoint_zone[5];
 uint64_t last_motion,last_tick,endpoint_at,endpoint_until;
 bool moving,releasing,endpoint;
 double target,speed,release_speed;
} strip_motion_t;
void strip_motion_observe(strip_motion_t *s,uint64_t now,const uint16_t position[5],uint8_t valid);
double strip_motion_tick(strip_motion_t *s,uint64_t now,bool online);
double strip_motion_dim(const strip_motion_t *s,uint64_t now);
