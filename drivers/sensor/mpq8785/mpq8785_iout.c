/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT mps_mpq8785_iout

#include <zephyr/drivers/mfd/mpq8785.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(MPQ8785_IOUT, CONFIG_SENSOR_LOG_LEVEL);

struct mpq8785_iout_config {
	const struct device *mfd;
	uint8_t page;
	int m;
	int b;
	int R;
};

struct mpq8785_iout_data {
	int current;
};

/* convert to milliamps */
static int val2data(const struct device *dev, uint16_t data)
{
	const struct mpq8785_iout_config *config = dev->config;

	int m = config->m;
	int b = config->b;
	int R = -(config->R);
	int ret = (int)data;

	R += 3;
	b *= 1000;
	
	while (R > 0) {
		ret *= 10;
		R--;
	}

	ret = (ret - b) / m;

	return ret;
}

static int mpq8785_iout_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct mpq8785_iout_config *config = dev->config;
	struct mpq8785_iout_data *data = dev->data;
        uint16_t val;
	int result;

	if ((chan != SENSOR_CHAN_CURRENT) && (chan != SENSOR_CHAN_ALL)) {
		return -ENOTSUP;
	}

	result = mfd_mpq8785_read_word(config->mfd, config->page, PMBUS_READ_IOUT, &val);

	if (result != 0) {
		return result;
	}

	result = val2data(dev, val);
//	LOG_DBG("%s Iout: %dmA", dev->name, result);

	data->current = result;

	return 0;
}

static int mpq8785_iout_channel_get(const struct device *dev, enum sensor_channel chan,
				    struct sensor_value *val)
{
	struct mpq8785_iout_data *data = dev->data;

	if (chan != SENSOR_CHAN_CURRENT) {
		return -ENOTSUP;
	}

	val->val1 = (data->current / 1000);
	val->val2 = (data->current % 1000) * 1000;
	return 0;
}

static struct sensor_driver_api mpq8785_iout_api = {
	.sample_fetch = mpq8785_iout_sample_fetch,
	.channel_get = mpq8785_iout_channel_get,
};

static int mpq8785_iout_init(const struct device *dev)
{
	mpq8785_iout_sample_fetch(dev, SENSOR_CHAN_ALL);

	return 0;
}

#define MPQ8785_IOUT_INIT(inst)                                                                    \
	static const struct mpq8785_iout_config mpq8785_iout_##inst##_config = {                   \
		.mfd = DEVICE_DT_GET(DT_INST_BUS(inst)),                                           \
		.page = DT_INST_PROP(inst, page),                                                  \
                .m = DT_INST_PROP_BY_IDX(inst, coefficients, 0),                                   \
		.b = DT_INST_PROP_BY_IDX(inst, coefficients, 1),                                   \
		.R = DT_INST_PROP_BY_IDX(inst, coefficients, 2),                                   \
	};                                                                                         \
                                                                                                   \
	static struct mpq8785_iout_data mpq8785_iout_##inst##_data;                                \
                                                                                                   \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, mpq8785_iout_init, NULL,                                \
			      &mpq8785_iout_##inst##_data,                                         \
			      &mpq8785_iout_##inst##_config, POST_KERNEL,                          \
			      81, &mpq8785_iout_api);

DT_INST_FOREACH_STATUS_OKAY(MPQ8785_IOUT_INIT);
