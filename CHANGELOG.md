# Histórico de entregas

## 2026-11-20 — Bateria e mínimos durante CLIP

- ADC2 do C3 com alvo de 10ms, média/mínima por segundo e contagem de amostras válidas/perdidas.
- Média lenta de até 30s para descarga sustentada e mínimos de 1s para quedas breves.
- Avisos amarelos a cada 5s e vermelhos a cada 2s; CLIP mantém prioridade.
- Quatro limiares configuráveis na aba Monitoramento, persistidos na NVS do S3.
- Tensão principal e mínima na janela do último CLIP, com identificação do evento e idade; correlação aproximada via ESP-NOW.
- Firmware enviado, testes aprovados; seis janelas físicas com 100 amostras/s e nenhuma perda. Validação visual e CLIP real pendentes.

## 2026-11-20 — Painel C3, proteções e fluxo de atualização

### Controle e interface

- Painel ESP32-C3 com cinco potenciômetros B10K e comunicação ESP-NOW com o S3.
- Mapeamento de ganhos pelo portal, leitura RAW/mV, inversão do sentido e zona neutra nos extremos.
- C3 assume suas posições na conexão/reconexão, após o bootstrap USB. Desconexão mantém os ajustes; abrir o portal não reaplica presets.
- Sincronização dos ganhos dos filtros na interface e redução de filas/consultas USB para melhorar a resposta.
- Portal cativo e potência de transmissão Wi-Fi configurável.

### Monitoramento e proteção

- Parâmetros do detector de CLIP persistidos, exclusão das janelas de CLIP da referência óptica e indicação de sensor possivelmente desconectado.
- Aviso breve no pill, flash com fade para CLIP repetido e proteção ajustável no pré-ganho do sub, com recuperação lenta.
- Temperatura estimada da bobina disponível na tela principal; NTC e histórico na aba de monitoramento.

### Iluminação

- WS2812 RGB, 22 LEDs, 11 paletas e renderização de 120 Hz sem alterar a velocidade dos efeitos.
- Giro acompanha os potenciômetros, retém por 2 s e retorna suavemente em 3 s; extremos indicados por dois fades até 20% do brilho.
- CLIP tem prioridade e vermelho em intensidade 255, independente do brilho decorativo.
- Sem enlace: giro à esquerda em 8 s e respiração de 30–100% em 4 s. Com enlace: giro normal à direita.
- Horário capturado automaticamente do navegador, agenda de brilho CSV e cálculo solar com localização configurável.

### Desenvolvimento e validação

- CLI único para testes, build, flash, captura curta de logs e OTA do C3 através do S3.
- Ponte OTA isolada do build de produção, confirmação do novo boot/SHA/banco e restauração automática do S3.
- Suíte nativa/Node e testes do pipeline aprovados. Builds S3/C3 aprovados.
- Último OTA confirmou o C3 (842912 bytes, banco 0x10000); S3 de produção restaurado, NVS preservada.
- Funções visuais anteriores e enlace foram conferidos pelo usuário; o novo efeito de desconexão foi testado em software e enviado, aguardando avaliação visual.

Detalhes e limitações estão nos guias de cada função em `docs/`.
