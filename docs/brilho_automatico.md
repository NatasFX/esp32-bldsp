# Brilho automático por relógio e posição solar

Painel → Brilho automático: habilitar, ajustar localização e editar o programa CSV. Configuração salva na NVS do S3, separada dos presets e do brilho manual. C3 recebe o brilho efetivo pelo protocolo existente; não precisa de nova atualização. CLIP mantém RGB (255,0,0); desabilitar a fita continua apagando tudo.

## Relógio sem botão

O portal envia `Date.now()` e o deslocamento UTC do telefone automaticamente: ao abrir, ao recuperar foco/visibilidade e uma vez por minuto enquanto a página permanece ativa. `/api/clock` aceita segundos Unix e `utc_offset_minutes` (leste positivo). O S3 ancora essa leitura em `esp_timer_get_time()` e avança o relógio sem internet ou portal aberto. O envio não grava NVS e não ocorre a cada consulta de telemetria. Se falhar, tenta novamente após 5 s.

Sem bateria/RTC externo, não há hora confiável após cortar energia. Na inicialização, usa o brilho manual até receber hora válida. Não restaura a última data salva como se fosse atual. Fuso é atualizado junto com a hora do telefone. Sem novas sincronizações, permanece no último deslocamento UTC; não há tabela autônoma de mudanças sazonais de fuso.

## Localização e cálculo offline

Referência aproximada inicial do RS: latitude -30, longitude -53. Não representa localização GPS medida. Coordenadas são editáveis e persistem. O portal tenta geolocalização automaticamente somente em contexto seguro; a API do navegador requer permissão e HTTPS. No endereço HTTP local 192.168.4.1, normalmente é necessário preencher manualmente uma vez. Referência: [MDN getCurrentPosition](https://developer.mozilla.org/en-US/docs/Web/API/Geolocation/getCurrentPosition).

O S3 calcula nascer e pôr do sol para a data local usando a aproximação de ano fracionário/equação do tempo/declinação da [NOAA](https://gml.noaa.gov/grad/solcalc/solareqns.PDF), zênite 90,833°. Valores aproximados, adequados ao brilho decorativo, não destinados à navegação. Considera ano bissexto. Se não houver evento solar ou pontos coincidirem após resolver a data, usa brilho manual e informa que o programa não foi aplicado.

## CSV

Duas a doze linhas sem cabeçalho, formato `horário,percentual`:

```csv
sunrise-1,15
sunrise,60
12:00,100
17:00,60
sunset+30,15
```

`HH:MM` usa horário civil local. `sunrise` e `sunset` aceitam deslocamento de até +/-720 minutos. Percentual inteiro de 0 a 100. Não aceita horários repetidos. Os pontos são ordenados após calcular os eventos do dia, incluindo a passagem pela meia-noite.

Com transições suaves habilitadas, interpola linearmente. A primeira linha mantém a madrugada em 15% e permite uma rampa de um minuto até 60% ao nascer do sol. Sem transições suaves, mantém o valor do último ponto até atingir o próximo: para troca exata ao nascer basta remover a linha `sunrise-1,15`. Meia-noite mantém o último brilho noturno até o próximo ponto matinal.

## APIs e testes

- `POST /api/clock`: `{unix_seconds,utc_offset_minutes}`; cabeçalho `X-BLDSP: 1`.
- `GET/POST /api/lighting`: enabled, smooth, latitude, longitude, steps; cada passo `{anchor,minute,brightness}`, anchor 0=horário civil, 1=nascer, 2=pôr.
- Telemetria inclui lighting com relógio, fuso, eventos solares do dia, brilho e confirmação applied.
- Ganho, presets, CLIP, paletas e animações mantêm seus próprios controles. O campo de brilho manual continua disponível como fallback.

`light_policy_test.c` cobre sazonalidade, fuso, condição polar, meia-noite, pontos exatos, interpolação, troca direta, duplicação e validação. Teste UI cobre parser CSV, salvamento e sincronização sem spam. Pipeline de deploy: `tools/fw.ps1 deploy s3`.

Entrega: firmware S3 compilado e gravado pelo pipeline em 20/11/2026. Testes nativos/Node passaram, incluindo programa solar e editor CSV. C3 usa o mesmo protocolo e mantém seu firmware de 120 Hz. A confirmação de horário/localização exibidos no telefone depende de abrir o portal atualizado; modo automático começa desativado até o usuário habilitar/salvar. Captura curta de execução: `build/pipeline/serial-20261120-132231.log`.
