#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float pm1_0;
    float pm2_5;
    float pm4_0;
    float pm10;
    float temperature_c;
    float humidity_pct;
    float voc_index;
} sensor_reading_t;

/**
 * @brief Start the HTTP server. Call once, after Wi-Fi is connected.
 *        Serves a live dashboard at http://<board-ip>/ and JSON data
 *        at http://<board-ip>/data
 */
esp_err_t app_http_server_start(void);

/**
 * @brief Call this from your existing sensor-read loop each time you
 *        have a fresh SEN5x reading, in place of / alongside your
 *        current ESP_LOGI print of the values.
 */
void app_http_server_update_reading(const sensor_reading_t *reading);

#ifdef __cplusplus
}
#endif
