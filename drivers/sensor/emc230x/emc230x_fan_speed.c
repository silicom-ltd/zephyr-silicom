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
	struct device *mfd;
	uint8_t channel_id;
};

struct emc230x_fan_speed_data {
	uint16_t rpm;
};

static int emc230x_fan_speed_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct emc230x_fan_speed_config *config = dev->config;
	struct emc230x_fan_speed_data *data = dev->data;
	uint16_t tach_count;
	uint8_t fan_dynamics;
	uint8_t number_tach_periods_counted;
	uint8_t speed_range;
	uint8_t reg;
	int result;

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL);

	reg = EMC230X_REGISTER_TACHCOUNTMSB(config->channel_id);
	result = mfd_emc230x_reg_read(config->mfd, reg, &tach_count);

	tach_count = sys_be16_to_cpu(tach_count);
	if (result != 0) {
		return result;
	}

	tach_count >>= 3;

	if (tach_count == 0) {
		LOG_WRN("%s: tach count is zero", dev->name);
		data->rpm = UINT16_MAX;
	} else {
		LOG_DBG("%s: %i tach count", dev->name,
			tach_count);
		data->rpm = 3932160 / tach_count;
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
		.channel_id = DT_INST_PROP(inst, channel) - 1,                                     \
	};                                                                                         \
                                                                                                   \
	static struct emc230x_fan_speed_data emc230x_fan_speed_##inst##_data;                      \
                                                                                                   \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, emc230x_fan_speed_init, NULL,                           \
			      &emc230x_fan_speed_##inst##_data,                                    \
			      &emc230x_fan_speed_##inst##_config, POST_KERNEL,                     \
			      CONFIG_SENSOR_INIT_PRIORITY, &emc230x_fan_speed_api);

DT_INST_FOREACH_STATUS_OKAY(EMC230X_FAN_SPEED_INIT);
