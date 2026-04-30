/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT maxim_max31785_dts

#include <zephyr/device.h>
#include <zephyr/drivers/smbus.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/fan.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_instance.h>

#define MFR_TEMP_SENSOR_CONFIG	0xF0

/* max31785 page devices */
enum max31785_pages {
	MAX31785_DTS0 = 6,
	MAX31785_DTS1,
	MAX31785_DTS2,
	MAX31785_DTS3,
	MAX31785_DTS4,
	MAX31785_DTS5,
	MAX31785_INTERNAL_TEMP,
	MAX31785_I2C_0,
	MAX31785_I2C_1,
	MAX31785_I2C_2,
	MAX31785_I2C_3,
};

//LOG_MODULE_REGISTER(maxim_max31785_dts, CONFIG_SENSOR_LOG_LEVEL);
LOG_MODULE_REGISTER(maxim_max31785_dts, 4);

struct dts_max31785_common_config {
	const struct smbus_dt_spec smbus;
};

struct dts_max31785_data {
	uint16_t temp;
};

union mfr_temp_sensor_config {
	struct {
		uint16_t fans :6;
		uint16_t :4;
		uint16_t offset :5;
		uint16_t enable :1;
	};
	uint16_t raw;
};

struct dts_max31785_config {
	const struct smbus_dt_spec smbus;
	uint8_t page;
	int nfans;
	const int *fan_devs;
	LOG_INSTANCE_PTR_DECLARE(log);
};	

static int dts_max31785_temp_sample_fetch(const struct device *dev, enum sensor_channel)
{
	const struct dts_max31785_config *config = dev->config;
	struct dts_max31785_data *data = dev->data;

	uint16_t value;
	int ret;

	ret = pmbus_read_word_data(&config->smbus, config->page, 0, PMBUS_READ_TEMP_1, &value);

	if (ret)
		return -EINVAL;

	data->temp = value;
	
	return 0;
}

static int dts_max31785_temp_channel_get(const struct device *dev, enum sensor_channel, struct sensor_value *val)
{
	struct dts_max31785_data *data = dev->data;

	val->val1 = data->temp / 100;
	val->val2 = (data->temp % 100) * 100;

	return 0;
}

static const struct sensor_driver_api temp_api = {
	.sample_fetch = dts_max31785_temp_sample_fetch,
	.channel_get = dts_max31785_temp_channel_get,
};
	
static int dts_max31785_init(const struct device *dev)
{
	const struct dts_max31785_config *config = dev->config;
	union mfr_temp_sensor_config dts_config;
	int i,ret;
	const int *fan_devs = config->fan_devs;

	for (i = 0; i < config->nfans; i++) {
		if (fan_devs[i] == -1) break;
		dts_config.fans |= (1 << fan_devs[i]);
	}
		
	
	dts_config.enable = 1;

	LOG_INST_DBG(config->log, "MAX31785 DTS %s, %d config: 0x%x", dev->name, config->page, dts_config.raw);

	ret = pmbus_write_word_data(&config->smbus, config->page, MFR_TEMP_SENSOR_CONFIG, dts_config.raw);
	
	return ret;
}

static int dts_max31785_common_init(const struct device *dev)
{
	__unused const struct dts_max31785_common_config *config = dev->config;

	return 0;
}

#define FAN_PAGE(node_id, idx) \
	DT_PROP_BY_PHANDLE_IDX_OR(node_id, fans, idx, page, -1)

#define DTS_MAX31785_DEFINE(node_id, id, _source)								\
	static const int fans_##id[] = {FAN_PAGE(node_id, 0), FAN_PAGE(node_id, 1), FAN_PAGE(node_id, 2),	\
					FAN_PAGE(node_id, 3), FAN_PAGE(node_id, 4), FAN_PAGE(node_id, 5)};	\
	static struct dts_max31785_data dts_data_##id;								\
														\
	LOG_INSTANCE_REGISTER(_source, node_id, 4);						\
	static const struct dts_max31785_config dts_config_##id = {						\
		.smbus = SMBUS_DT_SPEC_GET(DT_GPARENT(node_id)),						\
		.page = _source,										\
		.nfans = ARRAY_SIZE(fans_##id),								\
		.fan_devs = fans_##id,	\
		LOG_INSTANCE_PTR_INIT(log, _source, node_id)							\
	};											   		\
														\
	DEVICE_DT_DEFINE(node_id, dts_max31785_init, NULL, &dts_data_##id, &dts_config_##id,			\
			 POST_KERNEL, CONFIG_DTS_MAXIM_MAX31785_INIT_PRIORITY, &temp_api);
			
#define DTS_MAX31785_DEFINE_COND(inst, child, source)						\
	COND_CODE_1(DT_NODE_EXISTS(DT_INST_CHILD(inst, child)),					\
		    (DTS_MAX31785_DEFINE(DT_INST_CHILD(inst, child), child##inst, source)),	\
		     ())

#define DTS_MAX31785_DEFINE_ALL(inst)										\
	static const struct dts_max31785_common_config common_config_##inst = {					\
		.smbus = SMBUS_DT_SPEC_GET(DT_INST_PARENT(inst)),						\
	};													\
														\
	DEVICE_DT_INST_DEFINE(inst, dts_max31785_common_init, NULL, NULL, &common_config_##inst,		\
			      POST_KERNEL, CONFIG_DTS_MAXIM_MAX31785_COMMON_INIT_PRIORITY, NULL);		\
														\
	DTS_MAX31785_DEFINE_COND(inst, dts0, MAX31785_DTS0)							\
	DTS_MAX31785_DEFINE_COND(inst, dts1, MAX31785_DTS1)							\
	DTS_MAX31785_DEFINE_COND(inst, dts2, MAX31785_DTS2)							\
	DTS_MAX31785_DEFINE_COND(inst, dts3, MAX31785_DTS3)							\
	DTS_MAX31785_DEFINE_COND(inst, dts4, MAX31785_DTS4)							\
	DTS_MAX31785_DEFINE_COND(inst, dts5, MAX31785_DTS5)							\
	DTS_MAX31785_DEFINE_COND(inst, die,  MAX31785_INTERNAL_TEMP)						\
	DTS_MAX31785_DEFINE_COND(inst, i2c0, MAX31785_I2C_0)							\
	DTS_MAX31785_DEFINE_COND(inst, i2c1, MAX31785_I2C_1)							\
	DTS_MAX31785_DEFINE_COND(inst, i2c2, MAX31785_I2C_2)							\
	DTS_MAX31785_DEFINE_COND(inst, i2c3, MAX31785_I2C_3)							\

DT_INST_FOREACH_STATUS_OKAY(DTS_MAX31785_DEFINE_ALL);
