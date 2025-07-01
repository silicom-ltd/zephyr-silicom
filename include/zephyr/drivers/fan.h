#ifndef ZEPHYR_INCLUDE_DRIVERS_FAN_H_
#define ZEPHYR_INCLUDE_DRIVERS_FAN_H_

#include <errno.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/toolchain.h>
#include <zephyr/sys/util_macro.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*fan_set_config_t)(const struct device *dev);
typedef int (*fan_set_cycles_t)(const struct device *dev, uint32_t cycles);

__subsystem struct fan_parent_driver_api {
	fan_set_config_t set_config;
	fan_set_cycles_t set_cycles;
};

__syscall int fan_set_cycles(const struct device *dev, uint32_t pulse);

static inline int z_impl_fan_set_cycles(const struct device *dev, uint32_t pulse)
{
	const struct fan_parent_driver_api *api =
		(const struct fan_parent_driver_api *)dev->api;

	return api->set_cycles(dev, pulse);
}

#ifdef __cplusplus
}
#endif

#include <syscalls/fan.h>

#endif /* ZEPHYR_INCLUDE_DRIVERS_FAN_H_ */
