/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_FLASH_H
#define IS_FLASH_H

extern struct class *camera_class;

static int control_flash_gpio(u32 gpio, int val) {
	int ret = 0;

	if (gpio_is_valid(gpio)) {
		ret = gpio_request(gpio, NULL);
		if (ret < 0) {
			printk(KERN_ERR "gpio request fail %d", gpio);
			return ret;
		}
		__gpio_set_value(gpio, val);
		gpio_free(gpio);
	}

	return ret;
}

#endif
