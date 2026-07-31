#include "network_manager.h"
#include "ethernet_manager.h"
#include "app_wifi_prov.h"

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static const char *TAG = "NETWORK";


esp_err_t network_start(void)
{
    ESP_LOGI(TAG, "Starting network manager");


    ESP_ERROR_CHECK(ethernet_init());


    ESP_LOGI(TAG, "Waiting for Ethernet");


    for (int i = 0; i < 150; i++)
    {
        if (ethernet_link_up())
        {
            ESP_LOGI(TAG, "Ethernet link detected");

            for (int j = 0; j < 100; j++)
            {
                if (ethernet_has_ip())
                {
                    ESP_LOGI(TAG, "Ethernet ready");
                    return ESP_OK;
                }

                vTaskDelay(pdMS_TO_TICKS(100));
            }

            break;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }


    ESP_LOGI(TAG, "Ethernet unavailable");


    return app_wifi_prov_start();
}