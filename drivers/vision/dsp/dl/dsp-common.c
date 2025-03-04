// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/slab.h>
#include "dl/dsp-common.h"
#include "dl/dsp-list.h"

#define DL_LOG_BUF_MAX		(1024)

struct dsp_dl_mem {
	const char *msg;
	struct dsp_list_node node;
	char data[0];
};

static char *dsp_dl_log_buf;
static struct dsp_list_head *dl_mem_list;

static void *__dsp_alloc(unsigned int size)
{
	return kzalloc(size, GFP_KERNEL);
}

void dsp_dl_lib_file_reset(struct dsp_dl_lib_file *file)
{
	file->r_ptr = 0;
}

int dsp_dl_lib_file_read(char *buf, unsigned int size,
	struct dsp_dl_lib_file *file)
{
	if (file->r_ptr + size > file->size)
		return -1;

	memcpy(buf, (char *)(file->mem) + file->r_ptr, size);
	file->r_ptr += size;
	return size;
}

static void __dsp_dl_mem_init(void)
{
	dl_mem_list = (struct dsp_list_head *)__dsp_alloc(
			sizeof(struct dsp_list_head));
	dsp_list_head_init(dl_mem_list);
}

static void __dsp_dl_mem_free(void)
{
	kfree(dl_mem_list);
}

static void __dsp_dl_mem_print(void)
{
	struct dsp_list_node *node;

	DL_DEBUG("Print dl mems\n");
	dsp_list_for_each(node, dl_mem_list)
	DL_DEBUG("allocated memory(%s)\n",
		(container_of(node, struct dsp_dl_mem,
				node))->msg);
}

void dsp_dl_put_log_buf(const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	snprintf(dsp_dl_log_buf + strlen(dsp_dl_log_buf), DL_LOG_BUF_MAX,
			"%pV", &vaf);
	va_end(args);
}

void dsp_dl_print_log_buf(int level, const char *func)
{
	switch (level) {
	case DL_LOG_ERROR:
		dsp_err("[%-30s]%s", func, dsp_dl_log_buf);
		dsp_dl_log_buf[0] = '\0';
		break;
	case DL_LOG_INFO:
		dsp_info("%s", dsp_dl_log_buf);
		dsp_dl_log_buf[0] = '\0';
		break;
	case DL_LOG_DEBUG:
	default:
		dsp_dl_dbg("[%-30s]%s", func, dsp_dl_log_buf);
		dsp_dl_log_buf[0] = '\0';
		break;
	}
}

void dsp_common_init(void)
{
	dsp_dl_log_buf = (char *)__dsp_alloc(DL_LOG_BUF_MAX);
	dsp_dl_log_buf[0] = '\0';
	__dsp_dl_mem_init();
}

void dsp_common_free(void)
{
	__dsp_dl_mem_print();
	__dsp_dl_mem_free();
	kfree(dsp_dl_log_buf);
}

void *dsp_dl_malloc(size_t size, const char *msg)
{
	struct dsp_dl_mem *mem;

	mem = (struct dsp_dl_mem *)__dsp_alloc(sizeof(*mem) + size);
	if (!mem) {
		DL_ERROR("%s malloc(%zu) is failed\n", msg, size);
		return NULL;
	}

	mem->msg = msg;
	dsp_list_node_init(&mem->node);
	dsp_list_node_push_back(dl_mem_list, &mem->node);

	return (void *)(mem->data);
}

void dsp_dl_free(void *data)
{
	struct dsp_dl_mem *mem = container_of(data, struct dsp_dl_mem, data);

	dsp_list_node_remove(dl_mem_list, &mem->node);
	kfree(mem);
}
