/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_DMA_MCHP_XEC_I2C_DMA_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_DMA_MCHP_XEC_I2C_DMA_H_

/*
 * XEC DMA controller hardware flow-control device numbers for the I2C/SMB
 * network layer's Master state machine, one pair (Slave, Master) per I2C/SMB
 * controller instance.
 *
 * NOTE: these values are taken from the MEC1501 HAL
 * (modules/hal/microchip/mec/mec1501/component/dma.h,
 * MCHP_DMA_DEVNUM_SMB<n>_SLV / MCHP_DMA_DEVNUM_SMB<n>_MTR), since no
 * MEC172x-specific HAL component header exists in this tree. The I2C/SMB
 * register layout in soc/arm/microchip_mec/mec172x/reg/mec172x_i2c_smb.h
 * matches the MEC1501 table 1:1, but this DMA device-number assignment is
 * chip-integration information, not core-IP information, and has NOT been
 * independently confirmed against a MEC172x datasheet. Verify against the
 * full MEC172x datasheet's DMA chapter before relying on this on real
 * hardware.
 */
#define MCHP_XEC_I2C_DMA_SLV(n)		(2 * (n))
#define MCHP_XEC_I2C_DMA_MTR(n)		(2 * (n) + 1)

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_DMA_MCHP_XEC_I2C_DMA_H_ */
