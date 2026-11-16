# Sensores e aba Monitoramento

GPIO17: LDR sobre o LED CLIP. GPIO18: NTC genérico 10 kΩ. GPIO2 permanece reservado à seleção de fonte, GPIO48 ao RGB. Ambos os sensores são ligados à massa como braço inferior:

```text
3V3 do ESP ─ resistor fixo 10 kΩ ─┬─ entrada GPIO17 ou GPIO18
                                └─ LDR ou NTC ─ GND do ESP
```

Montar um divisor separado para cada sensor. Não ligar ao 5/12/16 V. Prever capacitor de 100 nF da entrada para GND junto ao ESP, fios curtos e isolamento do termistor em relação a condutores de potência. O LDR recebe luz do LED CLIP, sem ligação elétrica aos terminais do LED/amplificador. Fixar e proteger da iluminação ambiente. Fazer a montagem sem alimentação.

O termistor segue hipótese autorizada de 10 kΩ a 25 °C, Beta 3950. A conversão usa resistor fixo 10 kΩ e alimentação nominal 3300 mV; tolerâncias e curva desconhecida afetam a precisão. Todos os valores são estimativas até validar contra referência. Um sensor próximo da bobina mede o próprio ponto de contato e tem atraso. Não prender em parte móvel/entreferro, não usar sensor genérico além de sua faixa desconhecida e não interpretar sua leitura como temperatura direta do enrolamento.

A aquisição ADC2 oneshot opera em tarefa local, com média de até quatro conversões por canal e ciclo nominal de 20 ms. Leituras com menos de duas conversões válidas, sem calibração de tensão ou vencidas não viram temperatura válida. Saturação e tensão fora da faixa de conversão são mostradas sem temperatura; não é diagnóstico infalível de sensor desconectado. Entrada flutuante pode produzir números plausíveis: montar os divisores antes de confiar nas leituras.

GET `/api/telemetry` informa temperatura estimada/resistência/tensão/ADC, CLIP, referências e contagem. A interface consulta aproximadamente uma vez por segundo enquanto a aba está aberta. Trocar de aba não altera os EQs/presets nem interrompe a aquisição. Histórico: 300 amostras (cadência nominal de 1 s) e os últimos 64 eventos em buffers circulares na RAM do ESP, coletados mesmo sem navegador. GET `/api/telemetry/history` entrega snapshot cronológico; a página carrega ao abrir e consulta novamente a cada 5 s. Reiniciar o ESP limpa esse histórico, mas não as referências de calibração em NVS. Lacunas de leitura permanecem lacunas no gráfico. Horários são relativos ao boot, sem presumir relógio real/NTP.

## Calibração óptica

Na aba Monitoramento, capturar a referência do LED apagado e a do LED aceso. A segunda deve produzir ADC pelo menos 100 pontos abaixo da primeira. Pode usar luz externa controlada para verificar resposta do LDR antes de qualquer teste real de clipping; não provocar clipping forte/prolongado para obter referência. Validar depois o nível com o LED real, em ensaio controlado.

As referências são salvas em NVS somente ao capturar. Detecção usa ponto médio, histerese de 10% da diferença e duas amostras consecutivas para trocar estado. Contador acumula bordas de acendimento desde o boot e não representa duração nem porcentagem de distorção. O painel retém a indicação por 2 s para tornar um pulso visível; sem leitura recente não informa “sem CLIP”; modo manual provisório por limiar é identificado separadamente da calibração óptica.

## Sem atuação automática nesta entrega

Nenhum valor destes sensores altera ganho, mute ou limite térmico. A proposta futura registrada pelo usuário tolera pico isolado de ~50 ms e avalia redução gradual com clipping contínuo/recorrente por ~1 s. Essa política deve usar tempo real do sensor, nunca os 2 s retidos do indicador. Limites de temperatura, recuperação e intensidade da redução ainda não definidos.

Referência ADC2/Wi-Fi e erro de arbitragem: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/api-reference/peripherals/adc_oneshot.html

## Validação de bancada — 16/11/2026

Build completo ESP-IDF e testes C/JS passaram; firmware gravado na COM7 com hashes conferidos. ADC2 iniciou com Wi-Fi AP e USB ativos. Logs observaram GPIO17 em ~352–373 mV com conversões válidas e GPIO18 em 0 mV. Isto confirma aquisição, não montagem/calibração dos sensores. A conversão de temperatura recusa 0 mV e o CLIP permanece sem interpretação até obter referências válidas. EQs e seleção automática de fonte voltaram ao estado operacional. Layout em viewport móvel de 390 px foi renderizado e verificado sem overflow horizontal; testes exercitam abas, leituras ausentes, perda de conexão e preservação das edições de EQ.

Usuário confirmou usar resistor pull-up de 10 kΩ em ambos. Permanecem pendentes validação física do NTC em temperatura de referência e calibração do LDR contra o LED CLIP real. Nenhuma proteção automática por sensores foi habilitada.


## Histórico no ESP e gráfico técnico

A janela de histórico deixou de depender da sessão do navegador. O buffer de amostras e eventos ocupa menos de 16 KiB, sobrescreve os registros mais antigos ao encher e não grava continuamente na flash. A resposta HTTP é transmitida em blocos, sem montar uma árvore JSON grande na RAM. A captura é copiada sob exclusão mútua e serializada fora da seção crítica.

Eventos: início do monitoramento, CLIP acendendo/apagando, disponibilidade do NTC e LDR, captura das referências e falha inicial do ADC. Amostras incluem temperatura estimada ou ausência de leitura, tensão do LDR e estado filtrado de CLIP quando calibrado. A retenção visual de 2 s não é gravada como duração real do CLIP.

O gráfico usa eixo Y numérico em °C com limites inferior/superior explícitos, média tracejada e estatísticas mínima/média/máxima das amostras válidas dos últimos 5 minutos. Amostras ausentes não são convertidas em zero nem entram na média. A escala é automática; seus limites não são alarmes térmicos. Reabrir a página, trocar de celular ou alternar abas preserva os dados que já estavam no ESP; desligar/reiniciar o ESP limpa os buffers.

Validação do histórico no dispositivo em 16/11/2026: após build e gravação conferidos, logs de 30,676 s a 45,676 s de uptime mostraram history=30 → 35 → 40 → 45 e events=3, com NTC em 1629–1634 mV. A tarefa de aquisição acumula o buffer local sem depender da página. Testes C verificaram capacidade, sobrescrita e ordem cronológica; teste do front carregou eventos anteriores à abertura, excluiu amostras ausentes da média e preservou lacunas no traçado. Renderização em 390 px conferiu limites e linha média visível sem overflow horizontal. A lista de eventos tem horário relativo ao boot; o buffer continua sendo RAM e não persiste a reset.

## Limiar manual e captura RAW — 16/11/2026

A pedido do usuário, detecção provisória ativa por padrão com RAW < 450. Confirma em duas amostras consecutivas e libera em RAW ≥ 470 (histerese de 20 pontos). A aquisição continua nominalmente a cada 20 ms, média de até quatro conversões por canal; confirmação nominal ~40 ms mais resposta física do LDR. Esses tempos não são garantia de detectar duração exata de um pulso óptico.

Campo numérico e slider permitem alterar o limiar (1–4075). Salvar persiste em NVS e ativa modo manual; o limiar não muda apenas ao arrastar o slider. O gráfico mostra a linha em edição e sinaliza quando não está salva. Capturar as duas referências ópticas válidas muda para o modo calibrado; salvar um limiar volta ao manual. Tudo continua sem atuação sobre o volume.

O gráfico RAW foi separado do histórico térmico: buffer de 128 amostras rápidas na RAM do ESP e GET `/api/telemetry/ldr/capture`, filtrado pelos últimos 2000 ms. Só é consultado ao clicar em Atualizar captura. A imagem permanece congelada, inclusive ao editar o limiar. Escala inferior 0 e superior ajustada ao máximo observado, mantendo o limiar visível; slider acompanha essa escala e o campo numérico permite ampliá-la dentro de 1–4075. Mostra mínimo/máximo capturados e zona abaixo do limiar. Invalidade vira lacuna, não RAW zero.

A retenção visual de CLIP por 2 s continua independente da captura RAW e não representa duração do clipping. O histórico de temperatura permanece 1 Hz/5 minutos, com consulta periódica própria.

Validação após gravação: logs confirmaram CLIP manual=1, limit=450 e raw_window=128, com RAW atual do LDR ~2763–2802 e leitura NTC válida. Testes passaram para fronteira 449/450, liberação 469/470, sobrescrita do buffer rápido e acionamento da captura somente por botão. Teste de interface confirmou que editar limiar não consulta amostras novas e que salvar envia o valor escolhido. Layout conferido em 390 px com slider acompanhando a escala superior de 2500 na fixture isolada de QA. O limiar ainda não foi validado com o LED CLIP real; nenhuma ação automática de ganho foi habilitada.
