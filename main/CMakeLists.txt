idf_component_register(
    SRCS "wifi_test.c" "sen54_data.c" "web_server.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        wifi_provisioning
        nvs_flash
        sen5x
        esp_wifi
        esp_event
        esp_netif
        esp_system
	esp_http_server
        log
        i2cdev
)
