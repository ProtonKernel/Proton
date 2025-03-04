/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DL_DSP_RULE_READER_H__
#define __DL_DSP_RULE_READER_H__

#include "dl/dsp-common.h"
#include "dl/dsp-list.h"

#define RULE_MAX	(100)

#pragma pack(push, 4)

enum dsp_sign_type {
	SIGNED,
	UNSIGNED
};

struct dsp_type {
	int bit_sz;
	enum dsp_sign_type sign;
};

enum dsp_element_type {
	EXP_OP,
	EXP_INTEGER,
	EXP_LINKER_VALUE
};

enum dsp_linker_value_type {
	ADDEND,
	BLOCK_ADDR_AR,
	BLOCK_ADDR_BR,
	ITEM_ADDR_AR,
	ITEM_ADDR_BR,
	ITEM_VALUE,
	STORED_VALUE,
	SYMBOL_ADDR_AR,
	SYMBOL_ADDR_BR,
	SYMBOL_BLOCK_ADDR_AR,
	SYMBOL_BLOCK_ADDR_BR
};

struct dsp_exp_element {
	enum dsp_element_type type;
	union {
		char op;
		enum dsp_linker_value_type linker_value;
		int integer;
	};
	struct dsp_list_node list_node;
};

enum dsp_bit_ext_type {
	BIT_ONE,
	BIT_ZERO,
	BIT_SIGN,
	BIT_NONE
};

struct dsp_bit_slice {
	enum dsp_bit_ext_type h_ext;
	int high;
	int low;
	enum dsp_bit_ext_type l_ext;
	int value;
	struct dsp_list_node list_node;
};

enum dsp_pos_type {
	BIT_POS,
	BIT_SLICE_LIST
};

struct dsp_pos {
	enum dsp_pos_type type;
	union {
		int bit_pos;
		struct dsp_list_head bit_slice_list;
	};
	struct dsp_list_node list_node;
};

struct dsp_cont {
	struct dsp_type type;
	int inst_num;
};

enum dsp_range_type {
	RANGE_STRICT,
	RANGE_NOSTRICT
};

struct dsp_reloc_rule {
	int idx;
	struct dsp_list_head exp;
	struct dsp_type type;
	struct dsp_list_head pos_list;
	struct dsp_cont cont;
	enum dsp_range_type range_chk;
};

struct dsp_reloc_rule_list {
	struct dsp_reloc_rule *list[RULE_MAX];
	int cnt;
};

#pragma pack(pop)

void dsp_type_print(struct dsp_type *type);

void dsp_exp_binary_op(struct dsp_list_head *head, struct dsp_list_head *head1,
	struct dsp_list_head *head2, char op);
void dsp_exp_print(struct dsp_list_head *head);
int dsp_exp_import(struct dsp_exp_element *exp, struct dsp_dl_lib_file *file);
void dsp_exp_free(struct dsp_list_head *head);

void dsp_bit_ext_print(enum dsp_bit_ext_type ext);

void dsp_bit_slice_print(struct dsp_list_head *head);
int dsp_bit_slice_import(struct dsp_bit_slice *bs,
	struct dsp_dl_lib_file *file);
void dsp_bit_slice_free(struct dsp_list_head *head);

void dsp_pos_print(struct dsp_list_head *head);
int dsp_pos_import(struct dsp_pos *pos, struct dsp_dl_lib_file *file);
void dsp_pos_free(struct dsp_list_head *head);

void dsp_cont_print(struct dsp_cont *cont);

void dsp_range_print(enum dsp_range_type ran);

void dsp_reloc_rule_print(struct dsp_reloc_rule *rule);
int dsp_reloc_rule_import(struct dsp_reloc_rule *rule,
	struct dsp_dl_lib_file *file);
void dsp_reloc_rule_free(struct dsp_reloc_rule *rule);

void dsp_reloc_rule_list_print(struct dsp_reloc_rule_list *list);
int dsp_reloc_rule_list_import(struct dsp_reloc_rule_list *list,
	struct dsp_dl_lib_file *file);
void dsp_reloc_rule_list_free(struct dsp_reloc_rule_list *list);

#endif
