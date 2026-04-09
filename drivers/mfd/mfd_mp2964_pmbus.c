/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT mps_mp2964

#include <zephyr/device.h>
#include <zephyr/drivers/smbus.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mps_mp2964, 4); //CONFIG_MFD_LOG_LEVEL);

struct mfd_mp2964_config {
	const struct smbus_dt_spec smbus;
};

static int mfd_mp2964_init(const struct device *dev)
{
	const struct mfd_mp2964_config *config = dev->config;
	int result;
	uint16_t word_value;

	LOG_DBG("Entry to MP2964: addr 0x%x",config->smbus.addr);
	result = pmbus_read_word_data(&config->smbus, 0, 0, 0x92, &word_value);

	if (result != 0) {
		LOG_DBG("Error reading MP2964");
		return result;
	}
	else
		LOG_DBG("MP2964: found vendor 0x%x, product 0x%x", (word_value >> 8), (word_value & 0xFF));

	return 0;
}

#define MFD_MP2964_DEFINE(inst)										\
	static const struct mfd_mp2964_config mp2964_config_##inst = {					\
		.smbus = SMBUS_DT_SPEC_INST_GET(inst),								\
	};													\
														\
	DEVICE_DT_INST_DEFINE(inst, mfd_mp2964_init, NULL, NULL, &mp2964_config_##inst,				\
			      POST_KERNEL, CONFIG_MFD_INIT_PRIORITY, NULL);					\

DT_INST_FOREACH_STATUS_OKAY(MFD_MP2964_DEFINE);
