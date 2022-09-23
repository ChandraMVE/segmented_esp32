idf_component_register(SRCS "segment_display_i2c.c"
                       INCLUDE_DIRS "include"
                       REQUIRES spi_flash nvs_flash console adc)
