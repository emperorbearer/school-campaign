/* SPDX-License-Identifier: Apache-2.0 */
/*
 * ZephyrHome framework main entry point.
 * Called from the generated main.c via zh_main_run().
 */
#include "zh_main.h"
#include "zh_component.h"
#include "zh_coap_server.h"
#include "zh_ot_init.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zh_main, LOG_LEVEL_INF);

#define ANNOUNCE_STACK_SIZE 1024
#define ANNOUNCE_PRIO       7

K_THREAD_STACK_DEFINE(g_announce_stack, ANNOUNCE_STACK_SIZE);
static struct k_thread g_announce_thread;

static void announce_thread_fn(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

	/* Initial delay to let CoAP server stabilise */
	k_sleep(K_SECONDS(2));

	while (true) {
		zh_coap_announce();
		k_sleep(K_SECONDS(CONFIG_ZEPHYRHOME_ANNOUNCE_INTERVAL_SEC));
	}
}

static void on_network_ready(void)
{
	LOG_INF("Thread network joined — starting ZephyrHome CoAP server");

	int ret = zh_coap_server_start();

	if (ret < 0) {
		LOG_ERR("CoAP server start failed: %d", ret);
		return;
	}

	zh_components_init();
	LOG_INF("Registered %d component(s)", zh_component_count());

	k_thread_create(&g_announce_thread, g_announce_stack,
			K_THREAD_STACK_SIZEOF(g_announce_stack),
			announce_thread_fn, NULL, NULL, NULL,
			ANNOUNCE_PRIO, 0, K_NO_WAIT);
	k_thread_name_set(&g_announce_thread, "zh_announce");
}

int zh_main_run(void)
{
	LOG_INF("=== ZephyrHome v0.1.0 — %s ===",
		CONFIG_ZEPHYRHOME_DEVICE_NAME);

	zh_ot_init(on_network_ready);

	/* main thread exits; Zephyr idle thread takes over → WFI low power */
	return 0;
}
