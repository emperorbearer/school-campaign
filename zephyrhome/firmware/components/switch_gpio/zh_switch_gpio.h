/* SPDX-License-Identifier: Apache-2.0 */
/* GPIO switch component for ZephyrHome. */
#ifndef ZH_SWITCH_GPIO_H_
#define ZH_SWITCH_GPIO_H_

#include <zephyr/device.h>
#include <stdint.h>

/*
 * Register a GPIO pin as a ZephyrHome switch component.
 *
 * name   - component name used in CoAP paths (e.g. "relay1")
 * port   - GPIO controller device (e.g. DEVICE_DT_GET(DT_NODELABEL(gpio0)))
 * pin    - GPIO pin number
 * flags  - GPIO flags (GPIO_OUTPUT_INACTIVE, GPIO_ACTIVE_LOW, …)
 *
 * Returns 0 on success.
 */
int zh_switch_gpio_register(const char *name,
			    const struct device *port,
			    uint32_t pin,
			    uint32_t flags);

#endif /* ZH_SWITCH_GPIO_H_ */
