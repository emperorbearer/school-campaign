/* SPDX-License-Identifier: Apache-2.0 */
/* OpenThread initialization for ZephyrHome. */
#ifndef ZH_OT_INIT_H_
#define ZH_OT_INIT_H_

typedef void (*zh_network_ready_cb)(void);

/*
 * Configure the Thread dataset from Kconfig (CONFIG_ZH_OT_*) and start
 * the stack. ready_cb is invoked asynchronously when the device joins as
 * CHILD or ROUTER.
 */
void zh_ot_init(zh_network_ready_cb ready_cb);

#endif /* ZH_OT_INIT_H_ */
