#include "gateway_id.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_random.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "GATEWAY_ID";
static const char *NVS_NAMESPACE = "gw";
static const char *NVS_KEY_ID    = "hardware_id";

static void uuid_v4_generate(uint8_t out[16])
{
    for (int i = 0; i < 16; i += 4) {
        uint32_t r = esp_random();
        memcpy(out + i, &r, 4);
    }
    out[6] = (uint8_t)((out[6] & 0x0F) | 0x40);
    out[8] = (uint8_t)((out[8] & 0x3F) | 0x80);
}

static void uuid_to_str(const uint8_t u[16], char out[GATEWAY_ID_STR_LEN])
{
    snprintf(out, GATEWAY_ID_STR_LEN,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             u[0],  u[1],  u[2],  u[3],
             u[4],  u[5],  u[6],  u[7],
             u[8],  u[9],  u[10], u[11],
             u[12], u[13], u[14], u[15]);
}

esp_err_t gateway_id_get(char out[GATEWAY_ID_STR_LEN])
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %d", err);
        return err;
    }

    size_t len = GATEWAY_ID_STR_LEN;
    err = nvs_get_str(nvs, NVS_KEY_ID, out, &len);
    if (err == ESP_OK) {
        nvs_close(nvs);
        ESP_LOGI(TAG, "Loaded hardware id: %s", out);
        return ESP_OK;
    }

    if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "nvs_get_str failed: %d, regenerating", err);
    }

    uint8_t uuid[16];
    uuid_v4_generate(uuid);
    uuid_to_str(uuid, out);

    err = nvs_set_str(nvs, NVS_KEY_ID, out);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to persist hardware id: %d", err);
        return err;
    }

    ESP_LOGI(TAG, "Generated hardware id: %s", out);
    return ESP_OK;
}