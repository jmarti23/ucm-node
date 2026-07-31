#include <math.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "nvs_flash.h"

#include "i2cdev.h"
#include "sen5x_i2c.h"
#include "sensirion_i2c_hal.h"
#include "sensirion_i2c_hal.h"
#include "sensirion_i2c_esp32_config.h"

/* -----------------------------
 * WIFI
 * ----------------------------- */

#define WIFI_SSID      "Verizon_GV3VCY-IoT"
#define WIFI_PASS      "ankle6cat3kitty"
#define MAXIMUM_RETRY  5

/* -----------------------------
 * SEN5x I2C
 * ----------------------------- */

#define SEN5X_SDA      GPIO_NUM_7
#define SEN5X_SCL      GPIO_NUM_8
#define SEN5X_I2C_PORT I2C_NUM_0
#define SEN5X_ADDRESS  0x69

/* -----------------------------
 * UCM NODE
 * ----------------------------- */

#define NODE_ID        "01"
#define DEVICE_NAME    "SEN54"
#define LOCATION       "UCM_TEST"

/* -----------------------------
 * MQTT
 * ----------------------------- */

#define MQTT_SERVER    "192.168.1.236"
#define MQTT_PORT      1883

#define MQTT_ENV_TOPIC    "ucm/01/environment"
#define MQTT_STATUS_TOPIC "ucm/node-01/status"

/* -----------------------------
 * TIMING
 * ----------------------------- */

#define SENSOR_INTERVAL_MS 10000

/* -----------------------------
 * STATE
 * ----------------------------- */

static const char *TAG = "UCM_SENSOR_NODE";

static EventGroupHandle_t s_wifi_event_group;
static EventGroupHandle_t s_mqtt_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MQTT_CONNECTED_BIT BIT0

static int s_retry_num = 0;
static esp_mqtt_client_handle_t s_mqtt_client = NULL;

/* -----------------------------
 * WIFI
 * ----------------------------- */

static void wifi_event_handler(void *arg,
                              esp_event_base_t event_base,
                              int32_t event_id,
                              void *event_data)
{
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_START) {

        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {

        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;

            ESP_LOGI(TAG, "Retrying Wi-Fi connection...");
        } else {
            xEventGroupSetBits(
                s_wifi_event_group,
                WIFI_FAIL_BIT
            );
        }

    } else if (event_base == IP_EVENT &&
               event_id == IP_EVENT_STA_GOT_IP) {

        ip_event_got_ip_t *event =
            (ip_event_got_ip_t *)event_data;

        ESP_LOGI(
            TAG,
            "Got IP address:" IPSTR,
            IP2STR(&event->ip_info.ip)
        );

        s_retry_num = 0;

        xEventGroupSetBits(
            s_wifi_event_group,
            WIFI_CONNECTED_BIT
        );
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &wifi_event_handler,
            NULL,
            NULL
        )
    );

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(WIFI_MODE_STA)
    );

    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_STA,
            &wifi_config
        )
    );

    ESP_ERROR_CHECK(
        esp_wifi_start()
    );

    ESP_LOGI(
        TAG,
        "Connecting to Wi-Fi SSID: %s",
        WIFI_SSID
    );

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY
    );

    if (bits & WIFI_CONNECTED_BIT) {

        ESP_LOGI(
            TAG,
            "Wi-Fi connected"
        );

    } else if (bits & WIFI_FAIL_BIT) {

        ESP_LOGE(
            TAG,
            "Wi-Fi connection failed"
        );

    } else {

        ESP_LOGE(
            TAG,
            "Unexpected Wi-Fi event"
        );
    }
}

/* -----------------------------
 * SEN5x
 * ----------------------------- */

static void sen5x_init(void)
{
    ESP_ERROR_CHECK(i2cdev_init());

    struct esp32_i2c_config i2c_cfg = {
        .freq = 100000,
        .addr = SEN5X_ADDRESS,
        .port = SEN5X_I2C_PORT,
        .sda = SEN5X_SDA,
        .scl = SEN5X_SCL,
        .sda_pullup = GPIO_PULLUP_DISABLE,
        .scl_pullup = GPIO_PULLUP_DISABLE,
    };

    ESP_ERROR_CHECK(
        sensirion_i2c_config_esp32(&i2c_cfg)
    );

    sensirion_i2c_hal_init();

    int16_t error = sen5x_device_reset();

    if (error) {
        ESP_LOGE(
            TAG,
            "SEN5x reset failed: %d",
            error
        );

        return;
    }

    ESP_LOGI(
        TAG,
        "SEN5x reset successful"
    );

    error = sen5x_start_measurement();

    if (error) {
        ESP_LOGE(
            TAG,
            "SEN5x start measurement failed: %d",
            error
        );

        return;
    }

    ESP_LOGI(
        TAG,
        "SEN5x measurement started"
    );
}

/* -----------------------------
 * MQTT
 * ----------------------------- */

static void publish_status(void)
{
    char payload[256];

    snprintf(
        payload,
        sizeof(payload),
        "{"
        "\"node\":\"%s\","
        "\"device\":\"%s\","
        "\"location\":\"%s\","
        "\"status\":\"online\""
        "}",
        NODE_ID,
        DEVICE_NAME,
        LOCATION
    );

    int message_id = esp_mqtt_client_publish(
        s_mqtt_client,
        MQTT_STATUS_TOPIC,
        payload,
        0,
        0,
        0
    );

    ESP_LOGI(
        TAG,
        "Status published. MQTT message ID: %d",
        message_id
    );

    ESP_LOGI(
        TAG,
        "Status payload: %s",
        payload
    );
}

static void mqtt_event_handler(
    void *handler_args,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    esp_mqtt_event_handle_t event =
        event_data;

    switch ((esp_mqtt_event_id_t)event_id) {

        case MQTT_EVENT_CONNECTED:

            ESP_LOGI(
                TAG,
                "MQTT connected to UCM_Hub"
            );

            xEventGroupSetBits(
                s_mqtt_event_group,
                MQTT_CONNECTED_BIT
            );

            publish_status();

            break;

        case MQTT_EVENT_DISCONNECTED:

            ESP_LOGW(
                TAG,
                "MQTT disconnected"
            );

            xEventGroupClearBits(
                s_mqtt_event_group,
                MQTT_CONNECTED_BIT
            );

            break;

        case MQTT_EVENT_ERROR:

            ESP_LOGE(
                TAG,
                "MQTT error"
            );

            break;

        default:
            break;
    }
}

static void mqtt_init(void)
{
    s_mqtt_event_group = xEventGroupCreate();

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri =
            "mqtt://192.168.1.236:1883",

        .credentials.client_id =
            NODE_ID,
    };

    s_mqtt_client =
        esp_mqtt_client_init(&mqtt_cfg);

    ESP_ERROR_CHECK(
        esp_mqtt_client_register_event(
            s_mqtt_client,
            ESP_EVENT_ANY_ID,
            mqtt_event_handler,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        esp_mqtt_client_start(s_mqtt_client)
    );
}

/* -----------------------------
 * SENSOR PUBLISH
 * ----------------------------- */

static void publish_sensor_data(void)
{
    float pm1p0;
    float pm2p5;
    float pm4p0;
    float pm10;

    float humidity;
    float temperature;
    float voc_index;
    float nox_index;

    int16_t error =
        sen5x_read_measured_values(
            &pm1p0,
            &pm2p5,
            &pm4p0,
            &pm10,
            &humidity,
            &temperature,
            &voc_index,
            &nox_index
        );

    if (error) {

        ESP_LOGE(
            TAG,
            "SEN5x read failed: %d",
            error
        );

        return;
    }

    char payload[512];

    snprintf(
        payload,
        sizeof(payload),
        "{"
        "\"node\":\"%s\","
        "\"device\":\"%s\","
        "\"location\":\"%s\","
        "\"pm1_0\":%.1f,"
        "\"pm2_5\":%.1f,"
        "\"pm4_0\":%.1f,"
        "\"pm10\":%.1f,"
        "\"temperature\":%.1f,"
        "\"humidity\":%.1f,"
        "\"voc\":%.1f"
        "}",
        NODE_ID,
        DEVICE_NAME,
        LOCATION,
        pm1p0,
        pm2p5,
        pm4p0,
        pm10,
        temperature,
        humidity,
        voc_index
    );

    int message_id = esp_mqtt_client_publish(
        s_mqtt_client,
        MQTT_ENV_TOPIC,
        payload,
        0,
        0,
        0
    );

    ESP_LOGI(
        TAG,
        "Environment published. MQTT message ID: %d",
        message_id
    );

    ESP_LOGI(
        TAG,
        "%s",
        payload
    );
}

/* -----------------------------
 * SENSOR TASK
 * ----------------------------- */

static void sensor_task(void *arg)
{
    xEventGroupWaitBits(
        s_mqtt_event_group,
        MQTT_CONNECTED_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY
    );

    vTaskDelay(pdMS_TO_TICKS(1000));

    while (1) {

        EventBits_t mqtt_bits =
            xEventGroupGetBits(
                s_mqtt_event_group
            );

        if (mqtt_bits & MQTT_CONNECTED_BIT) {
            publish_sensor_data();
        } else {
            ESP_LOGW(
                TAG,
                "MQTT not connected; sensor data not published"
            );
        }

        vTaskDelay(
            pdMS_TO_TICKS(SENSOR_INTERVAL_MS)
        );
    }
}

/* -----------------------------
 * APPLICATION
 * ----------------------------- */

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(
            nvs_flash_erase()
        );

        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    ESP_LOGI(
        TAG,
        "Starting UCM Sensor Node"
    );

    wifi_init_sta();

    sen5x_init();

    mqtt_init();

    xTaskCreate(
        sensor_task,
        "sensor_task",
        8192,
        NULL,
        5,
        NULL
    );
}