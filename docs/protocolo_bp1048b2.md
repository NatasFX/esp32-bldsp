# BP1048B2 — comandos observados nesta placa

Captura do ACPWorkbench 2.33.0, firmware V0.0.4, efeitos V1.35.1.
Bluetooth reproduzindo áudio; controle USB HID disponível no modo PID 171E (VID 8888).

## Rec Pre EQ (aba 4), pré-ganho

Confirmado por duas alterações manuais no ACP e captura de HidD_SetOutputReport.
Buffer da API Windows: 257 bytes, primeiro byte 00 (posição reservada ao Report ID), seguido do comando e preenchimento com zeros.
O tratamento do byte de Report ID no ESP-IDF ainda precisa ser validado com o descritor HID; não assumir que os 257 bytes são o payload USB bruto.

| Ajuste | Início do buffer capturado |
|---|---|
| -0,10 dB | `00 A5 5A 93 03 01 E6 FF 16` |
| 0,00 dB | `00 A5 5A 93 03 01 00 00 16` |
| Consulta do bloco, observada após escrita | `00 A5 5A 93 00 16` |

Interpretação consistente com exportação e captura: A5 5A = cabeçalho; 93 = módulo; 03 = tamanho de parâmetro+valor; 01 = seletor de pré-ganho; valor = int16 little-endian em dB × 256; 16 = terminador.
Para -0,10, o ACP gerou -26, isto é E6 FF. A regra exata de arredondamento para outros valores ainda não foi medida.

Não é necessário reenviar todo o equalizador para alterar o pré-ganho.
Não houve teste de envio independente pelo ESP32 nem gravação na flash.

## Sessão de mapeamento em 14/11/2026

O transporte observado é **USB HID**. O Bluetooth recebe música do celular; os comandos capturados não estão sendo enviados por Bluetooth.

Esta é uma engenharia reversa desta combinação de placa/firmware, não uma especificação universal da MVSilicon. Os exemplos são buffers da API Windows observados no ACP, não uma captura elétrica do barramento USB.

### Identificação HID confirmada

Obtida com `HidD_GetPreparsedData` e `HidP_GetCaps` no handle que o ACP usa para controle:

| Campo | Valor |
|---|---|
| VID | `8888` |
| PID no modo áudio + controle | `1719` |
| PID observado no modo somente HID | `171E` |
| Usage Page | `FF00` (fabricante) |
| Usage | `55AA` |
| InputReportByteLength | 257 |
| OutputReportByteLength | 257 |
| FeatureReportByteLength | 9 |

Os comprimentos incluem a posição reservada ao Report ID na API Windows. O ACP chama `HidD_SetOutputReport`, não um terminal serial USB. Para portar ao ESP32-S3, confirmar o descritor e o uso de SET_REPORT pela pilha; não presumir que escrever num endpoint de interrupção ou enviar 257 bytes brutos seja equivalente. O conteúdo completo do descritor HID ainda não foi obtido.

### Estrutura dos quadros

Sem o prefixo `00` específico do buffer Windows:

```text
A5 5A MODULO LEN PAYLOAD... 16
```

`LEN` conta bytes do payload. O tamanho do quadro é `LEN + 5`. Nos exemplos de escrita, o payload começa pelo seletor do parâmetro, seguido de valores little-endian de 16 bits. Payload vazio é uma consulta de módulo. `FF` seleciona escrita de um bloco inteiro.

Exemplos observados:

```text
00 A5 5A 00 00 16                consulta periódica do módulo 00
00 A5 5A 02 00 16                consulta periódica do módulo 02
00 A5 5A 93 00 16                consulta Rec Pre EQ
00 A5 5A 93 03 01 E6 FF 16       pré-ganho EQ -0,10 dB
```

O ACP preenche o restante do buffer de 257 bytes com zeros. Não apareceu checksum nos quadros observados. As respostas do dispositivo, confirmações, retentativas e temporizações ainda não foram decodificadas; a captura atual observa as chamadas de envio do ACP.

### Rec Pre EQ: frequência, Q, tipo, ganho e habilitação

Testes em `f5` (sexta banda da interface; `filter6` no arquivo INI): frequência 35 → 36 → 35 Hz; Q 2,000 → 2,100 → 2,000; todos os sete tipos; banda desligada e ligada; ganho restaurado para 2,50 dB.

| Atributo | Como o ACP enviou nesta sessão |
|---|---|
| Pré-ganho do módulo | Seletor individual `01` |
| Frequência da banda | Bloco completo `FF` |
| Q da banda | Bloco completo `FF` |
| Tipo da banda | Bloco completo `FF` |
| Ganho de f5 | Seletor individual `20` |
| Habilitação de f5 | Seletor individual `1C` |

Não foi demonstrado que comandos individuais de frequência/Q/tipo sejam aceitos. A forma reproduzível observada para eles é o bloco completo deste EQ, preservando os demais valores.

#### Bloco completo do EQ

```text
A5 5A 93 6B FF [53 palavras de 16 bits = 106 bytes] 16
```

Ordem das palavras após `FF`:

| Índice da palavra (base zero) | Campo | Conversão observada |
|---:|---|---|
| 0 | enable do módulo | 0/1 |
| 1 | pregain | dB × 256, int16 com sinal |
| 2 | calculation_type | preservar valor bruto; semântica pendente |
| 3 + 5n | f[n].enable | 0/1 |
| 4 + 5n | f[n].type | enum abaixo |
| 5 + 5n | f[n].frequency | Hz inteiro |
| 6 + 5n | f[n].Q | Q × 1024 |
| 7 + 5n | f[n].gain | dB × 256, int16 com sinal |

`n` vai de 0 a 9. São índices de palavras do bloco, não uma comprovação de que cada índice funciona como seletor individual. Os seletores individuais `1C` e `20` de f5 coincidem com esse padrão; extrapolar para outras bandas ainda exige teste.

Exemplos medidos: 36 Hz = `24 00`; Q 2,100 = 2150 = `66 08`; Q 2,000 = 2048 = `00 08`; ganho 2,50 dB = 640 = `80 02`.

```text
00 A5 5A 93 03 20 80 02 16      ganho f5 = +2,50 dB
00 A5 5A 93 03 1C 00 00 16      desabilitar f5
00 A5 5A 93 03 1C 01 00 16      habilitar f5
```

Durante um teste de entrada numérica, o ACP interpretou `2.4` como 24 dB e enviou `00 A5 5A 93 03 20 00 18 16`. Foi corrigido para 2,50 dB. Na UI local, selecionar todo o número e usar vírgula para decimais. Esse evento confirma a escala do ganho, mas não é um valor recomendado para uso.

#### Tipos confirmados em captura

| Valor | Sigla na UI | Tipo |
|---:|---|---|
| 0 | PK | Peak / paramétrico |
| 1 | LS | Low Shelf |
| 2 | HS | High Shelf |
| 3 | LP | Low Pass |
| 4 | HP | High Pass |
| 5 | BP | Band Pass |
| 6 | NH | Notch |

Todos foram selecionados em f5 e observados no bloco enviado. Não assumir os nove tipos de versões mais recentes do ACP: esta sessão mostrou sete.

O campo `calculation_type` mudou de 5 para 4 em uma sequência de habilitação. Não tratá-lo como um simples booleano Fixed/Floating sem mapear sua semântica. A restauração final devolveu o valor original 5.

### Volume / ganho da entrada Bluetooth: módulo 98

Confirmados em execução os seletores `00 = enable`, `01 = mute`, `02 = gain`.

| Ação | Início do buffer Windows capturado |
|---|---|
| Habilitar ganho | `00 A5 5A 98 03 00 01 00 16` |
| Desabilitar / bypass | `00 A5 5A 98 03 00 00 00 16` |
| Mute | `00 A5 5A 98 03 01 01 00 16` |
| Desmutar | `00 A5 5A 98 03 01 00 00 16` |
| -4 dB | `00 A5 5A 98 03 02 18 0A 16` |
| -3 dB | `00 A5 5A 98 03 02 54 0B 16` |

O ganho é amplitude linear normalizada por 4096, consistente com `round(4096 × 10^(dB/20))`. Exemplos: -4 dB → 2584; -3 dB → 2900; 0 dB → 4096. Os casos de arredondamento na fronteira ainda não foram estudados. Não usar a conversão dB × 256 do EQ neste bloco.

O backup tinha o módulo desabilitado. Após o teste, nível, mute e enable foram restaurados. Este ganho controla a entrada Bluetooth, não foi comprovado como volume exclusivo do sub ou como volume AVRCP do celular.

Os módulos 95 (Music Out Gain), 96 (Usb Out Gain), 97 (I2s In Gain), 99 (Usb&Card In Gain) e 9A (Spdif In Gain) têm o mesmo layout de bloco exportado: enable, mute e gain. Seus blocos completos também foram observados na restauração, mas seus seletores individuais e efeitos físicos não foram testados um a um.

### Rec Delay: módulo 8C

| Seletor | Função | Evidência |
|---|---|---|
| 00 | enable | ligado e desligado |
| 01 | delay, milissegundos | 49 e 50 ms |
| 02 | max_delay | posição no arquivo exportado; escrita individual não testada |
| 03 | high_quality_enable | 1 e 0 |

```text
00 A5 5A 8C 03 00 01 00 16     habilitar
00 A5 5A 8C 03 01 31 00 16     49 ms
00 A5 5A 8C 03 01 32 00 16     50 ms
00 A5 5A 8C 03 03 01 00 16     High Quality ligado
00 A5 5A 8C 03 03 00 00 16     High Quality desligado
00 A5 5A 8C 03 00 00 00 16     bypass
```

Max Delay e High Quality foram apresentados como opções configuráveis com o módulo desabilitado; não extrapolar sobre realocação de memória com ele em reprodução. O Music Delay (8B) possui layout completo equivalente, sem teste individual nesta sessão.

### Rec DRC: módulo 90

Ao contrário de um simples array de seletores por palavra, o DRC agrupa alguns parâmetros. O `LEN` inclui o seletor.

| Seletor | Valores enviados | LEN | Evidência |
|---|---|---:|---|
| 00 | enable | 03 | ligar/desligar capturado |
| 01 | frequência do crossover | 03 | 301 Hz = `2D 01` |
| 02 | modo | 03 | modos 0, 1, 2 e 3 capturados |
| 03 | Q LP, Q HP | 05 | `00 04 D4 02` = 1,000 e 0,707 |
| 04 | threshold1, threshold2, threshold3 | 07 | banda baixa e restauração |
| 05 | ratio1, ratio2, ratio3 | 07 | 99, 100, 100 |
| 06 | attack1, attack2, attack3 | 07 | 2, 1, 1 ms |
| 07 | release1, release2, release3 | 07 | 999, 1000, 1000 ms |
| 08 | pregain1 | 03 | -1 dB → `43 0E` |
| 09 | pregain2 | — | candidato por estrutura; não capturado individualmente |

```text
00 A5 5A 90 03 01 2D 01 16                      crossover 301 Hz
00 A5 5A 90 03 02 03 00 16                      modo 3
00 A5 5A 90 05 03 00 04 D4 02 16                Q LP/HP
00 A5 5A 90 07 04 9E FF 00 00 00 00 16          threshold baixo medido
00 A5 5A 90 07 04 00 00 00 00 00 00 16          thresholds zero
00 A5 5A 90 07 05 63 00 64 00 64 00 16          ratios
00 A5 5A 90 07 06 02 00 01 00 01 00 16          ataques
00 A5 5A 90 07 07 E7 03 E8 03 E8 03 16          releases
00 A5 5A 90 03 08 43 0E 16                      pré-ganho baixo
```

Q é consistente com ×1024 e pré-ganho com amplitude linear ×4096. Para threshold, digitar -1 dB produziu valor bruto -98 e a UI depois mostrou aproximadamente -0,97 dB. A conversão exata/quantização de threshold ainda precisa ser investigada; não presumir a escala do EQ nem prometer precisão centesimal. Os grupos de três mantêm posições para bandas inativas.

Modos selecionados e capturados: 0 = Full Band; 1 = 2 Band (Butter, order=1); 2 = 2 Band (L-R, order=4); 3 = 2 Band (Q, order=4). A lista também mostrou as três opções de 2 Band + Full Band; seus códigos ainda não foram confirmados por seleção.

Music DRC (8F) tem o mesmo layout completo exportado; equivalência dos seletores individuais ainda não foi testada.

## Catálogo ampliado e arquivos de evidência

- `catalogo_blocos_bp1048b2.md`: 39 blocos exportados e 444 campos de 16 bits, com offsets, nomes, bytes e valores do backup.
- `bp1048b2_export_catalog.json`: a mesma estrutura em formato legível por código.
- `build_protocol_catalog.py`: extração e validação do catálogo, sem acesso ao dispositivo.
- `acp_pregain_test.jsonl`: teste manual do pré-ganho.
- `acp_filters_test.jsonl`: testes de filtro, ganho Bluetooth e Rec Delay.
- `acp_other_effects.jsonl`: capacidades HID, DRC e restauração completa.
- `backup_live_20261114.ini/.c`: referência original.
- `novamente.ini/.c`: referência de -0,10 dB.
- `verificacao_restaurado_20261114.ini/.c`: exportação posterior à restauração.

Durante a restauração, 38 blocos completos foram observados na API HID e coincidiram byte a byte com o exportado, abrangendo também supressor de ruído, reverb, Voice Cut/Remove, 3D, Virtual Bass, Stereo Widener, Exciter, Phase, outros EQs e módulos de áudio. Isso comprova seus quadros completos, não unidades, seletores individuais nem efeito acústico de cada atributo. O módulo 00 é identificação de versão, não um ajuste a ser reproduzido indiscriminadamente.

## Restauração e interrupção do Bluetooth

Após os testes, foi importado o backup original pelo ACP. Todos os 38 blocos completos enviados na restauração coincidiram com o backup. Uma nova exportação INI foi comparada ao original: todos os parâmetros ficaram idênticos, excluindo apenas a data de criação. O ACP permaneceu CONNECTED. Não foi usado reset nem gravação na flash.

O usuário relatou desconexão espontânea do Bluetooth durante a sessão. A captura só observou envios USB; não monitorou eventos do rádio nem garante reprodução ininterrupta ao longo de todos os testes. A restauração completa também reenvia módulos de sistema, sample rate, relógios e interfaces, podendo interferir na reprodução; a causa exata não foi isolada. Alterações de modo do compressor e demais testes também não foram correlacionadas com eventos Bluetooth. Não atribuir a falha a um comando específico sem repetir um teste isolado.

Na enumeração Windows feita após o relato/restauração, o dispositivo estava novamente em VID 8888 / PID 1719, com áudio USB (MI_00) e duas interfaces HID (MI_03 e MI_04). Antes havia sido observado como PID 171E, somente HID, quando o usuário confirmou música Bluetooth. A mudança de apresentação USB é compatível com troca de modo, mas não demonstra qual teste a provocou nem se o vínculo Bluetooth foi encerrado ou apenas a fonte de áudio mudou.

**Correção de contexto do usuário:** ele informou depois que precisou desplugar e replugar a placa. Portanto, essa enumeração posterior em PID 1719 não pode ser usada para atribuir a troca de modo aos testes ou à importação do backup. A reconexão física é um fator de confusão conhecido; a causa da desconexão original continua sem isolamento.

Após o usuário selecionar novamente o modo, uma nova enumeração confirmou VID 8888 / PID 171E, somente HID. Nenhum comando de alteração foi enviado pelo agente após essa confirmação; a captura já havia sido encerrada.

Para o ESP32, começar com comandos do módulo 93 e evitar reaplicar toda a configuração de sistema durante a música. O teste de reconectar Bluetooth mantendo USB e uma sessão longa apenas de EQ ainda está pendente.

## Próximas validações

1. Capturar descritor HID completo e respostas; conferir SET_REPORT/Report ID, erros, timeouts e sequência de inicialização.
2. Testar no ESP32-S3 com envio independente, começando por uma consulta e pré-ganho pequeno.
3. Isolar desconexão Bluetooth com teste prolongado exclusivamente de EQ, sem importação global.
4. Mapear semântica de calculation_type e seletores individuais de frequência/Q/tipo, se existirem.
5. Confirmar demais seletores de volume, roteamento físico do sub, threshold DRC e modos restantes.
6. Mapear seletores/unidades dos outros efeitos individualmente. O catálogo de blocos não substitui esses testes.

## Troca de modo USB / botão físico (investigação)

O usuário confirmou que a seleção do modo somente HID foi feita por **botão físico na placa**, não por comando do ACPWorkbench. Portanto, as capturas do aplicativo até aqui não contêm um comando comprovado equivalente a esse botão.

O bloco `0x01 SYSTEM_CONTROL` possui `system_mode`, mas seu significado não foi confirmado. O nome isolado não comprova seleção de fonte Bluetooth/USB nem mudança entre PID 1719 e 171E. A interface inspecionada oferece Sleep, Reset, sample rate e MCLK; não foi identificada uma opção explícita de troca áudio USB/somente HID. Não foram enviados valores experimentais a `system_mode` nesta investigação.

Próximo caminho: identificar a ligação do botão (GPIO, rede resistiva ADC ou outra) e investigar se o firmware expõe comando equivalente. Monitorar apenas os envios do ACP não revela uma ação física que ele não executa. Se necessário, o ESP32 poderá simular o contato por circuito adequado à ligação medida; não conectar uma saída GPIO diretamente antes de conhecer tensão e topologia. O botão pode ciclar modos, em vez de selecionar um estado absoluto; verificar inicialização e sequência antes de automatizar.

## Save executado pelo usuário — captura isolada

Arquivo: `acp_save_test.jsonl`. Captura encerrada após o usuário confirmar a operação. Foram observados estes dois envios especiais, além das consultas periódicas dos módulos 00 e 02:

```text
00 A5 5A FC 0D FF 01 02 03 04 32 33 34 35 36 37 38 39 16
00 A5 5A FD 00 16
```

Ambos via `HidD_SetOutputReport`, buffers de 257 bytes com padding zero. O 00 inicial pertence ao formato da API Windows, conforme ressalvas de transporte anteriores. Intervalo observado entre as chamadas: aproximadamente 61 ms; não é requisito de temporização comprovado.

O módulo FC já foi identificado no exportado como USER_TAG. Neste save seu payload tem 13 bytes: FF seguido de 12 bytes de dados, incluindo ASCII `23456789` ao final. Não assumir que esses dados sejam chave mágica fixa de gravação: podem ser metadados/tag específicos desta operação.

O quadro `A5 5A FD 00 16` é o **candidato a comando de persistência**, por ter sido enviado imediatamente após USER_TAG durante o save solicitado. Não houve reenvio de todos os blocos de EQ nesta captura: a sequência é compatível com gravar a configuração que já está em execução. Isso é inferência da sequência observada, ainda não confirmação por replay ou leitura de flash.

Pendências: verificar retenção após desligar/ligar, capturar respostas/sucesso, repetir save para distinguir tag variável e verificar se FC é obrigatório ou se FD basta. A captura observa chamadas de envio, não retorno da API nem confirmação da placa. Não repetir gravações automaticamente em cada ajuste de volume/EQ enquanto a estratégia de persistência não estiver definida.

## Primeiro envio direto de ajuste

Executado `test_eq_direct.py` a pedido do usuário: envio único de `00 A5 5A 93 03 01 E6 FF 16`, com padding até 257 bytes, pelo handle HID ativo do ACP. O script aguarda uma consulta periódica identificável antes de usar o handle e se desconecta após a chamada. `HidD_SetOutputReport` retornou sucesso. Registro: `eq_direct_test_result.jsonl`.

Ajuste solicitado: Rec Pre EQ, aba 4, pré-ganho -0,10 dB. Nenhum comando FC/FD de save enviado. Sucesso da API não substitui confirmação do valor lido da placa; a leitura/visualização do parâmetro continua pendente neste teste. O envio usa o processo ACP como transporte, sem clicar no controle de ganho; ainda não é execução por ESP32 nem host independente.

## Investigação Bluetooth da BLDSP no PC

Adaptador encontrado: Realtek Bluetooth 5.0 Adapter. Dispositivo Classic BLDSP identificado no Windows em `CC:9C:85:03:11:01`, com transportes AVRCP enumerados. Ferramenta criada: `inspect_bldsp.py`; resultados em `bldsp_discovery.jsonl`.

Duas buscas BLE de 12 segundos não encontraram anúncio com nome BLDSP nem com o endereço Classic conhecido. Consulta RFCOMM não cacheada via WinRT retornou BluetoothError.SUCCESS (0), com lista de serviços vazia, nas duas execuções. A consulta direta BluetoothLEDevice para o endereço Classic não retornou dispositivo. Isso não exclui BLE com outro endereço/nome ou disponível apenas em outro estado/modo.

A COM11 encontrada no Windows tem endereço remoto zerado em sua instância; não foi identificada como porta de saída da BLDSP, portanto não foi usada para envio. Os serviços AVRCP observados não comprovam acesso ao EQ.

Resultado: nenhum canal de dados SPP/GATT para ajustes foi localizado no estado atual. Nenhum comando EQ nem save foi enviado por Bluetooth, pois não foi identificado um transporte de controle. A disponibilidade do rádio no chip não comprova que este firmware exponha controle de DSP. Investigações futuras podem repetir a busca com o celular desconectado ou em modo de pareamento; não afirmar impossibilidade definitiva a partir desta busca.

## Projetos relacionados encontrados no GitHub

- https://github.com/CobbyCode/bp10-dsp-controller — controlador ESP32-S3 USB Host com interface web, descoberta de efeitos e caminhos Music/Rec. README descreve alvo 4 MB flash / 2 MB Quad PSRAM, suporte experimental Generic ACP 8888:1719 e perfil AIYIMA 8888:171E. Não presumir que nosso PID171E tenha o mapa AIYIMA. Salva no NVS do ESP e reaplica, sem gravar flash do DSP. Melhor candidato para comparar/adaptar ao projeto atual; não testado nesta placa.
- https://github.com/leadercxn/bp1048_sdk_v0.1.12 — cópia pública de SDK, com BT_Audio_APP, MVsB1_Base_SDK e develop_lib. Histórico menciona SPP, BLE e variante de biblioteca sem BLE. Referência para investigar firmware próprio, não firmware pronto compatível com esta placa.
- https://github.com/stpf99/MVSilicon-BP1048B2_linux_fx_gui — interface Linux/Python/Qt para USB HID, auxiliares C para envio e consulta. README publica EQ em endereços diferentes dos nossos; comparar schemas antes de reutilizar comandos.

A pesquisa não confirmou firmware pronto que habilite controle Bluetooth mantendo especificamente o hardware/configuração desta BLDSP. Nenhum projeto foi executado ou gravado nesta investigação.

## Análise estática do SDK público

Relatório detalhado: `analise_sdk_bp1048.md`. Checkout de referência em `research/bp1048_sdk`, revisão 8105bd864b04995d81c9f9ae77cb158259f39015. Mesma família ACP, cabeçalho de efeitos 1.35.1 e tipo 0x30; não confirmado como firmware exato da BLDSP. Versão de aplicação, chip selecionado por padrão, catálogo de efeitos e implementação de save diferem ou precisam de validação.

No SDK, system_mode é standby/sleep, não troca de fonte. A terceira palavra de EQ é chamada filter_count e recalculada por bandas habilitadas; isso é pista para investigar calculation_type da nossa exportação, não confirmação da semântica do firmware instalado. Há handlers individuais de todos os atributos das bandas no SDK, ainda sem testes de todos os seletores na placa. Há pontos de recepção SPP/BLE, mas eles não encaminham diretamente ao ACP. Nenhuma gravação ou execução de ferramentas do SDK feita.

O usuário confirmou que reiniciar o ACP exibiu o pré-ganho -0,10 dB enviado por `test_eq_direct.py`; assim, além do sucesso da API, houve confirmação visual posterior da aplicação do ajuste. Não foi testada persistência desse ajuste por reinício da placa.

## Avaliação após reconexão e roadmap do firmware

Nova enumeração confirmou USB HID 8888:171E. Foram feitas apenas consultas, sem alteração de parâmetros, save, reset ou entrada em bootloader. A leitura via `HidD_GetInputReport` foi confirmada e reutilizada no handle USB ativo do ACP. Consultas e respostas completas estão em `live_acp_assessment.jsonl`; script: `inspect_live_acp.py`; validação offline: `summarize_live_acp.py`.

Resultado: versão 0.0.4 / efeitos 1.35.1, catálogo com 26 efeitos e todos os 26 nomes consultados. A consulta `A5 5A 80 01 00 16` retorna o catálogo; índices 1..26 retornam nomes. Rec Pre EQ permaneceu no módulo 93. Os blocos mapeados foram lidos, exceto FC (USER_TAG), que não foi solicitado nesta avaliação. Snapshot `estado_placa_20261114.json` também inclui respostas de catálogo e métricas.

Rec Pre EQ atual: pregain=0 dB; f5 Peak 35 Hz / Q=2 / +2,5 dB. Em comparação com o backup antigo, filter1_gain de Music Pre EQ e Rec Pre EQ está em +5 dB (antes +6); filter4_gain de Rec Pre EQ está em -6 dB (antes -10). Nenhuma restauração foi feita: preservar o estado atual do usuário. System Control responde com palavra adicional system_recover_enable em relação ao C exportado usado no catálogo; a diferença de tamanho de resposta não prova mudança de parâmetros.

As consultas automáticas do ACP podem sobrescrever a resposta antes da leitura. Algumas leituras foram repetidas até obter módulo/quadro válido; ausência inicial de resposta não foi tratada como inexistência do módulo. Foram usados 40 ms entre envio e leitura neste ensaio, não validados como requisito universal. O host definitivo deve serializar requisições, correlacionar respostas e prever timeout/retentativa.

Roadmap entregue em `roadmap_firmware_bldsp.md`: recuperação do original, toolchain Andes, porte do áudio/hardware, BLE/SPP, controle de EQ, persistência e testes simultâneos com A2DP. A viabilidade técnica permanece condicional a toolchain e recuperação; esta conexão ACP não forneceu dump do firmware executável.

## Exportação binária de parâmetros A e B

Analisado `parametros a e b.bin`: 8192 bytes, duas metades de 4096 bytes; cada uma contém 39 quadros ACP válidos (1132 bytes) e 2964 bytes de preenchimento FF. Versão 0.0.4 / efeitos 1.35.1. Diferenças A/B restritas a quatro ganhos: Music Pre EQ filtros 1, 2 e 10, e Rec Pre EQ filtro 1; A=-3 dB, B=+5 dB. Demais bytes idênticos.

B coincide com o snapshot lido anteriormente nos parâmetros comparáveis; System Control tem apenas os seis campos exportados, enquanto a consulta retorna sete. Isso não comprova a seleção interna de banco nem se A/B são presets, defaults/usuário ou redundância.

USER_TAG exportado é curto (01 02 03 04), sem a extensão ASCII da captura de save. Nenhuma conclusão nova sobre o efeito de FC/FD pode ser tomada apenas disso. Este arquivo é de parâmetros e não substitui backup do firmware executável. Relatório: `analise_parametros_a_b.md`; JSON: `parametros_a_b_decodificado.json`; validador offline: `analyze_parameter_banks.py`. Roadmap atualizado. Nenhuma operação na placa nesta análise.

## Identificação comercial BLenhance2.1 / pesquisa externa

Usuário identificou a placa como BLenhance2.1 e forneceu fotos frente/verso. Relatos de proprietários no diyAudio associam esse nome a Nvarcher, Rec Pre EQ na saída mono e troca de fonte por contato STOP/GND. Há referência indireta ao save A5 5A FD 00 16; não foi realizado teste novo de FD sozinho. Nenhum firmware original específico 0.0.4, esquema exato ou fonte da BLenhance foi localizado no GitHub nesta busca.

Relatório com links e limites: `pesquisa_blenhance_2_1.md`. QR com texto de contato para documentação não foi decodificado da imagem. Pads de programação não identificados. Roadmap atualizado; nenhuma alteração no hardware.
