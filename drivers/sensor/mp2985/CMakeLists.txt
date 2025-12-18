# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_MP2985_TEMP mp2985_temp.c)
zephyr_library_sources_ifdef(CONFIG_MP2985_VOUT mp2985_vout.c)
zephyr_library_sources_ifdef(CONFIG_MP2985_IOUT mp2985_iout.c)
zephyr_library_sources_ifdef(CONFIG_MP2985_VIN mp2985_vin.c)
