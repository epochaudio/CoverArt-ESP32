idf_component_register(SRCS "board.c" INCLUDE_DIRS "include"
                       REQUIRES board_interface board_common esp_lcd esp_driver_gpio)
