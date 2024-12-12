// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * Exynos - S2R Scenario Lockup Detector
 *
 */

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/cpu.h>
#include <linux/cpuhotplug.h>
#include <linux/suspend.h>
#include <linux/sched/clock.h>
#include <linux/preempt.h>
#include <linux/timer.h>
#include <uapi/linux/sched/types.h>
#include <trace/events/power.h>
#include <soc/samsung/exynos-ssld.h>

enum s2r_stage {
	ePM_RUNNING = 0,
	ePM_SUSPEND_PREPARE,
	ePM_DEV_SUSPEND_PREPARE,
	ePM_DEV_SUSPEND_NOIRQ,
	ePM_DEV_RESUME_NOIRQ,
	ePM_DEV_RESUME_COMPLETE,
	ePM_POST_SUSPEND,
	ePM_CNT,
};

struct ssld_descriptor {
	enum s2r_stage s2r_stage;
	const char *s2r_stage_name[ePM_CNT];
	struct timer_list timer;
	struct task_struct *s2r_leading_tsk;
	u64 pm_prepare_jiffies;
	struct s2r_trace_info last_info;
	struct notifier_block pre_s2r_lockup_detector_pm_nb;
	struct notifier_block post_s2r_lockup_detector_pm_nb;
};

static unsigned int threshold = 25;
module_param(threshold, uint, 0644);

ATOMIC_NOTIFIER_HEAD(ssld_notifier_list);

void ssld_notifier_chain_register(struct notifier_block *nb)
{
	atomic_notifier_chain_register(&ssld_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(ssld_notifier_chain_register);

static int __maybe_unused sec_from_saved_jiffies(u64 from)
{
	if (from > jiffies)
		return 0;
	else
		return (jiffies - from) / HZ;
}

static int s2r_lockup_detector_prepare(struct device *dev)
{
	struct ssld_descriptor *desc;

	desc = dev_get_drvdata(dev);
	desc->s2r_stage = ePM_DEV_SUSPEND_PREPARE;
	return 0;
}

static int s2r_lockup_detector_suspend_noirq(struct device *dev)
{
	struct ssld_descriptor *desc;

	desc = dev_get_drvdata(dev);
	desc->s2r_stage = ePM_DEV_SUSPEND_NOIRQ;
	return 0;
}

static int s2r_lockup_detector_resume_noirq(struct device *dev)
{
	struct ssld_descriptor *desc;

	desc = dev_get_drvdata(dev);
	desc->s2r_stage = ePM_DEV_RESUME_NOIRQ;
	return 0;
}

static void s2r_lockup_detector_complete(struct device *dev)
{
	struct ssld_descriptor *desc;

	desc = dev_get_drvdata(dev);
	desc->s2r_stage = ePM_DEV_RESUME_COMPLETE;
}

static const struct dev_pm_ops __maybe_unused s2r_lockup_detector_pm_ops = {
	.prepare = s2r_lockup_detector_prepare,
	.suspend_noirq = s2r_lockup_detector_suspend_noirq,
	.resume_noirq = s2r_lockup_detector_resume_noirq,
	.complete = s2r_lockup_detector_complete,
};

static int pre_s2r_lockup_detector_pm_notifier(struct notifier_block *notifier,
						unsigned long pm_event, void *v)
{
	struct ssld_descriptor *desc = container_of(notifier, struct ssld_descriptor,
							pre_s2r_lockup_detector_pm_nb);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		desc->s2r_stage = ePM_SUSPEND_PREPARE;
		desc->pm_prepare_jiffies = jiffies;
		desc->s2r_leading_tsk = current;
		if (timer_pending(&desc->timer)) {
			mod_timer(&desc->timer, jiffies + HZ * threshold);
		} else {
			desc->timer.expires = jiffies + HZ * threshold;
			add_timer(&desc->timer);
		}
		break;
	case PM_POST_SUSPEND:
		if (desc->s2r_stage != ePM_DEV_RESUME_COMPLETE) {
			del_timer_sync(&desc->timer);
			desc->s2r_stage = ePM_RUNNING;
		}
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int post_s2r_lockup_detector_pm_notifier(struct notifier_block *notifier,
						unsigned long pm_event, void *v)
{
	struct ssld_descriptor *desc = container_of(notifier, struct ssld_descriptor,
							post_s2r_lockup_detector_pm_nb);

	switch (pm_event) {
	case PM_POST_SUSPEND:
		if (timer_pending(&desc->timer))
			del_timer_sync(&desc->timer);
		desc->s2r_stage = ePM_RUNNING;
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static void print_last_suspend_resume_info(struct ssld_descriptor *desc)
{
	struct s2r_trace_info *info = &desc->last_info;

	if (info->start) {
		if (!!info->dev) {
			dev_emerg(info->dev, "**** DPM device timeout ****\n");
			dev_emerg(info->dev, "time = %lu nsec | pm_ops(%s, %d)\n",
									info->time,
									info->pm_ops,
									info->event);
		} else {
			pr_emerg("**** suspend/resume timeout ****\n");
			pr_emerg("time = %lu nsec | action(%s, %d)\n", info->time,
								info->action, info->val);
		}
	} else {
		if (!!info->dev)
			dev_emerg(info->dev, "last DPM info(%d) time = %lu nsec\n",
								info->error, info->time);
		else
			pr_emerg("last pm info(%s, %d) time = %lu nsec\n", info->action,
								info->val, info->time);
	}
}

static void s2r_lockup_detector_handler(struct timer_list *t)
{
	struct ssld_descriptor *desc = container_of(t, struct ssld_descriptor, timer);

	if (desc->s2r_stage > ePM_RUNNING && desc->s2r_stage < ePM_POST_SUSPEND) {
		struct ssld_notifier_data nb_data;

		pr_emerg("curr jiffies(%lu) pm_prepare_jiffies (%lu) expires jiffies(%lu)\n",
					jiffies, desc->pm_prepare_jiffies, t->expires);
		pr_emerg("Suspend leading process = [%s, %d] delayed about %d sec\n",
					desc->s2r_leading_tsk->comm, desc->s2r_leading_tsk->pid,
					sec_from_saved_jiffies(desc->pm_prepare_jiffies));
		print_last_suspend_resume_info(desc);

		nb_data.s2r_leading_tsk = desc->s2r_leading_tsk;
		nb_data.last_info = &desc->last_info;
		nb_data.pm_prepare_jiffies = desc->pm_prepare_jiffies;
		atomic_notifier_call_chain(&ssld_notifier_list, 0, &nb_data);

		panic("Suspend/Resume hang between %s and %s",
					desc->s2r_stage_name[desc->s2r_stage],
					desc->s2r_stage_name[desc->s2r_stage + 1]);
	}
}

static void ssld_suspend_resume(void *data, const char *action, int val, bool start)
{
	struct ssld_descriptor *desc = (struct ssld_descriptor *)data;

	desc->last_info.time = local_clock();
	desc->last_info.dev = NULL;
	desc->last_info.action = action;
	desc->last_info.val = val;
	desc->last_info.start = start;
}

static void ssld_dev_pm_cb_start(void *data, struct device *dev, const char *pm_ops, int event)
{
	struct ssld_descriptor *desc = (struct ssld_descriptor *)data;

	desc->last_info.time = local_clock();
	desc->last_info.dev = dev;
	desc->last_info.pm_ops = pm_ops;
	desc->last_info.event = event;
	desc->last_info.start = true;
}

static void ssld_dev_pm_cb_end(void *data, struct device *dev, int error)
{
	struct ssld_descriptor *desc = (struct ssld_descriptor *)data;

	desc->last_info.time = local_clock();
	desc->last_info.dev = dev;
	desc->last_info.pm_ops = NULL;
	desc->last_info.error = error;
	desc->last_info.start = false;
}

static void s2r_lockup_detector_resource_init(struct ssld_descriptor *desc)
{
	desc->s2r_stage_name[ePM_RUNNING] = "RUNNING";
	desc->s2r_stage_name[ePM_SUSPEND_PREPARE] = "PM_SUSPEND_PREPARE";
	desc->s2r_stage_name[ePM_DEV_SUSPEND_PREPARE] = "DEV_SUSPEND_PREPARE";
	desc->s2r_stage_name[ePM_DEV_SUSPEND_NOIRQ] = "DEV_SUSPEND_NOIRQ";
	desc->s2r_stage_name[ePM_DEV_RESUME_NOIRQ] = "DEV_RESUME_NOIRQ";
	desc->s2r_stage_name[ePM_DEV_RESUME_COMPLETE] = "DEV_RESUME_COMPLETE";
	desc->s2r_stage_name[ePM_POST_SUSPEND] = "PM_POST_SUSPEND";

	desc->pre_s2r_lockup_detector_pm_nb.notifier_call = pre_s2r_lockup_detector_pm_notifier;
	desc->pre_s2r_lockup_detector_pm_nb.priority = INT_MAX;
	register_pm_notifier(&desc->pre_s2r_lockup_detector_pm_nb);

	desc->post_s2r_lockup_detector_pm_nb.notifier_call = post_s2r_lockup_detector_pm_notifier;
	desc->post_s2r_lockup_detector_pm_nb.priority = INT_MIN;
	register_pm_notifier(&desc->post_s2r_lockup_detector_pm_nb);

	desc->s2r_stage = ePM_RUNNING;
	timer_setup(&desc->timer, s2r_lockup_detector_handler, 0);

	register_trace_suspend_resume(ssld_suspend_resume, desc);
	register_trace_device_pm_callback_start(ssld_dev_pm_cb_start, desc);
	register_trace_device_pm_callback_end(ssld_dev_pm_cb_end, desc);
}

static struct bus_type ssld_bus_type = {
	.name = "ssld",
};

static struct device_driver s2r_lockup_detector_driver = {
	.name = "ssld_drv",
	.bus = &ssld_bus_type,
	.pm = &s2r_lockup_detector_pm_ops,
};

static struct device s2r_lockup_detector_device = {
	.init_name = "ssld_dev",
	.bus = &ssld_bus_type,
};

static int __init s2r_lockup_detector_driver_init(void)
{
	struct ssld_descriptor *ssld_desc;
	int ret;

	ret = bus_register(&ssld_bus_type);
	if (ret)
		goto bus_register_err;

	ret = driver_register(&s2r_lockup_detector_driver);
	if (ret < 0)
		goto driver_register_err;

	ret = device_register(&s2r_lockup_detector_device);
	if (ret < 0)
		goto device_register_err;

	ssld_desc = devm_kzalloc(&s2r_lockup_detector_device,
				sizeof(struct ssld_descriptor), GFP_KERNEL);
	if (!ssld_desc) {
		ret = -ENOMEM;
		goto alloc_fail;
	}

	dev_set_drvdata(&s2r_lockup_detector_device, ssld_desc);
	s2r_lockup_detector_resource_init(ssld_desc);
	pr_info("s2r lockup detector is enabled(threshold = %u)\n", threshold);
	return 0;

alloc_fail:
	device_unregister(&s2r_lockup_detector_device);
device_register_err:
	driver_unregister(&s2r_lockup_detector_driver);
driver_register_err:
	bus_unregister(&ssld_bus_type);
bus_register_err:
	return ret;
}

static void __exit s2r_lockup_detector_driver_exit(void)
{
	struct ssld_descriptor *desc = dev_get_drvdata(&s2r_lockup_detector_device);

	unregister_trace_suspend_resume(ssld_suspend_resume, desc);
	unregister_trace_device_pm_callback_start(ssld_dev_pm_cb_start, desc);
	unregister_trace_device_pm_callback_end(ssld_dev_pm_cb_end, desc);

	unregister_pm_notifier(&desc->pre_s2r_lockup_detector_pm_nb);
	unregister_pm_notifier(&desc->post_s2r_lockup_detector_pm_nb);
	del_timer(&desc->timer);
	device_unregister(&s2r_lockup_detector_device);
	driver_unregister(&s2r_lockup_detector_driver);
	bus_unregister(&ssld_bus_type);
}

module_init(s2r_lockup_detector_driver_init);
module_exit(s2r_lockup_detector_driver_exit);

MODULE_DESCRIPTION("Samsung Exynos S2R Scenario Lockup Detector");
MODULE_LICENSE("GPL v2");
