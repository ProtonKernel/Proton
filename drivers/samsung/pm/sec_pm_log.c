/*
 * sec_pm_log.c
 *
 *  Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *  Minsung Kim <ms925.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched/clock.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/debug-snapshot-log.h>

#define QOS_FUNC_NAME_LEN	32

struct sec_pm_log_info {
	struct device		*dev;
	struct notifier_block	exynos_pm_qos_nb[EXYNOS_PM_QOS_NUM_CLASSES];
};

struct __pm_qos_log {
	unsigned long long time;
	int pm_qos_class;
	unsigned long value;
	char func[QOS_FUNC_NAME_LEN];
	unsigned int line;
};

struct sec_pm_log {
	struct __pm_qos_log exynos_pm_qos[SZ_4K];
};

struct sec_pm_log_idx {
	atomic_t exynos_pm_qos_idx;
};

static unsigned long curr_exynos_pm_qos_idx;

#define QOS_NUM_OF_EXCLUSIVE_CLASSES	2

static char pm_qos_exclusive_class[EXYNOS_PM_QOS_NUM_CLASSES] = {
	[PM_QOS_DEVICE_THROUGHPUT] = 1,
	[PM_QOS_DISPLAY_THROUGHPUT] = 1,
	[PM_QOS_NETWORK_THROUGHPUT] = 1,
};

struct proc_dir_entry *sec_pm_log_parent;

static DEFINE_SPINLOCK(exynos_pm_qos_lock);

static struct sec_pm_log *pm_log;
static struct sec_pm_log_idx pm_log_idx;


static int sec_pm_exynos_pm_qos_notifier(struct notifier_block *nb,
					unsigned long val, void *v)
{
	struct exynos_pm_qos_request *req;
	unsigned long flags;
	unsigned long i;

	if (!v) {
		pr_err("%s: v is NULL!\n", __func__);
		return NOTIFY_OK;
	}

	req = container_of(v, struct exynos_pm_qos_request, exynos_pm_qos_class);

	i = atomic_inc_return(&pm_log_idx.exynos_pm_qos_idx) &
		(ARRAY_SIZE(pm_log->exynos_pm_qos) - 1);

	spin_lock_irqsave(&exynos_pm_qos_lock, flags);
	pm_log->exynos_pm_qos[i].time = local_clock();
	pm_log->exynos_pm_qos[i].pm_qos_class = req->exynos_pm_qos_class;
	pm_log->exynos_pm_qos[i].value = val;
	strncpy(pm_log->exynos_pm_qos[i].func, req->func, QOS_FUNC_NAME_LEN - 1);
	pm_log->exynos_pm_qos[i].line = req->line;
	spin_unlock_irqrestore(&exynos_pm_qos_lock, flags);

	return NOTIFY_OK;
}

static void sec_pm_log_register_notifier(struct sec_pm_log_info *info)
{
	int ret, i;

	dev_info(info->dev, "%s\n", __func__);


	for (i = PM_QOS_DEVICE_THROUGHPUT; i < EXYNOS_PM_QOS_NUM_CLASSES; i++) {
		if (pm_qos_exclusive_class[i])
			continue;

		info->exynos_pm_qos_nb[i].notifier_call = sec_pm_exynos_pm_qos_notifier;
		ret = exynos_pm_qos_add_notifier(i, &info->exynos_pm_qos_nb[i]);
		if (ret < 0)
			dev_err(info->dev, "%s: fail to add notifier(%d)\n",
					__func__, i);
	}
}

static void sec_pm_log_unregister_notifier(struct sec_pm_log_info *info)
{
	int ret, i;

	dev_info(info->dev, "%s\n", __func__);

	for (i = PM_QOS_DEVICE_THROUGHPUT; i < EXYNOS_PM_QOS_NUM_CLASSES; i++) {
		if (pm_qos_exclusive_class[i])
			continue;

		ret = exynos_pm_qos_remove_notifier(i, &info->exynos_pm_qos_nb[i]);
		if (ret < 0)
			dev_err(info->dev, "%s: fail to remove notifier(%d)\n",
					__func__, i);
	}
}

static int exynos_pm_qos_seq_show(struct seq_file *m, void *v)
{
	const char pm_qos_class_name[EXYNOS_PM_QOS_NUM_CLASSES][24] = {
		"NULL", "NETWORK_LATENCY",
		"CLUSTER0_FREQ_MIN", "CLUSTER0_FREQ_MAX",
		"CLUSTER1_FREQ_MIN", "CLUSTER1_FREQ_MAX",
		"CLUSTER2_FREQ_MIN", "CLUSTER2_FREQ_MAX",
		"CPU_ONLINE_MIN", "CPU_ONLINE_MAX",
		"DEVICE_THROUGHPUT", "INTCAM_THROUGHPUT",
		"DEVICE_THROUGHPUT_MAX", "INTCAM_THROUGHPUT_MAX",
		"BUS_THROUGHPUT", "BUS_THROUGHPUT_MAX",
		"DISPLAY_THROUGHPUT", "DISPLAY_THROUGHPUT_MAX",
		"CAM_THROUGHPUT", "AUD_THROUGHPUT",
		"CAM_THROUGHPUT_MAX", "AUD_THROUGHPUT_MAX",
		"MFC_THROUGHPUT", "NPU_THROUGHPUT",
		"MFC_THROUGHPUT_MAX", "NPU_THROUGHPUT_MAX",
		"GPU_THROUGHPUT_MIN", "GPU_THROUGHPUT_MAX",
		"VCP_THROUGHPUT", "VCP_THROUGHPUT_MAX",
		"CSIS_THROUGHPUT", "CSIS_THROUGHPUT_MAX",
		"ISP_THROUGHPUT", "ISP_THROUGHPUT_MAX",
		"MFC1_THROUGHPUT", "MFC1_THROUGHPUT_MAX",
		"NETWORK_THROUGHPUT",
	};

	int i = *(loff_t *)v;
	unsigned long size = ARRAY_SIZE(pm_log->exynos_pm_qos);
	unsigned long idx;
	unsigned long flags;

	struct __pm_qos_log qos;
	const char *class;
	unsigned long rem_nsec;

	if (i >= size)
		return 0;

	if (i == 0) {
		curr_exynos_pm_qos_idx =
			atomic_read(&pm_log_idx.exynos_pm_qos_idx);

		seq_puts(m, "time\t\t\tclass\t\t\t\tfrequency\tfunction\n");
	}

	idx = curr_exynos_pm_qos_idx;
	idx &= size - 1;

	spin_lock_irqsave(&exynos_pm_qos_lock, flags);

	qos.time = pm_log->exynos_pm_qos[idx].time;
	if (!qos.time) {
		spin_unlock_irqrestore(&exynos_pm_qos_lock, flags);
		return 0;
	}

	qos.pm_qos_class = pm_log->exynos_pm_qos[idx].pm_qos_class;
	qos.value = pm_log->exynos_pm_qos[idx].value;
	strncpy(qos.func, pm_log->exynos_pm_qos[idx].func, QOS_FUNC_NAME_LEN);
	qos.line = pm_log->exynos_pm_qos[idx].line;

	spin_unlock_irqrestore(&exynos_pm_qos_lock, flags);

	rem_nsec = do_div(qos.time, NSEC_PER_SEC);

	if (qos.pm_qos_class < EXYNOS_PM_QOS_NUM_CLASSES)
		class = pm_qos_class_name[qos.pm_qos_class];
	else
		class = pm_qos_class_name[0];

	seq_printf(m, "[%8lu.%06lu]\t%-24s\t%-8lu\t%s:%u\n",
			(unsigned long)qos.time,
			rem_nsec / NSEC_PER_USEC, class,
			qos.value, qos.func, qos.line);

	curr_exynos_pm_qos_idx--;

	return 0;
}

static void *exynos_pm_qos_seq_start(struct seq_file *f, loff_t *pos)
{
	unsigned long size = ARRAY_SIZE(pm_log->exynos_pm_qos);

	if (*pos)
		curr_exynos_pm_qos_idx++;

	return (*pos < size) ? pos : NULL;
}

static void *exynos_pm_qos_seq_next(struct seq_file *f, void *v, loff_t *pos)
{
	unsigned long size = ARRAY_SIZE(pm_log->exynos_pm_qos);

	(*pos)++;
	if (*pos >= size)
		return NULL;

	return pos;
}

static void exynos_pm_qos_seq_stop(struct seq_file *f, void *v)
{
	/* Nothing to do */
}

static const struct seq_operations exynos_pm_qos_fops = {
	.start = exynos_pm_qos_seq_start,
	.next  = exynos_pm_qos_seq_next,
	.stop  = exynos_pm_qos_seq_stop,
	.show  = exynos_pm_qos_seq_show,
};

static int sec_pm_log_procfs_init(struct sec_pm_log_info *info)
{
	sec_pm_log_parent = proc_mkdir("sec_pm_log", NULL);
	if (!sec_pm_log_parent) {
		dev_err(info->dev, "%s: Failed to create procfs root\n",
				__func__);
		return -1;
	}

	proc_create_seq("exynos_pm_qos", 0, sec_pm_log_parent,
			&exynos_pm_qos_fops);

	exynos_pm_qos_procfs_init(sec_pm_log_parent);
	dbg_snapshot_log_procfs_init(sec_pm_log_parent);

	return 0;
}

static void sec_pm_log_init_log_idx(void)
{
	atomic_set(&(pm_log_idx.exynos_pm_qos_idx), -1);
}

static int sec_pm_log_probe(struct platform_device *pdev)
{
	struct sec_pm_log_info *info;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "%s: Fail to alloc info\n", __func__);
		return -ENOMEM;
	}

	pm_log = devm_kzalloc(&pdev->dev, sizeof(*pm_log), GFP_KERNEL);
	if (!pm_log) {
		dev_err(&pdev->dev, "%s: Fail to alloc pm_log\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, info);
	info->dev = &pdev->dev;

	if (sec_pm_log_procfs_init(info))
		return -EPERM;

	sec_pm_log_init_log_idx();
	sec_pm_log_register_notifier(info);

	return 0;
}

static int sec_pm_log_remove(struct platform_device *pdev)
{
	struct sec_pm_log_info *info = platform_get_drvdata(pdev);

	dev_info(info->dev, "%s\n", __func__);
	sec_pm_log_unregister_notifier(info);

	return 0;
}

static const struct of_device_id sec_pm_log_match[] = {
	{ .compatible = "samsung,sec-pm-log", },
	{ },
};
MODULE_DEVICE_TABLE(of, sec_pm_log_match);

static struct platform_driver sec_pm_log_driver = {
	.driver = {
		.name = "sec-pm-log",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sec_pm_log_match),
	},
	.probe = sec_pm_log_probe,
	.remove = sec_pm_log_remove,
};

static int __init sec_pm_log_init(void)
{
	return platform_driver_register(&sec_pm_log_driver);
}
late_initcall(sec_pm_log_init);

static void __exit sec_pm_log_exit(void)
{
	platform_driver_unregister(&sec_pm_log_driver);
}
module_exit(sec_pm_log_exit);

MODULE_AUTHOR("Minsung Kim <ms925.kim@samsung.com>");
MODULE_DESCRIPTION("SEC PM Logging Driver");
MODULE_LICENSE("GPL");
