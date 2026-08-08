/**
 * @file network_manager.c
 * @brief Top-level network bring-up for a UCM sensor node: tries wired
 *        Ethernet first, and falls back to Wi-Fi provisioning if Ethernet
 *        isn't available.
 *
 * This is the module behind network_start(), which node_main.c's
 * app_main() calls once at boot and gates all of the node's
 * identity/MQTT setup on (see node_main.c's documentation). It is the
 * single entry point other code uses to bring up connectivity -- callers
 * don't need to know or care whether the node ends up connected via
 * Ethernet or Wi-Fi.
 *
 * STRATEGY: this node is designed to be PoE/Ethernet-first (consistent
 * with the "ESP32-P4 PoE" hardware description elsewhere in this
 * project), with Wi-Fi provisioning treated as a fallback path rather
 * than the primary connectivity method -- e.g. for initial setup/
 * provisioning, or for deployments without wired Ethernet available.
 *
 * TIMING: waits up to 15 seconds for an Ethernet link to come up
 * (150 x 100ms), and then, if a link is detected, up to a further 10
 * seconds for that interface to actually obtain an IP address via DHCP
 * (100 x 100ms) -- see the loop-by-loop breakdown in network_start()
 * below. Total worst-case time spent waiting on Ethernet before falling
 * back to Wi-Fi provisioning is therefore up to ~25 seconds.
 */

#include "network_manager.h"
#include "ethernet_manager.h"  // ethernet_init(), ethernet_link_up(), ethernet_has_ip()
#include "app_wifi_prov.h"     // app_wifi_prov_start() -- Wi-Fi provisioning fallback
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "NETWORK";

/**
 * @brief Brings up network connectivity for this node: attempts Ethernet
 *        first, and falls back to starting Wi-Fi provisioning if Ethernet
 *        doesn't come up in time.
 *
 * @return ESP_OK if Ethernet obtained an IP address within the timeout
 *         windows described below. Otherwise, returns whatever
 *         app_wifi_prov_start() returns (this function does not
 *         reinterpret or override that value) -- i.e. a successful
 *         Wi-Fi-provisioning path also returns ESP_OK, while a failure
 *         there propagates back to the caller as-is. Note that
 *         "Ethernet failed" and "Wi-Fi provisioning failed" therefore
 *         cannot be distinguished from this function's return value
 *         alone -- both surface as whatever app_wifi_prov_start() reports.
 *
 * Sequence:
 *  1. ethernet_init() -- initializes the Ethernet driver/interface.
 *     Wrapped in ESP_ERROR_CHECK(), so a failure here (e.g. PHY not
 *     detected, driver init error) aborts firmware boot entirely rather
 *     than falling through to the Wi-Fi fallback below -- this function
 *     assumes Ethernet hardware initialization itself should always
 *     succeed on this hardware, and only treats "no link"/"no IP" as the
 *     recoverable failure modes worth falling back from.
 *  2. Outer loop (up to 150 x 100ms = 15s): polls ethernet_link_up(),
 *     i.e. waits for a physical link (cable plugged in, PHY negotiated)
 *     to appear. If no link is detected within 15s, the loop simply ends
 *     and execution falls through to the "Ethernet unavailable" case
 *     below.
 *  3. Once a link is detected, inner loop (up to 100 x 100ms = 10s): polls
 *     ethernet_has_ip(), i.e. waits for that link to actually obtain an IP
 *     address (via DHCP, presumably handled asynchronously by
 *     ethernet_manager/esp_netif in the background). If an IP is obtained
 *     in time, logs success and returns ESP_OK immediately -- Ethernet is
 *     considered ready and Wi-Fi provisioning is never attempted.
 *  4. If a link came up but no IP was obtained within the inner loop's 10s
 *     window, `break` exits the outer loop early (rather than continuing
 *     to poll for a link that's already known to be up), and execution
 *     falls through to the same "Ethernet unavailable" fallback as the
 *     no-link-at-all case in step 2 -- even though, in this branch, the
 *     physical Ethernet link genuinely was detected. The "Ethernet
 *     unavailable" log message that follows is a bit imprecise for this
 *     particular case (link up, but no DHCP lease) versus the true
 *     no-link case; worth keeping in mind when debugging a node that logs
 *     this despite having a cable plugged in -- the underlying issue in
 *     that scenario would be DHCP/upstream network configuration, not the
 *     physical Ethernet connection itself.
 *  5. Fallback: app_wifi_prov_start() is called (and its result returned
 *     directly) whenever Ethernet did not fully come up (no link, or link
 *     without an IP) within the timeouts above. Presumably this starts
 *     whatever Wi-Fi provisioning flow (e.g. a BLE/SoftAP-based
 *     onboarding flow) is implemented in app_wifi_prov.c.
 *
 * Note: none of these hardcoded loop counts (150, 100) or per-iteration
 * delays (100ms) are expressed in terms of the ETHERNET_TIMEOUT_MS
 * constant defined (but currently unused) in node_main.c -- that constant
 * and this function's actual ~15s/~10s timeouts are not currently wired
 * together, so changing one will not affect the other.
 */
esp_err_t network_start(void)
{
    ESP_LOGI(TAG, "Starting network manager");
    ESP_ERROR_CHECK(ethernet_init());
    ESP_LOGI(TAG, "Waiting for Ethernet");
    for (int i = 0; i < 150; i++)
    {
        if (ethernet_link_up())
        {
            ESP_LOGI(TAG, "Ethernet link detected");
            for (int j = 0; j < 100; j++)
            {
                if (ethernet_has_ip())
                {
                    ESP_LOGI(TAG, "Ethernet ready");
                    return ESP_OK;
                }
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "Ethernet unavailable");
    return app_wifi_prov_start();
}