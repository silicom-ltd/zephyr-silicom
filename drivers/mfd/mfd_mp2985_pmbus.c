/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT mps_mp2985

#include <zephyr/device.h>
#include <zephyr/drivers/smbus.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>


LOG_MODULE_REGISTER(mps_mp2985, CONFIG_MFD_LOG_LEVEL);

struct mfd_mp2985_config {
	const struct smbus_dt_spec smbus;
};

struct mfd_mp2985_data {
       struct k_mutex mutex;
};

/* write a byte of data to device */
int mfd_mp2985_write(const struct device *dev, uint8_t data)
{
	const struct mfd_mp2985_config *config = dev->config;

	return pmbus_write_byte(&config->smbus, data, 1);
}

int mfd_mp2985_write_byte(const struct device *dev, int page, uint8_t reg, uint8_t byte)
{
	const struct mfd_mp2985_config *config = dev->config;

	return pmbus_write_byte_data(&config->smbus, page, reg, byte);
}


int mfd_mp2985_write_word(const struct device *dev, int page, uint8_t reg, uint16_t word)
{
	const struct mfd_mp2985_config *config = dev->config;

	return pmbus_write_word_data(&config->smbus, page, reg, word);
}

int mfd_mp2985_read_word(const struct device *dev, int page, uint8_t reg, uint16_t *word)
{
	const struct mfd_mp2985_config *config = dev->config;

	return pmbus_read_word_data(&config->smbus, page, 0, reg, word);
}

int mfd_mp2985_read_byte(const struct device *dev, int page, uint8_t reg, uint8_t *byte)
{
	const struct mfd_mp2985_config *config = dev->config;

	return pmbus_read_byte_data(&config->smbus, page, reg, byte);
}

static int mfd_mp2985_init(const struct device *dev)
{
	const struct mfd_mp2985_config *config = dev->config;
	int result;
	uint8_t byte_value;

	result = pmbus_read_byte_data(&config->smbus, 0, 0x98, &byte_value);

	if (result != 0) {
		LOG_DBG("Error reading MP2985");
		return result;
	}
	else
		LOG_DBG("MP2985 %s, PMBus Rev: 0x%x", dev->name, byte_value);

	return 0;
}

#define MFD_MP2985_DEFINE(inst)                                                                    \
        static struct mfd_mp2985_data data_##inst;                                                 \
	                                                                                           \
	static const struct mfd_mp2985_config config##inst = {                                     \
		.smbus = SMBUS_DT_SPEC_INST_GET(inst),						   \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, mfd_mp2985_init, NULL, &data_##inst, &config##inst,            \
			      POST_KERNEL, CONFIG_MFD_EMC230X_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(MFD_MP2985_DEFINE);
