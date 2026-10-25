#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PROV_SSID_MAX_LEN   32
#define PROV_PASS_MAX_LEN   64
#define PROV_TOKEN_LEN      32

typedef struct {
    char    ssid[PROV_SSID_MAX_LEN + 1];
    char    pass[PROV_PASS_MAX_LEN + 1];
    uint8_t token[PROV_TOKEN_LEN];
    bool    ssid_ok;
    bool    pass_ok;
    bool    token_ok;
} prov_credentials_t;

esp_err_t credentials_init(void);
esp_err_t credentials_set_ssid(const uint8_t *data, size_t len);
esp_err_t credentials_set_pass(const uint8_t *data, size_t len);
esp_err_t credentials_set_token(const uint8_t *data, size_t len);
bool credentials_is_complete(void);
esp_err_t credentials_wait_complete(TickType_t timeout);
esp_err_t credentials_get(prov_credentials_t *out);

void credentials_reset(void);

#ifdef __cplusplus
}
#endif