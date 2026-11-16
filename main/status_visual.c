#include "status_visual.h"
status_kind_t status_visual_kind(const status_snapshot_t *s) {
    if(!s->system_ready)return LED_BOOT;
    if(!s->usb_present)return LED_WAIT_USB;
    if(s->source_error)return LED_ERROR;
    if(!s->bluetooth_mode)return LED_SWITCHING;
    if(s->bootstrap_error||s->eq_error)return LED_ERROR;
    if(!s->eq_ready||s->working)return LED_SYNC;
    return LED_READY;
}
status_rgb_t status_visual_color(status_kind_t kind,uint64_t ms) {
    // 128/255 maximum per channel: 50% command brightness, not measured optical power.
    unsigned phase=(unsigned)(ms%2000),level=16+(phase<1000?phase:2000-phase)*112/1000;
    switch(kind) {
        case LED_BOOT:return (status_rgb_t){0,0,(uint8_t)level};
        case LED_WAIT_USB:return (status_rgb_t){(uint8_t)level,(uint8_t)(level*2/3),0};
        case LED_SWITCHING:return (status_rgb_t){(uint8_t)(level*3/4),0,(uint8_t)level};
        case LED_SYNC:return (status_rgb_t){0,(uint8_t)level,(uint8_t)level};
        case LED_ERROR:return (status_rgb_t){ms%1000<500?128:0,0,0};
        case LED_READY: {
            unsigned hue=(unsigned)((ms%5120)*1536/5120),sector=hue/256;
            uint8_t up=(uint8_t)((hue%256)*128/255),down=128-up;
            switch(sector) {
                case 0:return (status_rgb_t){128,up,0};
                case 1:return (status_rgb_t){down,128,0};
                case 2:return (status_rgb_t){0,128,up};
                case 3:return (status_rgb_t){0,down,128};
                case 4:return (status_rgb_t){up,0,128};
                default:return (status_rgb_t){128,0,down};
            }
        }
    }
    return (status_rgb_t){0,0,0};
}
