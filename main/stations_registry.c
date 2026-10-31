#include "stations_registry.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "STATIONS";
static const char *NVS_NAMESPACE = "stations";
static const char *NVS_KEY_UIDS  = "uids";

static uint32_t          s_stations[STATIONS_MAX_COUNT];
static size_t            s_count = 0;
static SemaphoreHandle_t s_mutex = NULL;

static void load_from_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        return;
    }

    size_t len = sizeof(s_stations);
    if (nvs_get_blob(h, NVS_KEY_UIDS, s_stations, &len) == ESP_OK) {
        s_count = len / sizeof(uint32_t);
        if (s_count > STATIONS_MAX_COUNT) {
            s_count = STATIONS_MAX_COUNT;
        }
    }
    nvs_close(h);

    ESP_LOGI(TAG, "Loaded %u station(s) from NVS", (unsigned)s_count);
}

static void save_to_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed for save");
        return;
    }
    nvs_set_blob(h, NVS_KEY_UIDS, s_stations, s_count * sizeof(uint32_t));
    nvs_commit(h);
    nvs_close(h);
}

esp_err_t stations_registry_init(void)
{
    if (s_mutex != NULL) {
        return ESP_OK;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    load_from_nvs();
    return ESP_OK;
}

static int find_station(uint32_t uid)
{
    for (size_t i = 0; i < s_count; i++) {
        if (s_stations[i] == uid) {
            return (int)i;
        }
    }
    return -1;
}

bool stations_registry_add_if_missing(uint32_t uid)
{
    if (s_mutex == NULL) {
        return false;
    }
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    bool added = false;

    if (find_station(uid) < 0) {
        if (s_count < STATIONS_MAX_COUNT) {
            s_stations[s_count++] = uid;
            save_to_nvs();
            added = true;
            ESP_LOGI(TAG, "New station: %08lx (total=%u)",
                     (unsigned long)uid, (unsigned)s_count);
        } else {
            ESP_LOGW(TAG, "Registry full, cannot add %08lx",
                     (unsigned long)uid);
        }
    }

    xSemaphoreGive(s_mutex);
    return added;
}

bool stations_registry_has(uint32_t uid)
{
    if (s_mutex == NULL) {
        return false;
    }
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    bool has = find_station(uid) >= 0;
    xSemaphoreGive(s_mutex);
    return has;
}

size_t stations_registry_count(void)
{
    if (s_mutex == NULL) {
        return 0;
    }
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return 0;
    }
    size_t n = s_count;
    xSemaphoreGive(s_mutex);
    return n;
}

size_t stations_registry_get_all(char out[][STATION_ID_STR_LEN], size_t max_count)
{
    if (s_mutex == NULL || out == NULL || max_count == 0) {
        return 0;
    }
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return 0;
    }

    size_t n = (s_count < max_count) ? s_count : max_count;
    for (size_t i = 0; i < n; i++) {
        snprintf(out[i], STATION_ID_STR_LEN, "%08lx", (unsigned long)s_stations[i]);
    }

    xSemaphoreGive(s_mutex);
    return n;
}

void stations_registry_clear(void)
{
    if (s_mutex == NULL) {
        return;
    }
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    s_count = 0;
    save_to_nvs();
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Cleared");
}