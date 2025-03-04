/*
 * exynos-ssp.c - Samsung Secure Platform driver for the Exynos
 *
 * Copyright (C) 2019 Samsung Electronics
 * Keunyoung Park <keun0.park@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pm_wakeup.h>
#include <linux/smc.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/debugfs.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <soc/samsung/exynos-itmon.h>
#include <soc/samsung/exynos-smc.h>
#include <soc/samsung/exynos-pd.h>
#include <linux/soc/samsung/exynos-soc.h>

#define SSP_RET_OK		0
#define SSP_RET_FAIL		-1
#define SSP_RET_BUSY		0x20010	/* defined at LDFW */
#define SSP_MAX_USER_CNT	100
#define SSP_MAX_USER_PATH_LEN	128

/* smc to call ldfw functions */
#define SMC_CMD_SSP		(0xC2001040)
#define SSP_CMD_BOOT		(0x1)
#define SSP_CMD_BACKUP		(0x2)
#define SSP_CMD_RESTORE		(0x3)
#define SSP_CMD_SELF_TEST	(0x10)

/* ioctl command for TAC */
#define SSP_IOCTL_MAGIC		'c'
#define SSP_IOCTL_INIT		_IO(SSP_IOCTL_MAGIC, 1)
#define SSP_IOCTL_EXIT		_IOWR(SSP_IOCTL_MAGIC, 2, uint64_t)
#define SSP_IOCTL_TEST		_IOWR(SSP_IOCTL_MAGIC, 3, uint64_t)
#define SSP_IOCTL_DEBUG		_IOWR(SSP_IOCTL_MAGIC, 4, uint64_t)

#define ssp_err(dev, fmt, arg...)	printk("[EXYNOS][CMSSP][ERROR] " fmt, ##arg)
#define ssp_info(dev, fmt, arg...)	printk("[EXYNOS][CMSSP][ INFO] " fmt, ##arg)

/* SFR for ssp power control */
#define PMU_ALIVE_PA_BASE		(0x15860000 + 0x3000)
#define PMU_SSP_STATUS_VA_OFFSET	(0x4)
#define PMU_SSP_STATUS_MASK		(1 << 0)

void __iomem *pmu_va_base;

spinlock_t ssp_lock;
struct mutex ssp_ioctl_lock;
static int ssp_power_count;
static int ssp_idle_ip_index;
extern struct exynos_chipid_info exynos_soc_info;

struct ssp_device {
	struct device *dev;
	struct miscdevice misc_device;
};

struct ssp_user {
	char path[SSP_MAX_USER_PATH_LEN];
	unsigned long init_count;
	unsigned long init_time;
	unsigned long exit_count;
	unsigned long exit_time;
	unsigned long init_fail_count;
	unsigned long init_fail_time;
	unsigned long exit_fail_count;
	unsigned long exit_fail_time;
	struct ssp_user *next;
};

struct ssp_user *head = NULL;

static void exynos_ssp_print_user_list(struct device *dev)
{
	struct ssp_user *ptr = head;
	int i = 0;

	ssp_info(dev, "====== Power control status ================="
		      "=============================================\n");
	ssp_info(dev, "No: %8s:\t%8s\t%8s\t%16s\t%16s\n",
		      "Caller", "ON", "OFF", "ON-TIME", "OFF-TIME");
	ssp_info(dev, "---------------------------------------------"
		      "---------------------------------------------\n");

	while (ptr != NULL) {
		i++;

		ssp_info(dev, "%2u:\t%8s: %s\n",
			i,
			"Path",
			ptr->path);

		ssp_info(dev, "%2u:\t%8s:\t%8u\t%8u\t%16u\t%16u\n",
			i,
			"Success",
			ptr->init_count,
			ptr->exit_count,
			ptr->init_time,
			ptr->exit_time);

		ssp_info(dev, "%2u:\t%8s:\t%8u\t%8u\t%16u\t%16u\n",
			i,
			"Fail",
			ptr->init_fail_count,
			ptr->exit_fail_count,
			ptr->init_fail_time,
			ptr->exit_fail_time);

		ptr = ptr->next;
	}
	ssp_info(dev, "---------------------------------------------"
		      "---------------------------------------------\n");
}

static int exynos_ssp_get_path(struct device *dev, struct task_struct *task, char *task_path)
{
	int ret = SSP_RET_OK;
	char *buf;
	char *path;
	struct file *exe_file;

	buf = (char *)__get_free_page(GFP_KERNEL);
	if (!buf) {
		ssp_err(dev, "%s: fail to __get_free_page.\n", __func__);
		return -ENOMEM;
	}

	exe_file = get_task_exe_file(task);
	if (!exe_file) {
		ret = -ENOENT;
		ssp_err(dev, "%s: fail to get_task_exe_file.\n", __func__);
		goto end;
	}

	path = d_path(&exe_file->f_path, buf, PAGE_SIZE);
	if (IS_ERR(path)) {
		ret = PTR_ERR(path);
		ssp_err(dev, "%s: fail to d_path. ret = 0x%x\n", __func__, ret);
		goto end;
	}

	memset(task_path, 0, SSP_MAX_USER_PATH_LEN);
	strncpy(task_path, path, SSP_MAX_USER_PATH_LEN - 1);
end:
	free_page((unsigned long)buf);

	return ret;
}

static struct ssp_user *exynos_ssp_find_user(char *path)
{
	struct ssp_user *now = head;

	if (head == NULL)
		return NULL;

	while (strncmp(now->path, path, SSP_MAX_USER_PATH_LEN)) {
		if (now->next == NULL)
			return NULL;

		now = now->next;
	}

	return now;
}

static int exynos_ssp_powerctl_update(struct device *dev, bool power_on, bool pass)
{
	int ret = SSP_RET_OK;
	char path[SSP_MAX_USER_PATH_LEN];
	static int ssp_user_count;
	struct ssp_user *link = NULL;

	ret = exynos_ssp_get_path(dev, current, path);
	if (ret) {
		ssp_err(dev, "%s: fail to get path. ret = 0x%x\n", __func__, ret);
		return ret;
	}

	link = exynos_ssp_find_user(path);

	if (link == NULL) {
		if (++ssp_user_count >= SSP_MAX_USER_CNT) {
			ssp_err(dev, "%s: exceed max user count.\n", __func__);
			return -ENOMEM;
		}

		link = (struct ssp_user *)kzalloc(sizeof(struct ssp_user), GFP_KERNEL);
		if (!link) {
			ssp_err(dev, "%s: fail to kzalloc.\n", __func__);
			return -ENOMEM;
		}

		memcpy(link->path, path, sizeof(path));
		link->init_count = 0;
		link->exit_count = 0;

		link->next = head;
		head = link;
	}

	if (power_on == 1 && pass == 1) {
		link->init_count++;
		link->init_time = ktime_get_boottime_ns() / NSEC_PER_USEC;
	} else if (power_on == 1 && pass == 0) {
		link->init_fail_count++;
		link->init_fail_time = ktime_get_boottime_ns() / NSEC_PER_USEC;
	} else if (power_on == 0 && pass == 1) {
		link->exit_count++;
		link->exit_time = ktime_get_boottime_ns() / NSEC_PER_USEC;
	} else if (power_on == 0 && pass == 0) {
		link->exit_fail_count++;
		link->exit_fail_time = ktime_get_boottime_ns() / NSEC_PER_USEC;
	}

	return ret;
}


static int exynos_cm_smc(uint64_t *arg0, uint64_t *arg1,
			 uint64_t *arg2, uint64_t *arg3)
{
	struct arm_smccc_res res;

	arm_smccc_smc(*arg0, *arg1, *arg2, *arg3, 0, 0, 0, 0, &res);

	*arg0 = res.a0;
	*arg1 = res.a1;
	*arg2 = res.a2;
	*arg3 = res.a3;

	return *arg0;
}

static int exynos_ssp_map_sfr(struct ssp_device *sspdev)
{
	int ret = SSP_RET_OK;

	pmu_va_base = ioremap(PMU_ALIVE_PA_BASE, SZ_4K);
	if (!pmu_va_base) {
		ssp_err(sspdev->dev, "%s: fail to ioremap\n", __func__);
		ret = SSP_RET_FAIL;
	}

	return ret;
}

static bool exynos_ssp_check_power_status(void)
{
	unsigned int reg;

	if (!pmu_va_base)
		return false;

	reg = readl(pmu_va_base + PMU_SSP_STATUS_VA_OFFSET);

	if (reg & PMU_SSP_STATUS_MASK)
		return true;
	else
		return false;
}

static void exynos_ssp_pm_enable(struct ssp_device *sspdev)
{
	pm_runtime_enable(sspdev->dev);
	ssp_info(sspdev->dev, "pm_runtime_enable\n");
}

static int exynos_ssp_power_on(struct ssp_device *sspdev)
{
	int ret = SSP_RET_OK;

	ret = pm_runtime_get_sync(sspdev->dev);
	if (ret != SSP_RET_OK)
		ssp_err(sspdev->dev, "%s: fail to pm_runtime_get_sync. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(sspdev->dev, "pm_runtime_get_sync done\n");

	if (exynos_ssp_check_power_status() == false) {
		ssp_err(sspdev->dev, "%s: ssp power status\n", __func__);
		return SSP_RET_FAIL;
	}

	return ret;
}

static int exynos_ssp_power_off(struct ssp_device *sspdev)
{
	int ret = SSP_RET_OK;

	ret = pm_runtime_put_sync(sspdev->dev);
	if (ret != SSP_RET_OK)
		ssp_err(sspdev->dev, "%s: fail to pm_runtime_put_sync. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(sspdev->dev, "pm_runtime_put_sync done\n");

	return ret;
}

static int exynos_ssp_boot(struct device *dev)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	uint64_t count;
	unsigned long flag;

	ssp_info(dev, "ssp boot start\n");

	exynos_update_ip_idle_status(ssp_idle_ip_index, 0);
	count = 0;

	do {
		count++;

		reg0 = SMC_CMD_SSP;
		reg1 = SSP_CMD_BOOT;
		reg2 = 0;
		reg3 = 0;

		spin_lock_irqsave(&ssp_lock, flag);
		ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
		spin_unlock_irqrestore(&ssp_lock, flag);

		if (ret == SSP_RET_BUSY)
			usleep_range(500, 1000);

	} while (ret == SSP_RET_BUSY);

	exynos_update_ip_idle_status(ssp_idle_ip_index, 1);

	if (ret != SSP_RET_OK)
		ssp_err(dev, "%s: fail to boot at ldfw. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(dev, "ssp boot done: %d\n", count);

	return ret;
}

static int exynos_ssp_backup(struct device *dev)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	unsigned long flag;

	ssp_info(dev, "ssp backup start\n");

	reg0 = SMC_CMD_SSP;
	reg1 = SSP_CMD_BACKUP;
	reg2 = 0;
	reg3 = 0;

	exynos_update_ip_idle_status(ssp_idle_ip_index, 0);

	spin_lock_irqsave(&ssp_lock, flag);
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	spin_unlock_irqrestore(&ssp_lock, flag);

	exynos_update_ip_idle_status(ssp_idle_ip_index, 1);

	if (ret != SSP_RET_OK)
		ssp_err(dev, "%s: fail to backup at ldfw. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(dev, "ssp backup done\n");

	return ret;
}

static int exynos_ssp_restore(struct device *dev)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	unsigned long flag;

	ssp_info(dev, "ssp restore start\n");

	reg0 = SMC_CMD_SSP;
	reg1 = SSP_CMD_RESTORE;
	reg2 = 0;
	reg3 = 0;

	exynos_update_ip_idle_status(ssp_idle_ip_index, 0);

	spin_lock_irqsave(&ssp_lock, flag);
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	spin_unlock_irqrestore(&ssp_lock, flag);

	exynos_update_ip_idle_status(ssp_idle_ip_index, 1);

	if (ret != SSP_RET_OK)
		ssp_err(dev, "%s: fail to restore at ldfw. ret = 0x%x\n", __func__, ret);
	else
		ssp_info(dev, "ssp restore done\n");

	return ret;
}

static int exynos_ssp_self_test(struct device *dev, uint64_t test_mode)
{
	int ret = SSP_RET_OK;
	uint64_t reg0;
	uint64_t reg1;
	uint64_t reg2;
	uint64_t reg3;
	unsigned long flag;

	ssp_info(dev, "call ssp function: %d\n", (int)test_mode);

	reg0 = SMC_CMD_SSP;
	reg1 = SSP_CMD_SELF_TEST;
	reg2 = test_mode;
	reg3 = 0;

	spin_lock_irqsave(&ssp_lock, flag);
	ret = exynos_cm_smc(&reg0, &reg1, &reg2, &reg3);
	spin_unlock_irqrestore(&ssp_lock, flag);

	ssp_info(dev, "return from ldfw: 0x%x\n", ret);

	return ret;
}

static void exynos_ssp_itmon_enable(struct device *dev, uint64_t enable)
{
	static int ssp_itmon_enable_flag = 1;
#if defined(CONFIG_SOC_EXYNOS9830)
	/* fixed from evt1.1 */
	if (!(exynos_soc_info.main_rev && exynos_soc_info.sub_rev))
		return;
#elif defined(CONFIG_SOC_EXYNOS2100)
	/* fixed from evt1.0 */
	if (exynos_soc_info.main_rev)
		return;
#endif
	if (enable) {
		if (ssp_itmon_enable_flag)
			return;

		ssp_info(dev, "enable itmon\n");
		itmon_wa_enable("SSP", M_NODE, true);
		itmon_wa_enable("BUS0_CORE", T_S_NODE, true);
		itmon_wa_enable("BUS0_CORE", T_M_NODE, true);
		itmon_wa_enable("ALIVE", S_NODE, true);
		itmon_wa_enable("BUS0_DP", M_NODE, true);
		itmon_wa_enable("BUS0_DP", S_NODE, true);
		itmon_wa_enable("BUS2_DP", T_M_NODE, true);
		itmon_wa_enable("BUS2_DP", T_S_NODE, true);

		ssp_itmon_enable_flag = 1;
	} else {
		if (!ssp_itmon_enable_flag)
			return;

		ssp_info(dev, "disable itmon\n");
		itmon_wa_enable("SSP", M_NODE, false);
		itmon_wa_enable("BUS0_CORE", T_S_NODE, false);
		itmon_wa_enable("BUS0_CORE", T_M_NODE, false);
		itmon_wa_enable("ALIVE", S_NODE, false);
		itmon_wa_enable("BUS0_DP", M_NODE, false);
		itmon_wa_enable("BUS0_DP", S_NODE, false);
		itmon_wa_enable("BUS2_DP", T_M_NODE, false);
		itmon_wa_enable("BUS2_DP", T_S_NODE, false);

		ssp_itmon_enable_flag = 0;
	}
}

static int exynos_ssp_enable(struct ssp_device *sspdev)
{
	int ret = SSP_RET_OK;
	static int ssp_boot_flag;

	++ssp_power_count;

	if (ssp_power_count == 1) {
		pm_stay_awake(sspdev->dev);

		ret = exynos_ssp_power_on(sspdev);
		if (unlikely(ret))
			goto ERR_OUT1;

		exynos_ssp_itmon_enable(sspdev->dev, 0);

		if (exynos_ssp_check_power_status() == false) {
			ssp_err(sspdev->dev, "%s: ssp power status\n", __func__);
			goto ERR_OUT2;
		}

		if (!ssp_boot_flag) {
			ret = exynos_ssp_boot(sspdev->dev);
			exynos_ssp_itmon_enable(sspdev->dev, 1);
			if (unlikely(ret))
				goto ERR_OUT2;
			ssp_boot_flag = 1;
		} else {
			ret = exynos_ssp_restore(sspdev->dev);
			exynos_ssp_itmon_enable(sspdev->dev, 1);
			if (unlikely(ret))
				goto ERR_OUT2;
		}
	}

	exynos_ssp_powerctl_update(sspdev->dev, 1, 1);
	ssp_info(sspdev->dev, "ssp enable: count: %d\n", ssp_power_count);

	return ret;

ERR_OUT2:
	exynos_ssp_power_off(sspdev);

ERR_OUT1:
	exynos_ssp_powerctl_update(sspdev->dev, 1, 0);
	pm_relax(sspdev->dev);
	--ssp_power_count;

	return ret;
}

static int exynos_ssp_disable(struct ssp_device *sspdev)
{
	int ret = SSP_RET_OK;

	if (ssp_power_count <= 0) {
		ssp_err(sspdev->dev, "%s ssp has already been disabled\n", __func__);
		ssp_err(sspdev->dev, "ssp disable: count: %d\n", ssp_power_count);
		return ret;
	}

	--ssp_power_count;

	if (ssp_power_count == 0) {
		ret = exynos_ssp_backup(sspdev->dev);
		if (unlikely(ret))
			goto ERR_OUT1;

		ret = exynos_ssp_power_off(sspdev);
		if (unlikely(ret))
			goto ERR_OUT2;

		/* keep the wake-up lock when above two functions fail */
		/* for debugging purpose */

		pm_relax(sspdev->dev);
	}

	exynos_ssp_powerctl_update(sspdev->dev, 0, 1);
	ssp_info(sspdev->dev, "ssp disable: count: %d\n", ssp_power_count);

	return ret;

ERR_OUT1:
	exynos_ssp_power_off(sspdev);

ERR_OUT2:
	exynos_ssp_powerctl_update(sspdev->dev, 0, 0);
	pm_relax(sspdev->dev);

	return ret;
}

static long ssp_ioctl(struct file *filp, unsigned int cmd, unsigned long __arg)
{
	int ret = SSP_RET_OK;
	uint64_t test_mode;
	char path[SSP_MAX_USER_PATH_LEN];

	struct ssp_device *sspdev = filp->private_data;
	void __user *arg = (void __user *)__arg;

	ret = exynos_ssp_get_path(sspdev->dev, current, path);
	if (ret) {
		ssp_err(sspdev->dev, "%s: fail to get user path. ret = 0x%x\n", __func__, ret);
		return ret;
	}

	ssp_info(sspdev->dev, "requested by %s\n", path);

	mutex_lock(&ssp_ioctl_lock);

	switch (cmd) {
	case SSP_IOCTL_INIT:
		ret = exynos_ssp_enable(sspdev);
		if (ret != SSP_RET_OK)
			exynos_ssp_print_user_list(sspdev->dev);
		break;
	case SSP_IOCTL_EXIT:
		ret = exynos_ssp_disable(sspdev);
		if (ret != SSP_RET_OK)
			exynos_ssp_print_user_list(sspdev->dev);
		break;
	case SSP_IOCTL_TEST:
		ret = get_user(test_mode, (uint64_t __user *)arg);
		if (unlikely(ret)) {
			ssp_err(sspdev->dev, "%s: fail to get_user. ret = 0x%x\n", __func__, ret);
			break;
		}
		ret = exynos_ssp_self_test(sspdev->dev, test_mode);
		break;
	case SSP_IOCTL_DEBUG:
		ssp_info(sspdev->dev, "power-on count: %d\n", ssp_power_count);
		exynos_ssp_print_user_list(sspdev->dev);
		break;
	default:
		ssp_err(sspdev->dev, "%s: invalid ioctl cmd: 0x%x\n", __func__, cmd);
		ret = -EPERM;
		break;
	}

	mutex_unlock(&ssp_ioctl_lock);

	return ret;
}

static int ssp_open(struct inode *inode, struct file *file)
{
	struct miscdevice *misc = file->private_data;
	struct ssp_device *sspdev = container_of(misc,
			struct ssp_device, misc_device);

	file->private_data = sspdev;

	ssp_info(sspdev->dev, "driver open is done\n");

	return 0;
}

static const struct file_operations ssp_fops = {
	.owner		= THIS_MODULE,
	.open		= ssp_open,
	.unlocked_ioctl	= ssp_ioctl,
	.compat_ioctl	= ssp_ioctl,
};

static int exynos_ssp_probe(struct platform_device *pdev)
{
	int ret = SSP_RET_OK;
	struct ssp_device *sspdev = NULL;

	sspdev = kzalloc(sizeof(struct ssp_device), GFP_KERNEL);
	if (!sspdev) {
		ssp_err(&pdev->dev, "%s: fail to kzalloc.\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, sspdev);
	sspdev->dev = &pdev->dev;

	spin_lock_init(&ssp_lock);
	mutex_init(&ssp_ioctl_lock);

	exynos_ssp_map_sfr(sspdev);

	/* enable runtime PM */
	exynos_ssp_pm_enable(sspdev);
	ssp_idle_ip_index = exynos_get_idle_ip_index(dev_name(sspdev->dev), 1);
	exynos_update_ip_idle_status(ssp_idle_ip_index, 1);

	/* set misc driver */
	memset((void *)&sspdev->misc_device, 0, sizeof(struct miscdevice));
	sspdev->misc_device.minor = MISC_DYNAMIC_MINOR;
	sspdev->misc_device.name = "ssp";
	sspdev->misc_device.fops = &ssp_fops;
	ret = misc_register(&sspdev->misc_device);
	if (ret) {
		ssp_err(sspdev->dev, "%s: fail to misc_register. ret = %d\n", __func__, ret);
		ret = -ENOMEM;
		goto err;
	}

	ret = device_init_wakeup(sspdev->dev, true);
	if (ret) {
		ssp_err(sspdev->dev, "%s: fail to init wakeup. ret = %d\n", __func__, ret);
		goto err;
	}

	return SSP_RET_OK;
err:
	kfree(sspdev);

	return ret;
}

static int exynos_ssp_remove(struct platform_device *pdev)
{
	struct ssp_device *sspdev = platform_get_drvdata(pdev);

	misc_deregister(&sspdev->misc_device);
	kfree(sspdev);

	return 0;
}

#if defined(CONFIG_PM_SLEEP) || defined(CONFIG_PM_RUNTIME)
static int exynos_ssp_suspend(struct device *dev)
{
	int ret = SSP_RET_OK;

	/* Do nothing */

	return ret;
}

static int exynos_ssp_resume(struct device *dev)
{
	int ret = SSP_RET_OK;

	/* Do nothing */

	return ret;
}
#endif

static struct dev_pm_ops exynos_ssp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(exynos_ssp_suspend,
			exynos_ssp_resume)
};

#ifdef CONFIG_OF
static const struct of_device_id exynos_ssp_match[] = {
	{
		.compatible = "samsung,exynos-ssp",
	},
	{},
};
#endif

static struct platform_driver exynos_ssp_driver = {
	.probe		= exynos_ssp_probe,
	.remove		= exynos_ssp_remove,
	.driver		= {
		.name	= "ssp",
		.owner	= THIS_MODULE,
		.pm     = &exynos_ssp_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = exynos_ssp_match,
#endif
	},
};

static int __init exynos_ssp_init(void)
{
	int ret = SSP_RET_OK;

	ret = platform_driver_register(&exynos_ssp_driver);
	if (ret) {
		pr_err("[Exynos][CMSSP] %s: fail to register driver. ret = 0x%x\n", __func__, ret);
		return ret;
	}

	pr_info("[Exynos][CMSSP] driver, (c) 2019 Samsung Electronics\n");

	return 0;
}

static void __exit exynos_ssp_exit(void)
{
	platform_driver_unregister(&exynos_ssp_driver);
}

module_init(exynos_ssp_init);
module_exit(exynos_ssp_exit);

MODULE_DESCRIPTION("EXYNOS Samsung Secure Platform driver");
MODULE_AUTHOR("Keunyoung Park <keun0.park@samsung.com>");
MODULE_LICENSE("GPL");
