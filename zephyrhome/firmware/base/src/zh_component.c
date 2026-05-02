/* SPDX-License-Identifier: Apache-2.0 */
#include "zh_component.h"
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(zh_component, LOG_LEVEL_INF);

static struct zh_component *registry[ZH_MAX_COMPONENTS];
static int registry_count;

int zh_component_register(struct zh_component *comp)
{
	if (registry_count >= ZH_MAX_COMPONENTS) {
		LOG_ERR("Component registry full (max %d)", ZH_MAX_COMPONENTS);
		return -ENOMEM;
	}

	int ret = comp->ops->init(comp);
	if (ret < 0) {
		LOG_ERR("Component '%s' init failed: %d", comp->name, ret);
		return ret;
	}

	comp->last_update_ms = k_uptime_get();
	registry[registry_count++] = comp;
	LOG_INF("Registered '%s' (type=%d, interval=%u ms)",
		comp->name, comp->type, comp->update_interval_ms);
	return 0;
}

struct zh_component *zh_component_find(const char *name)
{
	for (int i = 0; i < registry_count; i++) {
		if (strncmp(registry[i]->name, name, ZH_COMPONENT_NAME_MAX) == 0) {
			return registry[i];
		}
	}
	return NULL;
}

int zh_component_count(void) { return registry_count; }

struct zh_component *zh_component_get(int index)
{
	if (index < 0 || index >= registry_count) {
		return NULL;
	}
	return registry[index];
}

void zh_component_tick_all(void)
{
	int64_t now = k_uptime_get();

	for (int i = 0; i < registry_count; i++) {
		struct zh_component *c = registry[i];

		if (!c->ops->tick) {
			continue;
		}
		if ((uint32_t)(now - c->last_update_ms) >= c->update_interval_ms) {
			c->ops->tick(c);
			c->last_update_ms = now;
		}
	}
}
