# Мінімальний локальний компонент на базі оригінального olikraus/u8g2
# (лише "чисте" графічне ядро — жодного знання про I2C/SPI).
# Транспорт (u8x8_byte_cb / u8x8_gpio_and_delay_cb) реалізовано окремо в app.c
# через уже наявний i2cdev, щоб OLED ділив одну I2C-шину з RTC і BMP280.

file(GLOB U8G2_CORE_SOURCES "core/*.c")

set(U8G2_DRIVER_SOURCES
    "driver/u8g2_d_setup.c"
    "driver/u8g2_d_memory.c"
    "driver/u8x8_d_ssd1306_128x64_noname.c"
)

idf_component_register(
    SRCS ${U8G2_CORE_SOURCES} ${U8G2_DRIVER_SOURCES}
    INCLUDE_DIRS "core"
)
