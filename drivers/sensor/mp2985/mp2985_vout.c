/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT mps_mp2985_vout

#include <zephyr/drivers/mfd/mp2985.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(MP2985_VOUT, CONFIG_SENSOR_LOG_LEVEL);

struct mp2985_vout_config {
	const struct device *mfd;
	uint8_t page;
};

struct mp2985_vout_data {
	int voltage;
	uint8_t mode;
	int m;
	int b;
	int R;
	int Kr;
};

/* convert to millivolts */
static int val2data_vid(const struct device *dev, uint16_t val)
{
	struct mp2985_vout_data *data = dev->data;

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
	ret /= 32;

	return ret;
}

static int mp2985_vout_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct mp2985_vout_config *config = dev->config;
	struct mp2985_vout_data *data = dev->data;
        uint16_t val;
	int result;

	if ((chan != SENSOR_CHAN_VOLTAGE) && (chan != SENSOR_CHAN_ALL)) {
		return -ENOTSUP;
	}

	result = mfd_mp2985_read_word(config->mfd, config->page, PMBUS_READ_VOUT, &val);

	if (result != 0) {
		return result;
	}

	result = val2data_vid(dev, val);

	LOG_DBG("%s vout: %dmV", dev->name, result);

	data->voltage = result;

	return 0;
}

static int mp2985_vout_channel_get(const struct device *dev, enum sensor_channel chan,
				    struct sensor_value *val)
{
	struct mp2985_vout_data *data = dev->data;

	if (chan != SENSOR_CHAN_VOLTAGE) {
		return -ENOTSUP;
	}

	val->val1 = (data->voltage / 1000);
	val->val2 = (data->voltage % 1000) * 1000;
	return 0;
}

static struct sensor_driver_api mp2985_vout_api = {
	.sample_fetch = mp2985_vout_sample_fetch,
	.channel_get = mp2985_vout_channel_get,
};

static int mp2985_vout_init(const struct device *dev)
{
	const struct mp2985_vout_config *config = dev->config;
	struct mp2985_vout_data *data = dev->data;
	int ret;
	uint16_t word_val;
	uint8_t byte_val;

	ret = mfd_mp2985_read_byte(config->mfd, config->page, PMBUS_VOUT_MODE, &byte_val);

	if (ret != 0)
		return -ENOTSUP;

	data->mode = byte_val;

	if (byte_val == 0x21) {
		if (config->page == 0) {
			ret = mfd_mp2985_read_word(config->mfd, 2, 0x0D, &word_val);
			if (word_val & 0x10) {
				data->m = 200;
				data->R = 3;
			} else {
				data->m = 100;
				data->R = 3;
			}
		}
		else {
			ret = mfd_mp2985_read_word(config->mfd, 2, 0x1D, &word_val);
			if (word_val & 0x8) {
				data->m = 200;
				data->R = 3;
			} else {
				data->m = 100;
				data->R = 3;
			}
		}
	}
	else if (byte_val == 0x40) {
		data->m = 1;
		data->R = 0;
	}
	else if (byte_val == 0x17) {
		data->m = 512;
		data->R = 3;
	}

	ret = mfd_mp2985_read_word(config->mfd, config->page, PMBUS_VOUT_SCALE_LOOP, &word_val);

	data->Kr = (word_val & 0xFF);

	mp2985_vout_sample_fetch(dev, SENSOR_CHAN_VOLTAGE);

	return 0;
}

#define MP2985_VOUT_INIT(inst)                                                                     \
	static const struct mp2985_vout_config mp2985_vout_##inst##_config = {                     \
		.mfd = DEVICE_DT_GET(DT_INST_BUS(inst)),                                           \
		.page = DT_INST_PROP(inst, page),                                                  \
	};                                                                                         \
                                                                                                   \
	static struct mp2985_vout_data mp2985_vout_##inst##_data;                                  \
                                                                                                   \
	SENSOR_DEVICE_DT_INST_DEFINE(inst, mp2985_vout_init, NULL,                                 \
			      &mp2985_vout_##inst##_data,                                          \
			      &mp2985_vout_##inst##_config, POST_KERNEL,                           \
			      81, &mp2985_vout_api);

DT_INST_FOREACH_STATUS_OKAY(MP2985_VOUT_INIT);
