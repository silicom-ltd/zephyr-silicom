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

struct emc230x_config {
	struct i2c_dt_spec i2c;
};

static int emc230x_init(const struct device *dev)
{
	const struct emc230x_config *config = dev->config;
	int result;
	uint8_t reg_value;

	if (!i2c_is_ready_dt(&config->i2c)) {
		LOG_ERR("I2C device not ready");
		return -ENODEV;
	}

	reg_value = 0;
	reg_value &= ~EMC230X_GLOBALCONFIGURATION_WD_EN_BIT;
	reg_value |= EMC230X_GLOBALCONFIGURATION_MASK_BIT;
	reg_value |= EMC230X_GLOBALCONFIGURATION_DIS_TO_BIT;
	reg_value &= ~EMC230X_GLOBALCONFIGURATION_DRECK_BIT;
	reg_value &= ~EMC230X_GLOBALCONFIGURATION_USECK_BIT;

	result = i2c_reg_write_byte_dt(&config->i2c, EMC230X_REGISTER_GLOBALCONFIGURATION,
				       reg_value);
	if (result != 0) {
		return result;
	}

	return 0;
}

#define EMC230X_INIT(inst)                                                                         \
	static const struct emc230x_config emc230x_##inst##_config = {                             \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, emc230x_init, NULL, NULL, &emc230x_##inst##_config,            \
			      POST_KERNEL, CONFIG_MFD_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(EMC230X_INIT);
