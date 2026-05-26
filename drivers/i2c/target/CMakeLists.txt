# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources_ifdef(CONFIG_I2C_EEPROM_TARGET eeprom_target.c)
zephyr_library_sources_ifdef(CONFIG_I2C_LM75_TARGET lm75_target.c)
