#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t sntp_sync_init_and_wait(TickType_t timeout);
bool      sntp_sync_is_synced(void);
esp_err_t sntp_sync_get_iso8601(char *out, size_t out_len);

#ifdef __cplusplus
}
#endif