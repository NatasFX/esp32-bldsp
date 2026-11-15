# Catálogo dos blocos exportados

Extraído de `backup_live_20261114.c`. São 39 blocos, incluindo identificação de versão. Durante a restauração, 38 blocos completos foram observados na API HID com bytes idênticos ao exportado. Isso não comprova seletores individuais, unidades ou efeito acústico de cada campo.

Os offsets abaixo começam após `FF` (seletor de bloco completo). Todos os campos listados são palavras de 16 bits little-endian. Versão e USER_TAG têm formato especial e não são listados como palavras.

## 0x00 — Version

Bloco completo idêntico observado na API USB HID: **não**.

Comprimento do payload: `7`. Quadro completo: `a5 5a 00 07 30 00 00 04 01 23 01 16`.

## 0x01 — SYSTEM_CONTROL

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `13`. Quadro completo: `a5 5a 01 0d ff 00 00 00 00 01 00 07 00 01 00 00 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | system_mode | `00 00` | 0 | 0 |
| 2 | system_reset | `00 00` | 0 | 0 |
| 4 | system_sr_enable | `01 00` | 1 | 1 |
| 6 | system_sr_index | `07 00` | 7 | 7 |
| 8 | system_mclk_enable | `01 00` | 1 | 1 |
| 10 | system_mclk_index | `00 00` | 0 | 0 |

## 0x03 — PGA0

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `47`. Quadro completo: `a5 5a 03 2f ff 00 00 00 00 00 00 00 00 00 00 00 00 01 00 01 00 12 00 12 00 12 00 12 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 03 00 03 00 03 00 03 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | line1_left_enable | `00 00` | 0 | 0 |
| 2 | line1_right_enable | `00 00` | 0 | 0 |
| 4 | line2_left_enable | `00 00` | 0 | 0 |
| 6 | line2_right_enable | `00 00` | 0 | 0 |
| 8 | line4_left_enable | `00 00` | 0 | 0 |
| 10 | line4_right_enable | `00 00` | 0 | 0 |
| 12 | line5_left_enable | `01 00` | 1 | 1 |
| 14 | line5_right_enable | `01 00` | 1 | 1 |
| 16 | line1_left_gain | `12 00` | 18 | 18 |
| 18 | line1_right_gain | `12 00` | 18 | 18 |
| 20 | line2_left_gain | `12 00` | 18 | 18 |
| 22 | line2_right_gain | `12 00` | 18 | 18 |
| 24 | line4_5_left_gain | `00 00` | 0 | 0 |
| 26 | line4_5_right_gain | `00 00` | 0 | 0 |
| 28 | reserved | `00 00` | 0 | 0 |
| 30 | reserved | `00 00` | 0 | 0 |
| 32 | Diff_input_enable | `00 00` | 0 | 0 |
| 34 | Diff_left_gain | `00 00` | 0 | 0 |
| 36 | Diff_right_gain | `00 00` | 0 | 0 |
| 38 | Line1_left_right_pins | `03 00` | 3 | 3 |
| 40 | Line2_left_right_pins | `03 00` | 3 | 3 |
| 42 | Line4_left_right_pins | `03 00` | 3 | 3 |
| 44 | Line5_left_right_pins | `03 00` | 3 | 3 |

## 0x04 — ADC0

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `21`. Quadro completo: `a5 5a 04 15 ff 00 00 00 00 a3 0f a3 0f 07 00 00 00 00 00 00 00 00 00 00 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | mute | `00 00` | 0 | 0 |
| 4 | left_volume | `a3 0f` | 4003 | 4003 |
| 6 | right_volume | `a3 0f` | 4003 | 4003 |
| 8 | sample_rate | `07 00` | 7 | 7 |
| 10 | LR_swap | `00 00` | 0 | 0 |
| 12 | hpc | `00 00` | 0 | 0 |
| 14 | fade_time | `00 00` | 0 | 0 |
| 16 | mclk_source | `00 00` | 0 | 0 |
| 18 | dc_blocker | `00 00` | 0 | 0 |

## 0x06 — PGA1

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `19`. Quadro completo: `a5 5a 06 13 ff 00 00 00 00 10 00 00 00 10 00 00 00 00 00 01 00 01 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | line3_left_mic1_enable | `00 00` | 0 | 0 |
| 2 | line3_right_mic2_enable | `00 00` | 0 | 0 |
| 4 | line3_left_mic1_gain | `10 00` | 16 | 16 |
| 6 | line3_left_mic1_gain_boost | `00 00` | 0 | 0 |
| 8 | line3_right_mic2_gain | `10 00` | 16 | 16 |
| 10 | line3_right_mic2_gain_boost | `00 00` | 0 | 0 |
| 12 | mic_line_sel | `00 00` | 0 | 0 |
| 14 | line3_left_mic1_pin | `01 00` | 1 | 1 |
| 16 | line3_right_mic2_pin | `01 00` | 1 | 1 |

## 0x07 — ADC1

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `21`. Quadro completo: `a5 5a 07 15 ff 00 00 00 00 ff 0f ff 0f 07 00 00 00 00 00 00 00 00 00 00 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | mute | `00 00` | 0 | 0 |
| 4 | left_volume | `ff 0f` | 4095 | 4095 |
| 6 | right_volume | `ff 0f` | 4095 | 4095 |
| 8 | sample_rate | `07 00` | 7 | 7 |
| 10 | LR_swap | `00 00` | 0 | 0 |
| 12 | hpc | `00 00` | 0 | 0 |
| 14 | fade_time | `00 00` | 0 | 0 |
| 16 | mclk_source | `00 00` | 0 | 0 |
| 18 | dc_blocker | `00 00` | 0 | 0 |

## 0x08 — AGC1

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `29`. Quadro completo: `a5 5a 08 1d ff 00 00 00 00 00 00 00 00 00 00 00 00 0a 00 0a 00 0a 00 0a 00 00 00 05 00 00 00 00 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | mode | `00 00` | 0 | 0 |
| 2 | max_level | `00 00` | 0 | 0 |
| 4 | target_level | `00 00` | 0 | 0 |
| 6 | max_gain | `00 00` | 0 | 0 |
| 8 | min_gain | `00 00` | 0 | 0 |
| 10 | gain_offset | `00 00` | 0 | 0 |
| 12 | frame_time | `0a 00` | 10 | 10 |
| 14 | hold_time | `0a 00` | 10 | 10 |
| 16 | attack_time | `0a 00` | 10 | 10 |
| 18 | decay_time | `0a 00` | 10 | 10 |
| 20 | noise_gate_enable | `00 00` | 0 | 0 |
| 22 | noise_gate_threshold | `05 00` | 5 | 5 |
| 24 | noise_gate_mode | `00 00` | 0 | 0 |
| 26 | noise_gate_hold_time | `00 00` | 0 | 0 |

## 0x09 — DAC0

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `29`. Quadro completo: `a5 5a 09 1d ff 03 00 07 00 00 00 58 11 58 11 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `03 00` | 3 | 3 |
| 2 | sample_rate | `07 00` | 7 | 7 |
| 4 | mute | `00 00` | 0 | 0 |
| 6 | left_volume | `58 11` | 4440 | 4440 |
| 8 | right_volume | `58 11` | 4440 | 4440 |
| 10 | dither | `00 00` | 0 | 0 |
| 12 | scramble | `00 00` | 0 | 0 |
| 14 | mode | `00 00` | 0 | 0 |
| 16 | pause | `00 00` | 0 | 0 |
| 18 | sample_edge_mode | `00 00` | 0 | 0 |
| 20 | SCF_mute | `00 00` | 0 | 0 |
| 22 | fade_time | `00 00` | 0 | 0 |
| 24 | zero_num | `00 00` | 0 | 0 |
| 26 | mclk_source | `00 00` | 0 | 0 |

## 0x0A — DAC1

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `29`. Quadro completo: `a5 5a 0a 1d ff 01 00 07 00 00 00 58 11 05 08 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `01 00` | 1 | 1 |
| 2 | sample_rate | `07 00` | 7 | 7 |
| 4 | mute | `00 00` | 0 | 0 |
| 6 | left_volume | `58 11` | 4440 | 4440 |
| 8 | right_volume | `05 08` | 2053 | 2053 |
| 10 | dither | `00 00` | 0 | 0 |
| 12 | scramble | `00 00` | 0 | 0 |
| 14 | mode | `00 00` | 0 | 0 |
| 16 | pause | `00 00` | 0 | 0 |
| 18 | sample_edge_mode | `00 00` | 0 | 0 |
| 20 | SCF_mute | `00 00` | 0 | 0 |
| 22 | fade_time | `00 00` | 0 | 0 |
| 24 | zero_num | `00 00` | 0 | 0 |
| 26 | mclk_source | `00 00` | 0 | 0 |

## 0x0B — I2S0

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `23`. Quadro completo: `a5 5a 0b 17 ff 00 00 00 00 07 00 00 00 00 00 00 00 00 00 00 00 02 00 00 00 00 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | tx_enable | `00 00` | 0 | 0 |
| 2 | rx_enable | `00 00` | 0 | 0 |
| 4 | sample_rate | `07 00` | 7 | 7 |
| 6 | mclk_source | `00 00` | 0 | 0 |
| 8 | master_slave | `00 00` | 0 | 0 |
| 10 | word_length | `00 00` | 0 | 0 |
| 12 | stereo_mono | `00 00` | 0 | 0 |
| 14 | fade_time | `00 00` | 0 | 0 |
| 16 | data_format | `02 00` | 2 | 2 |
| 18 | bclk_invert | `00 00` | 0 | 0 |
| 20 | lrclk_invert | `00 00` | 0 | 0 |

## 0x0C — I2S1

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `23`. Quadro completo: `a5 5a 0c 17 ff 00 00 00 00 07 00 00 00 00 00 00 00 00 00 00 00 02 00 00 00 00 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | tx_enable | `00 00` | 0 | 0 |
| 2 | rx_enable | `00 00` | 0 | 0 |
| 4 | sample_rate | `07 00` | 7 | 7 |
| 6 | mclk_source | `00 00` | 0 | 0 |
| 8 | master_slave | `00 00` | 0 | 0 |
| 10 | word_length | `00 00` | 0 | 0 |
| 12 | stereo_mono | `00 00` | 0 | 0 |
| 14 | fade_time | `00 00` | 0 | 0 |
| 16 | data_format | `02 00` | 2 | 2 |
| 18 | bclk_invert | `00 00` | 0 | 0 |
| 20 | lrclk_invert | `00 00` | 0 | 0 |

## 0x0D — SPDIF

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `11`. Quadro completo: `a5 5a 0d 0b ff 00 00 07 00 00 00 00 00 00 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | sample_rate | `07 00` | 7 | 7 |
| 4 | channel_mode | `00 00` | 0 | 0 |
| 6 | io_mode | `00 00` | 0 | 0 |
| 8 | lock_status | `00 00` | 0 | 0 |

## 0xFC — USER_TAG

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `5`. Quadro completo: `a5 5a fc 05 ff 01 02 03 04 16`.

## 0x81 — 2:Music Noise Suppressor

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `11`. Quadro completo: `a5 5a 81 0b ff 00 00 9c e6 03 00 05 00 a0 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | threshold | `9c e6` | 59036 | -6500 |
| 4 | ratio | `03 00` | 3 | 3 |
| 6 | attack | `05 00` | 5 | 5 |
| 8 | release | `a0 00` | 160 | 160 |

## 0x82 — 2:Mic Reverb

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `15`. Quadro completo: `a5 5a 82 0f ff 00 00 64 00 5a 00 5a 00 4c 00 23 00 00 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | dry | `64 00` | 100 | 100 |
| 4 | wet | `5a 00` | 90 | 90 |
| 6 | width | `5a 00` | 90 | 90 |
| 8 | room | `4c 00` | 76 | 76 |
| 10 | damping | `23 00` | 35 | 35 |
| 12 | mono | `00 00` | 0 | 0 |

## 0x83 — 2:Music Voice Cut

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `5`. Quadro completo: `a5 5a 83 05 ff 00 00 64 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | value | `64 00` | 100 | 100 |

## 0x84 — 2:Music Voice Remove

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `7`. Quadro completo: `a5 5a 84 07 ff 00 00 c8 00 98 3a 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | low_frequency | `c8 00` | 200 | 200 |
| 4 | high_frequency | `98 3a` | 15000 | 15000 |

## 0x85 — 2:Music 3D

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `5`. Quadro completo: `a5 5a 85 05 ff 00 00 32 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | intensity | `32 00` | 50 | 50 |

## 0x86 — 2:Music Virtual Bass

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `9`. Quadro completo: `a5 5a 86 09 ff 00 00 46 00 14 00 00 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | cutoff_frequency | `46 00` | 70 | 70 |
| 4 | intensity | `14 00` | 20 | 20 |
| 6 | enhanced | `00 00` | 0 | 0 |

## 0x87 — 4:Rec Virtual Bass

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `9`. Quadro completo: `a5 5a 87 09 ff 00 00 41 00 14 00 01 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | cutoff_frequency | `41 00` | 65 | 65 |
| 4 | intensity | `14 00` | 20 | 20 |
| 6 | enhanced | `01 00` | 1 | 1 |

## 0x88 — 2:Music Virtual Bass Clas

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `7`. Quadro completo: `a5 5a 88 07 ff 00 00 1e 00 0a 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | cutoff_frequency | `1e 00` | 30 | 30 |
| 4 | intensity | `0a 00` | 10 | 10 |

## 0x89 — 4:Rec Virtual Bass Clas

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `7`. Quadro completo: `a5 5a 89 07 ff 00 00 23 00 0a 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | cutoff_frequency | `23 00` | 35 | 35 |
| 4 | intensity | `0a 00` | 10 | 10 |

## 0x8A — 2:Music Stereo Windener

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `5`. Quadro completo: `a5 5a 8a 05 ff 00 00 01 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | shaping | `01 00` | 1 | 1 |

## 0x8B — 2:Music Delay

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `9`. Quadro completo: `a5 5a 8b 09 ff 00 00 32 00 32 00 00 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | delay | `32 00` | 50 | 50 |
| 4 | max_delay | `32 00` | 50 | 50 |
| 6 | high_quality_enable | `00 00` | 0 | 0 |

## 0x8C — 4:Rec Delay

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `9`. Quadro completo: `a5 5a 8c 09 ff 00 00 32 00 32 00 00 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | delay | `32 00` | 50 | 50 |
| 4 | max_delay | `32 00` | 50 | 50 |
| 6 | high_quality_enable | `00 00` | 0 | 0 |

## 0x8D — 2:Music Exciter

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `9`. Quadro completo: `a5 5a 8d 09 ff 00 00 e8 03 32 00 32 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | cutoff_frequency | `e8 03` | 1000 | 1000 |
| 4 | dry | `32 00` | 50 | 50 |
| 6 | wet | `32 00` | 50 | 50 |

## 0x8E — 2:Music Phase

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `5`. Quadro completo: `a5 5a 8e 05 ff 00 00 00 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | phase_difference | `00 00` | 0 | 0 |

## 0x8F — 2:Music Drc

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `39`. Quadro completo: `a5 5a 8f 27 ff 00 00 64 00 02 00 d4 02 d4 02 00 00 00 00 00 00 64 00 64 00 64 00 01 00 01 00 01 00 e8 03 e8 03 e8 03 00 10 00 10 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | crossover frequency | `64 00` | 100 | 100 |
| 4 | mode | `02 00` | 2 | 2 |
| 6 | q1 | `d4 02` | 724 | 724 |
| 8 | q2 | `d4 02` | 724 | 724 |
| 10 | threshold1 | `00 00` | 0 | 0 |
| 12 | threshold2 | `00 00` | 0 | 0 |
| 14 | threshold3 | `00 00` | 0 | 0 |
| 16 | ratio1 | `64 00` | 100 | 100 |
| 18 | ratio2 | `64 00` | 100 | 100 |
| 20 | ratio3 | `64 00` | 100 | 100 |
| 22 | attack1 | `01 00` | 1 | 1 |
| 24 | attack2 | `01 00` | 1 | 1 |
| 26 | attack3 | `01 00` | 1 | 1 |
| 28 | release1 | `e8 03` | 1000 | 1000 |
| 30 | release2 | `e8 03` | 1000 | 1000 |
| 32 | release3 | `e8 03` | 1000 | 1000 |
| 34 | pregain1 | `00 10` | 4096 | 4096 |
| 36 | pregain2 | `00 10` | 4096 | 4096 |

## 0x90 — 4:Rec Drc

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `39`. Quadro completo: `a5 5a 90 27 ff 00 00 2c 01 01 00 d4 02 d4 02 00 00 00 00 00 00 64 00 64 00 64 00 01 00 01 00 01 00 e8 03 e8 03 e8 03 00 10 00 10 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | crossover frequency | `2c 01` | 300 | 300 |
| 4 | mode | `01 00` | 1 | 1 |
| 6 | q1 | `d4 02` | 724 | 724 |
| 8 | q2 | `d4 02` | 724 | 724 |
| 10 | threshold1 | `00 00` | 0 | 0 |
| 12 | threshold2 | `00 00` | 0 | 0 |
| 14 | threshold3 | `00 00` | 0 | 0 |
| 16 | ratio1 | `64 00` | 100 | 100 |
| 18 | ratio2 | `64 00` | 100 | 100 |
| 20 | ratio3 | `64 00` | 100 | 100 |
| 22 | attack1 | `01 00` | 1 | 1 |
| 24 | attack2 | `01 00` | 1 | 1 |
| 26 | attack3 | `01 00` | 1 | 1 |
| 28 | release1 | `e8 03` | 1000 | 1000 |
| 30 | release2 | `e8 03` | 1000 | 1000 |
| 32 | release3 | `e8 03` | 1000 | 1000 |
| 34 | pregain1 | `00 10` | 4096 | 4096 |
| 36 | pregain2 | `00 10` | 4096 | 4096 |

## 0x91 — 2:Music Pre Eq

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `107`. Quadro completo: `a5 5a 91 6b ff 00 00 00 00 03 00 01 00 01 00 3c 00 d4 02 00 06 01 00 00 00 f6 02 00 02 00 05 00 00 00 00 88 13 d4 02 00 00 00 00 00 00 10 27 d4 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00 c8 32 cd 00 00 05 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | pregain | `00 00` | 0 | 0 |
| 4 | calculation_type | `03 00` | 3 | 3 |
| 6 | filter1_enable | `01 00` | 1 | 1 |
| 8 | filter1_type | `01 00` | 1 | 1 |
| 10 | filter1_f0 | `3c 00` | 60 | 60 |
| 12 | filter1_Q | `d4 02` | 724 | 724 |
| 14 | filter1_gain | `00 06` | 1536 | 1536 |
| 16 | filter2_enable | `01 00` | 1 | 1 |
| 18 | filter2_type | `00 00` | 0 | 0 |
| 20 | filter2_f0 | `f6 02` | 758 | 758 |
| 22 | filter2_Q | `00 02` | 512 | 512 |
| 24 | filter2_gain | `00 05` | 1280 | 1280 |
| 26 | filter3_enable | `00 00` | 0 | 0 |
| 28 | filter3_type | `00 00` | 0 | 0 |
| 30 | filter3_f0 | `88 13` | 5000 | 5000 |
| 32 | filter3_Q | `d4 02` | 724 | 724 |
| 34 | filter3_gain | `00 00` | 0 | 0 |
| 36 | filter4_enable | `00 00` | 0 | 0 |
| 38 | filter4_type | `00 00` | 0 | 0 |
| 40 | filter4_f0 | `10 27` | 10000 | 10000 |
| 42 | filter4_Q | `d4 02` | 724 | 724 |
| 44 | filter4_gain | `00 00` | 0 | 0 |
| 46 | filter5_enable | `00 00` | 0 | 0 |
| 48 | filter5_type | `00 00` | 0 | 0 |
| 50 | filter5_f0 | `00 00` | 0 | 0 |
| 52 | filter5_Q | `00 00` | 0 | 0 |
| 54 | filter5_gain | `00 00` | 0 | 0 |
| 56 | filter6_enable | `00 00` | 0 | 0 |
| 58 | filter6_type | `00 00` | 0 | 0 |
| 60 | filter6_f0 | `00 00` | 0 | 0 |
| 62 | filter6_Q | `00 00` | 0 | 0 |
| 64 | filter6_gain | `00 00` | 0 | 0 |
| 66 | filter7_enable | `00 00` | 0 | 0 |
| 68 | filter7_type | `00 00` | 0 | 0 |
| 70 | filter7_f0 | `00 00` | 0 | 0 |
| 72 | filter7_Q | `00 00` | 0 | 0 |
| 74 | filter7_gain | `00 00` | 0 | 0 |
| 76 | filter8_enable | `00 00` | 0 | 0 |
| 78 | filter8_type | `00 00` | 0 | 0 |
| 80 | filter8_f0 | `00 00` | 0 | 0 |
| 82 | filter8_Q | `00 00` | 0 | 0 |
| 84 | filter8_gain | `00 00` | 0 | 0 |
| 86 | filter9_enable | `00 00` | 0 | 0 |
| 88 | filter9_type | `00 00` | 0 | 0 |
| 90 | filter9_f0 | `00 00` | 0 | 0 |
| 92 | filter9_Q | `00 00` | 0 | 0 |
| 94 | filter9_gain | `00 00` | 0 | 0 |
| 96 | filter10_enable | `01 00` | 1 | 1 |
| 98 | filter10_type | `00 00` | 0 | 0 |
| 100 | filter10_f0 | `c8 32` | 13000 | 13000 |
| 102 | filter10_Q | `cd 00` | 205 | 205 |
| 104 | filter10_gain | `00 05` | 1280 | 1280 |

## 0x92 — 2:Music Out Eq

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `107`. Quadro completo: `a5 5a 92 6b ff 01 00 00 00 05 00 01 00 04 00 1a 00 33 03 00 00 01 00 01 00 8c 00 00 04 00 0b 01 00 00 00 23 00 00 08 80 02 01 00 02 00 90 01 d4 02 00 f6 01 00 02 00 c4 09 3a 01 00 04 00 00 00 00 20 03 6e 00 00 f4 00 00 00 00 e8 03 d4 02 00 00 00 00 00 00 e8 03 d4 02 00 00 00 00 00 00 e8 03 d4 02 00 00 00 00 00 00 e8 03 d4 02 00 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `01 00` | 1 | 1 |
| 2 | pregain | `00 00` | 0 | 0 |
| 4 | calculation_type | `05 00` | 5 | 5 |
| 6 | filter1_enable | `01 00` | 1 | 1 |
| 8 | filter1_type | `04 00` | 4 | 4 |
| 10 | filter1_f0 | `1a 00` | 26 | 26 |
| 12 | filter1_Q | `33 03` | 819 | 819 |
| 14 | filter1_gain | `00 00` | 0 | 0 |
| 16 | filter2_enable | `01 00` | 1 | 1 |
| 18 | filter2_type | `01 00` | 1 | 1 |
| 20 | filter2_f0 | `8c 00` | 140 | 140 |
| 22 | filter2_Q | `00 04` | 1024 | 1024 |
| 24 | filter2_gain | `00 0b` | 2816 | 2816 |
| 26 | filter3_enable | `01 00` | 1 | 1 |
| 28 | filter3_type | `00 00` | 0 | 0 |
| 30 | filter3_f0 | `23 00` | 35 | 35 |
| 32 | filter3_Q | `00 08` | 2048 | 2048 |
| 34 | filter3_gain | `80 02` | 640 | 640 |
| 36 | filter4_enable | `01 00` | 1 | 1 |
| 38 | filter4_type | `02 00` | 2 | 2 |
| 40 | filter4_f0 | `90 01` | 400 | 400 |
| 42 | filter4_Q | `d4 02` | 724 | 724 |
| 44 | filter4_gain | `00 f6` | 62976 | -2560 |
| 46 | filter5_enable | `01 00` | 1 | 1 |
| 48 | filter5_type | `02 00` | 2 | 2 |
| 50 | filter5_f0 | `c4 09` | 2500 | 2500 |
| 52 | filter5_Q | `3a 01` | 314 | 314 |
| 54 | filter5_gain | `00 04` | 1024 | 1024 |
| 56 | filter6_enable | `00 00` | 0 | 0 |
| 58 | filter6_type | `00 00` | 0 | 0 |
| 60 | filter6_f0 | `20 03` | 800 | 800 |
| 62 | filter6_Q | `6e 00` | 110 | 110 |
| 64 | filter6_gain | `00 f4` | 62464 | -3072 |
| 66 | filter7_enable | `00 00` | 0 | 0 |
| 68 | filter7_type | `00 00` | 0 | 0 |
| 70 | filter7_f0 | `e8 03` | 1000 | 1000 |
| 72 | filter7_Q | `d4 02` | 724 | 724 |
| 74 | filter7_gain | `00 00` | 0 | 0 |
| 76 | filter8_enable | `00 00` | 0 | 0 |
| 78 | filter8_type | `00 00` | 0 | 0 |
| 80 | filter8_f0 | `e8 03` | 1000 | 1000 |
| 82 | filter8_Q | `d4 02` | 724 | 724 |
| 84 | filter8_gain | `00 00` | 0 | 0 |
| 86 | filter9_enable | `00 00` | 0 | 0 |
| 88 | filter9_type | `00 00` | 0 | 0 |
| 90 | filter9_f0 | `e8 03` | 1000 | 1000 |
| 92 | filter9_Q | `d4 02` | 724 | 724 |
| 94 | filter9_gain | `00 00` | 0 | 0 |
| 96 | filter10_enable | `00 00` | 0 | 0 |
| 98 | filter10_type | `00 00` | 0 | 0 |
| 100 | filter10_f0 | `e8 03` | 1000 | 1000 |
| 102 | filter10_Q | `d4 02` | 724 | 724 |
| 104 | filter10_gain | `00 00` | 0 | 0 |

## 0x93 — 4:Rec Pre EQ

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `107`. Quadro completo: `a5 5a 93 6b ff 01 00 00 00 05 00 01 00 04 00 1a 00 66 02 00 06 00 00 03 00 50 00 d4 02 00 00 01 00 01 00 8c 00 00 04 00 0b 01 00 02 00 90 01 d4 02 00 f6 01 00 02 00 c4 09 3a 01 00 04 01 00 00 00 23 00 00 08 80 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 20 00 00 04 00 03 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `01 00` | 1 | 1 |
| 2 | pregain | `00 00` | 0 | 0 |
| 4 | calculation_type | `05 00` | 5 | 5 |
| 6 | filter1_enable | `01 00` | 1 | 1 |
| 8 | filter1_type | `04 00` | 4 | 4 |
| 10 | filter1_f0 | `1a 00` | 26 | 26 |
| 12 | filter1_Q | `66 02` | 614 | 614 |
| 14 | filter1_gain | `00 06` | 1536 | 1536 |
| 16 | filter2_enable | `00 00` | 0 | 0 |
| 18 | filter2_type | `03 00` | 3 | 3 |
| 20 | filter2_f0 | `50 00` | 80 | 80 |
| 22 | filter2_Q | `d4 02` | 724 | 724 |
| 24 | filter2_gain | `00 00` | 0 | 0 |
| 26 | filter3_enable | `01 00` | 1 | 1 |
| 28 | filter3_type | `01 00` | 1 | 1 |
| 30 | filter3_f0 | `8c 00` | 140 | 140 |
| 32 | filter3_Q | `00 04` | 1024 | 1024 |
| 34 | filter3_gain | `00 0b` | 2816 | 2816 |
| 36 | filter4_enable | `01 00` | 1 | 1 |
| 38 | filter4_type | `02 00` | 2 | 2 |
| 40 | filter4_f0 | `90 01` | 400 | 400 |
| 42 | filter4_Q | `d4 02` | 724 | 724 |
| 44 | filter4_gain | `00 f6` | 62976 | -2560 |
| 46 | filter5_enable | `01 00` | 1 | 1 |
| 48 | filter5_type | `02 00` | 2 | 2 |
| 50 | filter5_f0 | `c4 09` | 2500 | 2500 |
| 52 | filter5_Q | `3a 01` | 314 | 314 |
| 54 | filter5_gain | `00 04` | 1024 | 1024 |
| 56 | filter6_enable | `01 00` | 1 | 1 |
| 58 | filter6_type | `00 00` | 0 | 0 |
| 60 | filter6_f0 | `23 00` | 35 | 35 |
| 62 | filter6_Q | `00 08` | 2048 | 2048 |
| 64 | filter6_gain | `80 02` | 640 | 640 |
| 66 | filter7_enable | `00 00` | 0 | 0 |
| 68 | filter7_type | `00 00` | 0 | 0 |
| 70 | filter7_f0 | `00 00` | 0 | 0 |
| 72 | filter7_Q | `00 00` | 0 | 0 |
| 74 | filter7_gain | `00 00` | 0 | 0 |
| 76 | filter8_enable | `00 00` | 0 | 0 |
| 78 | filter8_type | `00 00` | 0 | 0 |
| 80 | filter8_f0 | `00 00` | 0 | 0 |
| 82 | filter8_Q | `00 00` | 0 | 0 |
| 84 | filter8_gain | `00 00` | 0 | 0 |
| 86 | filter9_enable | `00 00` | 0 | 0 |
| 88 | filter9_type | `00 00` | 0 | 0 |
| 90 | filter9_f0 | `00 00` | 0 | 0 |
| 92 | filter9_Q | `00 00` | 0 | 0 |
| 94 | filter9_gain | `00 00` | 0 | 0 |
| 96 | filter10_enable | `00 00` | 0 | 0 |
| 98 | filter10_type | `00 00` | 0 | 0 |
| 100 | filter10_f0 | `20 00` | 32 | 32 |
| 102 | filter10_Q | `00 04` | 1024 | 1024 |
| 104 | filter10_gain | `00 03` | 768 | 768 |

## 0x94 — 4:Rec Out EQ

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `107`. Quadro completo: `a5 5a 94 6b ff 00 00 33 fe 00 00 00 00 00 00 2a 00 d4 02 5f 00 00 00 00 00 c8 00 d4 02 00 00 00 00 00 00 20 03 d4 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | pregain | `33 fe` | 65075 | -461 |
| 4 | calculation_type | `00 00` | 0 | 0 |
| 6 | filter1_enable | `00 00` | 0 | 0 |
| 8 | filter1_type | `00 00` | 0 | 0 |
| 10 | filter1_f0 | `2a 00` | 42 | 42 |
| 12 | filter1_Q | `d4 02` | 724 | 724 |
| 14 | filter1_gain | `5f 00` | 95 | 95 |
| 16 | filter2_enable | `00 00` | 0 | 0 |
| 18 | filter2_type | `00 00` | 0 | 0 |
| 20 | filter2_f0 | `c8 00` | 200 | 200 |
| 22 | filter2_Q | `d4 02` | 724 | 724 |
| 24 | filter2_gain | `00 00` | 0 | 0 |
| 26 | filter3_enable | `00 00` | 0 | 0 |
| 28 | filter3_type | `00 00` | 0 | 0 |
| 30 | filter3_f0 | `20 03` | 800 | 800 |
| 32 | filter3_Q | `d4 02` | 724 | 724 |
| 34 | filter3_gain | `00 00` | 0 | 0 |
| 36 | filter4_enable | `00 00` | 0 | 0 |
| 38 | filter4_type | `00 00` | 0 | 0 |
| 40 | filter4_f0 | `00 00` | 0 | 0 |
| 42 | filter4_Q | `00 00` | 0 | 0 |
| 44 | filter4_gain | `00 00` | 0 | 0 |
| 46 | filter5_enable | `00 00` | 0 | 0 |
| 48 | filter5_type | `00 00` | 0 | 0 |
| 50 | filter5_f0 | `00 00` | 0 | 0 |
| 52 | filter5_Q | `00 00` | 0 | 0 |
| 54 | filter5_gain | `00 00` | 0 | 0 |
| 56 | filter6_enable | `00 00` | 0 | 0 |
| 58 | filter6_type | `00 00` | 0 | 0 |
| 60 | filter6_f0 | `00 00` | 0 | 0 |
| 62 | filter6_Q | `00 00` | 0 | 0 |
| 64 | filter6_gain | `00 00` | 0 | 0 |
| 66 | filter7_enable | `00 00` | 0 | 0 |
| 68 | filter7_type | `00 00` | 0 | 0 |
| 70 | filter7_f0 | `00 00` | 0 | 0 |
| 72 | filter7_Q | `00 00` | 0 | 0 |
| 74 | filter7_gain | `00 00` | 0 | 0 |
| 76 | filter8_enable | `00 00` | 0 | 0 |
| 78 | filter8_type | `00 00` | 0 | 0 |
| 80 | filter8_f0 | `00 00` | 0 | 0 |
| 82 | filter8_Q | `00 00` | 0 | 0 |
| 84 | filter8_gain | `00 00` | 0 | 0 |
| 86 | filter9_enable | `00 00` | 0 | 0 |
| 88 | filter9_type | `00 00` | 0 | 0 |
| 90 | filter9_f0 | `00 00` | 0 | 0 |
| 92 | filter9_Q | `00 00` | 0 | 0 |
| 94 | filter9_gain | `00 00` | 0 | 0 |
| 96 | filter10_enable | `00 00` | 0 | 0 |
| 98 | filter10_type | `00 00` | 0 | 0 |
| 100 | filter10_f0 | `00 00` | 0 | 0 |
| 102 | filter10_Q | `00 00` | 0 | 0 |
| 104 | filter10_gain | `00 00` | 0 | 0 |

## 0x95 — 2:Music Out Gain

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `7`. Quadro completo: `a5 5a 95 07 ff 00 00 00 00 d2 23 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | mute | `00 00` | 0 | 0 |
| 4 | gain | `d2 23` | 9170 | 9170 |

## 0x96 — 4:Usb Out Gain

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `7`. Quadro completo: `a5 5a 96 07 ff 00 00 00 00 b2 3f 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | mute | `00 00` | 0 | 0 |
| 4 | gain | `b2 3f` | 16306 | 16306 |

## 0x97 — 2:I2s In Gain

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `7`. Quadro completo: `a5 5a 97 07 ff 00 00 00 00 00 10 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | mute | `00 00` | 0 | 0 |
| 4 | gain | `00 10` | 4096 | 4096 |

## 0x98 — 2:Bt In Gain

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `7`. Quadro completo: `a5 5a 98 07 ff 00 00 00 00 54 0b 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | mute | `00 00` | 0 | 0 |
| 4 | gain | `54 0b` | 2900 | 2900 |

## 0x99 — 2:Usb&Card In Gain

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `7`. Quadro completo: `a5 5a 99 07 ff 00 00 00 00 54 0b 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | mute | `00 00` | 0 | 0 |
| 4 | gain | `54 0b` | 2900 | 2900 |

## 0x9A — 2:Spdif In Gain

Bloco completo idêntico observado na API USB HID: **sim**.

Comprimento do payload: `7`. Quadro completo: `a5 5a 9a 07 ff 00 00 00 00 69 01 16`.

| Offset | Campo | Bytes | Sem sinal | Com sinal |
|---:|---|---|---:|---:|
| 0 | enable | `00 00` | 0 | 0 |
| 2 | mute | `00 00` | 0 | 0 |
| 4 | gain | `69 01` | 361 | 361 |

