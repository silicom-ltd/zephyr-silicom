/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_MFD_MPQ8655_H_
#define ZEPHYR_INCLUDE_DRIVERS_MFD_MPQ8655_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>

#include <zephyr/sys/util.h>
#include <zephyr/sys/util_macro.h>

int mfd_mpq8655_read_word(const struct device *dev, uint8_t page, uint8_t reg,
			   uint16_t *val);
int mfd_mpq8655_read_byte(const struct device *dev, uint8_t page, uint8_t reg,
			   uint8_t *val);
int mfd_mpq8655_write_word(const struct device *dev, uint8_t page, uint8_t reg,
			    uint16_t val);
int mfd_mpq8655_write_byte(const struct device *dev, uint8_t page, uint8_t reg,
			    uint8_t val);

#ifdef __cplusplus
}
#endif
#endif /* ZEPHYR_INCLUDE_DRIVERS_MFD_MPQ8655_H_ */
