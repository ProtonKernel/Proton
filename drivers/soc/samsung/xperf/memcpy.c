/*
 * Exynos CPU Performance
 *   memcpy/memset: cacheable/non-cacheable
 *   Author: Jungwook Kim, jwook1.kim@samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <asm/cacheflush.h>
#include <linux/swiotlb.h>
#include <linux/types.h>

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/mod_devicetable.h>
#include <linux/init.h>

#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <linux/io.h>
#include <linux/syscalls.h>
#include <asm/io.h>
#include <asm/neon.h>

#define _PMU_COUNT_64
#include "pmufunc.h"
#include <soc/samsung/cal-if.h>
#include <soc/samsung/exynos-devfreq.h>

struct device *memcpy_dev;
static uint cal_id_mif = 0;
static uint devfreq_mif = 0;

enum memtype {
	MT_CACHE = 1,
	MT_NC,
	MT_C2NC,
	MT_NC2C,
	MT_COUNT,
};

enum functype {
	FT_MEMCPY = 1,
	FT_MEMSET,
	FT_CPYTOUSR,
	FT_CPYFRMUSR,
	FT_COUNT,
};

static char *memtype_str[] = {"", "MT_CACHE", "MT_NC", "MT_C2NC", "MT_NC2C"};
static char *functype_str[] = {"",
	"memcpy",
	"memset",
	"__copy_to_user",
	"__copy_from_user",
	"cache_flush",
};
//static char *level_str[] = { "ALL_CL", "LOUIS", "LLC" };
static uint type = MT_CACHE;
static uint start_size = 64;
static uint end_size = SZ_4M;
static uint core = 4;
static uint func = 1;
static char nc_memcpy_result_buf[PAGE_SIZE];
static const char *prefix = "xperf_ncmemcpy";
static int nc_memcpy_done = 0;

#define GiB	(1024*1024*1024LL)
#define GB	1000000000LL
#define MiB	(1024*1024)
#define NS_PER_SEC	1000000000LL
#define US_PER_SEC	1000000LL

#if 0
static u64 patterns[] = {
	/* The first entry has to be 0 to leave memtest with zeroed memory */
	0,
	0xffffffffffffffffULL,
	0x5555555555555555ULL,
	0xaaaaaaaaaaaaaaaaULL,
	0x1111111111111111ULL,
	0x2222222222222222ULL,
	0x4444444444444444ULL,
	0x8888888888888888ULL,
	0x3333333333333333ULL,
	0x6666666666666666ULL,
	0x9999999999999999ULL,
	0xccccccccccccccccULL,
	0x7777777777777777ULL,
	0xbbbbbbbbbbbbbbbbULL,
	0xddddddddddddddddULL,
	0xeeeeeeeeeeeeeeeeULL,
	0x7a6c7258554e494cULL, /* yeah ;-) */
};
static void memset_ptrn(ulong *p, uint count)
{
	uint j, idx;
	for (j = 0; j < count; j += sizeof(p)) {
		idx = j % ARRAY_SIZE(patterns);
		*p++ = patterns[idx];
	}
}
#endif

static u64 nano_time(void)
{
	struct timespec t;
	t.tv_sec = t.tv_nsec = 0;
	getnstimeofday(&t);
	return (t.tv_sec * NS_PER_SEC + t.tv_nsec);
}

#define MEMSET_FUNC(FUNC)				\
	total_size = (core < 4)? 128*MiB : GiB;		\
	iter = total_size / xfer_size;			\
	start_ns = nano_time();				\
	for (i = 0; i < iter; i++) {			\
		FUNC;					\
	}						\
	end_ns = nano_time();				\
	sum_ns += (end_ns - start_ns);			\
	if (sum_ns == 0) {			\
		pr_info("[%s] bench failed: divide_by_zero\n", prefix);	\
		continue;			\
	}			\
	bw = total_size / MiB * NS_PER_SEC / sum_ns;	\
	ret1 += snprintf(buf + ret1, PAGE_SIZE - ret1, "%-20s %5d %8s %7d %7d "		\
			"%10u %10llu %10u %15llu\n",	\
		functype_str[func], core, memtype_str[type], cpufreq, mif,		 \
		xfer_size, bw, iter, sum_ns);

#define MEMCPY_FUNC(FUNC, is_ret)	\
	total_size = (core < 4 || type == MT_NC)? 128*MiB : GiB;	\
	iter = total_size / xfer_size;			\
	start_ns = nano_time();				\
	for (k = 0; k < iter; k++) {			\
		FUNC;					\
	}						\
	end_ns = nano_time();				\
	sum_ns += (end_ns - start_ns);			\
	if (sum_ns == 0) {			\
		pr_info("[%s] bench failed: divide_by_zero\n", prefix);	\
		continue;			\
	}			\
	if (is_ret && ret > 0) {			\
		pr_info("[%s] bench failed: ret > 0\n", prefix);	\
		continue;					\
	}							\
	bw = total_size / MiB * NS_PER_SEC / sum_ns;		\
	ret1 += snprintf(buf + ret1, PAGE_SIZE - ret1, "%-20s %5d %8s %7d %7d "	\
		"%10u %10llu %15u %15llu\n",	\
		functype_str[func], core, memtype_str[type], cpufreq, mif,	\
		xfer_size, bw, iter, sum_ns);\

void func_perf(void *src, void *dst)
{
	u64 start_ns, end_ns, bw, sum_ns; //avg_us;
	uint total_size;
	uint iter;
	int i, k;
	uint xfer_size = start_size;
	size_t ret;

	static char buf[PAGE_SIZE];
	uint cpufreq;
	uint mif = 0;
	size_t ret1 = 0;

	nc_memcpy_done = 0;

	// HEADER
	if (func == FT_MEMSET) {
		ret1 += snprintf(buf + ret1, PAGE_SIZE - ret1, "%-20s %5s %8s %7s %7s %10s %10s %15s %10s\n",
			"name", "core", "type", "cpufreq", "mif", "size", "MiB/s", "iter", "total_ns");
	} else {
		ret1 += snprintf(buf + ret1, PAGE_SIZE - ret1, "%-20s %5s %8s %7s %7s %10s %10s %15s %15s\n",
			"name", "core", "type", "cpufreq", "mif", "size", "MiB/s", "iter", "total_ns");
	}

	// BODY - START
	bw = 0;
	while (xfer_size <= end_size) {
		ret = 0;
		sum_ns = 0;
		cpufreq = cpufreq_quick_get(core) / 1000;
#if defined(CONFIG_SOC_EXYNOS9830) || defined(CONFIG_SOC_EXYNOS991)
		mif = (uint)exynos_devfreq_get_domain_freq(devfreq_mif) / 1000;
#else
		mif = (uint)cal_dfs_cached_get_rate(cal_id_mif) / 1000;
#endif
		switch (func) {
			case FT_MEMCPY:
				MEMCPY_FUNC(memcpy(dst, src, xfer_size), 0);
				break;
			case FT_MEMSET:
				MEMSET_FUNC(memset(dst, 0xff, xfer_size));
				break;
			case FT_CPYTOUSR:
				MEMCPY_FUNC(ret += __copy_to_user(dst, src, xfer_size), 1);
				break;
			case FT_CPYFRMUSR:
				MEMCPY_FUNC(ret += __copy_from_user(dst, src, xfer_size), 1);
				break;
			default:
				pr_info("[%s] func type = %d invalid!\n", prefix, func);
				break;
		}

		xfer_size *= 2;
	}
	// BODY - END

	memcpy(nc_memcpy_result_buf, buf, PAGE_SIZE);

	nc_memcpy_done = 1;
}

static int perf_main(void)
{
	struct device *dev = memcpy_dev;
	void *src_cpu, *dst_cpu;
	dma_addr_t src_dma, dst_dma;
	gfp_t gfp;
	uint buf_size;
	u64 start_ns, end_ns;
	start_ns = nano_time();

	/* dma address */
	if (!dma_set_mask(dev, DMA_BIT_MASK(64))) {
		pr_info("[%s] dma_mask: 64-bits ok\n", prefix);
	} else if (!dma_set_mask(dev, DMA_BIT_MASK(32))) {
		pr_info("[%s] dma_mask: 32-bits ok\n", prefix);
	}

	//buf_size = (align == 0)? end_size : end_size + 64;
	buf_size = end_size;
	gfp = GFP_KERNEL;

	if (type == MT_CACHE) {
		gfp = GFP_KERNEL;
		src_cpu = kmalloc(buf_size, gfp);
		if (src_cpu == NULL) {
			pr_info("[%s] kmalloc failed: src\n", prefix);
			return 0;
		}
		dst_cpu = kmalloc(buf_size, gfp);
		if (dst_cpu == NULL) {
			pr_info("[%s] kmalloc failed: dst\n", prefix);
			kfree(src_cpu);
			return 0;
		}
		func_perf(src_cpu, dst_cpu);
		kfree(dst_cpu);
		kfree(src_cpu);

	} else if (type == MT_NC) {
		src_cpu = dma_alloc_coherent(dev, buf_size, &src_dma, gfp);
		if (src_cpu == NULL) {
			pr_info("[%s] dma_alloc_coherent failed: src: buf_size=%d\n", prefix, buf_size);
			return 0;
		}
		dst_cpu = dma_alloc_coherent(dev, buf_size, &dst_dma, gfp);
		if (dst_cpu == NULL) {
			pr_info("[%s] dma_alloc_coherent failed: dst: buf_size=%d\n", prefix, buf_size);
			dma_free_coherent(dev, buf_size, src_cpu, src_dma);
			return 0;
		}
		func_perf(src_cpu, dst_cpu);
		dma_free_coherent(dev, buf_size, dst_cpu, dst_dma);
		dma_free_coherent(dev, buf_size, src_cpu, src_dma);
	} else if (type == MT_C2NC) {
		/* cacheable --> non-cacheable */
		pr_info("[%s] cacheable --> non-cacheable\n", prefix);
		src_cpu = kmalloc(buf_size, gfp);
		if (src_cpu == NULL) {
			pr_info("[%s] kmalloc failed: src\n", prefix);
			return 0;
		}
		dst_cpu = dma_alloc_coherent(dev, buf_size, &dst_dma, gfp);
		if (dst_cpu == NULL) {
			pr_info("[%s] dma_alloc_coherent failed: dst\n", prefix);
			kfree(src_cpu);
			return 0;
		}
		func_perf(src_cpu, dst_cpu);
		dma_free_coherent(dev, buf_size, dst_cpu, dst_dma);
		kfree(src_cpu);
	} else if (type == MT_NC2C) {
		/* non-cacheable --> cacheable */
		pr_info("[%s] non-cacheable --> cacheable\n", prefix);
		src_cpu = dma_alloc_coherent(dev, buf_size, &src_dma, gfp);
		if (src_cpu == NULL) {
			pr_info("[%s] dma_alloc_coherent failed: src\n", prefix);
			return 0;
		}
		dst_cpu = kmalloc(buf_size, gfp);
		if (dst_cpu == NULL) {
			pr_info("[%s] kmalloc failed: dst\n", prefix);
			dma_free_coherent(dev, buf_size, src_cpu, src_dma);
			return 0;
		}
		func_perf(src_cpu, dst_cpu);
		dma_free_coherent(dev, buf_size, src_cpu, src_dma);
		kfree(dst_cpu);
	} else {
		pr_info("[%s] type = %d invalid!\n", prefix, type);
	}

	end_ns = nano_time();
	pr_info("[%s] Total elapsed time = %llu seconds\n", prefix, (end_ns - start_ns) / NS_PER_SEC);

	return 0;
}

/* thread main function */
static bool thread_running;
static int thread_func(void *data)
{
	thread_running = 1;
	perf_main();
	thread_running = 0;
	return 0;
}

static struct task_struct *task;
static int test_thread_init(void)
{
	task = kthread_create(thread_func, NULL, "thread%u", 0);
	kthread_bind(task, core);
	wake_up_process(task);
	return 0;
}

static void test_thread_exit(void)
{
	printk(KERN_ERR "[%s] Exit module: thread_running: %d\n", prefix, thread_running);
	if (thread_running)
		kthread_stop(task);
}


/*-------------------------------------------------------------------------------
 * sysfs starts here
 *-------------------------------------------------------------------------------*/
#define DEF_NODE(name) \
	static ssize_t show_##name(struct kobject *k, struct kobj_attribute *attr, char *buf) { \
		int ret = 0; \
		ret += sprintf(buf + ret, "%d\n", name); \
		return ret; } \
	static ssize_t store_##name(struct kobject *k, struct kobj_attribute *attr, const char *buf, size_t count) { \
		if (sscanf(buf, "%d", &name) != 1) \
			return -EINVAL; \
		return count; } \
	static struct kobj_attribute name##_attr = __ATTR(name, 0644, show_##name, store_##name);
DEF_NODE(start_size)
DEF_NODE(end_size)
DEF_NODE(func)
DEF_NODE(type)
DEF_NODE(core)

// run
static ssize_t show_run(struct kobject *k, struct kobj_attribute *attr, char *b)
{
	int ret = 0;
	if (nc_memcpy_done == 0) {
		ret += sprintf(b + ret, "%s\n", "NO RESULT");
	} else {
		ret += sprintf(b + ret, "%s\n", nc_memcpy_result_buf);
	}
	return ret;
}
static ssize_t store_run(struct kobject *k, struct kobj_attribute *attr, const char *b, size_t count)
{
	int run = 0;
	if (sscanf(b, "%d", &run) != 1)
		return -EINVAL;
	if (run)
		test_thread_init();
	else
		test_thread_exit();
	return count;
}
static struct kobj_attribute run_attr = __ATTR(run, 0644, show_run, store_run);

static struct kobject *nc_kobj;
static struct attribute *nc_attrs[] = {
	&start_size_attr.attr,
	&end_size_attr.attr,
	&func_attr.attr,
	&type_attr.attr,
	&core_attr.attr,
	&run_attr.attr,
	NULL
};
static struct attribute_group nc_group = {
	.attrs = nc_attrs,
};
/*------------------------------------------------------------------*/

//-------------------
// main
//-------------------
int xperf_memcpy_init(struct platform_device *pdev, struct kobject *kobj)
{
	struct device_node *dn = pdev->dev.of_node;
	int ret;

	memcpy_dev = &pdev->dev;

	of_property_read_u32(dn, "cal-id-mif", &cal_id_mif);
	of_property_read_u32(dn, "devfreq-mif", &devfreq_mif);

	nc_kobj = kobject_create_and_add("memcpy", kobj);
	if (!nc_kobj) {
		pr_info("[%s] create node failed: %s\n", prefix, __FILE__);
		return -EINVAL;
	}

	ret = sysfs_create_group(nc_kobj, &nc_group);
	if (ret) {
		pr_info("[%s] create group failed: %s\n", prefix, __FILE__);
		return -EINVAL;
	}

	return 0;
}
