# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_MPQ8655_VOUT mpq8655_vout.c)
zephyr_library_sources_ifdef(CONFIG_MPQ8655_IOUT mpq8655_iout.c)
zephyr_library_sources_ifdef(CONFIG_MPQ8655_VIN mpq8655_vin.c)
zephyr_library_sources_ifdef(CONFIG_MPQ8655_TEMP mpq8655_temp.c)
