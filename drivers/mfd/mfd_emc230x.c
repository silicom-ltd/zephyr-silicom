/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT microchip_emc230x

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/mfd/emc230x.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>


LOG_MODULE_REGISTER(microchip_emc230x, CONFIG_MFD_LOG_LEVEL);

struct mfd_emc230x_config {
	struct i2c_dt_spec i2c;
};

struct mfd_emc230x_data {
       struct k_mutex mutex;
       const struct device *dev;
};
       
static int mfd_emc230x_init(const struct device *dev)
{
	const struct mfd_emc230x_config *config = dev->config;
	struct mfd_emc230x_data *mfd_data = dev->data;
	int result;
	uint8_t reg_value;

	if (!i2c_is_ready_dt(&config->i2c)) {
		LOG_ERR("I2C device not ready");
		return -ENODEV;
	}

	mfd_data->dev = dev;

	reg_value = 0;
	reg_value &= ~EMC230X_GLOBALCONFIGURATION_WD_EN_BIT;
	reg_value |= EMC230X_GLOBALCONFIGURATION_MASK_BIT;
	reg_value |= EMC230X_GLOBALCONFIGURATION_DIS_TO_BIT;
	reg_value &= ~EMC230X_GLOBALCONFIGURATION_DRECK_BIT;
	reg_value &= ~EMC230X_GLOBALCONFIGURATION_USECK_BIT;

	result = mfd_emc230x_reg_write(dev, EMC230X_REGISTER_GLOBALCONFIGURATION,
				       reg_value);
	if (result != 0) {
		return result;
	}

	return 0;
}

int mfd_emc230x_reg_read_burst(const struct device *dev, uint8_t base, void *data,
			       size_t len)
{
	const struct mfd_emc230x_config *config = dev->config;
	uint8_t buff[] = {base};

	return i2c_write_read_dt(&config->i2c, buff, sizeof(buff), data, len);
}

int mfd_emc230x_reg_read(const struct device *dev, uint8_t base, uint8_t *data)
{
	return mfd_emc230x_reg_read_burst(dev, base, data, 1U);
}

int mfd_emc230x_reg_write(const struct device *dev, uint8_t base, uint8_t data)
{
	const struct mfd_emc230x_config *config = dev->config;
	uint8_t buff[] = {base, data};

	return i2c_write_dt(&config->i2c, buff, sizeof(buff));
}

int mfd_emc230x_reg_update(const struct device *dev, uint8_t base, uint8_t data,
			   uint8_t mask)
{
	struct mfd_emc230x_data *mfd_data = dev->data;
	uint8_t reg;
	int ret;

	k_mutex_lock(&mfd_data->mutex, K_FOREVER);

	ret = mfd_emc230x_reg_read(dev, base, &reg);

	if (ret == 0) {
		reg = (reg & ~mask) | (data & mask);
		ret = mfd_emc230x_reg_write(dev, base, reg);
	}

	k_mutex_unlock(&mfd_data->mutex);

	return ret;
}

#define MFD_EMC230X_DEFINE(inst)                                                                   \
        static struct mfd_emc230x_data data_##inst;                                                 \
	                                                                                           \
	static const struct mfd_emc230x_config config##inst = {                                    \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, mfd_emc230x_init, NULL, &data_##inst, &config##inst,           \
			      POST_KERNEL, CONFIG_MFD_EMC230X_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(MFD_EMC230X_DEFINE);
