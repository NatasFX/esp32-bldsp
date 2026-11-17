# Sensores e monitoramento

O ESP acompanha os sensores mesmo sem navegador aberto. A interface mostra a temperatura do NTC e a estimativa da bobina separadamente; nenhuma leitura altera automaticamente o ganho.

| GPIO | Função |
|---|---|
| 17 | LDR sobre o LED CLIP do amplificador |
| 18 | NTC MF52AT 10 kΩ, B3950 |
| 2 | Seleção de fonte da BLDSP, saída open-drain |
| 48 | RGB de status e CLIP |

## Montagem

Divisor separado para cada sensor, com resistor fixo externo de 10 kΩ:

```text
3V3 do ESP ─ resistor 10 kΩ ─┬─ GPIO17 ou GPIO18
                            └─ LDR ou NTC ─ GND do ESP
```

A tensão cai quando o LDR recebe mais luz ou o NTC aquece. Os sensores ficam no lado de alimentação/GND do ESP. O LDR lê o LED opticamente, sem conexão aos terminais do amplificador. Fios curtos, fixação estável e proteção contra iluminação externa ajudam a evitar interferência. Não ligar os ADCs ao 5/12/16 V. Conferir isolamento do NTC em relação aos condutores de potência.

A entrada flutuante pode produzir números plausíveis; o software não detecta toda ligação defeituosa. A leitura de temperatura rejeita saturação e tensão fora da faixa de conversão.

## Aquisição e temperatura

ADC2 oneshot, média de até quatro conversões por canal e cadência nominal de 20 ms. Menos de duas conversões válidas não gera amostra válida. A temperatura usa a tensão calibrada do ADC, resistor fixo 10 kΩ, alimentação nominal 3300 mV e curva 10 kΩ/B3950.

O usuário validou o NTC contra o multímetro até aproximadamente 70 °C e conferiu a tensão do ADC. Isso valida a leitura do sensor nesse teste, não o acoplamento térmico à bobina.

A bobina estimada usa o observador `joint_v2` a aproximadamente 1 Hz: média móvel de dez temperaturas e filtro de 12 s. Não precisa informar potência, tensão ou corrente. O estado térmico equivalente da estrutura é dissipativo. Ambiente padrão 27,8 °C, ajustável em 0–60 °C e salvo em NVS; alterá-lo reinicia a estimativa. Leituras inválidas/lacunas reiniciam o observador. Iniciar com o falante quente exige acomodação e mantém ambiguidade sobre o calor residual. [Equações, erros e testes](ensaios/20261116/joint_v2/estudo_v2.md).

## CLIP

Referência móvel de 60 s separada da validação do pulso. Exige descida rápida, pelo menos duas amostras abaixo de 80% da referência e subida rápida; só então confirma um pulso curto. Após descida válida, mais de 30 amostras baixas consecutivas confirma evento sustentado sem esperar a recuperação. Formar a referência exige um minuto após o boot. A antiga calibração óptica/limiar absoluto foi removida da interface. [Detalhes do detector](clip_relativo.md).

O painel e o RGB retêm a indicação por 2 s. Esse tempo visual não é a duração real do CLIP nem mede a porcentagem de distorção. O RGB pisca vermelho a 4 Hz após confirmação, preservando prioridade de falhas e estabilização inicial.

## Histórico e API

- `GET /api/telemetry`: leitura do NTC, observador da bobina e estado/referência/contador de CLIP.
- `POST /api/telemetry/thermal`, com `X-BLDSP: 1` e `{"ambient_c":27.8}`: salva o ambiente em NVS e reinicia o observador.
- `GET /api/telemetry/history`: 300 amostras de monitoramento, cadência nominal de 1 s, e últimos 64 eventos na RAM.
- `GET /api/telemetry/ldr/capture`: últimos 2 s do buffer RAW de 128 amostras, com timestamps e referência capturada. Consulta somente ao clicar em Atualizar captura; traçado fica congelado.
- `POST /api/telemetry/ldr`: compatibilidade antiga, retorna 410 explicando a remoção do limiar fixo.

O navegador consulta o estado aproximadamente uma vez por segundo e o histórico a cada 5 s enquanto a aba está aberta. Trocar de aba não interrompe aquisição nem altera os EQs. Reabrir a página carrega o histórico já existente no ESP. Horários são relativos ao boot. Desligar/reiniciar perde os buffers, preservando ambiente e presets na NVS.

O gráfico térmico de cinco minutos é o histórico do NTC; a bobina tem cartão de estimativa atual. Mínima/média/máxima excluem amostras inválidas, e lacunas permanecem lacunas. Escala automática não equivale a limite de proteção. Respostas de histórico são copiadas sob exclusão mútua e transmitidas em blocos fora da seção crítica, sem árvore JSON por amostra.

Serial a 115200: `CLIP_RAW` por ciclo nominal de 20 ms, `THERMAL` e `ESTIMATE` a aproximadamente 1 Hz, resumo `sensors` a cada 5 s. Para analisar flashes, usar o RAW rápido e seus timestamps, não apenas o resumo.

## Verificação e limites

Testes C/JS cobrem aquisição/conversão, janelas limitadas, pulsos e luz lenta, observador, perda de leitura/conexão e interface sem alterar EQ. Dois ensaios DC serviram ao ajuste térmico; um teste curto adicional de maior potência foi relatado pelo usuário como próximo da câmera, sem erro quantificado. Não há validação geral com música, ventilação ou outra posição de NTC.

Não há compressor, mute, limite térmico ou redução automática de ganho por sensores. A política futura de proteção permanece no [roadmap](roadmap_telemetria_protecao.md). O brilho do RGB e a interface não substituem a validação do LED CLIP real nem proteções do amplificador.
