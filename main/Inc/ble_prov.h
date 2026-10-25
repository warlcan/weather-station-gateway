#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ble_prov_init(void);
esp_err_t ble_prov_start_advertising(void);
esp_err_t ble_prov_stop(void);

#ifdef __cplusplus
}
#endif