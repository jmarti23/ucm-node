/*
 * UCM Ethernet Initialization
 *
 * Target:
 *   ESP32-P4
 *   Waveshare ESP32-P4-WIFI6-POE-ETH
 *
 * PHY:
 *   IP101
 *
 * ESP-IDF:
 *   5.5.5
 */

#include <stdlib.h>

#include "esp_log.h"
#include "esp_check.h"

#include "esp_eth.h"
#include "esp_eth_mac_esp.h"
#include "esp_eth_phy.h"

#include "ethernet_init.h"


static const char *TAG = "ucm_eth";


/*
 * Waveshare ESP32-P4-WIFI6-POE-ETH
 *
 * IP101 RMII PHY
 */

#define ETH_PHY_ADDR       1

#define ETH_MDC_GPIO       31
#define ETH_MDIO_GPIO      52

#define ETH_PHY_RESET_GPIO 51



static esp_eth_handle_t eth_init_internal(void)
{
    esp_err_t ret = ESP_OK;

    esp_eth_handle_t eth_handle = NULL;

    esp_eth_mac_t *mac = NULL;
    esp_eth_phy_t *phy = NULL;


    /*
     * Configure MAC
     */

    eth_mac_config_t mac_config =
        ETH_MAC_DEFAULT_CONFIG();


    eth_esp32_emac_config_t emac_config =
        ETH_ESP32_EMAC_DEFAULT_CONFIG();


    emac_config.smi_gpio.mdc_num =
        ETH_MDC_GPIO;

    emac_config.smi_gpio.mdio_num =
        ETH_MDIO_GPIO;



    mac = esp_eth_mac_new_esp32(
        &emac_config,
        &mac_config);


    ESP_GOTO_ON_FALSE(
        mac != NULL,
        ESP_ERR_NO_MEM,
        err,
        TAG,
        "Failed creating MAC");


    /*
     * Configure IP101 PHY
     */

    eth_phy_config_t phy_config =
        ETH_PHY_DEFAULT_CONFIG();


    phy_config.phy_addr =
        ETH_PHY_ADDR;

    phy_config.reset_gpio_num =
        ETH_PHY_RESET_GPIO;


    phy = esp_eth_phy_new_ip101(
        &phy_config);


    ESP_GOTO_ON_FALSE(
        phy != NULL,
        ESP_ERR_NO_MEM,
        err,
        TAG,
        "Failed creating IP101 PHY");



    /*
     * Install Ethernet driver
     */

    esp_eth_config_t eth_config =
        ETH_DEFAULT_CONFIG(
            mac,
            phy);


    ret = esp_eth_driver_install(
        &eth_config,
        &eth_handle);


    ESP_GOTO_ON_ERROR(
        ret,
        err,
        TAG,
        "Ethernet driver install failed");


    return eth_handle;



err:

    if (mac) {
        mac->del(mac);
    }

    if (phy) {
        phy->del(phy);
    }

    return NULL;
}




esp_err_t ucm_eth_init(
    esp_eth_handle_t **eth_handles_out,
    uint8_t *eth_cnt_out)
{
    ESP_RETURN_ON_FALSE(
        eth_handles_out != NULL &&
        eth_cnt_out != NULL,
        ESP_ERR_INVALID_ARG,
        TAG,
        "Invalid arguments");


    esp_eth_handle_t *handles =
        calloc(1, sizeof(esp_eth_handle_t));


    ESP_RETURN_ON_FALSE(
        handles != NULL,
        ESP_ERR_NO_MEM,
        TAG,
        "Failed allocating handles");



    handles[0] =
        eth_init_internal();



    if (handles[0] == NULL) {

        free(handles);

        return ESP_FAIL;
    }


    *eth_handles_out = handles;

    *eth_cnt_out = 1;


    ESP_LOGI(
        TAG,
        "Ethernet initialized");


    return ESP_OK;
}




esp_err_t ucm_eth_deinit(
    esp_eth_handle_t *eth_handles,
    uint8_t eth_cnt)
{
    ESP_RETURN_ON_FALSE(
        eth_handles != NULL,
        ESP_ERR_INVALID_ARG,
        TAG,
        "NULL handles");


    for (int i = 0; i < eth_cnt; i++) {

        if (eth_handles[i]) {

            esp_eth_driver_uninstall(
                eth_handles[i]);
        }
    }


    free(eth_handles);


    return ESP_OK;
}