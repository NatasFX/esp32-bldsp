# OTA do painel ESP32-C3

## Uso

Na rede `BLDSP-Control`, abra o equalizador → Potenciômetros → **Atualizar firmware do C3 sem cabo USB**, ou `http://192.168.4.1/panel-update`.
Selecione a imagem **de aplicação** `panel-c3/build/bldsp_panel_c3.bin` e inicie. Não use o firmware do S3, bootloader, tabela de partições nem imagem de flash completa.
O upload mostra progresso e só informa sucesso após receber outro identificador de boot, SHA-256 correspondente e confirmação do firmware pelo próprio C3. Mantenha a página e os dois ESPs ligados até concluir. O envio pode levar alguns minutos.

A instalação inicial exige USB nos dois dispositivos: C3 para novo bootloader/partições/aplicação, S3 para ponte e página. Depois o C3 não precisa de acesso físico para atualizações de aplicação compatíveis. A tabela de partições e o bootloader permanecem fixos via OTA.

## Flash C3 de 4 MiB

| Partição | Endereço | Tamanho |
|---|---:|---:|
| NVS | 0x9000 | 0x4000 |
| otadata | 0xd000 | 0x2000 |
| phy_init | 0xf000 | 0x1000 |
| ota_0 | 0x10000 | 0x1f0000 |
| ota_1 | 0x200000 | 0x1f0000 |

`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`. Cada aplicação tem 1984 KiB disponíveis. A migração USB grava `ota_data_initial.bin` para limpar os metadados antigos. No primeiro boot, o C3 registra a aplicação inicial como VALID para que a primeira OTA já tenha uma versão anterior registrada.

## Transporte e gravação

S3 é ponte HTTP → ESP-NOW, sem armazenar o firmware inteiro. O navegador envia blocos HTTP de até 4096 bytes. O S3 subdivide em dados de até 200 bytes, com uma mensagem em voo, confirmação de sessão/comando/offset e até 10 tentativas por pacote (750 ms cada). O C3 grava em tarefa própria; callback Wi-Fi apenas valida e enfileira. Não há escrita de flash no callback.

Mensagens usam `PANEL_OTA_MAGIC`, versão própria 1 e o mesmo par MAC/chave ESP-NOW criptografado. Protocolo de potenciômetros permanece na versão 2. Solicitação de 222 bytes, ACK de 20 bytes, informação de 48 bytes: todos cabem no limite ESP-NOW de 250 bytes.

Sessões aleatórias separam transferências; limites de tamanho/offset são verificados. Repetições confirmadas não regravam bytes; sobreposição parcial e saltos são rejeitados. Resposta HTTP perdida pode ser recuperada consultando o offset do S3. Cancelamento e 30 s sem atividade descartam o handle e preservam a seleção de boot anterior. Após erro de gravação/integridade, inicie uma nova transferência.

C3 aceita apenas imagem ESP32-C3 com SHA-256 anexado e projeto `bldsp_panel_c3`. `esp_ota_end()` valida o conteúdo, e só depois `esp_ota_set_boot_partition()` troca o boot. A checagem de integridade não equivale a assinatura de firmware; remetente autorizado é o S3 pareado, acessível pela rede local do portal.

## Confirmação e rollback

Após OTA, a aplicação fica PENDING_VERIFY. A partir de 20 s confirma somente se todos os cinco ADCs foram lidos e houve resposta recente do S3; a amostra de saúde também precisa ser recente. Ao completar 60 s sem passar, solicita rollback e reinicia. Falha/reset antes de confirmar aciona o rollback do bootloader na próxima inicialização. Não é garantia contra qualquer defeito funcional: o autoteste verifica inicialização, ADC e enlace, não o resultado acústico nem cada LED.

O C3 anuncia a cada segundo SHA-256 da aplicação em execução, banco, boot aleatório e estado pendente. O portal compara com o hash anexado no arquivo e espera outro boot confirmado; ACK de envio sozinho não significa atualização concluída.

Durante o envio o S3 suspende comandos dos potenciômetros, mas continua controle de CLIP/temperatura e aplica os últimos ajustes do som. Após reboot, o pickup dos potenciômetros é reiniciado. LEDs continuam em sua tarefa, podendo pausar brevemente durante gravação de flash.

## Endpoints do S3

Todas as mutações exigem `X-BLDSP: 1`; não há CORS liberado.

- `GET /api/panel/ota`: disponibilidade, sessão, progresso e imagem em execução.
- `POST /api/panel/ota/begin`: JSON `{ "size": tamanho }`.
- `POST /api/panel/ota/data?session=ID&offset=N`: corpo binário de 1 a 4096 bytes.
- `POST /api/panel/ota/end`: JSON `{ "session": ID }`.
- `POST /api/panel/ota/abort`: JSON `{ "session": ID }`.

## Verificação

`tests/run_native.cmd` inclui limites/ordem/repetição/sessão dos pacotes e testes do JavaScript real da página: arquivo S3 recusado, retomada de bloco parcial, resposta final perdida, confirmação de novo boot/hash e ausência de falsa mensagem de sucesso quando o boot não confirma. Builds ESP-IDF dos dois alvos verificam integração com as APIs.

A validação física ponta a ponta exige ambos ligados e o S3 com a ponte instalada. Não se deve inferir esse teste apenas dos testes nativos ou da gravação USB.

Referência: [ESP-IDF OTA, validação e rollback](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32c3/api-reference/system/ota.html).

### Instalação física — 20/11/2026

C3 COM8 gravado via USB: bootloader, aplicação, tabela de partições e otadata inicial; esptool verificou os hashes. Boot normal com os cinco ADCs válidos (`valid=1f`) e tarefa WS2805 ativa. Leitura posterior de `0xd000`, 8192 bytes, confirmou metadados `(ota_seq=1, ota_state=2/VALID)` para ota_0. Evidências locais: `build/flash-ota-c3-latest.log`, `build/ota-c3-installed-serial.log`, `build/ota-c3-metadata-readback.bin`.

Somente o C3 foi preparado nesta etapa, conforme solicitado. S3 não foi gravado com a ponte; transferência sem fio e rollback provocado ainda aguardam validação física com o par completo.

### Teste sem fio e correção WS2812 — 20/11/2026

S3 COM7 gravado com ponte OTA. Um harness temporário enviou 839664 bytes ao C3 pelo mesmo transporte ESP-NOW (blocos de 200 bytes, ACK e validação), sem USB no C3. A aplicação WS2812 foi selecionada em ota_1, endereço 0x200000. Aos 50,3 s do boot do S3, o receptor havia reiniciado, anunciado SHA-256 correspondente `911b3824dd7dae54d64c708f4e95706bc774d703127b234d9ba674436cc4ae8a` e confirmado o novo firmware. Log: `build/ws2812-ota-smoke-serial.log`, mensagem `PASS: new boot, SHA256 match, confirmed bank=0x200000`.

O usuário confirmou rainbow suave sem piscadas estranhas após a atualização. O harness de envio automático foi removido do código e da versão final; atualizações normais são iniciadas no portal. Este teste físico valida o transporte e a confirmação de boot; a interface HTTP tem testes automatizados separados. Rollback provocado por firmware defeituoso não foi ensaiado fisicamente.
