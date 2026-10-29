#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t list_update_client_send(const char *gateway_hw_id);

#ifdef __cplusplus
}
#endif