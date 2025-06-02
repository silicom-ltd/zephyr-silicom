/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_SENSOR_EMC230X_H_
#define ZEPHYR_INCLUDE_DRIVERS_SENSOR_EMC230X_H_

#include <zephyr/drivers/sensor.h>

/* EMC230X specific channels */
enum sensor_channel_emc230x {
	SENSOR_CHAN_EMC230X_FAN_FAULT = SENSOR_CHAN_PRIV_START,
};

#endif /* ZEPHYR_INCLUDE_DRIVERS_SENSOR_MAX31790_H_ */
