# Rotina de firmware (leitura inicial curta)

- Para build, flash, OTA e logs, use `tools/fw.ps1`; referência curta em `docs/pipeline.md`.
- S3: `./tools/fw.ps1 deploy s3`. C3 sem fio via S3 USB: `./tools/fw.ps1 ota`.
- Antes de gravar, mudanças de lógica devem passar pelos testes pertinentes; `./tools/fw.ps1 test` executa a suíte existente.
- Não recriar manualmente o harness OTA nem editar fontes temporariamente. A ponte usa build separado e restaura produção automaticamente.
- Logs completos ficam em `build/pipeline/`. Comece pelo resumo do CLI; se falhar leia somente erros/cauda de 10–20 linhas. Evite despejar logs de sensores a 20 ms no contexto.
- Logs curtos: `./tools/fw.ps1 logs --seconds 10 --match 'http_eq|EQ TX|mismatch'`.
- Não executar dois flashes/builds concorrentes nem matar processos desconhecidos que ocupem a serial.
- Não descartar mudanças existentes nem publicar commits sem autorização da tarefa.
