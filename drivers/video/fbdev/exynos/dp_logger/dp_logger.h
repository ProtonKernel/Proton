/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * DP logger
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DP_LOGGER_H_
#define _DP_LOGGER_H_

#if IS_ENABLED(CONFIG_SEC_DISPLAYPORT_LOGGER)
extern void dp_logger_set_max_count(int count);
extern void dp_logger_print(const char *fmt, ...);
extern void dp_print_hex_dump(void *buf, void *pref, size_t len);
extern int dp_logger_init(void);
#else
#define dp_logger_set_max_count(x)		do { } while (0)
#define dp_logger_print(fmt, ...)		do { } while (0)
#define dp_print_hex_dump(buf, pref, len)	do { } while (0)
#define dp_logger_init()			do { } while (0)
#endif

#endif
