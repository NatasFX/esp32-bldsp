#include <assert.h>
#include <stdio.h>
#include "source_policy.h"
int main(void) {
    source_policy_t p={0};
    assert(!source_policy_due(&p,20000000)); // never pulse blindly at boot
    source_policy_connected(&p,SOURCE_USB_AUDIO_PID,1000000);
    assert(!source_policy_due(&p,1999999));
    assert(source_policy_due(&p,2000000));
    source_policy_pulsed(&p,2000000);
    assert(!source_policy_due(&p,6999999));
    source_policy_disconnected(&p);
    assert(!source_policy_due(&p,9000000));
    source_policy_connected(&p,SOURCE_BLUETOOTH_PID,2500000);
    assert(source_policy_target(&p));assert(p.attempts==0);
    assert(!source_policy_due(&p,9000000)); // expected change never triggers another pulse
    source_policy_disconnected(&p);assert(!source_policy_target(&p));
    source_policy_connected(&p,SOURCE_BLUETOOTH_PID,10000000);
    assert(!source_policy_due(&p,20000000)); // reconnect already in target
    source_policy_connected(&p,SOURCE_USB_AUDIO_PID,11000000);
    assert(!source_policy_due(&p,11999999));assert(source_policy_due(&p,12000000));
    source_policy_pulsed(&p,12000000);
    assert(source_policy_due(&p,17000000));source_policy_pulsed(&p,17000000);
    source_policy_disconnected(&p);source_policy_connected(&p,SOURCE_USB_AUDIO_PID,18000000);
    assert(!source_policy_due(&p,21999999));assert(source_policy_due(&p,22000000));
    source_policy_pulsed(&p,22000000);
    assert(!source_policy_due(&p,50000000)); // stop after three failed pulses
    source_policy_disconnected(&p);source_policy_connected(&p,SOURCE_USB_AUDIO_PID,51000000);
    assert(!source_policy_due(&p,53000000)); // reenumeration cannot bypass retry budget
    source_policy_connected(&p,SOURCE_BLUETOOTH_PID,54000000);
    source_policy_connected(&p,SOURCE_USB_AUDIO_PID,55000000);
    assert(source_policy_due(&p,56000000)); // later source regression gets fresh recovery budget
    source_policy_connected(&p,0xffff,60000000);assert(!source_policy_due(&p,90000000));
    source_policy_connected(&p,SOURCE_BLUETOOTH_PID,100000000);
    assert(source_policy_begin_cycle(&p,100000000));assert(!source_policy_target(&p));
    assert(!source_policy_begin_cycle(&p,100100000));assert(!source_policy_due(&p,101000000));
    source_policy_disconnected(&p);source_policy_connected(&p,SOURCE_BLUETOOTH_PID,101000000);
    assert(p.cycle_wait_usb&&!source_policy_target(&p));
    source_policy_disconnected(&p);source_policy_connected(&p,SOURCE_USB_AUDIO_PID,102000000);
    assert(!p.cycle_wait_usb);assert(!source_policy_due(&p,102500000));assert(source_policy_due(&p,103000000));
    source_policy_pulsed(&p,103000000);source_policy_connected(&p,SOURCE_BLUETOOTH_PID,104000000);
    assert(source_policy_target(&p)&&!source_policy_due(&p,110000000));
    puts("Source: USB-gated boot, stable delay, target confirmation, recovery and bounded retries OK");
    return 0;
}
