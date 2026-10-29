#include "list_update_client.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_http_client.h"

#include "stations_registry.h"

static const char *TAG = "LIST_UPDATE";

#ifndef LIST_UPDATE_URL
#define LIST_UPDATE_URL "http://192.168.1.139:8080/api/gateway/list/update"
#endif

esp_err_t list_update_client_send(const char *gateway_hw_id)
{
    if (gateway_hw_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char ids[STATIONS_MAX_COUNT][STATION_ID_STR_LEN];
    size_t count = stations_registry_get_all(ids, STATIONS_MAX_COUNT);
    if (count == 0) {
        ESP_LOGW(TAG, "No stations to send");
        return ESP_ERR_INVALID_STATE;
    }

    char body[512];
    int w = 0;
    int n;

    n = snprintf(body + w, sizeof(body) - w,
                 "{\"GatewayHardwareId\":\"%s\",\"MetheostationHardwareIds\":[",
                 gateway_hw_id);
    if (n < 0 || (size_t)(w + n) >= sizeof(body)) {
        ESP_LOGE(TAG, "body overflow (header)");
        return ESP_ERR_INVALID_SIZE;
    }
    w += n;

    for (size_t i = 0; i < count; i++) {
        n = snprintf(body + w, sizeof(body) - w,
                     "%s\"%s\"", (i == 0) ? "" : ",", ids[i]);
        if (n < 0 || (size_t)(w + n) >= sizeof(body)) {
            ESP_LOGE(TAG, "body overflow (station %u)", (unsigned)i);
            return ESP_ERR_INVALID_SIZE;
        }
        w += n;
    }

    n = snprintf(body + w, sizeof(body) - w, "]}");
    if (n < 0 || (size_t)(w + n) >= sizeof(body)) {
        ESP_LOGE(TAG, "body overflow (footer)");
        return ESP_ERR_INVALID_SIZE;
    }
    w += n;

    ESP_LOGI(TAG, "POST %s -> %s", LIST_UPDATE_URL, body);

    esp_http_client_config_t cfg = {
        .url        = LIST_UPDATE_URL,
        .method     = HTTP_METHOD_POST,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, w);

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "perform failed: %d", err);
        esp_http_client_cleanup(client);
        return err;
    }

    int status = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP %d, %u station(s)", status, (unsigned)count);
    esp_http_client_cleanup(client);

    if (status < 200 || status >= 300) {
        return ESP_FAIL;
    }
    return ESP_OK;
}