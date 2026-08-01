idf_component_register(
    SRCS "bmp280.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_spi esp_driver_gpio
)
