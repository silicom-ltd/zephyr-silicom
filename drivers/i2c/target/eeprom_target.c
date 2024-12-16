/*
 * Copyright (c) 2017 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT zephyr_i2c_target_eeprom

#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <errno.h>
#include <zephyr/drivers/i2c.h>
#include <string.h>
#include <zephyr/drivers/i2c/target/eeprom.h>

#define LOG_LEVEL CONFIG_I2C_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(i2c_target, LOG_LEVEL_INF);

#define I2C_REQ_ADDR 0xFE
#define I2C_FNI_ADDR 0xEF

enum msg_func {
	FUNC_FIRST           = 1,
	FINI                 = 1,
	POWER_CTRL           = 2,
	GET_ACPI_POWER_STATE = 3,
	GET_SENSOR           = 4,
	READ_FRU             = 5,

	TEST                 = 6,
	FUNC_LAST            = 6
};

enum err_code {
	ERR_INV_SEED = 1,
	ERR_INV_FUNC,
	ERR_INV_DLEN,
	ERR_INV_CSUM,
	ERR_INV_SIZE,
};

#define POWER_CTRL_REBOOT 1

/*
 * Request header's field offsets
 */
#define REQ_SEED_OFF 0
#define REQ_FUNC_OFF 1
#define REQ_SIZE_OFF 2
#define REQ_CSUM_OFF 3
#define REQ_DATA_OFF 4

#define REQ_HEAD_LEN 4

/*
 * Response header's field offsets
 */
#define RES_CODE_OFF 0
#define RES_SIZE_OFF 1
#define RES_DATA_OFF 3

#define RES_HEAD_LEN 3


#define REQ_BUFF_SIZE 64
#define RES_BUFF_SIZE 2048

#define COM_META_REQ 0x1
#define COM_META_RES 0x2
#define COM_META_ALL 0x3

#define ERR_RET_DFL -1

enum msg_status {
	STA_INIT = 0,
	STA_RCV_REQ = 1,
        STA_SND_RES = 2
};

struct i2c_eeprom_target_data {
	struct i2c_target_config config;

	bool first_write;

#if 0
	bool wr_error_occurred;
	bool rd_error_occurred;
#endif

	uint8_t status;

	/*
	 * Request's meta data
	 */

	uint16_t req_idx;
	uint16_t req_size;
	uint8_t  req_func_curr; /* used for func FINI */

	/*
	 * Response's meta data
	 */
	uint8_t retransmit;

	uint8_t batch_id;
	uint8_t batch_id_last;

	uint16_t res_idx;
	uint16_t res_idx_last;

	uint16_t res_size;

	/* helper variables */
	uint8_t *req_data;
	uint8_t *res_data;

#define req_seed req[0]
#define req_func req[1]
#define req_dlen req[2]
#define req_csum req[3]
	uint8_t req[REQ_BUFF_SIZE]; /* 0: seed, 1: func, 2: data size, 3: csum */

#define res_code res[0]
	uint8_t res[RES_BUFF_SIZE]; /* 0: retcode, 1-2: payload_size */
};

struct i2c_eeprom_target_config {
	struct i2c_dt_spec bus;
};

static void handle_request(struct i2c_eeprom_target_data *data);

static inline uint8_t next_batch_id(uint8_t curr)
{
        uint8_t v = (curr - 1) % 255;

	if (v == I2C_REQ_ADDR || v == I2C_FNI_ADDR) {
		return next_batch_id(v);
	}

        if (v == 0) {
                v = 255;
        }

	return v;
}

static inline void com_meta_reset(struct i2c_eeprom_target_data *data, int meta_sel)
{
	if (meta_sel == COM_META_ALL) {
		data->req_func_curr = 0;
	}

	if (meta_sel & COM_META_REQ) {
		data->req_idx = 0;
		data->req_size = 0;
	}

	if (meta_sel & COM_META_RES) {
		data->batch_id = 0;
		data->batch_id_last = 0;

		data->res_idx = 0;
		data->res_size = 0;
		data->res_idx_last = 0;

		data->retransmit = 0;
	}
}

static int wr_error_return (struct i2c_eeprom_target_data *data)
{
#if 0
	int ret = 0;

	if (data->wr_error_occurred == 0) {
		data->wr_error_occurred = 1;
		ret = ERR_RET_DFL;
	}

#else
	int ret = -1;
#endif
	LOG_DBG("[lom mockup] WR-RET %d", ret);
	return ret;
}

static int rd_error_return (struct i2c_eeprom_target_data *data)
{
#if 0
	int ret = 0;

	if (data->rd_error_occurred == 0) {
		data->rd_error_occurred = 1;
		ret = ERR_RET_DFL;
	}
#else
	int ret = -1;
#endif
	LOG_DBG("[lom mockup] RD-RET %d", ret);
	return ret;
}

#if 0
int eeprom_target_program(const struct device *dev, const uint8_t *eeprom_data,
			  unsigned int length)
{
	struct i2c_eeprom_target_data *data = dev->data;

	LOG_WRN("Calling in %s", __func__);
	if (length > data->buffer_size) {
		return -EINVAL;
	}

	memcpy(data->buffer, eeprom_data, length);

	return 0;
}

int eeprom_target_read(const struct device *dev, uint8_t *eeprom_data,
		      unsigned int offset)
{
	struct i2c_eeprom_target_data *data = dev->data;

	LOG_WRN("Calling in %s", __func__);
	if (!data || offset >= data->buffer_size) {
		return -EINVAL;
	}

	*eeprom_data = data->buffer[offset];

	return 0;
}
#endif

#ifdef CONFIG_I2C_EEPROM_TARGET_RUNTIME_ADDR
int eeprom_target_set_addr(const struct device *dev, uint8_t addr)
{
	const struct i2c_eeprom_target_config *cfg = dev->config;
	struct i2c_eeprom_target_data *data = dev->data;
	int ret;

	ret = i2c_target_unregister(cfg->bus.bus, &data->config);
	if (ret) {
		LOG_DBG("eeprom target failed to unregister");
		return ret;
	}

	data->config.address = addr;

	return i2c_target_register(cfg->bus.bus, &data->config);
}
#endif /* CONFIG_I2C_EEPROM_TARGET_RUNTIME_ADDR */

static int eeprom_target_write_requested(struct i2c_target_config *config)
{
	struct i2c_eeprom_target_data *data = CONTAINER_OF(config,
						struct i2c_eeprom_target_data,
						config);
	data->first_write = true;

#if 0
	data->wr_error_occurred = false;
	data->rd_error_occurred = false;
#endif

	LOG_DBG("[lom mockup] write req, first write %d.", data->first_write);

	return 0;
}

/*
 * only be called in write done with first_write == true
 */
static int status_update(struct i2c_eeprom_target_data *data, uint8_t addr)
{
	if (addr == 0) {
		return -1;
	}

	switch (data->status) {
	case STA_INIT:
		if (addr != I2C_REQ_ADDR) {
			LOG_WRN("[lom mockup] un-expected address 0x%02x", addr);
			return -1;
		}

		data->status = STA_RCV_REQ;
		LOG_DBG("[lom mockup] >> status %d => %d", STA_INIT, data->status);

		break;

	case STA_SND_RES:
		if (data->batch_id == 0) {
			LOG_ERR("[lom mockup] invalid status: batch_id == 0");
			com_meta_reset(data, COM_META_ALL);
			LOG_DBG("[lom mockup] >> status %d => %d", STA_SND_RES, data->status);
			return -1;
		}

		if (addr == I2C_REQ_ADDR) {
			LOG_WRN("[lom mockup] >> status restart");
			com_meta_reset(data, COM_META_ALL);
			data->status = STA_RCV_REQ;
			LOG_DBG("[lom mockup] >> status %d => %d", STA_SND_RES, data->status);
		}
		else if (addr == I2C_FNI_ADDR) {
			com_meta_reset(data, COM_META_REQ); /* prepare for recv FINI command */
			data->status = STA_RCV_REQ;
			LOG_DBG("[lom mockup] >> status %d => %d", STA_SND_RES, data->status);
		}
		else {
			if (data->batch_id == addr) { /* retransmit */
				LOG_WRN(">>lom mockup: batch %d retransmit", addr);
				data->retransmit = 1;
				return 0;
			}

			if (next_batch_id(data->batch_id) != addr) {
				LOG_WRN("[lom mockup] invalid batch(/un-expected address) %d, should be %d.",
					addr, next_batch_id(data->batch_id));

				return -1;
			}

			data->retransmit = 0;

			data->batch_id = addr;

			LOG_DBG("[lom mockup] goto new batch %d", addr);
		}
		break;

	default:
		LOG_ERR("[lom mockup] INVALID status %d", data->status);
		com_meta_reset(data, COM_META_ALL);
		return -1;
	}

	return 0;
}


static int eeprom_target_write_received(struct i2c_target_config *config,
					uint8_t val)
{
	struct i2c_eeprom_target_data *data = CONTAINER_OF(config,
							   struct i2c_eeprom_target_data,
							   config);

	LOG_DBG("[lom mockup] write done, first write %d val=0x%x", data->first_write, val);

	if (data->first_write) {  /* val is the address */
		data->first_write = false;

		if (status_update(data, val) < 0) {
			return wr_error_return(data);
		}
	} else {  /* this should be the request's data */
		if (data->status != STA_RCV_REQ) {
			LOG_DBG("[lom mockup] error occurred, quit");
			return wr_error_return(data);
		}

		if (data->req_idx == REQ_BUFF_SIZE) {
			LOG_ERR("[lom mockup] write exceeds the limit %d", REQ_BUFF_SIZE);
			return wr_error_return(data);
		}

		data->req[data->req_idx++] = val;
	}

	return 0;
}

/*
 * This function's return will be ignored by upper framework
 */
static int eeprom_target_stop(struct i2c_target_config *config)
{
	struct i2c_eeprom_target_data *data = CONTAINER_OF(config,
							   struct i2c_eeprom_target_data,
							   config);

	data->first_write = true;

	LOG_DBG("[lom mockup] target stop. first_write %d", data->first_write);

	if (data->status != STA_RCV_REQ) {
		LOG_DBG("[lom mockup] error occured, quit");
		return 0;
	}

	data->req_size = data->req_idx;
	data->req_data = &data->req[REQ_DATA_OFF];
	data->res_data = &data->res[RES_DATA_OFF];

	handle_request(data);

	return 0;
}

static int eeprom_target_read_requested(struct i2c_target_config *config,
					uint8_t *val)
{
	struct i2c_eeprom_target_data *data = CONTAINER_OF(config,
							   struct i2c_eeprom_target_data,
							   config);

#if 0
	if (data->wr_error_occurred) {
		LOG_WRN("[lom mockup] Read Req Error return");
		return rd_error_return(data);
	}
#endif

	if (data->status < STA_SND_RES) {
		LOG_WRN("[lom mockup] error occured, quit");
		return rd_error_return(data);
	}

	if (data->retransmit) {
		data->res_idx = data->res_idx_last;
	}
	else {
#ifdef LOM_DBG
		if (data->res_idx != 0) {
			LOG_DBG("[lom mockup] batch[%d] provided %d bytes", data->batch_id_last,
				data->res_idx - data->res_idx_last);
		}
#endif

		if (data->res_idx == data->res_size) {
			LOG_ERR("[lom mockup] read req exceeds the limit %d",
				data->res_size);
			//com_meta_reset(data, COM_META_ALL);
			//data->status = STA_INIT;

			return rd_error_return(data);
		}

		data->res_idx_last = data->res_idx;
		data->batch_id_last = data->batch_id;
	}

	*val = data->res[data->res_idx++];

	LOG_DBG("[lom mockup] read req, res_idx=%d val=0x%x", data->res_idx - 1, *val);

#ifdef LOM_DBG
	if (data->res_idx == data->res_size) {
		LOG_DBG("[lom mockup] batch[%d] provided %d bytes", data->batch_id,
			data->res_idx - data->res_idx_last);
	}
#endif

	return 0;
}

static int eeprom_target_read_processed(struct i2c_target_config *config,
					uint8_t *val)
{
	struct i2c_eeprom_target_data *data = CONTAINER_OF(config,
							   struct i2c_eeprom_target_data,
							   config);

	if (data->status < STA_SND_RES) {
		return rd_error_return(data);
	}

	if (data->res_idx == data->res_size) {
		LOG_ERR("[lom mockup] read done exceeds the limit %d, status reset", data->res_size);
		//com_meta_reset(data, COM_META_ALL);
		//data->status = STA_INIT;
		return rd_error_return(data);
	}

	*val = data->res[data->res_idx++];

	LOG_DBG("[lom mockup] read done, res_idx=%d, val=0x%x", data->res_idx - 1, *val);

#ifdef LOM_DBG
	if (data->res_idx == data->res_size) {
		LOG_DBG("[lom mockup] batch[%d] provided %d bytes", data->batch_id,
			data->res_idx - data->res_idx_last);
	}
#endif

	return 0;
}

static void update_response_meta(struct i2c_eeprom_target_data *data,
				 uint8_t code, uint16_t dat_size)
{
	data->res[RES_CODE_OFF] = code;

	/* ret payload_size */
	data->res[RES_SIZE_OFF + 0] = (dat_size >> 8) & 0xFF;
	data->res[RES_SIZE_OFF + 1] = dat_size & 0xFF;

	data->res_size = dat_size + RES_HEAD_LEN;

	data->res_idx = 0;
}

static bool check_csum(struct i2c_eeprom_target_data *data)
{
	uint8_t csum = 0, csum_org;
	int i;

	csum_org = data->req_csum;
	data->req_csum = 0;

	for (i = 0; i < data->req_size; i++) {
		csum += data->req[i];
	}

	if (csum != csum_org) {
		LOG_DBG("[lom mockup] invalid csum: org 0x%02x, should be 0x%02x",
			csum_org, csum);
		return false;
	}

	return true;
}

static void handle_request(struct i2c_eeprom_target_data *data)
{
	int i;
	uint16_t dat_size = 0;

	LOG_DBG("[lom mockup] handle request: seed %d, func %d, dlen %d",
		data->req_seed, data->req_func, data->req_dlen);

	if (data->req_seed == 0) {
		LOG_ERR("[lom mockup] invalid request, seed can't be zero");
		update_response_meta(data, ERR_INV_SEED, 0);
		return;
	}

	if (data->req_func > FUNC_LAST || data->req_func < FUNC_FIRST) {
		LOG_ERR("[lom mockup] invalid request, func %d", data->req_func);
		update_response_meta(data, ERR_INV_FUNC, 0);
		return;
	}

	if (data->req_dlen != data->req_size - REQ_HEAD_LEN) {
		LOG_ERR("[lom mockup] invalid request format");
		update_response_meta(data, ERR_INV_DLEN, 0);
		return;
	}

	if (check_csum(data) == false) {
		LOG_ERR("[lom mockup] invalid request csum");
		update_response_meta(data, ERR_INV_CSUM, 0);
		return;
	}

	/*
	 * handle request and prepare response
	 */
	switch (data->req_func) {
	case POWER_CTRL:
		data->req_func_curr = data->req_func;

		if (data->req_data[0] == POWER_CTRL_REBOOT) {
			LOG_DBG("[lom mockup] REBOOT");
			update_response_meta(data, 0, 0);
		}

		break;
	case GET_ACPI_POWER_STATE:
		data->req_func_curr = data->req_func;
		break;
	case GET_SENSOR:
		data->req_func_curr = data->req_func;
		break;
	case READ_FRU:
		data->req_func_curr = data->req_func;
		break;
	case TEST:
		data->req_func_curr = data->req_func;

		/* get test size */
		dat_size = ((uint16_t)data->req_data[0] << 8) | data->req_data[1];

		LOG_DBG("[lom mockup] test size %d", dat_size);

		if (dat_size > (RES_BUFF_SIZE - RES_HEAD_LEN)) {
			LOG_ERR("[lom mockup] request size exceeds the limit: %d",
				RES_BUFF_SIZE - RES_HEAD_LEN);
			update_response_meta(data, ERR_INV_SIZE, 0);
			return;
		}

		/* prepare payload */
		for (i = 0; i < dat_size; i++) {
			data->res_data[i] = i;
		}

		update_response_meta(data, 0, dat_size);

		break;
	case FINI:
		LOG_INF("[lom mockup] func %d completed", data->req_func_curr);
		if (data->res_idx != data->res_size) {
			LOG_WRN("[lom mockup] ! remains bytes %d", data->res_size - data->res_idx);
		}
		com_meta_reset(data, COM_META_ALL);
		data->status = STA_INIT;
		break;
	default:
		LOG_ERR("[lom mockup] unkonw request func %d", data->req_func);
		update_response_meta(data, ERR_INV_FUNC, 0);
		return;
	}

	if (data->req_func != FINI) {
		data->batch_id = data->req_seed;

		LOG_DBG("[lom mockup] >> status %d ==> %d", data->status, STA_SND_RES);
		data->status = STA_SND_RES;

		LOG_DBG("[lom mockup] new read batches should start from %d", next_batch_id(data->batch_id));
	}
}

#ifdef CONFIG_I2C_TARGET_BUFFER_MODE
static void eeprom_target_buf_write_received(struct i2c_target_config *config,

					     uint8_t *ptr, uint32_t len)
{
#if 0
	struct i2c_eeprom_target_data *data = CONTAINER_OF(config,
						struct i2c_eeprom_target_data,
						config);
	/* The first byte is offset */
	data->buffer_idx = *ptr;
	memcpy(&data->buffer[data->buffer_idx], ptr + 1, len - 1);
#endif
}

static int eeprom_target_buf_read_requested(struct i2c_target_config *config,
					    uint8_t **ptr, uint32_t *len)
{
#if 0
	struct i2c_eeprom_target_data *data = CONTAINER_OF(config,
						struct i2c_eeprom_target_data,
						config);

	*ptr = &data->buffer[data->buffer_idx];
	*len = data->buffer_size;
#endif

	return 0;
}
#endif

static int eeprom_target_register(const struct device *dev)
{
	const struct i2c_eeprom_target_config *cfg = dev->config;
	struct i2c_eeprom_target_data *data = dev->data;

//	LOG_WRN("Calling in %s", __func__);
	return i2c_target_register(cfg->bus.bus, &data->config);
}

static int eeprom_target_unregister(const struct device *dev)
{
	const struct i2c_eeprom_target_config *cfg = dev->config;
	struct i2c_eeprom_target_data *data = dev->data;

//	LOG_WRN("Calling in %s", __func__);
	return i2c_target_unregister(cfg->bus.bus, &data->config);
}

static const struct i2c_target_driver_api api_funcs = {
	.driver_register = eeprom_target_register,
	.driver_unregister = eeprom_target_unregister,
};

static const struct i2c_target_callbacks eeprom_callbacks = {
	.write_requested = eeprom_target_write_requested,
	.read_requested = eeprom_target_read_requested,
	.write_received = eeprom_target_write_received,
	.read_processed = eeprom_target_read_processed,
#ifdef CONFIG_I2C_TARGET_BUFFER_MODE
	.buf_write_received = eeprom_target_buf_write_received,
	.buf_read_requested = eeprom_target_buf_read_requested,
#endif
	.stop = eeprom_target_stop,
};

static int i2c_eeprom_target_init(const struct device *dev)
{
	struct i2c_eeprom_target_data *data = dev->data;
	const struct i2c_eeprom_target_config *cfg = dev->config;

	if (!device_is_ready(cfg->bus.bus)) {
		LOG_ERR("I2C controller device not ready");
		return -ENODEV;
	}

	LOG_INF("[lom mockup] init addr 0x%x", cfg->bus.addr);

	data->config.address = cfg->bus.addr;
	data->config.callbacks = &eeprom_callbacks;

	if (eeprom_target_register(dev) < 0)
		LOG_ERR("%s, Register failed", __func__);

	return 0;
}

#define I2C_EEPROM_INIT(inst)						\
	static struct i2c_eeprom_target_data				\
		i2c_eeprom_target_##inst##_dev_data;			\
									\
	static const struct i2c_eeprom_target_config			\
		i2c_eeprom_target_##inst##_cfg = {			\
		.bus = I2C_DT_SPEC_INST_GET(inst),			\
	};								\
									\
	DEVICE_DT_INST_DEFINE(inst,					\
			    &i2c_eeprom_target_init,			\
			    NULL,			\
			    &i2c_eeprom_target_##inst##_dev_data,	\
			    &i2c_eeprom_target_##inst##_cfg,		\
			    POST_KERNEL,				\
			    CONFIG_I2C_TARGET_INIT_PRIORITY,		\
			    &api_funcs);

DT_INST_FOREACH_STATUS_OKAY(I2C_EEPROM_INIT)
