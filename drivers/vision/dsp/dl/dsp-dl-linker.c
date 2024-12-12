// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dl/dsp-dl-linker.h"
#include "dl/dsp-lib-manager.h"
#include "dl/dsp-pm-manager.h"
#include "dl/dsp-elf-loader.h"
#include "dl/dsp-common.h"
#include "dl/dsp-string-tree.h"
#include "dl/dsp-llstack.h"

static struct dsp_string_tree_node *gp_str;
static struct dsp_reloc_rule_list *rules;

static const char *const reloc_gp_str[RELOC_GP_NUM] = {
	"_segment_start_thread_local_DMb",
	"_segment_end_thread_local_DMb",
	"_segment_start_thread_local_TCMb",
	"_segment_end_thread_local_TCMb",
	"_dm_gp",
	"_tcm_gp",
	"_sfr_gp",
};

struct dsp_link_info *dsp_link_info_create(struct dsp_elf32 *elf)
{
	struct dsp_link_info *l_info;
	int idx;

	l_info = (struct dsp_link_info *)dsp_dl_malloc(
			sizeof(*l_info), "lib link_info");
	l_info->sec = (unsigned long *)dsp_dl_malloc(
			sizeof(*l_info->sec) *
			elf->hdr->e_shnum,
			"info sec");

	for (idx = 0; idx < elf->hdr->e_shnum; idx++)
		l_info->sec[idx] = -1;

	l_info->elf = elf;
	dsp_list_head_init(&l_info->reloc_sym);
	return l_info;
}

void dsp_link_info_free(struct dsp_link_info *info)
{
	dsp_dl_free(info->sec);
	dsp_list_free(&info->reloc_sym,
		struct dsp_reloc_sym, node);
}

static int dsp_reloc_sym_str_compare(
	struct dsp_list_node *a,
	struct dsp_list_node *b)
{
	struct dsp_reloc_sym *a_sym = container_of(a,
			struct dsp_reloc_sym, node);
	struct dsp_reloc_sym *b_sym = container_of(b,
			struct dsp_reloc_sym, node);

	return strcmp(a_sym->sym_str, b_sym->sym_str);
}

static int dsp_reloc_sym_addr_compare(
	struct dsp_list_node *a,
	struct dsp_list_node *b)
{
	struct dsp_reloc_sym *a_sym = container_of(a,
			struct dsp_reloc_sym, node);
	struct dsp_reloc_sym *b_sym = container_of(b,
			struct dsp_reloc_sym, node);

	if (a_sym->align < b_sym->align)
		return 1;
	else if (a_sym->align > b_sym->align)
		return -1;
	else if (a_sym->value < b_sym->value)
		return 1;
	else if (a_sym->value > b_sym->value)
		return -1;
	return 0;
}

void dsp_link_info_print(struct dsp_link_info *info)
{
	struct dsp_elf32 *elf = info->elf;
	struct dsp_list_head remove;
	struct dsp_list_node *node;
	int idx;

	DL_DEBUG("text start address : 0x%lx\n", info->text);
	DL_DEBUG("text size : %zu(0x%x)\n", info->text_size,
		(unsigned int)info->text_size);
	DL_DEBUG("DMb start address : 0x%lx\n", info->DMb);
	DL_DEBUG("DMb size : %zu(0x%x)\n", info->DMb_size,
		(unsigned int)info->DMb_size);
	DL_DEBUG("DMb_local start address : 0x%lx\n", info->DMb_local);
	DL_DEBUG("DMb_local size : %zu(0x%x)\n", info->DMb_local_size,
		(unsigned int)info->DMb_local_size);
	DL_DEBUG("DRAMb start address : 0x%lx\n", info->DRAMb);
	DL_DEBUG("DRAMb size : %zu(0x%x)\n", info->DRAMb_size,
		(unsigned int)info->DRAMb_size);
	DL_DEBUG("TCMb start address : 0x%lx\n", info->TCMb);
	DL_DEBUG("TCMb size : %zu(0x%x)\n", info->TCMb_size,
		(unsigned int)info->TCMb_size);
	DL_DEBUG("TCMb_local start address : 0x%lx\n", info->TCMb_local);
	DL_DEBUG("TCMb_local size : %zu(0x%x)\n", info->TCMb_local_size,
		(unsigned int)info->TCMb_local_size);
	DL_DEBUG("SFRw start address : 0x%lx\n", info->SFRw);
	DL_DEBUG("SFRw size : %zu(0x%x)\n", info->SFRw_size,
		(unsigned int)info->SFRw_size);

	DL_DEBUG("Section address\n");

	for (idx = 0; idx < elf->hdr->e_shnum; idx++) {
		struct dsp_elf32_shdr *shdr = &elf->shdr[idx];

		DL_BUF_STR("[%d] ", idx);
		DL_BUF_STR("%s ", elf->shstrtab + shdr->sh_name);
		DL_BUF_STR("0x%lx\n", info->sec[idx]);
		DL_PRINT_BUF(DEBUG);
	}

	dsp_list_head_init(&remove);
	dsp_list_unique(&info->reloc_sym, &remove, dsp_reloc_sym_str_compare);
	dsp_list_free(&remove, struct dsp_reloc_sym, node);

	dsp_list_sort(&info->reloc_sym, dsp_reloc_sym_addr_compare);
	dsp_list_for_each(node, &info->reloc_sym) {
		struct dsp_reloc_sym *sym_info = container_of(node,
				struct dsp_reloc_sym, node);

		DL_INFO("Symbol(%s) value(0x%x) addr(0x%x)\n",
			sym_info->sym_str, sym_info->value,
			sym_info->value * sym_info->align);
	}
}

static unsigned long __addr_align(unsigned long addr)
{
	return (addr + 0x3) & ~0x3;
}

static unsigned long __dsp_link_info_sec_alloc(struct dsp_link_info *info,
	unsigned long start, struct dsp_list_head *head)
{
	struct dsp_elf32 *elf = info->elf;
	struct dsp_list_node *node;

	dsp_list_for_each(node, head) {
		struct dsp_elf32_idx_node *idx_node =
			container_of(node, struct dsp_elf32_idx_node, node);
		int idx = idx_node->idx;
		struct dsp_elf32_shdr *shdr = &elf->shdr[idx];

		DL_DEBUG("%s\n", elf->shstrtab + shdr->sh_name);

		start = __addr_align(start);
		info->sec[idx] = start;

		DL_DEBUG("[%d]addr : 0x%lx(0x%x)\n", idx, info->sec[idx],
			elf->shdr[idx].sh_size);
		start += shdr->sh_size;
	}

	start = __addr_align(start);
	return start;
}

void dsp_link_info_set_text(struct dsp_link_info *info, unsigned long addr)
{
	struct dsp_elf32 *elf = info->elf;

	info->text = addr;

	DL_DEBUG("set text\n");
	info->text_size = __dsp_link_info_sec_alloc(info, addr,
			&elf->text.text) - info->text;
}

void dsp_link_info_set_DMb(struct dsp_link_info *info, unsigned long addr)
{
	struct dsp_elf32 *elf = info->elf;
	unsigned long addr_acc;

	info->DMb = addr_acc = addr;

	DL_DEBUG("set DMb\n");
	addr_acc = __dsp_link_info_sec_alloc(info, addr_acc, &elf->DMb.robss);
	addr_acc = __dsp_link_info_sec_alloc(info, addr_acc, &elf->DMb.rodata);
	addr_acc = __dsp_link_info_sec_alloc(info, addr_acc, &elf->DMb.bss);
	info->DMb_size = __dsp_link_info_sec_alloc(info, addr_acc,
			&elf->DMb.data) - info->DMb;
}

void dsp_link_info_set_DMb_local(struct dsp_link_info *info, unsigned long addr)
{
	struct dsp_elf32 *elf = info->elf;
	unsigned long addr_acc;

	info->DMb_local = addr_acc = addr;

	DL_DEBUG("set DMb_local\n");
	addr_acc = __dsp_link_info_sec_alloc(info, addr_acc,
			&elf->DMb_local.robss);
	addr_acc = __dsp_link_info_sec_alloc(info, addr_acc,
			&elf->DMb_local.rodata);
	addr_acc = __dsp_link_info_sec_alloc(info, addr_acc,
			&elf->DMb_local.bss);
	info->DMb_local_size = __dsp_link_info_sec_alloc(info, addr_acc,
			&elf->DMb_local.data) - info->DMb_local;
}

void dsp_link_info_set_DRAMb(struct dsp_link_info *info, unsigned long addr)
{
	struct dsp_elf32 *elf = info->elf;
	unsigned long addr_acc;

	info->DRAMb = addr_acc = addr;

	DL_DEBUG("set DRAMb\n");
	addr_acc = __dsp_link_info_sec_alloc(info, addr_acc, &elf->DRAMb.robss);
	addr_acc = __dsp_link_info_sec_alloc(info, addr_acc,
			&elf->DRAMb.rodata);
	addr_acc = __dsp_link_info_sec_alloc(info, addr_acc, &elf->DRAMb.bss);
	info->DRAMb_size = __dsp_link_info_sec_alloc(info, addr_acc,
			&elf->DRAMb.data) - info->DRAMb;
}

void dsp_link_info_set_TCMb(struct dsp_link_info *info, unsigned long addr)
{
	struct dsp_elf32 *elf = info->elf;
	unsigned long addr_acc;

	info->TCMb = addr_acc = addr;

	DL_DEBUG("set TCMb\n");
	addr_acc = __dsp_link_info_sec_alloc(info, addr_acc, &elf->TCMb.robss);
	addr_acc = __dsp_link_info_sec_alloc(info, addr_acc, &elf->TCMb.rodata);
	addr_acc = __dsp_link_info_sec_alloc(info, addr_acc, &elf->TCMb.bss);
	info->TCMb_size = __dsp_link_info_sec_alloc(info, addr_acc,
			&elf->TCMb.data) - info->TCMb;
}

void dsp_link_info_set_TCMb_local(struct dsp_link_info *info,
	unsigned long addr)
{
	struct dsp_elf32 *elf = info->elf;
	unsigned long addr_acc;

	info->TCMb_local = addr_acc = addr;

	DL_DEBUG("set TCMb_local\n");
	addr_acc = __dsp_link_info_sec_alloc(info, addr_acc,
			&elf->TCMb_local.robss);
	addr_acc = __dsp_link_info_sec_alloc(info, addr_acc,
			&elf->TCMb_local.rodata);
	addr_acc = __dsp_link_info_sec_alloc(info, addr_acc,
			&elf->TCMb_local.bss);
	info->TCMb_local_size = __dsp_link_info_sec_alloc(info, addr_acc,
			&elf->TCMb_local.data) - info->TCMb_local;
}

void dsp_link_info_set_SFRw(struct dsp_link_info *info, unsigned long addr)
{
	struct dsp_elf32 *elf = info->elf;
	unsigned long addr_acc;

	info->SFRw = addr_acc = addr;

	DL_DEBUG("set SFRw\n");
	addr_acc = __dsp_link_info_sec_alloc(info, addr_acc, &elf->SFRw.robss);
	addr_acc = __dsp_link_info_sec_alloc(info, addr_acc, &elf->SFRw.rodata);
	addr_acc = __dsp_link_info_sec_alloc(info, addr_acc, &elf->SFRw.bss);
	info->SFRw_size = __dsp_link_info_sec_alloc(info, addr_acc,
			&elf->SFRw.data) - info->SFRw;
}

unsigned int dsp_link_info_get_kernel_addr(struct dsp_link_info *info,
	char *kernel_name)
{
	int ret;
	struct dsp_elf32 *elf = info->elf;
	struct dsp_elf32_sym *sym;
	struct dsp_reloc_sym *reloc_sym;
	unsigned int sym_align;
	unsigned long sec_addr;
	unsigned int kernel_addr;

	if (kernel_name == NULL)
		return 0;

	ret = dsp_hash_get(&elf->symhash, kernel_name, (void **)&sym);
	if (ret == -1) {
		DL_ERROR("No kernel %s in lib\n", kernel_name);
		return (unsigned int) -1;
	}

	sym_align = elf->shdr[sym->st_shndx].sh_addralign;

	sec_addr = info->sec[sym->st_shndx];
	kernel_addr = (unsigned int)(sec_addr /
			sym_align + sym->st_value);

	DL_DEBUG("sh ndx: %u, sec addr: %lu, sym value: %u, align value: %u\n",
		sym->st_shndx, sec_addr, sym->st_value, sym_align);
	DL_INFO("kernel name(%s) value(0x%x) addr(0x%x)\n",
		kernel_name, kernel_addr, kernel_addr * sym_align);

	reloc_sym = (struct dsp_reloc_sym *)dsp_dl_malloc(
			sizeof(struct dsp_reloc_sym),
			"Information of symbol of kernel");
	reloc_sym->sym_str = kernel_name;
	reloc_sym->value = kernel_addr;
	reloc_sym->align = sym_align;
	dsp_list_node_init(&reloc_sym->node);
	dsp_list_node_push_back(&info->reloc_sym, &reloc_sym->node);

	return kernel_addr;
}

void dsp_linker_alloc_bss(struct dsp_elf32 *elf)
{
	unsigned int *alloc_tmp =
		(unsigned int *)dsp_dl_malloc(sizeof(*alloc_tmp) *
			elf->hdr->e_shnum, "alloc bss alloc_tmp");
	struct dsp_list_node *node;

	memset(alloc_tmp, 0, sizeof(*alloc_tmp) * elf->hdr->e_shnum);

	dsp_list_for_each(node, &elf->bss_sym) {
		struct dsp_elf32_idx_node *idx_node =
			container_of(node, struct dsp_elf32_idx_node, node);
		struct dsp_elf32_sym *sym = &elf->symtab[idx_node->idx];
		int ndx = sym->st_shndx;

		alloc_tmp[ndx] = __addr_align(alloc_tmp[ndx]);
		sym->st_value = alloc_tmp[ndx];
		DL_DEBUG("%s(%d) %s : %d\n", elf->shstrtab +
			elf->shdr[ndx].sh_name, ndx,
			elf->strtab + sym->st_name, sym->st_value);

		alloc_tmp[ndx] += sym->st_size;
	}
	dsp_dl_free(alloc_tmp);
}

static long long __get_linker_value(enum dsp_linker_value_type link_v,
	struct dsp_link_info *sym_info, struct dsp_link_info *rela_info,
	struct dsp_elf32_shdr *rela_shdr, struct dsp_elf32_sym *sym,
	struct dsp_elf32_rela *rela)
{
	long long ret;
	struct dsp_elf32 *sym_elf = sym_info->elf;
	struct dsp_elf32 *rela_elf = rela_info->elf;

	if (rela_shdr->sh_info >= rela_elf->shdr_num) {
		DL_ERROR("invalid sh_info(%u/%zu)\n",
				rela_shdr->sh_info, rela_elf->shdr_num);
		return -1;
	}

	switch (link_v) {
	case ADDEND:
		ret = (long long)rela->r_addend;
		DL_DEBUG("addend : %llu\n", ret);
		return ret;
	case BLOCK_ADDR_AR:
		ret = (long long)rela_info->sec[rela_shdr->sh_info];
		DL_DEBUG("block_addr_AR : %llu\n", ret);
		return ret;
	case BLOCK_ADDR_BR:
		ret = (long long)rela_elf->shdr[rela_shdr->sh_info].sh_offset;
		DL_DEBUG("block_addr_BR : %llu\n", ret);
		return ret;
	case ITEM_ADDR_AR:
		ret = (long long)(rela_info->sec[rela_shdr->sh_info] +
				rela->r_offset);
		DL_DEBUG("item_addr_AR : %llu\n", ret);
		return ret;
	case ITEM_ADDR_BR:
		ret = (long long)rela->r_offset;
		DL_DEBUG("item_addr_BR: %llu\n", ret);
		return ret;
	case ITEM_VALUE:
		DL_DEBUG("item_value : %u\n", 0);
		return 0;
	case STORED_VALUE:
		DL_DEBUG("stored_value : %u\n", 0);
		return 0;
	case SYMBOL_ADDR_AR:
		ret = (long long)sym_info->sec[sym->st_shndx] /
			sym_elf->shdr[sym->st_shndx].sh_addralign +
			sym->st_value;
		DL_DEBUG("symbol_addr_AR : %llu\n", ret);
		return ret;
	case SYMBOL_ADDR_BR:
		ret = (long long)sym->st_value *
			sym_elf->shdr[sym->st_shndx].sh_addralign;
		DL_DEBUG("symbol_addr_BR : %llu\n", ret);
		return ret;
	case SYMBOL_BLOCK_ADDR_AR:
		ret = (long long)sym_info->sec[sym->st_shndx];
		DL_DEBUG("symbol_block_addr_AR: %llu\n", ret);
		return ret;
	case SYMBOL_BLOCK_ADDR_BR:
		ret = (long long)sym_elf->shdr[sym->st_shndx].sh_offset;
		DL_DEBUG("symbol_block_addr_BR: %llu\n", ret);
		return ret;
	}

	return -1;
}

static int __process_op(struct dsp_llstack *st, char op)
{
	int ret;
	long long op1, op2;

	switch (op) {
	case '|':
		ret = dsp_llstack_pop(st, &op2);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		ret = dsp_llstack_pop(st, &op1);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		ret = dsp_llstack_push(st, op1 | op2);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		DL_DEBUG("%llu | %llu: %llu\n", op1, op2, op1 | op2);
		break;

	case '&':
		ret = dsp_llstack_pop(st, &op2);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		ret = dsp_llstack_pop(st, &op1);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		ret = dsp_llstack_push(st, op1 & op2);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		DL_DEBUG("%llu & %llu: %llu\n", op1, op2, op1 & op2);
		break;

	case '>':
		ret = dsp_llstack_pop(st, &op2);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		ret = dsp_llstack_pop(st, &op1);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		ret = dsp_llstack_push(st, op1 >> op2);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		DL_DEBUG("%llu > %llu: %llu\n", op1, op2, op1 >> op2);
		break;

	case '<':
		ret = dsp_llstack_pop(st, &op2);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		ret = dsp_llstack_pop(st, &op1);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		ret = dsp_llstack_push(st, op1 << op2);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		DL_DEBUG("%llu < %llu: %llu\n", op1, op2, op1 << op2);
		break;

	case '-':
		ret = dsp_llstack_pop(st, &op2);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		ret = dsp_llstack_pop(st, &op1);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		ret = dsp_llstack_push(st, op1 - op2);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		DL_DEBUG("%llu - %llu: %llu\n", op1, op2, op1 - op2);
		break;

	case '+':
		ret = dsp_llstack_pop(st, &op2);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		ret = dsp_llstack_pop(st, &op1);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		ret = dsp_llstack_push(st, op1 + op2);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		DL_DEBUG("%llu + %llu: %llu\n", op1, op2, op1 + op2);
		break;

	case '%':
		ret = dsp_llstack_pop(st, &op2);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		ret = dsp_llstack_pop(st, &op1);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		ret = dsp_llstack_push(st, op1 % op2);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		DL_DEBUG("%llu %% %llu: %llu\n", op1, op2, op1 % op2);
		break;

	case '/':
		ret = dsp_llstack_pop(st, &op2);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		ret = dsp_llstack_pop(st, &op1);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		ret = dsp_llstack_push(st, op1 / op2);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		DL_DEBUG("%llu / %llu: %llu\n", op1, op2, op1 / op2);
		break;

	case '*':
		ret = dsp_llstack_pop(st, &op2);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		ret = dsp_llstack_pop(st, &op1);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		ret = dsp_llstack_push(st, op1 * op2);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		DL_DEBUG("%llu * %llu: %llu\n", op1, op2, op1 * op2);
		break;

	case '~':
		ret = dsp_llstack_pop(st, &op1);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		ret = dsp_llstack_push(st, -1 * op1);
		if (ret == -1) {
			DL_ERROR("[%s] CHK_ERR\n", __func__);
			return -1;
		}

		DL_DEBUG("~ %llu: %llu\n", op1, -1 * op1);
		break;
	}

	return 0;
}

static unsigned int __cast_to_type(long long value, struct dsp_type *type)
{
	if (type->sign == UNSIGNED || value >= 0) {
		return (unsigned int)(value &
				((1LL << type->bit_sz) - 1));
	} else {
		return (unsigned int)(value |
				~((1LL << type->bit_sz) - 1));
	}
}

static unsigned int __rule_get_value(struct dsp_reloc_rule *rule,
	struct dsp_link_info *sym_info, struct dsp_link_info *rela_info,
	struct dsp_elf32_shdr *rela_shdr, struct dsp_elf32_sym *sym,
	struct dsp_elf32_rela *rela)
{
	int ret;
	struct dsp_llstack st;
	long long value;
	struct dsp_list_node *node;

	dsp_llstack_init(&st);

	dsp_list_for_each(node, &rule->exp) {
		struct dsp_exp_element *elem =
			container_of(node, struct dsp_exp_element, list_node);

		if (elem->type == EXP_INTEGER) {
			DL_DEBUG("integer : %d\n", elem->integer);
			ret = dsp_llstack_push(&st, (long long)elem->integer);
			if (ret == -1) {
				DL_ERROR("[%s] CHK_ERR\n", __func__);
				return -1;
			}
		} else if (elem->type == EXP_LINKER_VALUE) {
			value = __get_linker_value(elem->linker_value,
					sym_info, rela_info, rela_shdr,
					sym, rela);

			ret = dsp_llstack_push(&st, value);
			if (ret == -1) {
				DL_ERROR("[%s] CHK_ERR\n", __func__);
				return -1;
			}
		} else if (elem->type == EXP_OP) {
			ret = __process_op(&st, elem->op);
			if (ret == -1) {
				DL_ERROR("[%s] CHK_ERR\n", __func__);
				return -1;
			}
		}
	}

	ret = dsp_llstack_pop(&st, &value);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	DL_DEBUG("value : %lld(%u)\n", value,
		__cast_to_type(value, &rule->type));

	return __cast_to_type(value, &rule->type);
}

static int __calc_gp_link_value(struct dsp_lib *lib, unsigned int *value,
	const char *sym_str)
{
	struct dsp_dl_out *lib_dl = lib->dl_out;
	struct dsp_gpt *gpt = lib->gpt;
	int ret_value = dsp_string_tree_get(gp_str, sym_str);

	DL_DEBUG("ret_value : %d\n", ret_value);
	switch (ret_value) {
	case DM_THREAD_LOCAL_START:
		*value = lib_dl->DM_sh.size;
		DL_DEBUG("value : %d\n", *value);
		return 0;
	case DM_THREAD_LOCAL_END:
		*value = lib_dl->DM_sh.size + lib_dl->DM_local.size;
		DL_DEBUG("value : %d\n", *value);
		return 0;
	case TCM_THREAD_LOCAL_START:
		*value = lib_dl->TCM_sh.size;
		DL_DEBUG("value : %d\n", *value);
		return 0;
	case TCM_THREAD_LOCAL_END:
		*value = lib_dl->TCM_sh.size + lib_dl->TCM_local.size;
		DL_DEBUG("value : %d\n", *value);
		return 0;
	case DM_GP:
		*value = gpt->addr;
		DL_DEBUG("value : %d\n", *value);
		return 0;
	case TCM_GP:
		*value = gpt->addr + sizeof(unsigned int);
		DL_DEBUG("value : %d\n", *value);
		return 0;
	case SH_MEM_GP:
		*value = gpt->addr + sizeof(unsigned int) * 2;
		DL_DEBUG("value : %d\n", *value);
		return 0;
	default:
		return -1;
	}

	return -1;
}

static int __find_real_sym(
	struct dsp_elf32_sym **real_sym, struct dsp_lib **real_lib,
	struct dsp_link_info **sym_info, const char *sym_str,
	struct dsp_link_info *rela_info,
	struct dsp_lib **libs, int libs_size,
	struct dsp_lib **common_libs, int common_size)
{
	int ret, idx;

	for (idx = 0; idx < libs_size; idx++) {
		struct dsp_link_info *cur_info = libs[idx]->link_info;

		if (cur_info == rela_info)
			continue;

		DL_DEBUG("Find %s in %s\n", sym_str, libs[idx]->name);
		ret = dsp_hash_get(&cur_info->elf->symhash, sym_str,
				(void **)real_sym);
		if (ret != -1) {
			*sym_info = cur_info;
			*real_lib = libs[idx];
			return 0;
		}
	}

	for (idx = 0; idx < common_size; idx++) {
		struct dsp_link_info *cur_info = common_libs[idx]->link_info;

		if (cur_info == rela_info)
			continue;

		DL_DEBUG("Find %s in common lib %s\n",
			sym_str, common_libs[idx]->name);
		ret = dsp_hash_get(&cur_info->elf->symhash, sym_str,
				(void **)real_sym);
		if (ret != -1) {
			*sym_info = cur_info;
			*real_lib = common_libs[idx];
			return 0;
		}
	}

	return -1;
}

static int __calc_link_value(struct dsp_lib *lib,
	unsigned int *value, const char *sym_str,
	struct dsp_elf32_rela *rela, struct dsp_elf32_shdr *rela_shdr,
	struct dsp_lib **libs, int libs_size,
	struct dsp_lib **common_libs, int common_size)
{
	int ret;
	struct dsp_link_info *rela_info;
	unsigned int ruleidx;
	struct dsp_reloc_rule *rule;
	struct dsp_link_info *sym_info = NULL;
	struct dsp_elf32_sym *real_sym = NULL;
	struct dsp_lib *real_lib = NULL;

	unsigned int sym_align = 1;
	struct dsp_reloc_sym *reloc_sym;

	ret = __calc_gp_link_value(lib, value, sym_str);
	if (ret == -1) {
		rela_info = lib->link_info;
		ruleidx = dsp_elf32_rela_get_rule_idx(rela);
		if (ruleidx >= rules->cnt) {
			DL_ERROR("invalid ruleidx(%u/%d)\n",
					ruleidx, rules->cnt);
			return -1;
		}
		rule = rules->list[ruleidx];

		ret = dsp_hash_get(&rela_info->elf->symhash, sym_str,
				(void **)&real_sym);
		if (ret == -1) {
			DL_DEBUG("Sym(%s) is external variable\n", sym_str);

			ret = __find_real_sym(&real_sym, &real_lib,
					&sym_info, sym_str, rela_info,
					libs, libs_size,
					common_libs, common_size);
			if (ret == -1) {
				DL_ERROR("Symbol(%s) is not found\n",
					sym_str);
				return -1;
			}
		} else {
			DL_DEBUG("Sym(%s) is internal variable\n", sym_str);
			real_lib = lib;
			sym_info = rela_info;
		}

		*value = __rule_get_value(rule, sym_info, rela_info, rela_shdr,
				real_sym, rela);
	}

	if (real_sym) {
		struct dsp_elf32 *real_elf = real_lib->elf;
		int real_shdr_idx = real_sym->st_shndx;

		sym_align = real_elf->shdr[real_shdr_idx].sh_addralign;
	}

	DL_DEBUG("value : %u\n", *value);

	if (strncmp(sym_str, "LE_F", 4) != 0) {
		reloc_sym = (struct dsp_reloc_sym *)dsp_dl_malloc(
				sizeof(struct dsp_reloc_sym),
				"Information of symbol relocated");
		reloc_sym->sym_str = sym_str;
		reloc_sym->value = *value;
		reloc_sym->align = sym_align;
		dsp_list_node_init(&reloc_sym->node);
		dsp_list_node_push_back(&lib->link_info->reloc_sym,
			&reloc_sym->node);
	}

	return 0;
}

static void __relocate(struct dsp_reloc_info *r_info, char *data, char item_rev)
{
	int v_bits = r_info->high - r_info->low + 1;
	int t_bits;
	int bits;

	v_bits = (v_bits > 0) ? v_bits : 0;
	t_bits = v_bits;

	if (r_info->l_ext != BIT_NONE)
		t_bits++;

	if (r_info->h_ext != BIT_NONE)
		t_bits++;

	bits = t_bits + r_info->sh;

	if (bits > 8) {
		struct dsp_reloc_info low_info;

		low_info.item_cnt = r_info->item_cnt;

		if (r_info->sh >= 8) {
			low_info.idx = (unsigned int) -1;
			r_info->sh -= 8;
			r_info->idx -= 1;
		} else {
			unsigned int l_bits = 8 - r_info->sh;

			low_info.idx = r_info->idx;
			low_info.sh = r_info->sh;
			low_info.low = r_info->low;

			if (r_info->l_ext != BIT_NONE)
				low_info.high = r_info->low + (l_bits - 1) - 1;
			else
				low_info.high = r_info->low + l_bits - 1;

			low_info.h_ext = BIT_NONE;
			low_info.l_ext = r_info->l_ext;
			low_info.value = r_info->value;

			r_info->sh = 0;
			r_info->idx -= 1;
			r_info->l_ext = BIT_NONE;
			r_info->low = low_info.high + 1;
		}

		__relocate(r_info, data, item_rev);
		__relocate(&low_info, data, item_rev);
	} else {
		unsigned int value = 0;
		unsigned int data_idx;
		int mask;

		if (r_info->idx == (unsigned int) -1)
			return;

		if (r_info->h_ext == BIT_ONE)
			value = 1;
		else if (r_info->h_ext == BIT_ZERO)
			value = 0;
		else if (r_info->h_ext == BIT_SIGN)
			value = ((int)r_info->value >= 0) ? 0 : 1;

		value <<= t_bits - 1;

		mask = ((1 << v_bits) - 1) << r_info->low;
		value |= (r_info->value & mask) >> r_info->low;

		if (r_info->l_ext == BIT_ONE)
			value |= 1;

		value <<= r_info->sh;
		DL_DEBUG("value : %d, total bit : %d\n", value, t_bits);

		mask = ((1 << t_bits) - 1) << r_info->sh;

		if (item_rev)
			data_idx = r_info->item_cnt - r_info->idx - 1;
		else
			data_idx = r_info->idx;

		data[data_idx] &= ~mask;
		data[data_idx] |= value;
	}
}

static int __cvt_to_item_idx(unsigned int *sh, int num, int item_cnt,
	int item_align)
{
	int idx;

	num += item_align;
	idx = item_cnt - num / 8 - 1;
	*sh = num % 8;
	return idx;
}

static int __process_rule(struct dsp_lib *lib, struct dsp_reloc_rule *rule,
	struct dsp_elf32_shdr *rela_shdr, unsigned int value,
	struct dsp_elf32_rela *rela)
{
	struct dsp_link_info *l_info;
	struct dsp_elf32 *elf;

	struct dsp_elf32_shdr *data_shdr;
	char *reloc_data;

	int item_bit;
	int item_cnt;
	int item_align;
	char item_rev;

	struct dsp_pos *pos;
	struct dsp_reloc_info r_info;
	struct dsp_bit_slice *sl;

	struct dsp_list_node *pos_node, *bit_node;

	l_info = lib->link_info;
	elf = l_info->elf;

	if (rela_shdr->sh_info >= elf->shdr_num) {
		DL_ERROR("invalid sh_info(%u/%zu)\n",
				rela_shdr->sh_info, elf->shdr_num);
		return -1;
	}

	data_shdr = &elf->shdr[rela_shdr->sh_info];
	reloc_data = elf->data + data_shdr->sh_offset + rela->r_offset;

	item_bit = rule->cont.type.bit_sz * rule->cont.inst_num;
	item_cnt = item_bit / 8 + ((item_bit % 8) ? 1 : 0);
	item_align = item_cnt * 8 - item_bit;
	item_rev = (char)(rule->cont.type.bit_sz == 8);

	if (rule->cont.inst_num > 4) {
		DL_ERROR("rule->const.inst_num is greater than 4(%u)\n",
				rule->cont.inst_num);
		return -1;
	}

	if ((reloc_data > (elf->data + elf->size)) ||
			(reloc_data < elf->data)) {
		DL_ERROR("reloc_data is out of range(%#lx/%#zx)\n",
				(unsigned long)(reloc_data - elf->data),
				elf->size);
		return -1;
	}
	DL_DEBUG("reloc_data : %#lx/%#zx\n",
			(unsigned long)(reloc_data - elf->data), elf->size);
	DL_DEBUG("item_bit : %d, cnt : %d, align : %d\n",
			item_bit, item_cnt, item_align);
	DL_DEBUG("cont bit sz : %d, item reversed : %d",
			rule->cont.type.bit_sz, item_rev);

	dsp_list_for_each(pos_node, &rule->pos_list) {
		pos = container_of(pos_node, struct dsp_pos, list_node);

		if (pos->type == BIT_POS) {
			r_info.value = value;
			r_info.idx = __cvt_to_item_idx(&r_info.sh,
					pos->bit_pos, item_cnt, item_align);
			r_info.item_cnt = item_cnt;
			r_info.low = 0;
			r_info.high = rule->type.bit_sz - 1;
			r_info.h_ext = BIT_NONE;
			r_info.l_ext = BIT_NONE;
			if ((&reloc_data[r_info.idx] >
					(elf->data + elf->size)) ||
					(reloc_data < elf->data)) {
				DL_ERROR(
			"reloc_data + r_info.idx is out of range(%#lx/%#zx)\n",
					(unsigned long)
					(reloc_data - elf->data),
					elf->size);
				return -1;
			}
			__relocate(&r_info, reloc_data, item_rev);
		} else {
			dsp_list_for_each(bit_node, &pos->bit_slice_list) {
				sl = container_of(bit_node,
						struct dsp_bit_slice,
						list_node);
				r_info.value = value;
				r_info.low = sl->low;
				r_info.high = sl->high;
				r_info.h_ext = sl->h_ext;
				r_info.l_ext = sl->l_ext;
				r_info.idx = __cvt_to_item_idx(&r_info.sh,
						sl->value, item_cnt,
						item_align);
				r_info.item_cnt = item_cnt;
				if ((&reloc_data[r_info.idx] >
						(elf->data + elf->size)) ||
						(reloc_data < elf->data)) {
					DL_ERROR(
			"reloc_data + r_info.idx is out of range(%#lx/%#zx)\n",
						(unsigned long)
						(reloc_data - elf->data),
						elf->size);
					return -1;
				}
				__relocate(&r_info, reloc_data, item_rev);
			}
		}
	}
	return 0;
}

static int __rela_relocation(struct dsp_lib *lib, struct dsp_elf32_rela *rela,
	struct dsp_elf32_shdr *rela_shdr,
	struct dsp_lib **libs, int libs_size,
	struct dsp_lib **common_libs, int common_size)
{
	int ret;
	struct dsp_elf32 *elf;
	unsigned int symidx;
	struct dsp_elf32_sym *sym;
	const char *sym_str;
	unsigned int ruleidx;
	struct dsp_reloc_rule *rule;
	unsigned int value;

	elf = lib->link_info->elf;
	symidx = dsp_elf32_rela_get_sym_idx(rela);
	if (symidx >= elf->symtab_num) {
		DL_ERROR("invalid symidx(%u/%zu)\n",
				symidx, elf->symtab_num);
		return -1;
	}
	sym = &elf->symtab[symidx];
	sym_str = elf->strtab + sym->st_name;

	ruleidx = dsp_elf32_rela_get_rule_idx(rela);
	if (ruleidx >= rules->cnt) {
		DL_ERROR("invalid ruleidx(%u/%d)\n", ruleidx, rules->cnt);
		return -1;
	}
	rule = rules->list[ruleidx];

	ret = __calc_link_value(lib, &value, sym_str, rela, rela_shdr,
			libs, libs_size,
			common_libs, common_size);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	DL_DEBUG("Symbol(%s) Link value(%x)\n", sym_str, value);

	return __process_rule(lib, rule, rela_shdr, value, rela);
}

static int __linker_reloc_list(struct dsp_lib *lib,
	struct dsp_list_head *rela_list,
	struct dsp_lib **libs, int libs_size,
	struct dsp_lib **common_libs, int common_size)
{
	int ret;
	struct dsp_elf32_rela_node *rela_node;
	struct dsp_elf32_shdr *rela_shdr;
	unsigned int idx;
	struct dsp_list_node *node;

	dsp_list_for_each(node, rela_list) {
		rela_node =
			container_of(node, struct dsp_elf32_rela_node, node);
		DL_DEBUG("Relocate section %d\n", rela_node->idx);

		rela_shdr = &lib->link_info->elf->shdr[rela_node->idx];

		for (idx = 0; idx < rela_node->rela_num; idx++) {
			DL_DEBUG("Relocate rela %d\n", idx);
			ret = __rela_relocation(lib, &rela_node->rela[idx],
					rela_shdr, libs, libs_size,
					common_libs, common_size);
			if (ret == -1) {
				DL_ERROR("[%s] CHK_ERR\n", __func__);
				return -1;
			}
		}
	}
	return 0;
}

static int __linker_relocate(struct dsp_lib *lib,
	struct dsp_lib **libs, int libs_size,
	struct dsp_lib **common_libs, int common_size)
{
	int ret;
	struct dsp_elf32 *elf = lib->link_info->elf;

	DL_DEBUG("Reloc text\n");
	ret = __linker_reloc_list(lib, &elf->text.rela,
			libs, libs_size,
			common_libs, common_size);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	DL_DEBUG("Reloc DMb\n");
	ret = __linker_reloc_list(lib, &elf->DMb.rela,
			libs, libs_size,
			common_libs, common_size);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	DL_DEBUG("Reloc DMb local\n");
	ret = __linker_reloc_list(lib, &elf->DMb_local.rela,
			libs, libs_size,
			common_libs, common_size);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	DL_DEBUG("Reloc TCMb\n");
	ret = __linker_reloc_list(lib, &elf->TCMb.rela,
			libs, libs_size,
			common_libs, common_size);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	DL_DEBUG("Reloc TCMb_local\n");
	ret = __linker_reloc_list(lib, &elf->TCMb_local.rela,
			libs, libs_size,
			common_libs, common_size);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	DL_DEBUG("Reloc DRAMb\n");
	ret = __linker_reloc_list(lib, &elf->DRAMb.rela,
			libs, libs_size,
			common_libs, common_size);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	DL_DEBUG("Reloc SFRw\n");
	ret = __linker_reloc_list(lib, &elf->SFRw.rela,
			libs, libs_size,
			common_libs, common_size);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	return 0;
}

int dsp_linker_init(struct dsp_dl_lib_file *file)
{
	int idx;
	int ret;

	rules = (struct dsp_reloc_rule_list *)dsp_dl_malloc(
			sizeof(struct dsp_reloc_rule_list),
			"Reloc rules");
	ret = dsp_reloc_rule_list_import(rules, file);
	if (ret == -1) {
		DL_ERROR("Failed to import reloc rule list\n");
		dsp_dl_free(rules);
		return -1;
	}

	gp_str = (struct dsp_string_tree_node *)dsp_dl_malloc(
			sizeof(struct dsp_string_tree_node),
			"Linker string tree");

	dsp_string_tree_init(gp_str);

	for (idx = 0; idx < RELOC_GP_NUM; idx++) {
		DL_DEBUG("Strin tree push %s\n", reloc_gp_str[idx]);
		dsp_string_tree_push(gp_str, reloc_gp_str[idx], idx);
	}

	return 0;
}

void dsp_linker_free(void)
{
	dsp_reloc_rule_list_free(rules);
	dsp_string_tree_free(gp_str);
	dsp_dl_free(gp_str);
	dsp_dl_free(rules);
}

int dsp_linker_link_libs(struct dsp_lib **libs, int libs_size,
	struct dsp_lib **common_libs, int common_size)
{
	int ret, idx;
	struct dsp_lib *lib;
	struct dsp_elf32 *elf;
	struct dsp_link_info *l_info;
	unsigned long text_offset;

	DL_DEBUG(DL_BORDER);

	for (idx = 0; idx < libs_size; idx++) {
		lib = libs[idx];
		if (!lib->link_info) {
			elf = lib->elf;

			dsp_linker_alloc_bss(elf);

			if (!lib->link_info)
				lib->link_info = dsp_link_info_create(elf);

			l_info = lib->link_info;
			text_offset = lib->pm->start_addr -
				dsp_pm_manager_get_pm_start_addr();

			dsp_link_info_set_text(l_info, text_offset);
			dsp_link_info_set_DMb(l_info, 0UL);
			dsp_link_info_set_DMb_local(l_info, 0UL);
			dsp_link_info_set_DRAMb(l_info, 0UL);
			dsp_link_info_set_TCMb(l_info, 0UL);
			dsp_link_info_set_TCMb_local(l_info, 0UL);
			dsp_link_info_set_SFRw(l_info, 0UL);
		}
	}

	for (idx = 0; idx < libs_size; idx++) {
		lib = libs[idx];
		if (!lib->loaded) {
			ret = __linker_relocate(lib,
					libs, libs_size,
					common_libs, common_size);
			if (ret == -1) {
				DL_ERROR("[%s] CHK_ERR\n", __func__);
				return -1;
			}
		}
	}

	return 0;
}
