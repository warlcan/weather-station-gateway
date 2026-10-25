#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "credentials.h"
#include "ble_prov.h"
#include "wifi_sta.h"
#include "gateway_id.h"
#include "provision_client.h"
#include "nrf24_receiver.h"
#include "stations_registry.h"
#include "list_update_client.h"
#include "sntp_sync.h"
#include "measurement_client.h"

static const char *TAG = "APP_MAIN";

#define WIFI_CONNECT_TIMEOUT_TICKS  pdMS_TO_TICKS(30000)

static esp_err_t nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_init());

    char hw_id[GATEWAY_ID_STR_LEN];
    ESP_ERROR_CHECK(gateway_id_get(hw_id));

    ESP_ERROR_CHECK(credentials_init());
    ESP_ERROR_CHECK(ble_prov_init());

    ESP_LOGI(TAG, "Waiting for BLE provisioning...");

    esp_err_t err = credentials_wait_complete(portMAX_DELAY);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "credentials_wait_complete failed: %d", err);
        return;
    }

    prov_credentials_t creds;
    err = credentials_get(&creds);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "credentials_get failed: %d", err);
        return;
    }

    ESP_LOGI(TAG, "Provisioning complete for SSID \"%s\"", creds.ssid);

    ble_prov_stop();

    ESP_ERROR_CHECK(wifi_sta_init());

    err = wifi_sta_connect(creds.ssid, creds.pass);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "wifi_sta_connect failed: %d", err);
        goto cleanup;
    }

    err = wifi_sta_wait_connected(WIFI_CONNECT_TIMEOUT_TICKS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi connection failed/timed out: %d", err);
        goto cleanup;
    }

    ESP_LOGI(TAG, "Wi-Fi connected, registering on server...");

    err = provision_client_register(hw_id, creds.token, PROV_TOKEN_LEN);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "provision_client_register failed: %d", err);
        goto cleanup;
    }

    ESP_LOGI(TAG, "Device registered, hardware_id=%s", hw_id);

    ESP_ERROR_CHECK(stations_registry_init());

    if (stations_registry_count() > 0) {
        ESP_LOGI(TAG, "Syncing %u known station(s) to server",
                 (unsigned)stations_registry_count());
        list_update_client_send(hw_id);
    }

    ESP_LOGI(TAG, "Syncing time via SNTP...");
    err = sntp_sync_init_and_wait(pdMS_TO_TICKS(15000));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SNTP sync failed, continuing: %d", err);
    }

    ESP_ERROR_CHECK(nrf24_receiver_init());

        NRF24_Data_t last_packet = {0};

        while (1) {
        if (nrf24_receiver_poll(&last_packet)) {
            if (last_packet.uid == 0) {
                ESP_LOGW(TAG, "Ignoring packet with uid=0");
            } else {
                ESP_LOGI(TAG,
                         "NRF24 packet: uid=%08lx, t=%.2f, h=%.2f, p=%.2f, err=%u, v=%lu",
                         (unsigned long)last_packet.uid,
                         last_packet.temperature,
                         last_packet.humidity,
                         last_packet.pressure,
                         last_packet.errors,
                         (unsigned long)last_packet.voltage_level);

                bool can_send = true;

                if (stations_registry_add_if_missing(last_packet.uid)) {
                    esp_err_t e = list_update_client_send(hw_id);
                    if (e != ESP_OK) {
                        ESP_LOGE(TAG, "list_update_client_send failed: %d", e);
                        can_send = false;
                    }
                }

                if (can_send) {
                    esp_err_t e = measurement_client_send(hw_id, &last_packet);
                    if (e != ESP_OK) {
                        ESP_LOGE(TAG, "measurement_client_send failed: %d", e);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
cleanup:
    memset(creds.pass, 0, sizeof(creds.pass));
    memset(creds.token, 0, sizeof(creds.token));
}