#include "wifi_sta.h"

#include <string.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/event_groups.h"

static const char *TAG = "WIFI_STA";

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

static EventGroupHandle_t s_wifi_events = NULL;
static esp_netif_t *s_netif = NULL;
static bool s_initialized = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *ev = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW(TAG, "Disconnected, reason=%d, retrying...", ev->reason);
        xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&ev->ip_info.ip));
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_sta_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    if (s_wifi_events == NULL) {
        s_wifi_events = xEventGroupCreate();
        if (s_wifi_events == NULL) {
            ESP_LOGE(TAG, "Failed to create event group");
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed: %d", err);
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_loop_create_default failed: %d", err);
        return err;
    }

    if (s_netif == NULL) {
        s_netif = esp_netif_create_default_wifi_sta();
        if (s_netif == NULL) {
            ESP_LOGE(TAG, "esp_netif_create_default_wifi_sta failed");
            return ESP_FAIL;
        }
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %d", err);
        return err;
    }

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              &wifi_event_handler, NULL, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register WIFI_EVENT failed: %d", err);
        return err;
    }

    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              &wifi_event_handler, NULL, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register IP_EVENT failed: %d", err);
        return err;
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed: %d", err);
        return err;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Initialized");
    return ESP_OK;
}

esp_err_t wifi_sta_connect(const char *ssid, const char *pass)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ssid == NULL || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t wifi_config = {0};

    size_t ssid_len = strnlen(ssid, sizeof(wifi_config.sta.ssid));
    if (ssid_len == 0 || ssid_len >= sizeof(wifi_config.sta.ssid)) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(wifi_config.sta.ssid, ssid, ssid_len);

    if (pass != NULL && pass[0] != '\0') {
        size_t pass_len = strnlen(pass, sizeof(wifi_config.sta.password));
        if (pass_len >= sizeof(wifi_config.sta.password)) {
            return ESP_ERR_INVALID_ARG;
        }
        memcpy(wifi_config.sta.password, pass, pass_len);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }

    wifi_config.sta.pmf_cfg.capable  = true;
    wifi_config.sta.pmf_cfg.required = false;

    xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config failed: %d", err);
        return err;
    }

    err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %d", err);
        return err;
    }

    ESP_LOGI(TAG, "Connecting to \"%s\"...", ssid);
    return ESP_OK;
}

esp_err_t wifi_sta_wait_connected(TickType_t timeout)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_events,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        timeout);

    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    }
    if (bits & WIFI_FAIL_BIT) {
        return ESP_FAIL;
    }
    return ESP_ERR_TIMEOUT;
}

bool wifi_sta_is_connected(void)
{
    if (s_wifi_events == NULL) {
        return false;
    }
    EventBits_t bits = xEventGroupGetBits(s_wifi_events);
    return (bits & WIFI_CONNECTED_BIT) != 0;
}