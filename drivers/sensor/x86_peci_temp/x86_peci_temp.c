/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT x86_peci_temp

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/peci.h>
#include <zephyr/pm/device.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(x86_peci_temp, CONFIG_SENSOR_LOG_LEVEL);

#define PECI_HOST_ADDR		0x30u
#define PECI_CONFIGINDEX_TJMAX	16u
#define PECI_CONFIGHOSTID	0u
#define PECI_CONFIGPARAM	0u

#define PECI_SAFE_TEMP		72


struct peci_dev_config {
	const struct device *peci_dev;
};

struct x86_peci_temp_data {
	const struct device *dev;
	struct k_work_delayable peci_check_work;
	bool cpu_up;
	/* Set once by peci_ping(); TjMax doesn't change across host power cycles */
	bool tjmax_valid;
	uint8_t tjmax;
	float temp_out;
};

static int peci_get_tjmax(const struct device *dev, uint8_t *tjmax);

/*
 * Runs in the system workqueue thread, not ISR context (unlike the k_timer
 * expiry function this replaces), so peci_get_tjmax() is free to block.
 * Reschedules itself once a second until a valid TjMax is read, then stops.
 */
static void peci_ping(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct x86_peci_temp_data *data =
		CONTAINER_OF(dwork, struct x86_peci_temp_data, peci_check_work);
	const struct peci_dev_config *config = data->dev->config;
	int ret;

	ret = peci_get_tjmax(config->peci_dev, &data->tjmax);

	if (ret != 0) {
		k_work_reschedule(dwork, K_SECONDS(1));
		return;
	}

	LOG_DBG("Got TJMax %d",data->tjmax);
	data->tjmax_valid = true;
	data->cpu_up = true;
}

/* 
 * Utility functions taken directly from samples
 */
static int peci_get_tjmax(const struct device *dev, uint8_t *tjmax)
{
	int ret;
	int retries = 3;
	uint8_t peci_resp;
	uint8_t rx_fcs;
	struct peci_msg packet;

	uint8_t peci_resp_buf[PECI_RD_PKG_LEN_DWORD+1] = {0};
	uint8_t peci_req_buf[] = { PECI_CONFIGHOSTID,
				PECI_CONFIGINDEX_TJMAX,
				PECI_CONFIGPARAM & 0x00FF,
				(PECI_CONFIGPARAM & 0xFF00) >> 8,
	};

	packet.tx_buffer.buf = peci_req_buf;
	packet.tx_buffer.len = PECI_RD_PKG_WR_LEN;
	packet.rx_buffer.buf = peci_resp_buf;
	packet.rx_buffer.len = PECI_RD_PKG_LEN_DWORD;

	do {
		rx_fcs = 0;
		packet.addr = PECI_HOST_ADDR;
		packet.cmd_code = PECI_CMD_RD_PKG_CFG0;

		ret = peci_transfer(dev, &packet);

		peci_resp = packet.rx_buffer.buf[0];
		rx_fcs = packet.rx_buffer.buf[PECI_RD_PKG_LEN_DWORD];
		k_sleep(K_MSEC(1));
		retries--;
	} while ((peci_resp != PECI_CC_RSP_SUCCESS) && (retries > 0));

	if (peci_resp != PECI_CC_RSP_SUCCESS)
		return -1;

	*tjmax = packet.rx_buffer.buf[3];

	return 0;
}

static int peci_get_temp(const struct device *dev, float *temperature)
{
	const struct peci_dev_config *config = dev->config;
	struct x86_peci_temp_data *data = dev->data;
	int16_t raw_cpu_temp;
	double tmp;
	uint8_t rx_fcs;
	int ret;
	struct peci_msg packet = {0};

	uint8_t peci_resp_buf[PECI_GET_TEMP_RD_LEN+1];

	if (!data->cpu_up) return -ENODEV;

	rx_fcs = 0;
	packet.tx_buffer.buf = NULL;
	packet.tx_buffer.len = PECI_GET_TEMP_WR_LEN;
	packet.rx_buffer.buf = peci_resp_buf;
	packet.rx_buffer.len = PECI_GET_TEMP_RD_LEN;

	packet.addr = PECI_HOST_ADDR;
	packet.cmd_code = PECI_CMD_GET_TEMP0;

	ret = peci_transfer(config->peci_dev, &packet);
	if (ret) {
		LOG_ERR("PECI get temp failed %d",ret);
		return ret;
	}

	rx_fcs = packet.rx_buffer.buf[PECI_GET_TEMP_RD_LEN];
	raw_cpu_temp = (int16_t)(packet.rx_buffer.buf[PECI_GET_TEMP_LSB] |
			(int16_t)((packet.rx_buffer.buf[PECI_GET_TEMP_MSB] << 8) & 0xFF00));

	if (raw_cpu_temp == 0x8000) {
		LOG_ERR("Invalid PECI temp 0x8000");
		*temperature = PECI_SAFE_TEMP;
		return -1;
	}

	raw_cpu_temp = ~raw_cpu_temp + 1;
	*temperature = (float)(raw_cpu_temp & 0x3F) / 64;
	raw_cpu_temp = (raw_cpu_temp >> 6);
	tmp = (double)(*temperature + raw_cpu_temp);
	tmp = (double)data->tjmax - tmp;
	*temperature = (float)tmp;

	LOG_DBG("PECI CPU temp %f",(double)*temperature);

	return 0;
}

static int x86_peci_temp_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	struct x86_peci_temp_data *data = dev->data;
	int ret;

	if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_DIE_TEMP) {
		return -ENOTSUP;
	}
	ret = peci_get_temp(dev, &data->temp_out);

	return ret;
}

static int x86_peci_temp_channel_get(const struct device *dev, enum sensor_channel chan,
				    struct sensor_value *val)
{
	struct x86_peci_temp_data *data = dev->data;

	if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_DIE_TEMP) {
		return -ENOTSUP;
	}

	return sensor_value_from_float(val, data->temp_out);
}

static const struct sensor_driver_api x86_peci_temp_driver_api = {
	.sample_fetch = x86_peci_temp_sample_fetch,
	.channel_get = x86_peci_temp_channel_get,
};

static int x86_peci_temp_init(const struct device *dev)
{
	const struct peci_dev_config *config = dev->config;
	struct x86_peci_temp_data *data = dev->data;
	int ret;

	ret = peci_config(config->peci_dev, 1000u);
	if (ret) {
		LOG_ERR("Failed to configure bitrate");
		return 0;
	}

	peci_enable(config->peci_dev);

	data->dev = dev;
	k_work_init_delayable(&data->peci_check_work, peci_ping);
	k_work_schedule(&data->peci_check_work, K_SECONDS(5));

	return 0;
}

#ifdef CONFIG_PM_DEVICE
/*
 * TURN_OFF/TURN_ON come from the power domain the node sits on (e.g. one that
 * follows host S0). They only gate reads through cpu_up: peci_ping() runs
 * only after EC boot, and TjMax isn't re-read on host power cycles. If the
 * first ping hasn't succeeded yet, it is left to set cpu_up itself.
 */
static int x86_peci_temp_pm_action(const struct device *dev,
				   enum pm_device_action action)
{
	struct x86_peci_temp_data *data = dev->data;

	switch (action) {
	case PM_DEVICE_ACTION_SUSPEND:
	case PM_DEVICE_ACTION_RESUME:
		/* No state to save: the bus is idle between transfers */
		break;
	case PM_DEVICE_ACTION_TURN_OFF:
		data->cpu_up = false;
		break;
	case PM_DEVICE_ACTION_TURN_ON:
		data->cpu_up = data->tjmax_valid;
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}
#endif /* CONFIG_PM_DEVICE */

#define X86_PECI_TEMP_DEFINE(inst)								\
	static struct x86_peci_temp_data x86_peci_temp_dev_data_##inst;				\
												\
	static const struct peci_dev_config peci_dev_config_##inst = {				\
		.peci_dev = DEVICE_DT_GET(DT_PHANDLE(DT_DRV_INST(inst), peci_dev)),		\
	};											\
												\
	PM_DEVICE_DT_INST_DEFINE(inst, x86_peci_temp_pm_action);				\
												\
	SENSOR_DEVICE_DT_INST_DEFINE(inst, x86_peci_temp_init, PM_DEVICE_DT_INST_GET(inst),	\
			      &x86_peci_temp_dev_data_##inst, &peci_dev_config_##inst,		\
			      POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,				\
			      &x86_peci_temp_driver_api);					\

DT_INST_FOREACH_STATUS_OKAY(X86_PECI_TEMP_DEFINE)
