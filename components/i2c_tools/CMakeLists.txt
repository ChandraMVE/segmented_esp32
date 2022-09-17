idf_component_register(SRCS "cmd_i2ctools.c"
                       INCLUDE_DIRS "include"
                       REQUIRES spi_flash nvs_flash console)
