#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bring Wi-Fi up, provisioning it via phone (SoftAP) if no
 *        credentials are stored yet, or connecting directly with
 *        saved credentials otherwise.
 *
 * Blocks until either:
 *   - already-provisioned STA successfully connects and gets an IP, or
 *   - a fresh provisioning session completes and the resulting STA
 *     connects and gets an IP.
 *
 * Call this once from app_main(), after nvs_flash_init(), in place of
 * your old hard-coded wifi_init_sta().
 */
esp_err_t app_wifi_prov_start(void);

/**
 * @brief Force-clear stored Wi-Fi credentials so the next boot starts
 *        provisioning again. Useful for a "reset wifi" button/command.
 */
esp_err_t app_wifi_prov_reset_credentials(void);

#ifdef __cplusplus
}
#endif
