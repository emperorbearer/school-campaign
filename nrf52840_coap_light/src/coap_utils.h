#ifndef COAP_UTILS_H_
#define COAP_UTILS_H_

#include <zephyr/net/coap.h>
#include <zephyr/net/net_ip.h>

/* CoAP 리소스 경로 */
#define COAP_LIGHT_RESOURCE   "light"
#define COAP_URI_PATH_MAX     32

/* 라이트 명령 */
typedef enum {
    LIGHT_CMD_ON  = 1,
    LIGHT_CMD_OFF = 0,
    LIGHT_CMD_TOGGLE = 2,
} light_cmd_t;

/**
 * @brief CoAP 서버/클라이언트 초기화
 * @return 0 성공, 음수 오류
 */
int coap_utils_init(void);

/**
 * @brief CoAP 라이트 PUT 요청 전송
 *
 * @param peer_addr  대상 서버 IPv6 주소 (문자열, NULL이면 멀티캐스트 ff03::1)
 * @param cmd        LIGHT_CMD_ON / OFF / TOGGLE
 * @return 0 성공, 음수 오류
 */
int coap_utils_light_request(const char *peer_addr, light_cmd_t cmd);

/**
 * @brief 수신 CoAP 패킷 처리 (rx thread에서 호출)
 */
void coap_utils_process_rx(void);

#endif /* COAP_UTILS_H_ */
