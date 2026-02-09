# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_MAESTRO_SWITCH_TEMP maestro_switch_temp.c)
