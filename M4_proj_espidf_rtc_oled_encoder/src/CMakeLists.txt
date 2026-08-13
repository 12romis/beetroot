file(GLOB app_sources "*.c")

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "."  # Поточна папка src уже являється корнем інклюдів
    REQUIRES app_state i2c_bus app_rtc env_sensor rotary_encoder oled
)