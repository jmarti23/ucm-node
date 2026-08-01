idf_component_register(
    SRCS "node_main.c" "sen54_data.c" "web_server.c" "app_wifi_prov.c" "ethernet_manager.c" "ethernet_init.c" "network_manager.c" "mqtt_manager.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        nvs_flash
        sen5x
        esp_wifi
        esp_event
        esp_netif   
        esp_system
        esp_eth
	esp_http_server
        mdns
        log
        i2cdev
)
