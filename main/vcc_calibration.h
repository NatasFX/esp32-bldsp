#pragma once
// 68k top, 10k||10k bottom, board pull-up retained.
// Least-squares fit of user reference -> OLD displayed voltage:
// 8->8.23, 11->10.84, 12->11.71, 13->12.61 V.
// Correction: new = 1.1437461893 * old - 1.4067867880.
// Old = 27.2 * ADC_volts - 43.7472. Validated fit range: 8..13V.
// Fit residuals do not establish accuracy or cure ADC2 hardware instability.
static inline double vcc_from_mv(unsigned mv){return 31.109896348*(mv/1000.)-51.442480079;}

// Rail readings cannot represent this biased divider in its intended range.
static inline int vcc_raw_valid(unsigned raw){return raw>0&&raw<4095;}
