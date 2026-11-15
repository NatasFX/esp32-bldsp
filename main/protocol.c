#include "protocol.h"
#include <string.h>
bool dsp_valid_eq_words(const uint16_t w[DSP_EQ_WORDS]) {
    // Signed Q8.8 minimum (-128 dB) represents the UI's bottom attenuation stop.
    if(w[0]>1 || (int16_t)w[1]>3*256)return false;
    for(int i=0;i<10;i++) {
        const uint16_t *b=w+3+i*5;
        if(b[0]>1 || b[1]>6 || b[2]<24 || b[2]>20000 || b[3]>10*1024 ||
           (int16_t)b[4]<-24*256 || (int16_t)b[4]>12*256)return false;
        if(b[0] && (b[2]<24 || b[3]<1))return false;
    }
    return true;
}
size_t dsp_frame(uint8_t *out, uint8_t module, const uint8_t *payload, size_t len) {
    if (len > DSP_REPORT_SIZE - 5 || (len && !payload)) return 0;
    memset(out, 0, DSP_REPORT_SIZE);
    out[0]=0xa5; out[1]=0x5a; out[2]=module; out[3]=(uint8_t)len;
    if (len) memcpy(out+4,payload,len);
    out[4+len]=0x16;
    return len+5;
}
bool dsp_valid_frame(const uint8_t *p, size_t n, uint8_t module) {
    return n>=5 && p[0]==0xa5 && p[1]==0x5a && p[2]==module &&
           (size_t)p[3]+5<=n && p[4+p[3]]==0x16;
}
bool dsp_decode_eq(const uint8_t *p, size_t n, uint16_t w[DSP_EQ_WORDS]) {
    return dsp_decode_eq_module(p,n,0x93,w);
}
bool dsp_decode_eq_module(const uint8_t *p, size_t n, uint8_t module, uint16_t w[DSP_EQ_WORDS]) {
    if ((module!=0x91 && module!=0x93) || !dsp_valid_frame(p,n,module) || p[3]!=107 || p[4]!=0xff) return false;
    for (int i=0;i<DSP_EQ_WORDS;i++) w[i]=p[5+2*i] | ((uint16_t)p[6+2*i]<<8);
    return true;
}
size_t dsp_encode_eq(uint8_t *p, const uint16_t w[DSP_EQ_WORDS]) {
    return dsp_encode_eq_module(p,0x93,w);
}
size_t dsp_encode_eq_module(uint8_t *p, uint8_t module, const uint16_t w[DSP_EQ_WORDS]) {
    if(module!=0x91 && module!=0x93)return 0;
    uint8_t payload[107]={0xff};
    for (int i=0;i<DSP_EQ_WORDS;i++) {payload[1+2*i]=(uint8_t)w[i]; payload[2+2*i]=(uint8_t)(w[i]>>8);}
    return dsp_frame(p,module,payload,sizeof(payload));
}
