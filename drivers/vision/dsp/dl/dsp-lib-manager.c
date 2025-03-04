// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dl/dsp-common.h"
#include "dl/dsp-dl-engine.h"
#include "dl/dsp-lib-manager.h"
#include "dl/dsp-pm-manager.h"
#include "dl/dsp-gpt-manager.h"
#include "dl/dsp-dl-out-manager.h"
#include "dl/dsp-xml-parser.h"

static const char *dl_lib_path;
static struct dsp_hash_tab *dsp_lib_hash;

void dsp_lib_init(struct dsp_lib *lib, struct dsp_dl_lib_info *info)
{
	lib->name = (char *)dsp_dl_malloc(strlen(info->name) + 1,
			"lib name");
	strcpy(lib->name, info->name);

	DL_DEBUG("lib name : %s\n", lib->name);

	lib->elf = NULL;
	lib->pm = NULL;
	lib->gpt = NULL;
	lib->dl_out = NULL;
	lib->dl_out_mem = NULL;
	lib->link_info = NULL;
	lib->ref_cnt = 0;
	lib->loaded = 0;
}

void dsp_lib_unload(struct dsp_lib *lib)
{
	DL_DEBUG("DL lib unload\n");

	if (lib->elf) {
		dsp_elf32_free(lib->elf);
		dsp_dl_free(lib->elf);
		lib->elf = NULL;
	}

	if (lib->link_info) {
		dsp_link_info_free(lib->link_info);
		dsp_dl_free(lib->link_info);
		lib->link_info = NULL;
	}
}

void dsp_lib_free(struct dsp_lib *lib)
{
	DL_DEBUG("DL lib free\n");

	dsp_dl_free(lib->name);

	dsp_lib_unload(lib);

	dsp_pm_free(lib);
	dsp_gpt_free(lib);
	dsp_dl_out_free(lib);
}

void dsp_lib_print(struct dsp_lib *lib)
{
	DL_INFO(DL_BORDER);
	DL_INFO("Library name(%s) ref_cnt(%u) %s\n",
		lib->name, lib->ref_cnt,
		(lib->loaded) ? "loaded" : "unloaded");

	if (lib->elf) {
		DL_DEBUG("\n");
		DL_DEBUG("ELF information\n");
		dsp_elf32_print(lib->elf);
	}

	if (lib->pm) {
		DL_INFO("\n");
		DL_INFO("Program memory\n");
		dsp_tlsf_mem_print(lib->pm);
		DL_DEBUG("\n");
		dsp_pm_print(lib);
	}

	if (lib->link_info) {
		DL_INFO("\n");
		DL_INFO("Linking information\n");
		dsp_link_info_print(lib->link_info);
	}

	if (lib->gpt) {
		DL_INFO("\n");
		DL_INFO("Global pointer\n");
		dsp_gpt_print(lib->gpt);
	}

	if (lib->dl_out_mem) {
		DL_INFO("\n");
		DL_INFO("Loader output\n");
		dsp_tlsf_mem_print(lib->dl_out_mem);
		dsp_dl_out_print(lib->dl_out);
	}
}

void dsp_lib_manager_init(const char *lib_path)
{
	DL_DEBUG("DL lib init\n");
	dsp_lib_hash = (struct dsp_hash_tab *)dsp_dl_malloc(
			sizeof(struct dsp_hash_tab),
			"Dsp lib hash");
	dl_lib_path = lib_path;
	DL_DEBUG("%s\n", dl_lib_path);
	dsp_hash_tab_init(dsp_lib_hash);
}

void dsp_lib_manager_free(void)
{
	unsigned int idx;

	DL_DEBUG("DL lib_manager free\n");

	for (idx = 0; idx < DSP_HASH_MAX; idx++) {
		struct dsp_list_node *cur, *next;

		cur = (&dsp_lib_hash->list[idx])->next;

		while (cur != NULL) {
			struct dsp_hash_node *hash_node =
				container_of(cur, struct dsp_hash_node, node);
			struct dsp_lib *lib =
				(struct dsp_lib *)hash_node->value;

			next = cur->next;
			dsp_lib_free(lib);
			cur = next;
		}
	}

	dsp_hash_free(dsp_lib_hash, 1);
	dsp_dl_free(dsp_lib_hash);
}

void dsp_lib_manager_print(void)
{
	unsigned int idx;

	DL_INFO(DL_BORDER);
	DL_INFO("Library manager\n");

	for (idx = 0; idx < DSP_HASH_MAX; idx++) {
		struct dsp_list_node *cur, *next;

		cur = (&dsp_lib_hash->list[idx])->next;

		while (cur != NULL) {
			struct dsp_hash_node *hash_node =
				container_of(cur, struct dsp_hash_node, node);
			struct dsp_lib *lib =
				(struct dsp_lib *)hash_node->value;

			next = cur->next;
			dsp_lib_print(lib);
			cur = next;
		}
	}
}

struct dsp_lib **dsp_lib_manager_get_libs(struct dsp_dl_lib_info *lib_infos,
	size_t lib_infos_size)
{
	int ret;
	unsigned int idx;
	struct dsp_lib **libs = (struct dsp_lib **)dsp_dl_malloc(
			sizeof(*libs) * lib_infos_size,
			"struct dsp_lib list");

	DL_DEBUG("DL lib_manager get libs\n");

	for (idx = 0; idx < lib_infos_size; idx++) {
		struct dsp_lib *lib;
		const char *name = lib_infos[idx].name;

		ret = dsp_hash_get(dsp_lib_hash, name, (void **)&lib);
		if (ret == -1) {
			DL_DEBUG("Create library %s\n", name);
			lib = (struct dsp_lib *)dsp_dl_malloc(
					sizeof(*lib), "Lib created");
			dsp_lib_init(lib, &lib_infos[idx]);
			dsp_hash_push(dsp_lib_hash, name, lib);
			libs[idx] = lib;
		} else {
			DL_DEBUG("Check lib duplicate\n");
			if (!lib->loaded) {
				DL_ERROR("Library(%s) is duplicated\n",
					lib->name);
				dsp_lib_manager_delete_unloaded_libs(libs, idx);
				dsp_dl_free(libs);
				return NULL;
			}
		}

		libs[idx] = lib;
	}

	return libs;
}

void dsp_lib_manager_inc_ref_cnt(struct dsp_lib **libs, size_t libs_size)
{
	unsigned int idx;

	DL_DEBUG("DL lib_manager inc_ref_cnt\n");

	for (idx = 0; idx < libs_size; idx++)
		libs[idx]->ref_cnt++;
}

void dsp_lib_manager_dec_ref_cnt(struct dsp_lib **libs, size_t libs_size)
{
	unsigned int idx;

	DL_DEBUG("DL lib_manager dec_ref_cnt\n");
	for (idx = 0; idx < libs_size; idx++)
		libs[idx]->ref_cnt--;
}

void dsp_lib_manager_delete_unloaded_libs(struct dsp_lib **libs,
	size_t libs_size)
{
	unsigned int idx;

	DL_DEBUG("DL lib_manager delete unloaded libs\n");
	for (idx = 0; idx < libs_size; idx++) {
		if (!libs[idx]->loaded)
			dsp_lib_manager_delete_lib(libs[idx]);
	}
}

void dsp_lib_manager_delete_lib(struct dsp_lib *lib)
{
	DL_DEBUG("DL lib-manager delete_lib\n");
	dsp_hash_pop(dsp_lib_hash, lib->name, 0);
	dsp_lib_free(lib);
	dsp_dl_free(lib);
}

int dsp_lib_manager_delete_no_ref(void)
{
	unsigned int idx;
	int ret = 0;

	DL_DEBUG("Delete no ref libs\n");
	for (idx = 0; idx < DSP_HASH_MAX; idx++) {
		struct dsp_list_node *cur, *next;

		cur = (&dsp_lib_hash->list[idx])->next;
		while (cur != NULL) {
			struct dsp_hash_node *hash_node =
				container_of(cur, struct dsp_hash_node, node);
			struct dsp_lib *lib =
				(struct dsp_lib *)hash_node->value;

			if (lib->loaded && lib->ref_cnt == 0) {
				DL_DEBUG("Delete lib(%s)\n", lib->name);
				ret = 1;
				dsp_lib_manager_delete_lib(lib);
			}

			next = cur->next;
			cur = next;
		}
	}
	return ret;
}

static int __dsp_lib_manager_load_kernel_table(struct dsp_lib *lib,
	struct dsp_dl_out_section sec)
{
	unsigned int ret;
	struct dsp_xml_lib *xml_lib;
	struct dsp_dl_kernel_table *kernel_table;
	unsigned int idx;

	ret = dsp_hash_get(&xml_libs->lib_hash, lib->name,
			(void **)&xml_lib);
	if (ret == -1U) {
		DL_ERROR("No Library %s\n", lib->name);
		return -1;
	}

	kernel_table = (struct dsp_dl_kernel_table *)(lib->dl_out->data +
			sec.offset);

	for (idx = 0; idx < xml_lib->kernel_cnt; idx++) {
		struct dsp_xml_kernel_table *kernel = &xml_lib->kernels[idx];

		ret = dsp_link_info_get_kernel_addr(lib->link_info,
				kernel->pre);
		if (ret == (unsigned int) -1) {
			DL_ERROR("[%s] pre CHK_ERR\n", __func__);
			return -1;
		}

		kernel_table[idx].pre = ret;

		ret = dsp_link_info_get_kernel_addr(lib->link_info,
				kernel->exe);
		if (ret == (unsigned int) -1) {
			DL_ERROR("[%s] exe CHK_ERR\n", __func__);
			return -1;
		}

		kernel_table[idx].exe = ret;

		ret = dsp_link_info_get_kernel_addr(lib->link_info,
				kernel->post);
		if (ret == (unsigned int) -1) {
			DL_ERROR("[%s] post CHK_ERR\n", __func__);
			return -1;
		}

		kernel_table[idx].post = ret;
	}

	return 0;
}

static void __dsp_lib_manager_load_gpt(struct dsp_lib *lib)
{
	DL_DEBUG("load gpt\n");
	lib->dl_out->gpt_addr = (unsigned int)lib->gpt->addr;
}

static int __dsp_lib_manager_load_pm(struct dsp_lib *lib)
{
	int ret;
	struct dsp_elf32 *elf = lib->elf;
	struct dsp_list_node *node;

	DL_DEBUG("load pm\n");
	dsp_list_for_each(node, &elf->text.text) {
		struct dsp_elf32_idx_node *idx_node =
			container_of(node, struct dsp_elf32_idx_node, node);
		unsigned int ndx = idx_node->idx;
		struct dsp_elf32_shdr *text_hdr = elf->shdr + ndx;
		unsigned char *text = (unsigned char *)(elf->data
				+ text_hdr->sh_offset);
		unsigned char *text_end = text + text_hdr->sh_size;
		unsigned char *dest =
			(unsigned char *)(dsp_pm_manager_get_pm_start_addr() +
					lib->link_info->sec[ndx]);

		ret = dsp_elf32_check_range(elf, (size_t)text_hdr->sh_offset +
				text_hdr->sh_size);
		if (ret) {
			DL_ERROR("invalid text range(%u/%u)\n",
					text_hdr->sh_offset,
					text_hdr->sh_size);
			return -1;
		}

		if (lib->link_info->sec[ndx] + text_hdr->sh_size >
				dsp_pm_manager_get_pm_total_size()) {
			DL_ERROR("invalid dest range(%lu/%u/%zu)\n",
					lib->link_info->sec[ndx],
					text_hdr->sh_size,
					dsp_pm_manager_get_pm_total_size());
			return -1;
		}

		DL_DEBUG("Dest : %p, Src : %p, Src end : %p, size : %u\n",
			dest, text, text_end, text_hdr->sh_size);

		for (; text < text_end; text += 4, dest += 4) {
			int idx;

			for (idx = 0; idx < 4; idx++)
				dest[idx] = text[3 - idx];
		}
	}

	return 0;
}

static int __dsp_lib_manager_load_bss_sec(struct dsp_lib *lib,
	struct dsp_list_head *head, struct dsp_dl_out_section sec)
{
	struct dsp_list_node *node;
	struct dsp_elf32 *elf = lib->elf;

	DL_DEBUG("load bss sec\n");
	dsp_list_for_each(node, head) {
		struct dsp_elf32_idx_node *idx_node =
			container_of(node, struct dsp_elf32_idx_node, node);
		unsigned int ndx = idx_node->idx;
		struct dsp_elf32_shdr *mem_hdr = elf->shdr + ndx;
		char *dest = lib->dl_out->data + sec.offset +
			lib->link_info->sec[ndx];
		unsigned long end = (unsigned long)(dest + mem_hdr->sh_size);
		unsigned int *addr;

		if (sec.offset >
				sec.offset + lib->link_info->sec[ndx] +
				mem_hdr->sh_size) {
			DL_ERROR("Overflow happened.\n");
			return -1;
		}

		if (lib->link_info->sec[ndx] >
				sec.offset + lib->link_info->sec[ndx] +
				mem_hdr->sh_size) {
			DL_ERROR("Overflow happened.\n");
			return -1;
		}

		if (mem_hdr->sh_size >
				sec.offset + lib->link_info->sec[ndx] +
				mem_hdr->sh_size) {
			DL_ERROR("Overflow happened.\n");
			return -1;
		}

		if (sec.offset + lib->link_info->sec[ndx] + mem_hdr->sh_size >
				lib->dl_out_data_size) {
			DL_ERROR("invalid dest range(%u/%lu/%u/%zu)\n",
					sec.offset,
					lib->link_info->sec[ndx],
					mem_hdr->sh_size,
					lib->dl_out_data_size);
			return -1;
		}

		DL_DEBUG("load sec %u\n", ndx);
		DL_DEBUG("Dest : %p, size : %u\n", dest, mem_hdr->sh_size);

		for (addr = (unsigned int *)dest; (unsigned long)addr < end;
			addr++)
			*addr = 0;
	}

	return 0;
}

static int __dsp_lib_manager_load_sec(struct dsp_lib *lib,
	struct dsp_list_head *head, struct dsp_dl_out_section sec,
	int rev_endian)
{
	int ret;
	struct dsp_list_node *node;
	struct dsp_elf32 *elf = lib->elf;

	DL_DEBUG("load sec\n");
	dsp_list_for_each(node, head) {
		struct dsp_elf32_idx_node *idx_node =
			container_of(node, struct dsp_elf32_idx_node, node);
		unsigned int ndx = idx_node->idx;
		struct dsp_elf32_shdr *mem_hdr = elf->shdr + ndx;
		unsigned char *data = (unsigned char *)(elf->data +
				mem_hdr->sh_offset);
		unsigned char *data_end = data + mem_hdr->sh_size;
		unsigned char *dest = lib->dl_out->data + sec.offset +
			lib->link_info->sec[ndx];

		ret = dsp_elf32_check_range(elf,
				(size_t)mem_hdr->sh_offset + mem_hdr->sh_size);
		if (ret) {
			DL_ERROR("invalid data range(%u/%u)\n",
					mem_hdr->sh_offset,
					mem_hdr->sh_size);
			return -1;
		}

		if (sec.offset >
				sec.offset + lib->link_info->sec[ndx] +
				mem_hdr->sh_size) {
			DL_ERROR("Overflow happened.\n");
			return -1;
		}

		if (lib->link_info->sec[ndx] >
				sec.offset + lib->link_info->sec[ndx] +
				mem_hdr->sh_size) {
			DL_ERROR("Overflow happened.\n");
			return -1;
		}

		if (mem_hdr->sh_size >
				sec.offset + lib->link_info->sec[ndx] +
				mem_hdr->sh_size) {
			DL_ERROR("Overflow happened.\n");
			return -1;
		}

		if (sec.offset + lib->link_info->sec[ndx] + mem_hdr->sh_size >
				lib->dl_out_data_size) {
			DL_ERROR("invalid dest range(%u/%lu/%u/%zu)\n",
					sec.offset,
					lib->link_info->sec[ndx],
					mem_hdr->sh_size,
					lib->dl_out_data_size);
			return -1;
		}

		DL_DEBUG("load sec %u\n", ndx);
		DL_DEBUG("Dest : %p, Src : %p, size : %u\n",
			dest, data, mem_hdr->sh_size);

		if (rev_endian) {
			for (; data < data_end; data += 4, dest += 4) {
				int idx;

				for (idx = 0; idx < 4; idx++)
					dest[idx] = data[3 - idx];
			}
		} else
			memcpy(dest, data, mem_hdr->sh_size);
	}

	return 0;
}

static int __dsp_lib_manager_load_mem(struct dsp_lib *lib,
	struct dsp_elf32_mem *mem, struct dsp_dl_out_section sec,
	int rev_endian)
{
	int ret;

	DL_DEBUG("load mem\n");
	DL_DEBUG("Load robss\n");
	ret = __dsp_lib_manager_load_bss_sec(lib, &mem->robss, sec);
	if (ret) {
		DL_ERROR("Failed to load robss\n");
		return -1;
	}

	DL_DEBUG("Load rodata\n");
	ret = __dsp_lib_manager_load_sec(lib, &mem->rodata, sec, rev_endian);
	if (ret) {
		DL_ERROR("Failed to load rodata\n");
		return -1;
	}

	DL_DEBUG("Load bss\n");
	ret = __dsp_lib_manager_load_bss_sec(lib, &mem->bss, sec);
	if (ret) {
		DL_ERROR("Failed to load bss\n");
		return -1;
	}

	DL_DEBUG("Load data\n");
	ret = __dsp_lib_manager_load_sec(lib, &mem->data, sec, rev_endian);
	if (ret) {
		DL_ERROR("Failed to load data\n");
		return -1;
	}

	return 0;
}

int dsp_lib_manager_load_libs(struct dsp_lib **libs, size_t libs_size)
{
	int ret;
	unsigned int idx;
	struct dsp_dl_out *dl_out = NULL;

	DL_DEBUG(DL_BORDER);
	DL_DEBUG("DL lib_manager load libs\n");
	for (idx = 0; idx < libs_size; idx++) {

		if (libs[idx]->loaded)
			continue;

		dl_out = libs[idx]->dl_out;
		if (dl_out) {
			DL_DEBUG("DL out data addr : %pK\n",
					dl_out->data);

			DL_DEBUG("Load Kernel table\n");
			ret = __dsp_lib_manager_load_kernel_table(
					libs[idx],
					dl_out->kernel_table);
			if (ret == -1) {
				DL_ERROR("[%s] CHK_ERR\n", __func__);
				return -1;
			}
		}

		if (libs[idx]->pm) {
			DL_DEBUG("Load PM\n");
			ret = __dsp_lib_manager_load_pm(libs[idx]);
			if (ret) {
				DL_ERROR("Failed to load PM\n");
				return -1;
			}
		}

		if (libs[idx]->gpt) {
			DL_DEBUG("Load GPT\n");
			__dsp_lib_manager_load_gpt(libs[idx]);
		}

		if (dl_out && libs[idx]->dl_out_mem) {
			DL_DEBUG("Load DM\n");
			ret = __dsp_lib_manager_load_mem(libs[idx],
					&libs[idx]->elf->DMb,
					dl_out->DM_sh, 0);
			if (ret) {
				DL_ERROR("Failed to load DM\n");
				return -1;
			}

			DL_DEBUG("Load DM_local\n");
			ret = __dsp_lib_manager_load_mem(libs[idx],
					&libs[idx]->elf->DMb_local,
					dl_out->DM_local, 0);
			if (ret) {
				DL_ERROR("Failed to load DM_local\n");
				return -1;
			}

			DL_DEBUG("Load TCM\n");
			ret = __dsp_lib_manager_load_mem(libs[idx],
					&libs[idx]->elf->TCMb,
					dl_out->TCM_sh, 0);
			if (ret) {
				DL_ERROR("Failed to load TCM\n");
				return -1;
			}

			DL_DEBUG("Load TCM_local\n");
			ret = __dsp_lib_manager_load_mem(libs[idx],
					&libs[idx]->elf->TCMb_local,
					dl_out->TCM_local, 0);
			if (ret) {
				DL_ERROR("Failed to load TCM_local\n");
				return -1;
			}

			DL_DEBUG("Load Shared mem\n");
			ret = __dsp_lib_manager_load_mem(libs[idx],
					&libs[idx]->elf->SFRw,
					dl_out->sh_mem, 1);
			if (ret) {
				DL_ERROR("Failed to load Shared mem\n");
				return -1;
			}
		}
		libs[idx]->loaded = 1;
	}

	return 0;
}

void dsp_lib_manager_unload_libs(struct dsp_lib **libs, size_t libs_size)
{
	unsigned int idx;

	DL_DEBUG("DL lib_manager unload libs\n");

	dsp_lib_manager_dec_ref_cnt(libs, libs_size);
	for (idx = 0; idx < libs_size; idx++) {
		if (libs[idx]->ref_cnt == 0)
			dsp_lib_unload(libs[idx]);
	}

}
