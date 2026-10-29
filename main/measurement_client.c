#include "measurement_client.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_http_client.h"

#include "sntp_sync.h"

static const char *TAG = "MEAS_CLIENT";

#ifndef MEASUREMENTS_URL
#define MEASUREMENTS_URL "http://192.168.1.139:8080/api/gateway/measure"
#endif

esp_err_t measurement_client_send(const char *gateway_hw_id,
                                  const NRF24_Data_t *data)
{
    if (gateway_hw_id == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char time_buf[32];
    esp_err_t err = sntp_sync_get_iso8601(time_buf, sizeof(time_buf));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "time not ready: %d", err);
        return err;
    }

    char body[512];
    int body_len = snprintf(body, sizeof(body),
        "{\"GatewayHardwareId\":\"%s\","
        "\"WeatherStationHardwareId\":\"%08lx\","
        "\"Time\":\"%s\","
        "\"Temperature\":%.2f,"
        "\"Humidity\":%.2f,"
        "\"Pressure\":%.2f,"
        "\"VoltageLevel\":%.3f,"
        "\"ErrorMask\":%u}",
        gateway_hw_id,
        (unsigned long)data->uid,
        time_buf,
        data->temperature,
        data->humidity,
        data->pressure,
        (double)data->voltage_level / 1000.0,
        (unsigned)data->errors);

    if (body_len < 0 || body_len >= (int)sizeof(body)) {
        ESP_LOGE(TAG, "body too long");
        return ESP_ERR_INVALID_SIZE;
    }

    esp_http_client_config_t cfg = {
        .url        = MEASUREMENTS_URL,
        .method     = HTTP_METHOD_POST,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, body_len);

    err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "perform failed: %d", err);
        esp_http_client_cleanup(client);
        return err;
    }

    int status = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "POST -> HTTP %d (uid=%08lx t=%s)",
             status, (unsigned long)data->uid, time_buf);
    esp_http_client_cleanup(client);

    if (status < 200 || status >= 300) {
        return ESP_FAIL;
    }
    return ESP_OK;
}