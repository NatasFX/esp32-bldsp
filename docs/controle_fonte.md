# Seleção automática de Bluetooth por contato

GPIO reservado: **GPIO2** do ESP32-S3, open-drain, com pull-up/pull-down internos desabilitados. O usuário mediu 3,3 V em repouso e confirmou troca de fonte ao puxar o contato para GND por 100 ms.

## Funcionamento

1. Ao iniciar, o ESP libera GPIO2; não aplica pulso antes de reconhecer a BLDSP no USB.
2. Depois de verificar VID 8888, PID conhecido e descritor HID compatível, registra a enumeração.
3. Se o PID for **1719** (modo com áudio USB observado nesta placa), espera 1 s estável e aplica nível baixo no GPIO2 por 100 ms; depois solta a linha.
4. Aguarda enumeração como **171E**, o modo HID/Bluetooth observado nesta placa. Somente então libera a aplicação do preset 1 em estéreo e subwoofer.
5. A mudança esperada para 171E confirma a seleção e não gera outro pulso. Reconectar já em 171E também não alterna a fonte.
6. Se a placa posteriormente voltar a 1719, tenta recuperar Bluetooth novamente. Sem USB não há pulso.
7. Aguarda pelo menos 5 s entre tentativas automáticas e limita a três tentativas sem confirmação. Depois registra falha e para. A confirmação em 171E renova o orçamento para uma regressão futura. Simples desconexões/reconexões em 1719 não burlam o limite.

O PID confirma a apresentação USB esperada deste equipamento; não confirma conexão do celular, transmissão de música ou som nas saídas. Não interpretar qualquer mudança de USB como motivo para alternar: isso criaria um ciclo entre fontes.

Os logs `source` mostram espera pelo USB, modo reconhecido, motivo/tentativa do pulso, liberação e duração medida, confirmação do alvo ou esgotamento das tentativas. O temporizador de software tem 100 ms nominais e pode liberar com algum atraso sob carga; não é um mecanismo de corte de proteção em tempo real.

## Comando de recuperação no celular

**Fonte de áudio → Ativar Bluetooth** envia POST `/api/source/pulse`, corpo `{}`, cabeçalho `X-BLDSP: 1`. Se já está em 171E, responde sem pulsar. Se está em 1719, permite nova tentativa explícita, inclusive depois de esgotadas as automáticas. Sem USB conhecido, recusa. Não há repetição automática de requisição HTTP por perda de resposta. Há bloqueio durante o pulso e intervalo mínimo de 1,5 s no acionamento, além da espera entre tentativas automáticas.

`accepted` confirma agendamento, não o resultado da troca. A confirmação real aparece no log quando o USB enumera em 171E. A interface não permite escolher pino/duração por requisição. A prévia simula aceitação e não controla hardware.

## Ligação física a confirmar

Usar o pino rotulado **2 / IO2 / GPIO2**, não a segunda posição do conector nem o pino 3V3. O firmware nunca aplica nível alto ativamente: fica solto e só afunda corrente durante o pulso. O pull-up existente na BLDSP mantém o repouso.

Uma ligação direta ao contato exige referência GND comum, linha realmente de 3,3 V e corrente compatível com o GPIO. A condição com um dos lados desligado deve ser avaliada para evitar alimentação pelo pino. Não conectar sinais de 5/12/16 V. Fazer a solda com ambas as placas sem alimentação.

Se for necessário manter domínios galvanicamente separados, usar optoacoplador com transistor entre contato e GND da BLDSP, e circuito adequado no lado ESP. Esse circuito pode exigir polaridade ativa alta: não conectar um opto arbitrariamente ao firmware open-drain sem adaptar a polaridade. Transistor/MOSFET com GND comum também exige circuito/polaridade definidos. Não tratar alimentação isolada como prova de isolamento dos cabos USB/áudio.

## Testes e pendências

Testes nativos usam a lógica C real (`source_policy.c`): ausência de pulso sem USB, espera estável, confirmação, reconexão já no alvo, recuperação de regressão, cooldown, limite de tentativas e rejeição de PID desconhecido. Teste do front confere comando explícito e bloqueio de cliques repetidos. Build completo ESP-IDF deve passar antes da gravação.

Pendente: solda e confirmação do pulso físico no GPIO2, boot com BLDSP inicialmente em 1719, chegada a 171E e continuidade do áudio Bluetooth. Testar power-cycle, reconexão e retorno involuntário ao modo USB. Se o contato ainda não estiver ligado, as tentativas não terão efeito e o limite produzirá aviso no log; isso não é falha da enumeração HID.
