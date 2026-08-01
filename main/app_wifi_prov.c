#include <string.h>
#include <stdlib.h>
#include <ctype.h>
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
#define NVS_KEY_NAME    "node_name"
#define NVS_KEY_DESC    "node_desc"
#define NVS_KEY_LAT     "node_lat"
#define NVS_KEY_LON     "node_lon"
#define MAXIMUM_RETRY   5

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int s_retry_num = 0;

// Cached after a successful load/save so app_wifi_prov_get_node_info()
// can hand it back without re-touching NVS.
static char s_node_name[32] = {0};
static char s_node_desc[64] = {0};
static float s_node_lat = 0.0f;
static float s_node_lon = 0.0f;

// Setup form: Wi-Fi credentials + node identity/location fields.
static const char SETUP_FORM_HTML[] =
"<!DOCTYPE html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1'>"
"<title>Sensor Setup</title>"
"<style>"
"body{font-family:sans-serif;background:#111;color:#eee;padding:24px;}"
"h1{font-size:1.2em;color:#8ab4f8;}"
"h2{font-size:1em;color:#8ab4f8;margin-top:28px;}"
"label{display:block;margin-top:16px;font-size:0.9em;color:#aaa;}"
"input{width:100%;padding:10px;margin-top:4px;border-radius:8px;"
"border:none;font-size:1em;box-sizing:border-box;}"
"button{margin-top:28px;width:100%;padding:12px;border-radius:8px;"
"border:none;background:#8ab4f8;color:#111;font-size:1em;font-weight:600;}"
"</style></head><body>"
"<h1>Set up this sensor</h1>"
"<form method='POST' action='/save'>"
"<h2>Wi-Fi</h2>"
"<label>Network name (SSID)</label>"
"<input name='ssid' required>"
"<label>Password</label>"
"<input name='pass' type='password'>"
"<h2>Sensor info (optional)</h2>"
"<label>Node name (e.g. Lobby, Warehouse-2)</label>"
"<input name='name' maxlength='31'>"
"<label>Description</label>"
"<input name='desc' maxlength='63'>"
"<label>Latitude</label>"
"<input name='lat' type='text' inputmode='decimal' placeholder='e.g. 40.7128'>"
"<label>Longitude</label>"
"<input name='lon' type='text' inputmode='decimal' placeholder='e.g. -74.0060'>"
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

// Minimal application/x-www-form-urlencoded decoder.
// Handles %XX and + escapes -- good enough for our text fields.
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
        out[0] = '\0';
        return false;
    }
    found += strlen(search);
    const char *end = strchr(found, '&');
    size_t raw_len = end ? (size_t)(end - found) : strlen(found);
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
    char body[512] = {0};
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
    char name[32] = {0};
    char desc[64] = {0};
    char lat_str[32] = {0};
    char lon_str[32] = {0};

    extract_field(body, "ssid", ssid, sizeof(ssid));
    extract_field(body, "pass", pass, sizeof(pass));
    extract_field(body, "name", name, sizeof(name));
    extract_field(body, "desc", desc, sizeof(desc));
    extract_field(body, "lat", lat_str, sizeof(lat_str));
    extract_field(body, "lon", lon_str, sizeof(lon_str));

    ESP_LOGI(TAG, "Received setup form: SSID '%s', name '%s'", ssid, name);

    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_str(nvs, NVS_KEY_SSID, ssid));
    ESP_ERROR_CHECK(nvs_set_str(nvs, NVS_KEY_PASS, pass));
    ESP_ERROR_CHECK(nvs_set_str(nvs, NVS_KEY_NAME, name));
    ESP_ERROR_CHECK(nvs_set_str(nvs, NVS_KEY_DESC, desc));
    // Stored as strings (NVS has no native float type); parsed back on load.
    ESP_ERROR_CHECK(nvs_set_str(nvs, NVS_KEY_LAT, lat_str[0] ? lat_str : "0"));
    ESP_ERROR_CHECK(nvs_set_str(nvs, NVS_KEY_LON, lon_str[0] ? lon_str : "0"));
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

void app_wifi_prov_get_node_info(char *name, size_t name_size,
                                  char *desc, size_t desc_size,
                                  float *lat, float *lon)
{
    if (name && name_size) {
        strncpy(name, s_node_name, name_size - 1);
        name[name_size - 1] = '\0';
    }
    if (desc && desc_size) {
        strncpy(desc, s_node_desc, desc_size - 1);
        desc[desc_size - 1] = '\0';
    }
    if (lat) {
        *lat = s_node_lat;
    }
    if (lon) {
        *lon = s_node_lon;
    }
}

// Turns a user-typed node name into a valid, unique-ish mDNS hostname:
// lowercase, only [a-z0-9-], collapsed, truncated. Falls back to
// "ucm-sensor" if the name is empty or ends up with nothing usable.
static void sanitize_hostname(const char *in, char *out, size_t out_size)
{
    size_t oi = 0;
    bool last_was_dash = false;
    for (size_t i = 0; in[i] != '\0' && oi + 1 < out_size; i++) {
        char c = in[i];
        if (isalnum((unsigned char)c)) {
            out[oi++] = (char)tolower((unsigned char)c);
            last_was_dash = false;
        } else if (!last_was_dash && oi > 0) {
            out[oi++] = '-';
            last_was_dash = true;
        }
    }
    while (oi > 0 && out[oi - 1] == '-') {
        oi--;
    }
    out[oi] = '\0';

    if (out[0] == '\0') {
        strncpy(out, "ucm-sensor", out_size - 1);
        out[out_size - 1] = '\0';
    }
}

esp_err_t app_wifi_prov_start(void)
{
    s_wifi_event_group = xEventGroupCreate();

 //   ESP_ERROR_CHECK(esp_netif_init());
 //  ESP_ERROR_CHECK(esp_event_loop_create_default());
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

        size_t name_len = sizeof(s_node_name);
        size_t desc_len = sizeof(s_node_desc);
        nvs_get_str(nvs, NVS_KEY_NAME, s_node_name, &name_len);
        nvs_get_str(nvs, NVS_KEY_DESC, s_node_desc, &desc_len);

        char lat_str[32] = {0};
        char lon_str[32] = {0};
        size_t lat_len = sizeof(lat_str);
        size_t lon_len = sizeof(lon_str);
        if (nvs_get_str(nvs, NVS_KEY_LAT, lat_str, &lat_len) == ESP_OK) {
            s_node_lat = strtof(lat_str, NULL);
        }
        if (nvs_get_str(nvs, NVS_KEY_LON, lon_str, &lon_len) == ESP_OK) {
            s_node_lon = strtof(lon_str, NULL);
        }

        nvs_close(nvs);
    }

    if (!have_creds) {
        ESP_LOGI(TAG, "No saved Wi-Fi credentials, entering setup mode");
        start_setup_server_and_ap(); // never returns
    }

    ESP_LOGI(TAG, "Using saved Wi-Fi credentials, SSID '%s'", ssid);
    ESP_LOGI(TAG, "Node name '%s', lat=%.5f lon=%.5f", s_node_name, s_node_lat, s_node_lon);

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

        char hostname[32];
        sanitize_hostname(s_node_name, hostname, sizeof(hostname));

        esp_err_t mdns_err = mdns_init();
        if (mdns_err == ESP_OK) {
            mdns_hostname_set(hostname);
            mdns_instance_name_set(s_node_desc[0] ? s_node_desc : "UCM Sensor Dashboard");
            mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
            ESP_LOGI(TAG, "mDNS started: http://%s.local", hostname);
        } else {
            ESP_LOGW(TAG, "mDNS init failed: %s", esp_err_to_name(mdns_err));
        }

        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to connect to SSID: %s", ssid);
    ESP_LOGW(TAG, "Falling back to setup mode -- clearing saved credentials");
    app_wifi_prov_reset_credentials();
    esp_wifi_stop();
    start_setup_server_and_ap(); // never returns
    return ESP_FAIL; // unreachable
}