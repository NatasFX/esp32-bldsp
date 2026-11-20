idf_component_register(
    SRCS
        "app.c"
        "wifi_power.c"
        "panel_control.c"
        "lighting.c"
        "light_policy.c"
        "panel_ota.c"
        "knob_policy.c"
        "dsp_usb.c"
        "protocol.c"
        "presets.c"
        "source_control.c"
        "source_policy.c"
        "status_led.c"
        "status_visual.c"
        "telemetry.c"
        "clip_detector.c"
        "clip_health.c"
        "clip_alert.c"
        "protection_policy.c"
        "clip_protection.c"
        "telemetry_math.c"
        "monitor_log.c"
        "thermal_model.c"
        "thermal_observer.c"
        "thermal_test.c"
    INCLUDE_DIRS "." "../shared"
    REQUIRES dns_server usb esp_wifi esp_netif esp_event nvs_flash esp_http_server json esp_timer esp_driver_gpio esp_driver_rmt esp_adc
    EMBED_TXTFILES "panel-update.html" "index.html" "eq-response.js" "thermal-test.html")

# Deployment bridge is built separately by tools/fw.py, never in production.
if(BLDSP_OTA_BRIDGE_IMAGE)
    set_source_files_properties("panel_ota.c" "app.c" PROPERTIES COMPILE_DEFINITIONS PANEL_OTA_SMOKE_TEST=1)
    target_add_binary_data(${COMPONENT_LIB} "${BLDSP_OTA_BRIDGE_IMAGE}" BINARY RENAME_TO panel_smoke_bin)
endif()
