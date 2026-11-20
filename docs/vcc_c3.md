# VCC automotiva pelo ADC2 — experimento

GPIO5 (ADC2 canal 0) do C3, leitura única a cada segundo, tarefa independente dos cinco potenciômetros. Rádio/ESP-NOW permanecem ligados. Durante OTA a amostragem é suspensa. Nenhuma proteção ou ajuste de áudio usa esse dado.

## Ligação

VCC -> resistor 68 kΩ 1% -> GPIO5 -> dois resistores 10 kΩ 1% em paralelo (5 kΩ) -> GND do C3. Capacitor de 100 nF entre GPIO5 e GND, perto do ESP. A referência GND deve ser comum à alimentação medida. O divisor não protege contra transientes automotivos: sua proteção deve ser dimensionada antes de uso definitivo no carro. Não aplicar VCC diretamente ao GPIO.

Conversão atual: Ventrada = 31,109896348 × VADC − 51,442480079 (ajuste de quatro pontos, faixa conferida 8–13 V). Ela considera o pull-up equivalente da placa e os dois resistores inferiores em paralelo. Tensões fora de 8–13 V ainda são extrapolação. Faixa útil esperada do ADC: até cerca de 2,5 V, atenuação de 12 dB.

## Limitação conhecida

O ADC2 do ESP32-C3 tem resultados instáveis documentados pela Espressif. Uso forçado por CONFIG_ADC_ONESHOT_FORCE_USE_ADC2_ON_C3=y, somente para experimentar. Se a calibração de fábrica não estiver disponível, o portal identifica conversão aproximada (RAW × 2500/4095). Comparar RAW/mV e VCC com multímetro em vários pontos; não presumir precisão por obter ESP_OK.

Relatório ESP-NOW independente tipo 7: boot, sequência, validade, calibração, RAW, mV, erro e duração da conversão. S3 aceita dados recentes do mesmo boot do enlace; dados expirados ficam indisponíveis. Portal Painel mostra diagnósticos e log panel_vcc registra uma linha por segundo. Não há histórico persistido destas amostras.

Referência: https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32c3/api-reference/peripherals/adc_oneshot.html

## Primeiro envio em 20/11/2026

Testes nativos/Node e builds aprovados. OTA C3 confirmou novo boot/SHA/banco 0x200000; S3 normal restaurado. Cinco leituras retornaram ESP_OK e calibração disponível, duração 40 µs, RAW 4095 / 2938 mV constantes (saturação). Isso não valida a tensão: aguarda ligação do divisor e comparação com multímetro. Pots seguiram valid_mask=1f/picked_mask=1f.

## Compensação fixa do pull-up da placa

Divisor isolado conferido (~1 V em 7,96 V), mas GPIO5 alterava sua tensão. Medições com pull-down original de 10k: Vin=7,96 V / GPIO=2,469 V; Vin=10 V / GPIO=2,569 V (multímetro). Inclinação 0,1/2,04 e intercepto 2,0788039 V. Assumindo rede linear e 3V3 fixo, isso equivale a pull-up de cerca de 5,40k para ~3,37 V (equivalente ajustado, não medição física desses componentes). Os 6,11k medidos com a placa desligada incluem outros caminhos e não foram tratados como resistor isolado.

Adicionar outro 10k para GND altera a carga: a nova inversão é Vin=(1/a+68000/10000)*VGPIO-b/a, resultando 27,2*VGPIO-42,4076. Firmware usa mV calibrados do ADC, que diferiram do multímetro em 14–20mV nos pontos anteriores; a sensibilidade final é 0,0272 V de entrada por mV do ADC. Ainda requer conferência em dois pontos após instalar o segundo 10k. Não é calibração final nem proteção. C3 permanece igual; conversão e portal atualizados apenas no S3.

Após instalar o segundo 10k: usuário mediu 1,948 V no GPIO com Vin=10 V. Porém o ADC2 passou a retornar RAW=0/mV=0 com ESP_OK durante cinco leituras, duração 115–123 µs. A conversão não foi ajustada com esses dados inválidos. S3 rejeita RAW 0 e 4095, retorna volts=null e indica leitura inválida no portal. Precisa reiniciar o C3 e investigar repetibilidade; não há precisão validada nesta condição.

Após reiniciar o C3, leituras retornaram: 1978,1978,1973,1972,1978 mV (média 1975,8), 40–41 µs. Usuário confirmou entrada de 10 V e multímetro no GPIO de 1,948 V. Offset atualizado usando 1976 mV do ADC e Vin=10 V, mantendo inclinação estimada 27,2: Vin=27,2*VADC-43,7472. Validação de inclinação em segundo ponto pendente.

## Ajuste de quatro pontos após a montagem

Referência/valor mostrado pela fórmula anterior: 8/8,23; 11/10,84; 12/11,71; 13/12,61 V. Regressão linear: novo=1,1437461893*antigo-1,4067867880. Composta com a conversão anterior: novo=31,109896348*VADC-51,442480079. Resíduos nos pontos de ajuste: +0,0062; -0,0086; -0,0135; +0,0159 V. São resíduos do ajuste, não erro garantido de medição. Leitura amostrada após relato: 2071–2073mV, 40–45µs, compatível com ponto de 13V.

Usuário relatou nova saturação sem alterar a montagem, recuperada por reinício do C3. Isso mostra que o problema não pode ser atribuído somente à solda energizada. Calibração não corrige instabilidade do ADC2; os rails permanecem rejeitados, sem reset automático do painel.
