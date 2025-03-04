/* tui/stui_core.c
 *
 * Samsung TUI HW Handler driver.
 *
 * Copyright (C) 2012-2021, Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/highmem.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>

#include "stui_core.h"
#include "stui_hal.h"
#include "stui_inf.h"

#include <linux/smc.h>

#ifdef SAMSUNG_TUI_TEST
static uint64_t g_fb_pa;

static void stui_write_signature(void)
{
	uint32_t *kaddr;
	struct page *page;

	page = pfn_to_page(g_fb_pa >> PAGE_SHIFT);
	kaddr = kmap(page);
	if (kaddr) {
		*kaddr = 0x01020304;
		pr_debug(TUIHW_LOG_TAG " kaddr : %pK %x\n", kaddr, *kaddr);
		kunmap(page);
	} else {
		pr_err(TUIHW_LOG_TAG " kmap failed\n");
	}
}
#endif


#define SMC_DRM_TUI_PROT		(0x82002120)
#define SMC_DRM_TUI_UNPROT		(0x82002121)
#define STUI_RET_OK              0x00030000

long stui_process_cmd(struct file *f, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	/* Handle command */
	pr_debug(TUIHW_LOG_TAG " %s >>\n", __func__);
	switch (cmd) {
	case STUI_HW_IOCTL_START_TUI: {
		struct tui_hw_buffer __user *argp = (struct tui_hw_buffer __user *)arg;
		struct tui_hw_buffer buffer;

		pr_debug(TUIHW_LOG_TAG " STUI_HW_IOCTL_START_TUI called\n");

		if (stui_get_mode() & STUI_MODE_ALL) {
			ret = -EBUSY;
			break;
		}

		ret = stui_open_touch();
		if (ret < 0) {
			pr_err(TUIHW_LOG_TAG " stui_open_touch failed\n");
			goto lbl_rollback_touch;
		}

		ret = stui_open_display(&buffer);
		if (ret < 0) {
			pr_err(TUIHW_LOG_TAG " stui_open_display failed\n");
			goto lbl_rollback_display;
		}

		buffer.touch_type = stui_get_touch_type();
		pr_info(TUIHW_LOG_TAG "stui tsp_type %d\n", buffer.touch_type);

		if (copy_to_user(argp, &buffer, sizeof(struct tui_hw_buffer))) {
			pr_err(TUIHW_LOG_TAG " copy_to_user failed\n");
			ret = -EFAULT;
			goto lbl_rollback_display;
		}
		break;

lbl_rollback_display:
		stui_close_display();
lbl_rollback_touch:
		stui_close_touch();
		break;
	}
	case STUI_HW_IOCTL_FINISH_TUI: {
		pr_debug(TUIHW_LOG_TAG " STUI_HW_IOCTL_FINISH_TUI called\n");
		if (stui_get_mode() == STUI_MODE_OFF) {
			pr_err(TUIHW_LOG_TAG " stui mode = STUI_MODE_OFF\n");
			ret = -EPERM;
			break;
		}
		stui_close_touch();
		stui_close_display();
		stui_set_mode(STUI_MODE_OFF);
		break;
	}
#ifdef SAMSUNG_TUI_TEST
	case STUI_HW_IOCTL_GET_PHYS_ADDR: {
		uint64_t __user *argp = (uint64_t __user *)arg;

		if (copy_to_user(argp, &g_fb_pa, sizeof(uint64_t))) {
			pr_err(TUIHW_LOG_TAG " copy_to_user failed\n");
			ret = -EFAULT;
		}
		break;
	}
#endif
	case STUI_HW_IOCTL_GET_RESOLUTION: {
		struct tui_hw_buffer __user *argp = (struct tui_hw_buffer __user *)arg;
		struct tui_hw_buffer buffer;

		pr_debug(TUIHW_LOG_TAG " TUI_HW_IOCTL_GET_RESOLUTION called\n");
		memset(&buffer, 0, sizeof(struct tui_hw_buffer));
		if (stui_get_resolution(&buffer)) {
			pr_err(TUIHW_LOG_TAG " stui_get_resolution failed\n");
			ret = -EPERM;
			break;
		}

		if (copy_to_user(argp, &buffer, sizeof(struct tui_hw_buffer))) {
			pr_err(TUIHW_LOG_TAG " copy_to_user failed\n");
			ret = -EFAULT;
		}
		break;
	}
	default:
		pr_err(TUIHW_LOG_TAG " stui_process_cmd(ERROR): Unknown command %d\n", cmd);
		ret = -ENOTTY;
		break;
	}
	pr_debug(TUIHW_LOG_TAG " %s << ret=%d\n", __func__, ret);
	if (ret == 0) {
		ret  = STUI_RET_OK;
	}
	return ret;
}

int stui_open_touch(void)
{
	pr_debug(TUIHW_LOG_TAG " %s >>\n", __func__);

	if (stui_get_mode() & STUI_MODE_TOUCH_SEC) {
		pr_err(TUIHW_LOG_TAG " already in TUI mode.\n");
		return -EBUSY;
	}

	if (stui_i2c_protect(true) != 0) {
		pr_err(TUIHW_LOG_TAG " stui_i2c_protect failed.\n");
		return -EPERM;
	}
	stui_set_mask(STUI_MODE_TOUCH_SEC);

	pr_debug(TUIHW_LOG_TAG " %s <<\n", __func__);
	return 0;
}

int stui_open_display(struct tui_hw_buffer *buffer)
{
	pr_debug(TUIHW_LOG_TAG " %s >>\n", __func__);

	if (stui_get_mode() & STUI_MODE_DISPLAY_SEC) {
		pr_err(TUIHW_LOG_TAG " already in TUI mode.\n");
		return -EBUSY;
	}

	/* allocate TUI frame buffer */
	pr_info(TUIHW_LOG_TAG " Allocating Framebuffer\n");
	memset(buffer, 0, sizeof(struct tui_hw_buffer));
	if (stui_alloc_video_space(buffer)) {
		pr_err(TUIHW_LOG_TAG " stui_alloc_video_space failed.\n");
		return -EPERM;
	}

#ifdef SAMSUNG_TUI_TEST
	g_fb_pa = buffer.fb_physical;
	stui_write_signature();
#endif

	/* Prepare display for TUI / Deactivate linux UI drivers */
	if (stui_prepare_tui()) {
		pr_err(TUIHW_LOG_TAG " stui_prepare_tui failed.\n");
		stui_free_video_space();
		return -EFAULT;
	}

	stui_set_mask(STUI_MODE_DISPLAY_SEC);
	pr_debug(TUIHW_LOG_TAG " %s <<\n", __func__);
	return 0;
}

void stui_close_touch(void)
{
	pr_debug(TUIHW_LOG_TAG " %s >>\n", __func__);
	if ((stui_get_mode() & STUI_MODE_TOUCH_SEC) == 0) {
		pr_err(TUIHW_LOG_TAG " already free.\n");
		return;
	}

	stui_i2c_protect(false);
	stui_clear_mask(STUI_MODE_TOUCH_SEC);
	pr_debug(TUIHW_LOG_TAG " %s <<\n", __func__);
}

void stui_close_display(void)
{
	pr_debug(TUIHW_LOG_TAG " %s >>\n", __func__);
	if ((stui_get_mode() & STUI_MODE_DISPLAY_SEC) == 0) {
		pr_err(TUIHW_LOG_TAG " already free.\n");
		return;
	}
	/* Disable STUI driver / Activate linux UI drivers */
	stui_clear_mask(STUI_MODE_DISPLAY_SEC);
	stui_finish_tui();
	stui_free_video_space();
	pr_debug(TUIHW_LOG_TAG " %s <<\n", __func__);
}
