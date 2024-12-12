/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - CHIP ID support
 * Author: Pankaj Dubey <pankaj.dubey@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sys_soc.h>
#include <linux/soc/samsung/exynos-soc.h>
#include <linux/module.h>

struct exynos_chipid_info exynos_soc_info;
EXPORT_SYMBOL(exynos_soc_info);

static const char * product_id_to_name(unsigned int product_id)
{
	const char *soc_name;
	unsigned int soc_id = product_id;

	switch (soc_id) {
	case EXYNOS3250_SOC_ID:
		soc_name = "EXYNOS3250";
		break;
	case EXYNOS4210_SOC_ID:
		soc_name = "EXYNOS4210";
		break;
	case EXYNOS4212_SOC_ID:
		soc_name = "EXYNOS4212";
		break;
	case EXYNOS4412_SOC_ID:
		soc_name = "EXYNOS4412";
		break;
	case EXYNOS4415_SOC_ID:
		soc_name = "EXYNOS4415";
		break;
	case EXYNOS5250_SOC_ID:
		soc_name = "EXYNOS5250";
		break;
	case EXYNOS5260_SOC_ID:
		soc_name = "EXYNOS5260";
		break;
	case EXYNOS5420_SOC_ID:
		soc_name = "EXYNOS5420";
		break;
	case EXYNOS5440_SOC_ID:
		soc_name = "EXYNOS5440";
		break;
	case EXYNOS5800_SOC_ID:
		soc_name = "EXYNOS5800";
		break;
	case EXYNOS7872_SOC_ID:
		soc_name = "EXYNOS7872";
		break;
	case EXYNOS8890_SOC_ID:
		soc_name = "EXYNOS8890";
		break;
	case EXYNOS8895_SOC_ID:
		soc_name = "EXYNOS8895";
		break;
	case EXYNOS9810_SOC_ID:
		soc_name = "EXYNOS9810";
		break;
	case EXYNOS9820_SOC_ID:
		soc_name = "EXYNOS9820";
		break;
	case EXYNOS9830_SOC_ID:
		soc_name = "EXYNOS9830";
		break;
	case EXYNOS2100_SOC_ID:
		soc_name = "EXYNOS2100";
		break;
	default:
		soc_name = "UNKNOWN";
	}
	return soc_name;
}
static const struct exynos_chipid_variant drv_data_exynos8890 = {
	.product_ver	= 1,
	.unique_id_reg	= 0x14,
	.rev_reg	= 0x0,
	.main_rev_bit	= 0,
	.sub_rev_bit	= 4,
};

static const struct exynos_chipid_variant drv_data_exynos8895 = {
	.product_ver	= 1,
	.unique_id_reg	= 0x04,
	.rev_reg	= 0x10,
	.main_rev_bit	= 20,
	.sub_rev_bit	= 16,
};

static const struct exynos_chipid_variant drv_data_exynos7872 = {
	.product_ver	= 2,
	.unique_id_reg	= 0x04,
	.rev_reg	= 0x10,
	.main_rev_bit	= 20,
	.sub_rev_bit	= 16,
};

static const struct exynos_chipid_variant drv_data_exynos9810 = {
	.product_ver	= 1,
	.unique_id_reg	= 0x04,
	.rev_reg	= 0x10,
	.main_rev_bit	= 20,
	.sub_rev_bit	= 16,
};

static const struct exynos_chipid_variant drv_data_exynos9830 = {
	.product_ver	= 1,
	.unique_id_reg	= 0x04,
	.rev_reg	= 0x10,
	.main_rev_bit	= 20,
	.sub_rev_bit	= 16,
};

static const struct exynos_chipid_variant drv_data_exynos2100 = {
	.product_ver	= 1,
	.unique_id_reg	= 0x04,
	.rev_reg	= 0x10,
	.main_rev_bit	= 20,
	.sub_rev_bit	= 16,
};

static const struct of_device_id of_exynos_chipid_ids[] = {
	{
		.compatible	= "samsung,exynos8890-chipid",
		.data		= &drv_data_exynos8890,
	},
	{
		.compatible	= "samsung,exynos8895-chipid",
		.data		= &drv_data_exynos8895,
	},
	{
		.compatible	= "samsung,exynos7872-chipid",
		.data		= &drv_data_exynos8895,
	},
	{
		.compatible	= "samsung,exynos9810-chipid",
		.data		= &drv_data_exynos9810,
	},
	{
		.compatible	= "samsung,exynos9830-chipid",
		.data		= &drv_data_exynos9830,
	},
	{
		.compatible	= "samsung,exynos2100-chipid",
		.data		= &drv_data_exynos2100,
	},
	{},
};

static char lot_id[6];

static u32 chipid_reverse_value(u32 val, u32 bitcnt)
{
	u32 temp, ret = 0;
	u32 i;

	for (i = 0; i < bitcnt; i++) {
		temp = (val >> i) & 0x1;
		ret += temp << ((bitcnt - 1) - i);
	}

	return ret;
}

static void chipid_dec_to_36(u32 in, char *p)
{
	const struct exynos_chipid_variant *data = exynos_soc_info.drv_data;

	u32 mod;
	u32 i;
	u32 val;

	for (i = 4; i >= 1; i--) {
		mod = in % 36;
		in /= 36;
		p[i] = (mod < 10) ? (mod + '0') : (mod - 10 + 'A');
	}

	val = __raw_readl(exynos_soc_info.reg + data->unique_id_reg + 0x4);
	val = (val >> 10) & 0x3;

	switch (val) {
	case 0:
		p[0] = 'N';
		break;
	case 1:
		p[0] = 'S';
		break;
	case 2:
		p[0] = 'A';
		break;
	case 3:
	default:
		break;
	}

	p[5] = 0;
}

/*
 *  sysfs implementation for exynos-snapshot
 *  you can access the sysfs of exynos-snapshot to /sys/devices/system/chip-id
 *  path.
 */
static struct bus_type chipid_subsys = {
	.name = "chip-id",
	.dev_name = "chip-id",
};

static ssize_t product_id_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%08X\n", exynos_soc_info.product_id);
}

static ssize_t unique_id_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%010LX\n", exynos_soc_info.unique_id);
}

static ssize_t lot_id_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 14, "%08X\n", exynos_soc_info.lot_id);
}

static ssize_t lot_id2_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 14, "%s\n", exynos_soc_info.lot_id2);
}

static ssize_t revision_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 14, "%08X\n", exynos_soc_info.revision);
}

static ssize_t evt_ver_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	if (exynos_soc_info.revision == 0)
		return snprintf(buf, 14, "EVT0\n");
	else
		return snprintf(buf, 14, "EVT%1X.%1X\n",
				exynos_soc_info.main_rev,
				exynos_soc_info.sub_rev);
}

static DEVICE_ATTR_RO(product_id);
static DEVICE_ATTR_RO(unique_id);
static DEVICE_ATTR_RO(lot_id);
static DEVICE_ATTR_RO(lot_id2);
static DEVICE_ATTR_RO(revision);
static DEVICE_ATTR_RO(evt_ver);

static struct attribute *chipid_sysfs_attrs[] = {
	&dev_attr_product_id.attr,
	&dev_attr_unique_id.attr,
	&dev_attr_lot_id.attr,
	&dev_attr_lot_id2.attr,
	&dev_attr_revision.attr,
	&dev_attr_evt_ver.attr,
	NULL,
};

static struct attribute_group chipid_sysfs_group = {
	.attrs = chipid_sysfs_attrs,
};

static const struct attribute_group *chipid_sysfs_groups[] = {
	&chipid_sysfs_group,
	NULL,
};

static ssize_t SVC_AP_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 20, "%010llX\n",
			(exynos_soc_info.unique_id));
}

static DEVICE_ATTR_RO(SVC_AP);

static struct attribute *svc_ap_attrs[] = {
	&dev_attr_SVC_AP.attr,
	NULL,
};

static struct attribute_group svc_ap_group = {
	.attrs = svc_ap_attrs,
};

static const struct attribute_group *svc_ap_groups[] = {
	&svc_ap_group,
	NULL,
};

static void svc_ap_release(struct device *dev)
{
	kfree(dev);
}

static int sysfs_create_svc_ap(void)
{
	struct kernfs_node *svc_sd;
	struct kobject *data;
	struct kobject *top_kobj = NULL;
	struct device *dev;
	int err;

	/* To find svc kobject */
	top_kobj = &exynos_soc_info.pdev->dev.kobj.kset->kobj;
	if (!top_kobj) {
		pr_err("No exynos_soc kobject\n");
		return -ENODEV;
	}
		
	svc_sd = sysfs_get_dirent(top_kobj->sd, "svc");
	if (IS_ERR_OR_NULL(svc_sd)) {
		/* try to create svc kobject */
		data = kobject_create_and_add("svc", top_kobj);
		if (IS_ERR_OR_NULL(data))
			pr_info("Existing path /sys/devices/svc : 0x%pK\n", data);
		else
			pr_info("Created /sys/devices/svc svc : 0x%pK\n", data);
	} else {
		data = (struct kobject *)svc_sd->priv;
		pr_info("Found svc_sd : 0x%pK svc : 0x%pK\n", svc_sd, data);
	}

	dev = kzalloc(sizeof(struct device), GFP_KERNEL);
	if (!dev) {
		pr_err("Error allocating svc_ap device\n");
		return -ENOMEM;
	}

	err = dev_set_name(dev, "AP");
	if (err < 0)
		goto err_name;

	dev->kobj.parent = data;
	dev->groups = svc_ap_groups;
	dev->release = svc_ap_release;

	err = device_register(dev);
	if (err < 0)
		goto err_dev_reg;

	exynos_soc_info.svc_dev = dev;
	return 0;

err_dev_reg:
	put_device(dev);
err_name:
	kfree(dev);
	dev = NULL;
	return err;
}

static int chipid_sysfs_init(void)
{
	int ret = 0;

	ret = subsys_system_register(&chipid_subsys, chipid_sysfs_groups);
	if (ret) {
		if (exynos_soc_info.pdev) {
			dev_err(&exynos_soc_info.pdev->dev,
				"fail to register chip-id subsys\n");
		} else {
			pr_err("fail to register chip-id subsys\n");
		}
	}

	ret = sysfs_create_svc_ap();
	if (ret)
		pr_err("fail to create svc_ap sysfs\n");

	return ret;
}

static void exynos_chipid_get_chipid_info(void)
{
	const struct exynos_chipid_variant *data = exynos_soc_info.drv_data;
	u64 val;
	u32 temp;

	val = __raw_readl(exynos_soc_info.reg);

	switch (data->product_ver) {
	case 2:
		exynos_soc_info.product_id = val & EXYNOS_SOC_MASK_V2;
		break;
	case 1:
	default:
		exynos_soc_info.product_id = val & EXYNOS_SOC_MASK;
		break;
	}

	val = __raw_readl(exynos_soc_info.reg + data->rev_reg);
	exynos_soc_info.main_rev = (val >> data->main_rev_bit) & EXYNOS_REV_MASK;
	exynos_soc_info.sub_rev = (val >> data->sub_rev_bit) & EXYNOS_REV_MASK;
	exynos_soc_info.revision = (exynos_soc_info.main_rev << 4) | exynos_soc_info.sub_rev;

	val = __raw_readl(exynos_soc_info.reg + data->unique_id_reg);
	val |= (u64)__raw_readl(exynos_soc_info.reg + data->unique_id_reg + 4) << 32UL;
	exynos_soc_info.unique_id  = val;
	exynos_soc_info.lot_id = val & EXYNOS_LOTID_MASK;

	temp = __raw_readl(exynos_soc_info.reg + data->unique_id_reg);
	temp = chipid_reverse_value(temp, 32);
	temp = (temp >> 11) & EXYNOS_LOTID_MASK;
	chipid_dec_to_36(temp, lot_id);
	exynos_soc_info.lot_id2 = lot_id;
}

/**
 *  exynos_chipid_early_init: Early chipid initialization
 *  @dev: pointer to chipid device
 */
void exynos_chipid_early_init(void)
{
	struct device_node *np;
	const struct of_device_id *match;

	if (exynos_soc_info.reg)
		return;

	np = of_find_matching_node_and_match(NULL, of_exynos_chipid_ids, &match);
	if (!np || !match)
		panic("%s, failed to find chipid node or match\n", __func__);

	exynos_soc_info.drv_data = (struct exynos_chipid_variant *)match->data;
	exynos_soc_info.reg = of_iomap(np, 0);
	if (!exynos_soc_info.reg)
		panic("%s: failed to map registers\n", __func__);

	exynos_chipid_get_chipid_info();
}

static int exynos_chipid_probe(struct platform_device *pdev)
{
	struct soc_device_attribute *soc_dev_attr;
	struct soc_device *soc_dev;
	struct device_node *root;
	int ret;

	exynos_chipid_early_init();
	soc_dev_attr = kzalloc(sizeof(*soc_dev_attr), GFP_KERNEL);
	if (!soc_dev_attr)
		return -ENODEV;

	soc_dev_attr->family = "Samsung Exynos";

	root = of_find_node_by_path("/");
	ret = of_property_read_string(root, "model", &soc_dev_attr->machine);
	of_node_put(root);
	if (ret)
		goto free_soc;

	soc_dev_attr->revision = kasprintf(GFP_KERNEL, "%d",
			exynos_soc_info.revision);
	if (!soc_dev_attr->revision)
		goto free_soc;

	soc_dev_attr->soc_id = product_id_to_name(exynos_soc_info.product_id);
	soc_dev = soc_device_register(soc_dev_attr);
	if (IS_ERR(soc_dev))
		goto free_rev;

//	dev_set_socdata(&pdev->dev, "Exynos", "ChipID");
	dev_info(&pdev->dev, "CPU[%s] CPU_REV[0x%x] Detected\n",
			product_id_to_name(exynos_soc_info.product_id),
			exynos_soc_info.revision);
	exynos_soc_info.pdev = pdev;

	chipid_sysfs_init();
	return 0;
free_rev:
	kfree(soc_dev_attr->revision);
free_soc:
	kfree(soc_dev_attr);
	return -EINVAL;
}

static struct platform_driver exynos_chipid_driver = {
	.driver = {
		.name = "exynos-chipid",
		.of_match_table = of_exynos_chipid_ids,
	},
	.probe = exynos_chipid_probe,
};

module_platform_driver(exynos_chipid_driver);

MODULE_DESCRIPTION("Exynos ChipID driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:exynos-chipid");
