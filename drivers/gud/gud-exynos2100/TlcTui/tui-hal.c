// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2014-2017 TRUSTONIC LIMITED
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


#include <linux/types.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/clk.h>
#include <linux/console.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>
#include <linux/dma-buf.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/ion.h>
#include <linux/scatterlist.h>
#include "inc/t-base-tui.h"
#include "public/tui_ioctl.h"
#include "inc/dciTui.h"
#include "tlcTui.h"
#include "tui-hal.h"
#include "../../../../drivers/video/fbdev/exynos/dpu30/decon.h"
#ifdef CONFIG_TRUSTED_UI_TOUCH_ENABLE
#include "../../../../input/touchscreen/sec_ts/sec_ts.h"
#endif

//#define STUI_FRAME_BUF_SIZE    (1440 * 3040 * 4)
#define STUI_FRAME_BUF_SIZE	0x2000000 /* Frame Buffer */
#define STUI_ALIGN_4KB_SZ	0x1000

#define STUI_ALIGN_UP(size, block) ((((size) + (block) - 1) / (block)) * (block))

static struct dma_buf *g_dma_buf;
struct dma_buf_attachment* g_attachment;
struct sg_table *g_sgt;

#ifdef CONFIG_TRUSTED_UI_TOUCH_ENABLE
static int tsp_irq_num = ;
#if defined(CONFIG_TOUCHSCREEN_SEC_TS)
static void tui_delay(unsigned int ms)
{
	if (ms < 20)
		usleep_range(ms * 1000, ms * 1000);
	else
		msleep(ms);
}
#endif
void trustedui_set_tsp_irq(int irq_num)
{
	tsp_irq_num = irq_num;
	pr_info("%s called![%d]\n", __func__, irq_num);
}
#endif

#define COUNT_OF_ION_HANDLE (4)
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
static int is_device_ok(struct device *fbdev, const void *p)
{
	return 1;
}
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
static struct device *get_fb_dev(void)
{
	struct device *fbdev = NULL;

	/* get the first framebuffer device */
	/* [TODO] Handle properly when there are more than one framebuffer */
	fbdev = class_find_device(fb_class, NULL, NULL, is_device_ok);
	if (NULL == fbdev) {
		pr_debug("ERROR cannot get framebuffer device\n");
		return NULL;
	}
	return fbdev;
}
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
static struct fb_info *get_fb_info(struct device *fbdev)
{
	struct fb_info *fb_info;

	if (!fbdev->p) {
		pr_debug("ERROR framebuffer device has no private data\n");
		return NULL;
	}

	fb_info = (struct fb_info *) dev_get_drvdata(fbdev);
	if (!fb_info) {
		pr_debug("ERROR framebuffer device has no fb_info\n");
		return NULL;
	}

	return fb_info;
}
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
static int fb_tui_protection(void)
{
	struct device *fbdev = NULL;
	struct fb_info *fb_info;
	struct decon_win *win;
	struct decon_device *decon;
	int ret = -ENODEV;

	fbdev = get_fb_dev();
	if (!fbdev) {
		pr_debug("get_fb_dev failed\n");
		return ret;
	}

	fb_info = get_fb_info(fbdev);
	if (!fb_info) {
		pr_debug("get_fb_info failed\n");
		return ret;
	}

	win = fb_info->par;
	decon = win->decon;

	lock_fb_info(fb_info);
	ret = decon_tui_protection(true);
	unlock_fb_info(fb_info);
	return ret;
}
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
static int fb_tui_unprotection(void)
{
	struct device *fbdev = NULL;
	struct fb_info *fb_info;
	struct decon_win *win;
	struct decon_device *decon;
	int ret = -ENODEV;

	fbdev = get_fb_dev();
	if (!fbdev) {
		pr_debug("get_fb_dev failed\n");
		return ret;
	}

	fb_info = get_fb_info(fbdev);
	if (!fb_info) {
		pr_err("get_fb_info failed\n");
		return ret;
	}

	win = fb_info->par;
	if (win == NULL) {
		pr_err("get win failed\n");
		return ret;
	}

	decon = win->decon;
	if (decon == NULL) {
		pr_err("get decon failed\n");
		return ret;
	}

	lock_fb_info(fb_info);
	ret = decon_tui_protection(false);
	unlock_fb_info(fb_info);

	return ret;
}
#endif

/**
 * hal_tui_init() - integrator specific initialization for kernel module
 *
 * This function is called when the kernel module is initialized, either at
 * boot time, if the module is built statically in the kernel, or when the
 * kernel is dynamically loaded if the module is built as a dynamic kernel
 * module. This function may be used by the integrator, for instance, to get a
 * memory pool that will be used to allocate the secure framebuffer and work
 * buffer for TUI sessions.
 *
 * Return: must return 0 on success, or non-zero on error. If the function
 * returns an error, the module initialization will fail.
 */
u32 hal_tui_init(void)
{
	return TUI_DCI_OK;
}

/**
 * hal_tui_exit() - integrator specific exit code for kernel module
 *
 * This function is called when the kernel module exit. It is called when the
 * kernel module is unloaded, for a dynamic kernel module, and never called for
 * a module built into the kernel. It can be used to free any resources
 * allocated by hal_tui_init().
 */
void hal_tui_exit(void)
{
}

/**
 * hal_tui_alloc() - allocator for secure framebuffer and working buffer
 * @allocbuffer:    input parameter that the allocator fills with the physical
 *                  addresses of the allocated buffers
 * @allocsize:      size of the buffer to allocate.  All the buffer are of the
 *                  same size
 * @number:         Number to allocate.
 *
 * This function is called when the module receives a CMD_TUI_SW_OPEN_SESSION
 * message from the secure driver.  The function must allocate 'number'
 * buffer(s) of physically contiguous memory, where the length of each buffer
 * is at least 'allocsize' bytes.  The physical address of each buffer must be
 * stored in the array of structure 'allocbuffer' which is provided as
 * arguments.
 *
 * Physical address of the first buffer must be put in allocate[0].pa , the
 * second one on allocbuffer[1].pa, and so on.  The function must return 0 on
 * success, non-zero on error.  For integrations where the framebuffer is not
 * allocated by the Normal World, this function should do nothing and return
 * success (zero).
 * If the working buffer allocation is different from framebuffers, ensure that
 * the physical address of the working buffer is at index 0 of the allocbuffer
 * table (allocbuffer[0].pa).
 */
#define ION_EXYNOS_FLAG_PROTECTED       (1 << 16)
extern struct device* dev_tlc_tui;

uint64_t find_heapmask(void)
{
	int i, cnt = ion_query_heaps_kernel(NULL, 0);
	const char *heapname;
	struct ion_heap_data data[ION_NUM_MAX_HEAPS];

	heapname = "tui_heap";
	ion_query_heaps_kernel((struct ion_heap_data *)data, cnt);
	for (i = 0; i < cnt; i++) {
		if (!strncmp(data[i].name, heapname, MAX_HEAP_NAME))
			break;
	}

	if (i == cnt) {
		pr_err("heap %s is not found\n", heapname);
		return 0;
	}

	return 1 << data[i].heap_id;
}

uint32_t hal_tui_alloc(
	struct tui_alloc_buffer_t *allocbuffer,
	size_t allocsize, uint32_t number)
{
	unsigned long framebuf_size = 0;
	dma_addr_t phys_addr = 0;
	unsigned int heapmask;
	int i;

	heapmask = find_heapmask();

	framebuf_size = STUI_ALIGN_UP(allocsize, STUI_ALIGN_4KB_SZ);
	tui_dev_devel("framebuf_size(%x), num(%x)\n", framebuf_size, heapmask);

	g_dma_buf = ion_alloc(framebuf_size * number, heapmask, ION_EXYNOS_FLAG_PROTECTED);
	if (IS_ERR(g_dma_buf)){
		pr_err("fail to allocate dma buffer\n");
		goto err_alloc;
	}

	g_attachment = dma_buf_attach(g_dma_buf, dev_tlc_tui);
	if (IS_ERR(g_attachment)){
		pr_err("fail to dma buf attachment\n");
		goto err_attach;
	}

	g_sgt = dma_buf_map_attachment(g_attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR(g_sgt)){
		pr_err("Failed to get sgt (err %ld)\n", PTR_ERR(g_sgt));
		goto err_attachment;
	}

	phys_addr = sg_phys(g_sgt->sgl);
	if (IS_ERR_VALUE(phys_addr)) {
		pr_err("Failed to get iova (err 0x%p)\n",
				&phys_addr);
		goto err_daddr;
	}

	for (i = 0; i < number; i++)
		allocbuffer[i].pa = (uint64_t)(phys_addr + framebuf_size * i);

	for (i = 0; i < number; i++)
		tui_dev_devel("buf address(%d)(%x)\n",i, allocbuffer[i].pa);

	return TUI_DCI_OK;

err_daddr:
        phys_addr = 0;
        dma_buf_unmap_attachment(g_attachment, g_sgt,
                                 DMA_BIDIRECTIONAL);
err_attachment:
	dma_buf_unmap_attachment(g_attachment,g_sgt,DMA_BIDIRECTIONAL);
err_attach:
	dma_buf_detach(g_dma_buf, g_attachment);
err_alloc:
	dma_buf_put(g_dma_buf);
	return -ENOMEM;
}

#ifdef CONFIG_TRUSTED_UI_TOUCH_ENABLE
void tui_i2c_reset(void)
{
	void __iomem *i2c_reg;
	u32 tui_i2c;
	u32 i2c_conf;

	i2c_reg = ioremap(HSI2C7_PA_BASE_ADDRESS, SZ_4K);
	tui_i2c = readl(i2c_reg + HSI2C_CTL);
	tui_i2c |= HSI2C_SW_RST;
	writel(tui_i2c, i2c_reg + HSI2C_CTL);

	tui_i2c = readl(i2c_reg + HSI2C_CTL);
	tui_i2c &= ~HSI2C_SW_RST;
	writel(tui_i2c, i2c_reg + HSI2C_CTL);

	writel(0x4c4c4c00, i2c_reg + 0x0060);
	writel(0x26004c4c, i2c_reg + 0x0064);
	writel(0x99, i2c_reg + 0x0068);

	i2c_conf = readl(i2c_reg + HSI2C_CONF);
	writel((HSI2C_FUNC_MODE_I2C | HSI2C_MASTER), i2c_reg + HSI2C_CTL);

	writel(HSI2C_TRAILING_COUNT, i2c_reg + HSI2C_TRAILIG_CTL);
	writel(i2c_conf | HSI2C_AUTO_MODE, i2c_reg + HSI2C_CONF);

	iounmap(i2c_reg);
}

static uint32_t prepare_enable_i2c_clock(void)
{
       struct clk *i2c_clock;

       i2c_clock = clk_get(NULL, "pclk_hsi2c7");
       if (IS_ERR(i2c_clock)) {
               pr_err("Can't get i2c clock\n");
               return -1;
       }

       clk_prepare_enable(i2c_clock);
       pr_info(KERN_ERR "[TUI] I2C clock prepared.\n");

       clk_put(i2c_clock);

       return 0;
}

static uint32_t disable_unprepare_i2c_clock(void)
{
       struct clk *i2c_clock;

       i2c_clock = clk_get(NULL, "pclk_hsi2c7");
       if (IS_ERR(i2c_clock)) {
               pr_err("Can't get i2c clock\n");
               return -1;
       }

       clk_disable_unprepare(i2c_clock);
       pr_info(KERN_ERR "[TUI] I2C clock released.\n");

       clk_put(i2c_clock);

       return 0;
}
#endif

/**
 * hal_tui_free() - free memory allocated by hal_tui_alloc()
 *
 * This function is called at the end of the TUI session, when the TUI module
 * receives the CMD_TUI_SW_CLOSE_SESSION message. The function should free the
 * buffers allocated by hal_tui_alloc(...).
 */

void hal_tui_free(void)
{
	dma_buf_unmap_attachment(g_attachment,g_sgt,DMA_BIDIRECTIONAL);
	dma_buf_detach(g_dma_buf, g_attachment);
	dma_buf_put(g_dma_buf);
}

/**
 * hal_tui_deactivate() - deactivate Normal World display and input
 *
 * This function should stop the Normal World display and, if necessary, Normal
 * World input. It is called when a TUI session is opening, before the Secure
 * World takes control of display and input.
 *
 * Return: must return 0 on success, non-zero otherwise.
 */
uint32_t hal_tui_deactivate(void)
{
	int ret = TUI_DCI_OK;

	/* Set linux TUI flag */
	trustedui_set_mask(TRUSTEDUI_MODE_TUI_SESSION);
	trustedui_blank_set_counter(0);
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
	pr_info(KERN_ERR "blanking!\n");

	ret = fb_tui_protection();
	if (ret != 0) {
		pr_info("blanking ERR! fb_tui_protection ret = %d\n", ret);
#ifdef CONFIG_TRUSTED_UI_TOUCH_ENABLE
		enable_irq(tsp_irq_num);
#endif
		/* Clear linux TUI flag */
		trustedui_set_mode(TRUSTEDUI_MODE_OFF);

		return TUI_DCI_ERR_INTERNAL_ERROR;
	}
#ifdef CONFIG_TRUSTED_UI_TOUCH_ENABLE
	prepare_enable_i2c_clock();
#endif
#endif
	trustedui_set_mask(TRUSTEDUI_MODE_VIDEO_SECURED);
	pr_info("Ready to use TUI!\n");

	return TUI_DCI_OK;
}

/**
 * hal_tui_activate() - restore Normal World display and input after a TUI
 * session
 *
 * This function should enable Normal World display and, if necessary, Normal
 * World input. It is called after a TUI session, after the Secure World has
 * released the display and input.
 *
 * Return: must return 0 on success, non-zero otherwise.
 */
uint32_t hal_tui_activate(void)
{
	int ret;
	// Protect NWd
	trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);
#ifdef CONFIG_TRUSTED_UI_TOUCH_ENABLE
	disable_unprepare_i2c_clock();
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
	pr_info("Unblanking\n");
	ret = fb_tui_unprotection();
	if (ret != 0) {
		pr_info("unblanking ERR! fb_tui_unprotection ret = %d retry!!!\n", ret);
		ret = fb_tui_unprotection();
		if (ret == -ENODEV)
			pr_info("unblanking retry ERR!!! fb_tui_unprotection ret = %d\n", ret);
	}
#endif
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
	pr_info("Unsetting TUI flag (blank counter=%d)", trustedui_blank_get_counter());
#endif

	/* Clear linux TUI flag */
	trustedui_set_mode(TRUSTEDUI_MODE_OFF);
	pr_info("Success to exit TUI!\n");

	return TUI_DCI_OK;
}
/* Do nothing it's only use for QC */
u32 hal_tui_process_cmd(struct tui_hal_cmd_t *cmd, struct tui_hal_rsp_t *rsp)
{
	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
u32 hal_tui_notif(void)
{
	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
void hal_tui_post_start(struct tlc_tui_response_t *rsp)
{
	pr_info("%s(%d)\n", __func__, __LINE__);
}
