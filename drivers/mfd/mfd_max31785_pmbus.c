/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT maxim_max31785

#include <zephyr/device.h>
#include <zephyr/drivers/mfd/max31785.h>
#include <zephyr/drivers/smbus.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(maxim_max31785, CONFIG_MFD_LOG_LEVEL);

struct mfd_max31785_config {
	const struct smbus_dt_spec smbus;
	const struct gpio_dt_spec reset_gpio;
	const struct gpio_dt_spec ctrl_gpio;
};

struct mfd_max31785_data {
	uint32_t last_time;
};

#define MAX31785_US_DELAY 9000 
/*
 * MAX31785 has an issue with accesses coming to quickly behind the previous.
 * Ensure at least 250us between each access.
 */
#if 0
static void max31785_timer_callback(struct k_timer *timer)
{
	struct mfd_max31785_data *data = timer->user_data;

	data->delay = false;
}

K_TIMER_DEFINE(max31785_timer, max31785_timer_callback, NULL);
#endif

static inline void max31785_delay(struct mfd_max31785_data *data)
{
	int32_t time_between = k_ticks_to_us_ceil32(k_cycle_get_32() - data->last_time);

	if (time_between < MAX31785_US_DELAY) {
		k_busy_wait(MAX31785_US_DELAY-time_between);
	}
}

int mfd_max31785_write_byte(const struct device *dev, int page, uint8_t reg, uint8_t byte)
{
	const struct mfd_max31785_config *config = dev->config;
	struct mfd_max31785_data *data = dev->data;

	int ret;

	max31785_delay(data);
	ret = pmbus_write_byte_data(&config->smbus, page, reg, byte);
	data->last_time = k_cycle_get_32();

	return ret;
}

int mfd_max31785_read_byte(const struct device *dev, int page, uint8_t reg, uint8_t *val)
{
	const struct mfd_max31785_config *config = dev->config;
	struct mfd_max31785_data *data = dev->data;

	int ret;

	max31785_delay(data);
	ret = pmbus_read_byte_data(&config->smbus, page, reg, val);
	data->last_time = k_cycle_get_32();

	return ret;
}

int mfd_max31785_write_word(const struct device *dev, int page, uint8_t reg, uint16_t word)
{
	const struct mfd_max31785_config *config = dev->config;
	struct mfd_max31785_data *data = dev->data;

	int ret;

	max31785_delay(data);
	ret = pmbus_write_word_data(&config->smbus, page, reg, word);
	data->last_time = k_cycle_get_32();

	return ret;
}

int mfd_max31785_read_word(const struct device *dev, int page, uint8_t reg, uint16_t *val)
{
	const struct mfd_max31785_config *config = dev->config;
	struct mfd_max31785_data *data = dev->data;

	int ret;

	max31785_delay(data);
	ret = pmbus_read_word_data(&config->smbus, page, 0, reg, val);
	data->last_time = k_cycle_get_32();

	return ret;
}

int mfd_max31785_write_block(const struct device *dev, int page, uint8_t reg, uint8_t count, uint8_t *buf)
{
	const struct mfd_max31785_config *config = dev->config;
	struct mfd_max31785_data *data = dev->data;

	int ret;

	max31785_delay(data);
	ret = pmbus_write_block_data(&config->smbus, page, reg, count, buf);
	data->last_time = k_cycle_get_32();
	
	return ret;
}

static int mfd_max31785_init(const struct device *dev)
{
	const struct mfd_max31785_config *config = dev->config;
	int result;
	uint16_t word_value;

	LOG_DBG("Entry to MAX31785: addr 0x%x",config->smbus.addr);
	if (config->reset_gpio.port) {
		if (gpio_pin_set_dt(&config->reset_gpio, 1))
			return -EIO;
	}

	if (config->ctrl_gpio.port) {
		LOG_DBG("Setting ctrl_gpio");
		if (gpio_pin_set_dt(&config->ctrl_gpio, 1))
			return -EIO;
	}

	k_busy_wait(500);
	result = mfd_max31785_read_word(dev, 0, 0x9b, &word_value);

	if (result != 0) {
		LOG_DBG("Error reading MAX31785");
		return result;
	}
	else
		LOG_DBG("MAX31785: found %s", ((word_value == 0x3030) ? "max31785" :
			((word_value == 0x3040) ? "max31785a" : (word_value == 0x3061) ? "max31785b" : NULL))) ;

	return 0;
}

#define MFD_MAX31785_DEFINE(inst)										\
	static struct mfd_max31785_data max31785_data_##inst = {						\
	};													\
														\
	static const struct mfd_max31785_config max31785_config_##inst = {					\
		.smbus = SMBUS_DT_SPEC_INST_GET(inst),								\
		.reset_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, reset_gpios, {0}),					\
		.ctrl_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, control_gpios, {0}),				\
	};													\
														\
	DEVICE_DT_INST_DEFINE(inst, mfd_max31785_init, NULL, &max31785_data_##inst, &max31785_config_##inst,	\
			      POST_KERNEL, CONFIG_MFD_INIT_PRIORITY, NULL);					\

DT_INST_FOREACH_STATUS_OKAY(MFD_MAX31785_DEFINE);
