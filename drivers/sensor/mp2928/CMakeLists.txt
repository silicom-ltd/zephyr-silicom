# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_MP2928_TEMP mp2928_temp.c)
zephyr_library_sources_ifdef(CONFIG_MP2928_VOUT mp2928_vout.c)
