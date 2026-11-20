# WS2812 no painel C3

Configuração física confirmada: **22 LEDs RGB**, 5 V, dados GPIO7, brilho inicial 30%, Aurora suave. Quantidade (1–150), brilho, animação e habilitação ficam em Equalizador → Fita LED, persistidos no S3 e enviados ao C3. O ajuste é independente da habilitação dos potenciômetros. A configuração antiga é migrada uma vez para 22 LEDs, preservando os outros ajustes.

## Conexão

- GPIO7 do C3 → resistor em série de 220–330 ohms → DIN da fita (lado de entrada indicado pela seta).
- GND da fita, da fonte de 5 V e do C3 em comum no painel.
- +5 V da fita na fonte dimensionada para a corrente da fita. Não alimentar a fita pelo pino 3V3.
- A comunicação com o S3 permanece sem fio: a fita não exige ligação de terra com a BLDSP isolada.

O modelo físico foi confirmado em 20/11/2026 como **WS2812 RGB**, não WS2805. O protocolo é 24 bits GRB/MSB primeiro; enviar 40 bits RGBWW embaralhava os pixels e as cores.

O driver usa SPI2 com DMA, MOSI GPIO7 e clock de 3,2 MHz, sem pinos físicos de clock/CS. Cada bit vira quatro bits SPI: `1000` para zero e `1110` para um (312,5/937,5 ns e 937,5/312,5 ns). Ao final, 128 bytes zero mantêm o sinal baixo por 320 us. O buffer DMA contém o quadro inteiro, evitando reabastecimento periódico de símbolos RMT por interrupção. Referências: [datasheet Worldsemi WS2812B](https://cdn-shop.adafruit.com/product-files/4684/4684_WS2812B-2020_V1.3_EN.pdf) e [Espressif: limitações de RMT sem DMA com Wi-Fi](https://github.com/espressif/esp-faq/blob/master/docs/en/software-framework/peripherals/rmt.rst).

## Animações

1. Aurora suave: gradiente cromático com ondulação lenta de brilho, uma volta em aproximadamente 30s.
2. Arco-íris deslizante: uma volta de cores ao longo da fita, deslizando em ciclo de aproximadamente 30s.
3. Respiração cromática: gradiente que se desloca enquanto o brilho respira suavemente.

Startup envia preto, espera 3s e faz fade-in de 1s. Sem resposta recente do principal (>1500ms), o rainbow gira para a esquerda; com enlace, para a direita (índices crescentes). A fase é preservada ao inverter o sentido, sem salto de cor. Diminuir a quantidade limpa os segmentos que deixam de ser usados. Desabilitar envia preto. O brilho configurado limita inclusive o vermelho, com teste de regressão garantindo esse limite.

## CLIP proporcional

O S3 acumula milissegundos de amostras confirmadas abaixo do limiar do detector (não a retenção de 2s do pill). Transmite contador de eventos, total acumulado de tempo baixo, estado atual, uptime e identificador de boot. O C3 usa diferenças dos contadores, portanto enxerga pulsos que ocorreram inteiramente entre dois pacotes. Essa é uma estimativa óptica do LED/LDR; não quantifica THD nem a potência do amplificador, e a inércia do LDR pode alongar a estimativa.

A ocupação recente é uma média exponencial de tempo baixo / tempo decorrido, com constante de 1,8s. Duração baixa recebida e novos eventos geram um envelope de alerta de 0,3 a 1. A retenção cresce de 200 até 2500ms; novos CLIPs renovam o prazo. O flash varia de 2 até 5Hz e aumenta a fração acesa com a gravidade estimada. Alerta forte suprime o rainbow e pisca vermelho/dim vermelho; alerta breve é um acento vermelho sobre a animação. Depois da retenção, o envelope cai exponencialmente com constante de 650ms, sem corte seco.

Mudança de boot do S3 reinicia a interpretação dos contadores. Rollover de uint32 é tratado por subtração modular, e deltas implausíveis/lacunas longas não viram alertas gigantes. Perda do enlace faz o alerta decair e inverte o rainbow para a esquerda. Logs do C3 registram configuração, envelope e ocupação a cada 5s, sem uma linha por quadro.

## Validação

Testes portáveis cobrem pulso entre pacotes, crescimento com duração, fade-out, reset de boot, rollover, limites de RGB/brilho, três modos e deslocamento nos dois sentidos. A emissão física da fita depende de solda e teste do usuário. A proteção de ganho roda separadamente no S3; a animação não substitui nem controla essa proteção.


## Validação física de 20/11/2026

Firmware corrigido entregue por OTA através do S3, C3 confirmado no banco ota_1. Usuário confirmou: “Rainbow suave, sem piscadas estranhas”. Configuração física: 22 LEDs, WS2812 RGB, GRB 24 bits, GPIO7, SPI/DMA. O alerta óptico de CLIP não foi reensaiado nesta sessão; sua política de animação mantém os testes anteriores.


## Feedback ao girar os potenciômetros

A animação usa os cinco controles locais do C3, após a inversão elétrica `65535-adc_position`, independente de terem assumido o ganho no DSP. Assim o LED mostra movimento mesmo durante pickup ou com comandos desativados.

- Mudança acumulada mínima de 328 unidades (~0,5%) evita que pequenas oscilações reiniciem o efeito. Maior deslocamento entre os cinco controles define a direção quando há movimentos simultâneos.
- Aumentar: direita; diminuir: esquerda. Velocidade acompanha a posição final de cada amostra: de uma volta/30 s no mínimo a uma volta/2 s no máximo, suavizada em 150 ms.
- Após o último movimento válido, mantém por **2 s** e retorna ao rainbow padrão em **3 s**, com ease-out cúbico. Retorno usa o sentido do enlace (conectado direita, desconectado esquerda), sem reiniciar fase/cor.
- Chegando a 0%/100% (tolerância 0,5%), faz duas reduções suaves de brilho a 50% do brilho atual: dois ciclos de 500 ms. Ao nascer no extremo não pisca. **2 s sem movimento rearma** o aviso; não exige afastar 2% do extremo, mas só novo movimento significativo pode dispará-lo.
- CLIP é aplicado por cima, sem sofrer redução de brilho do aviso de extremo. Brilho máximo segue a configuração do portal.

O portal distingue `ADC saturado` (RAW >=4090) de `limite da escala de tensão` (mV >=2480, antes da saturação física). A posição de controle continua invertida. Um giro que não altera tensão/posição não é observável pelo firmware.

Testes `strip_motion_test.c` cobrem direção/velocidade, espera/ease-out, tolerância de ruído, dois ciclos de brilho, rearme por silêncio, seleção do maior movimento e reinicialização de canais. `strip_visual_test.c` verifica prioridade do vermelho de CLIP sobre o aviso de extremo.

### Entrega OTA do feedback dos pots — 20/11/2026

Imagem C3 de 841632 bytes, SHA-256 de imagem `aeb1ad155c303ce3902356856f64b11fa0a83bc123c1058193504ebcffb6397c`, enviada pelo S3. O teste físico confirmou novo boot, hash correspondente e estado VALID no banco ota_1 (0x200000), conforme `build/pot-motion-ota-serial.log` aos 63,6 s (`PASS`). O harness temporário de envio automático foi removido do firmware final do S3.

Validação visual do usuário após OTA: “Sim, ficou como esperado” para acompanhamento do giro, espera de 2 s e retorno suave em 3 s. S3 final reiniciou e confirmou enlace autenticado com o C3.

Ajuste posterior: velocidade durante interação multiplicada por **2,5**, inclusive no início do curso: uma volta/12 s no mínimo até 1,25 voltas/s no máximo. Rainbow de repouso continua uma volta/30 s; espera de 2 s, ease-out de 3 s e avisos de extremo preservados.

Entrega OTA do multiplicador 2,5 confirmada: imagem de 841664 bytes, SHA-256 `726d6a9e8bae5dbddd3c500c27e18c4937d33a41a3370ab7d0ba6f516a998e78`, banco ota_0 (0x10000), novo boot/hash/estado VALID verificados em `build/motion-boost-ota-serial.log` (PASS aos 84,8 s).


### Aviso de extremos revisado

Removido o rearme por pausa. Toda nova entrada na faixa de 0% ou 100% dispara o aviso, mesmo após sair e retornar imediatamente. Permanecer parado na faixa não o dispara repetidamente; tolerância de movimento de 0,5% preservada. Qualquer um dos cinco pots pode disparar, mesmo que outro tenha o maior movimento e esteja conduzindo a velocidade. Os dois ciclos agora reduzem o brilho a **20% do configurado** (ex.: configuração de 30% → mínimo de 6%), com fade. Espera de 2 s e ease-out de 3 s da velocidade não mudaram.

OTA da revisão de extremos confirmado: 841616 bytes, SHA-256 de imagem `8aadd89ee9fbb218bc110a8f62536571595d42c25d45ad1fbdd1bc762825c086`, banco ota_1 (0x200000), novo boot/hash/VALID confirmados em `build/endpoint-dim-ota-serial.log` (PASS aos 55,7 s). O S3 foi restaurado a partir do binário de produção preservado antes do teste; fontes e configuração temporária também foram restauradas.


### Correção das chegadas lentas ao extremo

Reproduzido em teste: de 65199 para 65520 (99,49% → 99,98%), o deslocamento de 321 unidades não atingia o limiar de giro de 328 e a piscada era perdida. Detecção de entrada na faixa extrema agora roda em toda amostra válida, independente do limiar de giro; qualquer canal pode disparar sem ser o maior movimento. A velocidade mantém seu filtro de ruído.

Nova chegada durante uma piscada preserva a fase atual e acrescenta dois ciclos completos ao fim do ciclo em andamento, em vez de reiniciar sempre no brilho alto. Assim chegadas rápidas não impedem o fade de alcançar os 20%. Não há temporizador de rearme; ficar parado não repete o evento. Testes cobrem os dois extremos com passos menores que o limiar, canais diferentes, prolongamento sem corte da fase e leitura parada.

Também reproduzido: parar na faixa de tolerância e depois alcançar o 100,0% exibido não gerava novo aviso. Há agora dois níveis por extremo: tolerância de 0,5% e chegada ao 0,0%/100,0% arredondado da interface (32/65535 unidades). Aprofundar no extremo gera novo evento; sair para um nível menos extremo não dispara. A fase dos fades continua preservada entre os eventos.

OTA da correção validado: 841872 bytes, SHA-256 `01fa31eb15be89177f8f0de7e8048a50ea8da4b4f1435d4cf17e1b3ac95345f4`, banco ota_0 (0x10000), novo boot/hash/VALID confirmados em `build/endpoint-fix-ota-serial.log` (PASS aos 52,2 s).

Validação física do usuário após repetir chegadas lentas ao extremo: “Agora piscou em todas as chegadas”.


## Paletas independentes dos efeitos

Onze paletas originais disponíveis em Painel → Fita LED: Rainbow clássico, Oceano, Fogo, Aurora boreal, Pôr do sol, Synthwave, Floresta, Gelo, Algodão doce, Dourado e Brasa. Podem ser combinadas com os três modos existentes; o seletor tem prévia de gradiente. **Salvar fita** aplica e persiste no S3. A separação de efeitos/paletas segue a ideia da [interface do WLED](https://kno.wled.ge/features/palettes/), sem instalar WLED ou copiar seu catálogo.

As paletas são gradientes RGB cíclicos com interpolação entre seis cores, em `shared/strip_palette.h`; ID 0 preserva o rainbow HSV anterior. A direção, velocidade dos pots, avisos de extremos e CLIP continuam independentes da paleta. O vermelho de CLIP tem prioridade e, nos pulsos ativos, usa RGB (255, 0, 0), independente do brilho decorativo.

NVS de fita migrada à versão 3, mantendo 12 bytes e os ajustes anteriores; versões 1/2 passam a paleta 0. Campo opcional `palette` em `/api/strip` (0–10); clientes antigos que não enviam o campo preservam a paleta atual. Envio ao C3 usa o byte antes reservado `panel_reply_t.reserved[0]`, sem mudar tamanho ou versão dos pacotes. IDs inválidos no receptor voltam a 0.

Entrega das paletas por OTA validada: 842640 bytes, SHA-256 `b3ddc233b1e0042f1d51a0ac05c91f4967b0176944c022b4c03cbe64129405fc`, banco ota_1 (0x200000), novo boot/hash/VALID confirmados em `build/palettes-ota-serial.log` (PASS aos 51 s).

## CLIP com potência máxima

Pulsos de CLIP usam vermelho puro 255 inclusive com brilho decorativo em 0%. Duração/repetição seguem os contadores existentes; fade-out de 650 ms após a retenção, com intensidade decrescente apenas nessa cauda. Paletas, brilho normal e sinalização dos pots não mudam. Desabilitar a fita continua apagando tudo.

## Quadros a 120 Hz

Renderização e envio agora são disparados por esp_timer a cada 8333 us (aproximadamente 120 Hz), substituindo espera de 40 ms após cada envio (cerca de 25 Hz). O temporizador apenas acorda a tarefa; cálculo/SPI continuam fora do callback. Notificações acumuladas são descartadas, sem replay de quadros atrasados. Fases, fades e velocidade dos pots continuam baseados em tempo decorrido. ADC e ESP-NOW mantêm suas cadências. Envio de 22 LEDs mais reset leva aproximadamente 0,98 ms; 150 LEDs mais reset, 4,82 ms. Taxa-alvo, sujeita ao escalonamento do sistema. Teste compara deslocamento e decaimento após 1 s em 25 e 120 quadros.

OTA da versão 120 Hz concluído em 20/11/2026 pelo pipeline: imagem C3 842656 bytes, novo boot/SHA-256/banco 0x200000 confirmados aos 53,2 s. S3 de produção restaurado e NVS preservada. Registro `build/pipeline/serial-20261120-130129.log`. Taxa-alvo de 120 Hz configurada; não medida externamente com osciloscópio.

### Indicação de desconexão

Sem enlace com o S3, o C3 gira à esquerda a uma volta a cada 8 segundos e respira em ciclos de 4 segundos entre 30% e 100% do brilho decorativo. O estado desconectado tem prioridade sobre o movimento dos pots. Conectado, mantém o giro normal à direita e os efeitos dos controles. O alerta de CLIP continua independente do brilho decorativo.
