/**
 * @file ota_manager.c
 * @brief OTA boot-state and firmware validation management for the UCM node.
 *
 * This module is the foundation of the UCM OTA system.
 *
 * Current responsibilities:
 *   - Identify the currently running OTA partition.
 *   - Report the running firmware image and OTA state.
 *   - Detect whether the current image is awaiting first-boot validation.
 *   - Confirm a successfully validated firmware image.
 *
 * OTA download/install is intentionally NOT implemented here yet.
 *
 * IMPORTANT:
 * A newly installed firmware image may be in
 * ESP_OTA_IMG_PENDING_VERIFY state. We deliberately do not mark it valid
 * in ota_manager_init(). The application must first perform its
 * startup/self-tests and then explicitly confirm the image. This
 * preserves the rollback mechanism.
 */

#include "ota_manager.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

static const char *TAG = "UCM_OTA";


/**
 * @brief Initialize the OTA manager and report the current boot state.
 *
 * This function does NOT confirm a pending firmware image.
 * Confirmation is performed separately after application health checks
 * have completed.
 */
esp_err_t ota_manager_init(void)
{
    const esp_partition_t *running_partition =
        esp_ota_get_running_partition();

    if (running_partition == NULL) {
        ESP_LOGE(TAG, "Unable to determine running OTA partition");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "Running partition: %s "
             "(type=0x%02x subtype=0x%02x offset=0x%08lx size=0x%08lx)",
             running_partition->label,
             running_partition->type,
             running_partition->subtype,
             (unsigned long)running_partition->address,
             (unsigned long)running_partition->size);

    esp_ota_img_states_t ota_state;

    esp_err_t err = esp_ota_get_state_partition(
        running_partition,
        &ota_state
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to get OTA state: %s",
                 esp_err_to_name(err));
        return err;
    }

    switch (ota_state) {

        case ESP_OTA_IMG_NEW:
            ESP_LOGI(TAG, "OTA image state: NEW");
            break;

        case ESP_OTA_IMG_PENDING_VERIFY:
            ESP_LOGW(TAG,
                     "OTA image is PENDING_VERIFY");
            ESP_LOGW(TAG,
                     "Firmware must pass UCM startup validation "
                     "before being marked valid");
            break;

        case ESP_OTA_IMG_VALID:
            ESP_LOGI(TAG, "OTA image state: VALID");
            break;

        case ESP_OTA_IMG_INVALID:
            ESP_LOGW(TAG, "OTA image state: INVALID");
            break;

        case ESP_OTA_IMG_ABORTED:
            ESP_LOGW(TAG, "OTA image state: ABORTED");
            break;

        case ESP_OTA_IMG_UNDEFINED:
            ESP_LOGI(TAG, "OTA image state: UNDEFINED");
            break;

        default:
            ESP_LOGW(TAG,
                     "OTA image state: unknown (%d)",
                     ota_state);
            break;
    }

    /*
     * We intentionally do NOT call:
     *
     *     esp_ota_mark_app_valid_cancel_rollback();
     *
     * here.
     *
     * A firmware image in PENDING_VERIFY state has not yet passed the
     * UCM application-level health checks.
     */

    return ESP_OK;
}


/**
 * @brief Confirm that the currently running firmware has passed
 *        application-level validation.
 *
 * Calling this function marks the current OTA image as valid and
 * cancels the pending rollback mechanism.
 *
 * This function should only be called after the UCM application has
 * successfully completed its required startup checks.
 */
esp_err_t ota_manager_confirm_running_image(void)
{
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "Failed to confirm running firmware: %s",
                 esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Running firmware confirmed VALID");

    return ESP_OK;
}
/**
 * @brief Download and install a new firmware image.
 *
 * The firmware is written to the inactive OTA partition by ESP-IDF.
 * If the download and image verification succeed, the new partition
 * is selected as the next boot partition.
 *
 * The device is restarted after the new image is selected.
 *
 * @param firmware_url HTTP/HTTPS URL of the firmware binary.
 *
 * @return ESP_OK if the OTA image was successfully installed and the
 *         next boot partition was selected. This function normally
 *         does not return on success because the device restarts.
 */
esp_err_t ota_manager_update(const char *firmware_url)
{
    if (firmware_url == NULL || firmware_url[0] == '\0') {
        ESP_LOGE(TAG, "OTA firmware URL is empty");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting OTA update");
    ESP_LOGI(TAG, "Firmware URL: %s", firmware_url);

    esp_http_client_config_t http_config = {
        .url = firmware_url,
        .timeout_ms = 10000,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_err_t err = esp_https_ota(&ota_config);

    if (err != ESP_OK) {
        ESP_LOGE(TAG,
                 "OTA update failed: %s",
                 esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG,
             "OTA update successful. Restarting into new firmware...");

    vTaskDelay(pdMS_TO_TICKS(1000));

    esp_restart();

    return ESP_OK;
}