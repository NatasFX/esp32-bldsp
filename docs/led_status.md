# LED RGB de status

LED endereçável da placa no **GPIO48**, confirmado pelo usuário. Ao iniciar o aplicativo, transmite quadro preto explícito para apagar o LED, inclusive em reset com o WS2812 ainda alimentado. Nenhuma animação começa antes da inicialização dos serviços mais 3 s; então o brilho sobe gradualmente por 1 s. A espera acontece só na tarefa do indicador, sem atrasar Wi-Fi, USB ou controle de fonte. O firmware não controla o LED antes de entrar no aplicativo/driver. Implementado via RMT nativo do ESP32-S3, sem dependência externa. Ordem GRB, uma posição, atualização de aproximadamente 25 Hz em tarefa de prioridade baixa. Cada componente RGB fica limitado a 128/255 (~50% do comando máximo); isso não é uma medição de potência elétrica ou brilho percebido.

| Indicação | Significado |
|---|---|
| Apagado na partida | Aguarda os serviços e mais 3 s de estabilização; depois acende gradualmente em 1 s |
| Amarelo com respiração | Sem BLDSP USB reconhecida |
| Roxo com respiração | BLDSP em áudio USB; aguardando troca para Bluetooth/HID |
| Ciano com respiração | Bootstrap/aplicação de EQ ainda pendente ou requisição de ajuste em execução |
| Vermelho piscando lentamente | Falha de troca de fonte, bootstrap ou confirmação de EQ |
| Arco-íris suave (ciclo ~5,12 s) | Serviços iniciados, USB presente em 171E e preset/EQs confirmados, sem operação pendente ou erro conhecido |

O arco-íris não exige celular conectado ao Wi-Fi. Também não confirma conexão Bluetooth do celular, presença de música, funcionamento do amplificador ou sensores futuros: esses sinais não são medidos pelo firmware atual.

Desconectar o USB tira o LED de operacional. Falhas de aplicação são mantidas por saída até uma confirmação válida dessa saída ou reinicialização da conexão USB; confirmar a outra saída não apaga o erro. Falha transitória no bootstrap pode aparecer em vermelho antes da tentativa seguinte ter sucesso. Fonte em 1719 fica roxa; após esgotar tentativas, vermelha.

RMT envia os 24 bits e reset com temporização de hardware. O buffer é persistente e só é reutilizado após fim da transmissão; uma falha no indicador é registrada e interrompe a tarefa do LED, sem abortar o controle de áudio. Nenhuma tarefa de animação bloqueia a tarefa USB ou o temporizador de 100 ms do GPIO2. Não compartilhar GPIO48 com outro circuito. Se o jumper de alimentação/controle RGB desta revisão estiver aberto, o software não o substitui.

Os logs `status_led` registram GPIO, limite de brilho e transições de estado, sem gerar uma linha por quadro. A lógica de prioridade, ciclo de cores e limite de 128 são exercitados pelo teste C real `status_visual_test.c`. A emissão física de luz requer conferência do usuário.

## Gravação e verificação por serial — 15/11/2026

Build completo passou, firmware foi gravado na COM7 e os hashes foram conferidos pelo esptool. Driver RMT iniciou e a tarefa RGB registrou a sequência de estados. Houve falha inicial de enumeração USB, recuperada automaticamente; depois:

```text
I (9776) status_led: State: switching source purple
I (10766) source: Automatic pulse GPIO2 low 100 ms, attempt=1; awaiting USB 171e
I (10866) source: GPIO2 released; pulse elapsed=100066 us
I (12056) status_led: State: sync cyan
I (12456) status_led: State: error red
I (14846) dsp_usb: Preset 1 applied to stereo + sub; no DSP flash save
I (14856) status_led: State: operational rainbow
```

O vermelho transitório correspondeu a divergência da primeira leitura do módulo 91; a repetição do bootstrap confirmou as duas saídas e então liberou o arco-íris. Não houve erro RMT nessa captura. Emissão física e correspondência das cores ainda devem ser confirmadas pelo usuário, pois o computador só observa o comando e os logs.


## Partida com LED apagado — 16/11/2026

Usuário relatou suspeita de brownout na partida. Alterado para quadro RGB zero imediato na inicialização do driver, retenção apagado até 3 s após serviços prontos e rampa de 1 s até o brilho configurado. Testes C cobrem ausência de luz antes do prazo, início em zero e rampa até o limite. Nos logs disponíveis havia resets POWERON, sem mensagem explícita de brownout; causa elétrica não confirmada. A mudança reduz a iluminação durante a partida, mas não valida por si só a fonte de alimentação.
