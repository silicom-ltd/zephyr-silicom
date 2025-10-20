# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_MPQ8785_VIN mpq8785_vin.c)
zephyr_library_sources_ifdef(CONFIG_MPQ8785_VOUT mpq8785_vout.c)
zephyr_library_sources_ifdef(CONFIG_MPQ8785_IOUT mpq8785_iout.c)
zephyr_library_sources_ifdef(CONFIG_MPQ8785_TEMP mpq8785_temp.c)
