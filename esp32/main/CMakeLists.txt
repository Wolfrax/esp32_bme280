idf_component_register(
    SRCS
        # "wifi.c"
        "i2c_reader.c"
        # "main.c"
        # "i2c_scanner.c"
        "bme280_check.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        mdns
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        esp_http_client
        esp_driver_i2c
        lwip
        esp_driver_gpio
        esp-tls
        mbedtls
)