#include "ws2812_encode.h"
#include <assert.h>
#include <stdio.h>
int main(void){uint8_t out[12];
 ws2812_encode(255,0,0,out);for(int i=0;i<12;i++)assert(out[i]==(i>=4&&i<8?0xee:0x88));
 ws2812_encode(0,255,0,out);for(int i=0;i<12;i++)assert(out[i]==(i<4?0xee:0x88));
 ws2812_encode(0,0,255,out);for(int i=0;i<12;i++)assert(out[i]==(i>=8?0xee:0x88));
 for(unsigned v=0;v<256;v++){ws2812_encode(v,v,v,out);for(unsigned ch=0;ch<3;ch++){unsigned decoded=0;for(unsigned bit=0;bit<8;bit++){unsigned nib=(out[ch*4+bit/2]>>(bit%2?0:4))&15;assert(nib==8||nib==14);decoded=(decoded<<1)|(nib==14);}assert(decoded==v);}}
 puts("WS2812: 24-bit GRB mapping and all byte values encoded to bounded SPI waveforms passed");return 0;}
