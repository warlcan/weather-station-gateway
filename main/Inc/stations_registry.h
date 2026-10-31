#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STATIONS_MAX_COUNT  16
#define STATION_ID_STR_LEN  9   /* "%08lx" + \0 */

esp_err_t stations_registry_init(void);

/* true - только что добавлена, false - уже была (или ошибка) */
bool stations_registry_add_if_missing(uint32_t uid);

bool   stations_registry_has(uint32_t uid);
size_t stations_registry_count(void);

/* Копирует отформатированные ID в out[][STATION_ID_STR_LEN], возвращает кол-во */
size_t stations_registry_get_all(char out[][STATION_ID_STR_LEN], size_t max_count);

void stations_registry_clear(void);

#ifdef __cplusplus
}
#endif