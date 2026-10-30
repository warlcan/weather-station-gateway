#include "nrf24_receiver.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mirf.h"

static const char *TAG = "NRF24_RX";

#define NRF24_CHANNEL        100U
#define NRF24_PAYLOAD_SIZE   ((uint8_t)sizeof(NRF24_Data_t))
#define NRF24_SPEED_250KBPS  2

static NRF24_t s_dev;
static bool    s_initialized = false;

static const uint8_t s_rx_addr[5] = {0x9C, 0x96, 0xF1, 0x1F, 0x5E};

esp_err_t nrf24_receiver_init(void)
{
    if (s_initialized) return ESP_OK;

    Nrf24_init(&s_dev);
    Nrf24_config(&s_dev, NRF24_CHANNEL, NRF24_PAYLOAD_SIZE);

    // ВАЖНО: погасить CE перед записью регистров, иначе чип их игнорирует
    Nrf24_ceLow(&s_dev);

    Nrf24_SetSpeedDataRates(&s_dev, NRF24_SPEED_250KBPS);
    Nrf24_setRADDR(&s_dev, (uint8_t *)s_rx_addr);

    // Приводим EN_AA и SETUP_RETR к тем же значениям, что на STM32
    Nrf24_configRegister(&s_dev, EN_AA,       0x00);
    Nrf24_configRegister(&s_dev, SETUP_RETR,  0x00);

    // Сброс флагов и RX FIFO перед стартом
    Nrf24_configRegister(&s_dev, STATUS, 0x70);
    Nrf24_flushRx(&s_dev);

    Nrf24_ceHi(&s_dev);

    s_initialized = true;
    ESP_LOGI(TAG, "NRF24 receiver ready: ch=%u, payload=%u",
             NRF24_CHANNEL, NRF24_PAYLOAD_SIZE);
    return ESP_OK;
}

bool nrf24_receiver_poll(NRF24_Data_t *out)
{
    if (!s_initialized || out == NULL) {
        return false;
    }

    if (!Nrf24_dataReady(&s_dev)) {
        return false;
    }

    uint8_t buf[32];
    Nrf24_getData(&s_dev, buf);
    memcpy(out, buf, sizeof(NRF24_Data_t));
    return true;
}