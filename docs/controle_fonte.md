# Troca de fonte por contato — primeiro comando manual

Implementação inicial: botão **Fonte de áudio → Trocar fonte**, POST `/api/source/pulse` com corpo `{}` e cabeçalho `X-BLDSP: 1`. Aciona GPIO4 em open-drain por 100 ms, depois libera a linha. Não produz nível alto ativo. Não altera duração ou pino por requisição. Logs `source` registram início e liberação com duração observada.

O contato é um comando de alternância, não um comando idempotente “selecionar Bluetooth”. Por isso não há pulso no boot, repetição automática por falha HTTP, pulso em troca de preset nem em reconexão USB. Há bloqueio durante o pulso e intervalo mínimo de 1,5 s entre inícios; o front também bloqueia cliques repetidos. `accepted` confirma agendamento do pulso, não confirma qual fonte ficou ativa. O retorno USB e a música devem ser conferidos após acionar.

## Ligação a validar antes de acionar

O usuário mediu a linha de controle em 3,3 V em repouso e confirmou que um pull-down de 100 ms troca a fonte. GPIO4 é reservado para esse comando, mas a ligação física ainda precisa ser confirmada.

A implementação atual é para contato ativo em nível baixo: solta em repouso, afunda corrente durante o pulso. Ligação direta só deve ser considerada com referência GND comum, linha realmente de 3,3 V e corrente do contato compatível com o GPIO. Não ligar a saída a 5/12/16 V, não usar push-pull alto contra a linha e não usar pinos EN/BOOT do ESP. A condição com um dos lados desligado também precisa ser avaliada para não alimentar o outro por um pino de sinal.

Se for necessário manter os domínios galvanicamente separados, usar optoacoplador com transistor do lado BLDSP entre contato e GND local, acionado pelo ESP com resistor adequado. Essa montagem pode exigir polaridade ativa alta no GPIO, portanto o firmware atual open-drain não deve ser ligado arbitrariamente ao LED do opto. Confirmar circuito e adaptar polaridade antes do teste. Um transistor/MOSFET com GND comum também exige definição do circuito e polaridade.

GPIO4 não é o fio de 3V3: não conectar o barramento 3V3 do ESP ao contato. O pull-up existente na BLDSP mantém o repouso; o firmware desabilita os pulls internos do ESP.

## Validação

1. Confirmar GPIO/circuito e referência elétrica com o usuário; verificar linha solta em repouso, inclusive durante reset.
2. Com volume baixo, clicar uma vez e observar a troca de fonte e os logs `source` de pulso/liberação. Medir pulso com osciloscópio/analisador se disponível. O temporizador é de software: 100 ms nominal, liberação pode atrasar sob carga; não é um corte de proteção em tempo real.
3. Confirmar Bluetooth e USB HID depois da troca. A reconexão USB reaplica preset 1 conforme comportamento existente.
4. Verificar que recarregar página, reconectar Wi-Fi ou clicar duas vezes não produz pulsos extras. Se uma requisição perder a resposta, conferir a fonte antes de repetir manualmente.
5. Só depois estudar troca automática no boot com evidência confiável da fonte. Não usar presença de áudio ou ausência de Bluetooth como sinal presumido.

A prévia responde sem acionar hardware. Testes de front verificam ausência de pulso no boot, requisição explícita e bloqueio de repetição. Compilação não valida ligação elétrica nem tempo físico do pulso.
