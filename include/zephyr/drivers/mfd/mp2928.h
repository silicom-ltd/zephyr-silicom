/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_MFD_MP2928_H_
#define ZEPHYR_INCLUDE_DRIVERS_MFD_MP2928_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>

#include <zephyr/sys/util.h>
#include <zephyr/sys/util_macro.h>

int mfd_mp2928_read_word(const struct device *dev, uint8_t page, uint8_t reg,
			  uint16_t *val);
int mfd_mp2928_read_byte(const struct device *dev, uint8_t page, uint8_t reg,
			  uint8_t *val);

#ifdef __cplusplus
}
#endif
#endif /* ZEPHYR_INCLUDE_DRIVERS_MFD_MP2928_H_ */
