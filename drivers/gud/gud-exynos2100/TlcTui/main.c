// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2013-2019 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include "tlcTui.h"
#include "tui-hal.h"
#include "build_tag.h"
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include "linux/miscdevice.h"
#include "linux/mod_devicetable.h"
#include "linux/of.h"

/* Static variables */
struct device* (dev_tlc_tui) = NULL;
struct miscdevice tui;
/* Function pointer */
int (*fptr_get_fd)(u32 buff_id) = NULL;

static long tui_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	int ret = -ENOTTY;
	int __user *uarg = (int __user *)arg;

	if (_IOC_TYPE(cmd) != TUI_IO_MAGIC)
		return -EINVAL;

	tui_dev_devel("t-base-tui module: ioctl 0x%x", cmd);

	switch (cmd) {
	case TUI_IO_SET_RESOLUTION:
		/* TLC_TUI_CMD_SET_RESOLUTION is for specific platforms
		 * that rely on onConfigurationChanged to set resolution
		 * it has no effect on Trustonic reference implementation.
		 */
		tui_dev_devel("TLC_TUI_CMD_SET_RESOLUTION");
		/* NOT IMPLEMENTED */
		ret = 0;
		break;

	case TUI_IO_NOTIFY:
		tui_dev_devel("TUI_IO_NOTIFY");

		if (tlc_notify_event(arg))
			ret = 0;
		else
			ret = -EFAULT;
		break;

	case TUI_IO_WAITCMD: {
		struct tlc_tui_command_t tui_cmd = {0};

		tui_dev_devel("TUI_IO_WAITCMD");

		ret = tlc_wait_cmd(&tui_cmd);
		if (ret) {
			tui_dev_err(ret, "%d tlc_wait_cmd returned", __LINE__);
			return ret;
		}

		/* Write command id to user */
		tui_dev_devel("IOCTL: sending command %d to user.", tui_cmd.id);

		if (copy_to_user(uarg, &tui_cmd, sizeof(
						struct tlc_tui_command_t)))
			ret = -EFAULT;
		else
			ret = 0;

		break;
	}

	case TUI_IO_ACK: {
		struct tlc_tui_response_t rsp_id;

		tui_dev_devel("TUI_IO_ACK");

		/* Read user response */
		if (copy_from_user(&rsp_id, uarg, sizeof(rsp_id)))
			ret = -EFAULT;
		else
			ret = 0;

		tui_dev_devel("IOCTL: User completed command %d.", rsp_id.id);
		ret = tlc_ack_cmd(&rsp_id);
		if (ret)
			return ret;
		break;
	}

	case TUI_IO_INIT_DRIVER: {
		tui_dev_devel("TUI_IO_INIT_DRIVER");

		ret = tlc_init_driver();
		if (ret) {
			tui_dev_err(ret, "%d tlc_init_driver() failed",
				    __LINE__);
			return ret;
		}
		break;
	}

	/* Ioclt that allows to get buffer info from DrTui
	 */
	case TUI_IO_GET_BUFFER_INFO: {
		tui_dev_devel("TUI_IO_GET_BUFFER_INFO");

		/* Get all buffer info received from DrTui through the dci */
		struct tlc_tui_ioctl_buffer_info buff_info;

		get_buffer_info(&buff_info);

		/* Fill in return struct */
		if (copy_to_user(uarg, &buff_info, sizeof(buff_info)))
			ret = -EFAULT;
		else
			ret = 0;

		break;
	}

	/* Ioclt that allows to get the ion buffer f
	 */
	case TUI_IO_GET_ION_FD: {
		tui_dev_devel("TUI_IO_GET_ION_FD");

		/* Get the back buffer id (in the dci, from DrTui) */
		u32 buff_id = dci->buff_id;

		/* Get the fd of the ion buffer */
		struct tlc_tui_ioctl_ion_t ion;

		ion.buffer_fd = -1;
		if (fptr_get_fd) {
			ion.buffer_fd = (*fptr_get_fd)(buff_id);

			/* Fill in return struct */
			if (copy_to_user(uarg, &ion, sizeof(ion))) {
				ret = -EFAULT;
				break;
			}
		}

		ret = 0;
		break;
	}

	default:
		ret = -ENOTTY;
		tui_dev_err(ret, "%d Unknown ioctl (%u)!", __LINE__, cmd);
		return ret;
	}

	return ret;
}

atomic_t fileopened;

static int tui_open(struct inode *inode, struct file *file)
{
	tui_dev_devel("TUI file opened");
	atomic_inc(&fileopened);
	return 0;
}

static int tui_release(struct inode *inode, struct file *file)
{
	tui_dev_devel("TUI file closed");
	if (atomic_dec_and_test(&fileopened))
		tlc_notify_event(NOT_TUI_CANCEL_EVENT);

	return 0;
}

static int exynos_tui_probe(struct platform_device *pdev)
{
	dev_tlc_tui = &pdev->dev;
	arch_setup_dma_ops(&pdev->dev, 0x0ULL, 1ULL << 36, NULL, false);
	pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
	dma_set_mask(&pdev->dev, DMA_BIT_MASK(36));

	tui_dev_devel("TUI probe done.\n");

	return 0;
}

static struct device_driver tui_driver = {
	.name = "Trustonic"
};

struct device tui_dev = {
	.driver = &tui_driver
};

static const struct of_device_id exynos_tui_of_match_table[] = {
	{ .compatible = "samsung,exynos-tui", },
	{ },
};

static struct platform_driver exynos_tui_driver = {
	.probe = exynos_tui_probe,
	.driver = {
		.name = "exynos-tui",
		.owner = THIS_MODULE,
		.of_match_table = exynos_tui_of_match_table,
	}
};

static int __init tlc_tui_init(void)
{
	int ret;

	dev_set_name(&tui_dev, "TUI");
	tui_dev_info("Loading t-base-tui module.");
	tui_dev_devel("=============== Running TUI Kernel TLC ===============");
	tui_dev_info("%s", MOBICORE_COMPONENT_BUILD_TAG);

	atomic_set(&fileopened, 0);

	ret = misc_register(&tui);
	if (ret) {
		tui_dev_devel("tui can't register misc on minor=%d\n",
				MISC_DYNAMIC_MINOR);
		return ret;
	}

	if (!hal_tui_init())
		return -EPERM;

	return platform_driver_register(&exynos_tui_driver);
}

static void __exit tlc_tui_exit(void)
{
	tui_dev_devel("Unloading t-base-tui module.");

	misc_deregister(&tui);
	platform_driver_unregister(&exynos_tui_driver);

	hal_tui_exit();
}

static const struct file_operations tui_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = tui_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tui_ioctl,
#endif
	.open = tui_open,
	.release = tui_release,
};

struct miscdevice tui = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= TUI_DEV_NAME,
	.fops	= &tui_fops,
};

module_init(tlc_tui_init);
module_exit(tlc_tui_exit);

MODULE_AUTHOR("Trustonic Limited");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Kinibi TUI");
