# VCC automotiva pelo ADC2 — experimento

GPIO5 (ADC2 canal 0) do C3, leitura única a cada segundo, tarefa independente dos cinco potenciômetros. Rádio/ESP-NOW permanecem ligados. Durante OTA a amostragem é suspensa. Nenhuma proteção ou ajuste de áudio usa esse dado.

## Ligação

VCC -> resistor 68 kΩ 1% -> GPIO5 -> resistor 10 kΩ 1% -> GND do C3. Capacitor de 100 nF entre GPIO5 e GND, perto do ESP. A referência GND deve ser comum à alimentação medida. O divisor não protege contra transientes automotivos: sua proteção deve ser dimensionada antes de uso definitivo no carro. Não aplicar VCC diretamente ao GPIO.

Fator nominal 7,8. Em 15 V, ADC recebe 1,923 V; em 16 V, 2,051 V. Faixa útil esperada do ADC: até cerca de 2,5 V, atenuação de 12 dB.

## Limitação conhecida

O ADC2 do ESP32-C3 tem resultados instáveis documentados pela Espressif. Uso forçado por CONFIG_ADC_ONESHOT_FORCE_USE_ADC2_ON_C3=y, somente para experimentar. Se a calibração de fábrica não estiver disponível, o portal identifica conversão aproximada (RAW × 2500/4095). Comparar RAW/mV e VCC com multímetro em vários pontos; não presumir precisão por obter ESP_OK.

Relatório ESP-NOW independente tipo 7: boot, sequência, validade, calibração, RAW, mV, erro e duração da conversão. S3 aceita dados recentes do mesmo boot do enlace; dados expirados ficam indisponíveis. Portal Painel mostra diagnósticos e log panel_vcc registra uma linha por segundo. Não há histórico persistido destas amostras.

Referência: https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32c3/api-reference/peripherals/adc_oneshot.html

## Primeiro envio em 20/11/2026

Testes nativos/Node e builds aprovados. OTA C3 confirmou novo boot/SHA/banco 0x200000; S3 normal restaurado. Cinco leituras retornaram ESP_OK e calibração disponível, duração 40 µs, RAW 4095 / 2938 mV constantes (saturação). Isso não valida a tensão: aguarda ligação do divisor e comparação com multímetro. Pots seguiram valid_mask=1f/picked_mask=1f.
