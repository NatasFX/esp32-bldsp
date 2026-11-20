# Modelo da fita corrigido

A fita foi inicialmente informada como WS2805, mas em 20/11/2026 o usuário confirmou **WS2812 RGB com 22 LEDs**. O protocolo antigo de 40 bits foi substituído por 24 bits GRB com SPI/DMA.

A documentação atual está em [Fita WS2812](fita_ws2812.md). Os logs anteriores de RMT sem erro não validavam o formato físico da fita.
