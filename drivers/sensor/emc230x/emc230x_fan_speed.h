/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_MICROCHIP_EMC230X_FAN_SPEED_H_
#define ZEPHYR_DRIVERS_SENSOR_MICROCHIP_EMC230X_FAN_SPEED_H_

#include <zephyr/drivers/i2c.h>

struct emc230x_fan_speed_config {
	struct i2c_dt_spec i2c;
	uint8_t channel_id;
};

struct emc230x_fan_speed_data {
	uint16_t rpm;
};

#endif /* ZEPHYR_DRIVERS_SENSOR_MICROCHIP_EMC230X_FAN_SPEED_H_ */
