/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT maxim_max31785_fan

#include <zephyr/device.h>
#include <zephyr/drivers/smbus.h>
#include <zephyr/drivers/mfd/max31785.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/fan.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_instance.h>

#define MFR_FAN_CONFIG	0xF1
#define MFR_FAN_LUT	0xF2

/* max31785 page devices */
enum fan_max31785_pages {
	MAX31785_FAN0 = 0,
	MAX31785_FAN1,
	MAX31785_FAN2,
	MAX31785_FAN3,
	MAX31785_FAN4,
	MAX31785_FAN5,
};

enum max31785_fan_mode {
	FAN_AUTO_PWM,
	FAN_AUTO_RPM,
	FAN_MANUAL_PWM,
	FAN_MANUAL_RPM,
};

LOG_MODULE_REGISTER(maxim_max31785_fan, CONFIG_PWM_LOG_LEVEL);

struct fan_max31785_common_config {
	const struct smbus_dt_spec smbus;
};

struct fan_max31785_common_data {
	struct k_mutex lock;
};

#define MAX31785_FAN_MODE_PWM		0
#define MAX31785_FAN_MODE_RPM		1

#define MAX31785_FAN_SPINUP_DISABLE	0
#define MAX31785_FAN_SPINUP_2REV	1
#define MAX31785_FAN_SPINUP_4REV	2
#define MAX31785_FAN_SPINUP_8REV	3

#define MAX31785_FAN_LOCKED_ROTOR_EN	1
#define MAX31785_FAN_STUCK_LEVEL_LOW	0
#define MAX31785_FAN_STUCK_LEVEL_HIGH	1
#define MAX31785_FAN_HEALTH_DIS		0
#define MAX31785_FAN_HEALTH_EN		1

#define MAX31785_FAN_RAMP_1PERCENT_SLOW		0	/* slow is update every 1000ms */
#define MAX31785_FAN_RAMP_2PERCENT_SLOW		1
#define MAX31785_FAN_RAMP_3PERCENT_SLOW		2
#define MAX31785_FAN_RAMP_1PERCENT_FAST		3	/* fast is update every 200ms */
#define MAX31785_FAN_RAMP_2PERCENT_FAST		4
#define MAX31785_FAN_RAMP_3PERCENT_FAST		5
#define MAX31785_FAN_RAMP_4PERCENT_FAST		6
#define MAX31785_FAN_RAMP_5PERCENT_FAST		7

#define MAX31785_FAN_TACH_OVERRIDE_EN		0	/* fan @ 100% if fault detected */
#define MAX31785_FAN_TACH_OVERRIDE_DIS		1	/* fan @ last if fault detected */
#define MAX31785_FAN_TEMP_FAULT_OVERRIDE_EN	0
#define MAX31785_FAN_TEMP_FAULT_OVERRIDE_DIS	1

#define MAX31785_FAN_TEMP_HYST_2DEG		0
#define MAX31785_FAN_TEMP_HYST_4DEG		1
#define MAX31785_FAN_TEMP_HYST_6DEG		2
#define MAX31785_FAN_TEMP_HYST_8DEG		3

#define MAX37185_FAN_DUAL_TACH_DIS		0
#define MAX37185_FAN_DUAL_TACH_EN		1

#define MAX31785_FAN_PWM_FREQUENCY_30HZ		0
#define MAX31785_FAN_PWM_FREQUENCY_50HZ		1
#define MAX31785_FAN_PWM_FREQUENCY_100HZ	2
#define MAX31785_FAN_PWM_FREQUENCY_150HZ	3
#define MAX31785_FAN_PWM_FREQUENCY_25KHZ	7

union mfr_fan_config {
	struct {
		uint16_t spinup :2;
		uint16_t rotor_output :1;
		uint16_t rotor_stuck_hi :1;
		uint16_t health_en :1;
		uint16_t ramp_rate :3;
		uint16_t tacho :1;
		uint16_t tsfo :1;
		uint16_t hyst :2;
		uint16_t dual_tach_en :1;
		uint16_t frequency :3;
	};
	uint16_t raw;
}; 

union fan_config_1_2 {
	struct {
		uint8_t :4;
		uint8_t ppr :2;
		uint8_t mode :1;
		uint8_t enable :1;
	};
	uint8_t raw;
};

struct fan_max31785_data {
	uint16_t count;
};

struct fan_max31785_config {
	uint8_t page;
	const struct smbus_dt_spec smbus;
	const struct device *mfd;
	union mfr_fan_config fan_config;
	union fan_config_1_2 fan;
	uint16_t fan_speeds[8];
	uint16_t fan_temps[8];
	uint16_t initial_setting;
	enum max31785_fan_mode mode;

	LOG_INSTANCE_PTR_DECLARE(log);
};

#if 0
static int max31785_get_cycles_per_sec(const struct device *dev, uint32_t channel, uint64_t *cycles)
{
	__unused const struct fan_max31785_config *config = dev->config;
	__unused struct max31785_data *data = dev->data;
	__unused uint16_t value;

	__unused int result;

	return 0;
}

static const struct pwm_driver_api max31785_pwm_api = {
	.set_cycles = max31785_set_cycles,
	.get_cycles_per_sec = max31785_get_cycles_per_sec,
};
#endif

static int fan_max31785_speed_get(const struct device *dev, struct sensor_value *value)
{
	const struct fan_max31785_config *config = dev->config;

	uint16_t val;
	int ret;

	ret = mfd_max31785_read_word(config->mfd, config->page, PMBUS_READ_FAN_SPEED_1, &val);

	if (ret) return -EINVAL;

	value->val1 = val;

	return 0;
}

static int fan_max31785_cycles_set(const struct device *dev, unsigned int cycles)
{
	const struct fan_max31785_config *config = dev->config;
	int ret = 0;

	if (config->mode == FAN_MANUAL_PWM) {
		if (cycles > 100) return -EINVAL;
		ret = mfd_max31785_write_word(config->mfd, config->page, PMBUS_FAN_COMMAND_1, cycles*100);
	}

	if (config->mode == FAN_MANUAL_RPM) {
		if (cycles > 0x7FFF) return -EINVAL;
		ret = mfd_max31785_write_word(config->mfd, config->page, PMBUS_FAN_COMMAND_1, cycles);
	}

	if (ret) return -EINVAL;

	return 0;
}

#if 0
static int fan_max31785_speed_sample_fetch(const struct device *dev, enum sensor_channel)
{
	const struct fan_max31785_config *config = dev->config;
	struct fan_max31785_data *data = dev->data;

	uint16_t value;
	int ret;

	ret = pmbus_read_word_data(&config->smbus, config->page, 0, PMBUS_READ_FAN_SPEED_1, &value);

	if (ret)
		return -EINVAL;

	data->count = value;
	
	return 0;
}

static int fan_max31785_speed_channel_get(const struct device *dev, enum sensor_channel, struct sensor_value *val)
{
	struct fan_max31785_data *data = dev->data;

	val->val1 = data->count;
	val->val2 = 0U;

	return 0;
}

#endif
static const struct fan_parent_driver_api fan_api = {
	.get_speed = fan_max31785_speed_get,
	.set_cycles = fan_max31785_cycles_set,
};
#if 0
static const struct sensor_driver_api fan_api = {
	.sample_fetch = fan_max31785_speed_sample_fetch,
	.channel_get = fan_max31785_speed_channel_get,
};
#endif
	
static int fan_max31785_init(const struct device *dev)
{
	const struct fan_max31785_config *config = dev->config;
	int i,j;
	uint16_t fan_lut[16];
	int ret;

	LOG_INST_DBG(config->log, "cfg reg: 0x%x",config->fan_config.raw);
	ret = mfd_max31785_write_word(config->mfd, config->page, MFR_FAN_CONFIG, config->fan_config.raw);

	if (ret) {
		LOG_INST_DBG(config->log, "fan register write failed");
	}	
	
	if ((config->mode == FAN_AUTO_PWM) || (config->mode == FAN_AUTO_RPM)) {
		j = 0;
		for (i = 0; i < 16; i+=2) {
			fan_lut[i] = config->fan_temps[j]*100;
			fan_lut[i+1] = config->fan_speeds[j]*100;
			j++;
		}

		ret = mfd_max31785_write_block(config->mfd, config->page, MFR_FAN_LUT, 32, (uint8_t *)fan_lut);

		if (ret)
			LOG_INST_DBG(config->log, "MAX31785 fan write lut failed");

		ret = mfd_max31785_write_word(config->mfd, config->page, PMBUS_FAN_COMMAND_1, 0xFFFF);

		if (ret)
			LOG_INST_DBG(config->log, "MAX31785 FAN_COMMAND_1 write failed");

	}
	else if (config->initial_setting) {
			ret = mfd_max31785_write_word(config->mfd, config->page, PMBUS_FAN_COMMAND_1, config->initial_setting);
			if (ret)
				LOG_INST_DBG(config->log, "MAX31785 FAN_COMMAND_1 manual write failed");
			else
				LOG_INST_DBG(config->log, "MAX31785 FAN_COMMAND_1 manual write %d", config->initial_setting);
	}

	LOG_INST_DBG(config->log, "fan mode: 0x%x",config->mode);
	switch (config->mode) {
		case FAN_AUTO_RPM:
		case FAN_MANUAL_RPM:
			LOG_INST_DBG(config->log, "fan reg: 0x%x",config->fan.raw|(1<<6));
			ret = mfd_max31785_write_byte(config->mfd, config->page, PMBUS_FAN_CONFIG_12, config->fan.raw | (1 << 6));
			break;
		default:
			LOG_INST_DBG(config->log, "fan reg: 0x%x",config->fan.raw);
			ret = mfd_max31785_write_byte(config->mfd, config->page, PMBUS_FAN_CONFIG_12, config->fan.raw);
	}
	if (ret)
			LOG_INST_DBG(config->log, "MAX31785 FAN_COMMAND_1 write failed");

	return ret;
}

static int fan_max31785_common_init(const struct device *dev)
{
	__unused const struct fan_max31785_common_config *config = dev->config;
	struct fan_max31785_common_data *data = dev->data;

	k_mutex_init(&data->lock);

	return 0;
}

#define FAN_MAX31785_DEFINE(node_id, id, _source)								\
	static struct fan_max31785_data fan_data_##id;								\
														\
	LOG_INSTANCE_REGISTER(_source, node_id, 4);								\
	static const struct fan_max31785_config fan_config_##id = {						\
		.smbus = SMBUS_DT_SPEC_GET(DT_GPARENT(node_id)),						\
		.mfd = DEVICE_DT_GET(DT_BUS(node_id)),							\
		.page = _source,										\
		.fan.ppr = DT_PROP_OR(node_id, ppr, 2) - 1,							\
		.fan.enable = 1,							 			\
		.fan_config.frequency = DT_PROP_OR(node_id, frequency, MAX31785_FAN_PWM_FREQUENCY_25KHZ),	\
		.fan_config.hyst = DT_PROP_OR(node_id, hyst, MAX31785_FAN_TEMP_HYST_2DEG),			\
		.fan_config.tsfo = DT_PROP_OR(node_id, tsfo, MAX31785_FAN_TEMP_FAULT_OVERRIDE_DIS),		\
		.fan_config.tacho = DT_PROP_OR(node_id, tacho, MAX31785_FAN_TACH_OVERRIDE_DIS),			\
		.fan_config.ramp_rate = DT_PROP_OR(node_id, ramp_rate, MAX31785_FAN_RAMP_5PERCENT_FAST),	\
		.fan_config.health_en = DT_PROP_OR(node_id, health_en, MAX31785_FAN_HEALTH_DIS),		\
		.fan_config.rotor_stuck_hi = DT_PROP_OR(node_id, rotor_hi_lo, MAX31785_FAN_STUCK_LEVEL_LOW),	\
		.fan_config.rotor_output = DT_PROP_OR(node_id, locked_rotor_en, 0),				\
		.fan_config.spinup = DT_PROP_OR(node_id, spinup, 0),			   			\
		.fan_speeds = DT_PROP_OR(node_id, fan_speeds, {0}),						\
		.fan_temps = DT_PROP_OR(node_id, fan_temps, {0}),						\
		.mode = DT_STRING_UPPER_TOKEN(node_id, mode),							\
		.initial_setting = DT_PROP_OR(node_id, initial_setting, 0) * 100,				\
		LOG_INSTANCE_PTR_INIT(log, _source, node_id)							\
	};											   		\
														\
														\
	DEVICE_DT_DEFINE(node_id, fan_max31785_init, NULL, &fan_data_##id, &fan_config_##id,			\
			 POST_KERNEL, CONFIG_FAN_MAXIM_MAX31785_INIT_PRIORITY, &fan_api);
			
#define FAN_MAX31785_DEFINE_COND(inst, child, source)								\
	COND_CODE_1(DT_NODE_EXISTS(DT_INST_CHILD(inst, child)),							\
		    (FAN_MAX31785_DEFINE(DT_INST_CHILD(inst, child), child##inst, source)),			\
		     ())

#define FAN_MAX31785_DEFINE_ALL(inst)										\
	static const struct fan_max31785_common_config common_config_##inst = {					\
		.smbus = SMBUS_DT_SPEC_GET(DT_INST_PARENT(inst)),						\
	};													\
														\
	DEVICE_DT_INST_DEFINE(inst, fan_max31785_common_init, NULL, NULL, &common_config_##inst,		\
			      POST_KERNEL, CONFIG_FAN_MAXIM_MAX31785_COMMON_INIT_PRIORITY, NULL);		\
														\
	FAN_MAX31785_DEFINE_COND(inst, fan0, MAX31785_FAN0)							\
	FAN_MAX31785_DEFINE_COND(inst, fan1, MAX31785_FAN1)							\
	FAN_MAX31785_DEFINE_COND(inst, fan2, MAX31785_FAN2)							\
	FAN_MAX31785_DEFINE_COND(inst, fan3, MAX31785_FAN3)							\
	FAN_MAX31785_DEFINE_COND(inst, fan4, MAX31785_FAN4)							\
	FAN_MAX31785_DEFINE_COND(inst, fan5, MAX31785_FAN5)

DT_INST_FOREACH_STATUS_OKAY(FAN_MAX31785_DEFINE_ALL)
