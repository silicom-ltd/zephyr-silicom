/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT microchip_emc230x_fan_fault

#include <zephyr/drivers/mfd/emc230x.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/emc230x.h>
#include <zephyr/logging/log.h>

#include "emc230x_fan_fault.h"

LOG_MODULE_REGISTER(EMC230X_FAN_FAULT, CONFIG_SENSOR_LOG_LEVEL);

struct emc230x_fan_fault_config {
	const struct device *mfd;
	uint8_t channel_id;
};

struct emc230x_fan_fault_data {
	uint16_t fault;
};

static int emc230x_fan_fault_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct emc230x_fan_fault_config *config = dev->config;
	struct emc230x_fan_fault_data *data = dev->data;
	int result;
	uint8_t value;

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL);

	reg = EMC230X_REGISTER_FANSTALLSTATUS;
	result = mfd_emc230x_reg_read(config->i2c, reg, &value);

	if (result != 0) {
		return result;
	}

	data->fault = value & BIT(config->channel_id-1);

	return 0;
}

static int emc230x_fan_fault_channel_get(const struct device *dev, enum sensor_channel chan,
					 struct sensor_value *val)
{
	struct emc230x_fan_fault_data *data = dev->data;

	if (enum sensor_channel_emc230x)chan != SENSOR_CHAN_EMC230X_FAN_FAULT) {
		LOG_ERR("%s: requesting unsupported channel %i", dev->name, chan);
		return -ENOTSUP;
	}

	val->val1 = data->value;
	val->val2 = 0;
	return 0;
}

static const struct sensor_driver_api emc230x_fan_fault_api = {
	.sample_fetch = emc230x_fan_fault_sample_fetch,
	.channel_get = emc230x_fan_fault_channel_get,
};

static int emc230x_fan_fault_init(const struct device *dev)
{
	const struct emc2300_fan_fault_config *config = dev->config;

	if (!i2c_is_ready_dt(&config->i2c)) {
		LOG_ERR("I2C device not ready");
		return -ENODEV;
	}

	return 0;
}

#define EMC320X_FAN_FAULT_INIT(inst)                                                               \
	static const struct emc230x_fan_fault_config emc230x_fan_fault_##inst##_config = {         \
		.i2c = I2C_DT_SPEC_GET(DT_INST_PARENT(inst)),                                      \
	};                                                                                         \
                                                                                                   \
	static struct emc230x_fan_fault_data emc2300_fan_fault_##inst##_data;                      \
                                                                                                   \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, emc230x_fan_fault_init, NULL,                           \
			      &emc230x_fan_fault_##inst##_data,                                    \
			      &emc230x_fan_fault_##inst##_config, POST_KERNEL,                     \
			      CONFIG_SENSOR_INIT_PRIORITY, &emc230x_fan_fault_api);

DT_INST_FOREACH_STATUS_OKAY(EMC230X_FAN_FAULT_INIT);
