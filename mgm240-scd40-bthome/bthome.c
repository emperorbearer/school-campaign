#include "bthome.h"

void bthome_build_advertisement(uint8_t *buf, uint8_t *len,
                                uint16_t co2_ppm,
                                int16_t  temp_cdegc,
                                uint16_t hum_cpc)
{
    uint8_t *p = buf;

    // AD: Flags (LE General Discoverable | BR/EDR Not Supported)
    *p++ = 0x02;
    *p++ = 0x01;  // AD type: Flags
    *p++ = 0x06;

    // BTHome service data payload:
    //   1 byte  device info
    //   3 bytes temperature (obj_id + sint16 LE)
    //   3 bytes humidity    (obj_id + uint16 LE)
    //   3 bytes CO2         (obj_id + uint16 LE)
    const uint8_t payload_len = 1u + 3u + 3u + 3u;  // = 10

    // AD: Service Data – 16-bit UUID
    *p++ = (uint8_t)(1u + 2u + payload_len);             // AD length = type+uuid+payload
    *p++ = 0x16;                                          // AD type: Service Data
    *p++ = (uint8_t)(BTHOME_SERVICE_UUID & 0xFFu);       // UUID LSB 0xD2
    *p++ = (uint8_t)(BTHOME_SERVICE_UUID >> 8u);         // UUID MSB 0xFC

    *p++ = BTHOME_DEVICE_INFO;

    // Temperature – sint16 little-endian, factor 0.01 °C
    *p++ = BTHOME_OBJ_TEMPERATURE;
    *p++ = (uint8_t)((uint16_t)temp_cdegc & 0xFFu);
    *p++ = (uint8_t)((uint16_t)temp_cdegc >> 8u);

    // Humidity – uint16 little-endian, factor 0.01 %
    *p++ = BTHOME_OBJ_HUMIDITY;
    *p++ = (uint8_t)(hum_cpc & 0xFFu);
    *p++ = (uint8_t)(hum_cpc >> 8u);

    // CO2 – uint16 little-endian, factor 1 ppm
    *p++ = BTHOME_OBJ_CO2;
    *p++ = (uint8_t)(co2_ppm & 0xFFu);
    *p++ = (uint8_t)(co2_ppm >> 8u);

    *len = (uint8_t)(p - buf);  // = 17 bytes
}
