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

static unsigned long dhist_base;
static unsigned long dhist_size;
static void *dhist_va_base;

static ssize_t secdbg_hist_read(struct file *file, char __user *buf,
				  size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count, ret = 0;
	char *base = NULL;

	if (!dhist_size) {
		pr_crit("%s: size 0? %lx\n", __func__, dhist_size);

		ret = -ENXIO;

		goto fail;
	}

	if (!dhist_base) {
		pr_crit("%s: no base? %lx\n", __func__, dhist_base);

		ret = -ENXIO;

		goto fail;
	}

	if (pos >= dhist_size) {
		pr_crit("%s: pos %llx , dhist: %lx\n", __func__, pos, dhist_size);

		ret = 0;

		goto fail;
	}

	count = min(len, (size_t)(dhist_size - pos));

	base = dhist_va_base;
	if (!base) {
		pr_crit("%s: fail to get va (%lx)\n", __func__, dhist_base);

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

static const struct file_operations dhist_file_ops = {
	.owner = THIS_MODULE,
	.read = secdbg_hist_read,
};

static int __init secdbg_hist_late_init(void)
{
	struct proc_dir_entry *entry;
	char *p;

	dhist_base = secdbg_base_get_buf_base(SDN_MAP_DEBUG_PARAM);
	dhist_va_base = secdbg_base_get_ncva(dhist_base);
	dhist_size = secdbg_base_get_buf_size(SDN_MAP_DEBUG_PARAM);

	pr_info("%s: base: %p(%lx) size: %lx\n", __func__, dhist_va_base, dhist_base, dhist_size);

	if (!dhist_base || !dhist_size)
		return 0;

	p = dhist_va_base;
	pr_info("%s: dummy: %x\n", __func__, *p);
	pr_info("%s: magic: %x\n", __func__, *(unsigned int *)(p + 4));
	pr_info("%s: version: %x\n", __func__, *(unsigned int *)(p + 8));

	entry = proc_create("debug_history", S_IFREG | 0444, NULL, &dhist_file_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry debug hist\n", __func__);

		return 0;
	}

	pr_info("%s: success to create proc entry\n", __func__);

	proc_set_size(entry, (size_t)dhist_size);

	return 0;
}
late_initcall(secdbg_hist_late_init);

MODULE_DESCRIPTION("Samsung Debug history log driver");
MODULE_LICENSE("GPL");
