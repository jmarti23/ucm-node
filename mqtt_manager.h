#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "esp_err.h"
#include "sen54_data.h"

esp_err_t mqtt_manager_start(void);

esp_err_t mqtt_manager_publish_node_info(sensor_data_t *data);

esp_err_t mqtt_manager_publish_environment(sensor_data_t *data);

#endif