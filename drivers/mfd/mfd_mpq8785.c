/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT mps_mpq8785

#include <zephyr/device.h>
//#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/smbus.h>
#include <zephyr/drivers/pmbus.h>
//#include <zephyr/drivers/mfd/mpq8785.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>


LOG_MODULE_REGISTER(mps_mpq8785, CONFIG_MFD_LOG_LEVEL);

struct mfd_mpq8785_config {
//	struct i2c_dt_spec i2c;
	const struct device smbus_dev;
};

struct mfd_mpq8785_data {
       struct k_mutex mutex;
};

/* write a byte of data to device */
int mfd_mpq8785_write(const struct device *dev, uint8_t data)
{
	const struct mfd_mpq8785_config *config = dev->config;
	
	return i2c_write_dt(&config->i2c, &data, 1);
}

int mfd_mpq8785_write_byte(const struct device *dev, uint8_t reg, uint8_t *byte)
{
	const struct mfd_mpq8785_config *config = dev->config;

	return i2c_burst_write_dt(&config->i2c, reg, byte, 1U);
}

int pmbus_set_page(const struct device *dev, int page, int phase)
{
	int ret;

	ret = mfd_mpq8785_write_byte(dev, PMBUS_PAGE, (uint8_t *)&page);

	if (ret)
		return ret;

	return mfd_mpq8785_write_byte(dev, PMBUS_PHASE, (uint8_t *)&phase);
}

int pmbus_write_byte(const struct device *dev, int page, uint8_t byte)
{
	int ret;

	ret = pmbus_set_page(dev, page, 0xFF);

	if (ret)
		return ret;

	return mfd_mpq8785_write(dev, byte);
}

int mfd_mpq8785_write_word(const struct device *dev, uint8_t reg, uint16_t word)
{
	const struct mfd_mpq8785_config *config = dev->config;
	uint8_t data[2];

	data[0] = (uint8_t)word & 0xFF;
	data[1] = (uint8_t)word >> 8;

	return i2c_burst_write_dt(&config->i2c, reg, data, 2);

}
int pmbus_write_word_data(const struct device *dev, int page, uint8_t reg, uint16_t word)
{
	int ret;

	ret = pmbus_set_page(dev, page, 0xFF);

	if (ret)
		return ret;

	return mfd_mpq8785_write_word(dev, reg, word);
}

int mfd_mpq8785_read_word(const struct device *dev, uint8_t reg, uint16_t *word)
{
	const struct mfd_mpq8785_config *config = dev->config;
	int ret;
	uint8_t data[2];

	ret = i2c_burst_read_dt(&config->i2c, reg, data, sizeof(data));

	if (ret)
		return ret;

	*word = (uint16_t)(data[0] << 8 | data[1]);

	return 0;
}

int mfd_mpq8785_read_byte(const struct device *dev, uint8_t reg, uint8_t *byte)
{
	const struct mfd_mpq8785_config *config = dev->config;

	return i2c_burst_read_dt(&config->i2c, reg, byte, 1U);
}

int pmbus_read_word_data(const struct device *dev, int page, int phase, uint8_t reg, uint16_t *word)
{
	int ret;

	ret = pmbus_set_page(dev, page, phase);
	if (ret)
		return ret;

	return mfd_mpq8785_read_word(dev, reg, word);
}

int pmbus_read_byte_data(const struct device *dev, int page, uint8_t reg, uint8_t *data)
{
	int ret;

	ret = pmbus_set_page(dev, page, 0xff);

	if (ret)
		return ret;

	return mfd_mpq8785_read_byte(dev, reg, data);
}

int pmbus_write_byte_data(const struct device *dev, int page, uint8_t reg, uint8_t *data)
{
	int ret;

	ret = pmbus_set_page(dev, page, 0xff);
	if (ret)
		return ret;

	return mfd_mpq8785_write_byte(dev, reg, data);
}

static int mfd_mpq8785_init(const struct device *dev)
{
	const struct mfd_mpq8785_config *config = dev->config;
//	struct mfd_mpq8785_data *mfd_data = dev->data;
	int result;
	uint8_t reg_value;

	if (!i2c_is_ready_dt(&config->i2c)) {
		LOG_ERR("I2C device not ready");
		return -ENODEV;
	}

	result = pmbus_read_byte_data(dev, 0, PMBUS_MFR_ID,
				       &reg_value);

	if (result != 0) {
		LOG_DBG("Error reading MPQ8785");
		return result;
	}
	else
		LOG_DBG("MPQ8785 read: 0x%x",reg_value);

	return 0;
}

#define MFD_MPQ8785_DEFINE(inst)                                                                   \
        static struct mfd_mpq8785_data data_##inst;                                                \
	                                                                                           \
	static const struct mfd_mpq8785_config config##inst = {                                    \
		.smbus_dev = DEVICE_DT_GET(DT_BUS(inst)),					   \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, mfd_mpq8785_init, NULL, &data_##inst, &config##inst,           \
			      POST_KERNEL, CONFIG_MFD_EMC230X_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(MFD_MPQ8785_DEFINE);
//		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
