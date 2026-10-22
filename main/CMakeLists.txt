idf_component_register(
    SRCS "main.c" "wifi.c" "provision.c" "factory_reset.c" "store.c"
         "sood.c" "moo.c" "json_scan.c" "roon.c"
         "display.c" "cover.c" "ui.c" "font5x7.c" "knob.c" "screen_guard.c"
         "input.c" "touch.c" "motion.c" "power.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES board board_interface esp_wifi esp_netif esp_event nvs_flash esp_lcd
                  esp_driver_gpio esp_timer esp_rom esp_app_format
                  esp_http_server app_update lwip
)
