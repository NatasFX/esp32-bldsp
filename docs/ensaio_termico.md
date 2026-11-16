# Ensaio térmico e aquisição serial

Abra `http://192.168.4.1/thermal-test`, ou o novo link **Ensaio térmico** no topo do front. O modelo `pre17_v1` foi fixado usando apenas o primeiro vídeo; [estudo, equações e previsões anteriores ao segundo teste](ensaios/20261116/multinode/estudo_pre17_v1.md).

## Aquisição

- GPIO18 / NTC B3950 permanece na mesma curva conferida contra o multímetro. A leitura medida e a bobina prevista são campos separados.
- Uma amostra por segundo, média móvel de dez leituras de temperatura. Leituras inválidas ou lacunas acima de 2,5 s reiniciam a janela; a contagem aparece na tela.
- Serial 115200, uma linha `THERMAL` por segundo, mesmo sem iniciar registro no front: uptime, validade, °C, média, contagem, RAW, mV, sessão, fase e previsões quando disponíveis. São médias temporais de 1 Hz, não dez conversões consecutivas do ADC.
- Ensaio em RAM do ESP: até 2.700 amostras / aproximadamente 45 min, com alternativa de 1.800 / 30 min se a primeira alocação falhar. Cheio, o registro para preservando os dados. Até 96 marcações. A aquisição não depende da página aberta.
- CSV e JSON podem ser baixados após reabrir a página. Reiniciar o ESP perde a RAM; iniciar novo ensaio substitui o anterior. Calibração LDR e presets continuam no NVS.
- As marcações de fonte são apenas registros: o front não aciona a fonte nem muda o ganho do DSP.

## Procedimento

1. Manter NTC e orientação do falante. Aguardar resfriamento; registrar a referência fria e a posição da sonda adicional, se disponível. A segunda sonda pode ser a do multímetro, sem novo GPIO.
2. Na página, informar tensão real da fonte e limite de corrente. `0 A` no campo significa fonte sem limitação no modelo, não corrente medida zero. Padrão inicial: 17 V / limite 4 A. Não é uma instrução de capacidade segura do equipamento.
3. Esperar dez leituras válidas e clicar **Iniciar registro**. Gravar alguns segundos de linha de base. Ligar fisicamente a fonte e clicar **Marcar fonte ligada** no mesmo instante.
4. Gravar o hotspot da bobina. Registrar manualmente tensão/corrente observadas e temperaturas de câmera/segunda sonda em momentos identificáveis. Essas referências são evidências; não reajustam a previsão durante o teste.
5. Desligar fisicamente e marcar **fonte desligada**. Continuar registrando o resfriamento. Encerrar o registro e exportar antes de reiniciar. Trechos curtos também permitem validar o início da curva.

O modelo prevê o ensaio DC estacionário com a mesma fiação. Não mede potência real automaticamente, não infere a temperatura da bobina apenas do NTC e não está validado com música/ventilação. O resíduo do NTC compara a média medida com a média das previsões nos mesmos registros. Uma lacuna de integração maior que cinco segundos invalida a previsão da sessão.

## API

- `GET /api/thermal-test`: status, média, previsão, espaço e fase.
- `POST /api/thermal-test`, cabeçalho `X-BLDSP: 1`: `action` = `start`, `on`, `off`, `finish`, `reference`.
- `start`: `voltage` 1–24 V, `limit` 0–10 A, `ambient` 0–60 °C, `location` opcional (até 80 bytes). Exige dez leituras recentes. Não permite substituir um registro ativo.
- `reference`: qualquer combinação de `voltage`, `current`, `coil`, `body`; ao menos um valor. Referências de corrente e tensão são medições manuais; não alteram silenciosamente a configuração inicial do modelo.
- `GET /api/thermal-test/export`: JSON transmitido em blocos, com metadados, registros cronológicos e marcações. O navegador gera o CSV com aspas e escape de localização.

O dispositivo conserva apenas buffers limitados. Não aloca uma árvore JSON por amostra de exportação. Predições no arquivo têm precisão de armazenamento de 0,01 °C, o que não significa exatidão física de 0,01 °C.

## Validação de implementação

`tests/thermal_model_test.c` compara a integração RK4 em C com previsões congeladas de SciPy para 11 V, 17 V e 17 V limitado a 4 A; inclui resfriamento e rejeição de intervalo inválido. Demais testes nativos de EQ, fonte, RGB, telemetria e front permanecem aplicáveis. O teste físico de 17 V valida o modelo; esses testes de software validam a implementação das equações.
