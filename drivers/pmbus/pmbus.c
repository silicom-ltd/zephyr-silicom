/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/pmbus.h>
#include <zephyr/drivers/smbus.h>

int pmbus_set_page(const struct smbus_dt_spec *dev, int page, int phase)
{
	uint8_t read_page;
	int ret;

	ret = smbus_byte_data_write(dev->bus, dev->addr, PMBUS_PAGE, (uint8_t)page);

	if (ret)
		return ret;

	/* XXX JJD ignoring phase for now */
	return smbus_byte_data_read(dev->bus, dev->addr, PMBUS_PAGE, &read_page);
}

int pmbus_write_byte(const struct smbus_dt_spec *dev, int page, uint8_t value)
{
	int ret;

	ret = pmbus_set_page(dev, page, 0xff);
	if (ret)
		return ret;

	return smbus_byte_write(dev->bus, dev->addr, value);
}

int pmbus_write_word_data(const struct smbus_dt_spec *dev, int page, uint8_t reg, uint16_t word)
{
	int ret;

	ret = pmbus_set_page(dev, page, 0xff);

	if (ret)
		return ret;

	return smbus_word_data_write(dev->bus, dev->addr, reg, word);
}

int pmbus_write_byte_data(const struct smbus_dt_spec *dev, int page, uint8_t reg, uint8_t val)
{
	int ret;

	ret = pmbus_set_page(dev, page, 0xff);

	if (ret)
		return ret;

	return smbus_byte_data_write(dev->bus, dev->addr, reg, val);
}

int pmbus_read_word_data(const struct smbus_dt_spec *dev, int page, int phase, uint8_t reg, uint16_t *val)
{
	int ret;

	ret = pmbus_set_page(dev, page, phase);

	if (ret)
		return ret;

	return smbus_word_data_read(dev->bus, dev->addr, reg, val);
}

int pmbus_read_byte_data(const struct smbus_dt_spec *dev, int page, uint8_t reg, uint8_t *val)
{
	int ret;

	ret = pmbus_set_page(dev, page, 0xff);

	if (ret)
		return ret;

	return smbus_byte_data_read(dev->bus, dev->addr, reg, val);
}

int pmbus_read_block_data(const struct smbus_dt_spec *dev, int page, uint8_t reg, uint8_t *count, uint8_t *val)
{
	int ret;

	ret = pmbus_set_page(dev, page, 0xff);

	if (ret)
		return ret;

	return smbus_block_read(dev->bus, dev->addr, reg, count, val);
}

int pmbus_write_block_data(const struct smbus_dt_spec *dev, int page, uint8_t reg, uint8_t count, uint8_t *val)
{
	int ret;

	ret = pmbus_set_page(dev, page, 0xff);

	if (ret)
		return ret;

	return smbus_block_write(dev->bus, dev->addr, reg, count, val);
}
