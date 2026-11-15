idf_component_register(SRCS "app.c" "dsp_usb.c" "protocol.c" "presets.c"
    INCLUDE_DIRS "."
    REQUIRES usb esp_wifi esp_netif esp_event nvs_flash esp_http_server json esp_timer
    EMBED_TXTFILES "index.html" "eq-response.js")
