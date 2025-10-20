/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT mps_mpq8785_vout

#include <zephyr/drivers/mfd/mpq8785.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(MPQ8785_VOUT, CONFIG_SENSOR_LOG_LEVEL);

struct mpq8785_vout_config {
	const struct device *mfd;
	uint8_t page;
	int m;
	int b;
	int R;
};

struct mpq8785_vout_data {
	int voltage;
	int8_t exponent;
};

/* convert to millivolts */
static int val2data_direct(const struct device *dev, uint16_t data)
{
	const struct mpq8785_vout_config *config = dev->config;

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

static int val2data_linear(const struct device *dev, uint16_t val)
{
	const struct mpq8785_vout_data *data = dev->data;
	int ret = (int)val;

	ret *= 1000;

	if (data->exponent < 0) {
		ret >>= -data->exponent;
	} else {
		ret <<= data->exponent;
	}	

	return ret;
}

static int mpq8785_vout_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct mpq8785_vout_config *config = dev->config;
	struct mpq8785_vout_data *data = dev->data;
        uint16_t val;
	int result;

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL);
	result = mfd_mpq8785_read_word(config->mfd, config->page, PMBUS_READ_VOUT, &val);

	if (result != 0) {
		return result;
	}

	if (data->exponent)
		result = val2data_linear(dev, val);
	else
		result = val2data_direct(dev, val);

//	result = val2data_direct(dev, val);
	LOG_DBG("%s Vout: %dmV", dev->name, result);

	data->voltage = result;

	return 0;
}

static int mpq8785_vout_channel_get(const struct device *dev, enum sensor_channel chan,
				    struct sensor_value *val)
{
	struct mpq8785_vout_data *data = dev->data;

	if (chan != SENSOR_CHAN_VOLTAGE) {
		return -ENOTSUP;
	}

	val->val1 = data->voltage;
	val->val2 = 0;
	return 0;
}

static struct sensor_driver_api mpq8785_vout_api = {
	.sample_fetch = mpq8785_vout_sample_fetch,
	.channel_get = mpq8785_vout_channel_get,
};

static int mpq8785_vout_init(const struct device *dev)
{
	const struct mpq8785_vout_config *config = dev->config;
	struct mpq8785_vout_data *data = dev->data;
	uint8_t vout_mode;

	(void)mfd_mpq8785_read_byte(config->mfd, config->page, PMBUS_VOUT_MODE, &vout_mode);

	/* if linear mode, lower 5 bits are the 2s complement exponent */
	if ((vout_mode >> 5) == 0)
		data->exponent = ((int8_t)(vout_mode << 3) >> 3);

	mpq8785_vout_sample_fetch(dev, SENSOR_CHAN_ALL);

	return 0;
}

#define MPQ8785_VOUT_INIT(inst)                                                                    \
	static const struct mpq8785_vout_config mpq8785_vout_##inst##_config = {                   \
		.mfd = DEVICE_DT_GET(DT_INST_BUS(inst)),                                           \
		.page = DT_INST_PROP(inst, page),                                                  \
                .m = DT_INST_PROP_BY_IDX(inst, coefficients, 0),                                   \
		.b = DT_INST_PROP_BY_IDX(inst, coefficients, 1),                                   \
		.R = DT_INST_PROP_BY_IDX(inst, coefficients, 2),                                   \
	};                                                                                         \
                                                                                                   \
	static struct mpq8785_vout_data mpq8785_vout_##inst##_data;                                \
                                                                                                   \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, mpq8785_vout_init, NULL,                                \
			      &mpq8785_vout_##inst##_data,                                         \
			      &mpq8785_vout_##inst##_config, POST_KERNEL,                          \
			      81, &mpq8785_vout_api);

DT_INST_FOREACH_STATUS_OKAY(MPQ8785_VOUT_INIT);
