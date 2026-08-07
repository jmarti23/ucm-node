#include <string.h>
#include <stdbool.h>
#include <time.h>

#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif_sntp.h"

#include "network_manager.h"
#include "app_wifi_prov.h"
#include "esp_http_server.h"

#include "sen54_data.h"
#include "sen5x_i2c.h"
#include "sensirion_i2c_hal.h"
#include "sensirion_i2c_esp32_config.h"

#include "i2cdev.h"

#include "web_server.h"
#include "mqtt_manager.h"



// SEN5x I2C
#define SEN5X_SDA      GPIO_NUM_7
#define SEN5X_SCL      GPIO_NUM_8
#define SEN5X_I2C_PORT I2C_NUM_0
#define SEN5X_ADDRESS  0x69

// #define MQTT_BROKER_URI "mqtt://192.168.1.236"

#define ETHERNET_TIMEOUT_MS 30000

static const char *TAG = "P4_SENSOR_TEST";


// Derives a stable, unique node ID from the board's MAC address, e.g.
// "UCM-A1B2C3" -- guaranteed different per physical device, no manual
// config needed, and stable across reboots since MAC doesn't change.

static void generate_node_id(char *out, size_t out_size)
{
    uint8_t mac[6];

    ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));

    snprintf(out,
             out_size,
             "UCM-%02X%02X%02X",
             mac[3],
             mac[4],
             mac[5]);
}


static void time_sync_start(void)
{
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&config);

    ESP_LOGI(TAG, "Waiting for time sync...");
    esp_err_t err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Time sync failed/timed out -- timestamps will be inaccurate");
    } else {
        ESP_LOGI(TAG, "Time synced");
    }
}

static void format_timestamp(char *out, size_t out_size)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    gmtime_r(&now, &timeinfo);
    strftime(out, out_size, "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
}

static void sen5x_init(void)
{
    ESP_ERROR_CHECK(i2cdev_init());
    struct esp32_i2c_config i2c_cfg = {
        .freq = 100000,
        .addr = SEN5X_ADDRESS,
        .port = SEN5X_I2C_PORT,
        .sda = SEN5X_SDA,
        .scl = SEN5X_SCL,
        .sda_pullup = false,
        .scl_pullup = false,
    };

    ESP_ERROR_CHECK(sensirion_i2c_config_esp32(&i2c_cfg));
    sensirion_i2c_hal_init();
    ESP_ERROR_CHECK(sensirion_i2c_esp32_ok());

    int16_t error = sen5x_device_reset();
    if (error) {
        ESP_LOGE(TAG, "SEN5x reset failed: %d", error);
        return;
    }
    ESP_LOGI(TAG, "SEN5x reset successful");

    error = sen5x_start_measurement();
    if (error) {
        ESP_LOGE(TAG, "SEN5x start measurement failed: %d", error);
        return;
    }
    ESP_LOGI(TAG, "SEN5x measurement started");
}

static void sen5x_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        uint16_t pm1p0, pm2p5, pm4p0, pm10p0;
        int16_t humidity, temperature, voc_index, nox_index_unused;

        int16_t error = sen5x_read_measured_values(
            &pm1p0, &pm2p5, &pm4p0, &pm10p0,
            &humidity, &temperature, &voc_index, &nox_index_unused);

        if (error) {
            ESP_LOGE(TAG, "SEN5x read failed: %d", error);
            continue;
        }

        ESP_LOGI(TAG, "PM1.0=%.1f PM2.5=%.1f PM4.0=%.1f PM10=%.1f ug/m3",
                 pm1p0 / 10.0f,
                 pm2p5 / 10.0f,
                 pm4p0 / 10.0f,
                 pm10p0 / 10.0f);

        ESP_LOGI(TAG, "Temperature=%.1f C Humidity=%.1f %%RH VOC=%.1f",
                 temperature / 200.0f,
                 humidity / 100.0f,
                 voc_index / 10.0f);


        current_sensor_data.pm1 = pm1p0 / 10.0f;
        current_sensor_data.pm25 = pm2p5 / 10.0f;
        current_sensor_data.pm4 = pm4p0 / 10.0f;
        current_sensor_data.pm10 = pm10p0 / 10.0f;
        current_sensor_data.temperature = temperature / 200.0f;
        current_sensor_data.humidity = humidity / 100.0f;
        current_sensor_data.voc = voc_index / 10.0f;

        format_timestamp(
            current_sensor_data.timestamp,
            sizeof(current_sensor_data.timestamp)
        );


        mqtt_manager_publish_environment(
            &current_sensor_data
        );
        mqtt_manager_publish_heartbeat(
            &current_sensor_data
        );
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ENTERED app_main");

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || 
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Starting UCM ESP32-P4 environmental node");

    ESP_LOGI(TAG, "BEFORE SEN5X");
    sen5x_init();
    ESP_LOGI(TAG, "AFTER SEN5X");


    esp_err_t network_result = network_start();

    if (network_result != ESP_OK) {

        ESP_LOGE(TAG, "Network initialization failed");

    } else {

        time_sync_start();

        char node_name[32] = {0};
        char node_desc[64] = {0};
        float lat = 0.0f;
        float lon = 0.0f;

        app_wifi_prov_get_node_info(
            node_name,
            sizeof(node_name),
            node_desc,
            sizeof(node_desc),
            &lat,
            &lon
        );

        strncpy(current_sensor_data.node_name,
                node_name,
                sizeof(current_sensor_data.node_name) - 1);

        strncpy(current_sensor_data.description,
                node_desc,
                sizeof(current_sensor_data.description) - 1);

        current_sensor_data.latitude = lat;
        current_sensor_data.longitude = lon;



        generate_node_id(
            current_sensor_data.node_id,
            sizeof(current_sensor_data.node_id)
        );

        ESP_LOGI(TAG, "Node ID: %s",
                 current_sensor_data.node_id);

if (mqtt_manager_start(current_sensor_data.node_id) == ESP_OK) {

    mqtt_manager_publish_node_info(
        &current_sensor_data
    );

} else {

    ESP_LOGE(TAG, "MQTT startup failed");
}
    }


    start_webserver();

    xTaskCreate(
        sen5x_task,
        "sen5x_task",
        4096,
        NULL,
        5,
        NULL
    );


    while (1) {

        vTaskDelay(pdMS_TO_TICKS(10000));

        ESP_LOGI(TAG, "Network Active...");
    }
}