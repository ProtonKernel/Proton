/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/ctype.h>
#include <linux/nmi.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <asm-generic/io.h>

#include <soc/samsung/debug-snapshot.h>
#include <soc/samsung/exynos-pmu-if.h>
#include <soc/samsung/acpm_ipc_ctrl.h>

#include "debug-snapshot-local.h"

#define	REBOOT_REASON_WTSR		(1 << 0)
#define	REBOOT_REASON_SMPL		(1 << 1)
#define	REBOOT_REASON_WDT		(1 << 2)
#define REBOOT_REASON_PANIC		(1 << 3)
#define REBOOT_REASON_NA		(0)

typedef void (*force_error_func)(char *arg);

static void simulate_KP(char *arg);
static void simulate_DP(char *arg);
static void simulate_QDP(char *arg);
static void simulate_SVC(char *arg);
static void simulate_SFR(char *arg);
static void simulate_WP(char *arg);
static void simulate_TP(char *arg);
static void simulate_PANIC(char *arg);
static void simulate_BUG(char *arg);
static void simulate_WARN(char *arg);
static void simulate_DABRT(char *arg);
static void simulate_PABRT(char *arg);
static void simulate_UNDEF(char *arg);
static void simulate_DFREE(char *arg);
static void simulate_DREF(char *arg);
static void simulate_MCRPT(char *arg);
static void simulate_LOMEM(char *arg);
static void simulate_HARD_LOCKUP(char *arg);
static void simulate_SPIN_LOCKUP(char *arg);
static void simulate_PC_ABORT(char *arg);
static void simulate_SP_ABORT(char *arg);
static void simulate_JUMP_ZERO(char *arg);
static void simulate_BUSMON_ERROR(char *arg);
static void simulate_UNALIGNED(char *arg);
static void simulate_WRITE_RO(char *arg);
static void simulate_OVERFLOW(char *arg);
static void simulate_CACHE_FLUSH(char *arg);
static void simulate_APM_WDT(char *arg);

static int exynos_debug_test_parsing_dt(struct device_node *np);

struct debug_test_desc {
	int enabled;
	int nr_cpu;
	int nr_little_cpu;
	int nr_mid_cpu;
	int nr_big_cpu;
	int little_cpu_start;
	int mid_cpu_start;
	int big_cpu_start;
	unsigned int ps_hold_control_offset;
	unsigned int *null_pointer_ui;
	int *null_pointer_si;
	void (*null_function)(void);
	struct dentry *debugfs_root;
	struct device *dev;
	spinlock_t debug_test_lock;
};

struct force_error_item {
	char errname[SZ_32];
	force_error_func errfunc;
};

/* For printing test information */
struct force_error_test_item {
	char arg[SZ_128];
	int auto_reset;
	int reason;
};

static struct debug_test_desc exynos_debug_desc = { 0, };

static struct force_error_item force_error_vector[] = {
	{"KP",		&simulate_KP},
	{"DP",		&simulate_DP},
	{"QDP",		&simulate_QDP},
	{"SVC",		&simulate_SVC},
	{"SFR",		&simulate_SFR},
	{"WP",		&simulate_WP},
	{"TP",		&simulate_TP},
	{"panic",	&simulate_PANIC},
	{"bug",		&simulate_BUG},
	{"warn",	&simulate_WARN},
	{"dabrt",	&simulate_DABRT},
	{"pabrt",	&simulate_PABRT},
	{"undef",	&simulate_UNDEF},
	{"dfree",	&simulate_DFREE},
	{"danglingref",	&simulate_DREF},
	{"memcorrupt",	&simulate_MCRPT},
	{"lowmem",	&simulate_LOMEM},
	{"hardlockup",	&simulate_HARD_LOCKUP},
	{"spinlockup",	&simulate_SPIN_LOCKUP},
	{"pcabort",	&simulate_PC_ABORT},
	{"spabort",	&simulate_SP_ABORT},
	{"jumpzero",	&simulate_JUMP_ZERO},
	{"busmon",	&simulate_BUSMON_ERROR},
	{"unaligned",	&simulate_UNALIGNED},
	{"writero",	&simulate_WRITE_RO},
	{"overflow",	&simulate_OVERFLOW},
	{"cacheflush",	&simulate_CACHE_FLUSH},
	{"apmwdt",	&simulate_APM_WDT},
};

static struct force_error_test_item test_vector[] = {
	{"KP",				1, REBOOT_REASON_PANIC},
	{"SVC",				1, REBOOT_REASON_PANIC},
	{"SFR 0x1ffffff0",		1, REBOOT_REASON_PANIC | REBOOT_REASON_WDT},
	{"SFR 0x1ffffff0 0x12345678",	1, REBOOT_REASON_PANIC | REBOOT_REASON_WDT},
	{"WP",				1, REBOOT_REASON_WTSR},
	{"panic",			1, REBOOT_REASON_PANIC},
	{"bug",				1, REBOOT_REASON_PANIC},
	{"dabrt",			1, REBOOT_REASON_PANIC},
	{"pabrt",			1, REBOOT_REASON_PANIC},
	{"undef",			1, REBOOT_REASON_PANIC},
	{"memcorrupt",			1, REBOOT_REASON_PANIC},
	{"hardlockup 0",		1, REBOOT_REASON_WDT},
	{"hardlockup LITTLE",		1, REBOOT_REASON_WDT},
	{"hardlockup BIG",		1, REBOOT_REASON_WDT},
	{"spinlockup",			1, REBOOT_REASON_PANIC},
	{"pcabort",			1, REBOOT_REASON_PANIC},
	{"jumpzero",			1, REBOOT_REASON_PANIC},
	{"writero",			1, REBOOT_REASON_PANIC},
	{"danglingref",			0, REBOOT_REASON_PANIC},
	{"dfree",			0, REBOOT_REASON_PANIC},
	{"QDP",				0, REBOOT_REASON_WDT},
	{"spabort",			0, REBOOT_REASON_NA},
	{"overflow",			0, REBOOT_REASON_NA},
	{"cacheflush",			1, REBOOT_REASON_WDT},
	{"apmwdt",			0, REBOOT_REASON_WDT},
};

static int debug_force_error(const char *val)
{
	int i;
	char *temp;
	char *ptr;

	for (i = 0; i < (int)ARRAY_SIZE(force_error_vector); i++) {
		if (!strncmp(val, force_error_vector[i].errname,
				strlen(force_error_vector[i].errname))) {
			temp = (char *)val;
			ptr = strsep(&temp, " ");	/* ignore the first token */
			ptr = strsep(&temp, " ");	/* take the second token */
			force_error_vector[i].errfunc(ptr);
			break;
		}
	}

	if (i == (int)ARRAY_SIZE(force_error_vector))
		dev_info(exynos_debug_desc.dev,"%s(): INVALID TEST "
					"CMD = [%s]\n", __func__, val);

	return 0;
}

/* timeout for dog bark/bite */
#define DELAY_TIME 30000

static void pull_down_other_cpus(void)
{
#if IS_ENABLED(CONFIG_HOTPLUG_CPU)
	int cpu, ret;

	for (cpu = exynos_debug_desc.nr_cpu - 1; cpu > 0 ; cpu--) {
		ret = cpu_down(cpu);
		if (ret)
			dev_crit(exynos_debug_desc.dev, "%s() CORE%d ret: %x\n",
							__func__, cpu, ret);
	}
#endif
}

static int find_blank(char *arg)
{
	int i;

	/* if parameter is not one, a space between parameters is 0
	 * End of parameter is lf(10)
	 */
	for (i = 0; !isspace(arg[i]) && arg[i]; i++)
		continue;

	return i;
}

static void simulate_KP(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	*exynos_debug_desc.null_pointer_ui = 0x0; /* SVACE: intended */
}

static void simulate_DP(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	pull_down_other_cpus();
	dev_crit(exynos_debug_desc.dev, "%s() start to hanging\n", __func__);
	local_irq_disable();
	mdelay(DELAY_TIME);
	local_irq_enable();
	/* should not reach here */
}

static void simulate_QDP(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	dbg_snapshot_expire_watchdog();
	mdelay(DELAY_TIME);
	/* should not reach here */
}

static void simulate_SVC(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	asm("svc #0x0");
	/* should not reach here */
}

static void simulate_SFR(char *arg)
{
	int ret, index = 0;
	unsigned long reg, val;
	char *tmp, *tmparg;
	void __iomem *addr;

	dev_crit(exynos_debug_desc.dev, "%s() start\n", __func__);

	index = find_blank(arg);
	if (index > PAGE_SIZE)
		return;

	tmp = kmalloc(PAGE_SIZE, GFP_KERNEL);
	memcpy(tmp, arg, index);
	tmp[index] = '\0';

	ret = kstrtoul(tmp, 16, &reg);
	addr = ioremap(reg, 0x10);
	if (!addr) {
		dev_crit(exynos_debug_desc.dev, "%s() failed to remap 0x%lx, quit\n", __func__, reg);
		kfree(tmp);
		return;
	}
	dev_crit(exynos_debug_desc.dev, "%s() 1st parameter: 0x%lx\n", __func__, reg);

	tmparg = &arg[index + 1];
	index = find_blank(tmparg);
	if (index == 0) {
		dev_crit(exynos_debug_desc.dev, "%s() there is no 2nd parameter\n", __func__);
		dev_crit(exynos_debug_desc.dev, "%s() try to read 0x%lx\n", __func__, reg);

		ret = __raw_readl(addr);
		dev_crit(exynos_debug_desc.dev, "%s() result : 0x%x\n", __func__, ret);

	} else {
		if (index > PAGE_SIZE) {
			kfree(tmp);
			return;
		}
		memcpy(tmp, tmparg, index);
		tmp[index] = '\0';
		ret = kstrtoul(tmp, 16, &val);
		dev_crit(exynos_debug_desc.dev, "%s() 2nd parameter: 0x%lx\n", __func__, val);
		dev_crit(exynos_debug_desc.dev, "%s() try to write 0x%lx to 0x%lx\n", __func__, val, reg);

		__raw_writel(val, addr);
	}
	kfree(tmp);
	/* should not reach here */
}

static void simulate_WP(char *arg)
{
	unsigned int ps_hold_control;

	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	exynos_pmu_read(exynos_debug_desc.ps_hold_control_offset, &ps_hold_control);
	exynos_pmu_write(exynos_debug_desc.ps_hold_control_offset, ps_hold_control & 0xFFFFFEFF);
}

static void simulate_TP(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);
}

static void simulate_PANIC(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	panic("simulate_panic");
}

static void simulate_BUG(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	BUG();
}

static void simulate_WARN(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	WARN_ON(1);
}

static void simulate_DABRT(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	*exynos_debug_desc.null_pointer_si = 0; /* SVACE: intended */
}

static void simulate_PABRT(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s() call address=[%llx]\n", __func__,
			(unsigned long long)exynos_debug_desc.null_function);

	exynos_debug_desc.null_function(); /* SVACE: intended */
}

static void simulate_UNDEF(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	asm volatile(".word 0xe7f001f2\n\t");
	unreachable();
}

static void simulate_DFREE(char *arg)
{
	void *p;

	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	p = kmalloc(sizeof(unsigned int), GFP_KERNEL);
	if (p) {
		*(unsigned int *)p = 0x0;
		kfree(p);
		msleep(1000);
		kfree(p); /* SVACE: intended */
	}
}

static void simulate_DREF(char *arg)
{
	unsigned int *p;

	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	p = kmalloc(sizeof(int), GFP_KERNEL);
	if (p) {
		kfree(p);
		*p = 0x1234; /* SVACE: intended */
	}
}

static void simulate_MCRPT(char *arg)
{
	int *ptr;

	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	ptr = kmalloc(sizeof(int), GFP_KERNEL);
	if (ptr) {
		*ptr++ = 4;
		*ptr = 2;
		panic("MEMORY CORRUPTION");
	}
}

static void simulate_LOMEM(char *arg)
{
	int i = 0;

	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	dev_crit(exynos_debug_desc.dev, "Allocating memory until failure!\n");
	while (kmalloc(128 * 1024, GFP_KERNEL)) /* SVACE: intended */
		i++;
	dev_crit(exynos_debug_desc.dev, "Allocated %d KB!\n", i * 128);
}

static void simulate_HARD_LOCKUP_handler(void *info)
{
	asm("b .");
}

static void simulate_HARD_LOCKUP(char *arg)
{
	int cpu;
	int start = -1;
	int end;
	int curr_cpu;
	long temp;

	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	if (arg) {
		if (!strncmp(arg, "LITTLE", strlen("LITTLE"))) {
			if (exynos_debug_desc.little_cpu_start < 0 ||
					exynos_debug_desc.nr_little_cpu < 0) {
				dev_info(exynos_debug_desc.dev, "%s() no little cpu info\n", __func__);
				return;
			}
			start = exynos_debug_desc.little_cpu_start;
			end = start + exynos_debug_desc.nr_little_cpu - 1;
		} else if (!strncmp(arg, "MID", strlen("MID"))) {
			if (exynos_debug_desc.mid_cpu_start < 0 ||
						exynos_debug_desc.nr_mid_cpu < 0) {
				dev_info(exynos_debug_desc.dev, "%s() no mid cpu info\n", __func__);
				return;
			}
			start = exynos_debug_desc.mid_cpu_start;
			end = start + exynos_debug_desc.nr_mid_cpu - 1;
		} else if (!strncmp(arg, "BIG", strlen("BIG"))) {
			if (exynos_debug_desc.big_cpu_start < 0 ||
						exynos_debug_desc.nr_big_cpu < 0) {
				dev_info(exynos_debug_desc.dev, "%s() no big cpu info\n", __func__);
				return;
			}
			start = exynos_debug_desc.big_cpu_start;
			end = start + exynos_debug_desc.nr_big_cpu - 1;
		}

		if (start >= 0) {
			preempt_disable();
			curr_cpu = raw_smp_processor_id();
			for (cpu = start; cpu <= end; cpu++) {
				if (cpu == curr_cpu)
					continue;
				smp_call_function_single(cpu,
						simulate_HARD_LOCKUP_handler, 0, 0);
			}
			if (curr_cpu >= start && curr_cpu <= end) {
				local_irq_disable();
				asm("b .");
			}
			preempt_enable();
			return;
		}

		if (kstrtol(arg, 10, &temp)) {
			dev_err(exynos_debug_desc.dev, "%s() input is invalid\n", __func__);
			return;
		}

		cpu = (int)temp;
		if (cpu < 0 || cpu >= exynos_debug_desc.nr_cpu) {
			dev_info(exynos_debug_desc.dev, "%s() input is invalid\n", __func__);
			return;
		}
		smp_call_function_single(cpu, simulate_HARD_LOCKUP_handler, 0, 0);
	} else {
		start = 0;
		end = exynos_debug_desc.nr_cpu;

		local_irq_disable();
		curr_cpu = raw_smp_processor_id();

		for (cpu = start; cpu < end; cpu++) {
			if (cpu == curr_cpu)
				continue;
			smp_call_function_single(cpu, simulate_HARD_LOCKUP_handler, 0, 0);
		}
		asm("b .");
	}
}

static void simulate_SPIN_LOCKUP(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	spin_lock(&exynos_debug_desc.debug_test_lock);
	spin_lock(&exynos_debug_desc.debug_test_lock);
}

static void simulate_PC_ABORT(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	asm("add x30, x30, #0x1\n\t"
	    "ret");
}

static void simulate_SP_ABORT(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	asm("mov x29, #0xff00\n\t"
	    "mov sp, #0xff00\n\t"
	    "ret");
}

static void simulate_JUMP_ZERO(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	asm("mov x0, #0x0\n\t"
	    "br x0");
}

static void simulate_BUSMON_ERROR(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);
}

static void simulate_UNALIGNED(char *arg)
{
	static u8 data[5] __aligned(4) = {1, 2, 3, 4, 5};
	u32 *p;
	u32 val = 0x12345678;

	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	p = (u32 *)(data + 1);
	dev_crit(exynos_debug_desc.dev, "data=[0x%llx] p=[0x%llx]\n",
				(unsigned long long)data, (unsigned long long)p);
	if (*p == 0)
		val = 0x87654321;
	*p = val;
	dev_crit(exynos_debug_desc.dev, "val = [0x%x] *p = [0x%x]\n", val, *p);
}

static void simulate_WRITE_RO(char *arg)
{
	unsigned long *ptr;

	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	ptr = (unsigned long *)simulate_WRITE_RO;
	*ptr ^= 0x12345678;
}

#define BUFFER_SIZE SZ_1K

static int recursive_loop(int remaining)
{
	char buf[BUFFER_SIZE];

	dev_crit(exynos_debug_desc.dev, "%s() remainig = [%d]\n", __func__, remaining);

	/* Make sure compiler does not optimize this away. */
	memset(buf, (remaining & 0xff) | 0x1, BUFFER_SIZE);
	if (!remaining)
		return 0;
	else
		return recursive_loop(remaining - 1);
}

static void simulate_OVERFLOW(char *arg)
{
	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	recursive_loop(600);
}

static char *buffer[NR_CPUS];
static void simulate_CACHE_FLUSH_handler(void *info)
{
	int cpu = raw_smp_processor_id();
	u64 addr = virt_to_phys((void *)(buffer[cpu]));

	memset(buffer[cpu], 0x5A, PAGE_SIZE * 2);
	dbg_snapshot_set_debug_test_buffer_addr(addr, cpu);
	asm("b .");
}

static void simulate_CACHE_FLUSH_ALL(void *info)
{
	cache_flush_all();
}

static void simulate_CACHE_FLUSH(char *arg)
{
	u64 addr;
	int cpu;

	dev_crit(exynos_debug_desc.dev, "%s()\n", __func__);

	for_each_possible_cpu(cpu) {
		dbg_snapshot_set_debug_test_buffer_addr(0, cpu);
		buffer[cpu] = kmalloc(PAGE_SIZE * 2, GFP_KERNEL);
		memset(buffer[cpu], 0x3B, PAGE_SIZE * 2);
	}

	smp_call_function(simulate_CACHE_FLUSH_ALL, NULL, 1);
	cache_flush_all();

	smp_call_function(simulate_CACHE_FLUSH_handler, NULL, 0);
	for_each_possible_cpu(cpu) {
		if (cpu == raw_smp_processor_id())
			continue;

		while (!dbg_snapshot_get_debug_test_buffer_addr(cpu));
		dev_crit(exynos_debug_desc.dev, "CPU %d STOPPING\n", cpu);
	}

	cpu = raw_smp_processor_id();
	addr = virt_to_phys((void *)(buffer[cpu]));
	memset(buffer[cpu], 0x5A, PAGE_SIZE * 2);
	dbg_snapshot_set_debug_test_buffer_addr(addr, cpu);
	dbg_snapshot_expire_watchdog();
}

static void simulate_APM_WDT(char *arg)
{
	exynos_acpm_force_apm_wdt_reset();
	asm volatile("b .");
}

static ssize_t exynos_debug_test_write(struct file *file,
					const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	char *buf;
	size_t buf_size;
	int i;

	buf_size = ((count + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
	if (buf_size <= 0)
		return 0;

	buf = kmalloc(buf_size, GFP_KERNEL);
	if (!buf)
		return 0;

	if (copy_from_user(buf, user_buf, count)) {
		kfree(buf);
		return -EFAULT;
	}
	buf[count] = '\0';

	for (i = 0; buf[i] != '\0'; i++)
		if (buf[i] == '\n') {
			buf[i] = '\0';
			break;
		}

	dev_info(exynos_debug_desc.dev, "%s() user_buf=[%s]\n", __func__, buf);
	debug_force_error(buf);

	kfree(buf);
	return count;
}

static ssize_t exynos_debug_test_read(struct file *file,
				char __user *user_buf, size_t count,
				loff_t *ppos)
{
	char *buf;
	size_t copy_cnt;
	int i;
	int ret = 0;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return ret;

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"=============================="
			" DEBUG TEST EXAMPLES "
			"==============================\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"%-30s%-15s%s\n", "INPUT_ARG(S)",
			"AUTO_RESET", "RESET_RESON");

	for (i = 0; i < (int)ARRAY_SIZE(test_vector); i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"%-30s", test_vector[i].arg);

		if (test_vector[i].auto_reset)
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
							"%-15s", "yes");
		else
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
							"%-15s", "no");

		if (!test_vector[i].reason)
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "N/A");

		if (test_vector[i].reason & REBOOT_REASON_WTSR)
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "WTSR ");

		if (test_vector[i].reason & REBOOT_REASON_SMPL)
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "SMPL ");

		if (test_vector[i].reason & REBOOT_REASON_WDT)
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "WDT ");

		if (test_vector[i].reason & REBOOT_REASON_PANIC)
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "PANIC ");

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
	}

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"========================================="
			"========================================\n");

	copy_cnt = ret;
	ret = simple_read_from_buffer(user_buf, count, ppos, buf, copy_cnt);

	kfree(buf);
	return ret;
}
static const struct file_operations exynos_debug_test_file_fops = {
	.open	= simple_open,
	.read	= exynos_debug_test_read,
	.write	= exynos_debug_test_write,
	.llseek	= default_llseek,
};

static int exynos_debug_test_parsing_dt(struct device_node *np)
{
	int ret = 0;

	/* get data from device tree */
	ret = of_property_read_u32(np, "ps_hold_control_offset",
					&exynos_debug_desc.ps_hold_control_offset);
	if (ret) {
		dev_err(exynos_debug_desc.dev, "no data(ps_hold_control offset)\n");
		goto edt_desc_init_out;
	}

	ret = of_property_read_u32(np, "nr_cpu", &exynos_debug_desc.nr_cpu);
	if (ret) {
		dev_err(exynos_debug_desc.dev, "no data(nr_cpu)\n");
		goto edt_desc_init_out;
	}

	ret = of_property_read_u32(np, "little_cpu_start",
				&exynos_debug_desc.little_cpu_start);
	if (ret) {
		dev_info(exynos_debug_desc.dev, "no data(little_cpu_start)\n");
		exynos_debug_desc.little_cpu_start = -1;
	}

	ret = of_property_read_u32(np, "nr_little_cpu",	&exynos_debug_desc.nr_little_cpu);
	if (ret) {
		dev_info(exynos_debug_desc.dev, "no data(nr_little_cpu)\n");
		exynos_debug_desc.nr_little_cpu = -1;
	}

	ret = of_property_read_u32(np, "mid_cpu_start",	&exynos_debug_desc.mid_cpu_start);
	if (ret) {
		dev_info(exynos_debug_desc.dev, "no data(mid_cpu_start)\n");
		exynos_debug_desc.mid_cpu_start = -1;
	}

	ret = of_property_read_u32(np, "nr_mid_cpu", &exynos_debug_desc.nr_mid_cpu);
	if (ret) {
		dev_info(exynos_debug_desc.dev, "no data(nr_mid_cpu)\n");
		exynos_debug_desc.nr_mid_cpu = -1;
	}

	ret = of_property_read_u32(np, "big_cpu_start", &exynos_debug_desc.big_cpu_start);
	if (ret) {
		dev_info(exynos_debug_desc.dev, "no data(big_cpu_start)\n");
		exynos_debug_desc.big_cpu_start = -1;
	}

	ret = of_property_read_u32(np, "nr_big_cpu", &exynos_debug_desc.nr_big_cpu);
	if (ret) {
		dev_info(exynos_debug_desc.dev, "no data(nr_big_cpu)\n");
		exynos_debug_desc.nr_big_cpu = -1;
	}

	exynos_debug_desc.null_function = (void (*)(void))0x1234;
	spin_lock_init(&exynos_debug_desc.debug_test_lock);

	/* create debugfs test file */
	debugfs_create_file("test", 0644,
			exynos_debug_desc.debugfs_root,
			NULL, &exynos_debug_test_file_fops);
	ret = 0;
	exynos_debug_desc.enabled = 1;

edt_desc_init_out:
	return ret;
}

static int exynos_debug_test_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;

	dev_info(&pdev->dev, "%s() called\n", __func__);

	exynos_debug_desc.debugfs_root =
			debugfs_create_dir("exynos-debug-test", NULL);
	if (!exynos_debug_desc.debugfs_root) {
		dev_err(&pdev->dev, "cannot create debugfs dir\n");
		ret = -ENOMEM;
		goto edt_out;
	}

	exynos_debug_desc.dev = &pdev->dev;
	ret = exynos_debug_test_parsing_dt(np);
	if (ret)
		goto edt_out;

edt_out:
	dev_info(exynos_debug_desc.dev, "%s() ret=[0x%x]\n", __func__, ret);
	return ret;
}

static const struct of_device_id exynos_debug_test_matches[] = {
	{.compatible = "samsung,exynos-debug-test"},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_debug_test_matches);

static struct platform_driver exynos_debug_test_driver = {
	.probe		= exynos_debug_test_probe,
	.driver		= {
		.name	= "exynos-debug-test",
		.of_match_table	= of_match_ptr(exynos_debug_test_matches),
	},
};
module_platform_driver(exynos_debug_test_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Exynos Debug Test");
