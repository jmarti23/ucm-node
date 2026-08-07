#include <stdio.h>
#include <string.h>

#include "mqtt_manager.h"

#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "app_wifi_prov.h"


#define MQTT_BROKER_URI "mqtt://UCM-HUB.local"

static const char *TAG = "MQTT_MANAGER";

static esp_mqtt_client_handle_t mqtt_client = NULL;

static char s_node_id[32];
static char s_status_topic[64];
static char s_lwt_payload[96];


static void publish_online_status(void)
{
    char payload[96];
    snprintf(payload, sizeof(payload),
             "{\"node\":\"%s\",\"status\":\"online\"}", s_node_id);

    esp_mqtt_client_publish(mqtt_client, s_status_topic, payload,
                             0, /* qos */ 1, /* retain */ 1);
}


static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");

        char topic[64];
        snprintf(topic, sizeof(topic), "ucm/node/+/command");

        ESP_LOGI(TAG, "Subscribe topic='%s' length=%d", topic, strlen(topic));

        esp_mqtt_client_subscribe(mqtt_client, topic, 1);

        ESP_LOGI(TAG, "Subscribed to %s", topic);

        publish_online_status();
        break;


    case MQTT_EVENT_DATA:

        ESP_LOGI(TAG, "MQTT command topic: %.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "MQTT command data: %.*s", event->data_len, event->data);

        if (strncmp(event->data, "{\"command\":\"provision\"}", event->data_len) == 0) {
            ESP_LOGW(TAG, "Provision command received");
            app_wifi_prov_reset_credentials();
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
        }
        break;


    default:
        break;
    }
}


esp_err_t mqtt_manager_start(const char *node_id)
{
    strncpy(s_node_id, node_id, sizeof(s_node_id) - 1);
    s_node_id[sizeof(s_node_id) - 1] = '\0';

    snprintf(s_status_topic, sizeof(s_status_topic), "ucm/node-%s/status", s_node_id);
    snprintf(s_lwt_payload, sizeof(s_lwt_payload),
             "{\"node\":\"%s\",\"status\":\"offline\"}", s_node_id);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .session.last_will.topic = s_status_topic,
        .session.last_will.msg = s_lwt_payload,
        .session.last_will.msg_len = strlen(s_lwt_payload),
        .session.last_will.qos = 1,
        .session.last_will.retain = 1,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

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

    ESP_LOGI(TAG, "MQTT started");

    return ESP_OK;
}


esp_err_t mqtt_manager_publish_node_info(sen54_data_t *data)
{
    if (!mqtt_client) {
        return ESP_FAIL;
    }

    char payload[256];

    snprintf(payload, sizeof(payload),
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
    snprintf(topic, sizeof(topic), "ucm/node-%s/info", data->node_id);

    esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 1);

    ESP_LOGI(TAG, "Published node info: %s", payload);

    return ESP_OK;
}


esp_err_t mqtt_manager_publish_environment(sen54_data_t *data)
{
    if (!mqtt_client) {
        return ESP_FAIL;
    }

    char payload[256];

    snprintf(payload, sizeof(payload),
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
    snprintf(topic, sizeof(topic), "ucm/node-%s/environment", data->node_id);

    esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);

    return ESP_OK;
}


esp_err_t mqtt_manager_publish_heartbeat(sen54_data_t *data)
{
    if (!mqtt_client) {
        return ESP_FAIL;
    }

    uint32_t free_heap = esp_get_free_heap_size();
    int64_t uptime_s = esp_timer_get_time() / 1000000;

    int8_t rssi = 0;
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        rssi = ap_info.rssi;
    }

    char payload[256];

    snprintf(payload, sizeof(payload),
             "{"
             "\"node\":\"%s\","
             "\"timestamp\":\"%s\","
             "\"uptime_s\":%lld,"
             "\"free_heap\":%lu,"
             "\"rssi\":%d"
             "}",
             data->node_id,
             data->timestamp,
             uptime_s,
             free_heap,
             rssi);

    char topic[64];
    snprintf(topic, sizeof(topic), "ucm/node-%s/heartbeat", data->node_id);

    esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);

    ESP_LOGI(TAG, "Heartbeat: %s", payload);

    return ESP_OK;
}
