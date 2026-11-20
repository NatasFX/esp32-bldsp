# OTA do S3 e fluxo inteiramente sem fio

## Preparação inicial por USB

O S3 de 16MB mantém NVS em 0x9000 (0x6000 bytes), PHY em 0xf000 e factory em 0x10000 (3MB). Acrescenta ota_0 em 0x310000 (3MB), ota_1 em 0x610000 (3MB) e otadata em 0x910000 (0x2000). A preparação inicial exige gravação USB do bootloader com rollback habilitado, tabela nova e aplicação. Não usa erase_flash nem altera a NVS. Backup bruto e snapshots ficam em build/pipeline, fora do Git.

Depois, atualizar pela rede BLDSP-Control:

```powershell
.\tools\fw.ps1 wireless s3 --wifi-profile BLDSP-Control
.\tools\fw.ps1 wireless c3
# Reutilizar imagens já compiladas:
.\tools\fw.ps1 wireless s3 --no-build
.\tools\fw.ps1 wireless c3 --no-build
```

No Windows, --wifi-profile BLDSP-Control reconecta o adaptador após o reboot do S3. A reconexão é espaçada para não reiniciar associação/DHCP repetidamente. PC/celular precisa estar conectado ao Wi-Fi do ESP. Host padrão 192.168.4.1, configurável por --host. O CLI não usa COM nem ponte temporária no S3. Para o C3, usa o transporte ESP-NOW do portal existente. Atualizar um dispositivo por vez.

## Pelo celular

Painel possui links para /s3-update e /panel-update. Selecionar arquivo de aplicação correto: bldsp_control.bin para S3; bldsp_panel_c3.bin para C3. Imagem completa de flash, bootloader e arquivo do outro chip são rejeitados. No HTTP local não depende de Web Crypto; usa SHA anexado à imagem ESP e verificação do firmware pelo ESP-IDF.

## Fluxo S3

GET /api/s3/ota informa boot ID, banco, endereço, SHA256 de imagem e estado pending. POST recebe o binário (Content-Type application/octet-stream), X-BLDSP: 1 e X-Image-SHA256 com o hash de imagem anexado (últimos 32 bytes do .bin, não hash do arquivo inteiro).

Cabeçalho, chip S3 (ID9), magic da descrição e nome do projeto são conferidos antes de iniciar escrita. Atualização vai para o banco alternativo; esp_ota_end verifica imagem e hash, depois compara SHA informado e só então altera a seleção de boot. Falha de upload aborta a escrita e não seleciona o novo banco. Upload é rejeitado durante OTA C3.

S3 responde e reinicia. Confirma a aplicação cinco segundos após completar a inicialização dos serviços, servidor HTTP e portal. Não exige presença de DSP ou C3 para confirmar. Se falhar antes da confirmação, o bootloader tenta o firmware anterior no próximo boot. Não é teste funcional completo do áudio e não há assinatura criptográfica adicional: o acesso segue o modelo local da rede Wi-Fi do projeto.

CLI/página só anunciam sucesso após observar boot ID novo, SHA esperado e pending=false. Reenvio da mesma imagem ainda exige boot novo. O Wi-Fi cai brevemente no reboot; se o Windows/celular não reconectar automaticamente, reconecte a BLDSP-Control e consulte o estado. Não depende de internet.

## USB após OTA

Fluxo USB de aplicação S3 grava factory e ota_data_initial.bin em 0x910000, voltando a selecionar factory. --full também instala bootloader e tabela. NVS é preservada. Isso mantém a recuperação por USB e o caminho legado de OTA C3 por ponte funcionando, mas apaga a seleção do banco OTA anterior. O fluxo recomendado após montagem é wireless.

Durante reinício, o áudio fica sob o estado autônomo da BLDSP; ao iniciar o controle, o firmware reaplica preset inicial e depois os pots do C3, conforme o fluxo normal. Configurações de proteção e presets ficam na NVS. Upload não grava esses dados.

## Verificação

Testes cobrem cabeçalho truncado, chip/projeto errado, magic inválida e imagens completas versus aplicação. Testes anteriores de recuperação da ponte permanecem aprovados. Validação física registrada depois dos envios.

Referência ESP-IDF: https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32s3/api-reference/system/ota.html

### Testes físicos em 20/11/2026

Adaptador Realtek RTL8188EU ligado ao PC, IP 192.168.4.3; Ethernet mantida. Importado perfil manual BLDSP-Control. Varredura WLAN foi bloqueada pela permissão de localização do Windows, mas a conexão direta ao SSID conhecido funcionou sem alterar essa permissão.

C3 atualizado por HTTP/ESP-NOW sem ponte USB: imagem 845664 bytes, novo boot/SHA/banco 0x10000 confirmados. S3 preparado uma vez por USB, com backup bruto da NVS e snapshots dos quatro presets/configurações. Upload HTTP de prefixo C3 no endpoint S3 rejeitado com HTTP400 antes da escrita.

S3 atualizado realmente pelo Wi-Fi duas vezes com imagem 1172960 bytes e SHA 65445dfb4b14cecf7c9181fb1a2eb55fe674b44fd6ec1d73cee386d4d23c230d: factory -> ota_0 (0x310000) -> ota_1 (0x610000). Primeiro envio teve timeout do cliente na reconexão, mas boot/hash/pending=false foram verificados diretamente depois. Cliente foi corrigido para espaçar reconexões; segundo envio confirmou automaticamente novo boot/SHA/ota_1/pending=false. Não houve corte artificial de energia nem teste de crash/rollback.

Comparação de snapshots antes/depois: quatro presets, mapeamento dos pots, fita, thresholds de bateria, agenda de brilho e potência Wi-Fi idênticos. C3 continuou online/confirmado após os OTAs do S3. Página /s3-update retornou HTTP200. Não foi necessário desligar a Ethernet nem alterar privacidade/localização do Windows.
