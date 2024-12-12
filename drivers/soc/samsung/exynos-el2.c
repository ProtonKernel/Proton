/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - EL2 module
 * Author: Junho Choi <junhosj.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpumask.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>

#include <asm/barrier.h>
#include <asm/io.h>

#include <soc/samsung/exynos-el2.h>
#include <soc/samsung/exynos-hvc.h>
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#include <soc/samsung/debug-snapshot.h>
#endif


/* The buffer for EL2 crash information */
static uint64_t *el2_crash_buf;

static spinlock_t el2_lock;

static uint64_t exynos_lv3_table_idx;
static uint64_t *exynos_lv3_table[EL2_LV3_BUF_MAX];
static uint64_t *exynos_lv3_table_4MB;

static bool stage2_success;


static void exynos_report_el2_crash_info(int cpu,
					 int max_reg_num,
					 int crash_flag,
					 bool has_2nd_crash)
{
	char *buf_base = (char *)el2_crash_buf + (EL2_CRASH_BUFFER_SIZE * cpu);
	char *buf = buf_base;
	int i, len = 0;
	int rep_cnt = 2;
	unsigned long flags;

	spin_lock_irqsave(&el2_lock, flags);

	pr_info("\n================[EL2 EXCEPTION REPORT]================\n");
	pr_info("\n");

	pr_info("[CPU %d] Exception Information\n", cpu);

	pr_info("The exception happened in%sH-Arx\n",
			(crash_flag == 0) ? " " : " not ");
	pr_info("\n");

	do {
		if (rep_cnt == 1) {
			pr_info("The 2nd Exception Information\n");
			pr_info("\n");
		}

		for (i = 0; i < max_reg_num; i++) {
			len = pr_info("%s", buf);

			/* Skip '\n'(Line Feed) and '0'(NULL) characters */
			buf += (len + 2);

			pr_debug("\n");
			pr_debug("- String length : %x\n", len);
			pr_debug("- Next buffer : %#llx\n", (uint64_t)buf);
			pr_debug("- NO. of regs : %d\n", i);
			pr_debug("\n");

			if (*buf == '\0')
				break;
		}

		pr_info("\n======================================================\n");
		pr_info("\n");

		buf = buf_base + (EL2_CRASH_BUFFER_SIZE / 2);
		rep_cnt--;

	} while ((has_2nd_crash == true) && (rep_cnt > 0));

	spin_unlock_irqrestore(&el2_lock, flags);

	BUG();

	/* Never reach here */
	do {
		wfi();
	} while (1);
}

static irqreturn_t exynos_el2_dynamic_lv3_irq_handler_thread(int irq, void *dev_id)
{
	uint64_t ret = 0;
	uint64_t buf_pa = 0;
	uint64_t *buf = NULL;
	int retry_cnt = MAX_RETRY_COUNT;

	pr_info("%s: Start allocate EL2 level 3 256K table buffer\n", __func__);

	do {
		buf = kzalloc(EL2_DYNAMIC_LV3_TABLE_BUF_256K, GFP_KERNEL);
		if (buf != NULL) {
			exynos_lv3_table[exynos_lv3_table_idx++] = buf;
			buf_pa = virt_to_phys(buf);
			pr_info("%s: Allocate exynos lv3 256K table buf VA[%#llx]/IPA[%#llx]\n",
					__func__, (uint64_t)buf, buf_pa);
			break;
		}
	} while ((buf == NULL) && (retry_cnt-- > 0));

	if (buf == NULL)
		pr_err("%s: Fail to allocate 4MB lv3 table buffer\n", __func__);

	ret = exynos_hvc(HVC_FID_SET_EL2_LV3_TABLE_BUF,
				buf_pa,	EL2_DYNAMIC_LV3_TABLE_BUF_256K,	0, 0);
	if (ret)
		pr_err("%s: deliver lv3 table failed ret:%llx\n", __func__, ret);

	return IRQ_HANDLED;
}

static ssize_t exynos_stage2_enable_check(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	int ret = 0;

	if (stage2_success)
		ret += snprintf(buf, S2_BUF_SIZE, "stage2 is enable\n");
	else
		ret += snprintf(buf, S2_BUF_SIZE, "stage2 is disable\n");

	return ret;
}

static ssize_t exynos_stage2_enable_test(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t count)
{
	unsigned long i;
	unsigned long affinity;
	uint64_t ret = 0;

	for(i = 0; i < num_possible_cpus(); i++) {
		affinity = 1 << i;
		set_cpus_allowed_ptr(current, to_cpumask(&affinity));
		ret = exynos_hvc(HVC_FID_CHECK_STAGE2_ENABLE, 0, 0, 0, 0);

		if (!ret) {
			stage2_success = 0;
			dev_err(dev, "%llx core Stage2 mapping is not enable\n", i);
			return -EINVAL;
		}
	}

	stage2_success = 1;
	dev_info(dev, "stage2 is all enable\n");

	return count;
}

static ssize_t exynos_el2_log_print(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	int ret = 0;
	char *buf_tmp;
	uint64_t buf_pa;
	uint64_t ret_64 = 0;

	buf_tmp = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf_tmp)
		return -ENOMEM;

	buf_pa = virt_to_phys(buf_tmp);

	ret_64 = exynos_hvc(HVC_FID_GET_EL2_LOG_BUF, (uint64_t)buf_pa, 0, 0, 0);
	if (ret_64) {
		dev_err(dev, "Fail to get EL2 log buf, ret[%llx]\n", ret_64);
		return -EINVAL;
	}

	buf_tmp[PAGE_SIZE - 1] = 0;
	ret = snprintf(buf, PAGE_SIZE, "%s", buf_tmp);
	kfree(buf_tmp);

	return ret;
}

static ssize_t exynos_el2_log_clear(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t count)
{
	uint64_t ret;

	ret = exynos_hvc(HVC_FID_GET_EL2_LOG_BUF, 0, EL2_LOG_BUFFER_CLEAR, 0, 0);
	if (ret) {
		dev_err(dev, "Fail to reset EL2 log cur, ret[%llx]\n", ret);
		return -EINVAL;
	}

	dev_info(dev, "Successfully reset EL2 log cur\n");

	return count;
}

static DEVICE_ATTR(exynos_stage2_enable, 0644,
		exynos_stage2_enable_check, exynos_stage2_enable_test);
static DEVICE_ATTR(exynos_el2_log, 0644,
		exynos_el2_log_print, exynos_el2_log_clear);

static int exynos_el2_probe(struct platform_device *pdev)
{
	int ret;
	uint64_t ret_64;
	unsigned int irq_num = 0;
	struct irq_data *el2_d_irqd = NULL;
	irq_hw_number_t hwirq = 0;

	irq_num = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (!irq_num) {
		dev_err(&pdev->dev, "Fail to get irq from dt\n");
		return -EINVAL;
	}

	ret = devm_request_threaded_irq(&pdev->dev,
					irq_num,
					NULL,
					exynos_el2_dynamic_lv3_irq_handler_thread,
					IRQF_ONESHOT,
					pdev->name,
					NULL);
	if (ret) {
		dev_err(&pdev->dev, "Fail to request IRQ handler. ret(%d) irq(%d)\n",
			ret, irq_num);
		return ret;
	}

	el2_d_irqd = irq_get_irq_data(irq_num);
	if (!el2_d_irqd) {
		dev_err(&pdev->dev, "Fail to get irq data\n");
		return -EINVAL;
	}

	hwirq = irqd_to_hwirq(el2_d_irqd);

	ret_64 = exynos_hvc(HVC_FID_DELIVER_LV3_ALLOC_IRQ_NUM, hwirq, 0, 0, 0);
	if (ret_64) {
		dev_err(&pdev->dev, "Fail to deliver irq num, ret[%llx]\n", ret_64);
		return -EINVAL;
	}

	dev_info(&pdev->dev, "Irq register success\n");

	ret = device_create_file(&pdev->dev,
				 &dev_attr_exynos_stage2_enable);
	if (ret)
		dev_err(&pdev->dev, "stage2_enable sysfs create file fail\n");
	else
		dev_info(&pdev->dev, "stage2_enable sysfs create file success\n");

	ret = device_create_file(&pdev->dev,
				 &dev_attr_exynos_el2_log);
	if (ret)
		dev_err(&pdev->dev, "exynos_el2_log sysfs create file fail\n");
	else
		dev_info(&pdev->dev, "exynos_el2_log sysfs create file success\n");

	return 0;
}

static const struct of_device_id exynos_el2_of_match_table[] = {
	{ .compatible = "samsung,exynos-el2-dynamic-lv3", },
	{ },
};

static struct platform_driver exynos_el2_driver = {
	.probe = exynos_el2_probe,
	.driver = {
		.name = "exynos-el2",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_el2_of_match_table),
	}
};

static int __init exynos_el2_dynamic_lv3_table_init(void)
{
	uint64_t ret = 0;
	uint64_t buf_pa;
	uint64_t *buf;
	int retry_cnt = MAX_RETRY_COUNT;

	do {
		buf = kzalloc(EL2_LV3_TABLE_BUF_SIZE_4MB, GFP_KERNEL);
		if (buf != NULL) {
			exynos_lv3_table_4MB = buf;
			buf_pa = virt_to_phys(buf);
			pr_info("%s: Allocate 4MB buffer VA[%#llx]/IPA[%#llx]\n",
					__func__, (uint64_t)buf, buf_pa);
			break;
		}
	} while ((buf == NULL) && (retry_cnt-- > 0));

	if (buf == NULL) {
		pr_err("%s: Fail to allocate EL2 level3 table buffer\n", __func__);
		return -ENOMEM;
	}

	ret = exynos_hvc(HVC_FID_SET_EL2_LV3_TABLE_BUF,
				buf_pa, EL2_LV3_TABLE_BUF_SIZE_4MB, 0, 0);
	if (ret) {
		pr_err("%s: Fail to allocate lv3 table buf ret = %llx\n",__func__, ret);
		return -EINVAL;
	}

	pr_info("%s: EL2 dynamic level3 table buffer deliver successfully\n", __func__);

	return 0;
}

static int __init exynos_el2_crash_info_init(void)
{
	uint64_t ret = 0;
	uint64_t buf_pa;
	uint64_t *buf;
	size_t len = EL2_CRASH_BUFFER_SIZE * num_possible_cpus();
	int retry_cnt = MAX_RETRY_COUNT;

	unsigned long info_ver, log_base, log_size;
	int ret_32;

	spin_lock_init(&el2_lock);

	do {
		buf = kzalloc(len, GFP_KERNEL);
		if (buf != NULL) {
			el2_crash_buf = buf;
			buf_pa = virt_to_phys(buf);
			pr_info("%s: Allocate EL2 crash buffer VA[%#llx]/IPA[%#llx] len[%#llx]\n",
					__func__, (uint64_t)el2_crash_buf, buf_pa, len);

			break;
		}
	} while ((buf == NULL) && (retry_cnt-- > 0));

	if (buf == NULL) {
		pr_err("%s: Fail to allocate EL2 crash buffer\n", __func__);
		return -ENOMEM;
	}

	ret = exynos_hvc(HVC_FID_SET_EL2_CRASH_INFO_FP_BUF,
			 (uint64_t)exynos_report_el2_crash_info,
			 buf_pa, 0, 0);
	if (ret) {
		pr_err("%s: Already set EL2 exception report function\n",
				__func__);
		return -EPERM;
	}


#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	/* EL2 log dump */
	info_ver = exynos_hvc(HVC_FID_GET_HARX_INFO,
				HARX_INFO_TYPE_VERSION,
				0, 0, 0);
	if (info_ver != HARX_INFO_VERSION) {
		pr_err("%s: HARX_INFO_VERSION mismatch [%llx]\n",
				__func__, info_ver);
		goto err_el2_log_dump;
	}

	log_base = exynos_hvc(HVC_FID_GET_HARX_INFO,
				HARX_INFO_TYPE_RAM_LOG_BASE,
				0, 0, 0);
	if (log_base == ERROR_INVALID_INFO_TYPE) {
		pr_err("%s: Fail to get EL2 log base\n", __func__);
		goto err_el2_log_dump;
	}

	log_size = exynos_hvc(HVC_FID_GET_HARX_INFO,
				HARX_INFO_TYPE_RAM_LOG_SIZE,
				0, 0, 0);
	if (log_size == ERROR_INVALID_INFO_TYPE) {
		pr_err("%s: Fail to get EL2 log size\n", __func__);
		goto err_el2_log_dump;
	}

	pr_info("%s: EL2 log buffer base[%#llx]/size[%#llx]\n",
			__func__, log_base, log_size);

	ret_32 = dbg_snapshot_add_bl_item_info(EL2_LOG_DSS_NAME,
						log_base, log_size);
	if (ret_32)
		pr_err("%s: Fail to add EL2 log dump - ret[%d]\n",
				__func__, ret);

err_el2_log_dump:
#endif
	pr_info("%s: EL2 Crash Info Buffer and FP are delivered successfully\n",
			__func__);

	return 0;
}

static int __init exynos_el2_module_init(void)
{
	int ret = 0;

	ret = exynos_el2_crash_info_init();
	if (ret) {
		pr_err("%s: exynos_el2_crash_info_init fail ret[%d]\n", __func__, ret);
		return ret;
	}

	ret = exynos_el2_dynamic_lv3_table_init();
	if (ret) {
		pr_err("%s: exynos_el2_dynamic_lv3_table_init fail\n", __func__, ret);
		return ret;
	}

	return platform_driver_register(&exynos_el2_driver);
}

static void __exit exynos_el2_module_exit(void)
{
	kfree(el2_crash_buf);
	platform_driver_unregister(&exynos_el2_driver);
}

core_initcall(exynos_el2_module_init);
module_exit(exynos_el2_module_exit);

MODULE_DESCRIPTION("Exynos EL2 module driver");
MODULE_AUTHOR("<junhosj.choi@samsung.com>");
MODULE_LICENSE("GPL");
