#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi_remote.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
// #include "wifi_provisioning/manager.h"
// #include "wifi_provisioning/scheme_ble.h"
// #include "protocomm.h"

// #include "wifi_provisioning/manager.h"  // adding 7-27 afternoon
// #include "wifi_provisioning/scheme_softap.h"


#include "esp_http_server.h" // adding 7-27
#include "web_server.h"
#include "sen54_data.h" // adding 7-27
#include "mqtt_client.h" // adding 7-27
#include "sen5x_i2c.h"
#include "sensirion_i2c_hal.h"
#include "sensirion_i2c_esp32_config.h"
#include "i2cdev.h"
#include "app_wifi_prov.h"


// SEN5x I2C
#define SEN5X_SDA      GPIO_NUM_7
#define SEN5X_SCL      GPIO_NUM_8
#define SEN5X_I2C_PORT I2C_NUM_0
#define SEN5X_ADDRESS  0x69

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define UCM_NODE_ID "UCM-001" // adding 7-27
#define MQTT_BROKER_URI "mqtt://192.168.1.236"

static const char *TAG = "P4_SENSOR_TEST";
static int s_retry_num = 0;
static esp_mqtt_client_handle_t mqtt_client = NULL;

/* static bool wifi_is_provisioned(void)
{
    bool provisioned = false;

    ESP_ERROR_CHECK(
        wifi_prov_mgr_is_provisioned(&provisioned)
    );

    return provisioned;
}
static void start_ble_provisioning(void)
{
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
    };

    ESP_ERROR_CHECK(
        wifi_prov_mgr_init(config)
    );

    ESP_LOGI(TAG, "Starting BLE provisioning");

    ESP_ERROR_CHECK(
        wifi_prov_mgr_start_provisioning(
            WIFI_PROV_SECURITY_0,
            NULL,
            "UCM-SENSOR",
            NULL
        )
    );

    while (!wifi_is_provisioned()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "Provisioning complete");
} */




static void mqtt_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    ESP_ERROR_CHECK(
        esp_mqtt_client_start(mqtt_client)
    );

    ESP_LOGI(TAG, "MQTT started");
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

        // SEN5x requires external I2C pull-ups.
        .sda_pullup = false,
        .scl_pullup = false,
    };

    ESP_ERROR_CHECK(
        sensirion_i2c_config_esp32(&i2c_cfg));

    sensirion_i2c_hal_init();

    ESP_ERROR_CHECK(
        sensirion_i2c_esp32_ok());

    int16_t error;

    error = sen5x_device_reset();

    if (error) {
        ESP_LOGE(TAG,
                 "SEN5x reset failed: %d",
                 error);
        return;
    }

    ESP_LOGI(TAG, "SEN5x reset successful");

    error = sen5x_start_measurement();

    if (error) {
        ESP_LOGE(TAG,
                 "SEN5x start measurement failed: %d",
                 error);
        return;
    }

    ESP_LOGI(TAG,
             "SEN5x measurement started");
}



static void sen5x_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        uint16_t pm1p0;
        uint16_t pm2p5;
        uint16_t pm4p0;
        uint16_t pm10p0;

        int16_t humidity;
        int16_t temperature;
        int16_t voc_index;
        int16_t nox_index_unused;

        int16_t error =
            sen5x_read_measured_values(
                &pm1p0,
                &pm2p5,
                &pm4p0,
                &pm10p0,
                &humidity,
                &temperature,
                &voc_index,
		&nox_index_unused);
             

        if (error) {
            ESP_LOGE(TAG,
                     "SEN5x read failed: %d",
                     error);
            continue;
        }

        ESP_LOGI(
            TAG,
            "PM1.0=%.1f PM2.5=%.1f PM4.0=%.1f PM10=%.1f ug/m3",
            pm1p0 / 10.0f,
            pm2p5 / 10.0f,
            pm4p0 / 10.0f,
            pm10p0 / 10.0f);

        ESP_LOGI(
            TAG,
            "Temperature=%.1f C Humidity=%.1f %%RH VOC=%.1f",
            temperature / 200.0f,
            humidity / 100.0f,
            voc_index / 10.0f);
strcpy(current_sensor_data.node_id, UCM_NODE_ID);
current_sensor_data.pm1 = pm1p0 / 10.0f;
current_sensor_data.pm25 = pm2p5 / 10.0f;
current_sensor_data.pm4 = pm4p0 / 10.0f;
current_sensor_data.pm10 = pm10p0 / 10.0f;

current_sensor_data.temperature = temperature / 200.0f;
current_sensor_data.humidity = humidity / 100.0f;

current_sensor_data.voc = voc_index / 10.0f;

char payload[256];

snprintf(payload,
         sizeof(payload),
         "{"
         "\"node\":\"%s\","
         "\"pm1\":%.1f,"
         "\"pm25\":%.1f,"
         "\"pm4\":%.1f,"
         "\"pm10\":%.1f,"
         "\"temperature\":%.1f,"
         "\"humidity\":%.1f,"
         "\"voc\":%.1f,"
         "}",
         current_sensor_data.node_id,
         current_sensor_data.pm1,
         current_sensor_data.pm25,
         current_sensor_data.pm4,
         current_sensor_data.pm10,
         current_sensor_data.temperature,
         current_sensor_data.humidity,
         current_sensor_data.voc
         );


esp_mqtt_client_publish(
    mqtt_client,
    "ucm/node-UCM-001/environment",
    payload,
    0,
    1,
    0);
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

    ESP_LOGI(TAG,
             "Starting ESP32-P4 Wi-Fi + SEN5x test");

    ESP_LOGI(TAG, "BEFORE SEN5X");

    sen5x_init();

    ESP_LOGI(TAG, "AFTER SEN5X");

//    wifi_init_sta();
esp_err_t wifi_result = app_wifi_prov_start();
if (wifi_result != ESP_OK) {
    ESP_LOGE(TAG, "Wi-Fi failed to connect");
}
    
    mqtt_start();
    
    start_webserver();
    
    xTaskCreate(
        sen5x_task,
        "sen5x_task",
        4096,
        NULL,
        5,
        NULL);
 
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG,
                 "P4 Wi-Fi Link Active...");
    }
}