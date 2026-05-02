/* SPDX-License-Identifier: Apache-2.0 */
/* ZephyrHome component vtable and registry. */
#ifndef ZH_COMPONENT_H_
#define ZH_COMPONENT_H_

#include <zephyr/kernel.h>
#include <stdbool.h>

#define ZH_COMPONENT_NAME_MAX 32
#define ZH_MAX_COMPONENTS     16

/* ── Value type ─────────────────────────────────────────────────────────── */

typedef enum {
	ZH_TYPE_SENSOR = 0,
	ZH_TYPE_SWITCH = 1,
} zh_component_type_t;

/* Unified value returned by read() / passed to write() */
typedef union {
	float   f32;     /* sensor readings (temperature, humidity) */
	bool    boolean; /* switch state */
	int32_t i32;
} zh_value_t;

/* ── Component vtable ───────────────────────────────────────────────────── */

struct zh_component;

struct zh_component_ops {
	/* Called once at startup; returns 0 on success */
	int (*init)(struct zh_component *self);

	/*
	 * Sensors: populate *out with the latest reading.
	 * Switches: populate *out with the current on/off state.
	 */
	int (*read)(struct zh_component *self, zh_value_t *out);

	/* Switches only: apply new state. NULL for sensors. */
	int (*write)(struct zh_component *self, zh_value_t val);

	/* Called every update_interval_ms by zh_component_tick_all(). */
	void (*tick)(struct zh_component *self);
};

/* ── Component descriptor ───────────────────────────────────────────────── */

struct zh_component {
	char name[ZH_COMPONENT_NAME_MAX];
	zh_component_type_t type;
	const struct zh_component_ops *ops;
	void *driver_data;
	uint32_t update_interval_ms;
	int64_t  last_update_ms;
};

/* ── Registry API ───────────────────────────────────────────────────────── */

int zh_component_register(struct zh_component *comp);
struct zh_component *zh_component_find(const char *name);
int zh_component_count(void);
struct zh_component *zh_component_get(int index);

/* Drive all sensor tick() callbacks at their configured intervals. */
void zh_component_tick_all(void);

/* Declared in generated_components.c by the CLI code generator. */
extern void zh_components_init(void);

#endif /* ZH_COMPONENT_H_ */
