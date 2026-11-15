#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "protocol.h"
#include "fixture.h"
#include "../main/preset1_acp.h"
int main(void) {
    uint8_t frame[256];uint16_t words[53];
    assert(dsp_decode_eq(captured,sizeof(captured),words));
    assert(dsp_encode_eq(frame,words)==112);
    assert(!memcmp(frame,captured,sizeof(captured)));
    for(size_t i=sizeof(captured);i<256;i++)assert(frame[i]==0);
    assert((int16_t)words[1]==0);
    assert(words[28]==1 && words[29]==0 && words[30]==35 && words[31]==2048 && words[32]==640);
    for(size_t n=0;n<sizeof(captured);n++)assert(!dsp_decode_eq(captured,n,words));
    frame[111]=0;assert(!dsp_decode_eq(frame,256,words));
    frame[111]=0x16;frame[4]=0;assert(!dsp_decode_eq(frame,256,words));
    const uint8_t gain[]={1,0xe6,0xff};
    assert(dsp_frame(frame,0x93,gain,3)==8);
    const uint8_t expected[]={0xa5,0x5a,0x93,3,1,0xe6,0xff,0x16};
    assert(!memcmp(frame,expected,sizeof(expected)));
    assert(dsp_frame(frame,0,NULL,0)==5 && frame[4]==0x16);
    assert(!dsp_frame(frame,0,NULL,1));
    assert(!dsp_frame(frame,0,gain,252));
    assert(dsp_decode_eq(captured,sizeof(captured),words));
    assert(dsp_encode_eq_module(frame,0x91,words)==112);
    assert(frame[2]==0x91);
    assert(dsp_decode_eq_module(frame,256,0x91,words));
    assert(!dsp_decode_eq_module(frame,256,0x93,words));
    assert(!dsp_encode_eq_module(frame,0x92,words));
    for(int i=0;i<10;i++)if(words[5+5*i]<24)words[5+5*i]=24;
    assert(dsp_valid_eq_words(words));
    words[1]=3*256;assert(dsp_valid_eq_words(words));
    words[1]=3*256+1;assert(!dsp_valid_eq_words(words));
    words[1]=32768;assert(dsp_valid_eq_words(words));
    words[5]=23;assert(!dsp_valid_eq_words(words));words[5]=24;
    words[6]=1;assert(dsp_valid_eq_words(words));
    words[6]=10240;assert(dsp_valid_eq_words(words));
    words[6]=10241;assert(!dsp_valid_eq_words(words));
    words[6]=0;assert(!dsp_valid_eq_words(words));
    assert(dsp_valid_eq_words(acp_preset_sub));
    assert(dsp_valid_eq_words(acp_preset_stereo));
    assert(acp_preset_sub[30]==35 && acp_preset_sub[31]==2048 && acp_preset_sub[32]==640);
    const uint8_t captured_pregain[]={0xa5,0x5a,0x93,3,1,0xe6,0xff,0x16};
    assert(dsp_encode_eq_parameter(frame,0x93,1,(uint16_t)-26)==sizeof(captured_pregain));
    assert(!memcmp(frame,captured_pregain,sizeof(captured_pregain)));
    for(size_t i=sizeof(captured_pregain);i<sizeof(frame);i++)assert(frame[i]==0);
    assert(dsp_encode_eq_parameter(frame,0x91,1,0)==8 && frame[2]==0x91);
    assert(!dsp_encode_eq_parameter(frame,0x98,1,0));
    assert(!dsp_encode_eq_parameter(frame,0x93,2,0));
    assert(!dsp_encode_eq_parameter(frame,0x93,53,0));
    puts("Protocol: captured EQ round-trip, truncation, malformed frames, pregain and bounds OK");
    return 0;
}
