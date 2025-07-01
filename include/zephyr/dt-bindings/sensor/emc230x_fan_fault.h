/*
 * Copyright (c) 2025 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_EMC230X_FAN_FAULT_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_EMC230X_FAN_FAULT_H_

#include <zephyr/dt-bindings/dt-util.h>
/**
 * @brief EMC230X Fan Fault Interface
 * @defgroup emc230x_fan_faul_interface Fan Fault Interface
 * @ingroup sensor_interfaces
 * @{
 */

/**
 * @name EMC230X_FAN_FAULT flags
 *
 * The flags are on the lower 8bits of the emc230x_fan_fault_flags_t
 * @{
 */
/** FAN Fault types */
#define EMC230X_FAN_FAULT_STALL  BIT(0)
#define EMC230X_FAN_FAULT_SPINUP BIT(1)
#define EMC230X_FAN_FAULT_DRIVE  BIT(2)
/** @} */

/** @} */

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_EMC230X_FAN_FAULT_H_ */
