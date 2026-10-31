#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wifi_sta_init(void);
esp_err_t wifi_sta_connect(const char *ssid, const char *pass);
esp_err_t wifi_sta_wait_connected(TickType_t timeout);
bool wifi_sta_is_connected(void);

#ifdef __cplusplus
}
#endif