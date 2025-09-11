/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT microchip_emc230x_fan_speed

#include <zephyr/drivers/mfd/emc230x.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(EMC230X_FAN_SPEED, CONFIG_SENSOR_LOG_LEVEL);

struct emc230x_fan_speed_config {
	const struct device *mfd;
	uint8_t channel_id;
};

struct emc230x_fan_speed_data {
	uint16_t rpm;
};

static int emc230x_fan_speed_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct emc230x_fan_speed_config *config = dev->config;
	struct emc230x_fan_speed_data *data = dev->data;
	uint8_t tach_count;
	uint16_t tach;
	uint8_t reg;
	int result;

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL);

	reg = EMC230X_REGISTER_TACHCOUNTMSB(config->channel_id);
	result = mfd_emc230x_reg_read(config->mfd, reg, &tach_count);

	if (result != 0) {
		return result;
	}

	tach = (tach_count << 5);

	reg = EMC230X_REGISTER_TACHCOUNTLSB(config->channel_id);
	result = mfd_emc230x_reg_read(config->mfd, reg, &tach_count);

	tach |= (tach_count >> 3);

	if (tach == 0x1FFF) {
		LOG_DBG("%s: tach count is zero", dev->name);
		data->rpm = 0;
	} else {
		data->rpm = 3932160 / tach;
		LOG_DBG("%s: %i tach count, rpm %i", dev->name,
			tach, data->rpm);
	}

	return 0;
}

static int emc230x_fan_speed_channel_get(const struct device *dev, enum sensor_channel chan,
					 struct sensor_value *val)
{
	struct emc230x_fan_speed_data *data = dev->data;

	if (chan != SENSOR_CHAN_RPM) {
		LOG_ERR("%s: requesting unsupported channel %i", dev->name, chan);
		return -ENOTSUP;
	}

	val->val1 = data->rpm;
	val->val2 = 0;
	return 0;
}

static struct sensor_driver_api emc230x_fan_speed_api = {
	.sample_fetch = emc230x_fan_speed_sample_fetch,
	.channel_get = emc230x_fan_speed_channel_get,
};

static int emc230x_fan_speed_init(const struct device *dev)
{
	return 0;
}

#define EMC230X_FAN_SPEED_INIT(inst)                                                               \
	static const struct emc230x_fan_speed_config emc230x_fan_speed_##inst##_config = {         \
		.mfd = DEVICE_DT_GET(DT_INST_PARENT(inst)),                                        \
		.channel_id = DT_INST_PROP(inst, channel),                                         \
	};                                                                                         \
                                                                                                   \
	static struct emc230x_fan_speed_data emc230x_fan_speed_##inst##_data;                      \
                                                                                                   \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, emc230x_fan_speed_init, NULL,                           \
			      &emc230x_fan_speed_##inst##_data,                                    \
			      &emc230x_fan_speed_##inst##_config, POST_KERNEL,                     \
			      81, &emc230x_fan_speed_api);

DT_INST_FOREACH_STATUS_OKAY(EMC230X_FAN_SPEED_INIT);
