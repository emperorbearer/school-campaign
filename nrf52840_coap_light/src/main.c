#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/openthread.h>
#include <openthread/thread.h>
#include <openthread/dataset_ftd.h>

#include <dk_buttons_and_leds.h>

#include "coap_utils.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* ── Thread 네트워크 자격 증명 (실제 배포 시 변경) ──────────────────────────
 * 동일 네트워크에 속한 CoAP 서버와 같은 값을 사용해야 합니다.
 * openthread CLI → dataset init new / dataset commit active 로 생성 가능.
 * ─────────────────────────────────────────────────────────────────────────── */
#define OT_CHANNEL       15
#define OT_PANID         0xABCD
#define OT_NETWORK_NAME  "CoAPLight"
/* 16-byte master key (hex) */
static const uint8_t OT_NETWORK_KEY[OT_NETWORK_KEY_SIZE] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
};

/* ── CoAP 서버 주소 ─────────────────────────────────────────────────────────
 * NULL → 멀티캐스트(ff03::1)로 브로드캐스트
 * 실제 서버 주소 예: "fd12:3456:789a::1"
 * ─────────────────────────────────────────────────────────────────────────── */
#define COAP_SERVER_ADDR  NULL

/* ── 스레드 및 세마포어 ──────────────────────────────────────────────────── */
#define RX_THREAD_STACK   1536
#define RX_THREAD_PRIO    5

K_THREAD_STACK_DEFINE(rx_stack, RX_THREAD_STACK);
static struct k_thread rx_thread_data;

/* 버튼 이벤트를 rx 스레드로 전달 */
static K_SEM_DEFINE(button_sem, 0, 1);
static volatile light_cmd_t pending_cmd;

/* ── LED 상태 ────────────────────────────────────────────────────────────── */
static bool local_light_on;

/* ── Thread 네트워크 초기화 ─────────────────────────────────────────────── */
static void ot_dataset_init(otInstance *ot)
{
    otOperationalDataset ds = {};

    /* 채널 */
    ds.mChannel              = OT_CHANNEL;
    ds.mComponents.mIsChannelPresent = true;

    /* PAN ID */
    ds.mPanId                = OT_PANID;
    ds.mComponents.mIsPanIdPresent = true;

    /* 네트워크 이름 */
    strncpy(ds.mNetworkName.m8, OT_NETWORK_NAME,
            OT_NETWORK_NAME_MAX_SIZE);
    ds.mComponents.mIsNetworkNamePresent = true;

    /* 네트워크 키 */
    memcpy(ds.mNetworkKey.m8, OT_NETWORK_KEY, OT_NETWORK_KEY_SIZE);
    ds.mComponents.mIsNetworkKeyPresent = true;

    otDatasetSetActive(ot, &ds);
}

static void ot_state_changed_cb(otChangedFlags flags, void *ctx)
{
    ARG_UNUSED(ctx);
    otInstance *ot = openthread_get_default_instance();

    if (flags & OT_CHANGED_THREAD_ROLE) {
        otDeviceRole role = otThreadGetDeviceRole(ot);
        LOG_INF("Thread role: %s", otThreadDeviceRoleToString(role));

        /* 네트워크 합류 후 CoAP 초기화 */
        if (role == OT_DEVICE_ROLE_CHILD ||
            role == OT_DEVICE_ROLE_ROUTER) {
            if (coap_utils_init() != 0) {
                LOG_ERR("CoAP init failed");
            }
        }
    }
}

static void ot_init(void)
{
    otInstance *ot = openthread_get_default_instance();

    openthread_api_mutex_lock(openthread_get_default_context());

    ot_dataset_init(ot);
    otSetStateChangedCallback(ot, ot_state_changed_cb, NULL);

    /* MTD-SED: poll period 설정 (저전력 핵심) */
    otLinkSetPollPeriod(ot, CONFIG_OPENTHREAD_POLL_PERIOD);

    otIp6SetEnabled(ot, true);
    otThreadSetEnabled(ot, true);

    openthread_api_mutex_unlock(openthread_get_default_context());

    LOG_INF("OpenThread started (MTD-SED, poll=%d ms)",
            CONFIG_OPENTHREAD_POLL_PERIOD);
}

/* ── 버튼 콜백 (ISR 컨텍스트) ───────────────────────────────────────────── */
static void button_handler(uint32_t button_state, uint32_t has_changed)
{
    if (has_changed & DK_BTN1_MSK && button_state & DK_BTN1_MSK) {
        pending_cmd = LIGHT_CMD_ON;
        k_sem_give(&button_sem);
    }
    if (has_changed & DK_BTN2_MSK && button_state & DK_BTN2_MSK) {
        pending_cmd = LIGHT_CMD_OFF;
        k_sem_give(&button_sem);
    }
    if (has_changed & DK_BTN3_MSK && button_state & DK_BTN3_MSK) {
        pending_cmd = LIGHT_CMD_TOGGLE;
        k_sem_give(&button_sem);
    }
}

/* ── RX / 이벤트 처리 스레드 ────────────────────────────────────────────── */
static void rx_thread_fn(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

    while (true) {
        /*
         * 저전력 전략:
         * k_sem_take 에서 CPU 가 WFI(Wait-For-Interrupt) 슬립.
         * 버튼 인터럽트 또는 CoAP RX 타임아웃이 깨어날 때만 동작.
         */
        int ret = k_sem_take(&button_sem, K_MSEC(COAP_RX_TIMEOUT_MS));

        if (ret == 0) {
            /* 버튼 이벤트: CoAP 요청 전송 */
            light_cmd_t cmd = pending_cmd;
            LOG_INF("Button pressed → cmd=%d", cmd);
            coap_utils_light_request(COAP_SERVER_ADDR, cmd);

            /* 로컬 LED 즉시 반영 */
            if (cmd == LIGHT_CMD_ON) {
                local_light_on = true;
            } else if (cmd == LIGHT_CMD_OFF) {
                local_light_on = false;
            } else {
                local_light_on = !local_light_on;
            }
            dk_set_led(DK_LED1, local_light_on);
        }

        /* 수신 패킷 처리 (서버 역할) */
        coap_utils_process_rx();
    }
}

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    LOG_INF("=== CoAP Light Button (nRF52840, MTD-SED) ===");

    /* LED/버튼 초기화 */
    int ret = dk_buttons_and_leds_init(button_handler);
    if (ret) {
        LOG_ERR("dk_buttons_and_leds_init: %d", ret);
        return ret;
    }

    /* 시작 알림 LED */
    dk_set_led_on(DK_LED2);
    k_sleep(K_MSEC(300));
    dk_set_led_off(DK_LED2);

    /* OpenThread 시작 */
    ot_init();

    /* RX / 이벤트 스레드 시작 */
    k_thread_create(&rx_thread_data, rx_stack,
                    K_THREAD_STACK_SIZEOF(rx_stack),
                    rx_thread_fn, NULL, NULL, NULL,
                    RX_THREAD_PRIO, 0, K_NO_WAIT);
    k_thread_name_set(&rx_thread_data, "coap_rx");

    /*
     * main 스레드는 이후 사용하지 않으므로 종료.
     * Zephyr 스케줄러가 idle 스레드를 실행 → WFI → 저전력 상태 유지.
     */
    return 0;
}
