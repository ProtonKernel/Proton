// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/string.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/memblock.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/rwsem.h>
#include <linux/sysfs.h>
#include <linux/err.h>
#include <linux/idr.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <asm/setup.h>

#include <soc/samsung/imgloader.h>

#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
#include <soc/samsung/exynos-s2mpu.h>
#endif

#define IMGLOADER_NUM_DESC	36

#define imgloader_err(desc, fmt, ...)						\
	dev_err(desc->dev, "%s: " fmt, desc->name, ##__VA_ARGS__)
#define imgloader_info(desc, fmt, ...)						\
	dev_info(desc->dev, "%s: " fmt, desc->name, ##__VA_ARGS__)

/**
 * struct imgloader_priv - Private state for a imgloader_desc
 * @desc: pointer to imgloader_desc this is private data for
 * @fw_phys_base_addr: physical address where processor starts booting at
 * @fw_bin_size: firmware binary size
 * @fw_mem_size: firmware used size
 *
 * This struct contains data for a imgloader_desc that should not be exposed outside
 * of this file. This structure points to the descriptor and the descriptor
 * pointr to this structure so that imgloader drivers can't access the private
 * data of a descriptor but this file can access both.
 */
struct imgloader_priv {
	struct imgloader_desc *desc;
	phys_addr_t fw_phys_base;
	size_t fw_bin_size;
	size_t fw_mem_size;
	int id;
};

/**
 * imgloader_get_phys_base - Retrieve the physical base address of a peripheral image
 * @desc: descriptor from imgloader_desc_init()
 *
 * Returns the physical address where the image boots at or 0 if unknown.
 */
phys_addr_t imgloader_get_phys_base(struct imgloader_desc *desc)
{
	return desc->priv ? desc->priv->fw_phys_base : 0;
}
EXPORT_SYMBOL(imgloader_get_phys_base);

static int imgloader_parse_dt(struct imgloader_desc *desc)
{
	struct device_node *ofnode = desc->dev->of_node;

	if (!ofnode)
		return -EINVAL;

	desc->s2mpu_support = of_property_read_bool(ofnode,
				"samsung,imgloader-s2mpu-support");
	return 0;
}

static int imgloader_notify(struct imgloader_desc *desc, char *status)
{
	if (!desc->notify_signal)
		return 0;

	return 0;
}

static int imgloader_allow_permission(struct imgloader_desc *desc)
{
	uint64_t ret = 0;

#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
	ret = exynos_request_fw_stage2_ap(desc->name);
#endif
	return (int)ret;
}

static int imgloader_verify_fw(struct imgloader_desc *desc)
{
	struct imgloader_priv *priv = desc->priv;
	uint64_t ret = 0;

#if IS_ENABLED(CONFIG_EXYNOS_S2MPU)
	ret = exynos_verify_subsystem_fw(desc->name,
					  desc->fw_id,
					  priv->fw_phys_base,
					  priv->fw_bin_size,
					  priv->fw_mem_size);
#endif
	return (int)ret;
}

/**
 * imgloader_boot() - Load a peripheral image into memory and boot it
 * @desc: descriptor from imgloader_desc_init()
 *
 * Returns 0 on success or -ERROR on failure.
 */
int imgloader_boot(struct imgloader_desc *desc)
{
	int ret;
	const struct firmware *fw;
	struct imgloader_priv *priv = desc->priv;
	size_t fw_bin_size = 0;
	size_t fw_mem_size = 0;
	phys_addr_t fw_phys_base = 0;
	size_t fw_size = 0;
	const u8 *fw_data = NULL;

	/* Notify to other device */
	ret = imgloader_notify(desc, "on");
	if (ret < 0) {
		imgloader_err(desc, "Failed to noify - rc:%d\n", ret);
		return ret;
	}

	/* Warning when Subsystem shutdown is failed previously */
	if (desc->shutdown_fail)
		imgloader_err(desc, "shutdown failed previously!\n");

	/* 1 Step - request firmware */
	if (!desc->skip_request_firmware) {
		ret = request_firmware(&fw, desc->fw_name, desc->dev);
		if (ret) {
			imgloader_err(desc, "Failed to locate %s(rc:%d)\n",
						desc->fw_name, ret);
			goto out;
		}
		fw_size = fw->size;
		fw_data = fw->data;
	}

	/*
	 * 2nd Step - memcpy to own memory area, and
	 * then it must be returned their memory area.
	 */
	if (desc->ops->mem_setup)
		ret = desc->ops->mem_setup(desc,
					   fw_data,
					   fw_size,
					   &fw_phys_base,
					   &fw_bin_size,
					   &fw_mem_size);
	if (ret) {
		imgloader_err(desc,
			"firmware memory setup failed(rc:%d)\n", ret);
		goto err_boot;
	}

	if (!fw_phys_base || !fw_bin_size | !fw_mem_size) {
		imgloader_err(desc,
			"Failed to return fw address/size\n");
		goto err_boot;
	}

	priv->fw_phys_base = fw_phys_base;
	priv->fw_bin_size = fw_bin_size;
	priv->fw_mem_size = fw_mem_size;

	/* 3rd Step - authentication & allow permission by S2MPU or NOT */
	if (desc->s2mpu_support) {
		ret = imgloader_verify_fw(desc);
		if (ret) {
			imgloader_err(desc,
				"Failed to verify firmare (rc:%d)\n", ret);
			goto err_deinit_image;
		}

		if (desc->ops->blk_pwron) {
			ret = desc->ops->blk_pwron(desc);
			if (ret) {
				imgloader_err(desc,
					"Failed to blk pwr on (rc:%d)\n",
									ret);
				goto err_deinit_image;
			}
		}
		ret = imgloader_allow_permission(desc);
		if (ret) {
			imgloader_err(desc,
				"Failed to allow permission (rc:%d)\n", ret);
			goto err_deinit_image;
		}
	} else {
		if (desc->ops->verify_fw) {
			ret = desc->ops->verify_fw(desc,
						priv->fw_phys_base,
						priv->fw_bin_size,
						priv->fw_mem_size);
			if (ret) {
				imgloader_err(desc,
					"Failed to verify firmare (rc:%d)\n",
					ret);
				goto err_deinit_image;
			}
		}
	}

	/* 4th Step - init image (Run system */
	if (desc->ops->init_image)
		ret = desc->ops->init_image(desc);
	if (ret) {
		imgloader_err(desc, "Initializing image failed(rc:%d)\n", ret);
		goto err_deinit_image;
	}
err_deinit_image:
	if (ret && desc->ops->deinit_image)
		desc->ops->deinit_image(desc);
err_boot:
	if (!desc->skip_request_firmware)
		release_firmware(fw);
out:
	if (!ret)
		imgloader_notify(desc, "off");

	return ret;
}
EXPORT_SYMBOL(imgloader_boot);

/**
 * imgloader_shutdown() - Shutdown a peripheral
 * @desc: descriptor from imgloader_desc_init()
 */
void imgloader_shutdown(struct imgloader_desc *desc)
{
	int ret;

	if (desc->ops->shutdown) {
		if (desc->ops->shutdown(desc))
			desc->shutdown_fail = true;
		else
			desc->shutdown_fail = false;
	}

	ret = imgloader_notify(desc, "off");
	if (ret < 0)
		imgloader_err(desc,
			"failed to send OFF message to AOP rc:%d\n", ret);
}
EXPORT_SYMBOL(imgloader_shutdown);

static DEFINE_IDA(imgloader_ida);

/**
 * imgloader_desc_init() - Initialize a imgloader descriptor
 * @desc: descriptor to initialize
 *
 * Initialize a imgloader descriptor for use by other imgloader functions. This function
 * must be called before calling imgloader_boot() or imgloader_shutdown().
 *
 * Returns 0 for success and -ERROR on failure.
 */
int imgloader_desc_init(struct imgloader_desc *desc)
{
	struct imgloader_priv *priv;
	int ret;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	desc->priv = priv;
	priv->desc = desc;

	priv->id = ret = ida_simple_get(&imgloader_ida,
				0, IMGLOADER_NUM_DESC, GFP_KERNEL);
	if (priv->id < 0)
		goto err;

	ret = imgloader_parse_dt(desc);
	if (ret)
		goto err_parse_dt;

	return 0;
err_parse_dt:
	ida_simple_remove(&imgloader_ida, priv->id);
err:
	kfree(priv);
	return ret;
}
EXPORT_SYMBOL(imgloader_desc_init);

/**
 * imgloader_desc_release() - Release a imgloader descriptor
 * @desc: descriptor to free
 */
void imgloader_desc_release(struct imgloader_desc *desc)
{
	struct imgloader_priv *priv = desc->priv;

	desc->priv = NULL;
	kfree(priv);
}
EXPORT_SYMBOL(imgloader_desc_release);

static int __init imgloader_init(void)
{
	return 0;
}
subsys_initcall(imgloader_init);

MODULE_AUTHOR("Hosung Kim <hosung0.kim@samsung.com>");
MODULE_AUTHOR("Changki Kim <changki.kim@samsung.com>");
MODULE_AUTHOR("Donghyeok Choe <d7271.choe@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Image Loader");
