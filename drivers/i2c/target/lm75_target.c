/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT zephyr_i2c_target_lm75

#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <errno.h>
#include <zephyr/drivers/i2c.h>
#include <string.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c/target/lm75.h>
#include <zephyr/pm/device.h>

#define LOG_LEVEL CONFIG_I2C_LOG_LEVEL
#include <zephyr/logging/log.h>
//LOG_MODULE_REGISTER(i2c_target, CONFIG_I2C_LOG_LEVEL);
LOG_MODULE_REGISTER(i2c_target, 4);

struct i2c_lm75_target_data {
	struct i2c_target_config config;
	const struct device *dev;
	struct k_work_delayable sensor_work;
	uint32_t reg_ptr;
	uint8_t lm75_regs[8];
	bool msb;
	bool write_reg;
};

struct i2c_lm75_target_config {
	struct i2c_dt_spec bus;
	const struct device *temp_sensor;
};

/*
 * Runs in the system workqueue thread, not ISR context (unlike the k_timer
 * expiry function this replaces). Reschedules itself every 500ms to keep
 * polling the backing sensor for as long as the device exists.
 */
static void get_target_sensor_temp(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct i2c_lm75_target_data *data =
		CONTAINER_OF(dwork, struct i2c_lm75_target_data, sensor_work);
	const struct i2c_lm75_target_config *config = data->dev->config;
	struct sensor_value temp;

	int rounded;
	int bitmask = 0;
	int resolution = 3;
	int modulo = 500000;
	int ret;

	ret = sensor_sample_fetch_chan(config->temp_sensor, SENSOR_CHAN_DIE_TEMP);
	sensor_channel_get(config->temp_sensor, SENSOR_CHAN_DIE_TEMP, &temp);

	data->lm75_regs[0] = temp.val1;	
	rounded = ROUND_DOWN(temp.val2,125000);
	while (resolution > 0) {
		bitmask |= (rounded / modulo) ? (1 << (resolution + 4)) : 0;
		resolution--;
		modulo >>= 1;
	}
#if 0
	bitmask = (rounded - 500000 > 0 ? 0x80 : 0);
	rounded %= 500000;
	bitmask |= (rounded - 250000 > 0 ? 0x40 : 0);
	rounded %= 250000;
	bitmask |= (rounded - 125000 > 0 ? 0x20 : 0);
#endif
	data->lm75_regs[1] = bitmask;
//	data->lm75_regs[1] = DIV_ROUND_CLOSEST(temp.val2,125000) * 125000;
//	data->lm75_regs[1] = temp.val2 < 5000 ? 0 : 0x80;

	LOG_DBG("lm75 target sensor read %d.%d, bitmask = 0x%08x",temp.val1, temp.val2, bitmask);

	k_work_reschedule(dwork, K_MSEC(500));
}

static int lm75_target_write_requested(struct i2c_target_config *config)
{
	struct i2c_lm75_target_data *data = CONTAINER_OF(config,
						struct i2c_lm75_target_data,
						config);

	data->write_reg = true;

	return 0;
}

static int lm75_target_read_requested(struct i2c_target_config *config,
				       uint8_t *val)
{
	struct i2c_lm75_target_data *data = CONTAINER_OF(config,
						struct i2c_lm75_target_data,
						config);

	if (data->msb == true) {
		data->msb = false;
		*val = data->lm75_regs[data->reg_ptr*2];
	} else {
		*val = data->lm75_regs[data->reg_ptr*2+1];
		data->msb = true;
	}

	/* Increment will be done in the read_processed callback */

	return 0;
}

static int lm75_target_write_received(struct i2c_target_config *config,
				       uint8_t val)
{
	struct i2c_lm75_target_data *data = CONTAINER_OF(config,
						struct i2c_lm75_target_data,
						config);

	/* In case EEPROM wants to be R/O, return !0 here could trigger
	 * a NACK to the I2C controller, support depends on the
	 * I2C controller support
	 */
	if (data->write_reg) {
		data->reg_ptr = val;
		if (val != 1)
			data->msb = true;
		data->write_reg = false;
	} else if (data->reg_ptr == 0) {
		return 0;
	} else if (data->msb) {
		data->msb = false;
		data->lm75_regs[data->reg_ptr*2] = val;
	} else {
		data->msb = true;
		data->lm75_regs[data->reg_ptr*2+1] = val;
	}

	return 0;
}

static int lm75_target_read_processed(struct i2c_target_config *config,
				       uint8_t *val)
{
	struct i2c_lm75_target_data *data = CONTAINER_OF(config,
						struct i2c_lm75_target_data,
						config);
	if (data->msb) {
		*val = data->lm75_regs[data->reg_ptr*2];
		data->msb = false;
	} else {
		*val = data->lm75_regs[data->reg_ptr*2+1];
		data->msb = true;
	}

	/* Increment will be done in the next read_processed callback
	 * In case of STOP, the byte won't be taken in account
	 */

	return 0;
}

static int lm75_target_stop(struct i2c_target_config *config)
{
	struct i2c_lm75_target_data *data = CONTAINER_OF(config,
						struct i2c_lm75_target_data,
						config);

	data->write_reg = false;

	return 0;
}

static int lm75_target_register(const struct device *dev)
{
	const struct i2c_lm75_target_config *cfg = dev->config;
	struct i2c_lm75_target_data *data = dev->data;

	return i2c_target_register(cfg->bus.bus, &data->config);
}

static int lm75_target_unregister(const struct device *dev)
{
	const struct i2c_lm75_target_config *cfg = dev->config;
	struct i2c_lm75_target_data *data = dev->data;

	return i2c_target_unregister(cfg->bus.bus, &data->config);
}

static const struct i2c_target_driver_api api_funcs = {
	.driver_register = lm75_target_register,
	.driver_unregister = lm75_target_unregister,
};

static const struct i2c_target_callbacks lm75_callbacks = {
	.write_requested = lm75_target_write_requested,
	.read_requested = lm75_target_read_requested,
	.write_received = lm75_target_write_received,
	.read_processed = lm75_target_read_processed,
	.stop = lm75_target_stop,
};

static int i2c_lm75_target_init(const struct device *dev)
{
	struct i2c_lm75_target_data *data = dev->data;
	const struct i2c_lm75_target_config *cfg = dev->config;

	if (!device_is_ready(cfg->bus.bus)) {
		LOG_ERR("I2C controller device not ready");
		return -ENODEV;
	}

	data->reg_ptr = 0;
	data->write_reg = false;
	data->lm75_regs[0] = 0x11;
	data->lm75_regs[1] = 0x22;
	data->lm75_regs[2] = 0x33;
	data->lm75_regs[3] = 0x44;
	data->lm75_regs[4] = 0x55;
	data->lm75_regs[5] = 0x66;
	data->lm75_regs[6] = 0x77;
	data->lm75_regs[7] = 0x88;
	data->config.address = cfg->bus.addr;
	data->config.callbacks = &lm75_callbacks;

	if (lm75_target_register(dev) < 0) {
		LOG_ERR(" register failed");
	}

	data->dev = dev;
	k_work_init_delayable(&data->sensor_work, get_target_sensor_temp);
	k_work_schedule(&data->sensor_work, K_MSEC(500));

	return 0;
}

#ifdef CONFIG_PM_DEVICE
/*
 * TURN_OFF/TURN_ON come from the power domain this node sits on: the
 * backing target_sensor loses power along with it, so stop polling it
 * while off and resume once power is back.
 */
static int lm75_target_pm_action(const struct device *dev,
				  enum pm_device_action action)
{
	struct i2c_lm75_target_data *data = dev->data;

	switch (action) {
	case PM_DEVICE_ACTION_SUSPEND:
	case PM_DEVICE_ACTION_RESUME:
		/* No state to save: polling just re-reads the latest sample */
		break;
	case PM_DEVICE_ACTION_TURN_OFF:
		k_work_cancel_delayable(&data->sensor_work);
		break;
	case PM_DEVICE_ACTION_TURN_ON:
		k_work_reschedule(&data->sensor_work, K_MSEC(500));
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}
#endif /* CONFIG_PM_DEVICE */

#define I2C_LM75_INIT(inst)						\
	static struct i2c_lm75_target_data				\
		i2c_lm75_target_##inst##_dev_data;			\
									\
	static const struct i2c_lm75_target_config			\
		i2c_lm75_target_##inst##_cfg = {			\
		.bus = I2C_DT_SPEC_INST_GET(inst),			\
		.temp_sensor = DEVICE_DT_GET(DT_PHANDLE(DT_DRV_INST(inst), target_sensor)),  \
	};								\
									\
	PM_DEVICE_DT_INST_DEFINE(inst, lm75_target_pm_action);		\
									\
	DEVICE_DT_INST_DEFINE(inst,					\
			    &i2c_lm75_target_init,			\
			    PM_DEVICE_DT_INST_GET(inst),		\
			    &i2c_lm75_target_##inst##_dev_data,		\
			    &i2c_lm75_target_##inst##_cfg,		\
			    POST_KERNEL,				\
			    CONFIG_LM75_TARGET_INIT_PRIORITY,		\
			    &api_funcs);

DT_INST_FOREACH_STATUS_OKAY(I2C_LM75_INIT)
