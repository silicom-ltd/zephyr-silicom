/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT maestro_switch_temp

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>


LOG_MODULE_REGISTER(maestro_switch_temp, CONFIG_SENSOR_LOG_LEVEL);

struct maestro_switch_temp_config {
	const struct i2c_dt_spec i2c;
};

struct maestro_switch_temp_data {
       int temperature;
};

static int maestro_switch_temp_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	const struct maestro_switch_temp_config *config = dev->config;
	struct maestro_switch_temp_data *data = dev->data;
        uint8_t val;
	int result;

	if ((chan != SENSOR_CHAN_DIE_TEMP) && (chan != SENSOR_CHAN_ALL)) {
		return -ENOTSUP;
	}

	result = i2c_reg_read_byte_dt(&config->i2c, 0x1, &val);

	if (result != 0) {
		return result;
	}

	result *= 1000;

	LOG_DBG("%s Temp: %dmC", dev->name, val);

	data->temperature = val;

	return 0;
}

static int maestro_switch_temp_channel_get(const struct device *dev, enum sensor_channel chan,
			  		   struct sensor_value *val)
{
	struct maestro_switch_temp_data *data = dev->data;

	if (chan != SENSOR_CHAN_DIE_TEMP) {
		return -ENOTSUP;
	}

	val->val1 = (data->temperature / 1000);
	val->val2 = (data->temperature % 1000) * 1000;
	return 0;
}

static struct sensor_driver_api maestro_switch_temp_api = {
	.sample_fetch = maestro_switch_temp_sample_fetch,
	.channel_get = maestro_switch_temp_channel_get,
};

static int maestro_switch_temp_init(const struct device *dev)
{
	const struct maestro_switch_temp_config *config = dev->config;
	int result;
	uint8_t byte_value;

	result = i2c_reg_read_byte_dt(&config->i2c, 0xC, &byte_value);

	LOG_DBG("MAESTRO CPLD %s: 0x%x", dev->name, byte_value);

	result = maestro_switch_temp_sample_fetch(dev, SENSOR_CHAN_DIE_TEMP);

	return 0;
}

#define MAESTRO_SWITCH_TEMP_DEFINE(inst)                                                           \
        static struct maestro_switch_temp_data data_##inst;                                        \
	                                                                                           \
	static const struct maestro_switch_temp_config config##inst = {                            \
		.i2c = I2C_DT_SPEC_INST_GET(inst),						   \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, maestro_switch_temp_init, NULL, &data_##inst, &config##inst,   \
			      POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY, &maestro_switch_temp_api);

DT_INST_FOREACH_STATUS_OKAY(MAESTRO_SWITCH_TEMP_DEFINE);
