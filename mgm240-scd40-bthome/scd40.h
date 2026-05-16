#ifndef SCD40_H
#define SCD40_H

#include <stdint.h>
#include <stdbool.h>
#include "sl_status.h"

#define SCD40_I2C_ADDR  0x62u  // 7-bit I2C address

typedef struct {
    uint16_t co2_ppm;           // CO2 [ppm]
    int16_t  temperature_cdegc; // Temperature [0.01 °C], e.g. 2150 = 21.50 °C
    uint16_t humidity_cpc;      // Relative humidity [0.01 %], e.g. 5000 = 50.00 %
} scd40_data_t;

sl_status_t scd40_init(void);
sl_status_t scd40_start_periodic_measurement(void);
sl_status_t scd40_stop_periodic_measurement(void);
bool        scd40_data_ready(void);
sl_status_t scd40_read_measurement(scd40_data_t *data);

#endif // SCD40_H
