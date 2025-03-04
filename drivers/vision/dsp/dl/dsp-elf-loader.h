/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DL_DSP_ELF_LOADER_H__
#define __DL_DSP_ELF_LOADER_H__

#include "dl/dsp-common.h"
#include "dl/dsp-hash.h"
#include "dl/dsp-list.h"

struct dsp_lib;

enum {
	EI_MAG0 = 0,
	EI_MAG1 = 1,
	EI_MAG2 = 2,
	EI_MAG3 = 3,
	EI_CLASS = 4,
	EI_DATA = 5,
	EI_VERSION = 6,
	EI_OSABI = 7,
	EI_ABIVERSION = 8,
	EI_PAD = 9,
	EI_NIDENT = 16
};

#pragma pack(push, 4)
struct dsp_elf32_hdr {
	unsigned char e_ident[EI_NIDENT];
	unsigned short e_type;
	unsigned short e_machine;
	unsigned int e_version;
	unsigned int e_entry;
	unsigned int e_phoff;
	unsigned int e_shoff;
	unsigned int e_flags;
	unsigned short e_ehsize;
	unsigned short e_phentsize;
	unsigned short e_phnum;
	unsigned short e_shentsize;
	unsigned short e_shnum;
	unsigned short e_shstrndx;
};

struct dsp_elf32_shdr {
	unsigned int sh_name;
	unsigned int sh_type;
	unsigned int sh_flags;
	unsigned int sh_addr;
	unsigned int sh_offset;
	unsigned int sh_size;
	unsigned int sh_link;
	unsigned int sh_info;
	unsigned int sh_addralign;
	unsigned int sh_entsize;
};

struct dsp_elf32_sym {
	unsigned int st_name;
	unsigned int st_value;
	unsigned int st_size;
	unsigned char st_info;
	unsigned char st_other;
	unsigned short st_shndx;
};

struct dsp_elf32_rela {
	unsigned int r_offset;
	unsigned int r_info;
	unsigned int r_addend;
};


struct dsp_elf32_idx_node {
	int idx;
	struct dsp_list_node node;
};

struct dsp_elf32_rela_node {
	int idx;
	struct dsp_elf32_rela *rela;
	size_t rela_num;
	struct dsp_list_node node;
};

struct dsp_elf32_mem {
	struct dsp_list_head robss;
	struct dsp_list_head rodata;
	struct dsp_list_head bss;
	struct dsp_list_head data;
	struct dsp_list_head rela;
};

struct dsp_elf32_text {
	struct dsp_list_head text;
	struct dsp_list_head rela;
};

struct dsp_elf32 {
	char *data;
	size_t size;
	struct dsp_elf32_hdr *hdr;
	struct dsp_elf32_shdr *shdr;
	size_t shdr_num;
	char *shstrtab;
	char *strtab;
	struct dsp_elf32_sym *symtab;
	size_t symtab_num;
	struct dsp_hash_tab symhash;
	struct dsp_list_head bss_sym;
	struct dsp_list_head extern_sym;
	struct dsp_elf32_mem DMb;
	struct dsp_elf32_mem DMb_local;
	struct dsp_elf32_mem TCMb;
	struct dsp_elf32_mem TCMb_local;
	struct dsp_elf32_mem DRAMb;
	struct dsp_elf32_mem SFRw;
	struct dsp_elf32_text text;
};

#pragma pack(pop)

unsigned int dsp_elf32_rela_get_sym_idx(struct dsp_elf32_rela *rela);
unsigned int dsp_elf32_rela_get_rule_idx(struct dsp_elf32_rela *rela);

int dsp_elf32_check_range(struct dsp_elf32 *elf, size_t size);
int dsp_elf32_load(struct dsp_elf32 *elf, struct dsp_dl_lib_file *file);
void dsp_elf32_free(struct dsp_elf32 *elf);
unsigned int dsp_elf32_get_text_size(struct dsp_elf32 *elf);
unsigned int dsp_elf32_get_mem_size(struct dsp_elf32_mem *mem,
	struct dsp_elf32 *elf);
void dsp_elf32_print(struct dsp_elf32 *elf);

void dsp_elf32_hdr_print(struct dsp_elf32 *elf);
void dsp_elf32_shdr_print(struct dsp_elf32 *elf);
void dsp_elf32_symtab_print(struct dsp_elf32 *elf);
void dsp_elf32_bss_sym_print(struct dsp_elf32 *elf);
void dsp_elf32_extern_sym_print(struct dsp_elf32 *elf);
void dsp_elf32_symhash_print(struct dsp_elf32 *elf);
void dsp_elf32_text_print(struct dsp_elf32 *elf);
void dsp_elf32_DMb_print(struct dsp_elf32 *elf);
void dsp_elf32_DMb_local_print(struct dsp_elf32 *elf);
void dsp_elf32_DRAMb_print(struct dsp_elf32 *elf);
void dsp_elf32_TCMb_print(struct dsp_elf32 *elf);
void dsp_elf32_TCMb_local_print(struct dsp_elf32 *elf);
void dsp_elf32_SFRw_print(struct dsp_elf32 *elf);

int dsp_elf32_load_libs(struct dsp_dl_lib_info *infos,
	struct dsp_lib **libs, int libs_size);

#endif
