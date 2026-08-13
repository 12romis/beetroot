idf_component_register(
    SRCS "oled.c"
    INCLUDE_DIRS "."
    REQUIRES app_state board_config i2cdev u8g2
)
