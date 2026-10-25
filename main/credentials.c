#include "credentials.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

static const char *TAG = "CREDENTIALS";

#define CRED_SSID_BIT   BIT0
#define CRED_PASS_BIT   BIT1
#define CRED_TOKEN_BIT  BIT2
#define CRED_ALL_BITS   (CRED_SSID_BIT | CRED_PASS_BIT | CRED_TOKEN_BIT)

static prov_credentials_t   s_creds;
static SemaphoreHandle_t    s_mutex  = NULL;
static EventGroupHandle_t   s_events = NULL;

esp_err_t credentials_init(void)
{
    if (s_mutex != NULL && s_events != NULL) {
        return ESP_OK;
    }

    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    if (s_events == NULL) {
        s_events = xEventGroupCreate();
        if (s_events == NULL) {
            ESP_LOGE(TAG, "Failed to create event group");
            vSemaphoreDelete(s_mutex);
            s_mutex = NULL;
            return ESP_ERR_NO_MEM;
        }
    }

    memset(&s_creds, 0, sizeof(s_creds));
    ESP_LOGI(TAG, "Initialized");
    return ESP_OK;
}

esp_err_t credentials_set_ssid(const uint8_t *data, size_t len)
{
    if (s_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (data == NULL || len == 0 || len > PROV_SSID_MAX_LEN) {
        ESP_LOGW(TAG, "Invalid SSID len=%u (expected 1..%d)",
                 (unsigned)len, PROV_SSID_MAX_LEN);
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    memcpy(s_creds.ssid, data, len);
    s_creds.ssid[len] = '\0';
    s_creds.ssid_ok   = true;

    char log_ssid[PROV_SSID_MAX_LEN + 1];
    memcpy(log_ssid, s_creds.ssid, sizeof(log_ssid));

    xSemaphoreGive(s_mutex);
    xEventGroupSetBits(s_events, CRED_SSID_BIT);

    ESP_LOGI(TAG, "SSID stored: \"%s\"", log_ssid);
    return ESP_OK;
}

esp_err_t credentials_set_pass(const uint8_t *data, size_t len)
{
    if (s_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (len > 0 && data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (len > PROV_PASS_MAX_LEN) {
        ESP_LOGW(TAG, "Invalid PASS len=%u (max %d)",
                 (unsigned)len, PROV_PASS_MAX_LEN);
        return ESP_ERR_INVALID_SIZE;
    }

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (len > 0) {
        memcpy(s_creds.pass, data, len);
    }
    s_creds.pass[len] = '\0';
    s_creds.pass_ok   = true;

    xSemaphoreGive(s_mutex);
    xEventGroupSetBits(s_events, CRED_PASS_BIT);

    /* Пароль в лог не пишем — только длину */
    ESP_LOGI(TAG, "Password stored (len=%u)", (unsigned)len);
    return ESP_OK;
}

esp_err_t credentials_set_token(const uint8_t *data, size_t len)
{
    if (s_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (data == NULL || len != PROV_TOKEN_LEN) {
        ESP_LOGW(TAG, "Invalid token len=%u (expected %d)",
                 (unsigned)len, PROV_TOKEN_LEN);
        return ESP_ERR_INVALID_SIZE;
    }

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    memcpy(s_creds.token, data, PROV_TOKEN_LEN);
    s_creds.token_ok = true;

    xSemaphoreGive(s_mutex);
    xEventGroupSetBits(s_events, CRED_TOKEN_BIT);

    ESP_LOGI(TAG, "Token stored (%d bytes)", PROV_TOKEN_LEN);
    return ESP_OK;
}

bool credentials_is_complete(void)
{
    if (s_events == NULL) {
        return false;
    }
    EventBits_t bits = xEventGroupGetBits(s_events);
    return (bits & CRED_ALL_BITS) == CRED_ALL_BITS;
}

esp_err_t credentials_wait_complete(TickType_t timeout)
{
    if (s_events == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    EventBits_t bits = xEventGroupWaitBits(
        s_events,
        CRED_ALL_BITS,
        pdFALSE,
        pdTRUE,
        timeout);

    return ((bits & CRED_ALL_BITS) == CRED_ALL_BITS)
               ? ESP_OK
               : ESP_ERR_TIMEOUT;
}

esp_err_t credentials_get(prov_credentials_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    memcpy(out, &s_creds, sizeof(prov_credentials_t));

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

void credentials_reset(void)
{
    if (s_mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        memset(&s_creds, 0, sizeof(s_creds));
        xSemaphoreGive(s_mutex);
    }

    if (s_events != NULL) {
        xEventGroupClearBits(s_events, CRED_ALL_BITS);
    }

    ESP_LOGI(TAG, "Reset");
}