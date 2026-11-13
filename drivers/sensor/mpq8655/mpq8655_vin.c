/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT mps_mpq8655_vin

#include <zephyr/drivers/mfd/mpq8655.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(MPQ8655_VIN, CONFIG_SENSOR_LOG_LEVEL);

#define MPQ8655_VIN_MASK 0x3FF /* READ_VIN is 10 bits, unsigned */

struct mpq8655_vin_config {
	const struct device *mfd;
	uint8_t page;
};

struct mpq8655_vin_data {
	int voltage;
};

/*
 * READ_VIN (88h) is a plain unsigned count, fixed at 25mV/LSB (datasheet
 * Table 4), not PMBus LINEAR11 or DIRECT format with vendor-chosen m/b/R.
 */
static int val2data(uint16_t data)
{
	return (data & MPQ8655_VIN_MASK) * 25;
}

static int mpq8655_vin_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct mpq8655_vin_config *config = dev->config;
	struct mpq8655_vin_data *data = dev->data;
	uint16_t val;
	int ret;

	if ((chan != SENSOR_CHAN_VOLTAGE) && (chan != SENSOR_CHAN_ALL)) {
		return -ENOTSUP;
	}

	ret = mfd_mpq8655_read_word(config->mfd, config->page, PMBUS_READ_VIN, &val);
	if (ret != 0) {
		return ret;
	}

	data->voltage = val2data(val);

	LOG_DBG("%s vin: %dmV", dev->name, data->voltage);

	return 0;
}

static int mpq8655_vin_channel_get(const struct device *dev, enum sensor_channel chan,
				    struct sensor_value *val)
{
	struct mpq8655_vin_data *data = dev->data;

	if (chan != SENSOR_CHAN_VOLTAGE) {
		return -ENOTSUP;
	}

	val->val1 = data->voltage / 1000;
	val->val2 = (data->voltage % 1000) * 1000;

	return 0;
}

static const struct sensor_driver_api mpq8655_vin_api = {
	.sample_fetch = mpq8655_vin_sample_fetch,
	.channel_get = mpq8655_vin_channel_get,
};

static int mpq8655_vin_init(const struct device *dev)
{
	return mpq8655_vin_sample_fetch(dev, SENSOR_CHAN_ALL);
}

#define MPQ8655_VIN_INIT(inst)								\
	static const struct mpq8655_vin_config mpq8655_vin_##inst##_config = {		\
		.mfd = DEVICE_DT_GET(DT_INST_BUS(inst)),					\
		.page = DT_INST_PROP(inst, page),						\
	};											\
												\
	static struct mpq8655_vin_data mpq8655_vin_##inst##_data;				\
												\
	SENSOR_DEVICE_DT_INST_DEFINE(inst, mpq8655_vin_init, NULL,				\
				      &mpq8655_vin_##inst##_data,				\
				      &mpq8655_vin_##inst##_config, POST_KERNEL,		\
				      CONFIG_SENSOR_INIT_PRIORITY, &mpq8655_vin_api);

DT_INST_FOREACH_STATUS_OKAY(MPQ8655_VIN_INIT)
