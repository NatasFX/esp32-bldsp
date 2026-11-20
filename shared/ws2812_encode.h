#pragma once
#include <stdint.h>
// SPI 3.2MHz, four SPI bits per WS2812 bit, GRB/MSB first.
static inline void ws2812_encode(uint8_t r,uint8_t g,uint8_t b,uint8_t out[12]){
 const uint8_t bytes[3]={g,r,b};
 for(unsigned i=0;i<12;i++){
  unsigned bit=i*2;uint8_t v=bytes[bit/8];
  out[i]=(uint8_t)(((v&(0x80>>(bit%8)))?0xe0:0x80)|((v&(0x40>>(bit%8)))?0x0e:0x08));
 }
}
