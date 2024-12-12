// SPDX-License-Identifier: GPL-2.0-only
/*
 * sec_debug_logbuf_sysfs.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/sec_debug.h>
#include <dt-bindings/soc/samsung/exynos2100-debug.h>

#include "sec_debug_internal.h"

enum secdbg_logbuf_type {
	SECDBG_LOGB_LAST_KMSG,
	SECDBG_LOGB_FIRST_KMSG,
	SECDBG_LOGB_END_OF_KMSG,
	SECDBG_LOGB_DEBUG_HISTORY,
	SECDBG_LOGB_AUTO_COMMENT,
	SECDBG_LOGB_DUMP_SUMMARY,
	SECDBG_LOGB_INIT_TASK,
	NR_MAX_SECDBG_LOGB,
};

static int current_target;

struct secdbg_logbuf {
	const char *name;
};

static struct secdbg_logbuf sdn_dump[NR_MAX_SECDBG_LOGB] = {
	{.name = "last_kmsg",},
	{.name = "first_kmsg",},
	{.name = "end_of_kmsg",},
	{.name = "debug_history",},
	{.name = "auto_comment",},
	{.name = "dump_summary",},
	{.name = "init_task",},
};

/* buffer size for sysfs is only PAGE_SIZE */
static ssize_t secdbg_logb_read(struct file *file, char __user *buf,
				  size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count, ret = 0;
	unsigned long log_pa, log_sz;
	void *log_va;

	log_va = NULL;
	log_pa = 0;
	log_sz = 0;

	if (current_target == SECDBG_LOGB_DEBUG_HISTORY) {
		log_pa = secdbg_base_get_buf_base(SDN_MAP_DEBUG_PARAM);
		log_va = secdbg_base_get_ncva(log_pa);
		log_sz = secdbg_base_get_buf_size(SDN_MAP_DEBUG_PARAM);
	} else if (current_target == SECDBG_LOGB_AUTO_COMMENT) {
		log_pa = secdbg_base_get_buf_base(SDN_MAP_AUTO_COMMENT);
		log_va = secdbg_base_get_ncva(log_pa);
		log_sz = secdbg_base_get_buf_size(SDN_MAP_AUTO_COMMENT);
	} else if (current_target == SECDBG_LOGB_DUMP_SUMMARY) {
		log_pa = secdbg_base_get_buf_base(SDN_MAP_DUMP_SUMMARY);
		log_va = secdbg_base_get_ncva(log_pa);
		log_sz = secdbg_base_get_buf_size(SDN_MAP_DUMP_SUMMARY);
	} else if (current_target == SECDBG_LOGB_FIRST_KMSG) {
		log_pa = secdbg_base_get_buf_base(SDN_MAP_FIRST2M_LOG);
		log_va = secdbg_base_get_ncva(log_pa);
		log_sz = secdbg_base_get_buf_size(SDN_MAP_FIRST2M_LOG);
	} else if (current_target == SECDBG_LOGB_INIT_TASK) {
		log_pa = secdbg_base_get_buf_base(SDN_MAP_INITTASK_LOG);
		log_va = secdbg_base_get_ncva(log_pa);
		log_sz = secdbg_base_get_buf_size(SDN_MAP_INITTASK_LOG);
	} else if (current_target == SECDBG_LOGB_END_OF_KMSG) {
		log_va = (void *)(secdbg_base_get_end_addr() - (unsigned long)DSS_KERNEL_SIZE);
		log_sz = DSS_KERNEL_SIZE;
	}

	if (!log_va) {
		pr_crit("%s: fail to get va\n", __func__);

		ret = -EFAULT;
		goto fail;
	}

	if (pos >= log_sz) {
		pr_crit("%s: pos %llx , target log size: %lx\n", __func__, pos, log_sz);

		ret = -EFAULT;
		goto fail;
	}

	count = min(len, (size_t)(log_sz - pos));

	if (copy_to_user(buf, log_va + pos, count)) {
		pr_crit("%s: fail to copy to use\n", __func__);

		ret = -EFAULT;
		goto fail;
	}

	*offset += count;
	ret = count;

fail:
	return ret;
}

#define MAX_LEN_INPUT_BUF		(16)

static ssize_t secdbg_logb_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	int err = 0, nr_inp = MAX_LEN_INPUT_BUF, i;
	char inp[MAX_LEN_INPUT_BUF] = {0, };

	if (!buf) {
		err = -ENODEV;
		goto out;
	}

	if (nr_inp > count)
		nr_inp = count;

	if (copy_from_user(inp, buf, nr_inp)) {
		pr_err("%s: copy_from_user failed\n", __func__);
		err = -EFAULT;
		goto out;
	}

	pr_crit("%s: input: %s\n", __func__, inp);

	for (i = 0; i < NR_MAX_SECDBG_LOGB; i++) {
		if (!strncmp(inp, sdn_dump[i].name, strlen(sdn_dump[i].name))) {
			pr_crit("%s: %s(%s) -> %d\n", __func__, inp, sdn_dump[i].name, i);

			current_target = i;
			break;
		}
	}

out:
	return err < 0 ? err : count;
}

static const struct file_operations secdbg_logb_file_ops = {
	.owner = THIS_MODULE,
	.read = secdbg_logb_read,
	.write = secdbg_logb_write,
};

#define SECDBG_LOGB_BUF_SIZE		(0x00400000)

static int __init secdbg_logb_late_init(void)
{
	struct proc_dir_entry *entry;

	pr_info("%s: try to make procfs for secdbg logbuf\n", __func__);

	entry = proc_create("secdbg_logbuf", S_IFREG | 0644, NULL, &secdbg_logb_file_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry debug hist\n", __func__);
		return 0;
	}

	pr_info("%s: success to create proc entry\n", __func__);
	proc_set_size(entry, (size_t)SECDBG_LOGB_BUF_SIZE);

	return 0;
}
late_initcall(secdbg_logb_late_init);

MODULE_DESCRIPTION("Samsung Debug LogBuf driver");
MODULE_LICENSE("GPL v2");
