#include "app.h"
#include "sl_bluetooth.h"
#include "sl_simple_timer.h"
#include "scd40.h"
#include "bthome.h"
#include "app_log.h"
#include "app_assert.h"

// 100 ms advertising interval (units of 0.625 ms)
#define ADV_INTERVAL_UNITS   ((uint32_t)((100u * 1000u) / 625u))

// SCD40 updates every 5 s in periodic mode
#define MEASUREMENT_INTERVAL_MS   5000u

static uint8_t             adv_handle = SL_BT_INVALID_ADVERTISING_SET_HANDLE;
static sl_simple_timer_t   meas_timer;

// ---------------------------------------------------------------------------

static void update_advertisement(uint16_t co2, int16_t temp, uint16_t hum)
{
    uint8_t buf[31];
    uint8_t len;
    bthome_build_advertisement(buf, &len, co2, temp, hum);

    sl_status_t sc = sl_bt_legacy_advertiser_set_data(
                         adv_handle,
                         sl_bt_advertiser_advertising_data_packet,
                         len, buf);
    if (sc != SL_STATUS_OK) {
        app_log_warning("adv set_data failed: 0x%04lX\n", (unsigned long)sc);
    }
}

static void on_meas_timer(sl_simple_timer_t *timer, void *data)
{
    (void)timer;
    (void)data;

    if (!scd40_data_ready()) {
        app_log_debug("SCD40 not ready\n");
        return;
    }

    scd40_data_t m;
    sl_status_t sc = scd40_read_measurement(&m);
    if (sc != SL_STATUS_OK) {
        app_log_warning("SCD40 read error: 0x%04lX\n", (unsigned long)sc);
        return;
    }

    // Pretty-print temperature handling both positive and negative values
    int16_t t     = m.temperature_cdegc;
    uint16_t tabs = (uint16_t)(t < 0 ? -t : t);
    app_log_info("CO2: %u ppm | T: %s%u.%02u C | RH: %u.%02u%%\n",
                 m.co2_ppm,
                 t < 0 ? "-" : "",
                 tabs / 100u, tabs % 100u,
                 m.humidity_cpc / 100u, m.humidity_cpc % 100u);

    update_advertisement(m.co2_ppm, m.temperature_cdegc, m.humidity_cpc);
}

// ---------------------------------------------------------------------------

void app_init(void)
{
    // BT stack not yet running; real init happens in sl_bt_evt_system_boot_id
}

void app_process_action(void)
{
    // All work is timer-driven
}

void sl_bt_on_event(sl_bt_msg_t *evt)
{
    sl_status_t sc;

    switch (SL_BT_MSG_ID(evt->header)) {

        case sl_bt_evt_system_boot_id:
            app_log_info("=== MGM240 SCD40 BTHome ===\n");

            sc = sl_bt_advertiser_create_set(&adv_handle);
            app_assert_status(sc);

            sc = sl_bt_advertiser_set_timing(adv_handle,
                                             ADV_INTERVAL_UNITS,
                                             ADV_INTERVAL_UNITS,
                                             0, 0);
            app_assert_status(sc);

            // Initialise sensor (stops any in-progress measurement)
            sc = scd40_init();
            if (sc != SL_STATUS_OK) {
                app_log_error("SCD40 init: 0x%04lX\n", (unsigned long)sc);
            }
            sc = scd40_start_periodic_measurement();
            if (sc != SL_STATUS_OK) {
                app_log_error("SCD40 start: 0x%04lX\n", (unsigned long)sc);
            }

            // Advertise zeros until the first real measurement arrives (~5 s)
            update_advertisement(0, 0, 0);

            sc = sl_bt_legacy_advertiser_start(
                     adv_handle,
                     sl_bt_legacy_advertiser_non_connectable);
            app_assert_status(sc);

            // Periodic timer: read + update advertisement every 5 s
            sc = sl_simple_timer_start(&meas_timer,
                                       MEASUREMENT_INTERVAL_MS,
                                       on_meas_timer,
                                       NULL,
                                       true);
            app_assert_status(sc);

            app_log_info("Advertising. First measurement in %u ms.\n",
                         MEASUREMENT_INTERVAL_MS);
            break;

        default:
            break;
    }
}
