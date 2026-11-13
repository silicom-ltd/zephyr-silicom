/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT mps_mpq8655

#include <zephyr/device.h>
#include <zephyr/drivers/smbus.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mps_mpq8655, CONFIG_MFD_LOG_LEVEL);

struct mfd_mpq8655_config {
	const struct smbus_dt_spec smbus;
};

struct mfd_mpq8655_data {
	struct k_mutex lock;
};

int mfd_mpq8655_read_word(const struct device *dev, uint8_t page, uint8_t reg, uint16_t *val)
{
	const struct mfd_mpq8655_config *config = dev->config;
	struct mfd_mpq8655_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	ret = pmbus_read_word_data(&config->smbus, page, 0, reg, val);
	k_mutex_unlock(&data->lock);

	return ret;
}

int mfd_mpq8655_read_byte(const struct device *dev, uint8_t page, uint8_t reg, uint8_t *val)
{
	const struct mfd_mpq8655_config *config = dev->config;
	struct mfd_mpq8655_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	ret = pmbus_read_byte_data(&config->smbus, page, reg, val);
	k_mutex_unlock(&data->lock);

	return ret;
}

int mfd_mpq8655_write_word(const struct device *dev, uint8_t page, uint8_t reg, uint16_t val)
{
	const struct mfd_mpq8655_config *config = dev->config;
	struct mfd_mpq8655_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	ret = pmbus_write_word_data(&config->smbus, page, reg, val);
	k_mutex_unlock(&data->lock);

	return ret;
}

int mfd_mpq8655_write_byte(const struct device *dev, uint8_t page, uint8_t reg, uint8_t val)
{
	const struct mfd_mpq8655_config *config = dev->config;
	struct mfd_mpq8655_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	ret = pmbus_write_byte_data(&config->smbus, page, reg, val);
	k_mutex_unlock(&data->lock);

	return ret;
}

static int mfd_mpq8655_init(const struct device *dev)
{
	const struct mfd_mpq8655_config *config = dev->config;
	struct mfd_mpq8655_data *data = dev->data;
	uint8_t byte_value;
	int ret;

	k_mutex_init(&data->lock);

	k_mutex_lock(&data->lock, K_FOREVER);
	ret = pmbus_read_byte_data(&config->smbus, 0, PMBUS_MFR_ID, &byte_value);
	k_mutex_unlock(&data->lock);

	if (ret != 0) {
		LOG_DBG("Error reading MPQ8655");
		return ret;
	}

	LOG_DBG("MPQ8655 %s, MFR_ID byte0: 0x%x", dev->name, byte_value);

	return 0;
}

#define MFD_MPQ8655_DEFINE(inst)							\
	static struct mfd_mpq8655_data data_##inst;					\
											\
	static const struct mfd_mpq8655_config config_##inst = {			\
		.smbus = SMBUS_DT_SPEC_INST_GET(inst),					\
	};										\
											\
	DEVICE_DT_INST_DEFINE(inst, mfd_mpq8655_init, NULL, &data_##inst, &config_##inst, \
			      POST_KERNEL, CONFIG_MFD_MPQ8655_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(MFD_MPQ8655_DEFINE)
