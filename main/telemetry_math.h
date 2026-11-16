#pragma once
#include <stdbool.h>
bool telemetry_ntc_estimate(int millivolts,double *ohms,double *celsius);
bool telemetry_ldr_active(int raw,bool was_active,int threshold,int hysteresis);
