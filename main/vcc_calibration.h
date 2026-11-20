#pragma once
// Provisional equivalent circuit fitted from DMM readings BEFORE the extra 10k:
// Vin 7.96V -> pin 2.469V; Vin 10V -> pin 2.569V.
// Rtop=68k, Rbottom=10k||10k=5k, fixed board pull-up retained.
// Old slope a=0.100/2.04; adding 10k gives inverse slope 1/a+68k/10k.
// Post-change anchor: Vin=10V, ADC mean=1975.8mV, rounded to 1976mV.
// Preserve estimated slope; second post-change point still required.
static inline double vcc_from_mv(unsigned mv){return 27.2*(mv/1000.)-43.7472;}

// Rail readings cannot represent this biased divider in its intended range.
static inline int vcc_raw_valid(unsigned raw){return raw>0&&raw<4095;}
