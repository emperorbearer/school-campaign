/* SPDX-License-Identifier: Apache-2.0 */
/*
 * GPIO switch component.
 * Controls a single GPIO output (relay, LED, MOSFET, …).
 */
#include "zh_switch_gpio.h"
#include "zh_component.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(zh_switch_gpio, LOG_LEVEL_INF);

#define SWITCH_POOL_MAX ZH_MAX_COMPONENTS

struct switch_data {
	const struct device *port;
	uint32_t pin;
	uint32_t flags;
	bool state;
};

static struct switch_data s_pool[SWITCH_POOL_MAX];
static struct zh_component s_comp[SWITCH_POOL_MAX];
static int s_pool_count;

/* ── Component vtable ───────────────────────────────────────────────────── */

static int sw_init(struct zh_component *self)
{
	struct switch_data *d = self->driver_data;

	if (!device_is_ready(d->port)) {
		LOG_ERR("GPIO port for switch '%s' not ready", self->name);
		return -ENODEV;
	}

	int ret = gpio_pin_configure(d->port, d->pin,
				     GPIO_OUTPUT_INACTIVE | d->flags);
	if (ret < 0) {
		LOG_ERR("gpio_pin_configure failed: %d", ret);
		return ret;
	}

	d->state = false;
	LOG_INF("Switch '%s' ready (pin=%u)", self->name, d->pin);
	return 0;
}

static int sw_read(struct zh_component *self, zh_value_t *out)
{
	struct switch_data *d = self->driver_data;

	out->boolean = d->state;
	return 0;
}

static int sw_write(struct zh_component *self, zh_value_t val)
{
	struct switch_data *d = self->driver_data;

	int ret = gpio_pin_set(d->port, d->pin, val.boolean ? 1 : 0);

	if (ret < 0) {
		LOG_ERR("gpio_pin_set failed: %d", ret);
		return ret;
	}

	d->state = val.boolean;
	LOG_INF("Switch '%s' → %s", self->name, val.boolean ? "ON" : "OFF");
	return 0;
}

static const struct zh_component_ops switch_ops = {
	.init  = sw_init,
	.read  = sw_read,
	.write = sw_write,
	.tick  = NULL, /* switches are event-driven; no polling needed */
};

/* ── Public API ─────────────────────────────────────────────────────────── */

int zh_switch_gpio_register(const char *name,
			    const struct device *port,
			    uint32_t pin,
			    uint32_t flags)
{
	if (s_pool_count >= SWITCH_POOL_MAX) {
		LOG_ERR("Switch pool full");
		return -ENOMEM;
	}

	int idx = s_pool_count++;
	struct switch_data *d = &s_pool[idx];
	struct zh_component *comp = &s_comp[idx];

	d->port  = port;
	d->pin   = pin;
	d->flags = flags;

	strncpy(comp->name, name, ZH_COMPONENT_NAME_MAX - 1);
	comp->name[ZH_COMPONENT_NAME_MAX - 1] = '\0';
	comp->type = ZH_TYPE_SWITCH;
	comp->ops  = &switch_ops;
	comp->driver_data = d;
	comp->update_interval_ms = 0; /* no periodic tick */

	return zh_component_register(comp);
}
