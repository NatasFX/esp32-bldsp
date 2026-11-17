#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    bool system_ready,usb_present,bluetooth_mode,eq_ready,working;
    bool source_error,bootstrap_error,eq_error;
    bool clip;
} status_snapshot_t;
typedef enum {LED_BOOT,LED_WAIT_USB,LED_SWITCHING,LED_SYNC,LED_ERROR,LED_READY,LED_CLIP} status_kind_t;
typedef struct {uint8_t r,g,b;} status_rgb_t;
status_kind_t status_visual_kind(const status_snapshot_t *s);
status_rgb_t status_visual_color(status_kind_t kind,uint64_t milliseconds);
status_rgb_t status_visual_startup_color(const status_snapshot_t *s,uint64_t now_ms,uint64_t ready_ms);
