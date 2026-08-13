idf_component_register(
    SRCS "app_rtc.c"
    INCLUDE_DIRS "."
    REQUIRES app_state board_config i2cdev ds1307
)
