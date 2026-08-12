file(GLOB app_sources "*.c")

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "."  # Поточна папка src уже являється корнем інклюдів
    REQUIRES driver knob button u8g2
)