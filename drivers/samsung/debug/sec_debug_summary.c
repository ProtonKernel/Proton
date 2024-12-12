// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 */

#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include "sec_debug_internal.h"

static unsigned long summ_base;
static unsigned long summ_size;
static void *summ_va_base;

static ssize_t secdbg_summ_read(struct file *file, char __user *buf,
				  size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count, ret = 0;
	char *base = NULL;

	if (!summ_size) {
		pr_crit("%s: size 0? %lx\n", __func__, summ_size);

		ret = -ENXIO;

		goto fail;
	}

	if (!summ_base) {
		pr_crit("%s: no base? %lx\n", __func__, summ_base);

		ret = -ENXIO;

		goto fail;
	}

	if (pos >= summ_size) {
		pr_crit("%s: pos %llx , summ_size: %lx\n", __func__, pos, summ_size);

		ret = 0;

		goto fail;
	}

	count = min(len, (size_t)(summ_size - pos));

	base = summ_va_base;
	if (!base) {
		pr_crit("%s: fail to get va (%lx)\n", __func__, summ_base);

		ret = -EFAULT;

		goto fail;
	}

	if (copy_to_user(buf, base + pos, count)) {
		pr_crit("%s: fail to copy to use\n", __func__);

		ret = -EFAULT;
	} else {
		pr_debug("%s: base: %p\n", __func__, base);

		*offset += count;
		ret = count;
	}

fail:
	return ret;
}

static const struct file_operations summ_file_ops = {
	.owner = THIS_MODULE,
	.read = secdbg_summ_read,
};

static int __init secdbg_summ_late_init(void)
{
	struct proc_dir_entry *entry;

	summ_base = secdbg_base_get_buf_base(SDN_MAP_DUMP_SUMMARY);
	summ_va_base = secdbg_base_get_ncva(summ_base);
	summ_size = secdbg_base_get_buf_size(SDN_MAP_DUMP_SUMMARY);

	pr_info("%s: base: %p(%lx) size: %lx\n", __func__, summ_va_base, summ_base, summ_size);

	if (!summ_base || !summ_size)
		return 0;

	entry = proc_create("reset_summary", S_IFREG | 0444, NULL, &summ_file_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry dump summary\n", __func__);

		return 0;
	}

	pr_info("%s: success to create proc entry\n", __func__);

	proc_set_size(entry, (size_t)summ_size);

	return 0;
}
late_initcall(secdbg_summ_late_init);

MODULE_DESCRIPTION("Samsung Debug summary log driver");
MODULE_LICENSE("GPL");
