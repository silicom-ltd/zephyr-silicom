/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_MFD_EMC230X_H_
#define ZEPHYR_INCLUDE_DRIVERS_MFD_EMC230X_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>

#include <zephyr/sys/util.h>
#include <zephyr/sys/util_macro.h>

#define EMC230X_OSCILLATOR_FREQUENCY_IN_HZ 32768

#define EMC2305_CHANNEL_COUNT              5
#define EMC2303_CHANNEL_COUNT              3
#define EMC2302_CHANNEL_COUNT              2
#define EMC2301_CHANNEL_COUNT              1
#define EMC230X_RESET_TIMEOUT_IN_US        1000

#define EMC230X_REGISTER_GLOBALCONFIGURATION               0x20
#define EMC230X_REGISTER_PWMFREQUENCY(channel)             (0x2D - channel/4) /* 0x2C ch 4/5 */
#define EMC230X_REGISTER_FANCONFIGURATION(channel)         (0x32 + channel*0x10)
#define EMC230X_REGISTER_FAN2CONFIGURATION(channel)        (0x33 + channel*0x10)
#define EMC230X_REGISTER_FANSTALLSTATUS                    0x25
#define EMC230X_REGISTER_TACHCOUNTMSB(channel)             (0x3E + channel*0x10)
#define EMC230X_REGISTER_TACHCOUNTLSB(channel)             (0x3F + channel*0x10)
#define EMC230X_REGISTER_FANDRIVESETTING(channel)          (0x30 + channel*0x10)
#define EMC230X_REGISTER_FANSPIN(channel)                  (0x36 + channel*0x10)
#define EMC230X_REGISTER_TACHTARGETCOUNTMSB(channel)       (0x3D + channel*0x10)
#define EMC230X_REGISTER_PRODUCT                           0xFD

#define EMC230X_GLOBALCONFIGURATION_MASK_BIT                BIT(7)
#define EMC230X_GLOBALCONFIGURATION_DIS_TO_BIT              BIT(6)
#define EMC230X_GLOBALCONFIGURATION_WD_EN_BIT               BIT(5)
#define EMC230X_GLOBALCONFIGURATION_DRECK_BIT               BIT(1)
#define EMC230X_GLOBALCONFIGURATION_USECK_BIT               BIT(0)
#define EMC230X_FANXCONFIGURATION_ENAG_BIT                  BIT(7)
#define EMC230X_FANXCONFIGURATION_RNG_MASK		    GENMASK(6,5)
#define EMC230X_FANXCONFIGURATION_RNG(n)		    FIELD_PREP(EMC230X_FANXCONFIGURATION_RNG_MASK, (n))
#define EMC230X_FANXCONFIGURATION_EDG_MASK		    GENMASK(4,3)
#define EMC230X_FANXCONFIGURATION_EDG(n)		    FIELD_PREP(EMC230X_FANXCONFIGURATION_EDG_MASK, (n))
#define EMC230X_FANXCONFIGURATION_UDT_MASK                  GENMASK(2,0) 
#define EMC230X_FANXCONFIGURATION_UDT(n)                    FIELD_PREP(EMC230X_FANXCONFIGURATION_UDT_MASK, (n))
#define EMC230X_FANSPIN_SPINUP_POS                          0
#define EMC230X_FANSPIN_SPINUP_LENGTH                       2
#define EMC230X_PRODUCT_MASK				    GENMASK(1,0)
#define EMC230X_PRODUCT_POS                                 0
#define EMC230X_PRODUCT_LENGTH                              2

#define EMC230X_PRODUCT_GET(value)			    FIELD_GET(EMC230X_PRODUCT_MASK, value)	

#define EMC230X_PRODUCT_EMC2305	0
#define EMC230X_PRODUCT_EMC2303 1
#define EMC230X_PRODUCT_EMC2302 2
#define EMC230X_PRODUCT_EMC2301 3

#define EMC230X_PWMFREQUENCY_PWM_LENGTH                    2

#define FAN_UPDATE_TIME_100MS				   0
#define FAN_UPDATE_TIME_200MS				   1
#define FAN_UPDATE_TIME_300MS				   2
#define FAN_UPDATE_TIME_400MS				   3
#define FAN_UPDATE_TIME_500MS				   4
#define FAN_UPDATE_TIME_800MS				   5
#define FAN_UPDATE_TIME_1200MS				   6
#define FAN_UPDATE_TIME_1600MS				   7

#define FAN_EDGES_3					   0
#define FAN_EDGES_5					   1
#define FAN_EDGES_7					   2
#define FAN_EDGES_9					   3

#define FAN_RANGE_MIN_500				   0
#define FAN_RANGE_MIN_1000				   1
#define FAN_RANGE_MIN_2000				   2
#define FAN_RANGE_MIN_4000				   3


int mfd_emc230x_reg_read_burst(const struct device *dev, uint8_t base, void *data,
			       size_t len);

int mfd_emc230x_reg_read(const struct device *dev, uint8_t base, uint8_t *data);

int mfd_emc230x_reg_write(const struct device *dev, uint8_t base, uint8_t data);

int mfd_emc230x_reg_update(const struct device *dev, uint8_t base, uint8_t data,
			   uint8_t mask);
#ifdef __cplusplus
}
#endif
#endif /* ZEPHYR_INCLUDE_DRIVERS_MFD_EMC230X_H_ */
