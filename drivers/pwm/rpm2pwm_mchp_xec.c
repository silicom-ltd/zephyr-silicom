/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include <soc.h>

LOG_MODULE_REGISTER(rpm2pwm_mchp_xec, CONFIG_PWM_LOG_LEVEL);

#define XEC_RPM2PWM_CFG_UPDATE_100MS	0
#define XEC_RPM2PWM_CFG_UPDATE_200MS	1
#define XEC_RPM2PWM_CFG_UPDATE_300MS	2
#define XEC_RPM2PWM_CFG_UPDATE_400MS	3
#define XEC_RPM2PWM_CFG_UPDATE_500MS	4
#define XEC_RPM2PWM_CFG_UPDATE_800MS	5
#define XEC_RPM2PWM_CFG_UPDATE_1200MS	6
#define XEC_RPM2PWM_CFG_UPDATE_1600MS	7

#define XEC_RPM2PWM_CFG_UPDATE_0	XEC_RPM2PWM_CFG_UPDATE_100MS
#define XEC_RPM2PWM_CFG_UPDATE_1	XEC_RPM2PWM_CFG_UPDATE_200MS
#define XEC_RPM2PWM_CFG_UPDATE_2	XEC_RPM2PWM_CFG_UPDATE_300MS
#define XEC_RPM2PWM_CFG_UPDATE_3	XEC_RPM2PWM_CFG_UPDATE_400MS
#define XEC_RPM2PWM_CFG_UPDATE_4	XEC_RPM2PWM_CFG_UPDATE_500MS
#define XEC_RPM2PWM_CFG_UPDATE_5	XEC_RPM2PWM_CFG_UPDATE_800MS
#define XEC_RPM2PWM_CFG_UPDATE_6	XEC_RPM2PWM_CFG_UPDATE_1200MS
#define XEC_RPM2PWM_CFG_UPDATE_7	XEC_RPM2PWM_CFG_UPDATE_1600MS

#define XEC_RPM2PWM_CFG_EDGES_POS	3
#define XEC_RPM2PWM_CFG_EDGES_MSK	0x18u
#define XEC_RPM2PWM_CFG_MIN_POS		5
#define XEC_RPM2PWM_CFG_MIN_500RPM	(0 << XEC_RPM2PWM_CFG_MIN_POS)
#define XEC_RPM2PWM_CFG_MIN_1000RPM	(1 << XEC_RPM2PWM_CFG_MIN_POS)
#define XEC_RPM2PWM_CFG_MIN_2000RPM	(2 << XEC_RPM2PWM_CFG_MIN_POS)
#define XEC_RPM2PWM_CFG_MIN_4000RPM	(3 << XEC_RPM2PWM_CFG_MIN_POS)
#define XEC_RPM2PWM_CFG_EN_RPM		0x80u
#define XEC_RPM2PWM_CFG_POL_INV		0x200u
#define XEC_RPM2PWM_CFG_ERR_RNG_POS	10
#define XEC_RPM2PWM_CFG_ERR_RNG_0	(0 << XEC_RPM2PWM_CFG_ERR_RNG_POS)
#define XEC_RPM2PWM_CFG_ERR_RNG_50	(1 << XEC_RPM2PWM_CFG_ERR_RNG_POS)
#define XEC_RPM2PWM_CFG_ERR_RNG_100	(2 << XEC_RPM2PWM_CFG_ERR_RNG_POS)
#define XEC_RPM2PWM_CFG_ERR_RNG_200	(3 << XEC_RPM2PWM_CFG_ERR_RNG_POS)
#define XEC_RPM2PWM_CFG_DER_OPT_POS	12
#define XEC_RPM2PWM_CFG_DIS_GLITCH	0x4000u
#define XEC_RPM2PWM_CFG_EN_RRC		0x8000u

#define XEC_RPM2PWM_GAIN_PROP_POS	0
#define XEC_RPM2PWM_GAIN_INT_POS	2
#define XEC_RPM2PWM_GAIN_DER_POS	4

struct rpm2pwm_regs {
	volatile uint16_t SETTING;
	volatile uint16_t CONFIG;
	volatile uint8_t  DIVIDE;
	volatile uint8_t  GAIN;
	volatile uint8_t  SPINUP;
	volatile uint8_t  STEP;
	volatile uint8_t  MINDRIVE;
	volatile uint8_t  VALIDCNT;
	volatile uint8_t  FAILBAND;
	volatile uint16_t TARGET;
	volatile uint16_t TACH;
	volatile uint8_t  PWMFREQ;
	volatile uint8_t  STATUS;
};

struct fan_config {
	const struct pwm_dt_spec *pwm;
	uint8_t edges;
};

/* DT enum values */
#define XEC_RPM2PWM_DER_OPT_NONE	0
#define XEC_RPM2PWM_DER_OPT_BASIC	1
#define XEC_RPM2PWM_DER_OPT_STEP	2
#define XEC_RPM2PWM_DER_OPT_BOTH	3

#define XEC_RPM2PWM_DER_OPT_0		XEC_RPM2PWM_DER_OPT_NONE
#define XEC_RPM2PWM_DER_OPT_1		XEC_RPM2PWM_DER_OPT_BASIC
#define XEC_RPM2PWM_DER_OPT_2		XEC_RPM2PWM_DER_OPT_STEP
#define XEC_RPM2PWM_DER_OPT_3		XEC_RPM2PWM_DER_OPT_BOTH

#define XEC_RPM2PWM_GAIN_DER_1X		(0 << 4)
#define XEC_RPM2PWM_GAIN_DER_2X		(1 << 4)
#define XEC_RPM2PWM_GAIN_DER_4X		(2 << 4)
#define XEC_RPM2PWM_GAIN_DER_8X		(3 << 4)
#define XEC_RPM2PWM_GAIN_DER_OPT_1	XEC_RPM2PWM_GAIN_DER_1X
#define XEC_RPM2PWM_GAIN_DER_OPT_2	XEC_RPM2PWM_GAIN_DER_2X
#define XEC_RPM2PWM_GAIN_DER_OPT_4	XEC_RPM2PWM_GAIN_DER_4X
#define XEC_RPM2PWM_GAIN_DER_OPT_8	XEC_RPM2PWM_GAIN_DER_8X

#define XEC_RPM2PWM_GAIN_INT_1X		(0 << 2)
#define XEC_RPM2PWM_GAIN_INT_2X		(1 << 2)
#define XEC_RPM2PWM_GAIN_INT_4X		(2 << 2)
#define XEC_RPM2PWM_GAIN_INT_8X		(3 << 2)
#define XEC_RPM2PWM_GAIN_INT_OPT_1	XEC_RPM2PWM_GAIN_INT_1X
#define XEC_RPM2PWM_GAIN_INT_OPT_2	XEC_RPM2PWM_GAIN_INT_2X
#define XEC_RPM2PWM_GAIN_INT_OPT_4	XEC_RPM2PWM_GAIN_INT_4X
#define XEC_RPM2PWM_GAIN_INT_OPT_8	XEC_RPM2PWM_GAIN_INT_8X

#define XEC_RPM2PWM_GAIN_PROP_1X	0
#define XEC_RPM2PWM_GAIN_PROP_2X	1
#define XEC_RPM2PWM_GAIN_PROP_4X	2
#define XEC_RPM2PWM_GAIN_PROP_8X	3
#define XEC_RPM2PWM_GAIN_PROP_OPT_1	XEC_RPM2PWM_GAIN_PROP_1X
#define XEC_RPM2PWM_GAIN_PROP_OPT_2	XEC_RPM2PWM_GAIN_PROP_2X
#define XEC_RPM2PWM_GAIN_PROP_OPT_4	XEC_RPM2PWM_GAIN_PROP_4X
#define XEC_RPM2PWM_GAIN_PROP_OPT_8	XEC_RPM2PWM_GAIN_PROP_8X

#define XEC_RPM2PWM_SPINUP_POS		0
#define XEC_RPM2PWM_SPINUP_250MS	0
#define XEC_RPM2PWM_SPINUP_500MS	1
#define XEC_RPM2PWM_SPINUP_1S		2
#define XEC_RPM2PWM_SPINUP_2S		3
#define XEC_RPM2PWM_SPINUP_OPT_0	XEC_RPM2PWM_SPINUP_250MS
#define XEC_RPM2PWM_SPINUP_OPT_1	XEC_RPM2PWM_SPINUP_500MS
#define XEC_RPM2PWM_SPINUP_OPT_2	XEC_RPM2PWM_SPINUP_1S
#define XEC_RPM2PWM_SPINUP_OPT_3	XEC_RPM2PWM_SPINUP_2S

#define XEC_RPM2PWM_SPINUP_LVL_POS	2
#define XEC_RPM2PWM_SPINUP_LVL_30	0
#define XEC_RPM2PWM_SPINUP_LVL_35	1
#define XEC_RPM2PWM_SPINUP_LVL_40	2
#define XEC_RPM2PWM_SPINUP_LVL_45	3
#define XEC_RPM2PWM_SPINUP_LVL_50	4
#define XEC_RPM2PWM_SPINUP_LVL_55	5
#define XEC_RPM2PWM_SPINUP_LVL_60	6
#define XEC_RPM2PWM_SPINUP_LVL_65	7
#define XEC_RPM2PMW_SPINUP_LVL_OPT_0	XEC_RPM2PWM_SPINUP_LVL_30
#define XEC_RPM2PMW_SPINUP_LVL_OPT_1	XEC_RPM2PWM_SPINUP_LVL_35
#define XEC_RPM2PMW_SPINUP_LVL_OPT_2	XEC_RPM2PWM_SPINUP_LVL_40
#define XEC_RPM2PMW_SPINUP_LVL_OPT_3	XEC_RPM2PWM_SPINUP_LVL_45
#define XEC_RPM2PMW_SPINUP_LVL_OPT_4	XEC_RPM2PWM_SPINUP_LVL_50
#define XEC_RPM2PMW_SPINUP_LVL_OPT_5	XEC_RPM2PWM_SPINUP_LVL_55
#define XEC_RPM2PMW_SPINUP_LVL_OPT_6	XEC_RPM2PWM_SPINUP_LVL_60
#define XEC_RPM2PMW_SPINUP_LVL_OPT_7	XEC_RPM2PWM_SPINUP_LVL_65

#define XEC_RPM2PWM_SPINUP_NOKICK		0x20u
#define XEC_RPM2PWM_SPINUP_DRIVEFAIL_POS	6
#define XEC_RPM2PWM_SPINUP_DRIVEFAIL_DIS	0
#define XEC_RPM2PWM_SPINUP_DRIVEFAIL_16		1
#define XEC_RPM2PWM_SPINUP_DRIVEFAIL_32		2
#define XEC_RPM2PWM_SPINUP_DRIVEFAIL_64		3
#define XEC_RPM2PWM_SPINUP_DRIVEFAIL_OPT_0	XEC_RPM2PWM_SPINUP_DRIVEFAIL_DIS
#define XEC_RPM2PMW_SPINUP_DRIVEFAIL_OPT_1	XEC_RPM2PWM_SPINUP_DRIVEFAIL_16
#define XEC_RPM2PMW_SPINUP_DRIVEFAIL_OPT_2	XEC_RPM2PWM_SPINUP_DRIVEFAIL_32
#define XEC_RPM2PMW_SPINUP_DRIVEFAIL_OPT_3	XEC_RPM2PWM_SPINUP_DRIVEFAIL_64

struct rpm2pwm_xec_config {
	struct rpm2pwm_regs *const regs;
	uint8_t pcr_idx;
	uint8_t pcr_pos;
	uint8_t der_opts;
	uint8_t derivative_gain;
	uint8_t integral_gain;
	uint8_t proportional_gain;
	uint8_t update;
	uint8_t spinup_time;
	uint8_t spinup_level;
	uint8_t spinup_nokick;
	uint8_t spinup_drive_fail;
	bool    enable_ramp_control;
	bool	manual_mode;
	const struct pinctrl_dev_config *pcfg;
	const struct fan_config *fan;
};

struct rpm2pwm_tach_xec_config {
	struct device * const parent;
};

struct rpm2pwm_tach_xec_data {
	uint16_t count;
};

struct rpm2pwm_xec_data {
	uint32_t config;
};

int rpm2pwm_tach_xec_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	ARG_UNUSED(chan);

	const struct rpm2pwm_tach_xec_config * const cfg = dev->config;
	struct device * const parent = cfg->parent;
	const struct rpm2pwm_xec_config * const parent_cfg = parent->config;
	struct rpm2pwm_regs * const regs = parent_cfg->regs;

	struct rpm2pwm_tach_xec_data * const data = dev->data;

	data->count = 3932160 / (regs->TACH >> 3);

	return 0;
}

static int rpm2pwm_tach_xec_channel_get(const struct device *dev,
					enum sensor_channel chan,
					struct sensor_value *val)
{
	struct rpm2pwm_tach_xec_data * const data = dev->data;

	if (chan != SENSOR_CHAN_RPM) {
		return -ENOTSUP;
	}

	val->val1 = data->count;
	val->val2 = 0U;

	return 0;
}

static const struct sensor_driver_api rpm2pwm_tach_xec_driver_api = {
	.sample_fetch = rpm2pwm_tach_xec_sample_fetch,
	.channel_get = rpm2pwm_tach_xec_channel_get,
};

static int rpm2pwm_tach_xec_init(const struct device *dev)
{
	const struct rpm2pwm_tach_xec_config *config = dev->config;

	if (!device_is_ready(config->parent))
		return -ENODEV;

	return 0;
}

static int rpm2pwm_xec_set_cycles_internal(const struct device *dev, uint32_t channel,
					   uint32_t period_count, uint32_t pulse_count,
					   pwm_flags_t flags)
{
	const struct rpm2pwm_xec_config * const cfg = dev->config;
	struct rpm2pwm_regs * const regs = cfg->regs;
	uint16_t config;

	config = regs->CONFIG;
#if 0
	config &= ~(0x3 << 5);	/* clear range field */
	config &= ~(0x3 << 3);	/* clear edges field */
	config &= ~(0x3 << 12); /* clear the DER field */
	config |= (1 << 12);	/* basic derivative */

	config |= (((fan->edges / 2) - 1) << 3);

	regs->GAIN = 0x25;
#endif
	if (!cfg->manual_mode) {
		regs->TARGET = (3932160 / pulse_count) << 3;
		regs->CONFIG = (config | 0x80);
	}
	else {
		regs->SETTING = (1023 / (100 / pulse_count)) << 6;
	}


	return 0;
}

static int rpm2pwm_xec_set_cycles(const struct device *dev, uint32_t channel,
				  uint32_t period_cycles, uint32_t pulse_cycles,
				  pwm_flags_t flags)
{
	if (channel > 0) {
		return -EIO;
	}

	rpm2pwm_xec_set_cycles_internal(dev, channel, period_cycles, pulse_cycles, flags);
	return 0;
}

static int rpm2pwm_xec_get_cycles_per_sec(const struct device *dev,
				      uint32_t channel, uint64_t *cycles)
{
	ARG_UNUSED(dev);

	if (channel > 0) {
		return -EIO;
	}

	if (cycles) {
		/* User does not have to know about lowest clock,
		 * the driver will select the most relevant one.
		 */
		*cycles = 32768;
	}

	return 0;
}

static const struct pwm_driver_api rpm2pwm_xec_driver_api = {
	.set_cycles = rpm2pwm_xec_set_cycles,
	.get_cycles_per_sec = rpm2pwm_xec_get_cycles_per_sec,
};

static int rpm2pwm_xec_init(const struct device *dev)
{
	const struct rpm2pwm_xec_config *const cfg = dev->config;
	struct rpm2pwm_regs *const regs = cfg->regs;
	const struct fan_config * const fan = cfg->fan;
	uint16_t config;

	int ret = pinctrl_apply_state(cfg->pcfg, PINCTRL_STATE_DEFAULT);

	config = cfg->update | (cfg->der_opts << XEC_RPM2PWM_CFG_DER_OPT_POS) |
		 (((fan->edges / 2) - 1) << XEC_RPM2PWM_CFG_EDGES_POS) | 
		 (cfg->enable_ramp_control ? XEC_RPM2PWM_CFG_EN_RRC : 0);

	regs->CONFIG = config;
	regs->GAIN = (cfg->derivative_gain | cfg->integral_gain | 
		      cfg->proportional_gain);
	regs->MINDRIVE = 0x33;

	regs->SPINUP = (cfg->spinup_time) | 
		       (cfg->spinup_level << XEC_RPM2PWM_SPINUP_LVL_POS) |
		       (cfg->spinup_nokick ? XEC_RPM2PWM_SPINUP_NOKICK : 0) |
		       (cfg->spinup_drive_fail << XEC_RPM2PWM_SPINUP_DRIVEFAIL_POS);

	if (ret != 0) {
		LOG_ERR("XEC RPM2PWM pinctrl init failed (%d)", ret);
		return ret;
	}

	return 0;
}

#define XEC_RPM2PWM_DER(n)								\
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, der_opts),					\
			(DT_INST_ENUM_IDX(n, der_opts)), (3))

#define XEC_RPM2PWM_GAIN_DER(n)								\
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, gain),					\
			(DT_INST_PROP_BY_IDX(n, gain, 2)), (4))

#define XEC_RPM2PWM_GAIN_INT(n)								\
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, gain),					\
			(DT_INST_PROP_BY_IDX(n, gain, 1)), (2))

#define XEC_RPM2PWM_GAIN_PROP(n)							\
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, gain),					\
			(DT_INST_PROP_BY_IDX(n, gain, 0)), (2))

#define XEC_RPM2PWM_UPDATE(n)								\
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, update),					\
			(DT_INST_ENUM_IDX(n, update)), (3))

#define XEC_RPM2PWM_SPINUP_TIME(n)							\
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, spinup_time),				\
			(DT_INST_ENUM_IDX(n, spinup_time)), (1))

#define XEC_RPM2PWM_SPINUP_LEVEL(n)							\
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, spinup_level),				\
			(DT_INST_ENUM_IDX(n, spinup_level)), (6))

#define XEC_RPM2PWM_SPINUP_DRIVEFAIL(n)							\
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, spinup_drive_fail),			\
			(DT_INST_ENUM_IDX(n, spinup_drive_fail)), (0))

#define XEC_RPM2PWM_SPINUP_KICK(n)							\
	COND_CODE_1(DT_INST_NODE_HAS_PROP(n, spinup_nokick),				\
			(DT_INST_PROP(n, spinup_nokick)), (0))

#define XEC_RPM2PWM_CONFIG(inst)							\
											\
	static const struct pwm_dt_spec fan_pwm_##inst =				\
		PWM_DT_SPEC_GET(DT_CHILD(DT_INST_CHILD(inst, fan), fan));		\
											\
	static const struct fan_config fan_##inst##_cfg = {				\
		.pwm = &fan_pwm_##inst,							\
		.edges = DT_PROP(DT_CHILD(DT_INST_CHILD(inst, fan), fan), edges),	\
	};										\
											\
	static struct rpm2pwm_xec_config rpm2pwm_xec_config_##inst = {			\
		.regs = (struct rpm2pwm_regs * const)DT_INST_REG_ADDR(inst),		\
		.pcr_idx = (uint8_t)DT_INST_PROP_BY_IDX(inst, pcrs, 0),			\
		.pcr_pos = (uint8_t)DT_INST_PROP_BY_IDX(inst, pcrs, 1),			\
		.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(inst),				\
		.der_opts = UTIL_CAT(XEC_RPM2PWM_DER_OPT_, XEC_RPM2PWM_DER(inst)), 	\
		.derivative_gain = UTIL_CAT(XEC_RPM2PWM_GAIN_DER_OPT_, 			\
				XEC_RPM2PWM_GAIN_DER(inst)),				\
		.integral_gain = UTIL_CAT(XEC_RPM2PWM_GAIN_INT_OPT_, 			\
				XEC_RPM2PWM_GAIN_INT(inst)),				\
		.proportional_gain = UTIL_CAT(XEC_RPM2PWM_GAIN_PROP_OPT_,		\
				XEC_RPM2PWM_GAIN_PROP(inst)),				\
		.update = UTIL_CAT(XEC_RPM2PWM_CFG_UPDATE_, XEC_RPM2PWM_UPDATE(inst)),	\
		.enable_ramp_control = DT_INST_PROP(inst, enable_ramp_control),		\
		.spinup_nokick = XEC_RPM2PWM_SPINUP_KICK(inst),				\
		.spinup_time = XEC_RPM2PWM_SPINUP_TIME(inst),				\
		.spinup_level = XEC_RPM2PWM_SPINUP_LEVEL(inst),				\
		.spinup_drive_fail = XEC_RPM2PWM_SPINUP_DRIVEFAIL(inst),		\
		.manual_mode = DT_INST_PROP(inst, manual_mode),				\
		.fan = &fan_##inst##_cfg,						\
	};

#define XEC_RPM2PWM_DEVICE_INIT(index)					\
									\
	static struct rpm2pwm_xec_data rpm2pwm_xec_data_##index;	\
									\
	PINCTRL_DT_INST_DEFINE(index);					\
									\
	XEC_RPM2PWM_CONFIG(index);					\
									\
	DEVICE_DT_INST_DEFINE(index, &rpm2pwm_xec_init,			\
			      NULL,					\
			      &rpm2pwm_xec_data_##index,		\
			      &rpm2pwm_xec_config_##index, POST_KERNEL,	\
			      CONFIG_PWM_INIT_PRIORITY,			\
			      &rpm2pwm_xec_driver_api);

#define DT_DRV_COMPAT microchip_xec_rpm2pwm
DT_INST_FOREACH_STATUS_OKAY(XEC_RPM2PWM_DEVICE_INIT)
#undef DT_DRV_COMPAT

#define XEC_RPM2PWM_TACH_CONFIG(inst)							\
	static struct rpm2pwm_tach_xec_config rpm2pwm_tach_xec_config_##inst = {	\
	       .parent = (struct device *const)DEVICE_DT_GET(DT_INST_PARENT(inst)),	\
	};

#define XEC_RPM2PWM_TACH_DEVICE_INIT(index)					\
	static struct rpm2pwm_tach_xec_data rpm2pwm_tach_xec_data_##index;	\
										\
	XEC_RPM2PWM_TACH_CONFIG(index);						\
										\
	DEVICE_DT_INST_DEFINE(index, &rpm2pwm_tach_xec_init, NULL,		\
			      &rpm2pwm_tach_xec_data_##index,			\
			      &rpm2pwm_tach_xec_config_##index, POST_KERNEL,	\
			      CONFIG_PWM_INIT_PRIORITY,				\
			      &rpm2pwm_tach_xec_driver_api);

#define DT_DRV_COMPAT microchip_xec_rpm2pwm_tach
DT_INST_FOREACH_STATUS_OKAY(XEC_RPM2PWM_TACH_DEVICE_INIT)
#undef DT_DRV_COMPAT
