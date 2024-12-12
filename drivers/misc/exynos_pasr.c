// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 */

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/list_sort.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/memory.h>

#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/module.h>

#include "exynos_pasr.h"

static int pasr_on;
module_param_named(pasr_on, pasr_on, uint, 0644);

static int pasr_enable(struct pasr_dev *pasr, int idx)
{
	/* TODO: set DMC to enable PASR */

	dev_info(pasr->dev,
		 "self-refresh is disabled for segment %d, mask:%#x\n",
		 idx, pasr->pasr_mask);

	return 0;
}

static int pasr_disable(struct pasr_dev *pasr, int idx)
{
	/* TODO: set DMC to disable PASR */

	dev_info(pasr->dev,
		 "self-refresh is enabled for segment %d, mask:%#x\n",
		 idx, pasr->pasr_mask);

	return 0;
}

int pasr_online_memory_segment(struct pasr_dev *pasr, int idx)
{
	struct pasr_segment *info = pasr->segment_info + idx;
	struct memory_block *mem;
	int ret;

	mem = find_memory_block(__pfn_to_section(info->addr));
	if (!mem) {
		dev_err(pasr->dev, "failed to get block for idx %d, addr:%#lx\n",
			idx, info->addr << PAGE_SHIFT);
		return -EINVAL;
	}

	ret = memory_block_online(mem);
	if (ret) {
		dev_err(pasr->dev, "failed to online memory, ret = %d\n", ret);
		dev_err(pasr->dev, "memory index %d, start addr: %#lx\n",
			idx, info->addr << PAGE_SHIFT);
		return ret;
	}

	clear_bit(idx, &pasr->offline_flag);

	return 0;
}

int pasr_offline_memory_segment(struct pasr_dev *pasr, int idx)
{
	struct pasr_segment *info = pasr->segment_info + idx;
	struct memory_block *mem;
	int ret;

	mem = find_memory_block(__pfn_to_section(info->addr));
	if (!mem) {
		dev_err(pasr->dev, "failed to get block for idx %d, addr:%#lx\n",
			idx, info->addr << PAGE_SHIFT);
		return -EINVAL;
	}

	ret = memory_block_offline(mem);
	if (ret) {
		dev_err(pasr->dev, "failed to offline memory, ret = %d\n", ret);
		dev_err(pasr->dev, "memory index %d, start addr: %#lx\n",
			idx, info->addr << PAGE_SHIFT);
		return ret;
	}

	set_bit(idx, &pasr->offline_flag);

	return 0;
}

static void pasr_memory_offline_work(struct kthread_work *work)
{
	struct pasr_dev *pasr =
			container_of(work, struct pasr_dev, off_thread.work);
	int i, ret, success_count = 0;
	ktime_t t_begin;

	t_begin = ktime_get();
	for (i = 0; i < pasr->num_segment; i++) {
		ret = pasr_offline_memory_segment(pasr, i);
		if (ret) {
			dev_info(pasr->dev,
				 "segment %d offline is stopped.\n", i);
			break;
		}
		success_count++;
	}

	if (success_count) {
		s64 elapsed_ms = ktime_to_ms(ktime_sub(ktime_get(), t_begin));

		dev_info(pasr->dev, "took %lld ms to offline %d segments\n",
			 elapsed_ms, success_count);
	}

	complete_all(&pasr->off_thread.completion);
}

static void pasr_memory_online_work(struct kthread_work *work)
{
	struct pasr_dev *pasr = container_of(work, struct pasr_dev,
					     on_thread.work);
	int i, ret, success_count = 0;
	ktime_t t_begin;

	t_begin = ktime_get();
	for (i = 0; i < pasr->num_segment; i++) {
		if (!test_bit(i, &pasr->offline_flag))
			continue;
		ret = pasr_online_memory_segment(pasr, i);
		if (ret)
			dev_err(pasr->dev,
				"online memory for segment %d is failed\n", i);
		success_count++;
	}

	if (success_count) {
		s64 elapsed_ms = ktime_to_ms(ktime_sub(ktime_get(), t_begin));

		dev_info(pasr->dev, "took %lld ms to online %d segments\n",
			 elapsed_ms, success_count);
	}

	complete_all(&pasr->on_thread.completion);
}

static const struct of_device_id exynos_pasr_match[] = {
	{ .compatible = "samsung,exynos-pasr" },
	{}
};

#define MAX_MEM_INFO	32
int pasr_get_segment_info(struct pasr_dev *pasr)
{
	int i, info_cnt = 0, segment_idx = -1;
	struct zone *movable = &NODE_DATA(0)->node_zones[ZONE_MOVABLE];
	struct memory_block *mem;
	struct pasr_segment *segment_info;
	unsigned long start_pfn, tpfn;
	unsigned long pfn, end;
	bool available;
	struct {
		int section_pfn;
		int index;
	} mem_info[MAX_MEM_INFO];

	if (movable->present_pages == 0) {
		dev_err(pasr->dev, "no present pages for movable zone\n");
		return -ENOMEM;
	}

	end = zone_end_pfn(movable);
	for (pfn = ALIGN_DOWN(movable->zone_start_pfn, PAGES_PER_SECTION);
			pfn < end; pfn += PAGES_PER_SECTION) {
		if (!pfn_present(pfn))
			continue;
		mem = find_memory_block(__pfn_to_section(pfn));
		if (!mem)
			continue;

		segment_idx++;

		start_pfn = section_nr_to_pfn(mem->start_section_nr);
		available = true;
		for (tpfn = pfn; tpfn < start_pfn + PAGES_PER_SECTION; tpfn++) {
			if (page_zone(pfn_to_page(tpfn)) != movable) {
				available = false;
				break;
			}
		}
		if (!available)
			continue;

		mem_info[info_cnt].section_pfn = pfn;
		mem_info[info_cnt].index = segment_idx;
		info_cnt++;

		if (info_cnt >= MAX_MEM_INFO) {
			dev_err(pasr->dev,
				"too many segment or something wrong\n");
			break;
		}
	}

	if (info_cnt == 0) {
		dev_err(pasr->dev, "no movable zone exists!\n");
		return -EINVAL;
	}

	segment_info = devm_kcalloc(pasr->dev, info_cnt,
				    sizeof(*segment_info), GFP_KERNEL);
	if (!segment_info)
		return -ENOMEM;

	for (i = 0; i < info_cnt; i++) {
		segment_info[i].index = mem_info[i].index;
		segment_info[i].addr = mem_info[i].section_pfn;

		dev_info(pasr->dev, "segment %d. addr:%#lx, index:%#x\n",
			 i, segment_info[i].addr, segment_info[i].index);
	}

	pasr->segment_info = segment_info;
	pasr->num_segment = info_cnt;

	return 0;
}

static int pasr_init_thread(struct pasr_thread *thread, const char *name,
			    void (*func)(struct kthread_work *))
{
	init_completion(&thread->completion);
	complete(&thread->completion);
	kthread_init_work(&thread->work, func);
	kthread_init_worker(&thread->worker);
	thread->thread = kthread_run(kthread_worker_fn,	&thread->worker, name);
	if (IS_ERR(thread->thread))
		return -ENOMEM;

	return 0;
}

static int pasr_probe(struct platform_device *pdev)
{
	struct pasr_dev *pasr;
	int ret;

	pasr = devm_kzalloc(&pdev->dev, sizeof(*pasr), GFP_KERNEL);
	if (!pasr)
		return -ENOMEM;

	pasr->dev = &pdev->dev;

	platform_set_drvdata(pdev, pasr);

	ret = pasr_get_segment_info(pasr);
	if (ret) {
		dev_err(pasr->dev, "failed to get segment info, ret:%d\n", ret);
		return ret;
	}

	if (pasr_init_thread(&pasr->on_thread, "exynos_pasr/online",
			     pasr_memory_online_work))
		return -ENOMEM;

	if (pasr_init_thread(&pasr->off_thread, "exynos_pasr/offline",
			     pasr_memory_offline_work)) {
		kthread_stop(pasr->on_thread.thread);
		return -ENOMEM;
	}

	dev_info(pasr->dev, "Exynos PASR is probed successfully.\n");

	return 0;
}

static int pasr_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int pasr_suspend(struct device *dev)
{
	return 0;
}

static int pasr_resume(struct device *dev)
{
	return 0;
}

static int pasr_suspend_noirq(struct device *dev)
{
	struct pasr_dev *pasr = dev_get_drvdata(dev);
	int i;

	if (!pasr_on)
		return 0;

	if (!completion_done(&pasr->off_thread.completion))
		wait_for_completion(&pasr->off_thread.completion);

	if (pasr->offline_flag) {
		for (i = 0; i < pasr->num_segment; i++) {
			if (!test_bit(i, &pasr->offline_flag))
				continue;
			dev_info(pasr->dev, "PASR ON for segment %d\n", i);
			pasr_enable(pasr, i);
		}
	}

	return 0;
}

static int pasr_resume_noirq(struct device *dev)
{
	struct pasr_dev *pasr = dev_get_drvdata(dev);
	int i;

	if (!pasr_on)
		return 0;

	if (pasr->offline_flag) {
		for (i = 0; i < pasr->num_segment; i++) {
			if (!test_bit(i, &pasr->offline_flag))
				continue;
			dev_info(pasr->dev, "PASR OFF for segment %d\n", i);
			pasr_disable(pasr, i);
		}
	}

	return 0;
}

static int pasr_prepare(struct device *dev)
{
	struct pasr_dev *pasr = dev_get_drvdata(dev);

	if (!pasr_on)
		return 0;

	if (!completion_done(&pasr->on_thread.completion))
		wait_for_completion(&pasr->on_thread.completion);

	reinit_completion(&pasr->off_thread.completion);
	kthread_queue_work(&pasr->off_thread.worker, &pasr->off_thread.work);

	return 0;
}

static void pasr_complete(struct device *dev)
{
	struct pasr_dev *pasr = dev_get_drvdata(dev);

	if (!pasr_on)
		return;

	if (!completion_done(&pasr->off_thread.completion))
		wait_for_completion(&pasr->off_thread.completion);

	if (!pasr->offline_flag) {
		dev_info(pasr->dev, "memory is not offlined!\n");
		return;
	}

	reinit_completion(&pasr->on_thread.completion);
	kthread_queue_work(&pasr->on_thread.worker, &pasr->on_thread.work);
}
#endif

static const struct dev_pm_ops pasr_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pasr_suspend, pasr_resume)
	.prepare = pasr_prepare,
	.complete = pasr_complete,
	.suspend_noirq = pasr_suspend_noirq,
	.resume_noirq = pasr_resume_noirq,
};

static struct platform_driver pasr_driver = {
	.driver = {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
		.pm	= &pasr_pm_ops,
		.of_match_table = of_match_ptr(exynos_pasr_match),
	},
	.probe = pasr_probe,
	.remove = pasr_remove,
};

static int __init pasr_init(void)
{
	return platform_driver_register(&pasr_driver);
}

device_initcall(pasr_init);
