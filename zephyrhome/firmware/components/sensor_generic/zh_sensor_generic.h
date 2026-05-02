/* SPDX-License-Identifier: Apache-2.0 */
/* Generic Zephyr sensor API wrapper for ZephyrHome. */
#ifndef ZH_SENSOR_GENERIC_H_
#define ZH_SENSOR_GENERIC_H_

#include <zephyr/device.h>
#include <stdint.h>

/*
 * Register a Zephyr sensor device as a ZephyrHome component.
 *
 * name         - component name used in CoAP paths (e.g. "room_sensor")
 * dev          - Zephyr device pointer (from DEVICE_DT_GET)
 * interval_sec - how often to poll the sensor (seconds)
 *
 * Returns 0 on success.
 */
int zh_sensor_register(const char *name, const struct device *dev,
		       uint32_t interval_sec);

#endif /* ZH_SENSOR_GENERIC_H_ */
