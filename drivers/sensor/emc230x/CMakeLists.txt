# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_EMC230X_FAN_SPEED emc230x_fan_speed.c)
zephyr_library_sources_ifdef(CONFIG_EMC230X_FAN_FAULT emc230x_fan_fault.c)
