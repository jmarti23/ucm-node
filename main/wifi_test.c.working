#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "sen5x_i2c.h"
#include "sensirion_i2c_hal.h"
#include "sensirion_i2c_esp32_config.h"
#include "i2cdev.h"

// Wi-Fi
#define WIFI_SSID      "Verizon_GV3VCY-IoT"
#define WIFI_PASS      "ankle6cat3kitty"
#define MAXIMUM_RETRY  5

// SEN5x I2C
#define SEN5X_SDA      GPIO_NUM_7
#define SEN5X_SCL      GPIO_NUM_8
#define SEN5X_I2C_PORT I2C_NUM_0
#define SEN5X_ADDRESS  0x69

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "P4_SENSOR_TEST";
static int s_retry_num = 0;

static void event_handler(void* arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void* event_data)
{
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_START) {

        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {

        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retrying connection to AP...");
        } else {
            xEventGroupSetBits(s_wifi_event_group,
                               WIFI_FAIL_BIT);
        }

        ESP_LOGE(TAG, "Failed to connect to AP");

    } else if (event_base == IP_EVENT &&
               event_id == IP_EVENT_STA_GOT_IP) {

        ip_event_got_ip_t* event =
            (ip_event_got_ip_t*) event_data;

        ESP_LOGI(TAG,
                 "Got IP address:" IPSTR,
                 IP2STR(&event->ip_info.ip));

        s_retry_num = 0;

        xEventGroupSetBits(s_wifi_event_group,
                           WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg =
        WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &event_handler,
            NULL,
            &instance_any_id));

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &event_handler,
            NULL,
            &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_ERROR_CHECK(
        esp_wifi_set_config(WIFI_IF_STA,
                            &wifi_config));

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG,
             "wifi_init_sta finished. Connecting to SSID: %s...",
             WIFI_SSID);

    EventBits_t bits =
        xEventGroupWaitBits(
            s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG,
                 "Successfully connected to AP SSID: %s",
                 WIFI_SSID);

    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG,
                 "Failed to connect to SSID: %s",
                 WIFI_SSID);
    }
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
        int16_t nox_index;

        int16_t error =
            sen5x_read_measured_values(
                &pm1p0,
                &pm2p5,
                &pm4p0,
                &pm10p0,
                &humidity,
                &temperature,
                &voc_index,
                &nox_index);

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
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(nvs_flash_erase());

        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG,
             "Starting ESP32-P4 Wi-Fi + SEN5x test");

    sen5x_init();

    wifi_init_sta();

    xTaskCreate(
        sen5x_task,
        "sen5x_task",
        4096,
        NULL,
        5,
        NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));

        ESP_LOGI(TAG,
                 "P4 Wi-Fi Link Active...");
    }
}