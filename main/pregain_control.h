#pragma once
#include <stdint.h>
// Q8.8 signed pregain. Protection may only attenuate; never wrap below cut.
static inline uint16_t pregain_attenuated(uint16_t base,unsigned reduction_q8) {
    int32_t value=(int16_t)base-(int32_t)reduction_q8;
    return (uint16_t)(value<INT16_MIN?INT16_MIN:value);
}
