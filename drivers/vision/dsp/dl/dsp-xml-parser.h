/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DL_DSP_XML_PARSER_H__
#define __DL_DSP_XML_PARSER_H__

#include "dl/dsp-sxml.h"
#include "dl/dsp-common.h"
#include "dl/dsp-hash.h"

enum dsp_xml_token {
	LIBS,
	COUNT,
	LIB,
	NAME,
	KERNEL_COUNT,
	KERNEL,
	ID,
	PRE,
	EXE,
	POST,
	TOKEN_NUM,
};

struct dsp_xml_kernel_table {
	char *pre;
	char *exe;
	char *post;
};

struct dsp_xml_lib {
	char *name;
	unsigned int kernel_cnt;
	struct dsp_xml_kernel_table *kernels;
};

struct dsp_xml_lib_table {
	unsigned int lib_cnt;
	struct dsp_hash_tab lib_hash;
};

void dsp_xml_parser_init(void);
void dsp_xml_parser_free(void);
int dsp_xml_parser_parse(struct dsp_dl_lib_file *file);

extern struct dsp_xml_lib_table *xml_libs;

#endif
