#include "ethernet_manager.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_wifi_prov.h"
#include "ethernet_init.h"
#include "esp_eth_netif_glue.h"

static const char *TAG = "ETHERNET";

static bool eth_link_up = false;
static bool eth_has_ip = false;
static esp_eth_handle_t *eth_handles = NULL;
static uint8_t eth_cnt = 0;




static void ethernet_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    if (event_base == ETH_EVENT)
    {
        if (event_id == ETHERNET_EVENT_CONNECTED)
        {
            eth_link_up = true;
            ESP_LOGI(TAG, "Ethernet link up");
        }

        if (event_id == ETHERNET_EVENT_DISCONNECTED)
        {
            eth_link_up = false;
            eth_has_ip = false;
            ESP_LOGI(TAG, "Ethernet link down");
        }
    }

    if (event_base == IP_EVENT &&
        event_id == IP_EVENT_ETH_GOT_IP)
    {
        eth_has_ip = true;
        ESP_LOGI(TAG, "Ethernet got IP");
    }
}


bool ethernet_link_up(void)
{
    return eth_link_up;
}


bool ethernet_has_ip(void)
{
    return eth_has_ip;
}

esp_err_t ethernet_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing Ethernet");

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(
        esp_event_loop_create_default()
    );

    ret = ucm_eth_init(&eth_handles, &eth_cnt);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ucm_eth_init failed");
        return ret;
    }

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();

    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);

    if (eth_netif == NULL) {
        ESP_LOGE(TAG, "Failed creating Ethernet netif");
        return ESP_FAIL;
    }

    esp_eth_netif_glue_handle_t eth_glue =
        esp_eth_new_netif_glue(eth_handles[0]);

    if (eth_glue == NULL) {
        ESP_LOGE(TAG, "Failed creating Ethernet glue");
        return ESP_FAIL;
    }

    ret = esp_netif_attach(eth_netif, eth_glue);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed attaching Ethernet netif");
        return ret;
    }

    ret = esp_event_handler_register(
        ETH_EVENT,
        ESP_EVENT_ANY_ID,
        &ethernet_event_handler,
        NULL);

    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_ETH_GOT_IP,
        &ethernet_event_handler,
        NULL);

    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_eth_start(eth_handles[0]);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed starting Ethernet");
        return ret;
    }

    ESP_LOGI(TAG, "Ethernet started");

    return ESP_OK;
}