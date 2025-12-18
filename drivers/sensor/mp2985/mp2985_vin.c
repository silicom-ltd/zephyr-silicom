/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT mps_mp2985_vin

#include <zephyr/drivers/mfd/mp2985.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(MP2985_VIN, CONFIG_SENSOR_LOG_LEVEL);

struct mp2985_vin_config {
	const struct device *mfd;
	uint8_t page;
};

struct mp2985_vin_data {
	int volts;
};

/* convert to millivolts */
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

static int mp2985_vin_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct mp2985_vin_config *config = dev->config;
	struct mp2985_vin_data *data = dev->data;
        uint16_t val;
	int result;

	if ((chan != SENSOR_CHAN_VOLTAGE) && (chan != SENSOR_CHAN_ALL)) {
		return -ENOTSUP;
	}

	result = mfd_mp2985_read_word(config->mfd, config->page, PMBUS_READ_VIN, &val);

	if (result != 0) {
		return result;
	}

	result = val2data_linear(dev, val);

	LOG_DBG("%s vin: %dmV", dev->name, result);

	data->volts = result;

	return 0;
}

static int mp2985_vin_channel_get(const struct device *dev, enum sensor_channel chan,
			          struct sensor_value *val)
{
	struct mp2985_vin_data *data = dev->data;

	if (chan != SENSOR_CHAN_VOLTAGE) {
		return -ENOTSUP;
	}

	val->val1 = (data->volts / 1000);
	val->val2 = (data->volts % 1000) * 1000;
	return 0;
}

static struct sensor_driver_api mp2985_vin_api = {
	.sample_fetch = mp2985_vin_sample_fetch,
	.channel_get = mp2985_vin_channel_get,
};

static int mp2985_vin_init(const struct device *dev)
{
	mp2985_vin_sample_fetch(dev, SENSOR_CHAN_VOLTAGE);

	return 0;
}

#define MP2985_VIN_INIT(inst)                                                                      \
	static const struct mp2985_vin_config mp2985_vin_##inst##_config = {                       \
		.mfd = DEVICE_DT_GET(DT_INST_BUS(inst)),                                           \
		.page = DT_INST_PROP(inst, page),                                                  \
	};                                                                                         \
                                                                                                   \
	static struct mp2985_vin_data mp2985_vin_##inst##_data;                                    \
                                                                                                   \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, mp2985_vin_init, NULL,                                  \
			      &mp2985_vin_##inst##_data,                                           \
			      &mp2985_vin_##inst##_config, POST_KERNEL,                            \
			      81, &mp2985_vin_api);

DT_INST_FOREACH_STATUS_OKAY(MP2985_VIN_INIT);
