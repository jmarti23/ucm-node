#include "esp_http_server.h"
#include "esp_log.h"
#include "sen54_data.h"

static const char *TAG = "WEB_SERVER";

static esp_err_t root_handler(httpd_req_t *req)
{
    const char html[] =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<title>UCM Sensor Dashboard</title>"
        "<style>"
        "body { font-family: Arial, sans-serif; margin: 20px; }"
        "h1 { font-size: 28px; }"
        ".card { border: 1px solid #ccc; border-radius: 8px; padding: 15px; margin-bottom: 15px; }"
        ".value { font-size: 24px; font-weight: bold; }"
        "</style>"
        "</head>"

        "<body>"

        "<h1 id='name'>UCM Sensor Dashboard</h1>"

        "<div class='card'>"
        "<h2>Environment</h2>"
        "<p>Temperature</p>"
        "<div class='value'><span id='temperature'>--</span> C</div>"
        "<p>Humidity</p>"
        "<div class='value'><span id='humidity'>--</span> %</div>"
        "</div>"

        "<div class='card'>"
        "<h2>Air Quality</h2>"
        "<p>PM1.0: <span id='pm1'>--</span> ug/m3</p>"
        "<p>PM2.5: <span id='pm25'>--</span> ug/m3</p>"
        "<p>PM4.0: <span id='pm4'>--</span> ug/m3</p>"
        "<p>PM10: <span id='pm10'>--</span> ug/m3</p>"
        "<p>VOC Index: <span id='voc'>--</span></p>"
        "</div>"

        "<div class='card'>"
        "<h2>Node Information</h2>"
        "<p>ID: <span id='node'>--</span></p>"
        "<p>Location: <span id='location'>--</span></p>"
        "<p>Last Reading: <span id='timestamp'>--</span></p>"
        "</div>"

        "<script>"
        "function updateData() {"
        " fetch('/data')"
        " .then(response => response.json())"
        " .then(data => {"
        " document.getElementById('name').innerHTML = data.name || 'UCM Sensor Node';"
        " document.getElementById('temperature').innerHTML = data.temperature.toFixed(1);"
        " document.getElementById('humidity').innerHTML = data.humidity.toFixed(1);"
        " document.getElementById('pm1').innerHTML = data.pm1.toFixed(1);"
        " document.getElementById('pm25').innerHTML = data.pm25.toFixed(1);"
        " document.getElementById('pm4').innerHTML = data.pm4.toFixed(1);"
        " document.getElementById('pm10').innerHTML = data.pm10.toFixed(1);"
        " document.getElementById('voc').innerHTML = data.voc.toFixed(1);"
        " document.getElementById('node').innerHTML = data.node;"
        " document.getElementById('location').innerHTML = data.lat + ', ' + data.lon;"
        " document.getElementById('timestamp').innerHTML = data.timestamp;"
        " })"
        " .catch(error => console.log(error));"
        "}"

        "updateData();"
        "setInterval(updateData, 5000);"

        "</script>"

        "</body>"
        "</html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static esp_err_t data_handler(httpd_req_t *req)
{
    char json[512];
    snprintf(json,
             sizeof(json),
             "{"
             "\"node\":\"%s\","
             "\"name\":\"%s\","
             "\"description\":\"%s\","
             "\"lat\":%.5f,"
             "\"lon\":%.5f,"
             "\"timestamp\":\"%s\","
             "\"pm1\":%.1f,"
             "\"pm25\":%.1f,"
             "\"pm4\":%.1f,"
             "\"pm10\":%.1f,"
             "\"temperature\":%.1f,"
             "\"humidity\":%.1f,"
             "\"voc\":%.1f"
             "}",
             current_sensor_data.node_id,
             current_sensor_data.node_name,
             current_sensor_data.description,
             current_sensor_data.latitude,
             current_sensor_data.longitude,
             current_sensor_data.timestamp,
             current_sensor_data.pm1,
             current_sensor_data.pm25,
             current_sensor_data.pm4,
             current_sensor_data.pm10,
             current_sensor_data.temperature,
             current_sensor_data.humidity,
             current_sensor_data.voc);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root);

        httpd_uri_t data = {
            .uri = "/data",
            .method = HTTP_GET,
            .handler = data_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &data);

        ESP_LOGI(TAG, "Web server started");
    }
}