idf_component_register(
    SRCS "mqtt.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES config mbedtls
)
