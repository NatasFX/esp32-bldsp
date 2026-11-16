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
    // No light until services are ready plus three seconds; then a one-second fade.
    for(unsigned ms=0;ms<8000;ms++){
        status_rgb_t c=status_visual_startup_color(&s,ms,5000);
        assert(c.r==0 && c.g==0 && c.b==0);
    }
    status_rgb_t beginning=status_visual_startup_color(&s,8000,5000);
    assert(beginning.r==0 && beginning.g==0 && beginning.b==0);
    status_rgb_t half=status_visual_startup_color(&s,8500,5000);
    assert(half.r<=64 && half.g<=64 && half.b<=64);
    status_rgb_t full=status_visual_startup_color(&s,9000,5000),expected=status_visual_color(LED_READY,9000);
    assert(full.r==expected.r && full.g==expected.g && full.b==expected.b);
    s.system_ready=false;
    status_rgb_t not_ready=status_visual_startup_color(&s,50000,0);
    assert(not_ready.r==0 && not_ready.g==0 && not_ready.b==0);
    puts("RGB: operational gating, error priority, animation cycle and 50-percent channel cap OK");
    return 0;
}
