# CLIP por pulso óptico — referência longa separada da validação

Atualização de 16/11/2026. Substitui a comparação de nível isolada e o limiar RAW fixo 450. GPIO17, LDR para GND e pull-up externo de 10 kΩ; aquisição nominal de 20 ms.

## Referência independente

Média aritmética móvel das amostras válidas nos 60 segundos anteriores. A amostra atual não participa da sua própria comparação. Timestamps expiram amostras por tempo real. Todos os valores válidos, incluindo dips, atualizam essa referência independentemente da máquina de estados de CLIP. Exige um minuto de leituras para ficar pronta. Leitura inválida ou lacuna maior que 500 ms reinicia a referência. As antigas chaves NVS de calibração óptica não são usadas.

## Máquina de estados do pulso

1. Abertura de candidato exige RAW abaixo de 80% da média e uma descida maior que 20% dessa média em até 60 ms, partindo de uma leitura de pelo menos 90% da referência. Um nível baixo que foi atingido lentamente não abre candidato.
2. A referência é congelada apenas para validar esse candidato, sem parar a média móvel. Disparo baixo = 80%; recuperação = 90% da referência congelada.
3. Pulso curto exige pelo menos duas amostras consecutivas baixas e depois subida maior que 20% da referência em até 60 ms, atingindo o nível de recuperação. Só então emite o evento de CLIP.
4. Uma recuperação lenta invalida um pulso curto ainda não confirmado. Uma amostra baixa isolada não conta.
5. Com descida válida e mais de 30 amostras consecutivas baixas, confirma CLIP sustentado na 31ª amostra, sem aguardar subida. Permanece ativo até a recuperação e não conta sua borda final novamente.

A janela de 60 ms usa os timestamps reais, não apenas a contagem de amostras. São normalmente três intervalos de 20 ms. O caso sustentado usa a contagem solicitada: cerca de 600 ms entre a primeira e a 31ª amostra na cadência nominal.

O detector reconhece a forma óptica medida, não a origem da luz. Uma luz externa reproduzindo duas bordas rápidas pode imitar o LED; isolamento óptico continua útil. Pulsos que caiam inteiramente entre amostras ou produzam só uma amostra baixa são rejeitados/não observáveis nesta configuração.

## Indicação, API e logs

Após confirmar um pulso curto, RGB GPIO48 pisca vermelho a 4 Hz por 2 s. Em evento sustentado, pisca enquanto ativo e por 2 s depois. Amplitude máxima 128/255. Erros do sistema e atraso inicial do RGB mantêm sua prioridade. Não altera ganho, não é compressor e não quantifica distorção do áudio.

GET /api/telemetry -> clip inclui status, active com retenção visual de 2 s, raw, millivolts, count, baseline_raw, threshold, release_raw, mode=relative, window_ms=60000, edge_window_ms=60, min_low_samples=2 e sustained_samples=31. Durante candidato, threshold/release refletem a referência congelada do pulso. POST /api/telemetry/ldr antigo retorna 410. O ajuste térmico é independente.

Serial por ciclo: `CLIP_RAW t=<ms> r=<RAW> b=<média móvel> ready=<0/1> a=<CLIP confirmado> n=<contador>`. O campo a não representa qualquer LED opticamente aceso: para pulso curto, fica 1 no ciclo de confirmação após a subida; para sustentado, permanece 1 até a recuperação. Pulsos candidatos não incrementam n. O resumo sensors de 5 s é adicional e não serve para reconstruir pulsos. O arquivo serial compartilhado continua gravado pelo PuTTY.

Gráfico manual: últimos 2 s a 20 ms, congelados até atualizar. Contagem e eventos são mantidos no ESP. Histórico de RAM é limitado e reinicia ao desligar.

## Verificação

Testes nativos cobrem duas amostras e ambas as bordas, rejeição de uma única amostra, limite estrito de 20%, confirmação na 31ª amostra sem dupla contagem, aproximação lenta e patamar baixo da lanterna, subida lenta, lacunas, ADC inválido, relógio uint32 e expiração da referência. O RGB mantém seus testes de amplitude e seleção de estado. O teste físico após esta alteração ainda precisa ser repetido para avaliar a sensibilidade na montagem.
