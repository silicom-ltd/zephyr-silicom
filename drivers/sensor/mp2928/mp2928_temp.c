/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT mps_mp2928_temp

#include <zephyr/drivers/mfd/mp2928.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(MP2928_TEMP, CONFIG_SENSOR_LOG_LEVEL);

struct mp2928_temp_config {
	const struct device *mfd;
	uint8_t page;
};

struct mp2928_temp_data {
	int temperature;
};

/* convert to millicelsius */
static int val2data(const struct device *dev, uint16_t data)
{
	int mantissa = (data & 0xFF);

	mantissa *= 1000;

	return mantissa;
}

static int mp2928_temp_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct mp2928_temp_config *config = dev->config;
	struct mp2928_temp_data *data = dev->data;
        uint16_t val;
	int result;

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL);

	result = mfd_mp2928_read_word(config->mfd, config->page, PMBUS_READ_TEMP_1, &val);

	if (result != 0) {
		return result;
	}

	result = val2data(dev, val);
	LOG_DBG("%s Temp: %dmC", dev->name, result);

	data->temperature = result;

	return 0;
}

static int mp2928_temp_channel_get(const struct device *dev, enum sensor_channel chan,
				   struct sensor_value *val)
{
	struct mp2928_temp_data *data = dev->data;

	if (chan != SENSOR_CHAN_DIE_TEMP) {
		return -ENOTSUP;
	}

	val->val1 = (data->temperature / 1000);
	val->val2 = (data->temperature % 1000) * 1000;
	return 0;
}

static struct sensor_driver_api mp2928_temp_api = {
	.sample_fetch = mp2928_temp_sample_fetch,
	.channel_get = mp2928_temp_channel_get,
};

static int mp2928_temp_init(const struct device *dev)
{
	mp2928_temp_sample_fetch(dev, SENSOR_CHAN_ALL);
	return 0;
}

#define MP2928_TEMP_INIT(inst)                                                                     \
	static const struct mp2928_temp_config mp2928_temp_##inst##_config = {                     \
		.mfd = DEVICE_DT_GET(DT_INST_BUS(inst)),                                           \
		.page = DT_INST_PROP(inst, page),                                                  \
	};                                                                                         \
                                                                                                   \
	static struct mp2928_temp_data mp2928_temp_##inst##_data;                                  \
                                                                                                   \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, mp2928_temp_init, NULL,                                 \
			      &mp2928_temp_##inst##_data,                                          \
			      &mp2928_temp_##inst##_config, POST_KERNEL,                           \
			      81, &mp2928_temp_api);

DT_INST_FOREACH_STATUS_OKAY(MP2928_TEMP_INIT);
