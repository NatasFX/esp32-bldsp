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
