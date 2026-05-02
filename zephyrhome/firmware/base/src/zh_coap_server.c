/* SPDX-License-Identifier: Apache-2.0 */
/*
 * ZephyrHome CoAP server.
 *
 * Exposes resources under /zephyrhome/:
 *   GET  /zephyrhome/info              → device info JSON
 *   GET  /zephyrhome/sensors/<name>    → sensor value JSON
 *   GET  /zephyrhome/switches/<name>   → switch state JSON
 *   PUT  /zephyrhome/switches/<name>   → set switch state
 *   POST /zephyrhome/announce          → multicast discovery beacon (sent BY device)
 *   POST /zephyrhome/ota               → trigger FOTA
 *
 * CoAP wildcard matching: Zephyr's coap_handle_request() matches the
 * longest prefix. The handler then reads the remaining URI-Path options
 * to extract the component name.
 */
#include "zh_coap_server.h"
#include "zh_component.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/coap.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/openthread.h>
#include <openthread/thread.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <version.h>

LOG_MODULE_REGISTER(zh_coap_server, LOG_LEVEL_INF);

/* ── Constants ──────────────────────────────────────────────────────────── */
#define COAP_PORT       5683
#define RX_BUF_SIZE     512
#define RSP_BUF_SIZE    256
#define RX_TIMEOUT_MS   2000
#define RX_STACK_SIZE   2048
#define RX_THREAD_PRIO  5

#define MCAST_ADDR      CONFIG_ZH_COAP_SERVER   /* default: "ff03::1" */

/* ── Internal state ─────────────────────────────────────────────────────── */
static int g_sock = -1;
static uint8_t g_rx_buf[RX_BUF_SIZE];
static uint8_t g_rsp_buf[RSP_BUF_SIZE];
static uint16_t g_msg_id;

K_THREAD_STACK_DEFINE(g_rx_stack, RX_STACK_SIZE);
static struct k_thread g_rx_thread;

/* ── Helpers ────────────────────────────────────────────────────────────── */

/* Extract the last URI-Path segment from a CoAP request (component name). */
static int get_last_path_segment(struct coap_packet *pkt,
				 char *out, size_t out_len)
{
	struct coap_option options[8];
	uint8_t count = ARRAY_SIZE(options);

	int ret = coap_find_options(pkt, COAP_OPTION_URI_PATH,
				    options, count);
	if (ret < 0 || ret == 0) {
		return -ENOENT;
	}

	/* Last option is the component name */
	struct coap_option *last = &options[ret - 1];
	size_t seg_len = MIN(last->len, out_len - 1);

	memcpy(out, last->value, seg_len);
	out[seg_len] = '\0';
	return 0;
}

static int send_response(struct coap_packet *req, uint8_t code,
			 const char *payload,
			 struct sockaddr *addr, socklen_t addr_len)
{
	struct coap_packet rsp;

	int ret = coap_ack_init(&rsp, req, g_rsp_buf, sizeof(g_rsp_buf), code);
	if (ret < 0) {
		return ret;
	}

	if (payload && *payload) {
		/* application/json content-format = 50 */
		uint8_t fmt = 50;

		ret = coap_packet_append_option(&rsp, COAP_OPTION_CONTENT_FORMAT,
						&fmt, sizeof(fmt));
		if (ret < 0) {
			return ret;
		}

		ret = coap_packet_append_payload_marker(&rsp);
		if (ret < 0) {
			return ret;
		}

		ret = coap_packet_append_payload(&rsp,
						 (const uint8_t *)payload,
						 strlen(payload));
		if (ret < 0) {
			return ret;
		}
	}

	return zsock_sendto(g_sock, rsp.data, rsp.offset, 0, addr, addr_len);
}

/* ── Resource handlers ──────────────────────────────────────────────────── */

static int info_get(struct coap_resource *res, struct coap_packet *req,
		    struct sockaddr *addr, socklen_t addr_len)
{
	ARG_UNUSED(res);

	char buf[RSP_BUF_SIZE];

	snprintf(buf, sizeof(buf),
		 "{\"name\":\"%s\",\"fn\":\"%s\",\"fw\":\"" KERNEL_VERSION_STRING "\","
		 "\"components\":%d}",
		 CONFIG_ZEPHYRHOME_DEVICE_NAME,
		 CONFIG_ZEPHYRHOME_FRIENDLY_NAME,
		 zh_component_count());

	return send_response(req, COAP_RESPONSE_CODE_CONTENT, buf, addr, addr_len);
}

static int sensor_get(struct coap_resource *res, struct coap_packet *req,
		      struct sockaddr *addr, socklen_t addr_len)
{
	ARG_UNUSED(res);

	char name[ZH_COMPONENT_NAME_MAX];

	if (get_last_path_segment(req, name, sizeof(name)) < 0) {
		return send_response(req, COAP_RESPONSE_CODE_BAD_REQUEST,
				     "{\"error\":\"missing name\"}",
				     addr, addr_len);
	}

	struct zh_component *comp = zh_component_find(name);

	if (!comp || comp->type != ZH_TYPE_SENSOR) {
		return send_response(req, COAP_RESPONSE_CODE_NOT_FOUND,
				     "{\"error\":\"not found\"}",
				     addr, addr_len);
	}

	zh_value_t val = {};
	int ret = comp->ops->read(comp, &val);

	if (ret < 0) {
		char err[64];

		snprintf(err, sizeof(err), "{\"error\":\"read failed: %d\"}", ret);
		return send_response(req, COAP_RESPONSE_CODE_INTERNAL_ERROR,
				     err, addr, addr_len);
	}

	char buf[128];

	snprintf(buf, sizeof(buf), "{\"v\":%.2f}", val.f32);
	return send_response(req, COAP_RESPONSE_CODE_CONTENT, buf, addr, addr_len);
}

static int switch_get(struct coap_resource *res, struct coap_packet *req,
		      struct sockaddr *addr, socklen_t addr_len)
{
	ARG_UNUSED(res);

	char name[ZH_COMPONENT_NAME_MAX];

	if (get_last_path_segment(req, name, sizeof(name)) < 0) {
		return send_response(req, COAP_RESPONSE_CODE_BAD_REQUEST, NULL,
				     addr, addr_len);
	}

	struct zh_component *comp = zh_component_find(name);

	if (!comp || comp->type != ZH_TYPE_SWITCH) {
		return send_response(req, COAP_RESPONSE_CODE_NOT_FOUND, NULL,
				     addr, addr_len);
	}

	zh_value_t val = {};

	comp->ops->read(comp, &val);

	char buf[32];

	snprintf(buf, sizeof(buf), "{\"state\":%s}",
		 val.boolean ? "true" : "false");
	return send_response(req, COAP_RESPONSE_CODE_CONTENT, buf, addr, addr_len);
}

static int switch_put(struct coap_resource *res, struct coap_packet *req,
		      struct sockaddr *addr, socklen_t addr_len)
{
	ARG_UNUSED(res);

	char name[ZH_COMPONENT_NAME_MAX];

	if (get_last_path_segment(req, name, sizeof(name)) < 0) {
		return send_response(req, COAP_RESPONSE_CODE_BAD_REQUEST, NULL,
				     addr, addr_len);
	}

	struct zh_component *comp = zh_component_find(name);

	if (!comp || comp->type != ZH_TYPE_SWITCH || !comp->ops->write) {
		return send_response(req, COAP_RESPONSE_CODE_NOT_FOUND, NULL,
				     addr, addr_len);
	}

	uint16_t payload_len;
	const uint8_t *payload = coap_packet_get_payload(req, &payload_len);

	if (!payload || payload_len == 0) {
		return send_response(req, COAP_RESPONSE_CODE_BAD_REQUEST, NULL,
				     addr, addr_len);
	}

	/* Accept: {"state":true}, {"state":false}, "true", "false", "1", "0" */
	char buf[64];
	size_t n = MIN(payload_len, sizeof(buf) - 1);

	memcpy(buf, payload, n);
	buf[n] = '\0';

	bool state = (strstr(buf, "true") || strstr(buf, "\"1\"") ||
		      (buf[0] == '1' && n == 1));

	zh_value_t val = { .boolean = state };
	int ret = comp->ops->write(comp, val);

	if (ret < 0) {
		return send_response(req, COAP_RESPONSE_CODE_INTERNAL_ERROR,
				     NULL, addr, addr_len);
	}

	LOG_INF("Switch '%s' → %s", name, state ? "ON" : "OFF");
	return send_response(req, COAP_RESPONSE_CODE_CHANGED, NULL, addr, addr_len);
}

static int ota_post(struct coap_resource *res, struct coap_packet *req,
		    struct sockaddr *addr, socklen_t addr_len)
{
	ARG_UNUSED(res);
	ARG_UNUSED(req);

	LOG_INF("OTA trigger received — rebooting to MCUBoot");

#if IS_ENABLED(CONFIG_BOOTLOADER_MCUBOOT)
	/* Mark update as confirmed and request upgrade on next boot */
	extern int boot_request_upgrade(int permanent);
	boot_request_upgrade(0 /* BOOT_UPGRADE_TEST */);
	k_sleep(K_MSEC(100));
	sys_reboot(SYS_REBOOT_COLD);
#endif

	return send_response(req, COAP_RESPONSE_CODE_CHANGED, NULL, addr, addr_len);
}

/* ── Resource table ─────────────────────────────────────────────────────── */

static const char *const path_info[]     = { "zephyrhome", "info",     NULL };
static const char *const path_sensors[]  = { "zephyrhome", "sensors",  NULL };
static const char *const path_switches[] = { "zephyrhome", "switches", NULL };
static const char *const path_ota[]      = { "zephyrhome", "ota",      NULL };

static struct coap_resource g_resources[] = {
	{ .path = path_info,     .get  = info_get                       },
	{ .path = path_sensors,  .get  = sensor_get                     },
	{ .path = path_switches, .get  = switch_get, .put = switch_put  },
	{ .path = path_ota,      .post = ota_post                       },
	{ /* sentinel */ },
};

/* ── Socket helpers ─────────────────────────────────────────────────────── */

static int open_socket(void)
{
	struct sockaddr_in6 addr = {
		.sin6_family = AF_INET6,
		.sin6_port   = htons(COAP_PORT),
		.sin6_addr   = IN6ADDR_ANY_INIT,
	};

	int s = zsock_socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

	if (s < 0) {
		LOG_ERR("socket() failed: %d", errno);
		return -errno;
	}

	if (zsock_bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		LOG_ERR("bind() failed: %d", errno);
		zsock_close(s);
		return -errno;
	}

	return s;
}

/* ── RX loop ────────────────────────────────────────────────────────────── */

static void rx_thread_fn(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	while (true) {
		/* Tick sensor update callbacks */
		zh_component_tick_all();

		struct zsock_pollfd fds = {
			.fd = g_sock, .events = ZSOCK_POLLIN
		};
		int ready = zsock_poll(&fds, 1, RX_TIMEOUT_MS);

		if (ready <= 0) {
			continue;
		}

		struct sockaddr_in6 src;
		socklen_t src_len = sizeof(src);

		int len = zsock_recvfrom(g_sock, g_rx_buf, sizeof(g_rx_buf),
					 0, (struct sockaddr *)&src, &src_len);
		if (len < 0) {
			continue;
		}

		struct coap_packet pkt;
		int ret = coap_packet_parse(&pkt, g_rx_buf, len, NULL, 0);

		if (ret < 0) {
			LOG_WRN("CoAP parse failed: %d", ret);
			continue;
		}

		ret = coap_handle_request(&pkt, g_resources,
					  (struct sockaddr *)&src, src_len);
		if (ret < 0 && ret != -ENOENT) {
			LOG_WRN("coap_handle_request: %d", ret);
		}
	}
}

/* ── Public API ─────────────────────────────────────────────────────────── */

int zh_coap_server_start(void)
{
	g_sock = open_socket();
	if (g_sock < 0) {
		return g_sock;
	}
	g_msg_id = sys_rand32_get() & 0xFFFF;

	k_thread_create(&g_rx_thread, g_rx_stack,
			K_THREAD_STACK_SIZEOF(g_rx_stack),
			rx_thread_fn, NULL, NULL, NULL,
			RX_THREAD_PRIO, 0, K_NO_WAIT);
	k_thread_name_set(&g_rx_thread, "zh_coap_rx");

	LOG_INF("ZephyrHome CoAP server listening on port %d", COAP_PORT);
	return 0;
}

int zh_coap_announce(void)
{
	if (g_sock < 0) {
		return -ENOTCONN;
	}

	struct coap_packet pkt;
	uint8_t buf[256];
	uint8_t token[4];

	sys_rand_get(token, sizeof(token));

	int ret = coap_packet_init(&pkt, buf, sizeof(buf),
				   COAP_VERSION_1, COAP_TYPE_NON_CON,
				   sizeof(token), token,
				   COAP_METHOD_POST, g_msg_id++);
	if (ret < 0) {
		return ret;
	}

	ret = coap_packet_append_option(&pkt, COAP_OPTION_URI_PATH,
					"zephyrhome", strlen("zephyrhome"));
	if (ret < 0) {
		return ret;
	}
	ret = coap_packet_append_option(&pkt, COAP_OPTION_URI_PATH,
					"announce", strlen("announce"));
	if (ret < 0) {
		return ret;
	}

	uint8_t fmt = 50; /* application/json */
	ret = coap_packet_append_option(&pkt, COAP_OPTION_CONTENT_FORMAT,
					&fmt, sizeof(fmt));
	if (ret < 0) {
		return ret;
	}

	ret = coap_packet_append_payload_marker(&pkt);
	if (ret < 0) {
		return ret;
	}

	char payload[256];

	snprintf(payload, sizeof(payload),
		 "{\"name\":\"%s\",\"fn\":\"%s\",\"fw\":\"" KERNEL_VERSION_STRING "\","
		 "\"components\":%d}",
		 CONFIG_ZEPHYRHOME_DEVICE_NAME,
		 CONFIG_ZEPHYRHOME_FRIENDLY_NAME,
		 zh_component_count());

	ret = coap_packet_append_payload(&pkt, (const uint8_t *)payload,
					 strlen(payload));
	if (ret < 0) {
		return ret;
	}

	struct sockaddr_in6 dst = {
		.sin6_family = AF_INET6,
		.sin6_port   = htons(COAP_PORT),
	};

	if (zsock_inet_pton(AF_INET6, MCAST_ADDR, &dst.sin6_addr) != 1) {
		return -EINVAL;
	}

	ret = zsock_sendto(g_sock, pkt.data, pkt.offset, 0,
			   (struct sockaddr *)&dst, sizeof(dst));
	if (ret < 0) {
		LOG_ERR("announce sendto failed: %d", errno);
		return -errno;
	}

	LOG_INF("Announced to %s", MCAST_ADDR);
	return 0;
}
