/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Microchip XEC I2C/SMB "network layer" driver: unlike i2c_mchp_xec_v2.c,
 * which drives the controller's legacy byte-at-a-time Data/Control/Status
 * registers and busy-polls the PIN bit for every byte, this driver uses the
 * controller's hardware Master Command state machine (MSTR_CMD/COMPL
 * registers) together with an external DMA channel to move whole
 * transactions, waking up only on completion (ENMI/MDONE) instead of
 * spinning the CPU. See the MEC172x I2C/SMB Controller data sheet (DS00003501),
 * Section 6.2, "SMBus Network Layer Interface", for the hardware model this
 * implements.
 *
 * Current scope: Controller (master) role only, and at most one write
 * message followed by at most one read message per i2c_transfer() call
 * (the common PMBus/SMBus "write register pointer, then read" pattern).
 * Arbitrary message chains and Slave/target-mode network-layer support are
 * not implemented.
 */

#define DT_DRV_COMPAT microchip_xec_i2c_v2

#include <zephyr/kernel.h>
#include <soc.h>
#include <errno.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/mchp_xec_clock_control.h>
#include <zephyr/drivers/dma.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/interrupt_controller/intc_mchp_xec_ecia.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/logging/log.h>
#include <zephyr/irq.h>
LOG_MODULE_REGISTER(i2c_mchp_nl, CONFIG_I2C_LOG_LEVEL);

#include "i2c-priv.h"

#define SPEED_100KHZ_BUS	0
#define SPEED_400KHZ_BUS	1
#define SPEED_1MHZ_BUS		2

#define EC_OWN_I2C_ADDR		0x7Fu
#define RESET_WAIT_US		20

/* Completion-wait timeout: generous vs. the legacy driver's 10ms byte
 * timeout, since a full block transfer can take longer than a single byte,
 * but this is still meant to catch a wedged bus/DMA, not normal operation.
 */
#define I2C_XEC_NL_XFER_TIMEOUT	K_MSEC(100)

enum i2c_xec_nl_result {
	I2C_XEC_NL_OK = 0,
	I2C_XEC_NL_ERR_BUS,
	I2C_XEC_NL_ERR_LAB,
	I2C_XEC_NL_ERR_NACK,
	I2C_XEC_NL_ERR_TIMEOUT,
	I2C_XEC_NL_ERR_DMA,
};

struct i2c_xec_nl_config {
	uint32_t port_sel;
	uint32_t base_addr;
	uint32_t clock_freq;
	uint8_t girq;
	uint8_t girq_pos;
	uint8_t pcr_idx;
	uint8_t pcr_bitpos;
	const struct pinctrl_dev_config *pcfg;
	const struct device *dma_dev;
	uint32_t dma_channel;
	uint32_t dma_slot;
	void (*irq_config_func)(void);
};

struct i2c_xec_nl_data {
	struct k_mutex bus_lock;
	struct k_sem xfer_sem;
	uint32_t i2c_compl;
	uint8_t speed_id;
	/* Scratch header bytes DMA'd ahead of/between the caller's message
	 * buffers: [addr_w] for a write, or [addr_w, addr_r] framing a
	 * write-then-read (addr_r is only sent once STARTN fires).
	 */
	uint8_t whdr[2];
};

static const struct xec_speed_cfg {
	uint32_t bus_clk;
	uint32_t data_timing;
	uint32_t start_hold_time;
	uint32_t idle_scale;
	uint32_t timeout_scale;
} xec_nl_cfg_params[] = {
	[SPEED_100KHZ_BUS] = {
		.bus_clk            = 0x00004F4F,
		.data_timing        = 0x0C4D5006,
		.start_hold_time    = 0x0000004D,
		.idle_scale         = 0x01FC01ED,
		.timeout_scale      = 0x4B9CC2C7,
	},
	[SPEED_400KHZ_BUS] = {
		.bus_clk            = 0x00000F17,
		.data_timing        = 0x040A0A06,
		.start_hold_time    = 0x0000000A,
		.idle_scale         = 0x01000050,
		.timeout_scale      = 0x159CC2C7,
	},
	[SPEED_1MHZ_BUS] = {
		.bus_clk            = 0x00000509,
		.data_timing        = 0x04060601,
		.start_hold_time    = 0x00000006,
		.idle_scale         = 0x10000050,
		.timeout_scale      = 0x089CC2C7,
	},
};

static int __maybe_unused i2c_xec_nl_reset_config(const struct device *dev)
{
	const struct i2c_xec_nl_config *cfg = dev->config;
	struct i2c_xec_nl_data *data = dev->data;
	struct i2c_smb_regs *regs = (struct i2c_smb_regs *)cfg->base_addr;

	z_mchp_xec_pcr_periph_reset(cfg->pcr_idx, cfg->pcr_bitpos);

	regs->CFG = MCHP_I2C_SMB_CFG_FLUSH_SXBUF_WO | MCHP_I2C_SMB_CFG_FLUSH_SRBUF_WO |
		    MCHP_I2C_SMB_CFG_FLUSH_MXBUF_WO | MCHP_I2C_SMB_CFG_FLUSH_MRBUF_WO;

	mchp_xec_ecia_girq_src_clr(cfg->girq, cfg->girq_pos);

	/* PIN=1 synchronizes the legacy status bits; harmless for NL mode. */
	regs->CTRLSTS = MCHP_I2C_SMB_CTRL_PIN;

	regs->OWN_ADDR = EC_OWN_I2C_ADDR | (EC_OWN_I2C_ADDR << 8);

	regs->CFG |= BIT(14); /* disable general call */
	regs->CFG |= MCHP_I2C_SMB_CFG_FEN;
	regs->CFG |= (cfg->port_sel & MCHP_I2C_SMB_CFG_PORT_SEL_MASK);

	regs->BUSCLK = xec_nl_cfg_params[data->speed_id].bus_clk;
	regs->RSHTM = xec_nl_cfg_params[data->speed_id].start_hold_time;
	regs->DATATM = xec_nl_cfg_params[data->speed_id].data_timing;
	regs->TMOUTSC = xec_nl_cfg_params[data->speed_id].timeout_scale;
	regs->IDLSC = xec_nl_cfg_params[data->speed_id].idle_scale;

	regs->CTRLSTS = MCHP_I2C_SMB_CTRL_PIN | MCHP_I2C_SMB_CTRL_ESO | MCHP_I2C_SMB_CTRL_ACK;

	/* Enable the controller and the Master Done interrupt; the network
	 * layer state machine is driven entirely through MSTR_CMD/COMPL from
	 * here on, not the legacy Control register's ENI/PIN bits.
	 */
	regs->CFG |= MCHP_I2C_SMB_CFG_ENAB | MCHP_I2C_SMB_CFG_ENMI;

	k_busy_wait(RESET_WAIT_US);

	return 0;
}

static void __maybe_unused i2c_xec_nl_isr(const struct device *dev)
{
	const struct i2c_xec_nl_config *cfg = dev->config;
	struct i2c_xec_nl_data *data = dev->data;
	struct i2c_smb_regs *regs = (struct i2c_smb_regs *)cfg->base_addr;

	/* Capture and clear (COMPL is R/WC) in the ISR; all classification
	 * and any DMA reconfiguration happens in thread context in
	 * i2c_xec_nl_transfer(), not here.
	 */
	data->i2c_compl = regs->COMPL;
	regs->COMPL = data->i2c_compl & MCHP_I2C_SMB_CMPL_MASK;

	mchp_xec_ecia_girq_src_clr(cfg->girq, cfg->girq_pos);

	k_sem_give(&data->xfer_sem);
}

/* Starts (or re-starts, after a REPEAT_WRITE/REPEAT_READ pause) the Master
 * DMA channel and MSTR_CMD, then waits for the next completion interrupt.
 * block/nblocks describe this phase's DMA transfer only; mstr_cmd is the
 * full MSTR_CMD value to write (software must rewrite WRITE_COUNT,
 * READ_COUNT, START0/STARTN/STOP and MRUN/MPROCEED even when only MPROCEED
 * changes, per the data sheet's Master Command Register description).
 */
static int __maybe_unused i2c_xec_nl_run_phase(const struct device *dev,
						struct dma_block_config *blocks,
						size_t nblocks, uint32_t mstr_cmd)
{
	const struct i2c_xec_nl_config *cfg = dev->config;
	struct i2c_xec_nl_data *data = dev->data;
	struct i2c_smb_regs *regs = (struct i2c_smb_regs *)cfg->base_addr;
	bool is_write = (mstr_cmd & GENMASK(23, 16)) != 0 &&
			(mstr_cmd & GENMASK(31, 24)) == 0;
	struct dma_config dma_cfg = {
		.dma_slot = cfg->dma_slot,
		.channel_direction = is_write ? MEMORY_TO_PERIPHERAL : PERIPHERAL_TO_MEMORY,
		.complete_callback_en = 0,
		.error_callback_en = 0,
		.source_data_size = 1,
		.dest_data_size = 1,
		.block_count = nblocks,
		.head_block = blocks,
	};
	int ret;

	for (size_t i = 0; i + 1 < nblocks; i++) {
		blocks[i].next_block = &blocks[i + 1];
	}
	blocks[nblocks - 1].next_block = NULL;

	ret = dma_config(cfg->dma_dev, cfg->dma_channel, &dma_cfg);
	if (ret) {
		LOG_ERR("DMA config failed: %d", ret);
		return I2C_XEC_NL_ERR_DMA;
	}

	ret = dma_start(cfg->dma_dev, cfg->dma_channel);
	if (ret) {
		LOG_ERR("DMA start failed: %d", ret);
		return I2C_XEC_NL_ERR_DMA;
	}

	regs->MCMD = mstr_cmd;

	ret = k_sem_take(&data->xfer_sem, I2C_XEC_NL_XFER_TIMEOUT);
	if (ret) {
		dma_stop(cfg->dma_dev, cfg->dma_channel);
		LOG_ERR("I2C NL transaction timed out");
		return I2C_XEC_NL_ERR_TIMEOUT;
	}

	if (data->i2c_compl & (MCHP_I2C_SMB_CMPL_BER_RWC | MCHP_I2C_SMB_CMPL_LAB_RWC)) {
		dma_stop(cfg->dma_dev, cfg->dma_channel);
		i2c_xec_nl_reset_config(dev);
		return (data->i2c_compl & MCHP_I2C_SMB_CMPL_BER_RWC) ?
			I2C_XEC_NL_ERR_BUS : I2C_XEC_NL_ERR_LAB;
	}

	if (data->i2c_compl & MCHP_I2C_SMB_CMPL_MNAKX_RWC) {
		return I2C_XEC_NL_ERR_NACK;
	}

	return I2C_XEC_NL_OK;
}

static int __maybe_unused i2c_xec_nl_transfer(const struct device *dev, struct i2c_msg *msgs,
						uint8_t num_msgs, uint16_t addr)
{
	const struct i2c_xec_nl_config *cfg = dev->config;
	struct i2c_xec_nl_data *data = dev->data;
	struct i2c_smb_regs *regs = (struct i2c_smb_regs *)cfg->base_addr;
	struct dma_block_config blocks[3] = {0};
	uint32_t mstr_cmd;
	bool do_write = num_msgs >= 1 && (msgs[0].flags & I2C_MSG_RW_MASK) == I2C_MSG_WRITE;
	bool do_read = (num_msgs == 2) && (msgs[1].flags & I2C_MSG_RW_MASK) == I2C_MSG_READ;
	int ret;

	if (num_msgs < 1 || num_msgs > 2 || !do_write || (num_msgs == 2 && !do_read)) {
		LOG_ERR("Unsupported message pattern (num_msgs=%d)", num_msgs);
		return -ENOTSUP;
	}

	k_mutex_lock(&data->bus_lock, K_FOREVER);

	data->whdr[0] = (uint8_t)((addr & 0x7Fu) << 1);

	blocks[0] = (struct dma_block_config){
		.source_address = (uint32_t)&data->whdr[0],
		.dest_address = (uint32_t)&regs->MTR_TXB,
		.block_size = 1,
		.source_addr_adj = DMA_ADDR_ADJ_INCREMENT,
		.dest_addr_adj = DMA_ADDR_ADJ_NO_CHANGE,
	};
	blocks[1] = (struct dma_block_config){
		.source_address = (uint32_t)msgs[0].buf,
		.dest_address = (uint32_t)&regs->MTR_TXB,
		.block_size = msgs[0].len,
		.source_addr_adj = DMA_ADDR_ADJ_INCREMENT,
		.dest_addr_adj = DMA_ADDR_ADJ_NO_CHANGE,
	};

	mstr_cmd = MCHP_I2C_SMB_MSTR_CMD_B0_MRUN | MCHP_I2C_SMB_MSTR_CMD_B0_MPROCEED;
	mstr_cmd |= (uint32_t)(1 + msgs[0].len) << MCHP_I2C_SMB_MSTR_CMD_WR_CNT_POS;
	mstr_cmd |= MCHP_I2C_SMB_MSTR_CMD_START0;

	if (do_read) {
		/* Trailing read-mode address byte, sent via STARTN right
		 * before the repeated START; READ_COUNT is a placeholder
		 * here (overwritten below once the write phase completes).
		 */
		data->whdr[1] = (uint8_t)(((addr & 0x7Fu) << 1) | BIT(0));
		blocks[2] = (struct dma_block_config){
			.source_address = (uint32_t)&data->whdr[1],
			.dest_address = (uint32_t)&regs->MTR_TXB,
			.block_size = 1,
			.source_addr_adj = DMA_ADDR_ADJ_INCREMENT,
			.dest_addr_adj = DMA_ADDR_ADJ_NO_CHANGE,
		};
		mstr_cmd += (uint32_t)1 << MCHP_I2C_SMB_MSTR_CMD_WR_CNT_POS;
		mstr_cmd |= MCHP_I2C_SMB_MSTR_CMD_STARTN;
	} else {
		mstr_cmd |= MCHP_I2C_SMB_MSTR_CMD_STOP;
	}

	ret = i2c_xec_nl_run_phase(dev, blocks, do_read ? 3 : 2, mstr_cmd);
	if (ret != I2C_XEC_NL_OK || !do_read) {
		goto done;
	}

	/* Write phase complete; reconfigure the same DMA channel for the
	 * read phase and let the state machine proceed (Data sheet Table
	 * 6-13, steps 8-11).
	 */
	blocks[0] = (struct dma_block_config){
		.source_address = (uint32_t)&regs->MTR_RXB,
		.dest_address = (uint32_t)msgs[1].buf,
		.block_size = msgs[1].len,
		.source_addr_adj = DMA_ADDR_ADJ_NO_CHANGE,
		.dest_addr_adj = DMA_ADDR_ADJ_INCREMENT,
	};

	mstr_cmd = MCHP_I2C_SMB_MSTR_CMD_B0_MRUN | MCHP_I2C_SMB_MSTR_CMD_B0_MPROCEED;
	mstr_cmd |= (uint32_t)msgs[1].len << MCHP_I2C_SMB_MSTR_CMD_RD_CNT_POS;
	mstr_cmd |= MCHP_I2C_SMB_MSTR_CMD_STOP;

	ret = i2c_xec_nl_run_phase(dev, blocks, 1, mstr_cmd);

done:
	k_mutex_unlock(&data->bus_lock);

	switch (ret) {
	case I2C_XEC_NL_OK:
		return 0;
	case I2C_XEC_NL_ERR_NACK:
		return -EIO;
	case I2C_XEC_NL_ERR_TIMEOUT:
		return -ETIMEDOUT;
	case I2C_XEC_NL_ERR_DMA:
		return -EIO;
	default:
		return -EIO;
	}
}

static int __maybe_unused i2c_xec_nl_configure(const struct device *dev, uint32_t dev_config)
{
	struct i2c_xec_nl_data *data = dev->data;

	switch (I2C_SPEED_GET(dev_config)) {
	case I2C_SPEED_STANDARD:
		data->speed_id = SPEED_100KHZ_BUS;
		break;
	case I2C_SPEED_FAST:
		data->speed_id = SPEED_400KHZ_BUS;
		break;
	case I2C_SPEED_FAST_PLUS:
		data->speed_id = SPEED_1MHZ_BUS;
		break;
	default:
		return -EINVAL;
	}

	return i2c_xec_nl_reset_config(dev);
}

static const struct i2c_driver_api __maybe_unused i2c_xec_nl_driver_api = {
	.configure = i2c_xec_nl_configure,
	.transfer = i2c_xec_nl_transfer,
};

static int __maybe_unused i2c_xec_nl_init(const struct device *dev)
{
	const struct i2c_xec_nl_config *cfg = dev->config;
	struct i2c_xec_nl_data *data = dev->data;
	int ret;

	k_mutex_init(&data->bus_lock);
	k_sem_init(&data->xfer_sem, 0, 1);

	ret = pinctrl_apply_state(cfg->pcfg, PINCTRL_STATE_DEFAULT);
	if (ret) {
		return ret;
	}

	if (!device_is_ready(cfg->dma_dev)) {
		LOG_ERR("DMA device not ready");
		return -ENODEV;
	}

	data->speed_id = SPEED_100KHZ_BUS;

	cfg->irq_config_func();

	return i2c_xec_nl_reset_config(dev);
}

#define I2C_XEC_NL_DEVICE(n)							\
	PINCTRL_DT_INST_DEFINE(n);						\
										\
	static void i2c_xec_nl_irq_config_func_##n(void);			\
										\
	static struct i2c_xec_nl_data i2c_xec_nl_data_##n;			\
	static const struct i2c_xec_nl_config i2c_xec_nl_config_##n = {	\
		.base_addr = DT_INST_REG_ADDR(n),				\
		.port_sel = DT_INST_PROP(n, port_sel),				\
		.clock_freq = DT_INST_PROP(n, clock_frequency),		\
		.girq = DT_INST_PROP_BY_IDX(n, girqs, 0),			\
		.girq_pos = DT_INST_PROP_BY_IDX(n, girqs, 1),			\
		.pcr_idx = DT_INST_PROP_BY_IDX(n, pcrs, 0),			\
		.pcr_bitpos = DT_INST_PROP_BY_IDX(n, pcrs, 1),			\
		.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),			\
		.dma_dev = DEVICE_DT_GET(DT_INST_DMAS_CTLR_BY_NAME(n, mtr)),	\
		.dma_channel = DT_INST_DMAS_CELL_BY_NAME(n, mtr, channel),	\
		.dma_slot = DT_INST_DMAS_CELL_BY_NAME(n, mtr, trigsrc),	\
		.irq_config_func = i2c_xec_nl_irq_config_func_##n,		\
	};									\
	I2C_DEVICE_DT_INST_DEFINE(n, i2c_xec_nl_init, NULL,			\
		&i2c_xec_nl_data_##n, &i2c_xec_nl_config_##n,			\
		POST_KERNEL, CONFIG_I2C_INIT_PRIORITY,				\
		&i2c_xec_nl_driver_api);					\
										\
	static void i2c_xec_nl_irq_config_func_##n(void)			\
	{									\
		IRQ_CONNECT(DT_INST_IRQN(n),					\
			    DT_INST_IRQ(n, priority),				\
			    i2c_xec_nl_isr,					\
			    DEVICE_DT_INST_GET(n), 0);				\
		irq_enable(DT_INST_IRQN(n));					\
	}

#define I2C_XEC_NL_DEVICE_COND(n)						\
	COND_CODE_1(DT_INST_DMAS_HAS_NAME(n, mtr), (I2C_XEC_NL_DEVICE(n)), ())

DT_INST_FOREACH_STATUS_OKAY(I2C_XEC_NL_DEVICE_COND)
