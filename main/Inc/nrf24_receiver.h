#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct __attribute__((packed)) {
    uint32_t uid;
    float    temperature;
    float    humidity;
    float    pressure;
    uint8_t  errors;
    uint32_t voltage_level;
} NRF24_Data_t;

esp_err_t nrf24_receiver_init(void);
bool      nrf24_receiver_poll(NRF24_Data_t *out);