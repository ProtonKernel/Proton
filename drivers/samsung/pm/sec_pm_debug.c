/*
 * sec_pm_debug.c
 *
 *  Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *  Minsung Kim <ms925.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/sec_class.h>
#include <linux/sec_pm_debug.h>
#include <linux/suspend.h>

struct sec_pm_debug_info {
	struct device		*dev;
	struct device		*sec_pm_dev;
};

static unsigned int sleep_count;
static struct timespec64 total_sleep_time;
static ktime_t last_monotime; /* monotonic time before last suspend */
static ktime_t curr_monotime; /* monotonic time after last suspend */
static ktime_t last_stime; /* monotonic boottime offset before last suspend */
static ktime_t curr_stime; /* monotonic boottime offset after last suspend */

static ssize_t sleep_time_sec_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%lld\n", total_sleep_time.tv_sec);
}

static ssize_t sleep_count_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", sleep_count);
}

static ssize_t pwr_on_off_src_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 pmic_onsrc = 0;
	u8 pmic_offsrc[2] = {0,};

	pmic_get_onsrc(&pmic_onsrc, 1);
	pmic_get_offsrc(pmic_offsrc, 2);

	return sprintf(buf, "ONSRC:0x%02X OFFSRC:0x%02X,0x%02X\n", pmic_onsrc,
			pmic_offsrc[0], pmic_offsrc[1]);
}

static DEVICE_ATTR_RO(sleep_time_sec);
static DEVICE_ATTR_RO(sleep_count);
static DEVICE_ATTR_RO(pwr_on_off_src);

static struct attribute *sec_pm_debug_attrs[] = {
	&dev_attr_sleep_time_sec.attr,
	&dev_attr_sleep_count.attr,
	&dev_attr_pwr_on_off_src.attr,
	NULL
};
ATTRIBUTE_GROUPS(sec_pm_debug);

static int suspend_resume_pm_event(struct notifier_block *notifier,
		unsigned long pm_event, void *unused)
{
	struct timespec64 sleep_time;
	struct timespec64 total_time;
	struct timespec64 suspend_resume_time;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		/* monotonic time since boot */
		last_monotime = ktime_get();
		/* monotonic time since boot including the time spent in suspend */
		last_stime = ktime_get_boottime();
		break;
	case PM_POST_SUSPEND:
		/* monotonic time since boot */
		curr_monotime = ktime_get();
		/* monotonic time since boot including the time spent in suspend */
		curr_stime = ktime_get_boottime();

		total_time = ktime_to_timespec64(ktime_sub(curr_stime, last_stime));
		suspend_resume_time =
			ktime_to_timespec64(ktime_sub(curr_monotime, last_monotime));
		sleep_time = timespec64_sub(total_time, suspend_resume_time);

		total_sleep_time = timespec64_add(total_sleep_time, sleep_time);
		sleep_count++;
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block sec_pm_notifier_block = {
	.notifier_call = suspend_resume_pm_event,
};

static int sec_pm_debug_probe(struct platform_device *pdev)
{
	struct sec_pm_debug_info *info;
	struct device *sec_pm_dev;
	int ret;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "%s: Fail to alloc info\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, info);
	info->dev = &pdev->dev;

	ret = register_pm_notifier(&sec_pm_notifier_block);
	if (ret) {
		dev_err(info->dev, "%s: failed to register PM notifier(%d)\n",
				__func__, ret);
		return ret;
	}
	total_sleep_time.tv_sec = 0;
	total_sleep_time.tv_nsec = 0;

	sec_pm_dev = sec_device_create(info, "pm");

	if (IS_ERR(sec_pm_dev)) {
		dev_err(info->dev, "%s: fail to create sec_pm_dev\n", __func__);
		return PTR_ERR(sec_pm_dev);
	}

	info->sec_pm_dev = sec_pm_dev;

	ret = sysfs_create_groups(&sec_pm_dev->kobj, sec_pm_debug_groups);
	if (ret) {
		dev_err(info->dev, "%s: failed to create sysfs groups(%d)\n",
				__func__, ret);
		goto err_create_sysfs;
	}

	return 0;

err_create_sysfs:
	sec_device_destroy(sec_pm_dev->devt);
	return ret;
}

static int sec_pm_debug_remove(struct platform_device *pdev)
{
	struct sec_pm_debug_info *info = platform_get_drvdata(pdev);

	sec_device_destroy(info->sec_pm_dev->devt);
	return 0;
}

static const struct of_device_id sec_pm_debug_match[] = {
	{ .compatible = "samsung,sec-pm-debug", },
	{ },
};
MODULE_DEVICE_TABLE(of, sec_pm_debug_match);

static struct platform_driver sec_pm_debug_driver = {
	.driver = {
		.name = "sec-pm-debug",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sec_pm_debug_match),
	},
	.probe = sec_pm_debug_probe,
	.remove = sec_pm_debug_remove,
};

static int __init sec_pm_debug_init(void)
{
	return platform_driver_register(&sec_pm_debug_driver);
}
late_initcall(sec_pm_debug_init);

static void __exit sec_pm_debug_exit(void)
{
	platform_driver_unregister(&sec_pm_debug_driver);
}
module_exit(sec_pm_debug_exit);

MODULE_SOFTDEP("pre: acpm-mfd-bus");
MODULE_AUTHOR("Minsung Kim <ms925.kim@samsung.com>");
MODULE_DESCRIPTION("SEC PM Debug Driver");
MODULE_LICENSE("GPL");
