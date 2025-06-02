/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_MICROCHIP_EMC230X_FAN_FAULT_H_
#define ZEPHYR_DRIVERS_SENSOR_MICROCHIP_EMC230X_FAN_FAULT_H_

#include <zephyr/drivers/i2c.h>

struct emc230x_fan_fault_config {
	struct i2c_dt_spec i2c;
};

struct emc320x_fan_fault_data {
	uint8_t value;
};

#endif /* ZEPHYR_DRIVERS_SENSOR_MICROCHIP_EMC230X_FAN_FAULT_H_ */
