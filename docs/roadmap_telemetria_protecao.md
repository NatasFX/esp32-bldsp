# Roadmap — telemetria e proteção do sistema 2.1

Status: proposta de arquitetura. Sensores, circuitos de aquisição e proteções ainda não implementados nem validados. Não conectar saídas de potência diretamente ao ESP32.

## Objetivo e arquitetura

Manter música por Bluetooth na BP1048B2, controle USB pelo ESP32-S3 e uma página Monitoramento no mesmo front. Aquisição e decisões de proteção executam localmente no ESP, sem depender de navegador, Wi-Fi ou internet. A interface mostra medidas, qualidade da leitura e causa de cada intervenção.

Separar três fluxos: aquisição com timestamp e validação; supervisor de proteção; publicação de telemetria. A fila de comandos USB continua serializada e recebe prioridade para redução de ganho, sem permitir que uma edição atrasada restaure ganho durante proteção.

## 0. Confirmar hardware e isolamento antes dos sensores

Levantar modelo/esquema do amplificador, classe AB/D, saída simples/BTL/PBTL, faixa e transientes de VCC, potência/carga, acesso a FAULT/CLIP/MUTE/SD, posição de filtros LC e aterramento das entradas de áudio. Registrar esquema real com todos os caminhos de GND, blindagem, USB, fonte e computador de programação.

O B0505 1 W fornece uma alimentação isolada, se usado dentro das especificações; isso não é o mesmo que gerar um ponto médio de alimentação (terra virtual). O isolamento completo depende também dos demais cabos. A saída analógica da BLDSP para o amplificador pode estabelecer uma referência entre os domínios: verificar continuidade e topologia com tudo desligado. Não presumir que a alimentação isolada elimina todos os caminhos de terra.

ESP alimentado no lado de potência é possível e conveniente para medir VCC, temperatura e corrente da fonte. Porém, USB direto entre ESP e BLDSP une os GNDs e pode contornar a separação pretendida. Se a arquitetura exigir isolamento entre esses domínios, prever isolação USB de dados e alimentação/VBUS corretamente separados. Conferir velocidade real enumerada e compatibilidade host/HID; ADuM3160 é apenas referência de isolador low/full-speed, não uma seleção final de módulo. Não unir os 5 V dos dois lados nem criar caminho paralelo pela blindagem ou USB-UART do computador.

Wi-Fi e conversores do ESP podem injetar ruído conduzido/radiado mesmo sendo digitais. Usar regulador adequado, desacoplamento, caminhos de retorno controlados e separação física das entradas analógicas. Dimensionar fonte do ESP para picos de rádio; não assumir que o B0505 de 1 W pode alimentar o ESP junto com a BLDSP.

Critério de conclusão: diagrama de alimentação/terra aprovado por medição e ensaio de ruído com Wi-Fi/USB ligados e desligados.

## 1. MVP: tensão da fonte e temperatura

### VCC

- Medir no barramento do amplificador, com divisor resistivo dimensionado para a tensão máxima e transientes, filtro RC e proteção de entrada. Atenuação configurada do ADC não torna o pino tolerante a tensões de potência.
- Preferir ADC1 do S3 e driver de calibração do ESP-IDF. Confirmar pinos livres na placa; não fixar GPIO antes do levantamento. ADC externo pode melhorar precisão, mas não substitui proteção nem cria isolamento.
- Calibrar em pelo menos dois pontos contra multímetro; documentar erro, faixa, taxa e unidades. Exibir VCC atual, mínimo/máximo e queda sob carga.
- Amostragem inicial proposta de 100–1000 amostras/s para VCC, estatísticas locais e atualização visual de 5–10 Hz; ajustar após medir ruído e quedas reais. Transientes mais rápidos exigem detector/comparador ou aquisição apropriada.

### Temperatura

- NTC no dissipador do amplificador e outro no conjunto magnético/carcaça do falante, fixados em partes estacionárias com isolamento elétrico e acoplamento térmico conhecido. Evitar partes móveis, entreferro, suspensão e alteração mecânica do falante.
- Sensor próximo da bobina mede a temperatura de seu ponto de montagem, com atraso térmico; não equivale à temperatura real do enrolamento. Rotular no front como temperatura do ímã/carcaça ou sensor do falante.
- Definir resistência nominal, curva/Beta ou Steinhart–Hart, tolerância, montagem, faixa e corrente de excitação para limitar autoaquecimento. Detectar NTC aberto/curto e leitura fora da faixa.
- Amostrar inicialmente 5–10 Hz; mostrar também tendência em °C/min. Limites térmicos só após conhecer componentes e correlacionar sensor com aquecimento real.
- Temperatura da bobina por resistência DC é possibilidade futura, requer método específico: não deduzir resistência a partir de V/I de música nem medir ohms com amplificador acionado.

Critério de conclusão: medidas calibradas, desconexão/curto do sensor detectados e comparação térmica com instrumento de referência. Começar apenas monitorando, sem redução automática.

## 2. Saída de áudio e indicação de clipping

Tensão de saída é útil para nível, Vrms, picos e margem de excursão elétrica, mas sozinha não mede THD nem confirma distorção.

Em classe D, aproximação dos nós de chaveamento a VCC/GND é operação normal. Em BTL, nenhum terminal do falante deve ser tratado como GND. Medir diferencialmente entre os terminais, com atenuação, rejeição de modo comum, proteção e filtragem/antialias apropriadas. Considerar ringing e sobretensões, inclusive na instrumentação de bancada; não ligar terra de osciloscópio a uma saída BTL.

Ordem de preferência para alerta:

1. Usar CLIP/FAULT do amplificador se disponível, interpretando cada sinal conforme datasheet e adaptando nível elétrico. FAULT pode indicar temperatura/curto e não necessariamente clipping.
2. Medir áudio diferencial filtrado e VCC para estimar margem de saída, calibrada para aquele amplificador/carga. Considerar queda interna, modulação, configuração BTL e queda dinâmica da fonte; não usar um limiar universal igual ao rail.
3. Para confirmação mais forte, comparar entrada e saída com ganho/atraso compensados ou estudar achatamento/erro de forma de onda. THD exige aquisição, banda e método de análise próprios; ADC lento de telemetria não basta.

Para o sub, a banda menor facilita aquisição. Para estéreo até 20 kHz, escolher ADC com taxa, resolução, antialias e canais simultâneos adequados; taxas acima de Nyquist com margem de filtro. Não prometer ADC interno como analisador de distorção. Um ADC externo lento para VCC/NTC não serve automaticamente para áudio.

Interface: indicador CLIP por canal, contador e pico retido por 0,5–1 s como ponto inicial. Alerta visual por borda/banner pulsante lento, com opção de movimento reduzido; evitar piscar toda a tela a cada batida. Rotular estimativa como “margem baixa”/“possível clipping”; reservar “clip detectado” para detecção validada.

Critério de conclusão: comparar com osciloscópio diferencial/analisador e sinal de teste, validando falsos positivos com música e diferentes cargas em bancada.

## 3. Corrente e potência

Primeiro medir corrente DC da alimentação do amplificador: permite consumo, potência elétrica de entrada P = VCC × I e energia acumulada. Isso não é potência entregue ao falante. Pode usar shunt Kelvin com amplificador/monitor high-side compatível com VCC/transientes ou sensor Hall adequadamente especificado. A escolha depende de corrente, banda, perda permitida e necessidade real de isolamento.

Para um shunt: I = Vshunt/Rshunt e dissipação Pshunt = I²Rshunt. Dimensionar potência contínua/pulsos e faixa diferencial/modo comum. Preferir não levantar GND de potência com shunt low-side sem analisar os retornos. Usar sensoriamento Kelvin; não estimar corrente pela queda em fio/conector de resistência desconhecida.

Corrente do falante é uma fase separada: sinal bidirecional, saída possivelmente BTL/PWM, sensor com banda/modo comum adequados. Para potência real no falante, amostrar tensão e corrente alinhadas no tempo e calcular média de v(t) × i(t). Vrms²/R nominal é apenas estimativa resistiva; impedância do falante varia com frequência e fase.

Features possíveis: consumo em repouso/carga, Wh por sessão, detecção de consumo anormal, limitação por orçamento da fonte. Não afirmar curto/impedância/excursão só com VCC e corrente DC.

## 4. Supervisor de proteção

Estados propostos: normal → aviso → redução gradual → corte/falha travada. Configurar por sensor: limite, histerese, tempo de persistência e critérios de recuperação. Nenhum limite térmico/elétrico será pré-definido como seguro sem especificações e testes.

- Primeiro observar e registrar; depois habilitar aviso; por último testar intervenção em bancada.
- Temperatura elevada persistente: limitar gradualmente o ganho máximo; temperatura crítica: solicitar mute/corte conforme caminho validado. A tendência pode antecipar aviso, sem substituir limite absoluto.
- VCC baixa persistente: reduzir demanda e registrar queda. Sobretensão/curto exigem recursos de proteção do hardware, não apenas comandos USB.
- Clipping repetido: reduzir teto de ganho com ataque/recuperação apropriados; um pico isolado pode gerar só aviso. USB/Wi-Fi não são um limiter de áudio amostra a amostra e não impedem o primeiro pico.
- Usar limiter no DSP para picos rápidos, depois de mapear ordem dos blocos, limites e retenção. Supervisor térmico/de fonte ajusta o teto lentamente.
- Preservar valor desejado pelo usuário separadamente da atenuação de proteção. Conceito em dB: ganho aplicado = ganho desejado + atenuação de proteção (≤ 0), respeitando os limites codificáveis. Mostrar ambos; não regravar preset por intervenção.
- Integrar proteção à fila USB: prioridade, descarte de comandos antigos e reaplicação segura após reconexão. Boot não pode aplicar preset alto antes de avaliar sensores/estado de proteção. Conferir se bloquear ganho é suficiente ou se EQ com boosts exige outra estratégia.
- Sensor aberto/curto, amostra vencida, ADC saturado, USB indisponível e reset do ESP precisam de política explícita por função. Mostrar “indisponível”, nunca converter falha em 0 °C ou 0 A.
- Para corte confiável, estudar MUTE/SD do amplificador ou circuito dedicado em estado seguro por padrão. Não tratar pré-ganho −128 dB como desconexão elétrica. Falha travada pode exigir reconhecimento e condição segura antes de liberar, sem saltos de volume.
- Não substituir proteções nativas do amplificador/fonte, fusíveis ou dimensionamento térmico. Watchdog e brownout do ESP não garantem desligamento do estágio de potência.

Critério de conclusão: testes de sensores desconectados, valores fora da faixa, superaquecimento simulado, subtensão controlada, perda de USB/Wi-Fi, reset e reconexão. Verificar tempos e ausência de restauração inesperada de ganho.

## 5. Página Monitoramento e serviços

- Aba Monitoramento ao lado do Equalizador; manter check de sincronização do EQ com seu significado atual. Sensores têm estado próprio de validade e proteção, sem confundir USB confirmado com hardware saudável.
- Cards: VCC e mínimo, temperaturas e tendência, corrente/consumo quando disponíveis, CLIP/margem por canal e teto de ganho ativo.
- Gráficos de 1/5/30 minutos em buffer circular de RAM; mínimos/máximos por janela. Mostrar lacunas de leitura em vez de interpolar falhas como normalidade.
- Eventos com timestamp relativo ao boot e relógio do celular quando sincronizado, fonte/qualidade do horário e motivo/duração de cada intervenção. Exportação CSV/JSON.
- SSE ou WebSocket de telemetria a 5–10 Hz; aquisição e proteção independentes da taxa visual. Reconexão e timestamp/idade da última amostra.
- Configuração/calibração separada dos quatro presets de áudio: unidades, coeficientes, nomes, habilitação e políticas. NVS versionada só ao salvar; não escrever flash a cada amostra. Histórico longo por exportação ou armazenamento opcional com política de desgaste.
- Alertas locais no ESP por LED/buzzer opcional. Página fechada/em segundo plano não garante alarme no celular; notificações persistentes são evolução separada, sem dependência delas para proteção.
- Simulador de sensores no preview: VCC caindo, aquecimento, NTC aberto/curto, clip e USB perdido, sempre distinguindo ambiente simulado de hardware na documentação.

## Ordem de entrega

| Etapa | Entrega | Dependência |
|---|---|---|
| A | Diagrama de terras, USB/VBUS e pinos disponíveis | Modelo do amplificador e medições |
| B | VCC + NTCs calibrados, somente leitura | Circuito de aquisição definido |
| C | Página Monitoramento, gráficos e eventos | Leituras com qualidade/timestamp |
| D | Alertas térmicos e de fonte, sem intervenção | Limites e atraso térmico validados |
| E | Redução de ganho e corte local independente do front | Testes de falhas e caminho MUTE/SD |
| F | CLIP nativo ou aquisição diferencial do sub | Topologia e instrumentação confirmadas |
| G | Corrente da fonte e energia | Faixa e sensor dimensionados |
| H | V/I de áudio, potência real e diagnóstico avançado | Aquisição sincronizada adequada |

## Informações a levantar na próxima bancada

Modelo/fotos/esquema do amplificador e fonte; VCC nominal/máxima; carga nominal dos falantes e dados térmicos; local de montagem dos NTCs; caminhos atuais de GND/áudio e B0505; disponibilidade CLIP/FAULT/MUTE/SD; necessidade de monitorar L/R/sub; faixas de corrente. Isso define componentes, divisores, pinos e limites; ainda não há BOM ou esquema final.

## Referências técnicas

- ESP32-S3 ADC e calibração: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/adc/index.html e https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/adc_calibration.html
- Isolação USB low/full-speed, características e limites do ADuM3160: https://www.analog.com/en/products/adum3160.html
- Exemplo de amplificador classe D com saídas BTL e medição diferencial; não identifica o amplificador deste projeto: https://www.ti.com/lit/ds/symlink/tpa3123d2.pdf

## Dados de bancada informados — 2026-11-15

- Alimentação: máximo de operação informado pelo usuário de 16 V. A aquisição deve ter margem acima disso para detectar sobretensão e tolerar transientes; 16 V não é limite absoluto de dimensionamento do circuito de entrada.
- Amplificador: Stetsom EX3000, versão de 4 Ω; marca e impedância confirmadas pelo usuário. Referências técnicas abaixo são do EX3000 Black; conferir revisão exata na etiqueta antes de intervenções elétricas. Não aplicar dados de SounDigital 3000.1 por similaridade de potência.
- Para o Stetsom EX3000 Black 4 Ω, a ficha oficial informa alimentação 9–16 V, consumo musical 137 A e com sinal bass 275 A. Esses valores orientam estudo de faixa, não representam consumo medido deste sistema. Fonte: https://stetsom.com.br/pt/produto/ex3000-black-edition-4-ohms/
- O manual oficial do EX3000 Black descreve LEDs POWER, PROT e CLIP. Priorizar estudo de leitura óptica externa do CLIP/PROT por fototransistor protegido da luz ambiente, sem abrir o amplificador nem unir terras. Isso replica a indicação do amplificador; não mede THD. Aquisição elétrica dos LEDs só após conhecer circuito, níveis e isolamento. Fonte: https://stetsom.com.br/manual/manual_ex3000_black.pdf
- A faixa de 16 V é da entrada de alimentação, não da saída de áudio nem necessariamente dos rails internos. Como ilustração de dimensionamento, 3000 W sobre resistência de 4 Ω correspondem a aproximadamente 109,5 Vrms e 155 V de pico senoidal entre terminais. Não é medida nem garantia de tensão deste aparelho; evidencia que o divisor de VCC não serve para saída de alto-falante. Dimensionar aquisição de saída e instrumentação separadamente.
- Para corrente de alimentação nessa ordem de grandeza, estudar sensor Hall com faixa e banda apropriadas ou shunt de alta corrente com montagem Kelvin e dissipação calculada. Exemplo de perda, não seleção de componente: 0,1 mΩ a 275 A produz 27,5 mV e dissipa cerca de 7,6 W; um módulo comum de baixa corrente não atende.
- Próxima decisão: conferir revisão na etiqueta, carga realmente conectada (versão 4 Ω do amplificador não confirma impedância do falante), fonte e montagem dos sensores. Começar por VCC, temperaturas e indicação óptica de CLIP; adiar aquisição de áudio em alta tensão.

## Prioridade atual — LDR no CLIP e NTC, 16/11/2026

Sensores escolhidos pelo usuário: LDR sobre o LED **CLIP** (substitui a prioridade inicial de PROT) e NTC genérico 10 kΩ. Entradas desejadas: GPIO17 para LDR e GPIO18 para NTC, ambos como elementos inferiores do divisor para GND. Tensão cai quando luminosidade/temperatura aumenta. Resistor superior fixo previsto de 10 kΩ para 3V3 em cada divisor. Usar ADC2 oneshot com tratamento de falhas/arbitragem Wi-Fi; não parar Wi-Fi para medir.

NTC: assumir inicialmente R25=10 kΩ e Beta=3950, conforme autorização explícita do usuário. Temperatura deve ser rotulada estimada, usando também hipótese de 3,3 V e resistor de 10 kΩ. Validar em referência conhecida antes de qualquer proteção térmica. A medição corresponde ao ponto de montagem, não diretamente à bobina. Sem limite térmico automático nesta fase.

Finalidade futura do LDR: proteção adaptativa por persistência de clipping, sem estimar percentual de distorção. Especificação inicial do usuário: ocorrência isolada de aproximadamente 50 ms pode ser tolerada; LED continuamente aceso ou recorrente por aproximadamente 1 s deve qualificar uma futura redução gradual de volume. Definir ainda a ocupação mínima/quantidade de recorrências nessa janela, histerese, atenuação por passo, teto, recuperação lenta e comportamento com perda do sensor. Não interpretar dois pulsos quaisquer separados por 1 s como clipping sustentado.

A lógica de ganho NÃO deve ser implementada nesta entrega. Primeiro calibrar LDR com LED apagado/aceso, verificar luz ambiente, saturação e resposta temporal. A latência do LDR pode limitar precisão de pulsos de 50 ms. O estado retido por 2 s no painel serve só para visualização: não usá-lo para qualificar a janela de 1 s da futura proteção, pois transformaria um pico curto em falso clipping sustentado. A futura política deve usar amostras temporizadas/estado filtrado original.

A aba Monitoramento passa a mostrar temperatura estimada, histórico de cinco minutos da sessão, tensão/ADC, CLIP com retenção visual e contador, calibração óptica e eventos. Aquisição/validade não depende da página aberta; histórico (300 amostras a 1 Hz) e últimos 64 eventos ficam em buffers circulares na RAM do ESP, carregados ao abrir a página e limpos ao reiniciar. Não escrever flash a cada amostra. A redução adaptativa será supervisão lenta via DSP/USB, não um limiter de áudio para impedir o primeiro pico.
