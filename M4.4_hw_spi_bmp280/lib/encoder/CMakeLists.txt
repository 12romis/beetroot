idf_component_register(
    SRCS "encoder.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_pcnt
)
