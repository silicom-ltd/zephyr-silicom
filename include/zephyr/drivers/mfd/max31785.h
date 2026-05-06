/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_MFD_MAX31785_H_
#define ZEPHYR_INCLUDE_DRIVERS_MFD_MAX31785_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>

#include <zephyr/sys/util.h>
#include <zephyr/sys/util_macro.h>

int mfd_max31785_read_word(const struct device *dev, int page, uint8_t reg,
			  uint16_t *val);
int mfd_max31785_write_word(const struct device *dev, int page, uint8_t reg,
			  uint16_t val);
int mfd_max31785_read_byte(const struct device *dev, int page, uint8_t reg,
			  uint8_t *val);
int mfd_max31785_write_byte(const struct device *dev, int page, uint8_t reg,
			    uint8_t val);
int mfd_max31785_write_block(const struct device *dev, int page, uint8_t reg,
			     uint8_t count, uint8_t *buf);

#ifdef __cplusplus
}
#endif
#endif /* ZEPHYR_INCLUDE_DRIVERS_MFD_MAX31785_H_ */
