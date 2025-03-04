// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * exynos is sysfs functions
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-core.h"

struct class *camera_class;
EXPORT_SYMBOL_GPL(camera_class);

int is_create_sysfs(void)
{
	if (!camera_class) {
		camera_class = class_create(THIS_MODULE, "camera");
		if (IS_ERR(camera_class)) {
			pr_err("Failed to create camera_class\n");
			return PTR_ERR(camera_class);
		}
	}

	return 0;
}

int is_destroy_sysfs(void)
{
	class_destroy(camera_class);
	camera_class = NULL;

	return 0;
}
