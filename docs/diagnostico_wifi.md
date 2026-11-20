# Ruído de áudio relacionado ao Wi-Fi — teste de 17/11/2026

O usuário relatou um “tec” baixo, aproximadamente cinco vezes por segundo, com ESP e BLDSP alimentados no lado isolado. Com USB do PC conectado, confirmou que o ruído continuava.

Foi gravado temporariamente um diagnóstico A/B que manteve USB, sensores, logs e RGB funcionando: Wi-Fi ligado por 30 s, desligado por 60 s e religado. Logs confirmaram PHASE OFF em aproximadamente 30,8 s de boot e PHASE RESTORED em 90,8 s. Durante o período OFF o usuário confirmou que o ruído sumiu.

Isso relaciona o ruído ao funcionamento do Wi-Fi. Ainda não separa acoplamento irradiado de perturbação da alimentação: desligar o rádio altera ambos. Não foi medida a potência RF nem gravado o áudio.

## Mitigação em teste

O firmware normal passou a chamar `esp_wifi_set_max_tx_power(8)` após `esp_wifi_start()` e registrar a leitura de volta com `esp_wifi_get_max_tx_power`. A API usa unidades de 0,25 dBm; 8 corresponde ao menor limite documentado pelo SDK instalado, 2 dBm. Trata-se do limite configurado, não de medição física de potência.

Não muda canal, beacons, sensores, USB, RGB ou a lógica de EQ/CLIP. Afeta a transmissão do ESP; não controla a potência de transmissão do celular. Pode reduzir o alcance. Verificar tanto o ruído quanto a estabilidade dos ajustes no local de uso.

O código de alternância A/B foi retirado da fonte normal antes desta compilação. Seus logs e binário de retorno ficaram localmente em build/wifi-ab, fora do Git. Nenhuma alteração de NVS ou firmware da BLDSP foi usada para o diagnóstico. O resultado acústico com o limite de 2 dBm ainda depende do teste do usuário.

## Potência configurável — 19/11/2026

Equalizador → Wi-Fi · potência de transmissão permite aplicar e salvar o limite do rádio do ESP principal na NVS. A alteração não reinicia Wi-Fi nem USB e afeta tanto AP como ESP-NOW do S3. O C3 permanece em 2 dBm. Botão de retorno rápido aplica/salva 2 dBm.

São expostos os passos do driver: 8,20,28,34,44,52,56,60,66,72,80 em unidades de 0,25dBm (2;5;7;8,5;11;13;14;15;16,5;18;20dBm). A API confirma com esp_wifi_get_max_tx_power. Esses valores indicam limite configurado, não medição da potência irradiada; PHY, modulação e restrições de rádio podem limitar a emissão efetiva.

GET/POST /api/wifi. POST exige X-BLDSP:1 e {"power_qdbm":8}, por exemplo. Só aceita passos conhecidos; aplica, lê de volta e grava em radio/tx_qdbm. Se a gravação falhar, restaura o limite anterior e devolve erro. Sem configuração válida salva, inicia em 2dBm.
