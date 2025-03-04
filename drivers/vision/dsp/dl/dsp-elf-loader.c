// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/kernel.h>
#include "dl/dsp-elf-loader.h"
#include "dl/dsp-lib-manager.h"
#include "dl/dsp-common.h"

static const char elf_magic[] = {
	0x7f,
	'E',
	'L',
	'F'
};

static const char elf_class[][6] = {
	"\0",
	"ELF32",
	"ELF64"
};

static const char elf_endian[][15] = {
	"\0",
	"Little endian",
	"Big endian"
};

static const char elf_os[][20] = {
	"System V",
	"HP-UX",
	"NetBSD",
	"Linux",
	"GNU Hurd",
	"Solaris",
	"AIX",
	"IRIX",
	"FreeBSD",
	"Tru64",
	"Novell Modesto",
	"OpenBSD",
	"OpenVMS",
	"NonStop Kernel",
	"AROS",
	"Fenix OS",
	"Cloud ABI"
};

static const char elf_type[][10] = {
	"NONE",
	"REL",
	"EXEC",
	"DYN",
	"CORE",
	"LOOS",
	"HIOS",
	"LOPROC",
	"HIPROC"
};

unsigned int dsp_elf32_rela_get_sym_idx(struct dsp_elf32_rela *rela)
{
	return rela->r_info >> 8;
}

unsigned int dsp_elf32_rela_get_rule_idx(struct dsp_elf32_rela *rela)
{
	return rela->r_info & ((1 << 8) - 1);
}

static void __dsp_elf32_init_mem(struct dsp_elf32_mem *mem)
{
	dsp_list_head_init(&mem->robss);
	dsp_list_head_init(&mem->rodata);
	dsp_list_head_init(&mem->bss);
	dsp_list_head_init(&mem->data);
	dsp_list_head_init(&mem->rela);
}

static void __dsp_elf32_init(struct dsp_elf32 *elf)
{
	elf->data = NULL;
	__dsp_elf32_init_mem(&elf->DMb);
	__dsp_elf32_init_mem(&elf->DMb_local);
	__dsp_elf32_init_mem(&elf->DRAMb);
	__dsp_elf32_init_mem(&elf->TCMb);
	__dsp_elf32_init_mem(&elf->TCMb_local);
	__dsp_elf32_init_mem(&elf->SFRw);
	dsp_list_head_init(&elf->text.text);
	dsp_list_head_init(&elf->text.rela);
	dsp_list_head_init(&elf->bss_sym);
	dsp_list_head_init(&elf->extern_sym);
	dsp_hash_tab_init(&elf->symhash);
}

static int __dsp_elf32_check_magic(struct dsp_dl_lib_file *file)
{
	int ck = 0;
	int idx;

	if (file->size < 4) {
		DL_ERROR("Elf file is invalid(%u)\n", file->size);
		return -1;
	}

	for (idx = 0; idx < 4; idx++)
		if (((char *)(file->mem))[idx] != elf_magic[idx])
			ck = -1;

	return ck;
}

int dsp_elf32_check_range(struct dsp_elf32 *elf, size_t size)
{
	if (size > elf->size) {
		DL_ERROR("invalid size range of elf(%zu/%zu)\n",
				size, elf->size);
		return -1;
	}

	return 0;
}

static void *__dsp_elf32_get_ptr(struct dsp_elf32 *elf, void *src, size_t size)
{
	if ((src < (void *)elf->data) ||
			(src + size > (void *)elf->data + elf->size)) {
		DL_ERROR("invalid ptr access\n");
		return NULL;
	}

	if (src + size < src) {
		DL_ERROR("ptr overflow\n");
		return NULL;
	}

	return src;
}

static int __dsp_elf32_get_symtab(int idx, struct dsp_elf32 *elf)
{
	struct dsp_elf32_shdr *shdr;
	struct dsp_elf32_shdr *str_hdr;

	shdr = __dsp_elf32_get_ptr(elf, elf->shdr + idx,
			sizeof(struct dsp_elf32_shdr));
	if (!shdr) {
		DL_ERROR("shdr is NULL\n");
		return -1;
	}

	if (!shdr->sh_entsize) {
		DL_ERROR("sh_entsize must not be zero\n");
		return -1;
	}
	elf->symtab_num = shdr->sh_size / shdr->sh_entsize;

	elf->symtab = __dsp_elf32_get_ptr(elf, elf->data + shdr->sh_offset,
			sizeof(struct dsp_elf32_sym) * elf->symtab_num);
	if (!elf->symtab) {
		DL_ERROR("elf->symtab is NULL\n");
		return -1;
	}

	str_hdr = __dsp_elf32_get_ptr(elf, elf->shdr + shdr->sh_link,
			sizeof(struct dsp_elf32_shdr));
	if (!str_hdr) {
		DL_ERROR("str_hdr is NULL\n");
		return -1;
	}

	elf->strtab = __dsp_elf32_get_ptr(elf, elf->data + str_hdr->sh_offset,
			sizeof(char));
	if (!elf->strtab) {
		DL_ERROR("elf->strtab is NULL\n");
		return -1;
	}

	return 0;
}

static void __dsp_elf32_get_text(int idx, struct dsp_elf32 *elf)
{
	struct dsp_elf32_idx_node *sec;

	DL_DEBUG("Elf32 get text\n");
	sec = (struct dsp_elf32_idx_node *)dsp_dl_malloc(
			sizeof(*sec), "Text idx node");
	sec->idx = idx;
	dsp_list_node_init(&sec->node);
	dsp_list_node_push_back(&elf->text.text, &sec->node);
}

static void __dsp_elf32_idx_node_push_back(int idx, struct dsp_list_head *head)
{
	struct dsp_elf32_idx_node *sec;

	sec = (struct dsp_elf32_idx_node *)dsp_dl_malloc(
			sizeof(*sec), "Idx node");
	sec->idx = idx;
	dsp_list_node_init(&sec->node);
	dsp_list_node_push_back(head, &sec->node);
}

static void __dsp_elf32_get_mem(int idx, const char *mem_name,
	struct dsp_list_head *DMb, struct dsp_list_head *DMb_local,
	struct dsp_list_head *DRAMb, struct dsp_list_head *TCMb,
	struct dsp_list_head *TCMb_local, struct dsp_list_head *SFRw)
{
	DL_DEBUG("Elf32 get mem\n");

	if (strncmp("DMb", mem_name, 3) == 0) {
		const char *local_name = mem_name + 5;

		DL_DEBUG("local_name : %s(%zu)\n",
			local_name, strlen(local_name));

		if (strlen(local_name) == 13 &&
			strncmp(".thread_local", local_name, 13) == 0) {
			DL_DEBUG("sec : %d to DMb_local\n", idx);
			__dsp_elf32_idx_node_push_back(idx, DMb_local);
		} else {
			DL_DEBUG("sec : %d to DMb\n", idx);
			__dsp_elf32_idx_node_push_back(idx, DMb);
		}
	} else if (strncmp("TCMb", mem_name, 4) == 0) {
		const char *local_name = mem_name + 6;

		DL_DEBUG("local_name : %s(%zu)\n",
			local_name, strlen(local_name));

		if (strlen(local_name) == 13 &&
			strncmp(".thread_local", local_name, 13) == 0) {
			DL_DEBUG("sec : %d to TCMb_local\n", idx);
			__dsp_elf32_idx_node_push_back(idx, TCMb_local);
		} else {
			DL_DEBUG("sec : %d to TCMb\n", idx);
			__dsp_elf32_idx_node_push_back(idx, TCMb);
		}
	} else if (strncmp("DRAMb", mem_name, 5) == 0)
		__dsp_elf32_idx_node_push_back(idx, DRAMb);
	else if (strncmp("SFRw", mem_name, 4) == 0)
		__dsp_elf32_idx_node_push_back(idx, SFRw);
}

static int __dsp_elf32_rela_node_push_back(int idx, struct dsp_list_head *head,
	struct dsp_elf32 *elf)
{
	struct dsp_elf32_shdr *shdr;
	struct dsp_elf32_rela_node *sec;

	shdr = __dsp_elf32_get_ptr(elf, elf->shdr + idx,
			sizeof(struct dsp_elf32_shdr));
	if (!shdr) {
		DL_ERROR("shdr is NULL\n");
		return -1;
	}

	if (!shdr->sh_entsize) {
		DL_ERROR("sh_entsize must not be zero\n");
		return -1;
	}

	sec = (struct dsp_elf32_rela_node *)dsp_dl_malloc(sizeof(*sec),
			"Rela node");
	sec->idx = idx;
	sec->rela_num = shdr->sh_size / shdr->sh_entsize;
	sec->rela = __dsp_elf32_get_ptr(elf, elf->data + shdr->sh_offset,
			sizeof(struct dsp_elf32_rela) * sec->rela_num);
	if (!sec->rela) {
		DL_ERROR("sec->rela is NULL\n");
		return -1;
	}

	dsp_list_node_init(&sec->node);
	dsp_list_node_push_back(head, &sec->node);
	return 0;
}

static int __dsp_elf32_get_mem_rela(int idx, const char *sec_name,
	struct dsp_elf32 *elf, struct dsp_list_head *text,
	struct dsp_list_head *DMb, struct dsp_list_head *DMb_local,
	struct dsp_list_head *DRAMb, struct dsp_list_head *TCMb,
	struct dsp_list_head *TCMb_local, struct dsp_list_head *SFRw)
{
	int ret;
	struct dsp_list_head *head = NULL;

	DL_DEBUG("Elf32 get mem rela\n");

	if (strncmp("text", sec_name, 4) == 0) {
		head = text;
	} else if (strncmp("data.", sec_name, 5) == 0) {
		const char *mem_name = sec_name + 5;

		if (strncmp("DMb", mem_name, 3) == 0) {
			const char *local_name = mem_name + 5;

			if (strlen(local_name) == 13 &&
					strncmp(".thread_local", local_name,
						13) == 0)
				head = DMb_local;
			else
				head = DMb;
		} else if (strncmp("TCMb", mem_name, 4) == 0) {
			const char *local_name = mem_name + 6;

			if (strlen(local_name) == 13 &&
					strncmp(".thread_local", local_name,
						13) == 0)
				head = TCMb_local;
			else
				head = TCMb;
		} else if (strncmp("DRAMb", mem_name, 5) == 0) {
			head = DRAMb;
		} else if (strncmp("SFRw", mem_name, 4) == 0) {
			head = SFRw;
		}
	}

	if (head) {
		ret = __dsp_elf32_rela_node_push_back(idx, head, elf);
		if (ret == -1) {
			DL_ERROR("Failed rela node push back\n");
			return -1;
		}
	}
	return 0;
}

static int __dsp_elf32_get_bss_symhash(struct dsp_elf32 *elf)
{
	unsigned int idx;
	struct dsp_elf32_sym *sym;
	int ndx;
	struct dsp_elf32_idx_node *symnode;

	DL_DEBUG("Elf32 get bss symhash\n");

	for (idx = 0; idx < elf->symtab_num; idx++) {
		sym = &elf->symtab[idx];
		ndx = sym->st_shndx;

		if (ndx > 0 && ndx < elf->hdr->e_shnum) {
			const char *sym_str;
			const char *shstr;

			sym_str = __dsp_elf32_get_ptr(elf, elf->strtab +
					sym->st_name, sizeof(char));
			if (!sym_str) {
				DL_ERROR("sym_str is NULL\n");
				return -1;
			}

			shstr = __dsp_elf32_get_ptr(elf, elf->shstrtab +
					elf->shdr[ndx].sh_name, sizeof(char));
			if (!shstr) {
				DL_ERROR("shstr is NULL\n");
				return -1;
			}

			dsp_hash_push(&elf->symhash, sym_str, sym);

			if (strncmp(".bss", shstr, 4) == 0 ||
				strncmp(".robss", shstr, 6) == 0) {
				symnode =
					(struct dsp_elf32_idx_node *)
					dsp_dl_malloc(
						sizeof(*symnode),
						"bss sym Idx node");
				symnode->idx = idx;
				dsp_list_node_init(&symnode->node);
				dsp_list_node_push_back(&elf->bss_sym,
					&symnode->node);
			}
		}
	}

	return 0;
}

static int __dsp_elf32_get_extern_sym(struct dsp_elf32 *elf)
{
	int ret;
	unsigned int idx;
	struct dsp_elf32_sym *sym;
	int ndx;
	struct dsp_elf32_idx_node *symnode;

	DL_DEBUG("Elf32 get extern symbols\n");

	for (idx = 0; idx < elf->symtab_num; idx++) {
		sym = &elf->symtab[idx];
		ndx = sym->st_shndx;

		if (ndx == 0) {
			const char *sym_str;

			sym_str = __dsp_elf32_get_ptr(elf, elf->strtab +
					sym->st_name, sizeof(char));
			if (!sym_str) {
				DL_ERROR("sym_str is NULL\n");
				return -1;
			}

			ret = dsp_hash_get(&elf->symhash, sym_str,
					(void **)&sym);

			if (ret == -1) {
				symnode =
					(struct dsp_elf32_idx_node *)
					dsp_dl_malloc(
						sizeof(*symnode),
						"extern sym node");
				symnode->idx = idx;
				dsp_list_node_init(&symnode->node);
				dsp_list_node_push_back(&elf->extern_sym,
					&symnode->node);
			}
		}
	}

	return 0;
}

int dsp_elf32_load(struct dsp_elf32 *elf, struct dsp_dl_lib_file *file)
{
	int ret;
	struct dsp_elf32_shdr *shstrtab_hdr;
	int idx;

	DL_DEBUG("Elf32 load\n");

	ret = __dsp_elf32_check_magic(file);
	if (ret == -1) {
		DL_ERROR("CHK_ERR\n");
		return -1;
	}

	elf->data = (char *)file->mem;
	elf->size = file->size;

	elf->hdr = __dsp_elf32_get_ptr(elf, elf->data,
			sizeof(struct dsp_elf32_hdr));
	if (!elf->hdr) {
		DL_ERROR("elf->hdr is NULL\n");
		return -1;
	}

	elf->shdr = __dsp_elf32_get_ptr(elf, elf->data + elf->hdr->e_shoff,
			sizeof(struct dsp_elf32_shdr));
	if (!elf->shdr) {
		DL_ERROR("elf->shdr is NULL\n");
		return -1;
	}
	elf->shdr_num = elf->hdr->e_shnum;

	shstrtab_hdr = __dsp_elf32_get_ptr(elf, elf->shdr +
			elf->hdr->e_shstrndx, sizeof(struct dsp_elf32_shdr));
	if (!shstrtab_hdr) {
		DL_ERROR("shstrtab_hdr is NULL\n");
		return -1;
	}

	elf->shstrtab = __dsp_elf32_get_ptr(elf, elf->data +
			shstrtab_hdr->sh_offset, sizeof(char));
	if (!elf->shstrtab) {
		DL_ERROR("elf->shstrtab is NULL\n");
		return -1;
	}

	for (idx = 0; idx < elf->hdr->e_shnum; idx++) {
		struct dsp_elf32_shdr *shdr;
		const char *shdr_name;

		shdr = __dsp_elf32_get_ptr(elf, elf->shdr + idx,
				sizeof(struct dsp_elf32_shdr));
		if (!shdr) {
			DL_ERROR("shdr is NULL\n");
			return -1;
		}

		shdr_name = __dsp_elf32_get_ptr(elf, elf->shstrtab +
				shdr->sh_name, sizeof(char));
		if (!shdr_name) {
			DL_ERROR("shdr_name is NULL\n");
			return -1;
		}

		if (shdr->sh_type == 2) {
			ret = __dsp_elf32_get_symtab(idx, elf);
			if (ret == -1) {
				DL_ERROR("Failed to get symtab\n");
				return -1;
			}
		} else if (strcmp(".text", shdr_name) == 0) {
			__dsp_elf32_get_text(idx, elf);
		} else if (strncmp(".robss.", shdr_name, 7) == 0) {
			const char *mem_name = shdr_name + 7;

			__dsp_elf32_get_mem(idx, mem_name, &elf->DMb.robss,
				&elf->DMb_local.robss,
				&elf->DRAMb.robss, &elf->TCMb.robss,
				&elf->TCMb_local.robss,
				&elf->SFRw.robss);
		} else if (strncmp(".bss.", shdr_name, 5) == 0) {
			const char *mem_name = shdr_name + 5;

			__dsp_elf32_get_mem(idx, mem_name, &elf->DMb.bss,
				&elf->DMb_local.bss, &elf->DRAMb.bss,
				&elf->TCMb.bss, &elf->TCMb_local.bss,
				&elf->SFRw.bss);
		} else if (strncmp(".data.", shdr_name, 6) == 0) {
			const char *mem_name = shdr_name + 6;

			__dsp_elf32_get_mem(idx, mem_name, &elf->DMb.data,
				&elf->DMb_local.data, &elf->DRAMb.data,
				&elf->TCMb.data, &elf->TCMb_local.data,
				&elf->SFRw.data);
		} else if (strncmp(".rodata.", shdr_name, 8) == 0) {
			const char *mem_name = shdr_name + 8;

			__dsp_elf32_get_mem(idx, mem_name, &elf->DMb.rodata,
				&elf->DMb_local.rodata,
				&elf->DRAMb.rodata, &elf->TCMb.rodata,
				&elf->TCMb_local.rodata,
				&elf->SFRw.rodata);
		} else if (strncmp(".rela.", shdr_name, 6) == 0) {
			const char *sec_name = shdr_name + 6;

			ret = __dsp_elf32_get_mem_rela(idx, sec_name, elf,
					&elf->text.rela, &elf->DMb.rela,
					&elf->DMb_local.rela, &elf->DRAMb.rela,
					&elf->TCMb.rela, &elf->TCMb_local.rela,
					&elf->SFRw.rela);
			if (ret == -1) {
				DL_ERROR("Failed to get mem rela\n");
				return -1;
			}
		}
	}

	ret = __dsp_elf32_get_bss_symhash(elf);
	if (ret == -1) {
		DL_ERROR("Failed to get bss symhash\n");
		return -1;
	}

	ret = __dsp_elf32_get_extern_sym(elf);
	if (ret == -1) {
		DL_ERROR("Failed to get extern sym\n");
		return -1;
	}

	return 0;
}

static void __dsp_elf32_mem_free(struct dsp_elf32_mem *mem)
{
	dsp_list_free(&mem->robss, struct dsp_elf32_idx_node, node);
	dsp_list_free(&mem->bss, struct dsp_elf32_idx_node, node);
	dsp_list_free(&mem->rodata, struct dsp_elf32_idx_node, node);
	dsp_list_free(&mem->data, struct dsp_elf32_idx_node, node);
	dsp_list_free(&mem->rela, struct dsp_elf32_rela_node, node);
}

static void __dsp_elf32_text_free(struct dsp_elf32_text *text)
{
	dsp_list_free(&text->text, struct dsp_elf32_idx_node, node);
	dsp_list_free(&text->rela, struct dsp_elf32_rela_node, node);
}

void dsp_elf32_free(struct dsp_elf32 *elf)
{
	dsp_list_free(&elf->bss_sym, struct dsp_elf32_idx_node, node);
	dsp_list_free(&elf->extern_sym, struct dsp_elf32_idx_node, node);
	dsp_hash_free(&elf->symhash, 0);
	__dsp_elf32_mem_free(&elf->DMb);
	__dsp_elf32_mem_free(&elf->DMb_local);
	__dsp_elf32_mem_free(&elf->TCMb);
	__dsp_elf32_mem_free(&elf->TCMb_local);
	__dsp_elf32_mem_free(&elf->DRAMb);
	__dsp_elf32_mem_free(&elf->SFRw);
	__dsp_elf32_text_free(&elf->text);
}

static unsigned int __dsp_elf32_align_4byte(unsigned int size)
{
	return (size + 0x3) & ~(0x3);
}

static unsigned int __dsp_elf32_get_section_list_size(
	struct dsp_list_head *head, struct dsp_elf32 *elf)
{
	unsigned int total = 0;
	struct dsp_list_node *node;

	dsp_list_for_each(node, head) {
		struct dsp_elf32_idx_node *idx_node =
			container_of(node, struct dsp_elf32_idx_node, node);
		struct dsp_elf32_shdr *sec = elf->shdr + idx_node->idx;
		unsigned int size = sec->sh_size;

		if (total > total + __dsp_elf32_align_4byte(size)) {
			DL_ERROR("Overflow happened\n");
			return UINT_MAX;
		}

		total += __dsp_elf32_align_4byte(size);
	}
	return total;
}

unsigned int dsp_elf32_get_text_size(struct dsp_elf32 *elf)
{
	return __dsp_elf32_get_section_list_size(&elf->text.text, elf);
}

unsigned int dsp_elf32_get_mem_size(struct dsp_elf32_mem *mem,
	struct dsp_elf32 *elf)
{
	unsigned int total = 0;
	unsigned int size = 0;

	total = __dsp_elf32_get_section_list_size(&mem->robss, elf);
	if (total == UINT_MAX) {
		DL_ERROR("Overflow happened\n");
		return UINT_MAX;
	}

	size = __dsp_elf32_get_section_list_size(&mem->rodata, elf);
	if (size == UINT_MAX) {
		DL_ERROR("Overflow happened\n");
		return UINT_MAX;
	}

	if (total > total + size) {
		DL_ERROR("Overflow happened\n");
		return UINT_MAX;
	}
	total += size;

	size = __dsp_elf32_get_section_list_size(&mem->bss, elf);
	if (size == UINT_MAX) {
		DL_ERROR("Overflow happened\n");
		return UINT_MAX;
	}

	if (total > total + size) {
		DL_ERROR("Overflow happened\n");
		return UINT_MAX;
	}
	total += size;

	size = __dsp_elf32_get_section_list_size(&mem->data, elf);
	if (size == UINT_MAX) {
		DL_ERROR("Overflow happened\n");
		return UINT_MAX;
	}

	if (total > total + size) {
		DL_ERROR("Overflow happened\n");
		return UINT_MAX;
	}
	total += size;

	return total;
}

void dsp_elf32_print(struct dsp_elf32 *elf)
{
	dsp_elf32_hdr_print(elf);
	dsp_elf32_shdr_print(elf);
	dsp_elf32_symtab_print(elf);
	dsp_elf32_bss_sym_print(elf);
	dsp_elf32_extern_sym_print(elf);
	dsp_elf32_symhash_print(elf);
	dsp_elf32_text_print(elf);
	dsp_elf32_DMb_print(elf);
	dsp_elf32_DMb_local_print(elf);
	dsp_elf32_DRAMb_print(elf);
	dsp_elf32_TCMb_print(elf);
	dsp_elf32_TCMb_local_print(elf);
	dsp_elf32_SFRw_print(elf);
}

void dsp_elf32_hdr_print(struct dsp_elf32 *elf)
{
	int idx;
	struct dsp_elf32_hdr *header = elf->hdr;

	DL_DEBUG(DL_BORDER);
	DL_DEBUG("Elf header\n");

	DL_BUF_STR("Elf Magic number : ");

	for (idx = 0; idx < 16; idx++)
		DL_BUF_STR("%x ", header->e_ident[idx]);

	DL_BUF_STR("\n");
	DL_PRINT_BUF(DEBUG);

	DL_DEBUG("Class : %s\n", elf_class[header->e_ident[EI_CLASS]]);
	DL_DEBUG("Endian : %s\n", elf_endian[header->e_ident[EI_DATA]]);
	DL_DEBUG("Version : %d\n", header->e_ident[EI_VERSION]);
	DL_DEBUG("OS ABI : %s\n", elf_os[header->e_ident[EI_OSABI]]);
	DL_DEBUG("ABI VERSION : %d\n", header->e_ident[EI_ABIVERSION]);
	DL_DEBUG("OBJ Type : %s\n", elf_type[header->e_type]);
	DL_DEBUG("Machine id : %#x\n", header->e_machine);
	DL_DEBUG("Elf version : %d\n", header->e_version);
	DL_DEBUG("Entry point : %#x\n", header->e_entry);
	DL_DEBUG("Program header address : %#x\n", header->e_phoff);
	DL_DEBUG("Section header address : %#x\n", header->e_shoff);
	DL_DEBUG("Flag : %d\n", header->e_flags);
	DL_DEBUG("Header Size : %d\n", header->e_ehsize);
	DL_DEBUG("Program header size : %d\n", header->e_phentsize);
	DL_DEBUG("Program header entry : %d\n", header->e_phnum);
	DL_DEBUG("Section header size : %d\n", header->e_shentsize);
	DL_DEBUG("Section header entry : %d\n", header->e_shnum);
	DL_DEBUG("Section header string table index : %d\n",
		header->e_shstrndx);
}

void dsp_elf32_shdr_print(struct dsp_elf32 *elf)
{
	unsigned int idx;
	struct dsp_elf32_shdr *shdr = elf->shdr;
	char *shstrtab = elf->shstrtab;

	DL_DEBUG(DL_BORDER);
	DL_DEBUG("section header\n");
	for (idx = 0; idx < elf->shdr_num; idx++) {
		DL_BUF_STR("[%d] ", idx);
		DL_BUF_STR("%s ", shstrtab + shdr[idx].sh_name);
		DL_BUF_STR("type:%d ", shdr[idx].sh_type);
		DL_BUF_STR("flag:%#x ", shdr[idx].sh_flags);
		DL_BUF_STR("addr:%#x ", shdr[idx].sh_addr);
		DL_BUF_STR("off:%#x ", shdr[idx].sh_offset);
		DL_BUF_STR("sz:%d ", shdr[idx].sh_size);
		DL_BUF_STR("link:%d ", shdr[idx].sh_link);
		DL_BUF_STR("info:%d ", shdr[idx].sh_info);
		DL_BUF_STR("align:%d ", shdr[idx].sh_addralign);
		DL_BUF_STR("entsz:%d\n", shdr[idx].sh_entsize);
		DL_PRINT_BUF(DEBUG);
	}
}

static void __dsp_elf32_print_sym(struct dsp_elf32_sym *sym, char *strtab)
{
	DL_BUF_STR("%s ", strtab + sym->st_name);
	DL_BUF_STR("value:%#x ", sym->st_value);
	DL_BUF_STR("sz:%d ", sym->st_size);
	DL_BUF_STR("info:%d ", sym->st_info);
	DL_BUF_STR("other:%d ", sym->st_other);
	DL_BUF_STR("ndx:%d\n", sym->st_shndx);
	DL_PRINT_BUF(DEBUG);
}

void dsp_elf32_symtab_print(struct dsp_elf32 *elf)
{
	unsigned int idx;

	DL_DEBUG(DL_BORDER);
	DL_DEBUG("symbol table\n");

	for (idx = 0; idx < elf->symtab_num; idx++) {
		DL_BUF_STR("[%d]: ", idx);
		__dsp_elf32_print_sym(&elf->symtab[idx], elf->strtab);
	}
}

void dsp_elf32_bss_sym_print(struct dsp_elf32 *elf)
{
	struct dsp_list_node *node;

	DL_DEBUG(DL_BORDER);
	DL_DEBUG("bss symbols\n");
	dsp_list_for_each(node, &elf->bss_sym) {
		struct dsp_elf32_idx_node *symnode =
			container_of(node, struct dsp_elf32_idx_node, node);
		struct dsp_elf32_sym *sym = &elf->symtab[symnode->idx];

		DL_BUF_STR("[bss]: ");
		__dsp_elf32_print_sym(sym, elf->strtab);
	}
}

void dsp_elf32_extern_sym_print(struct dsp_elf32 *elf)
{
	struct dsp_list_node *node;

	DL_DEBUG(DL_BORDER);
	DL_DEBUG("extern symbols\n");
	dsp_list_for_each(node, &elf->extern_sym) {
		struct dsp_elf32_idx_node *symnode =
			container_of(node, struct dsp_elf32_idx_node, node);
		struct dsp_elf32_sym *sym = &elf->symtab[symnode->idx];

		DL_BUF_STR("[extern]: ");
		__dsp_elf32_print_sym(sym, elf->strtab);
	}
}

void dsp_elf32_symhash_print(struct dsp_elf32 *elf)
{
	int idx;
	struct dsp_list_node *node;

	DL_DEBUG(DL_BORDER);
	DL_DEBUG("symbol hash table\n");

	for (idx = 0; idx < DSP_HASH_MAX; idx++) {
		dsp_list_for_each(node, &elf->symhash.list[idx]) {
			struct dsp_hash_node *hash_node =
				container_of(node, struct dsp_hash_node, node);
			struct dsp_elf32_sym *sym =
				(struct dsp_elf32_sym *)hash_node->value;

			DL_BUF_STR("key[%u] ", hash_node->key);
			__dsp_elf32_print_sym(sym, elf->strtab);
		}
	}
}

static void __dsp_elf32_print_rela(struct dsp_list_head *rela_list,
	struct dsp_elf32 *elf)
{
	unsigned int idx;
	unsigned int r_info;
	struct dsp_elf32_rela *rela;
	struct dsp_list_node *node;
	struct dsp_elf32_rela_node *rela_node;

	dsp_list_for_each(node, rela_list) {
		rela_node = container_of(node,
				struct dsp_elf32_rela_node, node);

		DL_DEBUG(".rela[%d]\n", rela_node->idx);
		rela = rela_node->rela;

		for (idx = 0; idx < rela_node->rela_num; idx++) {
			r_info = rela[idx].r_info;

			DL_BUF_STR("[%d]: ", idx);
			DL_BUF_STR("offset:%d ", rela[idx].r_offset);
			DL_BUF_STR("info:%#x ", r_info);
			DL_BUF_STR("sym:%d(%s) ", r_info >> 8, elf->strtab +
				elf->symtab[r_info >> 8].st_name);
			DL_BUF_STR("reltype:%d ", r_info & 0xff);
			DL_BUF_STR("r_addend:%d\n", rela[idx].r_addend);
			DL_PRINT_BUF(DEBUG);
		}

		DL_DEBUG("\n");
	}
}

void dsp_elf32_text_print(struct dsp_elf32 *elf)
{
	unsigned int idx;
	int jdx;
	struct dsp_list_node *node;

	DL_DEBUG(DL_BORDER);
	DL_DEBUG("Elf32 mem : text(%u)\n",
		dsp_elf32_get_text_size(elf));
	dsp_list_for_each(node, &elf->text.text) {
		struct dsp_elf32_idx_node *idx_node =
			container_of(node, struct dsp_elf32_idx_node, node);
		struct dsp_elf32_shdr *text_hdr = elf->shdr + idx_node->idx;
		unsigned int *text = (unsigned int *)(elf->data +
				text_hdr->sh_offset);
		size_t size = __dsp_elf32_align_4byte(text_hdr->sh_size) /
			sizeof(unsigned int);

		DL_DEBUG(".text[%d]\n", idx_node->idx);
		for (idx = 0; idx < size; idx++) {
			if (idx != 0 && idx % 4 == 0) {
				DL_BUF_STR("\n");
				DL_PRINT_BUF(DEBUG);
			}

			DL_BUF_STR("0x");
			for (jdx = 0; jdx < 4; jdx++)
				DL_BUF_STR("%02x",
					((const unsigned char *)
						(text + idx))[jdx]);
			DL_BUF_STR(" ");
		}

		DL_BUF_STR("\n");
		DL_PRINT_BUF(DEBUG);
		DL_DEBUG("\n");
	}
	__dsp_elf32_print_rela(&elf->text.rela, elf);
}

static void __dsp_elf32_print_mem(struct dsp_elf32_mem *mem,
	struct dsp_elf32 *elf)
{
	unsigned int idx;
	struct dsp_list_node *node;

	dsp_list_for_each(node, &mem->robss) {
		struct dsp_elf32_idx_node *idx_node =
			container_of(node, struct dsp_elf32_idx_node, node);
		struct dsp_elf32_shdr *bss_hdr = elf->shdr + idx_node->idx;

		DL_DEBUG("robss[%d] : %d\n", idx_node->idx, bss_hdr->sh_size);
	}

	dsp_list_for_each(node, &mem->rodata) {
		struct dsp_elf32_idx_node *idx_node =
			container_of(node, struct dsp_elf32_idx_node, node);
		struct dsp_elf32_shdr *rodata_hdr = elf->shdr + idx_node->idx;
		unsigned int *rodata = (unsigned int *)(elf->data +
				rodata_hdr->sh_offset);
		size_t size = __dsp_elf32_align_4byte(rodata_hdr->sh_size) /
			sizeof(unsigned int);

		DL_DEBUG("\n");
		DL_DEBUG("rodata[%d]\n", idx_node->idx);
		for (idx = 0; idx < size; idx++) {
			if (idx != 0 && idx % 4 == 0) {
				DL_BUF_STR("\n");
				DL_PRINT_BUF(DEBUG);
			}

			DL_BUF_STR("0x%08x ", rodata[idx]);
		}

		DL_BUF_STR("\n");
		DL_PRINT_BUF(DEBUG);
	}

	dsp_list_for_each(node, &mem->bss) {
		struct dsp_elf32_idx_node *idx_node =
			container_of(node, struct dsp_elf32_idx_node, node);
		struct dsp_elf32_shdr *bss_hdr = elf->shdr + idx_node->idx;

		DL_DEBUG("bss[%d] : %d\n", idx_node->idx, bss_hdr->sh_size);
	}

	dsp_list_for_each(node, &mem->data) {
		struct dsp_elf32_idx_node *idx_node =
			container_of(node, struct dsp_elf32_idx_node, node);
		struct dsp_elf32_shdr *data_hdr = elf->shdr + idx_node->idx;
		unsigned int *data = (unsigned int *)(elf->data +
				data_hdr->sh_offset);
		size_t size = __dsp_elf32_align_4byte(data_hdr->sh_size) /
			sizeof(unsigned int);

		DL_DEBUG("\n");
		DL_DEBUG("data[%d]\n", idx_node->idx);
		for (idx = 0; idx < size; idx++) {
			if (idx != 0 && idx % 4 == 0) {
				DL_BUF_STR("\n");
				DL_PRINT_BUF(DEBUG);
			}

			DL_BUF_STR("0x%08x ", data[idx]);
		}

		DL_BUF_STR("\n");
		DL_PRINT_BUF(DEBUG);
	}

	__dsp_elf32_print_rela(&mem->rela, elf);
}

void dsp_elf32_DMb_print(struct dsp_elf32 *elf)
{
	DL_DEBUG(DL_BORDER);
	DL_DEBUG("Elf32 mem : DMb(%u)\n",
		dsp_elf32_get_mem_size(&elf->DMb, elf));
	__dsp_elf32_print_mem(&elf->DMb, elf);
}

void dsp_elf32_DMb_local_print(struct dsp_elf32 *elf)
{
	DL_DEBUG(DL_BORDER);
	DL_DEBUG("Elf32 mem : DMb_local(%u)\n",
		dsp_elf32_get_mem_size(&elf->DMb_local, elf));
	__dsp_elf32_print_mem(&elf->DMb_local, elf);
}

void dsp_elf32_DRAMb_print(struct dsp_elf32 *elf)
{
	DL_DEBUG(DL_BORDER);
	DL_DEBUG("Elf32 mem : DRAMb(%u)\n",
		dsp_elf32_get_mem_size(&elf->DRAMb, elf));
	__dsp_elf32_print_mem(&elf->DRAMb, elf);
}

void dsp_elf32_TCMb_print(struct dsp_elf32 *elf)
{
	DL_DEBUG(DL_BORDER);
	DL_DEBUG("Elf32 mem : TCMb(%u)\n",
		dsp_elf32_get_mem_size(&elf->TCMb, elf));
	__dsp_elf32_print_mem(&elf->TCMb, elf);
}

void dsp_elf32_TCMb_local_print(struct dsp_elf32 *elf)
{
	DL_DEBUG(DL_BORDER);
	DL_DEBUG("Elf32 mem : TCMb_local(%u)\n",
		dsp_elf32_get_mem_size(&elf->TCMb_local, elf));
	__dsp_elf32_print_mem(&elf->TCMb_local, elf);
}

void dsp_elf32_SFRw_print(struct dsp_elf32 *elf)
{
	DL_DEBUG(DL_BORDER);
	DL_DEBUG("Elf32 mem : SFRw(%u)\n",
		dsp_elf32_get_mem_size(&elf->SFRw, elf));
	__dsp_elf32_print_mem(&elf->SFRw, elf);
}

int dsp_elf32_load_libs(struct dsp_dl_lib_info *infos,
	struct dsp_lib **libs, int libs_size)
{
	int ret, idx;

	DL_DEBUG("load elf\n");

	for (idx = 0; idx < libs_size; idx++) {
		if (!libs[idx]->elf) {
			struct dsp_elf32 *elf;

			DL_DEBUG("Load ELF for library %s\n",
				libs[idx]->name);
			elf = (struct dsp_elf32 *)
				dsp_dl_malloc(sizeof(*elf), "Elf");
			__dsp_elf32_init(elf);
			libs[idx]->elf = elf;

			ret = dsp_elf32_load(libs[idx]->elf,
					&infos[idx].file);
			if (ret == -1) {
				DL_ERROR("CHK_ERR\n");
				return -1;
			}
		}
	}

	return 0;
}
