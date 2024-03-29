/*
 * Copyright (c) 2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Full C support initialization
 *
 *
 * Initialization of full C support: zero the .bss, copy the .data if XIP,
 * call z_cstart().
 *
 * Stack is available in this module, but not the global data/bss until their
 * initialization is performed.
 */

#include <zephyr/types.h>
#include <zephyr/toolchain.h>
#include <zephyr/linker/linker-defs.h>
#include <zephyr/arch/arc/v2/aux_regs.h>
#include <zephyr/kernel_structs.h>
#include <kernel_internal.h>

/* XXX - keep for future use in full-featured cache APIs */
#if 0
/**
 * @brief Disable the i-cache if present
 *
 * For those ARC CPUs that have a i-cache present,
 * invalidate the i-cache and then disable it.
 */

static void disable_icache(void)
{
	unsigned int val;

	val = z_arc_v2_aux_reg_read(_ARC_V2_I_CACHE_BUILD);
	val &= 0xff; /* version field */
	if (val == 0) {
		return; /* skip if i-cache is not present */
	}
	z_arc_v2_aux_reg_write(_ARC_V2_IC_IVIC, 0);
	__builtin_arc_nop();
	z_arc_v2_aux_reg_write(_ARC_V2_IC_CTRL, 1);
}

/**
 * @brief Invalidate the data cache if present
 *
 * For those ARC CPUs that have a data cache present,
 * invalidate the data cache.
 */

static void invalidate_dcache(void)
{
	unsigned int val;

	val = z_arc_v2_aux_reg_read(_ARC_V2_D_CACHE_BUILD);
	val &= 0xff; /* version field */
	if (val == 0) {
		return; /* skip if d-cache is not present */
	}
	z_arc_v2_aux_reg_write(_ARC_V2_DC_IVDC, 1);
}
#endif

#ifdef __CCAC__
extern char __device_states_start[];
extern char __device_states_end[];
/**
 * @brief Clear device_states section
 *
 * This routine clears the device_states section,
 * as MW compiler marks the section with NOLOAD flag.
 */
static void dev_state_zero(void)
{
	z_early_memset(__device_states_start, 0, __device_states_end - __device_states_start);
}
#endif

extern FUNC_NORETURN void z_cstart(void);
/**
 * @brief Prepare to and run C code
 *
 * This routine prepares for the execution of and runs C code.
 */

void z_prep_c(void)
{
	z_bss_zero();
#ifdef __CCAC__
	dev_state_zero();
#endif
	z_data_copy();
	z_cstart();
	CODE_UNREACHABLE;
}
