#include "provision_client.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_http_client.h"

static const char *TAG = "PROV_CLIENT";

#ifndef PROV_SERVER_URL
#define PROV_SERVER_URL "http://192.168.1.139:8080/api/gateway/init"
#endif

static void hex_encode(const uint8_t *in, size_t in_len, char *out)
{
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < in_len; i++) {
        out[i * 2]     = hex[in[i] >> 4];
        out[i * 2 + 1] = hex[in[i] & 0x0F];
    }
    out[in_len * 2] = '\0';
}

esp_err_t provision_client_register(const char *hardware_id,
                                    const uint8_t *token,
                                    size_t token_len)
{
    if (hardware_id == NULL || token == NULL || token_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (token_len > 32) {
        token_len = 32;
    }

    char token_hex[65];
    hex_encode(token, token_len, token_hex);

    char body[256];
    int body_len = snprintf(body, sizeof(body),
        "{\"GatewayHardwareId\":\"%s\",\"UserApiToken\":\"%s\"}",
        hardware_id, token_hex);
    if (body_len < 0 || body_len >= (int)sizeof(body)) {
        ESP_LOGE(TAG, "request body too long");
        return ESP_ERR_INVALID_SIZE;
    }

    esp_http_client_config_t cfg = {
        .url        = PROV_SERVER_URL,
        .method     = HTTP_METHOD_POST,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "esp_http_client_init failed");
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, body_len);

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "perform failed: %d", err);
        esp_http_client_cleanup(client);
        return err;
    }

    int status = esp_http_client_get_status_code(client);
    int64_t clen = esp_http_client_get_content_length(client);
    ESP_LOGI(TAG, "POST %s -> HTTP %d, content-length=%lld",
             PROV_SERVER_URL, status, (long long)clen);

    esp_http_client_cleanup(client);

    if (status < 200 || status >= 300) {
        return ESP_FAIL;
    }
    return ESP_OK;
}