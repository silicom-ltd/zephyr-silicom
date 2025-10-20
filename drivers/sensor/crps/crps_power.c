/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ocp_crps_power

#include <zephyr/drivers/mfd/crps.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(CRPS_POWER, CONFIG_SENSOR_LOG_LEVEL);

#define PMBUS_READ_PIN	0x97

struct crps_power_config {
	const struct device *mfd;
	uint8_t page;
};

struct crps_power_data {
	int power;
};

/* convert to microwatts */
static int val2data_linear(const struct device *dev, uint16_t val)
{
	int exp = (int16_t)val >> 11;
	int ret = ((int16_t)(val & 0x7FF) << 5) >> 5;

	ret *= 1000000;

	if (exp < 0) {
		ret >>= -exp;
	} else {
		ret <<= exp;
	}	

	return ret;
}

static int crps_power_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct crps_power_config *config = dev->config;
	struct crps_power_data *data = dev->data;
        uint16_t val;
	int result;

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL);

	result = mfd_crps_read_word(config->mfd, config->page, PMBUS_READ_PIN, &val);

	if (result != 0) {
		return result;
	}

	result = val2data_linear(dev, val);

	//LOG_DBG("%s Power: %duW", dev->name, result);

	data->power = result;

	return 0;
}

static int crps_power_channel_get(const struct device *dev, enum sensor_channel chan,
				    struct sensor_value *val)
{
	struct crps_power_data *data = dev->data;

	if (chan != SENSOR_CHAN_POWER) {
		return -ENOTSUP;
	}

	val->val1 = data->power;
	val->val2 = 0;
	return 0;
}

static struct sensor_driver_api crps_power_api = {
	.sample_fetch = crps_power_sample_fetch,
	.channel_get = crps_power_channel_get,
};

static int crps_power_init(const struct device *dev)
{
	crps_power_sample_fetch(dev, SENSOR_CHAN_ALL);

	return 0;
}

#define CRPS_POWER_INIT(inst)                                                                      \
	static const struct crps_power_config crps_power_##inst##_config = {                       \
		.mfd = DEVICE_DT_GET(DT_INST_BUS(inst)),                                           \
		.page = DT_INST_PROP(inst, page),                                                  \
	};                                                                                         \
                                                                                                   \
	static struct crps_power_data crps_power_##inst##_data;                                    \
                                                                                                   \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, crps_power_init, NULL,                                  \
			      &crps_power_##inst##_data,                                           \
			      &crps_power_##inst##_config, POST_KERNEL,                            \
			      81, &crps_power_api);

DT_INST_FOREACH_STATUS_OKAY(CRPS_POWER_INIT);
