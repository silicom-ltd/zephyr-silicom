/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT mps_mpq8655_vout

#include <zephyr/drivers/mfd/mpq8655.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(MPQ8655_VOUT, CONFIG_SENSOR_LOG_LEVEL);

#define MPQ8655_VOUT_MASK 0x1FFF /* READ_VOUT is 13 bits, unsigned */

struct mpq8655_vout_config {
	const struct device *mfd;
	uint8_t page;
};

struct mpq8655_vout_data {
	int voltage;
	uint16_t scale_loop;
};

/*
 * READ_VOUT (8Bh) is a plain unsigned count at a fixed 1.25mV/LSB (datasheet
 * Table 4) -- but that 1.25mV/LSB is the ADC's own resolution sampling VFB,
 * not VOUT: the ADC's input range is 0-1.28V over 10 bits (1.28V/1024 =
 * 1.25mV), the same range as the VFB/DAC reference, not VOUT's 0.4V-5.5V
 * range. VOUT_SCALE_LOOP (29h) is VFB/VOUT (1LSB = 0.001, e.g. default
 * 0x01F4 = 0.5), so recovering the real VOUT needs dividing the raw count's
 * VFB-referenced millivolts by that ratio.
 */
static int val2data(uint16_t data, uint16_t scale_loop)
{
	return ((data & MPQ8655_VOUT_MASK) * 1250) / scale_loop;
}

static int mpq8655_vout_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct mpq8655_vout_config *config = dev->config;
	struct mpq8655_vout_data *data = dev->data;
	uint16_t val;
	int ret;

	if ((chan != SENSOR_CHAN_VOLTAGE) && (chan != SENSOR_CHAN_ALL)) {
		return -ENOTSUP;
	}

	ret = mfd_mpq8655_read_word(config->mfd, config->page, PMBUS_READ_VOUT, &val);
	if (ret != 0) {
		return ret;
	}

	data->voltage = val2data(val, data->scale_loop);

	LOG_DBG("%s vout: %dmV", dev->name, data->voltage);

	return 0;
}

static int mpq8655_vout_channel_get(const struct device *dev, enum sensor_channel chan,
				     struct sensor_value *val)
{
	struct mpq8655_vout_data *data = dev->data;

	if (chan != SENSOR_CHAN_VOLTAGE) {
		return -ENOTSUP;
	}

	val->val1 = data->voltage / 1000;
	val->val2 = (data->voltage % 1000) * 1000;

	return 0;
}

static const struct sensor_driver_api mpq8655_vout_api = {
	.sample_fetch = mpq8655_vout_sample_fetch,
	.channel_get = mpq8655_vout_channel_get,
};

static int mpq8655_vout_init(const struct device *dev)
{
	const struct mpq8655_vout_config *config = dev->config;
	struct mpq8655_vout_data *data = dev->data;
	int ret;

	ret = mfd_mpq8655_read_word(config->mfd, config->page, PMBUS_VOUT_SCALE_LOOP,
				     &data->scale_loop);
	if (ret != 0) {
		return ret;
	}

	LOG_DBG("%s VOUT_SCALE_LOOP: 0x%x", dev->name, data->scale_loop);

	return mpq8655_vout_sample_fetch(dev, SENSOR_CHAN_ALL);
}

#define MPQ8655_VOUT_INIT(inst)								\
	static const struct mpq8655_vout_config mpq8655_vout_##inst##_config = {		\
		.mfd = DEVICE_DT_GET(DT_INST_BUS(inst)),					\
		.page = DT_INST_PROP(inst, page),						\
	};											\
												\
	static struct mpq8655_vout_data mpq8655_vout_##inst##_data;				\
												\
	SENSOR_DEVICE_DT_INST_DEFINE(inst, mpq8655_vout_init, NULL,				\
				      &mpq8655_vout_##inst##_data,				\
				      &mpq8655_vout_##inst##_config, POST_KERNEL,		\
				      CONFIG_SENSOR_INIT_PRIORITY, &mpq8655_vout_api);

DT_INST_FOREACH_STATUS_OKAY(MPQ8655_VOUT_INIT)
