# Sensores e aba Monitoramento

GPIO17: LDR sobre o LED CLIP. GPIO18: NTC genérico 10 kΩ. GPIO2 permanece reservado à seleção de fonte, GPIO48 ao RGB. Ambos os sensores são ligados à massa como braço inferior:

```text
3V3 do ESP ─ resistor fixo 10 kΩ ─┬─ entrada GPIO17 ou GPIO18
                                └─ LDR ou NTC ─ GND do ESP
```

Montar um divisor separado para cada sensor. Não ligar ao 5/12/16 V. Prever capacitor de 100 nF da entrada para GND junto ao ESP, fios curtos e isolamento do termistor em relação a condutores de potência. O LDR recebe luz do LED CLIP, sem ligação elétrica aos terminais do LED/amplificador. Fixar e proteger da iluminação ambiente. Fazer a montagem sem alimentação.

O termistor segue hipótese autorizada de 10 kΩ a 25 °C, Beta 3950. A conversão usa resistor fixo 10 kΩ e alimentação nominal 3300 mV; tolerâncias e curva desconhecida afetam a precisão. Todos os valores são estimativas até validar contra referência. Um sensor próximo da bobina mede o próprio ponto de contato e tem atraso. Não prender em parte móvel/entreferro, não usar sensor genérico além de sua faixa desconhecida e não interpretar sua leitura como temperatura direta do enrolamento.

A aquisição ADC2 oneshot opera em tarefa local, com média de até quatro conversões por canal e ciclo nominal de 20 ms. Leituras com menos de duas conversões válidas, sem calibração de tensão ou vencidas não viram temperatura válida. Saturação e tensão fora da faixa de conversão são mostradas sem temperatura; não é diagnóstico infalível de sensor desconectado. Entrada flutuante pode produzir números plausíveis: montar os divisores antes de confiar nas leituras.

GET `/api/telemetry` informa temperatura estimada/resistência/tensão/ADC, CLIP, referências e contagem. A interface consulta aproximadamente uma vez por segundo enquanto a aba está aberta. Trocar de aba não altera os EQs/presets nem interrompe a aquisição. Histórico: até cinco minutos em RAM do navegador, lacunas em leituras inválidas; eventos são da sessão.

## Calibração óptica

Na aba Monitoramento, capturar a referência do LED apagado e a do LED aceso. A segunda deve produzir ADC pelo menos 100 pontos abaixo da primeira. Pode usar luz externa controlada para verificar resposta do LDR antes de qualquer teste real de clipping; não provocar clipping forte/prolongado para obter referência. Validar depois o nível com o LED real, em ensaio controlado.

As referências são salvas em NVS somente ao capturar. Detecção usa ponto médio, histerese de 10% da diferença e duas amostras consecutivas para trocar estado. Contador acumula bordas de acendimento desde o boot e não representa duração nem porcentagem de distorção. O painel retém a indicação por 2 s para tornar um pulso visível; sem calibração ou leitura recente não informa “sem CLIP”.

## Sem atuação automática nesta entrega

Nenhum valor destes sensores altera ganho, mute ou limite térmico. A proposta futura registrada pelo usuário tolera pico isolado de ~50 ms e avalia redução gradual com clipping contínuo/recorrente por ~1 s. Essa política deve usar tempo real do sensor, nunca os 2 s retidos do indicador. Limites de temperatura, recuperação e intensidade da redução ainda não definidos.

Referência ADC2/Wi-Fi e erro de arbitragem: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/api-reference/peripherals/adc_oneshot.html

## Validação de bancada — 16/11/2026

Build completo ESP-IDF e testes C/JS passaram; firmware gravado na COM7 com hashes conferidos. ADC2 iniciou com Wi-Fi AP e USB ativos. Logs observaram GPIO17 em ~352–373 mV com conversões válidas e GPIO18 em 0 mV. Isto confirma aquisição, não montagem/calibração dos sensores. A conversão de temperatura recusa 0 mV e o CLIP permanece sem interpretação até obter referências válidas. EQs e seleção automática de fonte voltaram ao estado operacional. Layout em viewport móvel de 390 px foi renderizado e verificado sem overflow horizontal; testes exercitam abas, leituras ausentes, perda de conexão e preservação das edições de EQ.

Usuário confirmou usar resistor pull-up de 10 kΩ em ambos. Permanecem pendentes validação física do NTC em temperatura de referência e calibração do LDR contra o LED CLIP real. Nenhuma proteção automática por sensores foi habilitada.
