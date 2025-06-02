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
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pwm_emc230x, CONFIG_PWM_LOG_LEVEL);

#define EMC230X_PWMTARGETDUTYCYCLE_MAXIMUM 255

struct emc230x_pwm_config {
	struct i2c_dt_spec i2c;
};

struct emc230x_pwm_data {
	struct k_mutex lock;
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

static int emc230x_read_register(const struct device *dev, uint8_t address, uint8_t *value)
{
	const struct emc230x_pwm_config *config = dev->config;
	int result;

	result = i2c_reg_read_byte_dt(&config->i2c, address, value);
	if (result != 0) {
		LOG_ERR("unable to read register 0x%02X, error %i", address, result);
	}

	LOG_DBG("read value 0x%02X from register 0x%02X", *value, address);

	return result;
}

static int emc230x_write_register_uint8(const struct device *dev, uint8_t address, uint8_t value)
{
	const struct emc230x_pwm_config *config = dev->config;
	int result;

	LOG_DBG("writing value 0x%02X to register 0x%02X", value, address);
	result = i2c_reg_write_byte_dt(&config->i2c, address, value);
	if (result != 0) {
		LOG_ERR("unable to write register 0x%02X, error %i", address, result);
	}

	return result;
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
	int result;
	uint8_t pwm_frequency_channel_value;
	uint8_t value_pwm_frequency;
	uint8_t value_fan_configuration;
#if 0
	uint8_t value_fan_dynamics;
	uint8_t value_speed_range = PWM_MAX31790_FLAG_SPEED_RANGE_GET(flags);
	uint8_t value_pwm_rate_of_change = PWM_MAX31790_FLAG_PWM_RATE_OF_CHANGE_GET(flags);
#endif

	if (!emc230x_convert_pwm_frequency_into_register(&pwm_frequency_channel_value,
							 period_count)) {
		return -EINVAL;
	}

	result = emc230x_read_register(dev, EMC230X_REGISTER_PWMFREQUENCY(channel), &value_pwm_frequency);
	if (result != 0) {
		return result;
	}

	emc230x_set_pwmfrequency(&value_pwm_frequency, channel, pwm_frequency_channel_value);

#if 0
	result = emc230x_write_register_uint8(dev, EMC230X_REGISTER_PWMFREQUENCY(channel),
					       value_pwm_frequency);
	if (result != 0) {
		return result;
	}

	value_fan_configuration = 0;
	value_fan_dynamics = 0;
#endif

	if (flags & PWM_EMC230X_FLAG_SPIN_UP) {
		emc230x_set_fan_spinup(&value_fan_configuration, 2);
	} else {
		emc230x_set_fan_spinup(&value_fan_configuration, 0);
	}

#if 0
	value_fan_configuration &= ~MAX37190_FANXCONFIGURATION_MONITOR_BIT;
	value_fan_configuration &= ~MAX37190_FANXCONFIGURATION_LOCKEDROTOR_BIT;
	value_fan_configuration &= ~MAX37190_FANXCONFIGURATION_LOCKEDROTORPOLARITY_BIT;
	value_fan_configuration &= ~MAX37190_FANXCONFIGURATION_TACH_BIT;
	value_fan_configuration |= MAX37190_FANXCONFIGURATION_TACHINPUTENABLED_BIT;

	max31790_set_fandynamics_speedrange(&value_fan_dynamics, value_speed_range);
	max31790_set_fandynamics_pwmrateofchange(&value_fan_dynamics, value_pwm_rate_of_change);
	value_fan_dynamics |= MAX37190_FANXDYNAMICS_ASYMMETRICRATEOFCHANGE_BIT;
#endif
#if 0
	if ((flags & PWM_EMC230X_FLAG_RPM_MODE) == 0) {
		LOG_DBG("PWM mode");
		uint16_t pwm_target_duty_cycle =
			pulse_count * EMC230X_PWMTARGETDUTYCYCLE_MAXIMUM / period_count;
		value_fan_configuration &= ~MAX37190_FANXCONFIGURATION_MODE_BIT;
#endif
		uint16_t pwm_target_duty_cycle = pulse_count * EMC230X_PWMTARGETDUTYCYCLE_MAXIMUM / 100;

		result = emc230x_write_register_uint8(
			dev, EMC230X_REGISTER_FANDRIVESETTING(channel),
			pwm_target_duty_cycle);
		if (result != 0) {
			return result;
		}
#if 0
	} else {
		LOG_DBG("RPM mode");
		value_fan_configuration |= MAX37190_FANXCONFIGURATION_MODE_BIT;

		result = max31790_write_register_uint16(
			dev, MAX31790_REGISTER_TACHTARGETCOUNTMSB(channel), pulse_count);
		if (result != 0) {
			return result;
		}
	}

	result = max31790_write_register_uint8(dev, MAX37190_REGISTER_FANCONFIGURATION(channel),
					       value_fan_configuration);
	if (result != 0) {
		return result;
	}

	result = max31790_write_register_uint8(dev, MAX31790_REGISTER_FANDYNAMICS(channel),
					       value_fan_dynamics);
	if (result != 0) {
		return result;
	}
#endif

	return 0;
}

static int emc230x_set_cycles(const struct device *dev, uint32_t channel, uint32_t period_count,
			      uint32_t pulse_count, pwm_flags_t flags)
{
	struct emc230x_pwm_data *data = dev->data;
	int result;

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

	k_mutex_lock(&data->lock, K_FOREVER);
	result = emc230x_set_cycles_internal(dev, channel, period_count, pulse_count, flags);
	k_mutex_unlock(&data->lock);
	return result;
}

static int emc230x_get_cycles_per_sec(const struct device *dev, uint32_t channel, uint64_t *cycles)
{
	struct emc230x_pwm_data *data = dev->data;
	int result;
	bool success;
//	uint8_t value;
	uint8_t pwm_frequency_register;
	uint8_t pwm_frequency = 1;
	uint16_t pwm_frequency_in_hz;

	if (channel > data->max_channels-1) {
		LOG_ERR("invalid channel number %i", channel);
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);
	result = emc230x_read_register(dev, EMC230X_REGISTER_PWMFREQUENCY(channel),
				       &pwm_frequency_register);

	if (result != 0) {
		k_mutex_unlock(&data->lock);
		return result;
	}

	pwm_frequency = emc230x_get_pwmfrequency(pwm_frequency_register, channel);
	success = emc230x_convert_pwm_frequency_into_hz(&pwm_frequency_in_hz, pwm_frequency);

	if (!success) {
		k_mutex_unlock(&data->lock);
		return -EINVAL;
	}

	*cycles = pwm_frequency_in_hz;

	k_mutex_unlock(&data->lock);
	return 0;
}

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

	k_mutex_init(&data->lock);

	if (!i2c_is_ready_dt(&config->i2c)) {
		LOG_ERR("I2C device not ready");
		return -ENODEV;
	}

	result = emc230x_read_register(dev, EMC230X_REGISTER_GLOBALCONFIGURATION, &reg_value);
	if (result != 0) {
		return result;
	}

	result = emc230x_read_register(dev, EMC230X_REGISTER_PRODUCT, &value);

	value = EMC230X_PRODUCT_GET(value);

	switch (value) {
		case 0:
			data->max_channels = 5;
			break;
		case 1:
			data->max_channels = 3;
			break;
		case 2:
			data->max_channels = 2;
			break;
		case 3:
			data->max_channels = 1;
			break;
		default:
			LOG_ERR("Unknown EMC230X device, setting max channels to 1");
			data->max_channels = 1;
			break;
	}

	return 0;
}

#define EMC230X_PWM_INIT(inst)                                                                     \
	static const struct emc230x_pwm_config emc230x_pwm_##inst##_config = {                     \
		.i2c = I2C_DT_SPEC_GET(DT_INST_PARENT(inst)),                                      \
	};                                                                                         \
                                                                                                   \
	static struct emc230x_pwm_data emc230x_pwm_##inst##_data;                                  \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, emc230x_pwm_init, NULL, &emc230x_pwm_##inst##_data,            \
			      &emc230x_pwm_##inst##_config, POST_KERNEL, 81, \
			      &emc230x_pwm_api);

DT_INST_FOREACH_STATUS_OKAY(EMC230X_PWM_INIT);
