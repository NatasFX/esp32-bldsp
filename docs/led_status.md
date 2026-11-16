# LED RGB de status

LED endereçável da placa no **GPIO48**, confirmado pelo usuário. Implementado via RMT nativo do ESP32-S3, sem dependência externa. Ordem GRB, uma posição, atualização de aproximadamente 25 Hz em tarefa de prioridade baixa. Cada componente RGB fica limitado a 128/255 (~50% do comando máximo); isso não é uma medição de potência elétrica ou brilho percebido.

| Indicação | Significado |
|---|---|
| Azul com respiração | Inicialização dos serviços |
| Amarelo com respiração | Sem BLDSP USB reconhecida |
| Roxo com respiração | BLDSP em áudio USB; aguardando troca para Bluetooth/HID |
| Ciano com respiração | Bootstrap/aplicação de EQ ainda pendente ou requisição de ajuste em execução |
| Vermelho piscando lentamente | Falha de troca de fonte, bootstrap ou confirmação de EQ |
| Arco-íris suave (ciclo ~5,12 s) | Serviços iniciados, USB presente em 171E e preset/EQs confirmados, sem operação pendente ou erro conhecido |

O arco-íris não exige celular conectado ao Wi-Fi. Também não confirma conexão Bluetooth do celular, presença de música, funcionamento do amplificador ou sensores futuros: esses sinais não são medidos pelo firmware atual.

Desconectar o USB tira o LED de operacional. Falhas de aplicação são mantidas por saída até uma confirmação válida dessa saída ou reinicialização da conexão USB; confirmar a outra saída não apaga o erro. Falha transitória no bootstrap pode aparecer em vermelho antes da tentativa seguinte ter sucesso. Fonte em 1719 fica roxa; após esgotar tentativas, vermelha.

RMT envia os 24 bits e reset com temporização de hardware. O buffer é persistente e só é reutilizado após fim da transmissão; uma falha no indicador é registrada e interrompe a tarefa do LED, sem abortar o controle de áudio. Nenhuma tarefa de animação bloqueia a tarefa USB ou o temporizador de 100 ms do GPIO2. Não compartilhar GPIO48 com outro circuito. Se o jumper de alimentação/controle RGB desta revisão estiver aberto, o software não o substitui.

Os logs `status_led` registram GPIO, limite de brilho e transições de estado, sem gerar uma linha por quadro. A lógica de prioridade, ciclo de cores e limite de 128 são exercitados pelo teste C real `status_visual_test.c`. A emissão física de luz requer conferência do usuário.
