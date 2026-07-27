#include "esp_http_server.h"
#include "esp_log.h"
#include "sen54_data.h"


static const char *TAG = "WEB_SERVER";


static esp_err_t root_handler(httpd_req_t *req)
{
    char html[512];

    snprintf(html,
             sizeof(html),

             "<html>"
             "<head>"
             "<meta http-equiv='refresh' content='5'>"
             "</head>"
             "<body>"
             "<h1>UCM Sensor Node</h1>"
             "<h2>SEN54 Data</h2>"
             "<p>PM1.0: %.1f ug/m3</p>"
             "<p>PM2.5: %.1f ug/m3</p>"
             "<p>PM4.0: %.1f ug/m3</p>"
             "<p>PM10: %.1f ug/m3</p>"
             "<p>Temperature: %.1f C</p>"
             "<p>Humidity: %.1f %%</p>"
             "<p>VOC Index: %.1f</p>"
             "</body>"
             "</html>",

             current_sensor_data.pm1,
             current_sensor_data.pm25,
             current_sensor_data.pm4,
             current_sensor_data.pm10,
             current_sensor_data.temperature,
             current_sensor_data.humidity,
             current_sensor_data.voc);         

    httpd_resp_send(req,
                    html,
                    HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}
static esp_err_t data_handler(httpd_req_t *req)
{
    char json[256];

    snprintf(json,
             sizeof(json),
             "{"
             "\"node\":\"%s\","
             "\"pm1\":%.1f,"
             "\"pm25\":%.1f,"
             "\"pm4\":%.1f,"
             "\"pm10\":%.1f,"
             "\"temperature\":%.1f,"
             "\"humidity\":%.1f,"
             "\"voc\":%.1f,"
       
             "}",
	     current_sensor_data.node_id,
             current_sensor_data.pm1,
             current_sensor_data.pm25,
             current_sensor_data.pm4,
             current_sensor_data.pm10,
             current_sensor_data.temperature,
             current_sensor_data.humidity,
             current_sensor_data.voc
             );

    httpd_resp_set_type(req, "application/json");

    httpd_resp_send(req,
                    json,
                    HTTPD_RESP_USE_STRLEN);

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