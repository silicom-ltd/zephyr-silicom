/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT mps_mpq8655_temp

#include <zephyr/drivers/mfd/mpq8655.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(MPQ8655_TEMP, CONFIG_SENSOR_LOG_LEVEL);

#define MPQ8655_TEMP_SIGN_BIT  BIT(9)
#define MPQ8655_TEMP_MAG_MASK  0x1FF /* bits[8:0] */

struct mpq8655_temp_config {
	const struct device *mfd;
	uint8_t page;
};

struct mpq8655_temp_data {
	int temp_c;
};

/*
 * READ_TEMPERATURE_1 (8Dh) is NOT two's complement: bit[9] is a separate
 * sign bit and bits[8:0] are the magnitude (datasheet Table 7, e.g.
 * 1 0000 0001 = -1, not -511). 1 degree C/LSB, no fractional part.
 */
static int val2data(uint16_t data)
{
	int magnitude = data & MPQ8655_TEMP_MAG_MASK;

	return (data & MPQ8655_TEMP_SIGN_BIT) ? -magnitude : magnitude;
}

static int mpq8655_temp_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct mpq8655_temp_config *config = dev->config;
	struct mpq8655_temp_data *data = dev->data;
	uint16_t val;
	int ret;

	if ((chan != SENSOR_CHAN_DIE_TEMP) && (chan != SENSOR_CHAN_ALL)) {
		return -ENOTSUP;
	}

	ret = mfd_mpq8655_read_word(config->mfd, config->page, PMBUS_READ_TEMP_1, &val);
	if (ret != 0) {
		return ret;
	}

	data->temp_c = val2data(val);

	LOG_DBG("%s temp: %dC", dev->name, data->temp_c);

	return 0;
}

static int mpq8655_temp_channel_get(const struct device *dev, enum sensor_channel chan,
				     struct sensor_value *val)
{
	struct mpq8655_temp_data *data = dev->data;

	if (chan != SENSOR_CHAN_DIE_TEMP) {
		return -ENOTSUP;
	}

	val->val1 = data->temp_c;
	val->val2 = 0;

	return 0;
}

static const struct sensor_driver_api mpq8655_temp_api = {
	.sample_fetch = mpq8655_temp_sample_fetch,
	.channel_get = mpq8655_temp_channel_get,
};

static int mpq8655_temp_init(const struct device *dev)
{
	return mpq8655_temp_sample_fetch(dev, SENSOR_CHAN_ALL);
}

#define MPQ8655_TEMP_INIT(inst)								\
	static const struct mpq8655_temp_config mpq8655_temp_##inst##_config = {		\
		.mfd = DEVICE_DT_GET(DT_INST_BUS(inst)),					\
		.page = DT_INST_PROP(inst, page),						\
	};											\
												\
	static struct mpq8655_temp_data mpq8655_temp_##inst##_data;				\
												\
	SENSOR_DEVICE_DT_INST_DEFINE(inst, mpq8655_temp_init, NULL,				\
				      &mpq8655_temp_##inst##_data,				\
				      &mpq8655_temp_##inst##_config, POST_KERNEL,		\
				      CONFIG_SENSOR_INIT_PRIORITY, &mpq8655_temp_api);

DT_INST_FOREACH_STATUS_OKAY(MPQ8655_TEMP_INIT)
