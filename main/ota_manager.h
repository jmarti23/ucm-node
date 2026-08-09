#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include "esp_err.h"

esp_err_t ota_manager_init(void);
esp_err_t ota_manager_confirm_running_image(void);
esp_err_t ota_manager_update(const char *firmware_url);

#endif // OTA_MANAGER_H