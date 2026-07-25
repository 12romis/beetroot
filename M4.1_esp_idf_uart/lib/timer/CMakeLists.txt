idf_component_register(
    SRCS "timer.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gpio esp_driver_ledc
)
