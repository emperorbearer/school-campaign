#ifndef BTHOME_H
#define BTHOME_H

#include <stdint.h>

// BTHome v2 service UUID (0xFCD2, little-endian on wire: 0xD2 0xFC)
#define BTHOME_SERVICE_UUID   0xFCD2U

// Device info: version=2 (bits 7-5 = 010), no encryption, no trigger
#define BTHOME_DEVICE_INFO    0x40U

// Object IDs — must appear in ascending order per BTHome v2 spec
#define BTHOME_OBJ_TEMPERATURE  0x02U  // sint16, factor=0.01, unit=°C
#define BTHOME_OBJ_HUMIDITY     0x03U  // uint16, factor=0.01, unit=%
#define BTHOME_OBJ_CO2          0x12U  // uint16, factor=1,    unit=ppm

/**
 * Build a complete BLE advertising payload in BTHome v2 format.
 *
 * Output (17 bytes):
 *   02 01 06                         — Flags AD record
 *   0D 16 D2 FC 40 02 TL TH 03 HL HH 12 CL CH  — BTHome service data
 *
 * @param buf        Output buffer, must be ≥ 31 bytes
 * @param len        Bytes written to buf
 * @param co2_ppm    CO2 concentration [ppm]
 * @param temp_cdegc Temperature [0.01 °C], e.g. 2150 = 21.50 °C
 * @param hum_cpc    Relative humidity [0.01 %],  e.g. 5000 = 50.00 %
 */
void bthome_build_advertisement(uint8_t *buf, uint8_t *len,
                                uint16_t co2_ppm,
                                int16_t  temp_cdegc,
                                uint16_t hum_cpc);

#endif // BTHOME_H
