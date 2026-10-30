#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t provision_client_register(const char *hardware_id,
                                    const uint8_t *token,
                                    size_t token_len);

#ifdef __cplusplus
}
#endif