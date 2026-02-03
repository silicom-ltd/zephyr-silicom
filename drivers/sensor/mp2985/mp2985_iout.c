/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT mps_mp2985_iout

#include <zephyr/drivers/mfd/mp2985.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(MP2985_IOUT, CONFIG_SENSOR_LOG_LEVEL);

struct mp2985_iout_config {
	const struct device *mfd;
	uint8_t page;
};

struct mp2985_iout_data {
	int current;
};

/* convert to mwatts */
static int val2data_linear(const struct device *dev, uint16_t val)
{
	int exp = (int16_t)val >> 11;
	int ret = ((int16_t)(val & 0x7FF) << 5) >> 5;

	ret *= 1000;

	if (exp < 0) {
		ret >>= -exp;
	} else {
		ret <<= exp;
	}	

	return ret;
}

static int mp2985_iout_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct mp2985_iout_config *config = dev->config;
	struct mp2985_iout_data *data = dev->data;
        uint16_t val;
	int result;

	if ((chan != SENSOR_CHAN_CURRENT) && (chan != SENSOR_CHAN_ALL)) {
		return -ENOTSUP;
	}

	result = mfd_mp2985_read_word(config->mfd, config->page, PMBUS_READ_IOUT, &val);

	if (result != 0) {
		return result;
	}

	result = val2data_linear(dev, val);

	LOG_DBG("%s iout: %dmA", dev->name, result);

	data->current = result;

	return 0;
}

static int mp2985_iout_channel_get(const struct device *dev, enum sensor_channel chan,
				    struct sensor_value *val)
{
	struct mp2985_iout_data *data = dev->data;

	if (chan != SENSOR_CHAN_CURRENT) {
		return -ENOTSUP;
	}

	val->val1 = (data->current / 1000);
	val->val2 = (data->current % 1000) * 1000;
	return 0;
}

static struct sensor_driver_api mp2985_iout_api = {
	.sample_fetch = mp2985_iout_sample_fetch,
	.channel_get = mp2985_iout_channel_get,
};

static int mp2985_iout_init(const struct device *dev)
{
	mp2985_iout_sample_fetch(dev, SENSOR_CHAN_CURRENT);

	return 0;
}

#define MP2985_IOUT_INIT(inst)                                                                     \
	static const struct mp2985_iout_config mp2985_iout_##inst##_config = {                     \
		.mfd = DEVICE_DT_GET(DT_INST_BUS(inst)),                                           \
		.page = DT_INST_PROP(inst, page),                                                  \
	};                                                                                         \
                                                                                                   \
	static struct mp2985_iout_data mp2985_iout_##inst##_data;                                  \
                                                                                                   \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, mp2985_iout_init, NULL,                                 \
			      &mp2985_iout_##inst##_data,                                          \
			      &mp2985_iout_##inst##_config, POST_KERNEL,                           \
			      81, &mp2985_iout_api);

DT_INST_FOREACH_STATUS_OKAY(MP2985_IOUT_INIT);
