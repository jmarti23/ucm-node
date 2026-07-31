#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ethernet_init(void);

bool ethernet_link_up(void);

bool ethernet_has_ip(void);

#ifdef __cplusplus
}
#endif