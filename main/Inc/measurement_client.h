#pragma once

#include "esp_err.h"
#include "nrf24_receiver.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t measurement_client_send(const char *gateway_hw_id,
                                  const NRF24_Data_t *data);

#ifdef __cplusplus
}
#endif