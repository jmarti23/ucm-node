idf_component_register(
    SRCS "wifi_test.c" "sen54_data.c" "web_server.c" "app_wifi_prov.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        nvs_flash
        sen5x
        esp_wifi
        esp_event
        esp_netif
        esp_system
	esp_http_server
        mdns
        log
        i2cdev
)
