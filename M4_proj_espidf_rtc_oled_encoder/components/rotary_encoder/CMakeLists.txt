idf_component_register(
    SRCS "rotary_encoder.c"
    INCLUDE_DIRS "."
    REQUIRES app_state board_config button encoder
)
