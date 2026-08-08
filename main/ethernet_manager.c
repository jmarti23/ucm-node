/**
 * @file ethernet_manager.c
 * @brief Ethernet driver bring-up and link/IP state tracking for a UCM
 *        sensor node.
 *
 * This is the layer directly beneath network_manager.c: it owns the
 * ESP-IDF Ethernet driver/netif lifecycle and exposes two simple state
 * queries -- ethernet_link_up() and ethernet_has_ip() -- that
 * network_manager.c's network_start() polls in a loop while waiting for
 * Ethernet to come up. This file does not make any decisions about
 * falling back to Wi-Fi; it only reports Ethernet's current state and
 * lets network_manager.c decide what to do with it.
 *
 * RELATIONSHIP TO ethernet_init.c: despite the similar name,
 * ethernet_init() (defined in *this* file) is a different function from
 * ucm_eth_init() (called from here, presumably defined in
 * ethernet_init.c). This file's ethernet_init() is the high-level
 * "set up ESP-IDF's networking plumbing around the Ethernet driver"
 * entry point that network_manager.c calls; ucm_eth_init() is
 * responsible for the lower-level PHY/MAC driver setup that produces the
 * eth_handles this file then wires up to esp_netif.
 *
 * STATE TRACKING: link-up/link-down and IP-acquired/IP-lost transitions
 * are driven entirely by ESP-IDF's event system (ETH_EVENT / IP_EVENT),
 * handled asynchronously in ethernet_event_handler() below, which updates
 * two module-level flags that ethernet_link_up()/ethernet_has_ip() simply
 * return. There's no polling of hardware state directly in those two
 * getter functions -- they only reflect whatever the most recent relevant
 * event reported.
 */

#include "ethernet_manager.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_wifi_prov.h"      // Included but not referenced anywhere in this file -- likely a leftover/unnecessary include.
#include "ethernet_init.h"      // ucm_eth_init() -- lower-level PHY/MAC driver bring-up, implemented elsewhere (presumably ethernet_init.c)
#include "esp_eth_netif_glue.h" // esp_eth_new_netif_glue() -- bridges the raw Ethernet driver handle to an esp_netif interface

static const char *TAG = "ETHERNET";

// Link/IP state flags, updated only from ethernet_event_handler() (running
// in the default ESP-IDF event loop's task context) and read from
// ethernet_link_up()/ethernet_has_ip() (called from other tasks, e.g.
// network_manager.c's polling loop). As with other shared state in this
// project, there's no explicit lock around these -- acceptable in
// practice here since single-byte bool reads/writes on this platform are
// effectively atomic, so callers will only ever observe a fully "true" or
// fully "false" value, just not necessarily one that's perfectly
// up-to-the-microsecond fresh.
static bool eth_link_up = false;
static bool eth_has_ip = false;

// Array of Ethernet driver handles (populated by ucm_eth_init()) and how
// many of them there are. Only eth_handles[0] is ever used below (in the
// netif glue creation and esp_eth_start() call) -- see the note on that in
// ethernet_init() -- so if ucm_eth_init() ever returns more than one
// handle (e.g. multiple PHYs/interfaces on future hardware revisions),
// only the first is currently brought up; eth_cnt itself is otherwise
// unused beyond being populated.
static esp_eth_handle_t *eth_handles = NULL;
static uint8_t eth_cnt = 0;

/**
 * @brief ESP-IDF event handler for both Ethernet link-state events
 *        (ETH_EVENT) and the Ethernet-specific "got an IP" event
 *        (IP_EVENT / IP_EVENT_ETH_GOT_IP). Registered twice, for both
 *        event bases, in ethernet_init() below.
 *
 * @param arg         Unused (registered with NULL as the handler arg for
 *                     both registrations).
 * @param event_base  Which event family fired: ETH_EVENT or IP_EVENT.
 * @param event_id     Specific event within that base.
 * @param event_data   Unused here (neither branch below inspects it).
 *
 * Handled cases:
 *   - ETH_EVENT / ETHERNET_EVENT_CONNECTED: sets eth_link_up = true. This
 *     fires when a physical link is established (cable plugged in, PHY
 *     negotiated) -- it does NOT mean an IP address has been obtained yet.
 *   - ETH_EVENT / ETHERNET_EVENT_DISCONNECTED: sets eth_link_up = false
 *     AND eth_has_ip = false -- losing the physical link is treated as
 *     also losing any IP that had been assigned (a new DHCP lease would
 *     be needed once the link comes back), rather than leaving a stale
 *     "has IP" flag set while the link is actually down.
 *   - IP_EVENT / IP_EVENT_ETH_GOT_IP: sets eth_has_ip = true, once DHCP
 *     (handled internally by esp_netif) successfully assigns this
 *     interface an address.
 *
 * Other ETH_EVENT IDs (e.g. ETHERNET_EVENT_START, ETHERNET_EVENT_STOP)
 * are implicitly ignored here, since the handler is registered for
 * ESP_EVENT_ANY_ID on ETH_EVENT but this function only checks for the
 * CONNECTED/DISCONNECTED cases explicitly.
 */
static void ethernet_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    if (event_base == ETH_EVENT)
    {
        if (event_id == ETHERNET_EVENT_CONNECTED)
        {
            eth_link_up = true;
            ESP_LOGI(TAG, "Ethernet link up");
        }
        if (event_id == ETHERNET_EVENT_DISCONNECTED)
        {
            eth_link_up = false;
            eth_has_ip = false;
            ESP_LOGI(TAG, "Ethernet link down");
        }
    }
    if (event_base == IP_EVENT &&
        event_id == IP_EVENT_ETH_GOT_IP)
    {
        eth_has_ip = true;
        ESP_LOGI(TAG, "Ethernet got IP");
    }
}

/**
 * @brief Returns whether a physical Ethernet link is currently up.
 *
 * @return Current value of the eth_link_up flag, last set by
 *         ethernet_event_handler() in response to an
 *         ETHERNET_EVENT_CONNECTED/DISCONNECTED event. True means the
 *         physical link is established; it says nothing about whether an
 *         IP address has been obtained (see ethernet_has_ip() for that).
 *
 * Polled by network_manager.c's network_start() while waiting for
 * Ethernet to come up.
 */
bool ethernet_link_up(void)
{
    return eth_link_up;
}

/**
 * @brief Returns whether this node's Ethernet interface currently has an
 *        IP address.
 *
 * @return Current value of the eth_has_ip flag, last set to true by
 *         ethernet_event_handler() in response to IP_EVENT_ETH_GOT_IP,
 *         and reset to false whenever the link goes down
 *         (ETHERNET_EVENT_DISCONNECTED).
 *
 * Polled by network_manager.c's network_start() to determine when
 * Ethernet is fully ready for use (link up AND IP assigned), not just
 * physically connected.
 */
bool ethernet_has_ip(void)
{
    return eth_has_ip;
}

/**
 * @brief Initializes ESP-IDF's networking stack and the Ethernet driver,
 *        wires the driver up to a network interface, registers the event
 *        handlers above, and starts the Ethernet driver.
 *
 * @return ESP_OK if every step below succeeds. On failure, returns/
 *         propagates the first non-ESP_OK error encountered, logging an
 *         error message first for most (but not quite all -- see the
 *         event-handler-registration steps) failure points.
 *
 * Sequence:
 *  1. esp_netif_init() -- initializes the underlying TCP/IP network
 *     interface layer (ESP-IDF's esp_netif component). ESP_ERROR_CHECK'd,
 *     so failure here aborts boot.
 *  2. esp_event_loop_create_default() -- creates the default system event
 *     loop if it doesn't already exist, needed for the ETH_EVENT/IP_EVENT
 *     handler registrations later in this function to have somewhere to
 *     dispatch to. Also ESP_ERROR_CHECK'd.
 *  3. ucm_eth_init(&eth_handles, &eth_cnt) -- performs the actual
 *     PHY/MAC-level Ethernet driver setup (implemented elsewhere,
 *     presumably ethernet_init.c) and populates eth_handles/eth_cnt.
 *     Unlike steps 1-2, a failure here is NOT ESP_ERROR_CHECK'd -- it's
 *     logged and this function returns the error code, letting the
 *     caller (network_manager.c) decide how to handle it (in practice,
 *     network_manager.c's ESP_ERROR_CHECK() around its own call to this
 *     function means a failure here still aborts boot overall, just one
 *     call frame up).
 *  4. esp_netif_new(&netif_cfg) with ESP_NETIF_DEFAULT_ETH() -- creates a
 *     new default-configured Ethernet network interface object.
 *  5. esp_eth_new_netif_glue(eth_handles[0]) -- creates the "glue" object
 *     that bridges the raw Ethernet driver handle to the esp_netif
 *     interface created in step 4. NOTE: this always uses eth_handles[0]
 *     specifically -- if ucm_eth_init() ever populates more than one
 *     handle (eth_cnt > 1), only the first Ethernet interface is ever
 *     attached/started; any additional handles are left unused by this
 *     function.
 *  6. esp_netif_attach(eth_netif, eth_glue) -- attaches the glue object to
 *     the netif, completing the wiring between the driver and the
 *     network stack.
 *  7. Registers ethernet_event_handler() for both ETH_EVENT
 *     (ESP_EVENT_ANY_ID, i.e. all Ethernet driver events) and IP_EVENT /
 *     IP_EVENT_ETH_GOT_IP specifically.
 *  8. esp_eth_start(eth_handles[0]) -- starts the Ethernet driver itself
 *     (again, only for the first handle -- see the note in step 5).
 *
 * On success, this function returning does NOT mean the link is up or an
 * IP has been obtained yet -- those are reported asynchronously later via
 * the event handler, and observed by callers via ethernet_link_up()/
 * ethernet_has_ip().
 */
esp_err_t ethernet_init(void)
{
    esp_err_t ret;
    ESP_LOGI(TAG, "Initializing Ethernet");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(
        esp_event_loop_create_default()
    );
    ret = ucm_eth_init(&eth_handles, &eth_cnt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ucm_eth_init failed");
        return ret;
    }
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);
    if (eth_netif == NULL) {
        ESP_LOGE(TAG, "Failed creating Ethernet netif");
        return ESP_FAIL;
    }
    esp_eth_netif_glue_handle_t eth_glue =
        esp_eth_new_netif_glue(eth_handles[0]);
    if (eth_glue == NULL) {
        ESP_LOGE(TAG, "Failed creating Ethernet glue");
        return ESP_FAIL;
    }
    ret = esp_netif_attach(eth_netif, eth_glue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed attaching Ethernet netif");
        return ret;
    }
    ret = esp_event_handler_register(
        ETH_EVENT,
        ESP_EVENT_ANY_ID,
        &ethernet_event_handler,
        NULL);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_ETH_GOT_IP,
        &ethernet_event_handler,
        NULL);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_eth_start(eth_handles[0]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed starting Ethernet");
        return ret;
    }
    ESP_LOGI(TAG, "Ethernet started");
    return ESP_OK;
}