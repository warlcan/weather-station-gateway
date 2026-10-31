#include "sntp_sync.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_log.h"
#include "esp_netif_sntp.h"

static const char *TAG = "SNTP";

#ifndef SNTP_SERVER
#define SNTP_SERVER "pool.ntp.org"
#endif

/* Порог "время явно синхронизировано" — 1 Jan 2025 00:00:00 UTC */
#define SNTP_SYNC_THRESHOLD 1735689600

esp_err_t sntp_sync_init_and_wait(TickType_t timeout)
{
    if (!sntp_sync_is_synced()) {
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(SNTP_SERVER);
        esp_err_t err = esp_netif_sntp_init(&config);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "esp_netif_sntp_init failed: %d", err);
            return err;
        }
    }

    ESP_LOGI(TAG, "Waiting for SNTP sync from %s...", SNTP_SERVER);
    esp_err_t err = esp_netif_sntp_sync_wait(timeout);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SNTP sync timeout: %d", err);
        return err;
    }

    char buf[32];
    if (sntp_sync_get_iso8601(buf, sizeof(buf)) == ESP_OK) {
        ESP_LOGI(TAG, "SNTP synced, UTC now: %s", buf);
    }
    return ESP_OK;
}

bool sntp_sync_is_synced(void)
{
    time_t now = 0;
    time(&now);
    return now > (time_t)SNTP_SYNC_THRESHOLD;
}

esp_err_t sntp_sync_get_iso8601(char *out, size_t out_len)
{
    if (out == NULL || out_len < 21) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!sntp_sync_is_synced()) {
        return ESP_ERR_INVALID_STATE;
    }

    time_t now;
    struct tm tm_utc;
    time(&now);
    gmtime_r(&now, &tm_utc);

    if (strftime(out, out_len, "%Y-%m-%dT%H:%M:%SZ", &tm_utc) == 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}