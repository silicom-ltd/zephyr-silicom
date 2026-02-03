/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT maxim_max31785

#include <zephyr/device.h>
#include <zephyr/drivers/smbus.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(maxim_max31785, 4); //CONFIG_MFD_LOG_LEVEL);

struct mfd_max31785_config {
	const struct smbus_dt_spec smbus;
	const struct gpio_dt_spec reset_gpio;
	const struct gpio_dt_spec ctrl_gpio;
};

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

	k_msleep(100);
	result = pmbus_read_word_data(&config->smbus, 0, 0, 0x9b, &word_value);

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
	static const struct mfd_max31785_config max31785_config_##inst = {					\
		.smbus = SMBUS_DT_SPEC_INST_GET(inst),								\
		.reset_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, reset_gpios, {0}),					\
		.ctrl_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, control_gpios, {0}),				\
	};													\
														\
	DEVICE_DT_INST_DEFINE(inst, mfd_max31785_init, NULL, NULL, &max31785_config_##inst,			\
			      POST_KERNEL, CONFIG_MFD_INIT_PRIORITY, NULL);					\

DT_INST_FOREACH_STATUS_OKAY(MFD_MAX31785_DEFINE);
