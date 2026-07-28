#include <string.h>
#include "app_wifi_prov.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "mdns.h"

static const char *TAG = "app_wifi_prov";

#define SETUP_AP_SSID   "UCM-SENSOR-SETUP"
#define NVS_NAMESPACE   "wifi_cfg"
#define NVS_KEY_SSID    "ssid"
#define NVS_KEY_PASS    "pass"
#define MAXIMUM_RETRY   5

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int s_retry_num = 0;

// Simple form page. No JS framework, no CSS files -- just works in any
// phone browser without extra requests.
static const char SETUP_FORM_HTML[] =
"<!DOCTYPE html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1'>"
"<title>Wi-Fi Setup</title>"
"<style>"
"body{font-family:sans-serif;background:#111;color:#eee;padding:24px;}"
"h1{font-size:1.2em;color:#8ab4f8;}"
"label{display:block;margin-top:16px;font-size:0.9em;color:#aaa;}"
"input{width:100%;padding:10px;margin-top:4px;border-radius:8px;"
"border:none;font-size:1em;box-sizing:border-box;}"
"button{margin-top:24px;width:100%;padding:12px;border-radius:8px;"
"border:none;background:#8ab4f8;color:#111;font-size:1em;font-weight:600;}"
"</style></head><body>"
"<h1>Connect the sensor to your Wi-Fi</h1>"
"<form method='POST' action='/save'>"
"<label>Wi-Fi network name (SSID)</label>"
"<input name='ssid' required>"
"<label>Password</label>"
"<input name='pass' type='password'>"
"<button type='submit'>Save &amp; Connect</button>"
"</form></body></html>";

static const char SAVED_HTML[] =
"<!DOCTYPE html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1'>"
"</head><body style='font-family:sans-serif;background:#111;color:#eee;"
"padding:24px;'><h1 style='color:#8ab4f8;'>Saved!</h1>"
"<p>The sensor is rebooting and connecting to your Wi-Fi. "
"You can close this page and reconnect your phone to your normal Wi-Fi.</p>"
"</body></html>";

static esp_err_t form_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, SETUP_FORM_HTML, HTTPD_RESP_USE_STRLEN);
}

// Minimal application/x-www-form-urlencoded decoder for our two fields.
// Good enough for SSID/password text -- handles %XX and + escapes.
static void url_decode(char *dst, const char *src, size_t dst_size)
{
    size_t di = 0;
    while (*src && di + 1 < dst_size) {
        if (*src == '%' && src[1] && src[2]) {
            int hi = src[1] >= 'A' ? (src[1] & ~0x20) - 'A' + 10 : src[1] - '0';
            int lo = src[2] >= 'A' ? (src[2] & ~0x20) - 'A' + 10 : src[2] - '0';
            dst[di++] = (char)((hi << 4) | lo);
            src += 3;
        } else if (*src == '+') {
            dst[di++] = ' ';
            src++;
        } else {
            dst[di++] = *src++;
        }
    }
    dst[di] = '\0';
}

static bool extract_field(const char *body, const char *key, char *out, size_t out_size)
{
    char search[32];
    snprintf(search, sizeof(search), "%s=", key);
    const char *found = strstr(body, search);
    if (!found) {
        return false;
    }
    found += strlen(search);
    const char *end = strchr(found, '&');
    size_t raw_len = end ? (size_t)(end - found) : strlen(found);
    if (raw_len >= out_size) {
        raw_len = out_size - 1;
    }
    char raw[128];
    if (raw_len >= sizeof(raw)) {
        raw_len = sizeof(raw) - 1;
    }
    memcpy(raw, found, raw_len);
    raw[raw_len] = '\0';
    url_decode(out, raw, out_size);
    return true;
}

static esp_err_t save_post_handler(httpd_req_t *req)
{
    char body[256] = {0};
    int total = 0;
    int remaining = req->content_len;
    if (remaining >= (int)sizeof(body)) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    while (remaining > 0) {
        int received = httpd_req_recv(req, body + total, remaining);
        if (received <= 0) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        total += received;
        remaining -= received;
    }
    body[total] = '\0';

    char ssid[64] = {0};
    char pass[64] = {0};
    extract_field(body, "ssid", ssid, sizeof(ssid));
    extract_field(body, "pass", pass, sizeof(pass));

    ESP_LOGI(TAG, "Received setup form: SSID '%s'", ssid);

    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_str(nvs, NVS_KEY_SSID, ssid));
    ESP_ERROR_CHECK(nvs_set_str(nvs, NVS_KEY_PASS, pass));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, SAVED_HTML, HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Credentials saved, rebooting in 2s...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK; // unreachable
}

static void start_setup_server_and_ap(void)
{
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = SETUP_AP_SSID,
            .ssid_len = strlen(SETUP_AP_SSID),
            .channel = 1,
            .authmode = WIFI_AUTH_OPEN,
            .max_connection = 2,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Setup AP started: %s (open network)", SETUP_AP_SSID);
    ESP_LOGI(TAG, "Connect your phone to it, then open http://192.168.4.1");

    httpd_config_t http_cfg = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(httpd_start(&server, &http_cfg));

    httpd_uri_t form_uri = { .uri = "/", .method = HTTP_GET, .handler = form_get_handler };
    httpd_uri_t save_uri = { .uri = "/save", .method = HTTP_POST, .handler = save_post_handler };
    httpd_register_uri_handler(server, &form_uri);
    httpd_register_uri_handler(server, &save_uri);

    ESP_LOGI(TAG, "Setup web server started");

    // Board stays here indefinitely, serving the setup page, until the
    // form is submitted (which calls esp_restart() itself).
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

static void event_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retrying connection to AP...");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP address:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t app_wifi_prov_reset_credentials(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return err;
    }
    nvs_erase_all(nvs);
    nvs_commit(nvs);
    nvs_close(nvs);
    return ESP_OK;
}

esp_err_t app_wifi_prov_start(void)
{
   
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    char ssid[64] = {0};
    char pass[64] = {0};
    bool have_creds = false;

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        size_t ssid_len = sizeof(ssid);
        size_t pass_len = sizeof(pass);
        if (nvs_get_str(nvs, NVS_KEY_SSID, ssid, &ssid_len) == ESP_OK &&
            strlen(ssid) > 0) {
            nvs_get_str(nvs, NVS_KEY_PASS, pass, &pass_len);
            have_creds = true;
        }
        nvs_close(nvs);
    }

    if (!have_creds) {
        ESP_LOGI(TAG, "No saved Wi-Fi credentials, entering setup mode");
        start_setup_server_and_ap(); // never returns
    }

    ESP_LOGI(TAG, "Using saved Wi-Fi credentials, SSID '%s'", ssid);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to SSID: %s...", ssid);

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE, portMAX_DELAY);

  if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Successfully connected to AP SSID: %s", ssid);

        // Advertise as ucm-sensor.local so it's reachable without
        // knowing the site's DHCP-assigned IP.
        esp_err_t mdns_err = mdns_init();
        if (mdns_err == ESP_OK) {
            mdns_hostname_set("ucm-sensor");
            mdns_instance_name_set("UCM Sensor Dashboard");
            mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
            ESP_LOGI(TAG, "mDNS started: http://ucm-sensor.local");
        } else {
            ESP_LOGW(TAG, "mDNS init failed: %s", esp_err_to_name(mdns_err));
        }

        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to connect to SSID: %s", ssid);
       // --- Field-deployment fallback ---
    // Couldn't connect with the saved network (wrong site, changed
    // password, etc). Wipe the bad credentials and drop into the
    // same browser-based setup flow, no computer/USB needed.
    ESP_LOGW(TAG, "Falling back to setup mode -- clearing saved credentials");
    app_wifi_prov_reset_credentials();
    esp_wifi_stop();
    start_setup_server_and_ap();  // never returns
    return ESP_FAIL; // unreachable
}
