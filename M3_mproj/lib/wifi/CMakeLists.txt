idf_component_register(
    SRCS "wifi_setup.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event
    PRIV_REQUIRES config nvs_flash
)
