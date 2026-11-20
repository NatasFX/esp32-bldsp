# Painel remoto ESP32-C3

O painel físico controla cinco ganhos pelo ESP-NOW criptografado. O S3 permanece responsável pelo USB do BP1048B2, presets e portal. OLED do C3 sem uso.

## Hardware e alimentação

- C3 ABrobot, flash de 4 MiB, USB Serial/JTAG COM8 quando conectado ao PC.
- S3 USB-UART COM7. Portal http://192.168.4.1/ na rede BLDSP-Control, senha equaliza123.
- Potenciômetros B10K: P1 GPIO0, P2 GPIO1, P3 GPIO2, P4 GPIO3, P5 GPIO4, todos ADC1.
- Por potenciômetro: 3V3 → resistor 3k3 → terminal alto; terminal baixo → GND; cursor → resistor 1k → ADC; capacitor 100 nF entre ADC e GND.
- A escala elétrica normalizada usa 2480 mV como fundo de escala. O ADC pode saturar antes de 3,3 V; nunca aplicar 12 V ao C3 ou aos GPIOs. Usar conversor regulado para alimentar o conjunto a partir do veículo.
- Fita real: WS2812 RGB, 22 LEDs, alimentação externa 5 V, GPIO7 via resistor 220–330 ohms para DIN e GND comum ao C3. Detalhes em [fita_ws2812.md](fita_ws2812.md).

## Enlace

ESP-NOW canal 6, pares conhecidos, PMK/LMK e criptografia. C3 MAC e8:3d:c1:8a:ee:78; S3 AP MAC 14:c1:9f:3b:10:65 (base 14:c1:9f:3b:10:64). Potência do C3 fixa em 2 dBm; S3 configurável no portal, padrão 2 dBm para evitar acoplamento audível. Não alterar o canal de apenas um dos dispositivos.

ADC é amostrado a cada 20 ms, média de 16 conversões por canal, calibração de fábrica quando disponível, filtro exponencial. Movimento envia posição no máximo a cada 100 ms; em repouso mantém heartbeat a cada 500 ms. O S3 considera enlace vencido após 1500 ms. Diagnósticos elétricos a cada 500 ms, suspensos durante OTA. A fita recebe status, configuração e contadores de CLIP pelo retorno do S3.

## Mapeamento e operação

Painel → Potenciômetros permite habilitar o painel, escolher função de cada pot e limites mínimo/máximo. Salvar grava configuração separada dos presets, na NVS do S3. Não é necessário usar o estéreo.

Padrão de funções: P1 pré-ganho do sub (efeito 4, módulo 0x93, seletor 1), faixa -60 a 0 dB. P2–P5 ganhos dos filtros 1–4 do efeito 2 (módulo 0x91, seletores 7, 12, 17, 22), faixa -12 a +12 dB. Outros filtros podem ser escolhidos no portal. Ganho das bandas aceita -24 a +12 dB. Um destino não pode ser atribuído a dois pots.

O painel nunca altera tipo, frequência ou Q, nem habilita EQ ou banda. Esses ajustes permanecem no celular. Bandas sem ganho não recebem comandos. Controles sem função continuam disponíveis no diagnóstico e na animação.

Na inicialização/conexão/reconexão do C3, os ganhos físicos assumem imediatamente, sem precisar cruzar os valores do preset. Após um ajuste pelo celular/preset durante o enlace, continua existindo soft takeover: o pot precisa alcançar ou cruzar o novo ganho para assumir novamente. Quantização de 0,25 dB e pequeno encaixe em zero. A redução por proteção de CLIP é separada do pré-ganho base e permanece aplicada.

## Sentido e margem dos extremos

Os cinco pots estão fisicamente invertidos: posição lógica é 65535 menos a posição elétrica normalizada. Portanto 0 V corresponde ao máximo. A inversão ocorre no S3 para controle e no C3 para animação; os pacotes e diagnósticos elétricos mantêm o sentido original.

Margem de extremos: posição lógica menor ou igual a 328/65535 (aproximadamente 0,5%) fixa em 0%; maior ou igual a 65207/65535 fixa em 100%. Ganhos, pickup e posição exibida usam o mesmo valor. A margem coincide com a indicação de extremo já presente nos LEDs do C3, sem atualizar seu firmware. RAW, mV e posição elétrica ficam intactos. Testes cobrem o teto observado de 99,7%, os limites inclusivos e monotonicidade em toda a faixa.

## Diagnóstico no portal e logs

A aba Painel mostra, para cada pot: percentual lógico, barra, posição 0–65535, RAW 0–4095, mV, disponibilidade de calibração, ganho solicitado, ganho confirmado e estado do pickup. Edições de mapeamento não salvas são preservadas durante as atualizações. Leituras vencidas ficam esmaecidas.

RAW maior ou igual a 4090 é indicado como ADC saturado. Caso contrário, tensão maior ou igual a 2480 mV indica limite da escala. Diagnóstico só é publicado se recente e do mesmo boot que as posições. O pacote adicional panel_adc_t usa tipo 6 e mantém a versão 2 do protocolo existente.

S3 registra panel_input e panel_adc a cada 2 s enquanto conectado. C3 registra LINK, máscara de canais e posições. Uma comparação anterior entre tensão intermediária no multímetro e ADC não teve conclusão definitiva; não foi aplicado fator arbitrário. Teste de extremos com P1/P3 em 3,3 V e P2 em 0 V confirmou os canais e sua resposta nos extremos.

## Sincronização e latência de ganho

A interface normaliza frequências de bandas desativadas antes de validar os snapshots do DSP. Isso evita descartar a atualização dos ganhos físicos quando uma banda retorna frequência zero. Edições locais pendentes continuam protegidas.

Telemetria no Equalizador/Painel a cada 200 ms após a resposta; Monitoramento mantém 1 s. Envio de edições agrega os últimos valores por 40 ms (antes 180 ms). S3 verifica posições a cada 20 ms (antes 100 ms); o intervalo de envio do C3 ainda limita o pior caso dos pots.

Alteração isolada de ganho usa comando escalar e leitura de confirmação. Pré-ganho preserva a sequência vizinho/alvo necessária ao ganho ao vivo. Mudanças estruturais usam envio completo. Se o escalar divergir na leitura, tenta o método completo antes de reportar falha. A API preserva a palavra desconhecida 2 pelo cache confirmado, lendo pelo USB se não existir cache. As operações USB continuam serializadas.

Meta de resposta próxima de 100 ms, sem garantia rígida: Wi-Fi, fila USB e confirmação acrescentam variação. Não foi realizado ensaio acústico de latência nessa etapa.

## Atualização e fita

C3 possui dois bancos OTA com rollback e confirmação de boot. Use /panel-update e imagem de aplicação bldsp_panel_c3.bin; detalhes em [ota_painel_c3.md](ota_painel_c3.md). Não enviar imagem do S3. Mantenha energia e enlace durante a atualização.

Rainbow gira à direita com enlace e à esquerda sem enlace. Movimento dos pots altera sentido e velocidade; espera 2 s após parar e retorna suavemente ao padrão em 3 s. Velocidade manual multiplicada por 2,5. Chegada aos extremos produz dois pulsos suaves até 20% do brilho configurado, sem rearmar por tempo. CLIP vermelho tem prioridade. Paletas e animações independentes são configuradas e salvas no S3. Oceano foi validada fisicamente, com tons de azul e verde.


## Autoridade na conexão do C3

O preset 1 continua aplicado pelo S3 no boot. Ao receber a primeira sessão do C3, após reiniciar o C3, ou após perda de enlace de pelo menos 1500 ms, os cinco destinos elegíveis recebem suas posições atuais, mesmo sem movimento. Desconexão não envia comandos nem reaplica preset: mantém os últimos valores. Reconexão aceita novamente os valores físicos, incluindo alterações feitas enquanto estava desconectado.

A aplicação aguarda USB/DSP e canais válidos. Um destino é confirmado por vez; falhas USB mantêm a aplicação pendente com nova tentativa após 1 s. EQ/bandas inativos e tipos sem ganho não são habilitados por esse fluxo. O controle mantém o limite, inversão, quantização e margem dos extremos já configurados. O painel físico precisa estar habilitado no portal.

Abrir/recarregar o portal não envia mais o preset 1: consulta o snapshot confirmado do S3 e acompanha a telemetria. Sem snapshot pronto, exibe o preset como base visual e aguarda o DSP, sem sobrescrever ganhos. Seleção explícita de preset pelo usuário continua funcionando. O modo de prévia mantém seu comportamento simulado.

Testes cobrem tomada imediata, ganho já igual, retry sem movimento, reconexão, tomada pelo celular após conexão e ausência de escritas ao iniciar o portal.
