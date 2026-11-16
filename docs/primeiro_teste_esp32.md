# Primeiro teste em ESP físico — 15/11/2026

- ESP32-S3 revision v0.2, flash 16 MB, PSRAM embarcada 8 MB identificados via esptool na COM7 (CH343).
- Ambiente instalado: ESP-IDF v5.4-dev-2472-gc9df77efbf-dirty; não é a release 5.4.2 prevista originalmente. Build completo passou após alinhar declarações de arrays entre headers e implementações. Testes nativos C/JS passaram.
- Bootloader, tabela de partições e aplicativo gravados com esptool a 460800 baud; hashes conferidos pelo gravador. Não foi executado erase_flash completo.
- Logs confirmaram boot, SoftAP, DHCP 192.168.4.1, instalação do USB host e servidor iniciado. Ainda sem confirmação da página por Wi-Fi ou comunicação HID com BLDSP.
- Monitor: PuTTY, COM7, 115200 baud, 8N1, sem flow control. Arquivo local ignorado no Git: serial-monitor.log. PuTTY mantém a única conexão serial; outros programas devem ler o arquivo e não abrir a COM7. Fechar o monitor antes de nova gravação.
- COM7 inicialmente negou acesso e depois liberou; causa não determinada. Handle oficial Microsoft Sysinternals permite investigar: handle64.exe -a Serial. Não fechar handles à força.
- USB entre ESP e BLDSP não enumerou no teste inicial. Usuário relatou jumper de solda USB-OTG na parte inferior. Em uma placa DevKitC1-N16R8 documentada, fechar esse jumper fornece VBUS da porta COM à USB; não confirma a revisão do usuário. Referência: https://github.com/maxgerhardt/esp32s3-usb-host-keyboard
- Antes de soldar: identificar placa/revisão e continuidade com tudo desligado. Se BLDSP usa fonte externa, avaliar backfeed e preservação de isolamento; não unir saídas de fontes. Cabo C–C pode exigir solução de CC/host, e habilitar VBUS não garante enumeração. O modo host de software já foi instalado pelo firmware.

## Iniciar monitor no Windows

Com COM7 livre, a partir da raiz do projeto:

```powershell
& 'C:\Program Files\PuTTY\putty.exe' -serial COM7 -sercfg 115200,8,n,1,N -sessionlog "$PWD\serial-monitor.log" -logappend
```

Para acompanhar o mesmo arquivo em outro leitor:

```powershell
Get-Content .\serial-monitor.log -Tail 60 -Wait
```

O arquivo inicial contém captura de boot pelo Python; o PuTTY foi aberto depois em modo append. Ausência de linhas novas pode significar firmware ocioso, não falha da conexão. Pressionar RESET no ESP permite observar o boot no monitor aberto.

## USB HID confirmado após fechar USB-OTG

Em 15/11/2026, após usuário fechar USB-OTG e manter IN-OUT aberto, nova captura serial confirmou:

```text
I (3545) dsp_usb: BLDSP 8888:171e
I (3545) dsp_usb: Vendor HID interface 0, reports 256 bytes
E (3725) USBH: Device 1 gone
W (3745) dsp_usb: Read module 91: ESP_ERR_INVALID_STATE
I (3745) dsp_usb: Closing USB device
I (4235) dsp_usb: BLDSP 8888:1719
I (4255) dsp_usb: Vendor HID interface 4, reports 256 bytes
I (4775) dsp_usb: Preset 1 applied to stereo + sub; no DSP flash save
```

Confirmados no ESP físico: descoberta dinâmica da interface HID, descritor de reports de 256 bytes, verificação do perfil e aplicação/leitura de retorno do preset 1 para módulos 91 e 93. A troca 171E → 1719 ocorreu neste boot; sua causa não foi determinada. Não inferir continuidade de Bluetooth a partir do PID. Permanecem pendentes confirmação auditiva/roteamento das saídas pelo usuário, testes do front via AP, persistência e reconexão prolongada.

## Correção de pré-ganho e diagnóstico

Relato: troca de presets altera áudio; pré-ganho na aba Subwoofer com EQ ativo termina com check, mas sem efeito audível. Check confirma leitura de parâmetros, não mede áudio.

No SDK relacionado (`audio/communication.c`, Communication EQ), o seletor FF copia pregain e chama apenas AudioEffectEQFilterConfig; o seletor 01 chama AudioEffectEQPregainConfig quando o valor muda. O caminho FF também copia pregain por variável sem sinal. Isso oferece uma explicação consistente para memória atualizada e ganho de áudio desatualizado; não prova identidade do firmware instalado com esse SDK.

Correção: após o bloco completo, enviar seletor 01 com vizinho de um passo Q8.8 e depois o alvo. O passo intermediário é −1/256 dB relativo ao alvo; no mínimo −128 dB usa +1/256 dB para não ultrapassar a faixa. Isso força reconfiguração mesmo quando a palavra lida já coincide com o alvo. Bloco inteiro, atualização individual e consulta permanecem na mesma tarefa USB, serializados. O formato escalar foi conferido contra a captura ACP −0,10 dB (`A5 5A 93 03 01 E6 FF 16`).

Logs novos: abertura da página, POST/URI/tamanho, módulo/enable/pré-ganho solicitados, TX USB, atualização de ganho em tempo real enviada, RX e confirmação/matches/duração ou erro USB. Nenhuma senha nos logs. Necessária confirmação auditiva após gravação; retorno de parâmetros não comprova atualização sonora.

## Troca automática de fonte validada no GPIO2

Usuário confirmou a ligação do GPIO2 ao contato de modo. Em novo boot, sem acionamento manual do botão:

```text
I (4152) source: USB audio mode 1719; wait for stable enumeration; attempts=0
I (5162) source: Automatic pulse GPIO2 low 100 ms, attempt=1; awaiting USB 171e
I (5262) source: GPIO2 released; pulse elapsed=100065 us
I (6432) source: Target USB 171e confirmed; Bluetooth/HID mode; no pulse
I (9232) dsp_usb: Preset 1 applied to stereo + sub; no DSP flash save
```

Após a enumeração alvo houve uma divergência transitória na leitura do módulo 91 (palavra 7); a repetição do bootstrap confirmou os dois módulos. Nenhum segundo pulso foi gerado. A sequência confirmou recuperação de 1719 para 171E por comando do ESP e posterior aplicação do preset, sem depender do navegador. A duração de pulso indicada é medida pelo software. Testes prolongados, power-cycle completo e continuidade auditiva durante a troca ainda devem ser conferidos.
