/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DL_DSP_DL_LINKER_H__
#define __DL_DSP_DL_LINKER_H__

#include "dl/dsp-elf-loader.h"
#include "dl/dsp-rule-reader.h"
#include "dl/dsp-common.h"
#include "dl/dsp-list.h"

struct dsp_lib;

struct dsp_reloc_sym {
	const char *sym_str;
	unsigned int value;
	unsigned int align;
	struct dsp_list_node node;
};

struct dsp_link_info {
	unsigned long text;
	size_t text_size;
	unsigned long DMb;
	size_t DMb_size;
	unsigned long DMb_local;
	size_t DMb_local_size;
	unsigned long DRAMb;
	size_t DRAMb_size;
	unsigned long TCMb;
	size_t TCMb_size;
	unsigned long TCMb_local;
	size_t TCMb_local_size;
	unsigned long SFRw;
	size_t SFRw_size;
	unsigned long *sec;
	struct dsp_elf32 *elf;
	struct dsp_list_head reloc_sym;
};

struct dsp_reloc_info {
	unsigned int idx;
	unsigned int item_cnt;
	unsigned int sh;
	unsigned int low;
	unsigned int high;
	enum dsp_bit_ext_type h_ext;
	enum dsp_bit_ext_type l_ext;
	unsigned int value;
};

enum dsp_reloc_gp {
	DM_THREAD_LOCAL_START,
	DM_THREAD_LOCAL_END,
	TCM_THREAD_LOCAL_START,
	TCM_THREAD_LOCAL_END,
	DM_GP,
	TCM_GP,
	SH_MEM_GP,
	RELOC_GP_NUM,
};

struct dsp_link_info *dsp_link_info_create(struct dsp_elf32 *elf);
void dsp_link_info_free(struct dsp_link_info *info);
void dsp_link_info_print(struct dsp_link_info *info);

void dsp_link_info_set_text(struct dsp_link_info *info, unsigned long addr);
void dsp_link_info_set_DMb(struct dsp_link_info *info, unsigned long addr);
void dsp_link_info_set_DRAMb(struct dsp_link_info *info, unsigned long addr);
void dsp_link_info_set_TCMb(struct dsp_link_info *info, unsigned long addr);
void dsp_link_info_set_SFRw(struct dsp_link_info *info, unsigned long addr);
unsigned int dsp_link_info_get_kernel_addr(struct dsp_link_info *info,
	char *kernel_name);

int dsp_linker_init(struct dsp_dl_lib_file *file);
void dsp_linker_free(void);
void dsp_linker_alloc_bss(struct dsp_elf32 *elf);
int dsp_linker_link_libs(struct dsp_lib **libs, int libs_size,
	struct dsp_lib **common_libs, int common_size);

#endif
