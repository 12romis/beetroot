file(GLOB app_sources "*.c")

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "."  # Текущая папка src уже является корнем инклудов
    REQUIRES driver ssd1306 knob button
)