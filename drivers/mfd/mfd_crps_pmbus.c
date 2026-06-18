/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ocp_crps

#include <zephyr/device.h>
#include <zephyr/drivers/smbus.h>
#include <zephyr/drivers/pmbus.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>


LOG_MODULE_REGISTER(ocp_crps, CONFIG_MFD_LOG_LEVEL);

struct mfd_crps_config {
	const struct smbus_dt_spec smbus;
};

struct mfd_crps_data {
       struct k_sem acc_lock;
};

/* write a byte of data to device */
int mfd_crps_write(const struct device *dev, uint8_t byte)
{
	const struct mfd_crps_config *config = dev->config;
	int ret;

	ret = pmbus_write_byte(&config->smbus, byte, 1);
	
	return ret;

}

int mfd_crps_write_byte(const struct device *dev, int page, uint8_t reg, uint8_t byte)
{
	const struct mfd_crps_config *config = dev->config;
	struct mfd_crps_data *data = dev->data;
	int ret;

	k_sem_take(&data->acc_lock, K_FOREVER);
	ret = pmbus_write_byte_data(&config->smbus, page, reg, byte);
	k_sem_give(&data->acc_lock);

	return ret;
}


int mfd_crps_write_word(const struct device *dev, int page, uint8_t reg, uint16_t word)
{
	const struct mfd_crps_config *config = dev->config;
	struct mfd_crps_data *data = dev->data;
	int ret;

	k_sem_take(&data->acc_lock, K_FOREVER);
	ret = pmbus_write_word_data(&config->smbus, page, reg, word);
	k_sem_give(&data->acc_lock);

	return ret;
}

int mfd_crps_read_word(const struct device *dev, int page, uint8_t reg, uint16_t *word)
{
	const struct mfd_crps_config *config = dev->config;
	struct mfd_crps_data *data = dev->data;
	int ret;

	k_sem_take(&data->acc_lock, K_FOREVER);
	ret = pmbus_read_word_data(&config->smbus, page, 0, reg, word);
	k_sem_give(&data->acc_lock);

	return ret;
}

int mfd_crps_read_byte(const struct device *dev, int page, uint8_t reg, uint8_t *byte)
{
	const struct mfd_crps_config *config = dev->config;
	struct mfd_crps_data *data = dev->data;
	int ret;

	k_sem_take(&data->acc_lock, K_FOREVER);
	ret = pmbus_read_byte_data(&config->smbus, page, reg, byte);
	k_sem_give(&data->acc_lock);

	return ret;
}

static int mfd_crps_init(const struct device *dev)
{
	const struct mfd_crps_config *config = dev->config;
	struct mfd_crps_data *data = dev->data;
	int result;
	uint8_t byte_value;
	uint8_t num_read;
	uint8_t msg_buf[32];

	k_sem_init(&data->acc_lock, 0, 1);

	result = pmbus_read_block_data(&config->smbus, 0, 0x99, &num_read, msg_buf);

	if (result == 0) {
		msg_buf[num_read] = 0;
		LOG_DBG("CRPS MFR_ID: %s", msg_buf);
	}
	else {
		LOG_DBG("Unable to read MFR_ID");
		dev->state->init_res = -ENODEV;
		return result;
	}
	result = pmbus_read_block_data(&config->smbus, 0, 0x9A, &num_read, msg_buf);
	if (result == 0) {
		msg_buf[num_read] = 0;
		LOG_DBG("CRPS MFR_MODEL %s", msg_buf);
	}
	else {
		return result;
	}

	result = pmbus_read_byte_data(&config->smbus, 0, 0x98, &byte_value);

	if (result != 0) {
		LOG_DBG("Error reading CRPS");
		return result;
	}
	else
		LOG_DBG("CRPS %s, PMBus Rev: 0x%x", dev->name, byte_value);

	return 0;
}

#define MFD_CRPS_DEFINE(inst)                                                                      \
        static struct mfd_crps_data data_##inst;                                                   \
	                                                                                           \
	static const struct mfd_crps_config config##inst = {                                       \
		.smbus = SMBUS_DT_SPEC_INST_GET(inst),						   \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, mfd_crps_init, NULL, &data_##inst, &config##inst,              \
			      POST_KERNEL, CONFIG_MFD_EMC230X_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(MFD_CRPS_DEFINE);
