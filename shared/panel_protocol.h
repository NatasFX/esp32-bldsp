#pragma once
#include <stdint.h>
#include <stdbool.h>
#define PANEL_MAGIC 0x50424c44u
#define PANEL_VERSION 2u
#define PANEL_COUNT 5
#define PANEL_CHANNEL 6
// Dedicated pair, not a general remote-control endpoint.
static const uint8_t panel_remote_mac[6]={0xe8,0x3d,0xc1,0x8a,0xee,0x78};
static const uint8_t panel_primary_mac[6]={0x14,0xc1,0x9f,0x3b,0x10,0x65};
static const uint8_t panel_pmk[16]={0x61,0x70,0x63,0x42,0x4c,0x44,0x53,0x50,0x50,0x41,0x4e,0x45,0x4c,0x30,0x30,0x31};
static const uint8_t panel_lmk[16]={0x39,0x88,0xe2,0x17,0x6c,0x54,0x90,0xba,0x22,0x48,0xc7,0x35,0xa1,0x0d,0x6e,0xf3};
typedef struct __attribute__((packed)) {
    uint32_t magic,boot,sequence;
    uint8_t version,type,valid_mask,reserved;
    uint16_t position[PANEL_COUNT]; // 0..65535
} panel_input_t;
typedef struct __attribute__((packed)) {
    uint32_t magic,sequence;
    uint8_t version,type,enabled,clip;
    uint8_t picked_mask,valid_mask,reserved[2]; // reserved[0]: LED palette ID (0=classic)
    int16_t gain_q8[PANEL_COUNT];
    uint32_t primary_boot,uptime_ms,clip_count,clip_low_ms;
    uint16_t led_count;
    uint8_t led_brightness,led_mode,led_enabled,sensor_ready;
    uint8_t battery_level,reserved_battery[3];
} panel_reply_t;

// Optional diagnostics; preserves the existing control/reply packet layout.
typedef struct __attribute__((packed)) {
    uint32_t magic,boot,sequence;
    uint8_t version,type,valid_mask,calibrated_mask;
    uint16_t raw[PANEL_COUNT],mv[PANEL_COUNT],position[PANEL_COUNT];
} panel_adc_t;

// Experimental GPIO5 / ADC2 voltage report, independent of pot packets.
typedef struct __attribute__((packed)) {
    uint32_t magic,boot,sequence;
    uint8_t version,type,valid,calibrated;
    uint16_t raw,mv;
    int32_t error;
    uint32_t duration_us,recoveries;
    uint8_t recovery_state,reserved[3]; // 0=normal, 1=ADC2 recreation, 2=C3 reboot pending
    uint16_t min_mv,samples,missed,clip_min_mv;
    uint32_t window_ms,clip_primary_boot,clip_count;
} panel_vcc_t;
