// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dl/dsp-dl-out-manager.h"
#include "dl/dsp-elf-loader.h"
#include "dl/dsp-lib-manager.h"
#include "dl/dsp-xml-parser.h"

#define DL_DL_OUT_ALIGN		(4)
#define DL_HASH_SIZE		(300)
#define DL_HASH_END		(0xFFFFFFFF)

static unsigned int *dl_hash;
static struct dsp_tlsf *out_manager;

unsigned int dsp_dl_hash_get_key(char *data)
{
	int len;
	unsigned int hash, tmp;
	int rem;

	if (data == NULL)
		return 0;

	len = strlen(data);
	if (len <= 0)
		return 0;

	hash = len;

	rem = len & 3;
	len >>= 2;

	for (; len > 0; len--) {
		hash += get16bits(data);
		tmp = (get16bits(data + 2) << 11) ^ hash;
		hash = (hash << 16) ^ tmp;
		data += 2 * sizeof(unsigned short);
		hash += hash >> 11;
	}

	switch (rem) {
	case 3:
		hash += get16bits(data);
		hash ^= hash << 16;
		hash ^= ((signed char)data[sizeof(unsigned short)]) << 18;
		hash += hash >> 11;
		break;
	case 2:
		hash += get16bits(data);
		hash ^= hash << 11;
		hash += hash >> 17;
		break;
	case 1:
		hash += (signed char)(*data);
		hash ^= hash << 10;
		hash += hash >> 1;
	}

	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 4;
	hash += hash >> 17;
	hash ^= hash << 25;
	hash += hash >> 6;

	return hash % DL_HASH_SIZE;
}

void dsp_dl_hash_init(void)
{
	int idx;
	unsigned int *node;

	for (idx = 0; idx < DL_HASH_SIZE; idx++) {
		node = dl_hash + idx;
		*node = DL_HASH_END;
	}
}

void dsp_dl_hash_push(struct dsp_dl_out *dl_out)
{
	unsigned int key = dsp_dl_hash_get_key(dl_out->data);

	dl_out->hash_next = dl_hash[key];
	dl_hash[key] = (unsigned long)dl_out - (unsigned long)dl_hash;
	DL_DEBUG("key : %u, head : %u\n", key, dl_hash[key]);
}

void dsp_dl_hash_pop(char *k)
{
	unsigned int key = dsp_dl_hash_get_key(k);
	unsigned int *node = dl_hash + key;
	struct dsp_dl_out *next;

	while (*node != DL_HASH_END) {
		next = (struct dsp_dl_out *)((unsigned long)dl_hash + *node);

		if (strcmp(next->data, k) == 0) {
			*node = next->hash_next;
			return;
		}

		node = &next->hash_next;
	}

	DL_ERROR("No hash node for %s\n", k);
}

void dsp_dl_hash_print(void)
{
	int idx;
	unsigned int *node;
	struct dsp_dl_out *next;

	for (idx = 0; idx < DL_HASH_SIZE; idx++) {
		node = dl_hash + idx;

		while (*node != DL_HASH_END) {
			DL_INFO("\n");
			DL_INFO("Hash node: key(%u) offset(%u)\n", idx, *node);
			next = (struct dsp_dl_out *)(
					(unsigned long)dl_hash + *node);
			DL_INFO("Library name: %s\n", next->data);
			DL_INFO("Hash next offset: 0x%x\n", next->hash_next);
			node = &next->hash_next;
		}
	}
}

static unsigned int __dsp_dl_out_offset_align(unsigned int offset)
{
	return (offset + 0x3) & ~0x3;
}

int dsp_dl_out_create(struct dsp_lib *lib)
{
	int ret;
	unsigned int offset = 0;
	struct dsp_xml_lib *xml_lib;

	DL_DEBUG("DL out create\n");
	lib->dl_out = (struct dsp_dl_out *)dsp_dl_malloc(
			sizeof(*lib->dl_out), "lib dl_out");

	lib->dl_out->hash_next = DL_HASH_END;

	offset += strlen(lib->name) + 1;
	offset = __dsp_dl_out_offset_align(offset);

	lib->dl_out->kernel_table.offset = offset;
	ret = dsp_hash_get(&xml_libs->lib_hash, lib->name,
			(void **)&xml_lib);
	if (ret == -1) {
		DL_ERROR("No global table for %s\n", lib->name);
		dsp_dl_free(lib->dl_out);
		lib->dl_out = NULL;
		return -1;
	}

	lib->dl_out->kernel_table.size = xml_lib->kernel_cnt *
		sizeof(struct dsp_dl_kernel_table);
	offset += lib->dl_out->kernel_table.size;
	offset = __dsp_dl_out_offset_align(offset);

	lib->dl_out->DM_sh.offset = offset;
	lib->dl_out->DM_sh.size = dsp_elf32_get_mem_size(&lib->elf->DMb,
			lib->elf);
	if (lib->dl_out->DM_sh.size == UINT_MAX) {
		DL_ERROR("failed to get DM_SH mem size\n");
		return -1;
	}

	offset += lib->dl_out->DM_sh.size;
	offset = __dsp_dl_out_offset_align(offset);

	lib->dl_out->DM_local.offset = offset;
	lib->dl_out->DM_local.size = dsp_elf32_get_mem_size(
			&lib->elf->DMb_local, lib->elf);
	if (lib->dl_out->DM_local.size == UINT_MAX) {
		DL_ERROR("failed to get DM_local mem size\n");
		return -1;
	}

	offset += lib->dl_out->DM_local.size;
	offset = __dsp_dl_out_offset_align(offset);

	lib->dl_out->TCM_sh.offset = offset;
	lib->dl_out->TCM_sh.size = dsp_elf32_get_mem_size(&lib->elf->TCMb,
			lib->elf);
	if (lib->dl_out->TCM_sh.size == UINT_MAX) {
		DL_ERROR("failed to get TCM_SH mem size\n");
		return -1;
	}

	offset += lib->dl_out->TCM_sh.size;
	offset = __dsp_dl_out_offset_align(offset);

	lib->dl_out->TCM_local.offset = offset;
	lib->dl_out->TCM_local.size = dsp_elf32_get_mem_size(
			&lib->elf->TCMb_local, lib->elf);
	if (lib->dl_out->TCM_local.size == UINT_MAX) {
		DL_ERROR("failed to get TCM_local mem size\n");
		return -1;
	}

	offset += lib->dl_out->TCM_local.size;
	offset = __dsp_dl_out_offset_align(offset);

	lib->dl_out->sh_mem.offset = offset;

	lib->dl_out->sh_mem.size = dsp_elf32_get_mem_size(&lib->elf->SFRw,
			lib->elf);
	if (lib->dl_out->sh_mem.size == UINT_MAX) {
		DL_ERROR("failed to get sh_mem mem size\n");
		return -1;
	}

	return 0;
}

size_t dsp_dl_out_get_size(struct dsp_dl_out *dl_out)
{
	if (sizeof(*dl_out) >
			sizeof(*dl_out) +
			dl_out->sh_mem.offset + dl_out->sh_mem.size)
		return UINT_MAX;

	if (dl_out->sh_mem.offset >
			sizeof(*dl_out) +
			dl_out->sh_mem.offset + dl_out->sh_mem.size)
		return UINT_MAX;

	if (dl_out->sh_mem.size >
			sizeof(*dl_out) +
			dl_out->sh_mem.offset + dl_out->sh_mem.size)
		return UINT_MAX;

	return sizeof(*dl_out) + dl_out->sh_mem.offset + dl_out->sh_mem.size;
}

static void __dsp_dl_out_cpy_metadata(struct dsp_dl_out *op1,
	struct dsp_dl_out *op2)
{
	memcpy(op1, op2, sizeof(*op1));
}

static void __dsp_dl_out_print_sec_data(struct dsp_dl_out *dl_out,
	struct dsp_dl_out_section sec)
{
	unsigned int idx;
	unsigned int *data = (unsigned int *)(dl_out->data + sec.offset);
	unsigned int sec_end = sec.size / sizeof(unsigned int);

	for (idx = 0; idx < sec_end; idx++) {
		DL_BUF_STR("0x%08x ", data[idx]);

		if ((idx + 1) % 4 == 0 || idx == sec_end - 1) {
			DL_BUF_STR("\n");
			DL_PRINT_BUF(DEBUG);
		}
	}
}

static void __dsp_dl_out_print_kernel_table(struct dsp_dl_out *dl_out)
{
	struct dsp_dl_out_section sec = dl_out->kernel_table;
	unsigned int idx;
	unsigned int *data = (unsigned int *)(dl_out->data + sec.offset);
	unsigned int sec_end = sec.size / sizeof(unsigned int);

	for (idx = 0; idx < sec_end; idx++) {
		DL_BUF_STR("0x%08x ", data[idx]);

		if ((idx + 1) % 3 == 0 || idx == sec_end - 1) {
			DL_BUF_STR("\n");
			DL_PRINT_BUF(DEBUG);
		}
	}
}

static void __dsp_dl_out_print_data(struct dsp_dl_out *dl_out)
{
	DL_DEBUG("Kernel address table\n");
	DL_DEBUG("    Pre        Exe       Post\n");
	__dsp_dl_out_print_kernel_table(dl_out);

	DL_DEBUG("DM shared data\n");
	__dsp_dl_out_print_sec_data(dl_out, dl_out->DM_sh);

	DL_DEBUG("DM thread local data\n");
	__dsp_dl_out_print_sec_data(dl_out, dl_out->DM_local);

	DL_DEBUG("TCM shared data\n");
	__dsp_dl_out_print_sec_data(dl_out, dl_out->TCM_sh);

	DL_DEBUG("TCM thread local data\n");
	__dsp_dl_out_print_sec_data(dl_out, dl_out->TCM_local);

	DL_DEBUG("Shared memory data\n");
	__dsp_dl_out_print_sec_data(dl_out, dl_out->sh_mem);
}

void dsp_dl_out_print(struct dsp_dl_out *dl_out)
{
	DL_INFO("Library name: %s\n", dl_out->data);
	DL_INFO("Hash next offset: 0x%x\n", dl_out->hash_next);
	DL_INFO("Gpt address: 0x%x\n", dl_out->gpt_addr);
	DL_INFO("\n");
	DL_INFO("Data offsets\n");
	DL_INFO("Kernel table: offset(%u) size(%u)\n",
		dl_out->kernel_table.offset, dl_out->kernel_table.size);
	DL_INFO("DM shared data: offset(%u) size(%u\n",
		dl_out->DM_sh.offset, dl_out->DM_sh.size);
	DL_INFO("DM thread local data: offset(%u) size(%u)\n",
		dl_out->DM_local.offset, dl_out->DM_local.size);
	DL_INFO("TCM shared data: offset(%u) size(%u)\n",
		dl_out->TCM_sh.offset, dl_out->TCM_sh.size);
	DL_INFO("TCM thread local data: offset(%u) size(%u)\n",
		dl_out->TCM_local.offset, dl_out->TCM_local.size);
	DL_INFO("Shared memory data: offset(%u) size(%u)\n",
		dl_out->sh_mem.offset, dl_out->sh_mem.size);
	DL_INFO("\n");
	DL_DEBUG("Data loaded\n");
	__dsp_dl_out_print_data(dl_out);
}

int dsp_dl_out_manager_init(unsigned long start_addr, size_t size)
{
	unsigned long mem_str;
	size_t mem_size;

	DL_DEBUG("DL out manager init\n");
	out_manager = (struct dsp_tlsf *)dsp_dl_malloc(
			sizeof(struct dsp_tlsf), "Out manager");

	if (size <= sizeof(unsigned int) * DL_HASH_SIZE) {
		DL_ERROR("Size(%zu) is not enough for DL_out\n", size);
		dsp_dl_free(out_manager);
		return -1;
	}

	dl_hash = (unsigned int *)start_addr;
	DL_DEBUG("DL hash : 0x%p\n", dl_hash);

	dsp_dl_hash_init();

	mem_str = (unsigned long)((unsigned int *)start_addr + DL_HASH_SIZE);
	mem_size = size - sizeof(unsigned int) * DL_HASH_SIZE;
	dsp_tlsf_init(out_manager, mem_str, mem_size, DL_DL_OUT_ALIGN);
	return 0;
}

int dsp_dl_out_manager_free(void)
{
	dsp_tlsf_delete(out_manager);
	dsp_dl_free(out_manager);
	return 0;
}

void dsp_dl_out_manager_print(void)
{
	DL_INFO(DL_BORDER);
	DL_INFO("Dynamic loader output manager\n");
	DL_INFO("\n");
	dsp_tlsf_print(out_manager);
	DL_INFO("\n");
	DL_INFO("Output hash table\n");
	dsp_dl_hash_print();
}

int dsp_dl_out_manager_alloc_libs(struct dsp_lib **libs, int libs_size,
	int *pm_inv)
{
	int ret, idx;

	DL_DEBUG("Alloc DL out\n");

	for (idx = 0; idx < libs_size; idx++) {
		if (!libs[idx]->dl_out) {
			DL_DEBUG("Alloc DL out for library %s\n",
				libs[idx]->name);
			ret = dsp_dl_out_alloc(libs[idx], pm_inv);

			if (ret == -1) {
				DL_ERROR("Alloc DL out failed\n");
				return -1;
			}
		}
	}

	return 0;
}

static int __dsp_dl_out_alloc_mem(size_t size, struct dsp_lib *lib,
	int *pm_inv)
{
	int ret;

	ret = dsp_tlsf_malloc(size, &lib->dl_out_mem,
			out_manager);
	if (ret == -1) {
		if (dsp_tlsf_can_be_loaded(out_manager, size)) {
			dsp_lib_manager_delete_no_ref();
			*pm_inv = 1;
			__dsp_dl_out_alloc_mem(size, lib, pm_inv);
		} else {
			DL_ERROR("Can not alloc DL out memory for library %s\n",
				lib->name);
			return -1;
		}
	}

	return 0;
}

int dsp_dl_out_alloc(struct dsp_lib *lib, int *pm_inv)
{
	int ret;
	size_t dl_out_size;
	int alloc_ret;
	unsigned long mem_addr;

	DL_DEBUG("DL out alloc\n");
	ret = dsp_dl_out_create(lib);
	if (ret == -1) {
		DL_ERROR("[%s] CHK_ERR\n", __func__);
		return -1;
	}

	dl_out_size = dsp_dl_out_get_size(lib->dl_out);
	if (dl_out_size == UINT_MAX) {
		DL_ERROR("[%s] failed to get dl_out size.\n", __func__);
		return -1;
	}
	DL_DEBUG("DL_out_size : %zu\n", dl_out_size);

	alloc_ret = __dsp_dl_out_alloc_mem(dl_out_size, lib,
			pm_inv);
	if (alloc_ret == -1) {
		dsp_dl_free(lib->dl_out);
		lib->dl_out = NULL;
		lib->dl_out_mem = NULL;
		return -1;
	}

	lib->dl_out_mem->lib = lib;

	mem_addr = lib->dl_out_mem->start_addr;
	__dsp_dl_out_cpy_metadata((struct dsp_dl_out *)mem_addr, lib->dl_out);
	dsp_dl_free(lib->dl_out);

	lib->dl_out = (struct dsp_dl_out *)mem_addr;
	lib->dl_out_data_size = dl_out_size - sizeof(*lib->dl_out);

	strcpy(lib->dl_out->data, lib->name);
	DL_DEBUG("lib name : %s\n", lib->dl_out->data);

	dsp_dl_hash_push(lib->dl_out);

	return 0;
}

void dsp_dl_out_free(struct dsp_lib *lib)
{
	if (lib->dl_out && lib->dl_out_mem) {
		dsp_dl_hash_pop(lib->dl_out->data);
		dsp_tlsf_free(lib->dl_out_mem, out_manager);
		lib->dl_out = NULL;
		lib->dl_out_mem = NULL;
	}
}
