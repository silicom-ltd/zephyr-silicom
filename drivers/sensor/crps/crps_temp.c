/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ocp_crps_temp

#include <zephyr/drivers/mfd/crps.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(CRPS_TEMP, CONFIG_SENSOR_LOG_LEVEL);

struct crps_temp_config {
	const struct device *mfd;
	uint8_t page;
	uint8_t temp_reg;
	int m;
	int b;
	int R;
};

struct crps_temp_data {
	int temperature;
};

/* convert to millicelsius */
static int val2data(const struct device *dev, uint16_t data)
{
	int mantissa = ((int16_t)((data & 0x7FF) << 5) >> 5);
	int exp = (int16_t)data >> 11;

	mantissa *= 1000;

	if (exp < 0)
		mantissa >>= -exp;
	else
		mantissa <<= exp;	

	return mantissa;
}

static int crps_temp_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct crps_temp_config *config = dev->config;
	struct crps_temp_data *data = dev->data;
        uint16_t val;
	int result;

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL);

	result = mfd_crps_read_word(config->mfd, config->page, config->temp_reg, &val);

	if (result != 0) {
		return result;
	}

	result = val2data(dev, val);
//	LOG_DBG("%s Temp: %dmC", dev->name, result);

	data->temperature = result;

	return 0;
}

static int crps_temp_channel_get(const struct device *dev, enum sensor_channel chan,
				    struct sensor_value *val)
{
	struct crps_temp_data *data = dev->data;

	if (chan != SENSOR_CHAN_DIE_TEMP) {
		return -ENOTSUP;
	}

	val->val1 = data->temperature;
	val->val2 = 0;
	return 0;
}

static struct sensor_driver_api crps_temp_api = {
	.sample_fetch = crps_temp_sample_fetch,
	.channel_get = crps_temp_channel_get,
};

static int crps_temp_init(const struct device *dev)
{
	crps_temp_sample_fetch(dev, SENSOR_CHAN_ALL);
	return 0;
}

#define CRPS_TEMP_INIT(inst)                                                                       \
	static const struct crps_temp_config crps_temp_##inst##_config = {                         \
		.mfd = DEVICE_DT_GET(DT_INST_BUS(inst)),                                           \
		.page = DT_INST_PROP(inst, page),                                                  \
		.temp_reg = DT_INST_PROP(inst, temp_reg),                                          \
	};                                                                                         \
                                                                                                   \
	static struct crps_temp_data crps_temp_##inst##_data;                                      \
                                                                                                   \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, crps_temp_init, NULL,                                   \
			      &crps_temp_##inst##_data,                                            \
			      &crps_temp_##inst##_config, POST_KERNEL,                             \
			      81, &crps_temp_api);

DT_INST_FOREACH_STATUS_OKAY(CRPS_TEMP_INIT);
