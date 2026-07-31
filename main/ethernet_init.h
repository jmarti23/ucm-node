/*
 * UCM Ethernet Initialization
 *
 * ESP32-P4 + IP101 PHY Ethernet support
 * ESP-IDF 5.5.5
 */

#pragma once

#include "esp_eth.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize UCM Ethernet interface
 *
 * @param[out] eth_handles_out Allocated array containing Ethernet handle(s)
 * @param[out] eth_cnt_out Number of initialized Ethernet interfaces
 *
 * @return
 *      ESP_OK on success
 *      ESP_ERR_INVALID_ARG on invalid arguments
 *      ESP_ERR_NO_MEM on allocation failure
 *      ESP_FAIL on initialization failure
 */
esp_err_t ucm_eth_init(esp_eth_handle_t **eth_handles_out,
                       uint8_t *eth_cnt_out);


/**
 * @brief Deinitialize UCM Ethernet interface
 *
 * @param[in] eth_handles Ethernet handle array
 * @param[in] eth_cnt Number of interfaces
 *
 * @return ESP_OK on success
 */
esp_err_t ucm_eth_deinit(esp_eth_handle_t *eth_handles,
                         uint8_t eth_cnt);


#ifdef __cplusplus
}
#endif
