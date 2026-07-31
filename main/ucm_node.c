#include <stdio.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "nvs_flash.h"

#include "mqtt_client.h"
#include "cJSON.h"

#include "driver/gpio.h"

#include "i2cdev.h"
#include "sen5x_i2c.h"
#include "sensirion_i2c_hal.h"
#include "sensirion_i2c_esp32_config.h"

#include "ucm_config.h"


// ============================================================
// TAGS
// ============================================================

static const char *TAG = "UCM_NODE";


// ============================================================
// WIFI
// ============================================================

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;


// ============================================================
// MQTT
// ============================================================

static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

static char mqtt_data_topic[64];


// ============================================================
// TIMING
// ============================================================

static unsigned long last_sensor_publish = 0;
static unsigned long last_status_publish = 0;


// ============================================================
// WIFI EVENT HANDLER
// ============================================================

static void wifi_event_handler(
    void *arg,
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
            "Got IP address: " IPSTR,
            IP2STR(&event->ip_info.ip)
        );

        s_retry_num = 0;

        xEventGroupSetBits(
            s_wifi_event_group,
            WIFI_CONNECTED_BIT
        );
    }
}


// ============================================================
// WIFI INITIALIZATION
// ============================================================

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(
        esp_event_loop_create_default()
    );

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg =
        WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(
        esp_wifi_init(&cfg)
    );

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL,
            &instance_any_id
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &wifi_event_handler,
            NULL,
            &instance_got_ip
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

    EventBits_t bits =
        xEventGroupWaitBits(
            s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY
        );

    if (bits & WIFI_CONNECTED_BIT) {

        ESP_LOGI(TAG, "Wi-Fi connected");

    } else {

        ESP_LOGE(TAG, "Wi-Fi connection failed");
    }
}


// ============================================================
// SEN54 INITIALIZATION
// ============================================================

static void sen5x_init(void)
{
    struct esp32_i2c_config i2c_cfg = {

        .freq = 100000,
        .addr = SEN5X_ADDRESS,
        .port = SEN5X_I2C_PORT,
        .sda = SEN5X_SDA,
        .scl = SEN5X_SCL,

        // SEN54 requires external I2C pull-ups.
        .sda_pullup = false,
        .scl_pullup = false,
    };

    ESP_ERROR_CHECK(
        sensirion_i2c_config_esp32(&i2c_cfg)
    );

    sensirion_i2c_hal_init();

    int16_t error;

    error = sen5x_device_reset();

    if (error != 0) {

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

    if (error != 0) {

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


// ============================================================
// MQTT EVENT HANDLER
// ============================================================

static void mqtt_event_handler(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data)
{
    esp_mqtt_event_handle_t event =
        event_data;

    switch ((esp_mqtt_event_id_t)event_id) {

        case MQTT_EVENT_CONNECTED:

            mqtt_connected = true;

            ESP_LOGI(
                TAG,
                "MQTT connected"
            );

            break;


        case MQTT_EVENT_DISCONNECTED:

            mqtt_connected = false;

            ESP_LOGW(
                TAG,
                "MQTT disconnected"
            );

            break;


        default:

            break;
    }
}


// ============================================================
// MQTT INITIALIZATION
// ============================================================

static void mqtt_init(void)
{
    snprintf(
        mqtt_data_topic,
        sizeof(mqtt_data_topic),
        "sensors/nyc/%s",
        NODE_ID
    );

    esp_mqtt_client_config_t mqtt_cfg = {

        .broker.address.uri = "mqtt://192.168.1.236:1883",

    };

    mqtt_client =
        esp_mqtt_client_init(&mqtt_cfg);

    ESP_ERROR_CHECK(
        esp_mqtt_client_register_event(
            mqtt_client,
            ESP_EVENT_ANY_ID,
            mqtt_event_handler,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        esp_mqtt_client_start(mqtt_client)
    );

    ESP_LOGI(
        TAG,
        "MQTT topic: %s",
        mqtt_data_topic
    );
}


// ============================================================
// TIMESTAMP
// ============================================================

static void get_timestamp(
    char *buffer,
    size_t buffer_size)
{
    time_t now;

    time(&now);

    struct tm timeinfo;

    gmtime_r(
        &now,
        &timeinfo
    );

    strftime(
        buffer,
        buffer_size,
        "%Y-%m-%dT%H:%M:%SZ",
        &timeinfo
    );
}


// ============================================================
// STATUS MESSAGE
// ============================================================

static void publish_status(void)
{
    if (!mqtt_connected) {

        return;
    }

    char topic[64];

    snprintf(
        topic,
        sizeof(topic),
        "sensors/nyc/%s/status",
        NODE_ID
    );

    cJSON *root =
        cJSON_CreateObject();

    cJSON_AddStringToObject(
        root,
        "node",
        NODE_ID
    );

    cJSON_AddStringToObject(
        root,
        "status",
        "online"
    );

    cJSON_AddNumberToObject(
        root,
        "lat",
        NODE_LAT
    );

    cJSON_AddNumberToObject(
        root,
        "lon",
        NODE_LON
    );

    char *json =
        cJSON_PrintUnformatted(root);

    esp_mqtt_client_publish(
        mqtt_client,
        topic,
        json,
        0,
        1,
        0
    );

    ESP_LOGI(
        TAG,
        "Status published: %s",
        json
    );

    free(json);

    cJSON_Delete(root);
}


// ============================================================
// SENSOR MESSAGE
// ============================================================

static void publish_sensor_data(void)
{
    if (!mqtt_connected) {

        ESP_LOGW(
            TAG,
            "MQTT not connected; sensor data not published"
        );

        return;
    }

    float pm1_0;
    float pm2_5;
    float pm4_0;
    float pm10_0;

    float ambient_temperature;
    float ambient_humidity;

    float voc_index;
    float nox_index;

    uint16_t error;

    error =
        sen5x_read_measured_values(
            &pm1_0,
            &pm2_5,
            &pm4_0,
            &pm10_0,
            &ambient_temperature,
            &ambient_humidity,
            &voc_index,
            &nox_index
        );

    if (error != 0) {

        ESP_LOGE(
            TAG,
            "SEN5x read failed: %d",
            error
        );

        return;
    }

    char timestamp[32];

    get_timestamp(
        timestamp,
        sizeof(timestamp)
    );

    cJSON *root =
        cJSON_CreateObject();

    cJSON_AddStringToObject(
        root,
        "node",
        NODE_ID
    );

    cJSON_AddStringToObject(
        root,
        "timestamp",
        timestamp
    );

    cJSON_AddNumberToObject(
        root,
        "lat",
        NODE_LAT
    );

    cJSON_AddNumberToObject(
        root,
        "lon",
        NODE_LON
    );

    cJSON_AddNumberToObject(
        root,
        "mass_concentration_pm1_0",
        pm1_0
    );

    cJSON_AddNumberToObject(
        root,
        "mass_concentration_pm2_5",
        pm2_5
    );

    cJSON_AddNumberToObject(
        root,
        "mass_concentration_pm4_0",
        pm4_0
    );

    cJSON_AddNumberToObject(
        root,
        "mass_concentration_pm10_0",
        pm10_0
    );

    cJSON_AddNumberToObject(
        root,
        "ambient_temperature",
        ambient_temperature
    );

    cJSON_AddNumberToObject(
        root,
        "ambient_humidity",
        ambient_humidity
    );

    cJSON_AddNumberToObject(
        root,
        "voc_index",
        voc_index
    );

    cJSON_AddNumberToObject(
        root,
        "nox_index",
        nox_index
    );

    char *json =
        cJSON_PrintUnformatted(root);

    esp_mqtt_client_publish(
        mqtt_client,
        mqtt_data_topic,
        json,
        0,
        1,
        0
    );

    ESP_LOGI(
        TAG,
        "Sensor data published: %s",
        json
    );

    free(json);

    cJSON_Delete(root);
}


// ============================================================
// MAIN
// ============================================================

void app_main(void)
{
    esp_err_t ret =
        nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(
            nvs_flash_erase()
        );

        ret =
            nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    ESP_LOGI(
        TAG,
        "Starting UCM node: %s",
        NODE_ID
    );

    ESP_ERROR_CHECK(
        i2cdev_init()
    );

    wifi_init_sta();

    sen5x_init();

    mqtt_init();

    last_sensor_publish = 0;
    last_status_publish = 0;

    while (1) {

        unsigned long now =
            xTaskGetTickCount() *
            portTICK_PERIOD_MS;

        if (now - last_sensor_publish >= 10000) {

            publish_sensor_data();

            last_sensor_publish = now;
        }

        if (now - last_status_publish >= 60000) {

            publish_status();

            last_status_publish = now;
        }

        vTaskDelay(
            pdMS_TO_TICKS(100)
        );
    }
}
'@ | Set-Content .\main\ucm_node.c