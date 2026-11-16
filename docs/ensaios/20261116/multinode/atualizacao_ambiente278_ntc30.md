# Atualização das condições iniciais

Ambiente informado: **27,8 °C**. NTC inicial informado: **30,0 °C**. Os parâmetros pre17_v1 foram preservados; não houve reajuste com outro teste.

A diferença de 2,2 °C não foi usada para elevar artificialmente a temperatura ambiente. Foi atribuída provisoriamente a calor residual na estrutura, considerando o NTC aproximadamente estável. A temperatura inicial da bobina ainda precisa ser informada pelo hotspot.

No modelo normalizado, as condições iniciais são:

```text
x0 = Tbobina_inicial − 27,8
n0 = 30,0 − 27,8 = 2,2
H0 = n0 − 0,1284767 × x0
```

A relação para H0 é uma hipótese de NTC inicialmente estacionário, não uma medição de temperatura da carcaça. Se houver aquecimento/resfriamento rápido, ela precisa ser revista usando a tendência temporal.

Para uma bobina inicialmente entre 27,8 e 35 °C, foram preservadas quatro alternativas. As tabelas numéricas abaixo usam **30 °C na bobina como hipótese provisória**. A principal diferença entre essas alternativas fica no começo da curva; a previsão perto de 13 minutos praticamente converge.

| Tempo ligado | Bobina 11 V | NTC 11 V | Bobina 17 V | NTC 17 V |
|---|---:|---:|---:|---:|
| 01:00 | 56.7 °C | 32.8 °C | 90.9 °C | 36.9 °C |
| 02:00 | 68.9 °C | 35.1 °C | 115.9 °C | 42.1 °C |
| 05:00 | 78.8 °C | 38.3 °C | 134.6 °C | 49.1 °C |
| 10:00 | 81.1 °C | 40.1 °C | 138.7 °C | 53.2 °C |
| 12:56 | 81.4 °C | 40.5 °C | 139.5 °C | 54.2 °C |

Com 17 V ajustados e limite de 4 A, o caso provisório prevê aproximadamente 84,8 °C na bobina / 35,9 °C no NTC após 60 s; 113,3 °C / 41,5 °C após 120 s; e 139,5 °C / 54,2 °C perto dos 13 minutos. A tensão real da fonte começa abaixo dos 17 V enquanto estiver limitada em corrente.

Arquivos `updated_ambient278_*.csv` e `updated_ambient278_ntc30.json` preservam todas as alternativas. O front foi atualizado para separar ambiente real, média inicial do NTC e hotspot inicial opcional. O NTC inicial é capturado pela média das dez leituras recentes. Sem hotspot informado, a bobina inicial é aproximada pelo NTC; isso é uma hipótese, não uma leitura de câmera. A diferença inicial entra como contribuição térmica residual aproximada, sem mudar os parâmetros pre17_v1. A origem dessa diferença não é identificada por uma única leitura.
