# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_CRPS_TEMP crps_temp.c)
zephyr_library_sources_ifdef(CONFIG_CRPS_IOUT crps_iout.c)
zephyr_library_sources_ifdef(CONFIG_CRPS_POWER crps_power.c)
