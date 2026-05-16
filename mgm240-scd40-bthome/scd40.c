#include "scd40.h"
#include "sl_i2cspm.h"
#include "sl_i2cspm_instances.h"  // declares extern sl_i2cspm_t *sl_i2cspm_sensor
#include "sl_sleeptimer.h"
#include "app_log.h"
#include <string.h>

// SCD40 2-byte commands (MSB first)
#define CMD_START_PERIODIC_MEAS   0x21B1u
#define CMD_READ_MEAS             0xEC05u
#define CMD_STOP_PERIODIC_MEAS    0x3F86u
#define CMD_GET_DATA_READY        0xE4B8u

// SCD40 CRC-8, poly=0x31, init=0xFF
static uint8_t crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            crc = (crc & 0x80u) ? ((crc << 1u) ^ 0x31u) : (crc << 1u);
        }
    }
    return crc;
}

static sl_status_t write_cmd(uint16_t cmd)
{
    uint8_t buf[2] = { (uint8_t)(cmd >> 8u), (uint8_t)(cmd & 0xFFu) };
    I2C_TransferSeq_TypeDef seq;
    memset(&seq, 0, sizeof(seq));
    seq.addr        = (uint16_t)(SCD40_I2C_ADDR << 1u);
    seq.flags       = I2C_FLAG_WRITE;
    seq.buf[0].data = buf;
    seq.buf[0].len  = sizeof(buf);
    return (sl_i2cspm_transfer(sl_i2cspm_sensor, &seq) == i2cTransferDone)
           ? SL_STATUS_OK : SL_STATUS_IO;
}

static sl_status_t read_bytes(uint8_t *buf, uint8_t len)
{
    I2C_TransferSeq_TypeDef seq;
    memset(&seq, 0, sizeof(seq));
    seq.addr        = (uint16_t)(SCD40_I2C_ADDR << 1u);
    seq.flags       = I2C_FLAG_READ;
    seq.buf[0].data = buf;
    seq.buf[0].len  = len;
    return (sl_i2cspm_transfer(sl_i2cspm_sensor, &seq) == i2cTransferDone)
           ? SL_STATUS_OK : SL_STATUS_IO;
}

sl_status_t scd40_init(void)
{
    // Ensure sensor is idle before starting (safe even if already stopped)
    return scd40_stop_periodic_measurement();
}

sl_status_t scd40_start_periodic_measurement(void)
{
    return write_cmd(CMD_START_PERIODIC_MEAS);
}

sl_status_t scd40_stop_periodic_measurement(void)
{
    sl_status_t sc = write_cmd(CMD_STOP_PERIODIC_MEAS);
    sl_sleeptimer_delay_millisecond(500);  // SCD40 requires 500 ms to stop
    return sc;
}

bool scd40_data_ready(void)
{
    if (write_cmd(CMD_GET_DATA_READY) != SL_STATUS_OK) {
        return false;
    }
    sl_sleeptimer_delay_millisecond(1);

    uint8_t buf[3];
    if (read_bytes(buf, sizeof(buf)) != SL_STATUS_OK) {
        return false;
    }
    if (crc8(buf, 2) != buf[2]) {
        return false;
    }
    // Lower 11 bits != 0 means a fresh measurement is available
    uint16_t word = (uint16_t)((uint16_t)buf[0] << 8u) | buf[1];
    return (word & 0x07FFu) != 0u;
}

sl_status_t scd40_read_measurement(scd40_data_t *data)
{
    sl_status_t sc = write_cmd(CMD_READ_MEAS);
    if (sc != SL_STATUS_OK) {
        return sc;
    }
    sl_sleeptimer_delay_millisecond(1);

    // Response layout: [CO2_H][CO2_L][CRC] [T_H][T_L][CRC] [RH_H][RH_L][CRC]
    uint8_t buf[9];
    sc = read_bytes(buf, sizeof(buf));
    if (sc != SL_STATUS_OK) {
        return sc;
    }

    if (crc8(&buf[0], 2) != buf[2]
        || crc8(&buf[3], 2) != buf[5]
        || crc8(&buf[6], 2) != buf[8]) {
        app_log_warning("SCD40: CRC error\n");
        return SL_STATUS_FAIL;
    }

    uint16_t co2_raw  = (uint16_t)((uint16_t)buf[0] << 8u) | buf[1];
    uint16_t temp_raw = (uint16_t)((uint16_t)buf[3] << 8u) | buf[4];
    uint16_t hum_raw  = (uint16_t)((uint16_t)buf[6] << 8u) | buf[7];

    data->co2_ppm = co2_raw;

    // T[°C] = -45 + 175 × raw/65535  →  [0.01 °C]: -4500 + 17500×raw/65535
    data->temperature_cdegc = (int16_t)((int32_t)(-4500)
                              + (int32_t)((uint32_t)17500u * temp_raw / 65535u));

    // RH[%] = 100 × raw/65535  →  [0.01 %]: 10000×raw/65535
    data->humidity_cpc = (uint16_t)((uint32_t)10000u * hum_raw / 65535u);

    return SL_STATUS_OK;
}
