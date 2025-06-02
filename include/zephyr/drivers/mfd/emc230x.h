/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_MFD_EMC230X_H_
#define ZEPHYR_INCLUDE_DRIVERS_MFD_EMC230X_H_

#include <zephyr/sys/util.h>
#include <zephyr/sys/util_macro.h>

#define EMC230X_OSCILLATOR_FREQUENCY_IN_HZ 32768
#if 0
#define MAX3179_PWMTARGETDUTYCYCLE_MAXIMUM ((1 << 9) - 1)
#define MAX3179_TACHTARGETCOUNT_MAXIMUM    ((1 << 11) - 1)
#endif
#define EMC2305_CHANNEL_COUNT              5
#define EMC2303_CHANNEL_COUNT              3
#define EMC2302_CHANNEL_COUNT              2
#define EMC2301_CHANNEL_COUNT              1
#define EMC230X_RESET_TIMEOUT_IN_US        1000

#define EMC230X_REGISTER_GLOBALCONFIGURATION               0x20
#define EMC230X_REGISTER_PWMFREQUENCY(channel)             (0x2D - channel/4) /* 0x2C ch 4/5 */
#define EMC230X_REGISTER_FANCONFIGURATION(channel)         (0x32 + channel*0x10)
#define EMC230X_REGISTER_FANFAULTSTATUS                    0x25
#define EMC230X_REGISTER_TACHCOUNTMSB(channel)             (0x3E + channel*0x10)
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
#define EMC230X_FANXCONFIGURATION_RNG_POS                   5
#define EMC230X_FANXCONFIGURATION_RNG_LENGTH                2
#define EMC230X_FANXCONFIGURATION_EDG_POS                   3
#define EMC230X_FANXCONFIGURATION_EDG_LENGTH                2
#define EMC230X_FANXCONFIGURATION_UDT_POS                   0
#define EMC230X_FANXCONFIGURATION_EDG_LENGTH                2
#define EMC230X_FANSPIN_SPINUP_POS                          0
#define EMC230X_FANSPIN_SPINUP_LENGTH                       2
#define EMC230X_PRODUCT_POS                                 0
#define EMC230X_PRODUCT_LENGTH                              2

#define EMC230X_PRODUCT_GET(value)				\
	FIELD_GET(GENMASK(EMC230X_PRODUCT_LENGTH, 		\
				EMC230X_PRODUCT_POS),		\
		value)

#define EMC230X_PRODUCT_EMC2305	0
#define EMC230X_PRODUCT_EMC2303 1
#define EMC230X_PRODUCT_EMC2302 2
#define EMC230X_PRODUCT_EMC2301 3

#define EMC230X_PWMFREQUENCY_PWM_LENGTH                    2

#if 0
#define MAX3719_GLOBALCONFIGURATION_I2CWATCHDOG_LENGTH 2
#define MAX3719_GLOBALCONFIGURATION_I2CWATCHDOG_POS    1
#define MAX3719_FANXDYNAMICS_SPEEDRANGE_LENGTH         3
#define MAX3719_FANXDYNAMICS_SPEEDRANGE_POS            5
#define MAX3719_FANXDYNAMICS_PWMRATEOFCHANGE_LENGTH    3
#define MAX3719_FANXDYNAMICS_PWMRATEOFCHANGE_POS       2
#define MAX3719_PWMFREQUENCY_PWM_LENGTH                4
#define MAX3719_PWMFREQUENCY_PWM4TO6_POS               4
#define MAX3719_PWMFREQUENCY_PWM1TO3_LENGTH            4
#define MAX3719_PWMFREQUENCY_PWM1TO3_POS               0
#define MAX3719_FANXCONFIGURATION_SPINUP_LENGTH        2
#define MAX3719_FANXCONFIGURATION_SPINUP_POS           5

#define MAX3179_FANXDYNAMCIS_SPEED_RANGE_GET(value)                                               \
	FIELD_GET(GENMASK(MAX37190_FANXDYNAMICS_SPEEDRANGE_LENGTH +                                \
				  MAX37190_FANXDYNAMICS_SPEEDRANGE_POS - 1,                        \
			  MAX37190_FANXDYNAMICS_SPEEDRANGE_POS),                                   \
		  value)

#define MAX3179_FLAG_SPEED_RANGE_GET(flags)                                                       \
	FIELD_GET(GENMASK(MAX37190_FANXDYNAMICS_SPEEDRANGE_LENGTH +                                \
				  PWM_MAX31790_FLAG_SPEED_RANGE_POS - 1,                           \
			  PWM_MAX31790_FLAG_SPEED_RANGE_POS),                                      \
		  flags)
#define MAX3179_FLAG_PWM_RATE_OF_CHANGE_GET(flags)                                                \
	FIELD_GET(GENMASK(MAX37190_FANXDYNAMICS_PWMRATEOFCHANGE_LENGTH +                           \
				  PWM_MAX31790_FLAG_PWM_RATE_OF_CHANGE_POS - 1,                    \
			  PWM_MAX31790_FLAG_PWM_RATE_OF_CHANGE_POS),                               \
		  flags)
#endif
#endif /* ZEPHYR_INCLUDE_DRIVERS_MFD_EMC230X_H_ */
