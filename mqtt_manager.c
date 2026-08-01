#include <stdio.h>
#include <string.h>

#include "mqtt_manager.h"

#include "mqtt_client.h"
#include "esp_log.h"


#define MQTT_BROKER_URI "mqtt://192.168.1.236"


static const char *TAG = "MQTT_MANAGER";


static esp_mqtt_client_handle_t mqtt_client = NULL;


esp_err_t mqtt_manager_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };


    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    if (!mqtt_client) {
        ESP_LOGE(TAG, "MQTT init failed");
        return ESP_FAIL;
    }


    ESP_ERROR_CHECK(
        esp_mqtt_client_start(mqtt_client)
    );


    ESP_LOGI(TAG,"MQTT started");

    return ESP_OK;
}



esp_err_t mqtt_manager_publish_node_info(sensor_data_t *data)
{
    if (!mqtt_client) {
        return ESP_FAIL;
    }


    char payload[256];


    snprintf(payload,
             sizeof(payload),
             "{"
             "\"node\":\"%s\","
             "\"name\":\"%s\","
             "\"description\":\"%s\","
             "\"lat\":%.5f,"
             "\"lon\":%.5f"
             "}",
             data->node_id,
             data->node_name,
             data->description,
             data->latitude,
             data->longitude);



    char topic[64];

    snprintf(topic,
             sizeof(topic),
             "ucm/node-%s/info",
             data->node_id);



    esp_mqtt_client_publish(
        mqtt_client,
        topic,
        payload,
        0,
        1,
        1
    );


    ESP_LOGI(TAG,
             "Published node info: %s",
             payload);


    return ESP_OK;
}



esp_err_t mqtt_manager_publish_environment(sensor_data_t *data)
{
    if (!mqtt_client) {
        return ESP_FAIL;
    }


    char payload[256];


    snprintf(payload,
             sizeof(payload),
             "{"
             "\"node\":\"%s\","
             "\"timestamp\":\"%s\","
             "\"pm1\":%.1f,"
             "\"pm25\":%.1f,"
             "\"pm4\":%.1f,"
             "\"pm10\":%.1f,"
             "\"temperature\":%.1f,"
             "\"humidity\":%.1f,"
             "\"voc\":%.1f"
             "}",
             data->node_id,
             data->timestamp,
             data->pm1,
             data->pm25,
             data->pm4,
             data->pm10,
             data->temperature,
             data->humidity,
             data->voc);



    char topic[64];

    snprintf(topic,
             sizeof(topic),
             "ucm/node-%s/environment",
             data->node_id);



    esp_mqtt_client_publish(
        mqtt_client,
        topic,
        payload,
        0,
        1,
        0
    );


    return ESP_OK;
}
