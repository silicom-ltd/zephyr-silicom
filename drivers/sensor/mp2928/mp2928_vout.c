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
	int Kr;
};

/* convert to millivolts */
static int val2data_direct(const struct device *dev, uint16_t val)
{
	struct mp2928_vout_data *data = dev->data;

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

	ret *= data->Kr;

	return ret;
}

static int mp2928_vout_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct mp2928_vout_config *config = dev->config;
	struct mp2928_vout_data *data = dev->data;
        uint16_t val;
	uint8_t byte;
	int direct_mode;
	int result;

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_VOLTAGE);

	result = mfd_mp2928_read_byte(config->mfd, 0, PMBUS_VOUT_MODE, &byte);
	if (result != 0)
		return result;

	direct_mode = !!(byte & 0x40);

	result = mfd_mp2928_read_word(config->mfd, config->page, PMBUS_READ_VOUT, &val);

	if (result != 0) {
		return result;
	}

	if (direct_mode) {
		result = val2data_direct(dev, val);
	}
	LOG_DBG("%s vout: %dmV", dev->name, result);

	data->voltage = result;

	return 0;
}

static int mp2928_vout_channel_get(const struct device *dev, enum sensor_channel chan,
				    struct sensor_value *val)
{
	struct mp2928_vout_data *data = dev->data;

	if (chan != SENSOR_CHAN_VOLTAGE) {
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
	struct mp2928_vout_data *data = dev->data;
	int result;
	uint16_t val;

	result  = mfd_mp2928_read_word(config->mfd, 0, MFR_VR_CONFIG1, &val);

	LOG_DBG("MFR_VR_CONFIG1: 0x%x", val);

	switch (val & 0xC0) {
		case 0x0:
			data->m = 16;
			data->R = 1;
			break;
		case 0x40:
			data->m = 2;
			data->R = 2;
			break;
		case 0x80:
			data->m = 5;
			data->R = 2;
		default:
			break;
	}

	result = mfd_mp2928_read_word(config->mfd, config->page, 0x29, &val);
	LOG_DBG("VOUT_SCALE_LOOP: 0x%x", val);
	data->Kr = (val & 0xFF) >> 5;

	mp2928_vout_sample_fetch(dev, SENSOR_CHAN_VOLTAGE);

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
