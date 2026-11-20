# Ajustes persistentes do detector de CLIP

Em Monitoramento, Sensibilidade do detector de CLIP permite salvar os parâmetros na NVS do ESP. A SRAM guarda o estado em execução; a NVS mantém a configuração após desligar. POST /api/telemetry/ldr exige X-BLDSP: 1 e os sete campos inteiros abaixo. GET /api/telemetry retorna os valores efetivamente em uso no objeto clip.

| Campo | Padrão | Limites |
|---|---:|---|
| drop_percent | 20 | 1–80 % |
| release_drop_percent | 10 | 0 até menor que drop_percent |
| edge_window_ms | 60 | 20–500 ms |
| min_low_samples | 2 | 1–25 |
| sustained_samples | 31 | mínimo acima até 250 |
| window_ms | 60000 | 5000–60000 ms |
| hard_min_samples | 6 | 1–250 |

Salvar valida todos os campos antes da escrita, confirma NVS e reinicia o detector e o alerta visual. O contador histórico de eventos não é zerado. É necessário aguardar uma nova janela de média para detectar. Falhas/lacunas de ADC reiniciam a referência preservando a configuração. O período nominal de aquisição continua em 20 ms.

Queda menor ou mínimo de uma amostra aumenta sensibilidade e também a chance de falso positivo. Janela das bordas maior aceita mudanças mais lentas de iluminação. A queda residual controla o retorno à referência: 10% significa liberar ao voltar a pelo menos 90% da média. CLIP forte é uma classificação visual; não muda os parâmetros da proteção de volume, que são configurados separadamente.

Os padrões reproduzem o detector anterior. Para investigar eventos perdidos, começar alterando apenas a queda de 20 para 15%, mantendo os demais parâmetros e comparando com a captura RAW.

Atualização 19/11: edge_window_ms agora limita apenas a descida. A confirmação acontece ao atingir min_low_samples; o retorno pode ser lento. O evento permanece ativo até o limiar de recuperação, sem contagem adicional na subida. sustained_samples classifica o episódio para repetição da proteção, não atrasa a primeira confirmação.
