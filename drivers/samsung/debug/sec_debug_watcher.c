// SPDX-License-Identifier: GPL-2.0-only
/*
 * sec_debug_test.c
 *
 * author: Myoungae Kim
 * email: myoungjae.kim@samsung.com
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <asm-generic/io.h>
#include <linux/ctype.h>
#include <linux/sec_debug.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/preempt.h>
#include <linux/rwsem.h>
#include <linux/moduleparam.h>
#include <linux/cpumask.h>
#include <linux/reboot.h>
#include <asm/stackprotector.h>

#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/exynos-pmu-if.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <uapi/linux/sched/types.h>

#include <asm/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <uapi/linux/hw_breakpoint.h>

#include <linux/pid.h>
#include <linux/rculist.h>
#include <linux/sched/task.h>

#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/errno.h>

#include "sec_debug_internal.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[sec_wp] " fmt

#define WATCHER_TEST			0

#define WATCHER_OPT_OPMODE			0x1
#define WATCHER_OPT_ADDR			0x2
#define WATCHER_OPT_RWMODE			0x4
#define WATCHER_OPT_PID				0x8
#define WATCHER_OPT_NOTIFY			0x10

#define WP_SIZE_OF_SYMBOL			64

enum {
	SEC_WP_OP_NONE,
	SEC_WP_OP_CLEAR,
	SEC_WP_OP_SET_KERNEL,
	SEC_WP_OP_SET_USER,
};

enum {
	SEC_WP_SUBOP_NONE = 0,
	SEC_WP_SUBOP_TASK,
};

enum {
	SEC_WP_NOTI_NONE,
	SEC_WP_NOTI_PANIC,
	SEC_WP_NOTI_DUMPSTACK,
};

enum {
	SEC_WP_DISABLED,
	SEC_WP_ENABLED_KERNEL,
	SEC_WP_ENABLED_USER,
};

#define SEC_WP_MD_READ			1
#define SEC_WP_MD_WRITE			2


struct __input_env {
	int op;
	int sub_op;
	u64 addr;
	char symbol[WP_SIZE_OF_SYMBOL];
	int rw_mode;
	pid_t pid;
	int noti_type;
	u64 opt_bitmap;
};

static int enable;
static struct perf_event * __percpu *kernel_hbp;
static struct perf_event *user_hbp;

#if WATCHER_TEST
static int test_var;

static int sec_debug_get_test_wp(char *buffer, const struct kernel_param *kp)
{
	int size = 0;

	size = sprintf(buffer, "%lx\n", (unsigned long)&test_var);

	return size;
}

static int sec_debug_set_test_wp(const char *val, const struct kernel_param *kp)
{
	pr_info("access test_var %d\n", test_var);
	test_var = 0;

	return 0;
}

static const struct kernel_param_ops sec_debug_test_wp_ops = {
		.set	= sec_debug_set_test_wp,
		.get	= sec_debug_get_test_wp,
};

module_param_cb(test_wp, &sec_debug_test_wp_ops, NULL, 0600);
#endif

static void __default_hbp_handler(struct perf_event *bp,
			       struct perf_sample_data *data,
			       struct pt_regs *regs)
{
	pr_info("%s\n", __func__);
	BUG();
}

static void __print_args(char **argv, int argc)
{
	int i;

	for (i = 0; i < argc; i++)
		pr_info("argv[%d]: %s\n", i, argv[i]);
}

static int __opt_is_same(const char *src, const char *dst)
{
	return !strncmp(src, dst, strlen(dst));
}

static int __parse_input(struct __input_env *input_env, char **argv, int argc)
{
	int ret = 0;
	char *opt_name, *opt_value;
	int opt_offset = 0;

	if (!argv || argc <= 0 || argc % 2 != 0)
		return -1;

	/* input argument parsing */
	do {
		opt_name = argv[opt_offset];
		opt_value = argv[opt_offset+1];

		if (__opt_is_same(opt_name, "-o")) {
			pr_info("-o is given:%s\n", opt_value);
			input_env->opt_bitmap |= WATCHER_OPT_OPMODE;
			switch (opt_value[0]) {
			case 'k':
			case 'K':
				input_env->op = SEC_WP_OP_SET_KERNEL;
				break;
			case 'u':
			case 'U':
				input_env->op = SEC_WP_OP_SET_USER;
				break;
			case 'c':
			case 'C':
				input_env->op = SEC_WP_OP_CLEAR;
				break;
			default:
				pr_info("invalid operation argument\n");
				return -1;
			}
			pr_info("given opt_value is %d\n", input_env->op);
		} else if (__opt_is_same(opt_name, "-m")) {
			pr_info("-m is given:%s\n", opt_value);
			input_env->opt_bitmap |= WATCHER_OPT_RWMODE;

			input_env->rw_mode = 0;
			if (strchr(opt_value, 'r') != NULL)
				input_env->rw_mode |= HW_BREAKPOINT_R;
			if (strchr(opt_value, 'w') != NULL)
				input_env->rw_mode |= HW_BREAKPOINT_W;

			pr_info("given opt_value is %d\n", input_env->rw_mode);
		} else if (__opt_is_same(opt_name, "-a")) {
			u64 addr;
			int ret;

			pr_info("-a is given:%s\n", opt_value);
			input_env->opt_bitmap |= WATCHER_OPT_ADDR;

			ret = kstrtou64(opt_value, 16, &addr);

			if (ret < 0) {
				pr_info("invalid address\n");
				return -1;
			}
			input_env->addr = addr;
			pr_info("given opt_value is %lu\n", (unsigned long)input_env->addr);
		} else if (__opt_is_same(opt_name, "-s")) {
			pr_info("-s is given:%s\n", opt_value);
		} else if (__opt_is_same(opt_name, "-n")) {
			pr_info("-n is given:%s\n", opt_value);
			input_env->opt_bitmap |= WATCHER_OPT_NOTIFY;

			switch (opt_value[0]) {
			case 'p':
			case 'P':
				input_env->noti_type = SEC_WP_NOTI_PANIC;
				break;
			case 'd':
			case 'D':
				input_env->noti_type = SEC_WP_NOTI_DUMPSTACK;
				break;
			default:
				pr_info("invalid notify option\n");
				return -1;
			}
			pr_info("given opt_value is %d\n", input_env->noti_type);
		} else if (__opt_is_same(opt_name, "-p")) {
			u32 pid;
			int ret;

			pr_info("-p is given:%s\n", opt_value);
			input_env->opt_bitmap |= WATCHER_OPT_PID;

			ret = kstrtou32(opt_value, 10, &pid);

			if (ret < 0) {
				pr_info("invalid pid\n");
				return -1;
			}
			input_env->pid = (pid_t)pid;
			input_env->sub_op = SEC_WP_SUBOP_TASK;
		} else {
			pr_info("invalid option is given\n");
		}
		opt_offset += 2;
	} while (opt_offset < argc);

	/* additional invalidation */

	/* operation mode is mandatory */
	if ((input_env->opt_bitmap & WATCHER_OPT_OPMODE) == 0)
		return -1;

	/* SET operation should get along with valid address input */
	if ((input_env->op == SEC_WP_OP_SET_KERNEL
		|| input_env->op == SEC_WP_OP_SET_USER)
		&& (input_env->opt_bitmap & WATCHER_OPT_ADDR) == 0)
		return -1;

	return ret;
}

static ssize_t sec_debug_watchpoint_read(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	char buf[30];
	ssize_t ret;

	ret = snprintf(buf, sizeof(buf), "%d\n", enable);
	if (ret < 0)
		return ret;

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

#define SEC_WP_CLI_INPUT_SIZE			200

static ssize_t sec_debug_watchpoint_write(struct file *file, const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	ssize_t ret = count;
	char buf[SEC_WP_CLI_INPUT_SIZE];
	int argc = 0;
	char **argv;
	struct __input_env input_env = {0,};
	struct perf_event_attr attr;

	if (count > SEC_WP_CLI_INPUT_SIZE) {
		pr_info("invalid cmd\n");
		return count;
	}

	copy_from_user(buf, user_buf, count);
	pr_info("cmd:%s\n", buf);

	argv = argv_split(GFP_KERNEL, buf, &argc);

	if (argv == NULL) {
		pr_info("failed to split argument\n");
		goto err;
	}

	__print_args(argv, argc);

	ret = __parse_input(&input_env, argv, argc);
	if (ret < 0) {
		pr_info("failed to parse argument\n");
		goto err;
	}

	/* additional env set */
	if ((input_env.opt_bitmap & WATCHER_OPT_RWMODE) == 0)
		input_env.rw_mode = HW_BREAKPOINT_RW;

	if ((input_env.opt_bitmap & WATCHER_OPT_NOTIFY) == 0)
		input_env.noti_type = SEC_WP_NOTI_PANIC;

	/* watchpoint set */
	if (input_env.op == SEC_WP_OP_SET_KERNEL
		|| input_env.op == SEC_WP_OP_SET_USER) {
		hw_breakpoint_init(&attr);
		attr.bp_addr = (unsigned long)input_env.addr;
		attr.bp_len = HW_BREAKPOINT_LEN_4;
		attr.bp_type = input_env.rw_mode;
	}

	if (input_env.op == SEC_WP_OP_SET_KERNEL) {
		if (enable != SEC_WP_DISABLED) {
			pr_info("watchpoint is already working\n");
			goto err;
		}

		kernel_hbp = register_wide_hw_breakpoint(&attr, __default_hbp_handler, NULL);
		if (IS_ERR((void __force *)kernel_hbp)) {
			ret = PTR_ERR((void __force *)kernel_hbp);
			pr_info("failed to register kernel hbp\n");
			goto err;
		}

		pr_info("kernel watchpoint addr:0x%lx, type:%d\n",
			(unsigned long)input_env.addr, input_env.rw_mode);
		enable = SEC_WP_ENABLED_KERNEL;
	}

	if (input_env.op == SEC_WP_OP_SET_USER) {
		struct task_struct *task = NULL;

		if (enable != SEC_WP_DISABLED) {
			pr_info("watchpoint is already working\n");
			goto err;
		}

		if (input_env.sub_op == SEC_WP_SUBOP_TASK) {
			rcu_read_lock();
			task = find_task_by_vpid(input_env.pid);
			if (task)
				get_task_struct(task);
			rcu_read_unlock();
		} else {
			task = get_current();
		}

		if (!task) {
			pr_info("failed to find task\n");
			goto err;
		}

		user_hbp = register_user_hw_breakpoint(&attr, __default_hbp_handler,
			NULL, task);
		if (IS_ERR((void __force *)user_hbp)) {
			ret = PTR_ERR((void __force *)user_hbp);
			pr_info("failed to register user hbp\n");
			goto err;
		}

		pr_info("user task %s watchpoint addr:0x%lx, type:%d\n",
			task->comm, (unsigned long)input_env.addr, input_env.rw_mode);
		enable = SEC_WP_ENABLED_USER;
	}

	if (input_env.op == SEC_WP_OP_CLEAR) {
		if (enable == SEC_WP_DISABLED) {
			pr_info("sec watcher is not activated\n");
		} else if (enable == SEC_WP_ENABLED_KERNEL && kernel_hbp != NULL) {
			unregister_wide_hw_breakpoint(kernel_hbp);
			kernel_hbp = NULL;
			enable = SEC_WP_DISABLED;
		} else if (enable == SEC_WP_ENABLED_USER && user_hbp != NULL) {
			unregister_hw_breakpoint(user_hbp);
			user_hbp = NULL;
			enable = SEC_WP_DISABLED;
		}
	}

	goto out;
err:
	pr_info("failed to set watchpoint\n");
out:
	pr_info("end of watchpoint\n");
	if (argv)
		argv_free(argv);

	return count;
}

static const struct file_operations sec_debug_watcher_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = sec_debug_watchpoint_read,
	.write = sec_debug_watchpoint_write,
	.llseek = seq_lseek,
};

static int __init sec_debug_watcher_proc_init(void)
{
	pr_info("proc init\n");

	if (!proc_create("sec_debug_watchpoint", 0660, NULL, &sec_debug_watcher_fops)) {
		pr_info("failed to create watchpoint proc node\n");
		goto err;
	}

	pr_info("watcher initialization success\n");
	return 0;

err:
	return -1;
}

late_initcall(sec_debug_watcher_proc_init);

MODULE_DESCRIPTION("Samsung Debug Watchpoint");
MODULE_LICENSE("GPL v2");
