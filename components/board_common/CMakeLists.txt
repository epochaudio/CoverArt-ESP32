# Helpers shared by several boards; a board lists this component only if it uses one.
idf_component_register(SRCS "spi_dc_panel.c" "qmi8658.c" "axp2101.c" INCLUDE_DIRS "include"
                       REQUIRES esp_lcd esp_driver_spi esp_driver_i2c)
