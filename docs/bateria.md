# Monitoramento de alimentação e avisos de bateria

## Aquisição

C3 GPIO5/ADC2, alvo de uma conversão a cada 10ms (100Hz). Continua a recuperação de ADC2 existente: rails e erros não entram nas estatísticas. Um relatório por segundo transmite média em mV, mínima, número de amostras válidas/perdidas e duração da janela. Recuperações são avisadas imediatamente, fora dessa cadência. Durante OTA a janela é descartada. Exigem-se pelo menos 80 amostras válidas e janela de 800–1500ms para publicar tensão válida. Perdas de comunicação/ADC são ausência de dados, não bateria em zero volts.

S3 usa a calibração linear de quatro pontos, com pull-up da placa compensado e pull-down de dois 10k em paralelo. A linearidade permite converter média e mínima de mV para volts. Continua sendo medição experimental no ponto que alimenta o painel: quedas de cabos/conectores não equivalem necessariamente à tensão nos bornes da bateria.

## Limiares iniciais (configuráveis)

| Situação | Aviso amarelo | Crítico vermelho |
|---|---:|---:|
| Média móvel de até 30s | abaixo de 12,2V por 10 relatórios | abaixo de 11,8V por 5 relatórios |
| Mínima de uma janela de 1s | abaixo de 11V | abaixo de 10,5V |

Condições sustentadas exigem pelo menos 10 relatórios válidos desde iniciar/resetar a referência. Ao limpar, exigem 5 relatórios acima do respectivo limite +0,2V; ao elevar a gravidade, assume imediatamente após a confirmação. Quedas breves mantêm indicação por 5s, renovada se repetirem. A maior gravidade prevalece. Sem dados recentes (2,5s), não apresenta alerta de bateria baseado em valores antigos.

Pulsos de 300ms com entrada/saída suaves: amarelo a cada 5s, vermelho a cada 2s. Respeitam brilho configurado/automático e habilitação da fita. CLIP tem prioridade e mantém vermelho em 255. Sem enlace, mantém o efeito de desconectado. Avisos de bateria não mexem no ganho nem desligam o sistema.

Os valores são escolhas conservadoras de interface, não limites universais de dano ou estado de carga. ODYSSEY recomenda recarregar antes de 12,2V; avaliação por tensão deve ser feita em condições adequadas de repouso, e carga intensa causa quedas transitórias. Os limiares carregados no som são ajustáveis justamente por isso. Fontes: https://www.odysseybattery.com/faqs/ e https://www.odysseybattery.com/proper-battery-testing-and-maintenance-maximizes-service-life-and-cost-savings/

## Tensão associada ao CLIP

C3 guarda até 100 amostras recentes. Quando recebe incremento do contador de CLIP do S3, recupera a mínima do último segundo e segue atualizando durante o sinal de CLIP e por 500ms após seu término. Envia contador e boot do S3 junto da mínima. Assim, pulsos breves ainda podem ser associados mesmo com a latência do retorno ESP-NOW (resposta nominal a cada 250ms).

Isso é uma janela de correlação, não uma medida exatamente sincronizada ao pulso do LDR. Se houver vários eventos em um relatório, o último contador representa a janela recente, que pode conter eventos anteriores. O portal explicita “mínima na janela do CLIP”, o número do evento e a idade. Estado guardado em RAM, sem escrita NVS por evento; reinícios limpam esse histórico.

## Portal e persistência

Tela principal: média/mínima de 1s, estado por cor e mínima na janela do último CLIP. Monitoramento: média 1s, mínima 1s, média lenta, contagem de amostras e campos dos quatro limiares. Salvar faz POST /api/battery, valida os números e grava em NVS panel/battery_v1. Configuração inválida ou erro de gravação é informado. GET /api/battery e telemetria incluem o mesmo objeto battery.

Os parâmetros são independentes dos presets de EQ e dos ajustes da proteção por CLIP. Log panel_vcc permanece limitado a aproximadamente uma linha por segundo, com média, mínima, amostras e perdas; avisos de recuperação aparecem imediatamente.

## Verificação

Testes nativos cobrem agregação de 100 amostras com dip, exclusão de amostras inválidas, média móvel, thresholds rápidos/lentos, timeout e validação dos limites. Testes de LED cobrem pulsos amarelo/vermelho e prioridade do CLIP. Testes da interface cobrem apresentação de tensão/CLIP e ausência de dados. Verificação de hardware registrada após deploy.

### Hardware após deploy em 20/11/2026

OTA confirmou imagem C3 de 845664 bytes, novo boot/SHA/banco 0x200000; S3 normal restaurado, NVS preservada. Captura serial-20261120-160945.log: seis relatórios consecutivos com n=100 e missed=0, aproximadamente 1s entre relatórios. Média 12,02V, mínimas 11,68–11,96V; conversão isolada 55–57µs. Pots continuaram valid_mask=1f e picked_mask=1f. Sem evento de CLIP real nem avaliação visual dos avisos de bateria nessa captura. O teste de software verifica a apresentação de uma mínima associada, mas a janela temporal de correlação ainda precisa ser validada em bancada.
