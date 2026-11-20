# Portal cativo e acesso local

O AP BLDSP-Control responde consultas DNS IPv4 com o endereço do próprio ESP e redireciona caminhos HTTP desconhecidos para http://192.168.4.1/. O componente DNS foi copiado do exemplo captive_portal do ESP-IDF 5.4 instalado, com sua licença original preservada.

A intenção é provocar a tela de entrada da rede no celular. O sistema pode abrir automaticamente ou mostrar uma notificação para entrar; depende do aparelho e das configurações. Não há acesso à internet, proxy nem interceptação HTTPS. O endereço manual continua disponível. Abertura automática e coexistência com dados móveis ainda precisam ser verificadas no Samsung S24 FE após gravar este firmware.

Na interface, a temperatura estimada da bobina fica nas informações do indicador CLIP: normal até 70 °C, vermelha acima de 70 °C e maior acima de 130 °C. Esses limites alteram apenas a apresentação; não ativam proteção térmica. O NTC real continua no rodapé.
