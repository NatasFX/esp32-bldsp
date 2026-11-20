# Pipeline local de firmware

Comece por este guia. Não é necessário reler os fontes de OTA/build para tarefas rotineiras.

## OTA recomendado após preparação do S3

Conecte ao Wi-Fi BLDSP-Control. `tools/fw.ps1 wireless s3 --wifi-profile BLDSP-Control` (Windows) e `tools/fw.ps1 wireless c3` compilam e enviam pelo portal de produção, sem USB e sem ponte temporária. `--no-build` reutiliza o binário. O S3 precisa da preparação inicial das partições OTA por USB: [guia](ota_s3.md). O caminho `ota` abaixo permanece como alternativa legada por USB.

## Comandos prontos

Na raiz do repositório, PowerShell:

```powershell
.\tools\fw.ps1 ports
.\tools\fw.ps1 test
.\tools\fw.ps1 build s3
.\tools\fw.ps1 deploy s3
.\tools\fw.ps1 ota
.\tools\fw.ps1 ota --prepare-only
.\tools\fw.ps1 logs --seconds 10
```

- `deploy s3`: compila e grava somente a aplicação do S3 na COM7, preservando NVS, bootloader e partições.
- `flash s3`: grava o build existente sem recompilar. Usar somente se o build corresponde aos fontes desejados.
- `ota`: compila S3 normal e C3, compila ponte temporária em `build/ota-bridge`, grava a ponte no S3 via COM7, envia ao C3 via ESP-NOW e espera novo boot, SHA-256 igual e banco confirmado. Depois restaura a aplicação normal do S3, inclusive se o OTA falhar. Não requer Wi-Fi no PC nem USB do C3. Ambos os ESPs precisam estar ligados.
- `ota --prepare-only`: compila e valida todos os artefatos OTA sem acessar dispositivos.
- `build c3`: apenas compila o C3.
- `deploy c3 --full --port COM8`: instalação USB inicial/reparo do C3; grava bootloader, partições, aplicação e metadados OTA. **Reseta a seleção dos bancos OTA**; para atualização normal usar `ota`.
- `deploy s3 --full`: instalação inicial/reparo do S3 com bootloader e partições, sem apagar toda a flash.
- `logs --seconds 10 --match 'http_eq|EQ TX|mismatch'`: captura curta, filtro de exibição; arquivo mantém todos os bytes recebidos. Exibe no máximo 40 correspondências.
- Todos os comandos de dispositivo aceitam `--port COMn`; portas padrão S3 COM7, C3 COM8.
- `python tools/fw.py ...` é equivalente, desde que o Python tenha pyserial. O launcher escolhe o Python do ESP-IDF instalado.

## Saída curta e arquivos

Logs fixos de build/flash/testes: `build/pipeline/<etapa>.log`, substituídos na próxima execução da mesma etapa. Capturas seriais têm data/hora no nome. Build mostra início, resultado e caminho; em falha, no máximo 12 linhas de erros ou cauda. Não colar logs completos no chat.

Após deploy, consultar só uma captura curta se necessário. O CLI fecha apenas PuTTY cuja linha de comando identifica a COM solicitada. Não mata processos desconhecidos. O comando de logs fecha a porta ao terminar e não deixa PuTTY rodando; o usuário pode reabri-lo depois. Se houver acesso negado por outro processo, o comando falha e mantém o diagnóstico em arquivo.

## OTA: isolamento e recuperação

O firmware normal é construído com `BLDSP_OTA_BRIDGE_IMAGE` vazio. A ponte só existe quando essa opção recebe a imagem C3. Seus hooks em `app.c` e `panel_ota.c` ficam fora da compilação normal. `tools/ota_bridge.inc` reutiliza transporte, criptografia, ACK/retry e confirmação já usados pelo firmware.

Não há patch temporário de fonte, cópia/restauração de CMake, nem troca dos binários normais pelo binário de ponte. O CLI compara as tabelas de partição e rejeita produção contendo a marca da ponte. Guarda o hash do binário normal e o confere antes de restaurar. OTA não confirmado retorna falha mesmo que a restauração tenha sucesso. Se a restauração falhar, executar:

```powershell
.\tools\fw.ps1 flash s3
```

O `finally` executa em erro/timeout e Ctrl+C normal. Encerrar o processo à força, desligar o PC ou remover o USB pode impedir a restauração. Não interromper deliberadamente o envio. Depois da confirmação, a ponte é removida ao restaurar a aplicação normal. A imagem C3 já possui rollback; não confundir confirmação do C3 com simples conclusão do upload.

O acompanhamento OTA aceita até 240 s; espera interna da ponte: até 60 s pelo C3, transporte com retries e até 85 s para confirmação do boot. Monitor serial inicia sem novo reset. Os logs detalhados ficam locais.

## Ambiente

ESP-IDF padrão em `%USERPROFILE%\Documents\esp-idf`; pode ser sobrescrito por `IDF_PATH`. Python padrão `%USERPROFILE%\.espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe`; sobrescrever por `BLDSP_IDF_PYTHON`.

Executar uma operação de firmware por vez: builds compartilham cache/configuração e as operações de dispositivo precisam da porta exclusiva. No Codex, build/serial/CIM podem exigir execução fora da sandbox; use a autorização de flash já presente na conversa, sem pedir confirmação repetida.

## Validação do próprio pipeline

```powershell
python -m unittest discover -s tests -p pipeline_test.py
.\tools\fw.ps1 test
.\tools\fw.ps1 build s3
```

Os testes simulam OTA bem-sucedido, falha e interrupção para conferir a restauração da produção sem tocar em hardware. Compilação da ponte é conferida separadamente; um OTA completo precisa dos dispositivos ligados. Detalhes do protocolo em [ota_painel_c3.md](ota_painel_c3.md).

## Validação em 20/11/2026

Sete testes do pipeline passaram, incluindo sucesso, timeout, interrupção, erro no primeiro flash, recusa de partições diferentes, recusa de ponte como produção e prepare-only sem acesso a dispositivo. Suíte nativa/Node do projeto passou. `ota --prepare-only` compilou S3, C3 e ponte separada e conferiu os artefatos. `logs --seconds 2` abriu/capturou/fechou COM7 sem reset. O novo orquestrador ainda não fez um ciclo físico completo de OTA; o transporte e a confirmação usados pela ponte vêm do harness já utilizado anteriormente. Nenhum firmware foi gravado ao preparar este pipeline.

Validação física do pipeline em 20/11/2026: `tools/fw.ps1 ota` executou integralmente a atualização do CLIP máximo. Imagem C3 841776 bytes; novo boot/SHA-256/banco confirmado 0x10000 aos 56,3 s. Restauração do S3 normal concluída com hash de flash verificado; NVS preservada. Registro: `build/pipeline/serial-20261120-073348.log`. Esta validação substitui a limitação anterior de ainda não ter executado OTA físico pelo orquestrador.
