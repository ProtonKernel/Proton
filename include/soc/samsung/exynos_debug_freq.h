/*
* Samsung debugging features for Samsung's SoC's.
*
* Copyright (c) 2019 Samsung Electronics Co., Ltd.
*      http://www.samsung.com
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*/

#ifndef EXYNOS_DEBUG_FREQ_H
#define EXYNOS_DEBUG_FREQ_H

#if IS_ENABLED(CONFIG_EXYNOS_DEBUG_FREQ)
extern void secdbg_freq_check(int type, unsigned long freq);
extern int secdbg_freq_init(void);
#else
#define secdbg_dtsk_print_info(a, b)		do { } while (0)
#define secdbg_freq_init()			do { } while (0)
#endif /* CONFIG_EXYNOS_DEBUG_FREQ */

#endif /* EXYNOS_DEBUG_FREQ_H */
