#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bring Wi-Fi up.
 *
 * If credentials are already saved in NVS (custom namespace "wifi_cfg"),
 * connects directly in STA mode and blocks until connected.
 *
 * If no credentials are saved, starts a SoftAP ("UCM-SENSOR-SETUP", open,
 * no password) and a plain HTTP server. Connect your phone's Wi-Fi to that
 * network, then open http://192.168.4.1 in any browser (Safari, Chrome,
 * etc. -- no app needed). Fill in your home Wi-Fi SSID/password and submit.
 * The board saves them to NVS and reboots to join your real network.
 *
 * This function does NOT return in the fresh-provisioning case -- the
 * board reboots itself once credentials are submitted via the form.
 * It only returns (ESP_OK) in the already-provisioned, connected case.
 */
esp_err_t app_wifi_prov_start(void);

/**
 * @brief Clear saved Wi-Fi credentials from NVS. Call this once (e.g. from
 *        a button press, a serial CLI command, or a temporary line in
 *        app_main during testing) to force the SoftAP setup page again
 *        on the next boot. Does NOT reboot for you -- caller decides when.
 */
esp_err_t app_wifi_prov_reset_credentials(void);

#ifdef __cplusplus
}
#endif
