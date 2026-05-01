#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/coap.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/openthread.h>
#include <openthread/thread.h>

#include <dk_buttons_and_leds.h>
#include <string.h>

#include "coap_utils.h"

LOG_MODULE_REGISTER(coap_utils, LOG_LEVEL_INF);

/* ── 상수 ─────────────────────────────────────────────────────────────────── */
#define COAP_PORT          5683
#define COAP_BUF_SIZE      256
#define COAP_TOKEN_LEN     4
#define COAP_RX_TIMEOUT_MS 3000

/* Thread 멀티캐스트 – 모든 FTD/MTD 노드 */
#define OT_MESH_LOCAL_MCAST "ff03::1"

/* ── 내부 상태 ───────────────────────────────────────────────────────────── */
static int sock = -1;
static uint8_t coap_buf[COAP_BUF_SIZE];
static uint16_t msg_id;

/* ── 헬퍼: 소켓 열기/닫기 ────────────────────────────────────────────────── */
static int udp_socket_open(void)
{
    struct sockaddr_in6 bind_addr = {
        .sin6_family = AF_INET6,
        .sin6_port   = htons(COAP_PORT),
        .sin6_addr   = IN6ADDR_ANY_INIT,
    };

    int s = zsock_socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        LOG_ERR("socket() failed: %d", errno);
        return -errno;
    }

    if (zsock_bind(s, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        LOG_ERR("bind() failed: %d", errno);
        zsock_close(s);
        return -errno;
    }

    return s;
}

/* ── 공개 API ────────────────────────────────────────────────────────────── */

int coap_utils_init(void)
{
    sock = udp_socket_open();
    if (sock < 0) {
        return sock;
    }
    msg_id = sys_rand32_get() & 0xFFFF;
    LOG_INF("CoAP socket ready (port %d)", COAP_PORT);
    return 0;
}

int coap_utils_light_request(const char *peer_addr, light_cmd_t cmd)
{
    struct coap_packet pkt;
    struct sockaddr_in6 dst = {
        .sin6_family = AF_INET6,
        .sin6_port   = htons(COAP_PORT),
    };

    /* 대상 주소 결정 */
    const char *addr_str = peer_addr ? peer_addr : OT_MESH_LOCAL_MCAST;
    if (zsock_inet_pton(AF_INET6, addr_str, &dst.sin6_addr) != 1) {
        LOG_ERR("Invalid peer address: %s", addr_str);
        return -EINVAL;
    }

    /* CoAP 패킷 구성 */
    uint8_t token[COAP_TOKEN_LEN];
    sys_rand_get(token, sizeof(token));

    int ret = coap_packet_init(&pkt, coap_buf, sizeof(coap_buf),
                               COAP_VERSION_1,
                               peer_addr ? COAP_TYPE_CON : COAP_TYPE_NON_CON,
                               COAP_TOKEN_LEN, token,
                               COAP_METHOD_PUT,
                               msg_id++);
    if (ret < 0) {
        LOG_ERR("coap_packet_init: %d", ret);
        return ret;
    }

    /* Uri-Path 옵션 */
    ret = coap_packet_append_option(&pkt, COAP_OPTION_URI_PATH,
                                    COAP_LIGHT_RESOURCE,
                                    strlen(COAP_LIGHT_RESOURCE));
    if (ret < 0) {
        LOG_ERR("append uri-path: %d", ret);
        return ret;
    }

    /* Content-Format: text/plain */
    uint8_t fmt = 0;
    ret = coap_packet_append_option(&pkt, COAP_OPTION_CONTENT_FORMAT,
                                    &fmt, sizeof(fmt));
    if (ret < 0) {
        LOG_ERR("append content-format: %d", ret);
        return ret;
    }

    /* payload */
    ret = coap_packet_append_payload_marker(&pkt);
    if (ret < 0) { return ret; }

    uint8_t payload = (uint8_t)cmd;
    ret = coap_packet_append_payload(&pkt, &payload, sizeof(payload));
    if (ret < 0) { return ret; }

    /* 전송 */
    ret = zsock_sendto(sock, pkt.data, pkt.offset, 0,
                       (struct sockaddr *)&dst, sizeof(dst));
    if (ret < 0) {
        LOG_ERR("sendto failed: %d", errno);
        return -errno;
    }

    LOG_INF("CoAP PUT → %s  cmd=%d  (%d bytes)", addr_str, cmd, pkt.offset);
    return 0;
}

/* ── 수신 처리 (서버 역할) ───────────────────────────────────────────────── */
static int light_put_handler(struct coap_resource *resource,
                              struct coap_packet *request,
                              struct sockaddr *addr, socklen_t addr_len)
{
    uint8_t payload[8];
    uint16_t payload_len = sizeof(payload);

    const uint8_t *p = coap_packet_get_payload(request, &payload_len);
    if (!p || payload_len == 0) {
        return -EINVAL;
    }

    light_cmd_t cmd = (light_cmd_t)p[0];
    LOG_INF("Received light cmd: %d", cmd);

    switch (cmd) {
    case LIGHT_CMD_ON:
        dk_set_led_on(DK_LED1);
        break;
    case LIGHT_CMD_OFF:
        dk_set_led_off(DK_LED1);
        break;
    case LIGHT_CMD_TOGGLE:
        dk_set_led(DK_LED1, !dk_get_buttons() /* 간이 토글 */);
        break;
    default:
        return -EINVAL;
    }

    /* 2.04 Changed 응답 */
    struct coap_packet rsp;
    uint8_t rsp_buf[64];
    uint8_t tok[COAP_TOKEN_LEN];
    uint8_t tok_len = coap_header_get_token(request, tok);

    int ret = coap_ack_init(&rsp, request, rsp_buf, sizeof(rsp_buf),
                            COAP_RESPONSE_CODE_CHANGED);
    if (ret < 0) { return ret; }

    return zsock_sendto(sock, rsp.data, rsp.offset, 0, addr, addr_len);
}

static const char * const light_path[] = { COAP_LIGHT_RESOURCE, NULL };

static struct coap_resource coap_resources[] = {
    {
        .path    = light_path,
        .put     = light_put_handler,
    },
    { }   /* sentinel */
};

void coap_utils_process_rx(void)
{
    if (sock < 0) { return; }

    struct zsock_pollfd fds = { .fd = sock, .events = ZSOCK_POLLIN };
    int ready = zsock_poll(&fds, 1, COAP_RX_TIMEOUT_MS);
    if (ready <= 0) { return; }

    struct sockaddr_in6 src;
    socklen_t src_len = sizeof(src);

    int len = zsock_recvfrom(sock, coap_buf, sizeof(coap_buf), 0,
                             (struct sockaddr *)&src, &src_len);
    if (len < 0) { return; }

    struct coap_packet pkt;
    if (coap_packet_parse(&pkt, coap_buf, len, NULL, 0) < 0) {
        LOG_WRN("CoAP parse failed");
        return;
    }

    int r = coap_handle_request(&pkt, coap_resources,
                                (struct sockaddr *)&src, src_len);
    if (r < 0 && r != -ENOENT) {
        LOG_WRN("coap_handle_request: %d", r);
    }
}
