idf_component_register(
    SRCS "env_sensor.c"
    INCLUDE_DIRS "."
    REQUIRES app_state board_config i2cdev bme280
)
