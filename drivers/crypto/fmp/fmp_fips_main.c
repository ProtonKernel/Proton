/*
 * Exynos FMP test driver
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/miscdevice.h>
#include <linux/crypto.h>
#include <linux/buffer_head.h>
#include <linux/genhd.h>
#include <linux/delay.h>

#include <crypto/authenc.h>
#include <crypto/fmp.h>

#if defined(CONFIG_EXYNOS_FMP_FIPS_FUNC_TEST)
#include "fmp_fips_func_test.h"
#endif
#include "fmp_fips_main.h"
#include "fmp_fips_fops.h"
#include "fmp_fips_selftest.h"
#include "fmp_fips_integrity.h"
#include "fmp_test.h"

static const char pass[] = "passed";
static const char fail[] = "failed";

enum fips_state {
	FMP_FIPS_INIT_STATE,
	FMP_FIPS_PROGRESS_STATE,
	FMP_FIPS_ERR_STATE,
	FMP_FIPS_SUCCESS_STATE
};

static enum fips_state fmp_fips_state = FMP_FIPS_INIT_STATE;

bool in_fmp_fips_err(void)
{
	if (fmp_fips_state == FMP_FIPS_INIT_STATE ||
	    fmp_fips_state == FMP_FIPS_ERR_STATE)
		return true;
	return false;
}

bool in_fmp_fips_init(void)
{
	if (fmp_fips_state == FMP_FIPS_INIT_STATE)
		return true;
	return false;
}

static void set_fmp_fips_state(uint32_t val)
{
	fmp_fips_state = val;
}

static ssize_t fmp_fips_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct exynos_fmp *fmp = dev_get_drvdata(dev);

	return snprintf(buf, sizeof(pass), "%s\n", fmp->result.overall ? pass : fail);
}

static ssize_t aes_xts_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct exynos_fmp *fmp = dev_get_drvdata(dev);

	return snprintf(buf, sizeof(pass), "%s\n", fmp->result.aes_xts ? pass : fail);
}

static ssize_t aes_cbc_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct exynos_fmp *fmp = dev_get_drvdata(dev);

	return snprintf(buf, sizeof(pass), "%s\n", fmp->result.aes_cbc ? pass : fail);
}

static ssize_t sha256_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct exynos_fmp *fmp = dev_get_drvdata(dev);

	return snprintf(buf, sizeof(pass), "%s\n", fmp->result.sha256 ? pass : fail);
}

static ssize_t hmac_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct exynos_fmp *fmp = dev_get_drvdata(dev);

	return snprintf(buf, sizeof(pass), "%s\n", fmp->result.hmac ? pass : fail);
}

static ssize_t integrity_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct exynos_fmp *fmp = dev_get_drvdata(dev);

	return snprintf(buf, sizeof(pass), "%s\n", fmp->result.integrity ? pass : fail);
}

static ssize_t fmp_fips_run_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	static bool run;

	if (!run) {
		struct exynos_fmp *fmp = dev_get_drvdata(dev);
		exynos_fmp_fips_test(fmp);
		run = 1;
	}
	return count;
}

static DEVICE_ATTR_RO(fmp_fips_status);
static DEVICE_ATTR_RO(aes_xts_status);
static DEVICE_ATTR_RO(aes_cbc_status);
static DEVICE_ATTR_RO(sha256_status);
static DEVICE_ATTR_RO(hmac_status);
static DEVICE_ATTR_RO(integrity_status);
static DEVICE_ATTR_WO(fmp_fips_run);

static struct attribute *fmp_fips_attr[] = {
	&dev_attr_fmp_fips_status.attr,
	&dev_attr_aes_xts_status.attr,
	&dev_attr_aes_cbc_status.attr,
	&dev_attr_sha256_status.attr,
	&dev_attr_hmac_status.attr,
	&dev_attr_integrity_status.attr,
	&dev_attr_fmp_fips_run.attr,
	NULL,
};

static struct attribute_group fmp_fips_attr_group = {
	.name	= "fmp-fips",
	.attrs	= fmp_fips_attr,
};

static const struct file_operations fmp_fips_fops = {
	.owner		= THIS_MODULE,
	.open		= fmp_fips_open,
	.release	= fmp_fips_release,
	.unlocked_ioctl = fmp_fips_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= fmp_fips_compat_ioctl,
#endif
};

int exynos_fmp_fips_register(struct exynos_fmp *fmp)
{
	int ret;

	if (!fmp || !fmp->dev) {
		pr_err("%s: Invalid exynos fmp dev\n", __func__);
		goto err;
	}

	set_fmp_fips_state(FMP_FIPS_INIT_STATE);

	fmp->miscdev.minor = MISC_DYNAMIC_MINOR;
	fmp->miscdev.name = "fmp";
	fmp->miscdev.fops = &fmp_fips_fops;
	ret = misc_register(&fmp->miscdev);
	if (ret) {
		dev_err(fmp->dev, "%s: Fail to register misc device. ret(%d)\n",
				__func__, ret);
		goto err;
	}

	ret = sysfs_create_group(&fmp->dev->kobj, &fmp_fips_attr_group);
	if (ret) {
		dev_err(fmp->dev, "%s: Fail to create sysfs. ret(%d)\n",
				__func__, ret);
		goto err_misc;
	}

	dev_info(fmp->dev, "%s: FMP register misc device. ret(%d)\n",
			__func__, ret);
	return 0;

err_misc:
	misc_deregister(&fmp->miscdev);
err:
	return -EINVAL;
}

void exynos_fmp_fips_test(struct exynos_fmp *fmp)
{
#if defined(CONFIG_EXYNOS_FMP_FIPS_FUNC_TEST)
	exynos_fmp_func_test_KAT_case(fmp);
#endif
	exynos_fmp_fips_init(fmp);
}

int exynos_fmp_fips_init(struct exynos_fmp *fmp)
{
	int ret = 0;

	if (!fmp || !fmp->dev) {
		pr_err("%s: Invalid exynos fmp dev\n", __func__);
		goto err;
	}

	dev_info(fmp->dev, "%s: Started!!\n", __func__);
	set_fmp_fips_state(FMP_FIPS_PROGRESS_STATE);

	fmp->test_data = fmp_test_init(fmp);
	if (!fmp->test_data) {
		dev_err(fmp->dev,
			"%s: fails to initialize fips test.\n", __func__);
		goto err;
	}

	ret = do_fmp_selftest(fmp);
	if (ret) {
		dev_err(fmp->dev, "%s: self-tests for FMP failed\n", __func__);
		goto err_data;
	} else {
		dev_info(fmp->dev, "%s: self-tests for FMP passed\n", __func__);
	}

	ret = do_fmp_integrity_check(fmp);
	if (ret) {
		dev_err(fmp->dev, "%s: integrity check for FMP failed\n", __func__);
		fmp->result.integrity = 0;
		goto err_data;
	} else {
		dev_info(fmp->dev, "%s: integrity check for FMP passed\n", __func__);
		fmp->result.integrity = 1;
	}

	set_fmp_fips_state(FMP_FIPS_SUCCESS_STATE);
	fmp->result.overall = 1;
	fmp_test_exit(fmp->test_data);
	return 0;
err_data:
#if defined(CONFIG_NODE_FOR_SELFTEST_FAIL)
	set_fmp_fips_state(FMP_FIPS_ERR_STATE);
	fmp->result.overall = 0;
	fmp_test_exit(fmp->test_data);
	return 0;
#else
	// return 0 if KAT function test mode
	if (fmp->test_vops) {
		set_fmp_fips_state(FMP_FIPS_ERR_STATE);
		fmp->result.overall = 0;
		fmp_test_exit(fmp->test_data);
		return 0;
	}
	else {
		panic("%s: Panic due to FMP self test for FIPS KAT", __func__);
	}
#endif
err:
	set_fmp_fips_state(FMP_FIPS_ERR_STATE);
	return -EINVAL;
}

void exynos_fmp_fips_deregister(struct exynos_fmp *fmp)
{
	sysfs_remove_group(&fmp->dev->kobj, &fmp_fips_attr_group);
	misc_deregister(&fmp->miscdev);
}
