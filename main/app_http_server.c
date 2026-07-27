#include <string.h>
#include <stdio.h>
#include "app_http_server.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "app_http_server";

static SemaphoreHandle_t s_reading_mutex;
static sensor_reading_t s_latest_reading = {0};
static httpd_handle_t s_server = NULL;

// Minimal single-page dashboard: polls /data every 2s and updates the DOM.
// No external dependencies, works in any phone browser.
static const char INDEX_HTML[] =
"<!DOCTYPE html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1'>"
"<title>Air Sensor</title>"
"<style>"
"body{font-family:sans-serif;background:#111;color:#eee;margin:0;padding:24px;}"
"h1{font-size:1.2em;color:#8ab4f8;margin-bottom:24px;}"
".grid{display:grid;grid-template-columns:1fr 1fr;gap:16px;}"
".card{background:#1e1e1e;border-radius:12px;padding:16px;}"
".label{font-size:0.85em;color:#999;}"
".value{font-size:1.8em;font-weight:600;margin-top:4px;}"
"</style></head><body>"
"<h1>ESP32-P4 Air Sensor</h1>"
"<div class='grid' id='grid'></div>"
"<script>"
"const fields=[['pm1_0','PM1.0','ug/m3'],['pm2_5','PM2.5','ug/m3'],"
"['pm4_0','PM4.0','ug/m3'],['pm10','PM10','ug/m3'],"
"['temperature_c','Temp','C'],['humidity_pct','Humidity','%'],"
"['voc_index','VOC index','']];"
"const grid=document.getElementById('grid');"
"fields.forEach(([key,label])=>{"
"  const c=document.createElement('div');c.className='card';"
"  c.innerHTML=`<div class='label'>${label}</div><div class='value' id='${key}'>--</div>`;"
"  grid.appendChild(c);});"
"async function poll(){"
"  try{const r=await fetch('/data');const d=await r.json();"
"    fields.forEach(([key,_,unit])=>{"
"      document.getElementById(key).textContent=d[key].toFixed(1)+(unit?(' '+unit):'');"
"    });"
"  }catch(e){console.log('poll failed',e);}"
"  setTimeout(poll,2000);"
"}"
"poll();"
"</script></body></html>";

static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t data_get_handler(httpd_req_t *req)
{
    sensor_reading_t r;
    xSemaphoreTake(s_reading_mutex, portMAX_DELAY);
    r = s_latest_reading;
    xSemaphoreGive(s_reading_mutex);

    char buf[256];
    int n = snprintf(buf, sizeof(buf),
        "{\"pm1_0\":%.2f,\"pm2_5\":%.2f,\"pm4_0\":%.2f,\"pm10\":%.2f,"
        "\"temperature_c\":%.2f,\"humidity_pct\":%.2f,\"voc_index\":%.2f}",
        r.pm1_0, r.pm2_5, r.pm4_0, r.pm10,
        r.temperature_c, r.humidity_pct, r.voc_index);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, n);
}

void app_http_server_update_reading(const sensor_reading_t *reading)
{
    if (!s_reading_mutex) {
        return; // server not started yet
    }
    xSemaphoreTake(s_reading_mutex, portMAX_DELAY);
    s_latest_reading = *reading;
    xSemaphoreGive(s_reading_mutex);
}

esp_err_t app_http_server_start(void)
{
    s_reading_mutex = xSemaphoreCreateMutex();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    httpd_uri_t index_uri = {
        .uri = "/", .method = HTTP_GET, .handler = index_get_handler,
    };
    httpd_uri_t data_uri = {
        .uri = "/data", .method = HTTP_GET, .handler = data_get_handler,
    };
    httpd_register_uri_handler(s_server, &index_uri);
    httpd_register_uri_handler(s_server, &data_uri);

    ESP_LOGI(TAG, "HTTP server started. Open http://<board-ip>/ on your phone.");
    return ESP_OK;
}
