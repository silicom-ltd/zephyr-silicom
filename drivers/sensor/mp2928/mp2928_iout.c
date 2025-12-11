/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT mps_mp2928_iout

#include <zephyr/drivers/mfd/mp2928.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(MP2928_IOUT, CONFIG_SENSOR_LOG_LEVEL);

#define MFR_VR_CONFIG1		0xC1

struct mp2928_iout_config {
	const struct device *mfd;
	uint8_t page;
};

struct mp2928_iout_data {
	int current;
	int m;
	int b;
	int R;
};

/* convert to millamps */
static int val2data_direct(const struct device *dev, uint16_t val)
{
	struct mp2928_iout_data *data = dev->data;

	int m = data->m;
	int b = data->b;
	int R = -(data->R);
	int ret = (int)val;

	R += 3;
	b *= 1000;
	
	while (R > 0) {
		ret *= 10;
		R--;
	}

	ret = (ret - b) / m;

	return ret;
}

static int mp2928_iout_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct mp2928_iout_config *config = dev->config;
	struct mp2928_iout_data *data = dev->data;
        uint16_t val;
	int result;

	if ((chan != SENSOR_CHAN_CURRENT) && (chan != SENSOR_CHAN_ALL)) {
		return -ENOTSUP;
	}

	result = mfd_mp2928_read_word(config->mfd, config->page, PMBUS_READ_IOUT, &val);

	if (result != 0) {
		return result;
	}

	val &= 0x7FF;

	result = val2data_direct(dev, val);

	LOG_DBG("%s iout: %dmV", dev->name, result);

	data->current = result;

	return 0;
}

static int mp2928_iout_channel_get(const struct device *dev, enum sensor_channel chan,
				    struct sensor_value *val)
{
	struct mp2928_iout_data *data = dev->data;

	if (chan != SENSOR_CHAN_CURRENT) {
		return -ENOTSUP;
	}

	val->val1 = (data->current / 1000);
	val->val2 = (data->current % 1000) * 1000;
	return 0;
}

static struct sensor_driver_api mp2928_iout_api = {
	.sample_fetch = mp2928_iout_sample_fetch,
	.channel_get = mp2928_iout_channel_get,
};

static int mp2928_iout_init(const struct device *dev)
{
	const struct mp2928_iout_config *config = dev->config;
	struct mp2928_iout_data *data = dev->data;
	int result;
	uint16_t val;

	result  = mfd_mp2928_read_word(config->mfd, 0, MFR_VR_CONFIG1, &val);

	LOG_DBG("MFR_VR_CONFIG1: 0x%x", val);

	val = !!(val & (0x8 << config->page));
	switch (val) {
		case 0x0:
			data->m = 2;
			data->R = 1;
			break;
		case 0x1:
			data->m = 4;
			data->R = 2;
			break;
		default:
			break;
	}

	mp2928_iout_sample_fetch(dev, SENSOR_CHAN_CURRENT);

	return 0;
}

#define MP2928_IOUT_INIT(inst)                                                                     \
	static const struct mp2928_iout_config mp2928_iout_##inst##_config = {                     \
		.mfd = DEVICE_DT_GET(DT_INST_BUS(inst)),                                           \
		.page = DT_INST_PROP(inst, page),                                                  \
	};                                                                                         \
                                                                                                   \
	static struct mp2928_iout_data mp2928_iout_##inst##_data;                                  \
                                                                                                   \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, mp2928_iout_init, NULL,                                 \
			      &mp2928_iout_##inst##_data,                                          \
			      &mp2928_iout_##inst##_config, POST_KERNEL,                           \
			      81, &mp2928_iout_api);

DT_INST_FOREACH_STATUS_OKAY(MP2928_IOUT_INIT);
