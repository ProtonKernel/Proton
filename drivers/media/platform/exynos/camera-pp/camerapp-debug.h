// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CAMERAPP_DEBUG_H
#define CAMERAPP_DEBUG_H

#include <linux/module.h>

void camerapp_debug_s2d(bool en_s2d, const char *fmt, ...);
#endif
