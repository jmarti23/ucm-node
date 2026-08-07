#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

bool wifi_has_credentials(void);

esp_err_t wifi_connect(void);

esp_err_t app_wifi_prov_start(void);

esp_err_t app_wifi_prov_reset_credentials(void);

void app_wifi_prov_get_node_info(
    char *name,
    size_t name_size,
    char *desc,
    size_t desc_size,
    float *lat,
    float *lon
);

#ifdef __cplusplus
}
#endif

