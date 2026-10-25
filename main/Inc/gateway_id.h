#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GATEWAY_ID_STR_LEN 37

esp_err_t gateway_id_get(char out[GATEWAY_ID_STR_LEN]);

#ifdef __cplusplus
}
#endif