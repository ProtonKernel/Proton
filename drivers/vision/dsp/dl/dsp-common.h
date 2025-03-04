/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DL_DSP_COMMON_H__
#define __DL_DSP_COMMON_H__

#include <linux/kernel.h>
#include <linux/string.h>

#include "dsp-log.h"

#undef get16bits
#if !defined get16bits
#define get16bits(d)		(*(const unsigned short *)(d))
#endif

#define DL_ERROR(fmt, args...)	dsp_err("[%-30s]" fmt, __func__, ##args)
#define DL_INFO(fmt, args...)	dsp_info(fmt, ##args)
#define DL_DEBUG(fmt, args...)	dsp_dl_dbg("[%-30s]" fmt, __func__, ##args)

#define DL_BORDER	\
"====================================================================\n"

#define DL_BUF_STR(fmt, args...)	dsp_dl_put_log_buf(fmt, ##args)
#define DL_PRINT_BUF(level)		\
	dsp_dl_print_log_buf(DL_LOG_##level, __func__)

enum dsp_dl_log_level {
	DL_LOG_ERROR,
	DL_LOG_INFO,
	DL_LOG_DEBUG,
};

struct dsp_dl_lib_file {
	void *mem;
	unsigned int size;
	unsigned int r_ptr;
};

struct dsp_dl_lib_info {
	const char *name;
	struct dsp_dl_lib_file file;
};

void dsp_dl_lib_file_reset(struct dsp_dl_lib_file *file);
int dsp_dl_lib_file_read(char *buf, unsigned int size,
	struct dsp_dl_lib_file *file);

void dsp_dl_put_log_buf(const char *fmt, ...);
void dsp_dl_print_log_buf(int level, const char *func);

void dsp_common_init(void);
void dsp_common_free(void);

void *dsp_dl_malloc(size_t size, const char *msg);
void dsp_dl_free(void *data);

#endif
