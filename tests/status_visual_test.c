#include <assert.h>
#include <stdio.h>
#include "status_visual.h"
int main(void) {
    status_snapshot_t s={0};assert(status_visual_kind(&s)==LED_BOOT);
    s.system_ready=true;assert(status_visual_kind(&s)==LED_WAIT_USB);
    s.usb_present=true;assert(status_visual_kind(&s)==LED_SWITCHING);
    s.source_error=true;assert(status_visual_kind(&s)==LED_ERROR);s.source_error=false;
    s.bluetooth_mode=true;assert(status_visual_kind(&s)==LED_SYNC);
    s.eq_ready=true;assert(status_visual_kind(&s)==LED_READY);
    s.working=true;assert(status_visual_kind(&s)==LED_SYNC);s.working=false;
    s.eq_error=true;assert(status_visual_kind(&s)==LED_ERROR);s.eq_error=false;
    s.bootstrap_error=true;assert(status_visual_kind(&s)==LED_ERROR);s.bootstrap_error=false;
    s.usb_present=false;assert(status_visual_kind(&s)==LED_WAIT_USB);
    s.usb_present=true;assert(status_visual_kind(&s)==LED_READY);
    for(int kind=LED_BOOT;kind<=LED_READY;kind++)for(unsigned ms=0;ms<12000;ms++) {
        status_rgb_t c=status_visual_color((status_kind_t)kind,ms);
        assert(c.r<=128 && c.g<=128 && c.b<=128);
    }
    status_rgb_t start=status_visual_color(LED_READY,0),middle=status_visual_color(LED_READY,2560),end=status_visual_color(LED_READY,5120);
    assert(start.r==128 && start.g==0 && start.b==0);
    assert(middle.r==0 && middle.g==128 && middle.b==128);
    assert(start.r==end.r && start.g==end.g && start.b==end.b);
    status_rgb_t on=status_visual_color(LED_ERROR,0),off=status_visual_color(LED_ERROR,500);
    assert(on.r==128 && off.r==0 && on.g==0 && on.b==0);
    puts("RGB: operational gating, error priority, animation cycle and 50-percent channel cap OK");
    return 0;
}
