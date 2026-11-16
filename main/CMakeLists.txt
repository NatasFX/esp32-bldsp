idf_component_register(SRCS "app.c" "dsp_usb.c" "protocol.c" "presets.c" "source_control.c" "source_policy.c" "status_led.c" "status_visual.c" "telemetry.c" "telemetry_math.c" "monitor_log.c"
    INCLUDE_DIRS "."
    REQUIRES usb esp_wifi esp_netif esp_event nvs_flash esp_http_server json esp_timer esp_driver_gpio esp_driver_rmt esp_adc
    EMBED_TXTFILES "index.html" "eq-response.js")
