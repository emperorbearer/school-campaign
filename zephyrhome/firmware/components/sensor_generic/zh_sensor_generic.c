/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Generic Zephyr sensor API wrapper.
 *
 * Wraps any sensor that implements SENSOR_CHAN_AMBIENT_TEMP / SENSOR_CHAN_HUMIDITY
 * (BME280, SHT4x, HTS221, …) as a ZephyrHome component.
 *
 * Multiple sensors are supported via a static pool. Each call to
 * zh_sensor_register() consumes one pool slot.
 */
#include "zh_sensor_generic.h"
#include "zh_component.h"
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(zh_sensor_generic, LOG_LEVEL_INF);

#define SENSOR_POOL_MAX ZH_MAX_COMPONENTS

struct sensor_data {
	const struct device *dev;
	float last_temp;
	float last_humidity;
	bool has_humidity;
};

/* Static pool — allocated at registration time, one per sensor */
static struct sensor_data s_pool[SENSOR_POOL_MAX];
static struct zh_component s_comp[SENSOR_POOL_MAX];
static int s_pool_count;

/* ── Component vtable ───────────────────────────────────────────────────── */

static int sensor_init(struct zh_component *self)
{
	struct sensor_data *d = self->driver_data;

	if (!device_is_ready(d->dev)) {
		LOG_ERR("Sensor device '%s' not ready", self->name);
		return -ENODEV;
	}

	/* Probe for humidity support */
	struct sensor_value sv;
	int ret = sensor_sample_fetch(d->dev);

	if (ret == 0) {
		ret = sensor_channel_get(d->dev, SENSOR_CHAN_HUMIDITY, &sv);
		d->has_humidity = (ret == 0);
	}

	LOG_INF("Sensor '%s' ready (humidity=%s)", self->name,
		d->has_humidity ? "yes" : "no");
	return 0;
}

static int sensor_read(struct zh_component *self, zh_value_t *out)
{
	struct sensor_data *d = self->driver_data;
	struct sensor_value sv;

	int ret = sensor_sample_fetch(d->dev);

	if (ret < 0) {
		LOG_ERR("sensor_sample_fetch failed: %d", ret);
		return ret;
	}

	ret = sensor_channel_get(d->dev, SENSOR_CHAN_AMBIENT_TEMP, &sv);
	if (ret < 0) {
		return ret;
	}

	out->f32 = sensor_value_to_double(&sv);
	return 0;
}

static void sensor_tick(struct zh_component *self)
{
	struct sensor_data *d = self->driver_data;
	struct sensor_value sv;

	if (sensor_sample_fetch(d->dev) < 0) {
		return;
	}

	if (sensor_channel_get(d->dev, SENSOR_CHAN_AMBIENT_TEMP, &sv) == 0) {
		d->last_temp = sensor_value_to_double(&sv);
	}
	if (d->has_humidity &&
	    sensor_channel_get(d->dev, SENSOR_CHAN_HUMIDITY, &sv) == 0) {
		d->last_humidity = sensor_value_to_double(&sv);
	}

	LOG_DBG("'%s': temp=%.2f°C hum=%.2f%%",
		self->name, (double)d->last_temp, (double)d->last_humidity);
}

static const struct zh_component_ops sensor_ops = {
	.init  = sensor_init,
	.read  = sensor_read,
	.write = NULL,
	.tick  = sensor_tick,
};

/* ── Public API ─────────────────────────────────────────────────────────── */

int zh_sensor_register(const char *name, const struct device *dev,
		       uint32_t interval_sec)
{
	if (s_pool_count >= SENSOR_POOL_MAX) {
		LOG_ERR("Sensor pool full");
		return -ENOMEM;
	}

	int idx = s_pool_count++;
	struct sensor_data *d = &s_pool[idx];
	struct zh_component *comp = &s_comp[idx];

	d->dev = dev;

	strncpy(comp->name, name, ZH_COMPONENT_NAME_MAX - 1);
	comp->name[ZH_COMPONENT_NAME_MAX - 1] = '\0';
	comp->type = ZH_TYPE_SENSOR;
	comp->ops  = &sensor_ops;
	comp->driver_data = d;
	comp->update_interval_ms = interval_sec * 1000U;

	return zh_component_register(comp);
}
