/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ocp_crps_iout

#include <zephyr/drivers/mfd/crps.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(CRPS_IOUT, CONFIG_SENSOR_LOG_LEVEL);

struct crps_iout_config {
	const struct device *mfd;
	uint8_t page;
};

struct crps_iout_data {
	int current;
};

/* convert to millivolts */
static int val2data(const struct device *dev, uint16_t data)
{
	int exp = (int16_t)data >> 11;
	int mantissa = ((int16_t)((data & 0x7FF) << 5) >> 5);

	mantissa *= 1000;
	
	if (exp < 0)
		mantissa >>= -exp;
	else
		mantissa <<= exp;

	return mantissa;
}

static int crps_iout_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct crps_iout_config *config = dev->config;
	struct crps_iout_data *data = dev->data;
        uint16_t val;
	int result;

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL);

	result = mfd_crps_read_word(config->mfd, config->page, PMBUS_READ_IOUT, &val);

	if (result != 0) {
		return result;
	}

	result = val2data(dev, val);
	//LOG_DBG("%s Iout: %dmA", dev->name, result);

	data->current = result;

	return 0;
}

static int crps_iout_channel_get(const struct device *dev, enum sensor_channel chan,
			         struct sensor_value *val)
{
	struct crps_iout_data *data = dev->data;

	if (chan != SENSOR_CHAN_VOLTAGE) {
		return -ENOTSUP;
	}

	val->val1 = data->current;
	val->val2 = 0;
	return 0;
}

static struct sensor_driver_api crps_iout_api = {
	.sample_fetch = crps_iout_sample_fetch,
	.channel_get = crps_iout_channel_get,
};

static int crps_iout_init(const struct device *dev)
{
	crps_iout_sample_fetch(dev, SENSOR_CHAN_ALL);

	return 0;
}

#define CRPS_IOUT_INIT(inst)                                                                       \
	static const struct crps_iout_config crps_iout_##inst##_config = {                         \
		.mfd = DEVICE_DT_GET(DT_INST_BUS(inst)),                                           \
		.page = DT_INST_PROP(inst, page),                                                  \
	};                                                                                         \
                                                                                                   \
	static struct crps_iout_data crps_iout_##inst##_data;                                      \
                                                                                                   \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, crps_iout_init, NULL,                                   \
			      &crps_iout_##inst##_data,                                            \
			      &crps_iout_##inst##_config, POST_KERNEL,                             \
			      81, &crps_iout_api);

DT_INST_FOREACH_STATUS_OKAY(CRPS_IOUT_INIT);
