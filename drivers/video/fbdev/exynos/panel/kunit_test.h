/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __KUNIT_TEST_H__
#define __KUNIT_TEST_H__
#if IS_ENABLED(CONFIG_SEC_KUNIT)
#include <kunit/test.h>
#include <kunit/mock.h>
#else
#define __mockable
#define __mockable_alias(id) __alias(id)
#define __visible_for_testing static
#endif
#endif /* __KUNIT_TEST_H__ */
