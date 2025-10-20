/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT mps_mp2928_vout

#include <zephyr/drivers/mfd/mp2928.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(MP2928_VOUT, CONFIG_SENSOR_LOG_LEVEL);

#define MFR_VR_CONFIG1		0xC1

struct mp2928_vout_config {
	const struct device *mfd;
	uint8_t page;
};

struct mp2928_vout_data {
	int voltage;
	int m;
	int b;
	int R;
};

/* convert to microwatts */
static int val2data_direct(const struct device *dev, uint16_t val)
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

static int mp2928_vout_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct mp2928_vout_config *config = dev->config;
	struct mp2928_vout_data *data = dev->data;
        uint16_t val;
	uint8_t byte;
	int result;

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL);

	result = mfd_mp2928_read_word(config->mfd, config->page, PMBUS_READ_VOUT, &val);

	if (result != 0) {
		return result;
	}

	result = val;
	LOG_DBG("%s vout: 0x%x", dev->name, result);

	result = mfd_mp2928_read_byte(config->mfd, config->page, PMBUS_VOUT_MODE, &byte);
	LOG_DBG("%s vout_mode: 0x%x", dev->name, byte);

	result = mfd_mp2928_read_word(config->mfd, config->page, 0x29, &val);
	LOG_DBG("%s vout_scale_loop: 0x%x", dev->name, val);

	data->voltage = result;

	return 0;
}

static int mp2928_vout_channel_get(const struct device *dev, enum sensor_channel chan,
				    struct sensor_value *val)
{
	struct mp2928_vout_data *data = dev->data;

	if (chan != SENSOR_CHAN_POWER) {
		return -ENOTSUP;
	}

	val->val1 = data->voltage;
	val->val2 = 0;
	return 0;
}

static struct sensor_driver_api mp2928_vout_api = {
	.sample_fetch = mp2928_vout_sample_fetch,
	.channel_get = mp2928_vout_channel_get,
};

static int mp2928_vout_init(const struct device *dev)
{
	const struct mp2928_vout_config *config = dev->config;
	int result;
	uint16_t val;

	result  = mfd_mp2928_read_word(config->mfd, config->page, MRF_VR_CONFIG1, &val);

	switch (val & 0xC0) {
		case 0x0:
			m = 160;
			break;
		case 0x40:
			m = 200;
			break;
		case 0x80:
			m = 500;
		default:
			break;
	}

	mp2928_vout_sample_fetch(dev, SENSOR_CHAN_ALL);

	return 0;
}

#define MP2928_VOUT_INIT(inst)                                                                     \
	static const struct mp2928_vout_config mp2928_vout_##inst##_config = {                     \
		.mfd = DEVICE_DT_GET(DT_INST_BUS(inst)),                                           \
		.page = DT_INST_PROP(inst, page),                                                  \
	};                                                                                         \
                                                                                                   \
	static struct mp2928_vout_data mp2928_vout_##inst##_data;                                  \
                                                                                                   \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, mp2928_vout_init, NULL,                                 \
			      &mp2928_vout_##inst##_data,                                          \
			      &mp2928_vout_##inst##_config, POST_KERNEL,                           \
			      81, &mp2928_vout_api);

DT_INST_FOREACH_STATUS_OKAY(MP2928_VOUT_INIT);
