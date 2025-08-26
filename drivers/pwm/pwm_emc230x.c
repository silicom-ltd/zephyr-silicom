/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT microchip_emc230x_pwm

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/mfd/emc230x.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/pwm/emc230x.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/fan.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pwm_emc230x, CONFIG_PWM_LOG_LEVEL);

#define EMC230X_PWMTARGETDUTYCYCLE_MAXIMUM 255

struct fan_config {
	const struct pwm_dt_spec *pwm;
	const struct device *mfd;
	const struct device *tach;
	uint8_t edges;
	uint8_t initial;
};

struct emc230x_pwm_config {
	const struct device *mfd;
};

struct emc230x_pwm_data {
	uint8_t max_channels;
};


#if 0
static void max31790_set_fandynamics_speedrange(uint8_t *destination, uint8_t value)
{
	uint8_t length = MAX37190_FANXDYNAMICS_SPEEDRANGE_LENGTH;
	uint8_t pos = MAX37190_FANXDYNAMICS_SPEEDRANGE_POS;

	*destination &= ~GENMASK(pos + length - 1, pos);
	*destination |= FIELD_PREP(GENMASK(pos + length - 1, pos), value);
}

static void max31790_set_fandynamics_pwmrateofchange(uint8_t *destination, uint8_t value)
{
	uint8_t length = MAX37190_FANXDYNAMICS_PWMRATEOFCHANGE_LENGTH;
	uint8_t pos = MAX37190_FANXDYNAMICS_PWMRATEOFCHANGE_POS;

	*destination &= ~GENMASK(pos + length - 1, pos);
	*destination |= FIELD_PREP(GENMASK(pos + length - 1, pos), value);
}
#endif

static void emc230x_set_pwmfrequency(uint8_t *destination, uint8_t channel, uint8_t value)
{
	uint8_t length = EMC230X_PWMFREQUENCY_PWM_LENGTH;
	uint8_t pos = (channel % 4) * 2;

	*destination &= ~GENMASK(pos + length - 1, pos);
	*destination |= FIELD_PREP(GENMASK(pos + length - 1, pos), value);
}

static uint8_t emc230x_get_pwmfrequency(uint8_t value, uint8_t channel)
{
	uint8_t length = EMC230X_PWMFREQUENCY_PWM_LENGTH;
	uint8_t pos = (channel % 4) * 2;

	return FIELD_GET(GENMASK(pos + length - 1, pos), value);
}

static void emc230x_set_fan_spinup(uint8_t *destination, uint8_t value)
{
	uint8_t length = EMC230X_FANSPIN_SPINUP_LENGTH;
	uint8_t pos = EMC230X_FANSPIN_SPINUP_POS;

	*destination &= ~GENMASK(pos + length - 1, pos);
	*destination |= FIELD_PREP(GENMASK(pos + length - 1, pos), value);
}

#if 0
static int emc230x_write_register_uint16(const struct device *dev, uint8_t address, uint16_t value)
{
	const struct emc230x_pwm_config *config = dev->config;
	int result;
	uint8_t buffer[] = {
		address,
		value >> 8,
		value,
	};

	LOG_DBG("writing value 0x%04X to address 0x%02X", value, address);
	result = i2c_write_dt(&config->i2c, buffer, sizeof(buffer));
	if (result != 0) {
		LOG_ERR("unable to write to address 0x%02X, error %i", address, result);
	}

	return result;
}
#endif

static bool emc230x_convert_pwm_frequency_into_hz(uint16_t *result, uint8_t pwm_frequency)
{
	switch (pwm_frequency) {
	case 0:
		*result = 26000;
		return true;
	case 1:
		*result = 19530;
		return true;
	case 2:
		*result = 4882;
		return true;
	case 3:
		*result = 2441;
		return true;
	default:
		LOG_ERR("invalid value %i for PWM frequency register", pwm_frequency);
		return false;
	}
}

static bool emc230x_convert_pwm_frequency_into_register(uint8_t *result, uint32_t pwm_frequency)
{
	switch (pwm_frequency) {
	case 26000:
		*result = 0;
		return true;
	case 19530:
		*result = 1;
		return true;
	case 4882:
		*result = 2;
		return true;
	case 2441:
		*result = 3;
		return true;
	default:
		LOG_ERR("invalid value %i for PWM frequency in Hz", pwm_frequency);
		return false;
	}
}

static int emc230x_set_cycles_internal(const struct device *dev, uint32_t channel,
				       uint32_t period_count, uint32_t pulse_count,
				       pwm_flags_t flags)
{
	const struct emc230x_pwm_config *config = dev->config;
	int result;
	uint8_t pwm_frequency_channel_value;
	uint8_t value_pwm_frequency;
	uint8_t value_fan_configuration;

	if (!emc230x_convert_pwm_frequency_into_register(&pwm_frequency_channel_value,
							 period_count)) {
		return -EINVAL;
	}

	result = mfd_emc230x_reg_read(config->mfd, EMC230X_REGISTER_PWMFREQUENCY(channel), &value_pwm_frequency);
	if (result != 0) {
		return result;
	}

	emc230x_set_pwmfrequency(&value_pwm_frequency, channel, pwm_frequency_channel_value);

	if (flags & PWM_EMC230X_FLAG_SPIN_UP) {
		emc230x_set_fan_spinup(&value_fan_configuration, 2);
	} else {
		emc230x_set_fan_spinup(&value_fan_configuration, 0);
	}

	uint16_t pwm_target_duty_cycle = pulse_count * EMC230X_PWMTARGETDUTYCYCLE_MAXIMUM / 100;

	result = mfd_emc230x_reg_write(config->mfd, 
				       EMC230X_REGISTER_FANDRIVESETTING(channel),
				       pwm_target_duty_cycle);
	return result;
}

static int emc230x_set_cycles(const struct device *dev, uint32_t channel, uint32_t period_count,
			      uint32_t pulse_count, pwm_flags_t flags)
{
	struct emc230x_pwm_data *data = dev->data;

	LOG_DBG("set period %i with pulse %i for channel %i and flags 0x%04X", period_count,
		pulse_count, channel, flags);

	if (channel > data->max_channels-1) {
		LOG_ERR("invalid channel number %i", channel);
		return -EINVAL;
	}

	if (period_count == 0) {
		LOG_ERR("period count must be > 0");
		return -EINVAL;
	}

	return emc230x_set_cycles_internal(dev, channel, period_count, pulse_count, flags);
}

static int emc230x_get_cycles_per_sec(const struct device *dev, uint32_t channel, uint64_t *cycles)
{
	const struct emc230x_pwm_config *config = dev->config;
	struct emc230x_pwm_data *data = dev->data;
	int result;
	bool success;
	uint8_t pwm_frequency_register;
	uint8_t pwm_frequency = 1;
	uint16_t pwm_frequency_in_hz;

	if (channel > data->max_channels-1) {
		LOG_ERR("invalid channel number %i", channel);
		return -EINVAL;
	}

	result = mfd_emc230x_reg_read(config->mfd, EMC230X_REGISTER_PWMFREQUENCY(channel),
				      &pwm_frequency_register);

	if (result != 0) {
		return result;
	}

	pwm_frequency = emc230x_get_pwmfrequency(pwm_frequency_register, channel);
	success = emc230x_convert_pwm_frequency_into_hz(&pwm_frequency_in_hz, pwm_frequency);

	if (!success) {
		return -EINVAL;
	}

	*cycles = pwm_frequency_in_hz;

	return 0;
}

static int set_fan_config(const struct device *dev)
{
	const struct fan_config *config = dev->config;
	const struct pwm_dt_spec *pwm = config->pwm;

	ARG_UNUSED(pwm);
	ARG_UNUSED(config);

	return 0;
}

static int set_fan_cycles(const struct device *dev, uint32_t pulse_count)
{
	const struct fan_config *config = dev->config;
	const struct pwm_dt_spec *pwm = config->pwm;

	ARG_UNUSED(config);

	return pwm_set_pulse_dt(pwm, pulse_count);
}

static int get_fan_speed(const struct device *dev, struct sensor_value *val)
{
	const struct fan_config *config = dev->config;
	const struct device *tach = config->tach;

	int ret;

	ret = sensor_sample_fetch(tach);

	if (ret)
		return ret;

	ret = sensor_channel_get(tach, SENSOR_CHAN_RPM, val);

	if (ret)
		return ret;

	return ret;
}

static int fan_init(const struct device *dev)
{
	const struct fan_config *config = dev->config;
	const struct pwm_dt_spec *pwm = config->pwm;
	int ret;

	ret = mfd_emc230x_reg_write(config->mfd, EMC230X_REGISTER_FANCONFIGURATION(pwm->channel), 0x8); // set edges to 5
	ret = mfd_emc230x_reg_write(config->mfd, EMC230X_REGISTER_FAN2CONFIGURATION(pwm->channel), 0x8); // basic der opts
	ret = mfd_emc230x_reg_write(config->mfd, EMC230X_REGISTER_FANSPIN(pwm->channel), (1 << 5)); // no kick
	ret = mfd_emc230x_reg_write(config->mfd, EMC230X_REGISTER_FANDRIVESETTING(pwm->channel), (255 * config->initial)/100);

	return 0;
}

static const struct fan_parent_driver_api fan_api = {
	.set_config = set_fan_config,
	.set_cycles = set_fan_cycles,
	.get_speed =  get_fan_speed,
};

static const struct pwm_driver_api emc230x_pwm_api = {
	.set_cycles = emc230x_set_cycles,
	.get_cycles_per_sec = emc230x_get_cycles_per_sec,
};

static int emc230x_pwm_init(const struct device *dev)
{
	const struct emc230x_pwm_config *config = dev->config;
	struct emc230x_pwm_data *data = dev->data;
	int result;
	uint8_t reg_value;
	uint8_t value;

	LOG_DBG("%s",__func__);

	result = mfd_emc230x_reg_read(config->mfd, EMC230X_REGISTER_GLOBALCONFIGURATION, &reg_value);
	if (result != 0) {
		return result;
	}

	result = mfd_emc230x_reg_read(config->mfd, 0xFE, &value);
	LOG_DBG("EMC230X MFG ID: 0x%x", (uint32_t)value);
	result = mfd_emc230x_reg_read(config->mfd, 0xFF, &value);
	LOG_DBG("EMC230X Silicon Rev: 0x%x", (uint32_t)value);

	result = mfd_emc230x_reg_read(config->mfd, EMC230X_REGISTER_PRODUCT, &value);
	LOG_DBG("EMC230X product id: 0x%x", (uint32_t)value);

	value = EMC230X_PRODUCT_GET(value);

	switch (value) {
		case 0:
			data->max_channels = 5;
			LOG_DBG("Found EMC2305 product");
			break;
		case 1:
			data->max_channels = 3;
			LOG_DBG("Found EMC2303 product");
			break;
		case 2:
			data->max_channels = 2;
			LOG_DBG("Found EMC2302 product");
			break;
		case 3:
			data->max_channels = 1;
			LOG_DBG("Found EMC2301 product");
			break;
		default:
			LOG_ERR("Unknown EMC230X device, setting max channels to 1");
			data->max_channels = 1;
			break;
	}

	return 0;
}

#define DT_FAN_SPEED_CTLR(node_id)								   \
	DT_PHANDLE_BY_IDX(node_id, tach, 0)

#define FAN_DEFINE(node_id, id)                                                                    \
	static const struct pwm_dt_spec fan_pwm_##id =                                             \
		PWM_DT_SPEC_GET(node_id);                                                          \
		                                                                                   \
	static const struct fan_config fan_##id##_cfg = {                                          \
		.pwm = &fan_pwm_##id,                                                              \
		.mfd = DEVICE_DT_GET(DT_GPARENT(node_id)),                                         \
		.tach = DEVICE_DT_GET(DT_FAN_SPEED_CTLR(node_id)),				   \
		.edges = DT_PROP(node_id, edges),                                                  \
		.initial = DT_PROP(node_id, initial),						   \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_DEFINE(node_id, fan_init, NULL, NULL, &fan_##id##_cfg,                           \
			POST_KERNEL, 81, &fan_api);

#define FAN_DEFINE_COND(inst, child)                                                               \
	COND_CODE_1(DT_NODE_EXISTS(DT_INST_CHILD(inst, child)),                                    \
			(FAN_DEFINE(DT_INST_CHILD(inst, child), child##inst)),                     \
			())

#define EMC230X_PWM_INIT_ALL(inst)                                                                 \
                                                                                                   \
	static const struct emc230x_pwm_config emc230x_pwm_##inst##_config = {                     \
		.mfd = DEVICE_DT_GET(DT_INST_PARENT(inst)),                                        \
	};                                                                                         \
                                                                                                   \
	static struct emc230x_pwm_data emc230x_pwm_##inst##_data;                                  \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, emc230x_pwm_init, NULL, &emc230x_pwm_##inst##_data,            \
			      &emc230x_pwm_##inst##_config, POST_KERNEL, 81,                       \
			      &emc230x_pwm_api);                                                   \
                                                                                                   \
	FAN_DEFINE_COND(inst, fan1)                                                                \
	FAN_DEFINE_COND(inst, fan2)                                                                \
	FAN_DEFINE_COND(inst, fan3)                                                                \
	FAN_DEFINE_COND(inst, fan4)                                                                \
	FAN_DEFINE_COND(inst, fan5)

DT_INST_FOREACH_STATUS_OKAY(EMC230X_PWM_INIT_ALL);
