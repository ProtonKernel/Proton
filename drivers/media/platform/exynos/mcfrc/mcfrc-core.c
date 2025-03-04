/* linux/drivers/media/platform/exynos/mcfrc-core.c
 *
 * Core file for Samsung MC FRC driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Authors: Jungik Seo <jungik.seo@samsung.com>
 *          Igor Kovliga <i.kovliga@samsung.com>
 *          Sergei Ilichev <s.ilichev@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

//#define DEBUG
#define _DEBUG_VERSION_STR_ "03SIrel"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/signal.h>
#include <linux/ioport.h>
#include <linux/kmod.h>
//#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/clk.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <asm/page.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/time.h>
#include <linux/delay.h>

#include <linux/iommu.h>
#if defined(CONFIG_EXYNOS_IOVMM)
#include <linux/exynos_iovmm.h>
#endif

#include <linux/pm_runtime.h>

#include "mcfrc-core.h"
#include "mcfrc-reg.h"
#include "mcfrc-helper.h"

//The name of the gate clock. This is set in the device-tree entry
static const char *clk_producer_name = "gate";


#ifdef DEBUG
#define HWMCFRC_PROFILE_ENABLE
#endif

#ifdef HWMCFRC_PROFILE_ENABLE
//static struct timeval global_time_start;
//static struct timeval global_time_end;
//static struct timeval global_time_start1;
//static struct timeval global_time_start2;
//static struct timeval global_time_start3;
//static struct timeval global_time_end1;
//static struct timeval global_time_end2;
//static struct timeval global_time_end3;
static ktime_t global_time_start;
static ktime_t global_time_end;
static ktime_t global_time_start1;
static ktime_t global_time_start2;
//static ktime_t global_time_start3;
static ktime_t global_time_end1;
static ktime_t global_time_end2;
//static ktime_t global_time_end3;
//linux / timekeeping.h
//ktime_t ktime_get(void);
/*
ktime_t entry_stamp, now;
s64 delta;
// Get the current time .
entry_stamp = ktime_get();
// some stuff
now = ktime_get();
delta = ktime_to_ns(ktime_sub(now, entry_stamp));
*/
/*
If variable is of Type,         use printk format specifier:
------------------------------------------------------------
int                     %d or %x
unsigned int            %u or %x
long                    %ld or %lx
unsigned long           %lu or %lx
long long               %lld or %llx
unsigned long long      %llu or %llx
size_t                  %zu or %zx
ssize_t                 %zd or %zx
s32                     %d or %x
u32                     %u or %x
s64                     %lld or %llx
u64                     %llu or %llx
*/

//#define HWMCFRC_PROFILE(statement, msg, dev) \
//	do { \
//		/* Use local structs to support nesting HWMCFRC_PROFILE calls */\
//		struct timeval time_start; \
//		struct timeval time_end; \
//		do_gettimeofday(&time_start); \
//		statement; \
//		do_gettimeofday(&time_end); \
//		dev_info(dev, msg ": %ld microsecs\n", \
//			1000000 * (time_end.tv_sec - time_start.tv_sec) + \
//			(time_end.tv_usec - time_start.tv_usec)); \
//	} while (0)
//#else
#define HWMCFRC_PROFILE(statement, msg, dev) \
	do { \
		/* Use local structs to support nesting HWMCFRC_PROFILE calls */\
		ktime_t time_start; \
		ktime_t time_end; \
		time_start = ktime_get(); \
		statement; \
		time_end = ktime_get(); \
		dev_info(dev, msg ": %lld nanosecs\n", \
			ktime_to_ns(ktime_sub(time_end, time_start))); \
	} while (0)
#else
#define HWMCFRC_PROFILE(statement, msg, dev) \
	do { \
		statement; \
	} while (0)
#endif

#define MAX_BUF_NUM	3

#define CAVY_TEST_BIT(n, reg) (n==(n&reg))
#define CAVY_SET_BIT(n, reg) (reg=(reg|n))
#define CAVY_CLI_BIT(n, reg) (reg=(reg&(~n)))

#if (MCFRC_DBG_MODE == 1) // remove debug parameters
static void mcfrc_run_programm(struct mcfrc_dev *mcfrc_device, struct hwMCFRC_reg_programm* prog) {
	void __iomem *base = mcfrc_device->regs;
	int i = 0;
	for (i = 0; i < HWMCFRC_REG_PROGRAMM_MAX_LEN; i++) {
		bool end = (prog->command[i].type & REG_COMMAND_TYPE_END) == REG_COMMAND_TYPE_END;
		bool rd_com = (prog->command[i].type & REG_COMMAND_TYPE_READ) == REG_COMMAND_TYPE_READ;
		bool wr_com = (prog->command[i].type & REG_COMMAND_TYPE_WRITE) == REG_COMMAND_TYPE_WRITE;

		if (end) {
			dev_dbg(mcfrc_device->dev, "%s: programm(%p) i=% 3d end reached\n", __func__, prog, i);
			break;
		}
		if (rd_com) {
			prog->command[i].data_read = readl(base + prog->command[i].address);
			dev_dbg(mcfrc_device->dev, "%s: programm(%p) i=% 3d read  (0x%04X)->0x%08X\n", __func__, prog, i, prog->command[i].address, prog->command[i].data_read);
		}
		if (wr_com) {
			dev_dbg(mcfrc_device->dev, "%s: programm(%p) i=% 3d write (0x%04X)<-0x%08x\n", __func__, prog, i, prog->command[i].address, prog->command[i].data_write);
			writel(prog->command[i].data_write, base + prog->command[i].address);
		}
	}
}
#endif
/**
 * mcfrc_buffer_map:
 *
 * Finalize the necessary preparation to make the buffer
 * accessible to HW, e.g. dma attachment setup and iova mapping
 * (by using IOVMM API, which will use IOMMU).
 * The buffer will be made available to H/W as a contiguous chunk
 * of virtual memory which, thanks to the IOMMU, can be used for
 * DMA transfers even though it the mapping might refer to scattered
 * physical pages.
 *
 * Returns 0 on success, otherwise < 0 err num
 */
  static int mcfrc_buffer_map(struct mcfrc_ctx *ctx,
				   struct mcfrc_buffer_dma *dma_buffer,
				   enum dma_data_direction dir)
{
	int ret = 0;

	dev_dbg(ctx->mcfrc_dev->dev, "%s: BEGIN\n", __func__);

	dev_dbg(ctx->mcfrc_dev->dev
		, "%s: about to map dma buffer in device address space...\n"
		, __func__);

	//map dma attachment:
	//this is the point where the dma buffer is actually
	//allocated, if it wasn't there before.
	//It also means the calling driver now owns the buffer
	ret = mcfrc_map_dma_attachment(ctx->mcfrc_dev->dev,
				   &dma_buffer->plane, dir);
	if (ret)
		return ret;

	dev_dbg(ctx->mcfrc_dev->dev, "%s: getting dma address of the buffer...\n"
		, __func__);

	//use iommu to map and get the virtual memory address
	ret = mcfrc_dma_addr_map(ctx->mcfrc_dev->dev, dma_buffer, dir);
	if (ret) {
		dev_dbg(ctx->mcfrc_dev->dev, "%s: mapping FAILED!\n", __func__);
		mcfrc_unmap_dma_attachment(ctx->mcfrc_dev->dev,
				   &dma_buffer->plane, dir);
		return ret;
	}

	dev_dbg(ctx->mcfrc_dev->dev, "%s: END\n", __func__);

	return 0;
}

/**
 * mcfrc_buffer_unmap:
 *
 * called after the task is finished to release all references
 * to the buffers. This is called for every buffer that
 * mcfrc_buffer_map was previously called for.
*/
static void mcfrc_buffer_unmap( struct mcfrc_ctx *ctx,
								struct mcfrc_buffer_dma *dma_buffer,
								enum dma_data_direction dir)
{
	dev_dbg(ctx->mcfrc_dev->dev, "%s: BEGIN\n", __func__);
	dev_dbg(ctx->mcfrc_dev->dev, "%s: about to unmap the DMA address\n"
		, __func__);

	//use iommu to unmap the dma address
	mcfrc_dma_addr_unmap(ctx->mcfrc_dev->dev, dma_buffer);

	dev_dbg(ctx->mcfrc_dev->dev, "%s: about to unmap the DMA buffer\n"
		, __func__);

	//unmap dma attachment, i.e. unmap the buffer so others can request it
	mcfrc_unmap_dma_attachment(ctx->mcfrc_dev->dev,
			   &dma_buffer->plane, dir);

	//hwmcfrc_task will dma detach and decrease the refcount on the dma fd

	dev_dbg(ctx->mcfrc_dev->dev, "%s: END\n", __func__);

}


/**
 * Use the Runtime Power Management framework to request device power on/off.
 * This will cause the Runtime PM handlers to be called when appropriate (i.e.
 * when the device is asleep and needs to be woken up or nothing is using it
 * and can be suspended).
 *
 * Returns 0 on success, < 0 err num otherwise.
 */

static int enable_mcfrc(struct mcfrc_dev *mcfrc)
{
	int ret;

	dev_dbg(mcfrc->dev, "%s: runtime pm, getting device\n", __func__);

	//we're assuming our runtime_resume callback will enable the clock
	ret = in_irq() ? pm_runtime_get(mcfrc->dev) :
		pm_runtime_get_sync(mcfrc->dev);
	if (ret < 0) {
		dev_err(mcfrc->dev
			, "%s: failed to increment Power Mgmt usage cnt (%d)\n"
			, __func__, ret);
		return ret;
	}

	//No need to enable the clock, our runtime pm callback will do it

	return 0;
}

static int disable_mcfrc(struct mcfrc_dev *mcfrc)
{
	int ret;

	dev_dbg(mcfrc->dev
		, "%s: disabling clock, decrementing runtime pm usage\n"
		, __func__);

	//No need to disable the clock, our runtime pm callback will do it

	//we're assuming our runtime_suspend callback will disable the clock
	if (in_irq()) {
		dev_dbg(mcfrc->dev
			, "%s: IN IRQ! DELAYED autosuspend\n", __func__);
		ret = pm_runtime_put_autosuspend(mcfrc->dev);
	}
	else {
		dev_dbg(mcfrc->dev
			, "%s: SYNC autosuspend\n", __func__);
		ret = pm_runtime_put_sync_autosuspend(mcfrc->dev);
	}
	//ret = in_irq() ? pm_runtime_put_autosuspend(mcfrc->dev) :
	//		 pm_runtime_put_sync_autosuspend(mcfrc->dev);

	if (ret < 0) {
		dev_err(mcfrc->dev
			, "%s: failed to decrement Power Mgmt usage cnt (%d)\n"
			, __func__, ret);
		return ret;
	}
	return 0;
}

static int mcfrc_setup_device_start(struct mcfrc_dev *mcfrc_device,
									struct mcfrc_task *task)
{
	void __iomem *base_addr;

	if (!mcfrc_device) {
		pr_err("%s: invalid mcfrc device!\n", __func__);
		return -EINVAL;
	}
	if (!task) {
		dev_err(mcfrc_device->dev, "%s: invalid task!\n", __func__);
		return -EINVAL;
	}

	base_addr = mcfrc_device->regs;

	SET_WDMA_FRAME_START_REG(base_addr, 0, 0x1);
	SET_WDMA_FRAME_START_REG(base_addr, 1, 0x1);

	if (task->user_task.config.mode == MCFRC_MODE_FALL_PREV) {
		SET_RDMA_FRAME_START_REG(base_addr, 0, 0x1);
		SET_RDMA_FRAME_START_REG(base_addr, 1, 0x1);
		SET_RDMA_FRAME_START_REG(base_addr, 5, 0x1);
	}
	else if (task->user_task.config.mode == MCFRC_MODE_FALL_CURR) {
		SET_RDMA_FRAME_START_REG(base_addr, 2, 0x1);
		SET_RDMA_FRAME_START_REG(base_addr, 3, 0x1);
		SET_RDMA_FRAME_START_REG(base_addr, 5, 0x1);
	}
	else {
		if (task->user_task.config.dbl_on != 0) {
			SET_WDMA_FRAME_START_REG(base_addr, 2, 0x1);
			SET_WDMA_FRAME_START_REG(base_addr, 3, 0x1);
		}

		SET_RDMA_FRAME_START_REG(base_addr, 0, 0x1);
		SET_RDMA_FRAME_START_REG(base_addr, 1, 0x1);
		SET_RDMA_FRAME_START_REG(base_addr, 2, 0x1);
		SET_RDMA_FRAME_START_REG(base_addr, 3, 0x1);
		if (task->user_task.config.dbl_on != 0) {
			SET_RDMA_FRAME_START_REG(base_addr, 4, 0x1);
		}
		SET_RDMA_FRAME_START_REG(base_addr, 5, 0x1);
	}

#ifdef HWMCFRC_PROFILE_ENABLE
	global_time_start = ktime_get();
#endif

	SET_FRC_MC_START_REG(base_addr, 0x1);

	return 0;
}

static int mcfrc_setup_device_adresses_shadow(  struct mcfrc_dev *mcfrc_device,
												struct mcfrc_task *task)
{
	void __iomem *base_addr;
	u32 width, height;
	u32 offset, uv_offset;

	if (!mcfrc_device) {
		pr_err("%s: invalid mcfrc device!\n", __func__);
		return -EINVAL;
	}
	if (!task) {
		dev_err(mcfrc_device->dev, "%s: invalid task!\n", __func__);
		return -EINVAL;
	}

	base_addr = mcfrc_device->regs;
	width = task->user_task.img_markup.width;
	height = task->user_task.img_markup.height;
	offset = task->user_task.img_markup.offset;
	uv_offset = task->user_task.img_markup.offset_uv;
	//struct mcfrc_buffer_dma dma_buf_in[2];
	//struct mcfrc_buffer_dma dma_buf_out;
	//struct mcfrc_buffer_dma dma_buf_params;
	//struct mcfrc_buffer_dma dma_buf_temp;

	if (task->user_task.config.mode == MCFRC_MODE_FALL_PREV || task->user_task.config.mode == MCFRC_MODE_FULL_CURR || task->user_task.config.mode == MCFRC_MODE_FULL_PREV) {
		//0x0440 RDMA0_CH0_BASEADDR
		//PREV Y
		SET_RDMA_BASEADDR_REG(base_addr, 0, task->dma_buf_in[0].plane.dma_addr + offset);

		//0x04C0 RDMA0_CH1_BASEADDR
		//PREV UV
		SET_RDMA_BASEADDR_REG(base_addr, 1, task->dma_buf_in[0].plane.dma_addr + uv_offset);
	}

	if (task->user_task.config.mode == MCFRC_MODE_FALL_CURR || task->user_task.config.mode == MCFRC_MODE_FULL_CURR || task->user_task.config.mode == MCFRC_MODE_FULL_PREV) {
		//0x0540 RDMA0_CH2_BASEADDR
		//CURR Y
		SET_RDMA_BASEADDR_REG(base_addr, 2, task->dma_buf_in[1].plane.dma_addr + offset);

		//0x05C0 RDMA0_CH3_BASEADDR
		//CURR UV
		SET_RDMA_BASEADDR_REG(base_addr, 3, task->dma_buf_in[1].plane.dma_addr + uv_offset);
	}

	if (task->user_task.config.dbl_on != 0) {
		//0x0540 RDMA0_CH4_BASEADDR
		//OUT Y before DBL
		SET_RDMA_BASEADDR_REG(base_addr, 4, task->dma_buf_out.plane.dma_addr + offset);
	}

	//0x06C0 RDMA0_CH5_BASEADDR_0
	// PARAMS
	SET_RDMA_CH5_BASEADDR_0_REG(base_addr, task->dma_buf_params.plane.dma_addr);

	//0x06C8 RDMA0_CH5_BASEADDR_1
	// HVF
	SET_RDMA_CH5_BASEADDR_1_REG(base_addr, task->dma_buf_temp.plane.dma_addr);

	//0x0840 WDMA0_CH0_BASEADDR
	//OUT Y before DBL
	SET_WDMA_BASEADDR_REG(base_addr, 0, task->dma_buf_out.plane.dma_addr + offset);

	//0x08C0 WDMA0_CH1_BASEADDR
	//OUT UV
	SET_WDMA_BASEADDR_REG(base_addr, 1, task->dma_buf_out.plane.dma_addr + uv_offset);

	if (task->user_task.config.dbl_on != 0) {
		//0x0940 WDMA0_CH2_BASEADDR
		// HVF
		SET_WDMA_BASEADDR_REG(base_addr, 2, task->dma_buf_temp.plane.dma_addr);

		//0x09C0 WDMA0_CH3_BASEADDR
		//OUT Y after DBL
		SET_WDMA_BASEADDR_REG(base_addr, 3, task->dma_buf_out.plane.dma_addr + offset);
	}

	SET_FRC_APB_UPDATE_REG(base_addr, 0x1);

	return 0;
}

static int mcfrc_setup_device_TOP(  struct mcfrc_dev *mcfrc_device,
									struct mcfrc_task *task)
{
	void __iomem *base_addr;
	u32 var;
	u32 width, height;
	bool with_dbl;
	int  done_count_thresh;

	if (!mcfrc_device) {
		pr_err("%s: invalid mcfrc device!\n", __func__);
		return -EINVAL;
	}
	if (!task) {
		dev_err(mcfrc_device->dev, "%s: invalid task!\n", __func__);
		return -EINVAL;
	}

	with_dbl = task->user_task.config.dbl_on != 0;

	base_addr = mcfrc_device->regs;

	//TOP : 0x0010 ~ 0x002C, 0x0034~0x003C, 0x0044, 0x004C, 0x0064

	GET_MC_ERROR_STATUS_REG(base_addr, var);
	dev_dbg(mcfrc_device->dev, "%s: MC_ERROR_STATUS before: 0x%X \n", __func__, var);

	////    0x0068	WC		MC_ERROR_CLEAR
	////            WC[31:0]	MC_ERROR_CLEAR
	SET_MC_ERROR_CLEAR_REG(base_addr, 0xffffffff);

	//0x0010 MC_TIMEOUT
	var = 0;
	INSERT_MC_TIMEOUT_EN(var, 0);
	SET_MC_TIMEOUT_REG(base_addr, var);

	//0x0014 MC_CLKREQ_CONTROL
	//var = 0;
	//SET_MC_CLKREQ_CONTROL_REG(base, var);

	//0x0018 MC_FRAME_SIZE
	var = 0;
	width = task->user_task.img_markup.width;
	height = task->user_task.img_markup.height;
	INSERT_MC_FRAME_WIDTH(var, width);
	INSERT_MC_FRAME_HEIGHT(var, height);
	SET_MC_FRAME_SIZE_REG(base_addr, var);

	//0x001C MC_BYPASS_PHASE
	var = 0;
	/*if (task->user_task.config.mode == MCFRC_MODE_FALL_PREV) {
		INSERT_MC_BYPASS_TYPE(var, 0x3); // bypass prev
		dev_dbg(mcfrc_device->dev, "%s: programming bypass prev \n", __func__);
	}
	else if (task->user_task.config.mode == MCFRC_MODE_FALL_CURR) {
		INSERT_MC_BYPASS_TYPE(var, 0x1); // bypass curr
		dev_dbg(mcfrc_device->dev, "%s: programming bypass curr \n", __func__);
	}
	else  {
		INSERT_MC_BYPASS_TYPE(var, 0x0); // full processing (When timeout bypass case, copy current frame)
		dev_dbg(mcfrc_device->dev, "%s: programming full processing \n", __func__);
	}*/
	INSERT_MC_BYPASS_TYPE(var, task->user_task.config.mode);
	dev_dbg(mcfrc_device->dev, "%s: programming bypass: 0x%X \n", __func__, task->user_task.config.mode);
	INSERT_MC_FRAME_PHASE(var, task->user_task.config.phase);
	SET_MC_BYPASS_PHASE_REG(base_addr, var);

	//0x0020 MC_BATCH_MODE
	var = 0;
	SET_MC_BATCH_MODE_REG(base_addr, var);

	//0x0024 MC_FRAME_DONE_CHECK
	var = 0;
	INSERT_MC_FRAME_DONE_CHECK_EN(var, 1);
	//done_count_thresh = 5607000;
	done_count_thresh = task->user_task.results[1];
	INSERT_MC_FRAME_DONE_COUNT_TH(var, done_count_thresh);
	dev_dbg(mcfrc_device->dev, "%s: programming MC_FRAME_DONE_COUNT_TH = %d \n", __func__, done_count_thresh);
	SET_MC_FRAME_DONE_CHECK_REG(base_addr, var);

	//0x0028 MC_INTR_MASKING
	//var = 0;
	//SET_MC_INTR_MASKING_REG(base, var);

	//0x0034 FRC_DBL_EN
	var = with_dbl ? 0x1: 0x0;
	if(with_dbl)
		dev_dbg(mcfrc_device->dev, "%s: programming deblock on \n", __func__);
	else
		dev_dbg(mcfrc_device->dev, "%s: programming deblock OFF \n", __func__);
	SET_FRC_DBL_EN_REG(base_addr, var);

	//0x0038 FRC_DBL_MODE
	var = task->user_task.config.dbl_on == 1 ? FRC_DBL_MODE_PARALLEL : FRC_DBL_MODE_SERIAL;
	dev_dbg(mcfrc_device->dev, "%s: programming FRC_DBL_MODE = %d \n", __func__, var);
	SET_FRC_DBL_MODE_REG(base_addr, var);

	//0x003C DMA_SBI_RD_CONFIG
	var = 0;
	INSERT_SBI_RD_STOP_REQ(var, 0);
	INSERT_SBI_RD_CONFIG(var, 8);
	SET_DMA_SBI_RD_CONFIG_REG(base_addr, var);

	//0x0044 DMA_SBI_WR_CONFIG
	var = 0;
	INSERT_SBI_WR_STOP_REQ(var, 0);
	INSERT_SBI_WR_CONFIG(var, 8);
	SET_DMA_SBI_WR_CONFIG_REG(base_addr, var);

	//0x004C MC_AXI_USER
	var = 0;
	SET_MC_AXI_USER_REG(base_addr, var);

	//0x0064 MC_ERROR_DEBUG_ENABLE
	var = 0;
	INSERT_DEBUG_MONITORING_EN(var, 1);
	INSERT_MC_ERROR_ENABLE(var, 0xF);
	SET_MC_ERROR_DEBUG_ENABLE_REG(base_addr, var);

	dev_dbg(mcfrc_device->dev, "%s: END TOP regs setup\n", __func__);

	return 0;
}

static int mcfrc_setup_device_WDMA_frame_marking(   struct mcfrc_dev *mcfrc_device,
													struct mcfrc_task *task)
{
	void __iomem *base_addr;
	u32 var, upd_val;
	bool with_dbl;
	u32 width, height, stride, width_uv, height_uv, stride_uv;
	u32 dma_dbg_flags = 0xfffe;

	if (!mcfrc_device) {
		pr_err("%s: invalid mcfrc device!\n", __func__);
		return -EINVAL;
	}
	if (!task) {
		dev_err(mcfrc_device->dev, "%s: invalid task!\n", __func__);
		return -EINVAL;
	}

#if (MCFRC_DBG_MODE == 1) // remove debug parameters
	if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_5, task->user_task.dbg_control_flags)) {
		dma_dbg_flags = 0xffff;
	}
#endif

	base_addr = mcfrc_device->regs;
	width = task->user_task.img_markup.width;
	height = task->user_task.img_markup.height;
	stride = task->user_task.img_markup.stride;
	height_uv = task->user_task.img_markup.height_uv;
	width_uv = width;
	stride_uv = stride;

	with_dbl = task->user_task.config.dbl_on != 0;

	{
		// for with and without deblock both

		u32 tile_width = 256;
		u32 tile_height = 24;
		u32 num_tiles_hori = (width + tile_width - 1)/ tile_width;
		u32 num_tiles_vert = (height + tile_height - 1) / tile_height;
		u32 tile_width_uv = 256;
		u32 tile_height_uv = 12;
		u32 num_tiles_hori_uv = (width_uv + tile_width_uv - 1) / tile_width_uv;
		u32 num_tiles_vert_uv = (height_uv + tile_height_uv - 1) / tile_height_uv;

		//CH0 : 0x0804 ~0x085C  Y before DBL
		//CH1 : 0x0884 ~0x08DC  C

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//	CH0 : 0x0804 ~0x085C (Luma before DBL)

		//0x0804 WDMA0_CH0_TILE_CTRL_FRAME_SIZE
		var = 0;
		INSERT_WDMA_CHn_FRAME_SIZE_H(var, width);
		INSERT_WDMA_CHn_FRAME_SIZE_V(var, height);
		SET_WDMA_TILE_CTRL_FRAME_SIZE_REG(base_addr, 0, var);

		//0x0808 WDMA0_CH0_TILE_CTRL_FRAME_START
		var = 0;
		SET_WDMA_TILE_CTRL_FRAME_START_REG(base_addr, 0, var);

		//0x080C WDMA0_CH0_TILE_CTRL_TILE_SIZE
		var = 0;
		INSERT_WDMA_CHn_TILE_SIZE_H(var, tile_width);
		INSERT_WDMA_CHn_TILE_SIZE_V(var, tile_height);
		SET_WDMA_TILE_CTRL_TILE_SIZE_REG(base_addr, 0, var);

		//0x0810 WDMA0_CH0_TILE_CTRL_TILE_NUM
		var = 0;
		INSERT_WDMA_CHn_TILE_NUM_H(var, num_tiles_hori);
		INSERT_WDMA_CHn_TILE_NUM_V(var, num_tiles_vert);
		SET_WDMA_TILE_CTRL_TILE_NUM_REG(base_addr, 0, var);

		//0x0820 WDMA0_CH0_APB_READ_SEL
		//var = 0;
		//SET_WDMA_APB_READ_SEL_REG(base_addr, 0, var);

		//0x0840 WDMA0_CH0_BASEADDR
		//var = 0;
		//SET_WDMA_BASEADDR_REG(base_addr, 0, var);

		//0x0844 WDMA0_CH0_STRIDE
		var = 0;
		INSERT_WDMA_CHn_STRIDE(var, stride);
		SET_WDMA_STRIDE_REG(base_addr, 0, var);

		//0x0848 WDMA0_CH0_FORMAT
		var = 0;
		INSERT_WDMA0_CH_FMT_PPC(var, WDMA_FORMAT_FLAG_4PPC);
		INSERT_WDMA0_CH_FMT_BPP(var, 8);
		SET_WDMA_FORMAT_REG(base_addr, 0, var);

		//0x084C WDMA0_CH0_CONTROL
		var = 0;
		INSERT_WDMA_CHn_CTRL_FIFO_SIZE(var, 32);
		INSERT_WDMA_CHn_CTRL_FIFO_ADDR(var, 0);
		upd_val = task->user_task.results[16];
		if (upd_val != var)
			dev_dbg(mcfrc_device->dev, "%s: WDMA_CONTROL0: forced 0x%08X instead 0x%08X\n", __func__, upd_val, var);
		SET_WDMA_CONTROL_REG(base_addr, 0, var);

		SET_WDMA_ERROR_CLEAR_REG(base_addr, 0, 0xffff);
		////0x0850	RW		WDMA0_CH0_ERROR_ENABLE
		SET_WDMA_ERROR_ENABLE_REG(base_addr, 0, dma_dbg_flags);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//	CH1 : 0x0884 ~0x08DC  (Chroma)
		//CH1 : 0x0884 WDMA0_CH1_TILE_CTRL_FRAME_SIZE
		var = 0;
		INSERT_WDMA_CHn_FRAME_SIZE_H(var, width);
		INSERT_WDMA_CHn_FRAME_SIZE_V(var, height_uv);
		SET_WDMA_TILE_CTRL_FRAME_SIZE_REG(base_addr, 1, var);

		//CH1 : 0x0888 WDMA0_CH1_TILE_CTRL_FRAME_START
		var = 0;
		SET_WDMA_TILE_CTRL_FRAME_START_REG(base_addr, 1, var);

		//CH1 : 0x088C WDMA0_CH1_TILE_CTRL_TILE_SIZE
		var = 0;
		INSERT_WDMA_CHn_TILE_SIZE_H(var, tile_width_uv);
		INSERT_WDMA_CHn_TILE_SIZE_V(var, tile_height_uv);
		SET_WDMA_TILE_CTRL_TILE_SIZE_REG(base_addr, 1, var);

		//CH1 : 0x0890 WDMA0_CH1_TILE_CTRL_TILE_NUM
		var = 0;
		INSERT_WDMA_CHn_TILE_NUM_H(var, num_tiles_hori_uv);
		INSERT_WDMA_CHn_TILE_NUM_V(var, num_tiles_vert_uv);
		SET_WDMA_TILE_CTRL_TILE_NUM_REG(base_addr, 1, var);

		//CH1 : 0x08A0 WDMA0_CH1_APB_READ_SEL
		//var = 0;
		//SET_WDMA_APB_READ_SEL_REG(base_addr, 1, var);

		//CH1 : 0x08A8 RO WDMA0_CH1_TILE_CTRL_DBG
		//var = 0;

		//CH1 : 0x08AC RO WDMA0_CH1_TILE_CTRL_DBG_TILE
		//var = 0;

		//CH1 : 0x08B0 RO WDMA0_CH1_DBG_STATE
		//var = 0;

		//CH1 : 0x08C0 WDMA0_CH1_BASEADDR
		//var = 0;
		//SET_WDMA_BASEADDR_REG(base_addr, 1, var);

		//CH1 : 0x08C4 WDMA0_CH1_STRIDE
		var = 0;
		INSERT_WDMA_CHn_STRIDE(var, stride_uv);
		SET_WDMA_STRIDE_REG(base_addr, 1, var);

		//CH1 : 0x08C8 WDMA0_CH1_FORMAT
		var = 0;
		INSERT_WDMA0_CH_FMT_PPC(var, WDMA_FORMAT_FLAG_4PPC);
		INSERT_WDMA0_CH_FMT_BPP(var, 8);
		SET_WDMA_FORMAT_REG(base_addr, 1, var);

		//CH1 : 0x08C8 WDMA0_CH1_CONTROL
		var = 0;
		INSERT_WDMA_CHn_CTRL_FIFO_SIZE(var, 32);
		INSERT_WDMA_CHn_CTRL_FIFO_ADDR(var, 32);
		upd_val = task->user_task.results[17];
		if (upd_val != var)
			dev_dbg(mcfrc_device->dev, "%s: WDMA_CONTROL1: forced 0x%08X instead 0x%08X\n", __func__, upd_val, var);
		SET_WDMA_CONTROL_REG(base_addr, 1, upd_val);

		SET_WDMA_ERROR_CLEAR_REG(base_addr, 1, 0xffff);
		////0x08D0	RW		WDMA0_CH1_ERROR_ENABLE
		SET_WDMA_ERROR_ENABLE_REG(base_addr, 1, dma_dbg_flags);
	}

	if (with_dbl) {

		//CH2 : 0x0904 ~0x095C  HVF
		u32 width_in_blk = (width + 7) >> 3;
		u32 height_in_blk = (height + 7) >> 3;
		u32 hvf_width = width_in_blk << 4;
		u32 hvf_height = height_in_blk;

		u32 hvf_tile_width = 512;
		u32 hvf_tile_height = 3;
		u32 num_hvf_tiles_hori = (hvf_width + hvf_tile_width - 1) / hvf_tile_width;
		u32 num_hvf_tiles_vert = (hvf_height + hvf_tile_height - 1) / hvf_tile_height;

		//CH3 : 0x0984 ~0x09DC  Y after DBL
		u32 dblY_width = width-8;
		u32 dblY_height = height-8;
		u32 dblY_tile_width = 256;
		u32 dblY_tile_height = 8;
		u32 num_dblY_tiles_hori = (dblY_width + dblY_tile_width - 1) / dblY_tile_width;
		u32 num_dblY_tiles_vert = (dblY_height + dblY_tile_height - 1) / dblY_tile_height;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//	CH2 : 0x0904 ~ 0x095C  (HVF)

		//CH2 0x0904 WDMA0_CH2_TILE_CTRL_FRAME_SIZE
		var = 0;
		INSERT_WDMA_CHn_FRAME_SIZE_H(var, hvf_width);
		INSERT_WDMA_CHn_FRAME_SIZE_V(var, hvf_height);
		SET_WDMA_TILE_CTRL_FRAME_SIZE_REG(base_addr, 2, var);

		//CH2 0x0908 WDMA0_CH2_TILE_CTRL_FRAME_START
		var = 0;
		SET_WDMA_TILE_CTRL_FRAME_START_REG(base_addr, 2, var);

		//CH2 0x090C WDMA0_CH2_TILE_CTRL_TILE_SIZE
		var = 0;
		INSERT_WDMA_CHn_TILE_SIZE_H(var, hvf_tile_width);
		INSERT_WDMA_CHn_TILE_SIZE_V(var, hvf_tile_height);
		SET_WDMA_TILE_CTRL_TILE_SIZE_REG(base_addr, 2, var);

		//CH2 0x0910 WDMA0_CH2_TILE_CTRL_TILE_NUM
		var = 0;
		INSERT_WDMA_CHn_TILE_NUM_H(var, num_hvf_tiles_hori);
		INSERT_WDMA_CHn_TILE_NUM_V(var, num_hvf_tiles_vert);
		SET_WDMA_TILE_CTRL_TILE_NUM_REG(base_addr, 2, var);

		//CH2 0x0920 WDMA0_CH2_APB_READ_SEL
		//var = 0;
		//CH2 SET_WDMA_APB_READ_SEL_REG(base_addr, 2, var);

		//CH2 0x0940 WDMA0_CH2_BASEADDR
		//var = 0;
		//SET_WDMA_BASEADDR_REG(base_addr, 2, var);

		//CH2 0x0944 WDMA0_CH2_STRIDE
		var = 0;
		INSERT_WDMA_CHn_STRIDE(var, hvf_width);
		SET_WDMA_STRIDE_REG(base_addr, 2, var);

		//CH2 0x0948 WDMA0_CH2_FORMAT
		var = 0;
		INSERT_WDMA0_CH_FMT_PPC(var, WDMA_FORMAT_FLAG_4PPC);
		INSERT_WDMA0_CH_FMT_BPP(var, 8);
		SET_WDMA_FORMAT_REG(base_addr, 2, var);

		//CH2 0x094C WDMA0_CH2_CONTROL
		var = 0;
		INSERT_WDMA_CHn_CTRL_FIFO_SIZE(var, 32);
		INSERT_WDMA_CHn_CTRL_FIFO_ADDR(var, 64);
		upd_val = task->user_task.results[18];
		if (upd_val != var)
			dev_dbg(mcfrc_device->dev, "%s: WDMA_CONTROL2: forced 0x%08X instead 0x%08X\n", __func__, upd_val, var);
		SET_WDMA_CONTROL_REG(base_addr, 2, upd_val);

		SET_WDMA_ERROR_CLEAR_REG(base_addr, 2, 0xffff);
		//CH2 0x0950	RW		WDMA0_CH2_ERROR_ENABLE
		SET_WDMA_ERROR_ENABLE_REG(base_addr, 2, dma_dbg_flags);


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//	CH3 : 0x0984 ~0x09DC  (Luma after DBL)

		//u32 dblY_width = width - 8;
		//u32 dblY_height = height - 8;
		//u32 dblY_tile_width = 256;
		//u32 dblY_tile_height = 8;
		//u32 num_dblY_tiles_hori = (dblY_width + dblY_tile_width - 1) / dblY_tile_width;
		//u32 num_dblY_tiles_vert = (dblY_height + dblY_tile_height - 1) / dblY_tile_height;

		//CH3 : 0x0984 WDMA0_CH3_TILE_CTRL_FRAME_SIZE
		var = 0;
		INSERT_WDMA_CHn_FRAME_SIZE_H(var, dblY_width);
		INSERT_WDMA_CHn_FRAME_SIZE_V(var, dblY_height);
		SET_WDMA_TILE_CTRL_FRAME_SIZE_REG(base_addr, 3, var);

		//CH3 : 0x0988 WDMA0_CH3_TILE_CTRL_FRAME_START
		var = 0;
		INSERT_WDMA_CHn_FRAME_START_X(var, 4);
		INSERT_WDMA_CHn_FRAME_START_Y(var, 4);
		SET_WDMA_TILE_CTRL_FRAME_START_REG(base_addr, 3, var);

		//CH3 : 0x098C WDMA0_CH3_TILE_CTRL_TILE_SIZE
		var = 0;
		INSERT_WDMA_CHn_TILE_SIZE_H(var, dblY_tile_width);
		INSERT_WDMA_CHn_TILE_SIZE_V(var, dblY_tile_height);
		SET_WDMA_TILE_CTRL_TILE_SIZE_REG(base_addr, 3, var);

		//CH3 : 0x0990 WDMA0_CH3_TILE_CTRL_TILE_NUM
		var = 0;
		INSERT_WDMA_CHn_TILE_NUM_H(var, num_dblY_tiles_hori);
		INSERT_WDMA_CHn_TILE_NUM_V(var, num_dblY_tiles_vert);
		SET_WDMA_TILE_CTRL_TILE_NUM_REG(base_addr, 3, var);

		//CH3 : 0x09A0 WDMA0_CH3_APB_READ_SEL
		//var = 0;
		//SET_WDMA_APB_READ_SEL_REG(base_addr, 3, var);

		//CH3 : 0x09A8 RO WDMA0_CH3_TILE_CTRL_DBG
		//var = 0;

		//CH3 : 0x09AC RO WDMA0_CH3_TILE_CTRL_DBG_TILE
		//var = 0;

		//CH3 : 0x09B0 RO WDMA0_CH3_DBG_STATE
		//var = 0;

		//CH3 : 0x09C0 WDMA0_CH3_BASEADDR
		//var = 0;
		//SET_WDMA_BASEADDR_REG(base_addr, 3, var);

		//CH3 : 0x09C4 WDMA0_CH3_STRIDE
		var = 0;
		INSERT_WDMA_CHn_STRIDE(var, stride);
		SET_WDMA_STRIDE_REG(base_addr, 3, var);

		//CH3 : 0x09C8 WDMA0_CH3_FORMAT
		var = 0;
		INSERT_WDMA0_CH_FMT_PPC(var, WDMA_FORMAT_FLAG_4PPC);
		INSERT_WDMA0_CH_FMT_BPP(var, 8);
		SET_WDMA_FORMAT_REG(base_addr, 3, var);

		//CH3 : 0x09C8 WDMA0_CH3_CONTROL
		var = 0;
		INSERT_WDMA_CHn_CTRL_FIFO_SIZE(var, 32);
		INSERT_WDMA_CHn_CTRL_FIFO_ADDR(var, 96);
		upd_val = task->user_task.results[19];
		if (upd_val != var)
			dev_dbg(mcfrc_device->dev, "%s: WDMA_CONTROL3: forced 0x%08X instead 0x%08X\n", __func__, upd_val, var);
		SET_WDMA_CONTROL_REG(base_addr, 3, upd_val);

		SET_WDMA_ERROR_CLEAR_REG(base_addr, 3, 0xffff);
		//CH3 0x09D0 RW WDMA0_CH3_ERROR_ENABLE
		SET_WDMA_ERROR_ENABLE_REG(base_addr, 3, dma_dbg_flags);
	}

	dev_dbg(mcfrc_device->dev, "%s: END WDMA setup\n", __func__);

	return 0;
}

static int mcfrc_setup_device_RDMA_frame_marking(   struct mcfrc_dev *mcfrc_device,
													struct mcfrc_task *task)
{
	u32 width, height, stride;
	u32 width_uv, height_uv, stride_uv, params_stride, hvf_stride;
	void __iomem *base_addr;
	u32 var;
	u32 dma_dbg_flags = 0xfffe;
	bool with_dbl;
	u32 upd_val;

	if (!mcfrc_device) {
		pr_err("%s: invalid mcfrc device!\n", __func__);
		return -EINVAL;
	}
	if (!task) {
		dev_err(mcfrc_device->dev, "%s: invalid task!\n", __func__);
		return -EINVAL;
	}

#if (MCFRC_DBG_MODE == 1) // remove debug parameters
	if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_5, task->user_task.dbg_control_flags)) {
		dma_dbg_flags = 0xffff;
	}
#endif

	with_dbl = task->user_task.config.dbl_on != 0;

	base_addr = mcfrc_device->regs;
	width = task->user_task.img_markup.width;
	height = task->user_task.img_markup.height;
	stride = task->user_task.img_markup.stride;
	width = task->user_task.img_markup.width;
	height = task->user_task.img_markup.height;
	height_uv = task->user_task.img_markup.height_uv;
	width_uv = width;
	stride_uv = stride;
	params_stride = ((width + 7) >> 3) << 4;
	hvf_stride = ((width + 7) >> 3) << 4;

	{
		// for with and without deblock both

		//CH0 : 0x0440 ~0x045C  PREV_Y
		//CH1 : 0x04C0 ~0x04DC  PREV_C
		//CH2 : 0x0540 ~0x055C  CURR_Y
		//CH3 : 0x05C0 ~0x05DC  CURR_C

		//CH5 : 0x06C0 ~0x06E4  PARA/HVF

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//CH0 : 0x0440 ~0x045C  PREV_Y

		//0x0440 RDMA0_CH0_BASEADDR
		//var = 0;
		//SET_RDMA_BASEADDR_REG(base_addr, 0, var);

		//0x0444 RDMA0_CH0_STRIDE
		var = 0;
		INSERT_RDMA_CHn_STRIDE(var, stride);
		SET_RDMA_STRIDE_REG(base_addr, 0, var);

		//0x044C RDMA0_CH0_CONTROL
		var = 0;
		INSERT_RDMA_CHn_CTRL_FIFO_SIZE(var, 128);
		INSERT_RDMA_CHn_CTRL_FIFO_ADDR(var, 0);
		upd_val = task->user_task.results[10];
		if(upd_val!= var)
			dev_dbg(mcfrc_device->dev, "%s: RDMA_CONTROL0: forced 0x%08X instead 0x%08X\n", __func__, upd_val, var);
		SET_RDMA_CONTROL_REG(base_addr, 0, upd_val);

		SET_RDMA_ERROR_CLEAR_REG(base_addr, 0, 0xffff);
		//0x0450 RDMA0_CH0_ERROR_ENABLE
		var = dma_dbg_flags;
		SET_RDMA_ERROR_ENABLE_REG(base_addr, 0, var);

		//0x045C RDMA0_CH0_OFFSET_ADDR
		//var = 0;
		//SET_RDMA_OFFSET_ADDR_REG(base_addr, 0, val);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//CH1 : 0x04C0 ~0x04DC  PREV_C

		//0x04C0 RDMA0_CH1_BASEADDR
		//var = 0;
		//SET_RDMA_BASEADDR_REG(base_addr, 1, var);

		//0x04C4 RDMA0_CH1_STRIDE
		var = 0;
		INSERT_RDMA_CHn_STRIDE(var, stride);
		SET_RDMA_STRIDE_REG(base_addr, 1, var);

		//0x04CC RDMA0_CH1_CONTROL
		var = 0;
		INSERT_RDMA_CHn_CTRL_FIFO_SIZE(var, 64);
		INSERT_RDMA_CHn_CTRL_FIFO_ADDR(var, 128);
		upd_val = task->user_task.results[11];
		if (upd_val != var)
			dev_dbg(mcfrc_device->dev, "%s: RDMA_CONTROL1: forced 0x%08X instead 0x%08X\n", __func__, upd_val, var);
		SET_RDMA_CONTROL_REG(base_addr, 1, upd_val);

		SET_RDMA_ERROR_CLEAR_REG(base_addr, 1, 0xffff);
		//0x04D0 RDMA0_CH1_ERROR_ENABLE
		var = dma_dbg_flags;
		SET_RDMA_ERROR_ENABLE_REG(base_addr, 1, var);

		//0x045C RDMA0_CH1_OFFSET_ADDR
		//var = 0;
		//SET_RDMA_OFFSET_ADDR_REG(base_addr, 1, val)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//CH2 : 0x0540 ~0x055C  CURR_Y

		//0x0540 RDMA0_CH2_BASEADDR
		//var = 0;
		//SET_RDMA_BASEADDR_REG(base_addr, 2, var);

		//0x0544 RDMA0_CH2_STRIDE
		var = 0;
		INSERT_RDMA_CHn_STRIDE(var, stride);
		SET_RDMA_STRIDE_REG(base_addr, 2, var);

		//0x054C RDMA0_CH2_CONTROL
		var = 0;
		INSERT_RDMA_CHn_CTRL_FIFO_SIZE(var, 128);
		INSERT_RDMA_CHn_CTRL_FIFO_ADDR(var, 192);
		upd_val = task->user_task.results[12];
		if (upd_val != var)
			dev_dbg(mcfrc_device->dev, "%s: RDMA_CONTROL2: forced 0x%08X instead 0x%08X\n", __func__, upd_val, var);
		SET_RDMA_CONTROL_REG(base_addr, 2, upd_val);

		SET_RDMA_ERROR_CLEAR_REG(base_addr, 2, 0xffff);
		//0x0550 RDMA0_CH2_ERROR_ENABLE
		var = dma_dbg_flags;
		SET_RDMA_ERROR_ENABLE_REG(base_addr, 2, var);

		//0x055C RDMA0_CH2_OFFSET_ADDR
		//var = 0;
		//SET_RDMA_OFFSET_ADDR_REG(base_addr, 2, var);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//CH3 : 0x05C0 ~0x05DC  CURR_C

		//0x05C0 RDMA0_CH3_BASEADDR
		//var = 0;
		//SET_RDMA_BASEADDR_REG(base_addr, 3, var);

		//0x05C4 RDMA0_CH3_STRIDE
		var = 0;
		INSERT_RDMA_CHn_STRIDE(var, stride);
		SET_RDMA_STRIDE_REG(base_addr, 3, var);

		//0x05CC RDMA0_CH3_CONTROL
		var = 0;
		INSERT_RDMA_CHn_CTRL_FIFO_SIZE(var, 64);
		INSERT_RDMA_CHn_CTRL_FIFO_ADDR(var, 320);
		upd_val = task->user_task.results[13];
		if (upd_val != var)
			dev_dbg(mcfrc_device->dev, "%s: RDMA_CONTROL3: forced 0x%08X instead 0x%08X\n", __func__, upd_val, var);
		SET_RDMA_CONTROL_REG(base_addr, 3, upd_val);

		SET_RDMA_ERROR_CLEAR_REG(base_addr, 3, 0xffff);
		//0x05D0 RDMA0_CH3_ERROR_ENABLE
		var = dma_dbg_flags;
		SET_RDMA_ERROR_ENABLE_REG(base_addr, 3, var);

		//0x055C RDMA0_CH3_OFFSET_ADDR
		//var = 0;
		//SET_RDMA_OFFSET_ADDR_REG(base_addr, 3, var);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//CH5 : 0x06C0 ~0x06E4  PARA/HVF

		//0x06C0 RDMA0_CH5_BASEADDR_0
		//var = 0;
		//SET_RDMA_CH5_BASEADDR_0_REG(base_addr, var);

		//0x06C4 RDMA0_CH5_STRIDE_0
		var = 0;
		INSERT_RDMA_CHn_STRIDE(var, params_stride);
		SET_RDMA_CH5_STRIDE_0_REG(base_addr, var);

		//0x06C8 RDMA0_CH5_BASEADDR_1
		//var = 0;
		//SET_RDMA_CH5_BASEADDR_1_REG(base_addr, var);

		//0x06CC RDMA0_CH5_STRIDE_1
		var = 0;
		INSERT_RDMA_CHn_STRIDE(var, hvf_stride);
		SET_RDMA_CH5_STRIDE_1_REG(base_addr, var);

		//0x06D0 RDMA0_CH5_CONTROL
		var = 0;
		INSERT_RDMA_CHn_CTRL_FIFO_SIZE(var, 64);
		INSERT_RDMA_CHn_CTRL_FIFO_ADDR(var, 448);
		upd_val = task->user_task.results[15];
		if (upd_val != var)
			dev_dbg(mcfrc_device->dev, "%s: RDMA_CONTROL5: forced 0x%08X instead 0x%08X\n", __func__, upd_val, var);
		SET_RDMA_CH5_CONTROL_01_REG(base_addr, upd_val);

		SET_RDMA0_CH5_ERROR_CLEAR_REG(base_addr, 0xffff);
		//0x06D0 RDMA0_CH5_ERROR_ENABLE
		var = dma_dbg_flags;
		SET_RDMA_CH5_ERROR_ENABLE_01_REG(base_addr, var);

		//0x06E0 RDMA0_CH5_OFFSET_ADDR_0
		//var = 0;
		//SET_RDMA_CH5_OFFSET_ADDR_0(base_addr, var);

		//0x06E4 RDMA0_CH5_OFFSET_ADDR_1
		//var = 0;
		//SET_RDMA_CH5_OFFSET_ADDR_1(base_addr, var);

	}

	if (with_dbl){
		//CH4 : 0x0640 ~0x065C  BEFORE_DBL_OUT_Y

		//0x0640 RDMA0_CH4_BASEADDR
		//var = 0;
		//SET_RDMA_BASEADDR_REG(base_addr, 4, var);

		//0x0644 RDMA0_CH4_STRIDE
		var = 0;
		INSERT_RDMA_CHn_STRIDE(var, stride);
		SET_RDMA_STRIDE_REG(base_addr, 4, var);

		//0x064C RDMA0_CH4_CONTROL
		var = 0;
		INSERT_RDMA_CHn_CTRL_FIFO_SIZE(var, 64);
		INSERT_RDMA_CHn_CTRL_FIFO_ADDR(var, 384);
		upd_val = task->user_task.results[14];
		if (upd_val != var)
			dev_dbg(mcfrc_device->dev, "%s: RDMA_CONTROL4: forced 0x%08X instead 0x%08X\n", __func__, upd_val, var);
		SET_RDMA_CONTROL_REG(base_addr, 4, upd_val);

		SET_RDMA_ERROR_CLEAR_REG(base_addr, 4, 0xffff);
		//0x0650 RDMA0_CH4_ERROR_ENABLE
		var = dma_dbg_flags;
		SET_RDMA_ERROR_ENABLE_REG(base_addr, 4, var);

		//0x065C RDMA0_CH4_OFFSET_ADDR
		//var = 0;
		//SET_RDMA_OFFSET_ADDR_REG(base_addr, 4, var);
	}

	dev_dbg(mcfrc_device->dev, "%s: END RDMA setup\n", __func__);

	return 0;
}


/*
static int mcfrc_run_sqz(struct mcfrc_ctx *mcfrc_context,
			   struct mcfrc_task *task)
{
	struct mcfrc_dev *mcfrc_device = mcfrc_context->mcfrc_dev;
	int ret = 0;
	unsigned long flags = 0;

	if (!mcfrc_device) {
		pr_err("mcfrc is null\n");
		return -EINVAL;
	}

	dev_dbg(mcfrc_device->dev, "%s: BEGIN\n", __func__);

	spin_lock_irqsave(&mcfrc_device->slock, flags);
	if (test_bit(DEV_SUSPEND, &mcfrc_device->state)) {
		spin_unlock_irqrestore(&mcfrc_device->slock, flags);

		dev_err(mcfrc_device->dev,
			"%s: unexpected suspended device state right before running a task\n", __func__);
		return -1;
	}
	spin_unlock_irqrestore(&mcfrc_device->slock, flags);


	dev_dbg(mcfrc_device->dev, "%s: resetting mcfrc device\n", __func__);

	// H/W initialization: this will reset all registers to reset values
	mcfrc_sw_reset(mcfrc_device->regs);

	dev_dbg(mcfrc_device->dev, "%s: setting source addresses\n", __func__);

	// setting for source
	ret = mcfrc_set_source_n_config(mcfrc_device, task);
	if (ret) {
		dev_err(mcfrc_device->dev, "%s: error setting source\n", __func__);
		return ret;
	}

	dev_dbg(mcfrc_device->dev, "%s: enabling interrupts\n", __func__);
	mcfrc_interrupt_enable(mcfrc_device->regs);

#ifdef HWMCFRC_PROFILE_ENABLE
	global_time_start = ktime_get();
#endif

	// run H/W
	mcfrc_hw_start(mcfrc_device->regs);

	dev_dbg(mcfrc_device->dev, "%s: END\n", __func__);

	return 0;
}

static int mcfrc_run_msqz(struct mcfrc_ctx *mcfrc_context,
			   struct mcfrc_task *task)
{
	struct mcfrc_dev *mcfrc_device = mcfrc_context->mcfrc_dev;
	int ret = 0;
	unsigned long flags = 0;

	if (!mcfrc_device) {
		pr_err("mcfrc device is null\n");
		return -EINVAL;
	}

	dev_dbg(mcfrc_device->dev, "%s: BEGIN\n", __func__);

	spin_lock_irqsave(&mcfrc_device->slock, flags);
	if (test_bit(DEV_SUSPEND, &mcfrc_device->state)) {
		spin_unlock_irqrestore(&mcfrc_device->slock, flags);

		dev_err(mcfrc_device->dev,
			"%s: unexpected suspended device state right before running a task\n", __func__);
		return -1;
	}
	spin_unlock_irqrestore(&mcfrc_device->slock, flags);


	dev_dbg(mcfrc_device->dev, "%s: resetting mcfrc device\n", __func__);

	// H/W initialization: this will reset all registers to reset values
	mcfrc_sw_reset(mcfrc_device->regs);

	dev_dbg(mcfrc_device->dev, "%s: setting source addresses\n", __func__);

	// setting for source
	ret = mcfrc_set_source_n_config_msqz(mcfrc_device, task);
	if (ret) {
		dev_err(mcfrc_device->dev, "%s: error setting source\n", __func__);
		return ret;
	}

	dev_dbg(mcfrc_device->dev, "%s: enabling interrupts\n", __func__);
	mcfrc_interrupt_enable(mcfrc_device->regs);

#ifdef HWMCFRC_PROFILE_ENABLE
	global_time_start = ktime_get();
#endif

	// run H/W
	mcfrc_hw_start(mcfrc_device->regs);

	dev_dbg(mcfrc_device->dev, "%s: END\n", __func__);

	return 0;
}
*/

static int mcfrc_run(   struct mcfrc_ctx *mcfrc_context,
						struct mcfrc_task *task)
{
    struct mcfrc_dev *mcfrc_device = mcfrc_context->mcfrc_dev;
    int ret = 0;
    unsigned long flags = 0;

	if (!mcfrc_device) {
		pr_err("mcfrc device is null\n");
		return -EINVAL;
	}

	dev_dbg(mcfrc_device->dev, "%s: BEGIN\n", __func__);

	spin_lock_irqsave(&mcfrc_device->slock, flags);
	if (test_bit(DEV_SUSPEND, &mcfrc_device->state)) {
		spin_unlock_irqrestore(&mcfrc_device->slock, flags);

		dev_err(mcfrc_device->dev,
			"%s: unexpected suspended device state right before running a task\n", __func__);
		return -1;
	}
	spin_unlock_irqrestore(&mcfrc_device->slock, flags);

	// Cavy dev_dbg(mcfrc_device->dev, "%s: resetting mcfrc device\n", __func__);
	// H/W initialization: this will reset all registers to reset values
	// Cavy mcfrc_sw_reset(mcfrc_device->regs);

	dev_dbg(mcfrc_device->dev, "%s: setting source addresses\n", __func__);

	// setting for source
	ret = mcfrc_setup_device_TOP(mcfrc_device, task);
	if (ret) {
		dev_err(mcfrc_device->dev, "%s: error setting TOP\n", __func__);
		return ret;
	}
	ret = mcfrc_setup_device_RDMA_frame_marking(mcfrc_device, task);
	if (ret) {
		dev_err(mcfrc_device->dev, "%s: error setting RDMA\n", __func__);
		return ret;
	}
	ret = mcfrc_setup_device_WDMA_frame_marking(mcfrc_device, task);
	if (ret) {
		dev_err(mcfrc_device->dev, "%s: error setting WDMA\n", __func__);
		return ret;
	}
	ret = mcfrc_setup_device_adresses_shadow(mcfrc_device, task);
	if (ret) {
		dev_err(mcfrc_device->dev, "%s: error setting adresses_shadow\n", __func__);
		return ret;
	}

	// Cavy dev_dbg(mcfrc_device->dev, "%s: enabling interrupts\n", __func__);
	// Cavy mcfrc_interrupt_enable(mcfrc_device->regs);

#if (MCFRC_DBG_MODE == 1) // remove debug parameters
	if (!CAVY_TEST_BIT(DBG_CONTROL_FLAG_2, task->user_task.dbg_control_flags)) {
		// run H/W
		int var;

		// mask all interrupts
		if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_4, task->user_task.dbg_control_flags)) {
			var = 0;
			INSERT_FRC_MC_DONE_IRQ(var, 0x1);//                CAVY_INSERT_VAL(var, val, 16, 16)
			INSERT_FRC_MC_DBL_ERR_IRQ(var, 0x1);//             CAVY_INSERT_VAL(var, val, 12, 12)
			INSERT_SRST_DONE_IRQ(var, 0x1);//                  CAVY_INSERT_VAL(var, val, 8 , 8 )
			INSERT_FRC_DBL_M_DONE_IRQ(var, 0x1);//             CAVY_INSERT_VAL(var, val, 4 , 4 )
			INSERT_FRC_MC_ERROR_IRQ(var, 0x1);//               CAVY_INSERT_VAL(var, val, 0 , 0 )
			SET_MC_INTR_MASKING_REG(mcfrc_device->regs, var);//         writel(val, base_addr+REG_TOP_0x0028_MC_INTR_MASKING)
			var = 0;
			GET_MC_INTR_MASKING_REG(mcfrc_device->regs, var);//         (val = readl(base_addr+REG_TOP_0x0028_MC_INTR_MASKING))
			dev_dbg(mcfrc_device->dev, "%s: MC_INTR_MASKING  0x%08X \n", __func__, var);

			task->state = MCFRC_BUFSTATE_DONE;
		}

		if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_11, task->user_task.dbg_control_flags)) {
			//alternative start
			mcfrc_run_programm(mcfrc_device, &(task->user_task.programms[1]));
		}
		else {
			// Cavy mcfrc_hw_start(mcfrc_device->regs);
			mcfrc_setup_device_start(mcfrc_device, task);
		}

		if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_3, task->user_task.dbg_control_flags)) {
			msleep_interruptible(100);

			var = 0;
			GET_MC_ERROR_STATUS_REG(mcfrc_device->regs, var);
			dev_dbg(mcfrc_device->dev, "%s: MC_ERROR_STATUS after: 0x%X \n", __func__, var);
			var = 0;
			GET_MC_CORE_STATUS_0_REG(mcfrc_device->regs, var);
			dev_dbg(mcfrc_device->dev, "%s: MC_CORE_STATUS_0 after: 0x%X \n", __func__, var);
			var = 0;
			GET_MC_CORE_STATUS_1_REG(mcfrc_device->regs, var);
			dev_dbg(mcfrc_device->dev, "%s: MC_CORE_STATUS_1 after: 0x%X \n", __func__, var);
			var = 0;
			GET_FRC_MC_DONE_IRQ_STATUS_REG(mcfrc_device->regs, var);
			dev_dbg(mcfrc_device->dev, "%s: FRC_MC_DONE_IRQ_STATUS after: 0x%X \n", __func__, var);
			var = 0;
			GET_INTR_MASK_DBG_STATUS_REG(mcfrc_device->regs, var);
			dev_dbg(mcfrc_device->dev, "%s: INTR_MASK_DBG_STATUS after: 0x%X \n", __func__, var);
			var = 0;
			GET_FRC_MC_DBL_ERR_IRQ_STATUS_REG(mcfrc_device->regs, var);
			dev_dbg(mcfrc_device->dev, "%s:FRC_MC_DBL_ERR_IRQ_STATUS after: 0x%X \n", __func__, var);

			var = 0;
			GET_RDMA_ERROR_STATUS_REG(mcfrc_device->regs, 0, var);
			dev_dbg(mcfrc_device->dev, "%s:RDMA_ERROR_STATUS 0 after: 0x%X \n", __func__, var);
			var = 0;
			GET_RDMA_ERROR_STATUS_REG(mcfrc_device->regs, 1, var);
			dev_dbg(mcfrc_device->dev, "%s:RDMA_ERROR_STATUS 1 after: 0x%X \n", __func__, var);
			var = 0;
			GET_RDMA_ERROR_STATUS_REG(mcfrc_device->regs, 2, var);
			dev_dbg(mcfrc_device->dev, "%s:RDMA_ERROR_STATUS 2 after: 0x%X \n", __func__, var);
			var = 0;
			GET_RDMA_ERROR_STATUS_REG(mcfrc_device->regs, 3, var);
			dev_dbg(mcfrc_device->dev, "%s:RDMA_ERROR_STATUS 3 after: 0x%X \n", __func__, var);
			var = 0;
			GET_RDMA_ERROR_STATUS_REG(mcfrc_device->regs, 4, var);
			dev_dbg(mcfrc_device->dev, "%s:RDMA_ERROR_STATUS 4 after: 0x%X \n", __func__, var);
			var = 0;
			GET_RDMA_CH5_ERROR_STATUS_01_REG(mcfrc_device->regs, var);
			dev_dbg(mcfrc_device->dev, "%s:RDMA_CH5_ERROR_STATUS_01after: 0x%X \n", __func__, var);


			var = 0;
			GET_WDMA_ERROR_STATUS_REG(mcfrc_device->regs, 0, var);
			dev_dbg(mcfrc_device->dev, "%s:WDMA_ERROR_STATUS 0 after: 0x%X \n", __func__, var);
			var = 0;
			GET_WDMA_ERROR_STATUS_REG(mcfrc_device->regs, 1, var);
			dev_dbg(mcfrc_device->dev, "%s:WDMA_ERROR_STATUS 1 after: 0x%X \n", __func__, var);
			var = 0;
			GET_WDMA_ERROR_STATUS_REG(mcfrc_device->regs, 2, var);
			dev_dbg(mcfrc_device->dev, "%s:WDMA_ERROR_STATUS 0 after: 0x%X \n", __func__, var);
			var = 0;
			GET_WDMA_ERROR_STATUS_REG(mcfrc_device->regs, 3, var);
			dev_dbg(mcfrc_device->dev, "%s:WDMA_ERROR_STATUS 1 after: 0x%X \n", __func__, var);
		}

	}
	else {
		task->state = MCFRC_BUFSTATE_DONE;
		//wake up the task processing function and let it return
		complete(&task->complete);
	}
#else
	mcfrc_setup_device_start(mcfrc_device, task);
#endif

	dev_dbg(mcfrc_device->dev, "%s: END\n", __func__);

	return 0;
}

void mcfrc_task_schedule(struct mcfrc_task *task)
{
	struct mcfrc_dev *mcfrc_device;
	unsigned long flags;
	//u32 error_flag = 0;
	int ret = 0;
	//int i = 0;

	mcfrc_device = task->ctx->mcfrc_dev;

	dev_dbg(mcfrc_device->dev, "%s: BEGIN\n", __func__);

	if (test_bit(DEV_RUN, &mcfrc_device->state)) {
		dev_dbg(mcfrc_device->dev
			, "%s: unexpected state (ctx %p task %p)\n"
			, __func__, task->ctx, task);
	}

	while (true) {
		spin_lock_irqsave(&mcfrc_device->slock, flags);
		if (!test_bit(DEV_SUSPEND, &mcfrc_device->state)) {
			set_bit(DEV_RUN, &mcfrc_device->state);
			dev_dbg(mcfrc_device->dev,
				"%s: HW is active, continue.\n", __func__);
			spin_unlock_irqrestore(&mcfrc_device->slock, flags);
			break;
		}
		spin_unlock_irqrestore(&mcfrc_device->slock, flags);

		dev_dbg(mcfrc_device->dev,
			"%s: waiting on system resume...\n", __func__);

		wait_event(mcfrc_device->suspend_wait,
			!test_bit(DEV_SUSPEND, &mcfrc_device->state));
	}

	ret = enable_mcfrc(mcfrc_device);
	if (ret) {
		task->state = MCFRC_BUFSTATE_RETRY;
		goto err;
	}

#if (MCFRC_DBG_MODE == 1) // remove debug parameters
	if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_6, task->user_task.dbg_control_flags)) {
		if (!IS_ERR(mcfrc_device->clk_producer)) {
			unsigned long rate;
			rate = clk_get_rate(mcfrc_device->clk_producer);
			dev_info(mcfrc_device->dev, "mcfrc_task_schedule: clk_get_rate() returns: %lu Hz\n", rate);
		}
		else {
			dev_info(mcfrc_device->dev, "mcfrc_task_schedule: IS_ERR(mcfrc->clk_producer)\n");
		}
	}
	if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_7, task->user_task.dbg_control_flags)) {
		if (!IS_ERR(mcfrc_device->clk_producer)) {
			unsigned long rate, rate_round;
			rate = 663000000;
			rate_round = clk_round_rate(mcfrc_device->clk_producer, rate);
			dev_info(mcfrc_device->dev, "mcfrc_task_schedule: clk_round_rate() returns: %lu Hz\n", rate_round);
		}
		else {
			dev_info(mcfrc_device->dev, "mcfrc_task_schedule: IS_ERR(mcfrc->clk_producer)\n");
		}
	}
#endif
	//if (device_get_dma_attr(mcfrc_device->dev) == DEV_DMA_NON_COHERENT) {
	//	if (task->dma_buf_out.plane.dmabuf)
	//		exynos_ion_sync_dmabuf_for_device(mcfrc_device->dev,
	//			task->dma_buf_out.plane.dmabuf, task->dma_buf_out.plane.bytes_used, DMA_TO_DEVICE);
	//}


	spin_lock_irqsave(&mcfrc_device->lock_task, flags);
	mcfrc_device->current_task = task;
	spin_unlock_irqrestore(&mcfrc_device->lock_task, flags);

#if (MCFRC_DBG_MODE == 1) // remove debug parameters
	if (!CAVY_TEST_BIT(DBG_CONTROL_FLAG_1, task->user_task.dbg_control_flags)) {
		ret = mcfrc_run(task->ctx, task);

		if (ret) {
			task->state = MCFRC_BUFSTATE_ERROR;
			dev_err(mcfrc_device->dev, "%s: mcfrc_run returns 0!=ret\n", __func__);

			spin_lock_irqsave(&mcfrc_device->lock_task, flags);
			mcfrc_device->current_task = NULL;
			spin_unlock_irqrestore(&mcfrc_device->lock_task, flags);
		}
		else {
			if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_4, task->user_task.dbg_control_flags))
			{
				dev_dbg(mcfrc_device->dev, "%s: dont wait completion because DBG_CONTROL_FLAG_4\n", __func__);
			}
			else {
				dev_dbg(mcfrc_device->dev, "%s: wait completion\n", __func__);
				wait_for_completion(&task->complete);
			}
//#ifdef DEBUG
//            mcfrc_print_all_regs(mcfrc_device);
//#endif
		}
	}
#else
	ret = mcfrc_run(task->ctx, task);

	if (ret) {
		task->state = MCFRC_BUFSTATE_ERROR;
		dev_err(mcfrc_device->dev, "%s: mcfrc_run returns 0!=ret\n", __func__);

		spin_lock_irqsave(&mcfrc_device->lock_task, flags);
		mcfrc_device->current_task = NULL;
		spin_unlock_irqrestore(&mcfrc_device->lock_task, flags);
	}
	else {
		dev_dbg(mcfrc_device->dev, "%s: wait completion\n", __func__);
		wait_for_completion(&task->complete);
	}
#endif
	/* Cavy else {
		dev_dbg(mcfrc_device->dev, "%s: wait completion\n", __func__);
		wait_for_completion(&task->complete);
#ifdef DEBUG
		mcfrc_print_all_regs(mcfrc_device);
#endif
		error_flag = mcfrc_get_error_flags(mcfrc_device->regs);
		if (task->type == JPEG_SQZ) {
			if (error_flag & 0x001f0fff) {
				for (i = 0; i < 32; i++) {
					task->user_task.buf_q[i] = init_q_table[i];
				}
				if (error_flag & 0x00000fff) {
					dev_err(mcfrc_device->dev, "%s: mcfrc SFR ERROR!!!, %x\n", __func__, error_flag);
				} else if (error_flag & 0x000f0000) {
					dev_err(mcfrc_device->dev, "%s: mcfrc AXI ERROR!!!, %x\n", __func__, error_flag);
				} else {
					dev_info(mcfrc_device->dev, "%s: mcfrc device time out!\n", __func__);
				}
			}
			else {
				mcfrc_get_output_regs(mcfrc_device->regs, task->user_task.buf_q);
			}
		}
		else if (task->type == MPEG_SQZ) {
			if (error_flag & 0x001f0fff) {
				if (error_flag & 0x00000fff) {
					dev_err(mcfrc_device->dev, "%s: MSQZ SFR ERROR!!!, %x\n", __func__, error_flag);
				} else if (error_flag & 0x000f0000) {
					dev_err(mcfrc_device->dev, "%s: MSQZ AXI ERROR!!!, %x\n", __func__, error_flag);
				} else {
					dev_info(mcfrc_device->dev, "%s: msqz device time out!\n", __func__);
				}
			}
			else {
				task->user_mtask.frame_dqp = mcfrc_get_frame_dqp(mcfrc_device->regs);
			}
		}
		else {
			dev_err(mcfrc_device->dev, "%s: Task Type is wrong!!!\n", __func__);
		}
	}*/
	//mcfrc_sw_reset(mcfrc_device->regs);

	disable_mcfrc(mcfrc_device);

err:
	clear_bit(DEV_RUN, &mcfrc_device->state);
	if (test_bit(DEV_SUSPEND, &mcfrc_device->state)) {
		wake_up(&mcfrc_device->suspend_wait);
	}
	dev_dbg(mcfrc_device->dev, "%s: END\n", __func__);
	return;
}


static void mcfrc_task_schedule_work(struct work_struct *work)
{
	mcfrc_task_schedule(container_of(work, struct mcfrc_task, work));
}


static int mcfrc_task_signal_completion(struct mcfrc_dev *mcfrc_device,
										struct mcfrc_task *task, bool success)
{
	unsigned long flags;

	//dereferencing the ptr will produce an oops and the program will be
	//killed, but at least we don't crash the kernel using BUG_ON
	if (!mcfrc_device) {
		pr_err("HWmcfrc: NULL mcfrc device!\n");
		return -EINVAL;
	}
	if (!task) {
		dev_err(mcfrc_device->dev
			, "%s; finishing a NULL task!\n", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&mcfrc_device->lock_task, flags);
	dev_dbg(mcfrc_device->dev
		, "%s: resetting current task var...\n", __func__);

	if (!mcfrc_device->current_task) {
		dev_warn(mcfrc_device->dev
			 , "%s: invalid current task!\n", __func__);
	}
	if (mcfrc_device->current_task != task) {
		dev_warn(mcfrc_device->dev
			 , "%s: finishing an unexpected task!\n", __func__);
	}

	mcfrc_device->current_task = NULL;
	spin_unlock_irqrestore(&mcfrc_device->lock_task, flags);

	task->state = success ? MCFRC_BUFSTATE_DONE : MCFRC_BUFSTATE_ERROR;

	dev_dbg(mcfrc_device->dev
		, "%s: signalling task completion...\n", __func__);

	//wake up the task processing function and let it return
	complete(&task->complete);

	return 0;
}

/*
static void mcfrc_task_cancel(struct mcfrc_dev *mcfrc_device,
				 struct mcfrc_task *task,
				 enum mcfrc_state reason)
{
	unsigned long flags;

	spin_lock_irqsave(&mcfrc_device->lock_task, flags);
	dev_dbg(mcfrc_device->dev
		, "%s: resetting current task var...\n", __func__);

	mcfrc_device->current_task = NULL;
	spin_unlock_irqrestore(&mcfrc_device->lock_task, flags);

	task->state = reason;

	dev_dbg(mcfrc_device->dev
		, "%s: signalling task completion...\n", __func__);

	complete(&task->complete);

	dev_dbg(mcfrc_device->dev, "%s: scheduling a new task...\n", __func__);

	mcfrc_task_schedule(task);
}
*/

/**
 * Find the VMA that holds the input virtual address, and check
 * if it's associated to a fd representing a DMA buffer.
 * If it is, acquire that dma buffer (i.e. increase refcount on its fd).
 */

static struct dma_buf *mcfrc_get_dmabuf_from_userptr(
		struct mcfrc_dev *mcfrc_device, unsigned long start, size_t len,
		off_t *out_offset)
{
	struct dma_buf *dmabuf = NULL;
#if 0
	struct vm_area_struct *vma;

	dev_dbg(mcfrc_device->dev, "%s: BEGIN\n", __func__);

	down_read(&current->mm->mmap_sem);
	dev_dbg(mcfrc_device->dev
		, "%s: virtual address space semaphore acquired\n", __func__);

	vma = find_vma(current->mm, start);

	if (!vma || (start < vma->vm_start)) {
		dev_err(mcfrc_device->dev
			, "%s: Incorrect user buffer @ %#lx/%#zx\n"
			, __func__, start, len);
		dmabuf = ERR_PTR(-EINVAL);
		goto finish;
	}

	dev_dbg(mcfrc_device->dev
		, "%s: VMA found, %p. Starting at %#lx within parent area\n"
		, __func__, vma, vma->vm_start);

	//vm_file is the file that this vma is a memory mapping of
	if (!vma->vm_file) {
		dev_dbg(mcfrc_device->dev, "%s: this VMA does not map a file\n",
			__func__);
		goto finish;
	}

	// get_dma_buf_file is not support now
	//dmabuf = get_dma_buf_file(vma->vm_file);
	dev_dbg(mcfrc_device->dev
		, "%s: got dmabuf %p of vm_file %p\n", __func__, dmabuf
		, vma->vm_file);

	if (dmabuf != NULL)
		*out_offset = start - vma->vm_start;
finish:
	up_read(&current->mm->mmap_sem);
	dev_dbg(mcfrc_device->dev
		, "%s: virtual address space semaphore released\n", __func__);
#endif
	return dmabuf;
}


/**
 * If the buffer we received from userspace is an fd representing a DMA buffer,
 * (HWSQZ_BUFFER_DMABUF) then we attach to it.
 *
 * If it's a pointer to user memory (HWSQZ_BUFFER_USERPTR), then we check if
 * that memory is associated to a dmabuf. If it is, we attach to it, thus
 * reusing that dmabuf.
 *
 * If it's just a pointer to user memory with no dmabuf associated to it, we
 * don't need to do anything here. We will use IOVMM later to get access to it.
 */
static int mcfrc_buffer_get_and_attach( struct mcfrc_dev *mcfrc_device,
										struct hwMCFRC_buffer *buffer,
										struct mcfrc_buffer_dma *dma_buffer)
{
	struct mcfrc_buffer_plane_dma *plane;
	int ret = 0; //PTR_ERR returns a long
	off_t offset = 0;

	dev_dbg(mcfrc_device->dev
		, "%s: BEGIN, acquiring plane from buf %p into dma buffer %p\n"
		, __func__, buffer, dma_buffer);

	plane = &dma_buffer->plane;

	if (buffer->type == HWMCFRC_BUFFER_DMABUF) {
		plane->dmabuf = dma_buf_get(buffer->fd);

		dev_dbg(mcfrc_device->dev, "%s: dmabuf of fd %d is %p\n", __func__
			, buffer->fd, plane->dmabuf);
	} else if (buffer->type == HWMCFRC_BUFFER_USERPTR) {
		// Check if there's already a dmabuf associated to the
		// chunk of user memory our client is pointing us to
		plane->dmabuf = mcfrc_get_dmabuf_from_userptr(mcfrc_device,
								 buffer->userptr,
								 buffer->len,
								 &offset);
		if (!plane->dmabuf) {
			ret = -EINVAL;
			dev_err(mcfrc_device->dev,
				"%s: failed to get dmabuf, err %d\n", __func__, ret);
			goto err;
		}
	} else {
		if (!plane->dmabuf) {
			ret = -EINVAL;
			dev_err(mcfrc_device->dev,
				"%s: failed to get dmabuf, err %d\n", __func__, ret);
			goto err;
		}
	}

	if (IS_ERR(plane->dmabuf)) {
		ret = PTR_ERR(plane->dmabuf);
		dev_err(mcfrc_device->dev,
			"%s: failed to get dmabuf, err %d\n", __func__, ret);
		goto err;
	}

	// this is NULL when the buffer is of type HWMCFRC_BUFFER_USERPTR
	// but the memory it points to is not associated to a dmabuf
	if (plane->dmabuf) {
		if (plane->dmabuf->size < plane->bytes_used) {
			dev_err(mcfrc_device->dev,
				"%s: driver requires %zu bytes but user gave %zu\n",
				__func__, plane->bytes_used,
				plane->dmabuf->size);
			ret = -EINVAL;
			goto err;
		}

		plane->attachment = dma_buf_attach(plane->dmabuf
						   , mcfrc_device->dev);

		dev_dbg(mcfrc_device->dev
			, "%s: attachment of dmabuf %p is %p\n", __func__
			, plane->dmabuf, plane->attachment);

		plane->offset = offset;

		if (IS_ERR(plane->attachment)) {
			dev_err(mcfrc_device->dev,
				"%s: Failed to attach dmabuf\n", __func__);
			ret = PTR_ERR(plane->attachment);
			goto err;
		}
	} else {
		ret = -EINVAL;
		dev_err(mcfrc_device->dev,
			"%s: failed to get dmabuf, err %d\n", __func__, ret);
		goto err;
	}

	dev_dbg(mcfrc_device->dev, "%s: END\n", __func__);

	return 0;
err:
	dev_dbg(mcfrc_device->dev
		, "%s: ERROR releasing dma resources\n", __func__);

	if (!IS_ERR(plane->dmabuf)) // release dmabuf
		dma_buf_put(plane->dmabuf);

	// NOTE: attach was not reached or did not complete,
	//       so no need to detach here

	return ret;
}


/**
 * This should be called after unmapping the dma buffer.
 *
 * We detach the device from the buffer to signal that we're not
 * interested in it anymore, and we use "put" to decrease the usage
 * refcount of the dmabuf struct.
 * Detach is at the end of the function name just to make it easier
 * to associate mcfrc_buffer_get_* to mcfrc_buffer_put_*, but  it's
 * actually called in the inverse order.
*/
static void mcfrc_buffer_put_and_detach(struct mcfrc_buffer_dma *dma_buffer)
{
	const struct hwMCFRC_buffer *user_buffer = dma_buffer->buffer;
	struct mcfrc_buffer_plane_dma *plane = &dma_buffer->plane;

	// - buffer type DMABUF:
	//     in this case dmabuf should always be set
	// - buffer type USERPTR:
	//     in this case dmabuf is only set if we reuse any
	//     preexisting dmabuf (see mcfrc_buffer_get_*)
	if (user_buffer->type == HWMCFRC_BUFFER_DMABUF
			|| (user_buffer->type == HWMCFRC_BUFFER_USERPTR
				&& plane->dmabuf)) {
		dma_buf_detach(plane->dmabuf, plane->attachment);
		dma_buf_put(plane->dmabuf);
		plane->dmabuf = NULL;
	}
}


/**
 * This is the entry point for buffer dismantling.
 *
 * It is used whenever we need to dismantle a buffer that had been fully
 * setup, for instance after we finish processing a task, independently of
 * whether the processing was successful or not.
 *
 * It unmaps the buffer, detaches the device from it, and releases it.
 */
static void mcfrc_buffer_teardown(struct mcfrc_dev *mcfrc_device,
					   struct mcfrc_ctx *ctx,
					   struct mcfrc_buffer_dma *dma_buffer,
					   enum dma_data_direction dir)
{
	dev_dbg(mcfrc_device->dev, "%s: BEGIN\n", __func__);

	mcfrc_buffer_unmap(ctx, dma_buffer, dir);

	mcfrc_buffer_put_and_detach(dma_buffer);

	dev_dbg(mcfrc_device->dev, "%s: END\n", __func__);
}


/**
 * This is the entry point for complete buffer setup.
 *
 * It does the necessary mapping and allocation of the DMA buffer that
 * the HW will access.
 *
 * At the end of this function, the buffer is ready for DMA transfers.
 *
 * Note: this is needed when the input is a HWSQZ_BUFFER_DMABUF as well
 *       as when it is a HWSQZ_BUFFER_USERPTR, because the H/W ultimately
 *       needs a DMA address to operate on.
 */

static int mcfrc_buffer_setup(struct mcfrc_ctx *ctx,
				 struct hwMCFRC_buffer *buffer,
				 struct mcfrc_buffer_dma *dma_buffer,
				 enum dma_data_direction dir)
{
	struct mcfrc_dev *mcfrc_device = ctx->mcfrc_dev;
	int ret = 0;
	struct mcfrc_buffer_plane_dma *plane = &dma_buffer->plane;

	dev_dbg(mcfrc_device->dev, "%s: BEGIN\n", __func__);

	dev_dbg(mcfrc_device->dev
		, "%s: computing bytes needed for dma buffer %p\n"
		, __func__, dma_buffer);

	if (plane->bytes_used == 0) {
		dev_err(mcfrc_device->dev,
			"%s: BUG! driver expects 0 bytes! (user gave %zu)\n",
			__func__, buffer->len);
		return -EINVAL;

	} else if (buffer->len < plane->bytes_used) {
		dev_err(mcfrc_device->dev,
			"%s: driver expects %zu bytes but user gave %zu\n",
			__func__, plane->bytes_used,
			buffer->len);
		return -EINVAL;
	}

	if ((buffer->type != HWMCFRC_BUFFER_USERPTR) &&
			(buffer->type != HWMCFRC_BUFFER_DMABUF)) {
		dev_err(mcfrc_device->dev, "%s: unknown buffer type %u\n",
			__func__, buffer->type);
		return -EINVAL;
	}

	ret = mcfrc_buffer_get_and_attach(mcfrc_device, buffer, dma_buffer);

	if (ret)
		return ret;

	dma_buffer->buffer = buffer;

	dev_dbg(mcfrc_device->dev
		, "%s: about to prepare buffer %p, dir DMA_TO_DEVICE? %d, dir DMA_FROM_DEVICE? %d\n"
		, __func__, dma_buffer, dir == DMA_TO_DEVICE, dir == DMA_FROM_DEVICE);

	// the callback function should fill 'dma_addr' field
	ret = mcfrc_buffer_map(ctx, dma_buffer, dir);
	//ret = mcfrc_buffer_map(ctx, dma_buffer, dir == DMA_TO_DEVICE);
	if (ret) {
		dev_err(mcfrc_device->dev, "%s: Failed to prepare plane"
			, __func__);

		dev_dbg(mcfrc_device->dev, "%s: releasing buffer %p\n"
			, __func__, dma_buffer);

		mcfrc_buffer_put_and_detach(dma_buffer);

		return ret;
	}

	dev_dbg(mcfrc_device->dev, "%s: END\n", __func__);

	return 0;
}


/**
 * Validates the image formats (size, pixel formats) provided by userspace and
 * computes the size of the DMA buffers required to be able to process the task.
 *
 * See description of mcfrc_compute_buffer_size for more info.
 */
static int mcfrc_prepare_formats(struct mcfrc_dev *mcfrc_device,
	struct mcfrc_ctx *ctx,
	struct mcfrc_task *task)
{
	int ret = 0;
	//int i = 0;
	size_t out_size = 0;
	int hb = (task->user_task.img_markup.height + 7)>>3;
	int wb = (task->user_task.img_markup.width + 7)>>3;

	dev_dbg(mcfrc_device->dev, "%s: BEGIN\n", __func__);

	dev_dbg(ctx->mcfrc_dev->dev, "%s: Validating format...\n", __func__);


	// key frames
	//ret = mcfrc_compute_buffer_size(ctx, task, DMA_TO_DEVICE, &out_size, 0);
	//if (ret < 0)
	//    return ret;
	out_size = task->user_task.img_markup.height*task->user_task.img_markup.stride + task->user_task.img_markup.height_uv * task->user_task.img_markup.stride;
	task->dma_buf_in[0].plane.bytes_used = out_size;
	task->dma_buf_in[1].plane.bytes_used = out_size;
	task->dma_buf_params.plane.bytes_used = out_size;
	task->dma_buf_in[0].dir = DMA_TO_DEVICE;
	task->dma_buf_in[1].dir = DMA_TO_DEVICE;
	task->dma_buf_params.dir = DMA_TO_DEVICE;

	// interpolated/fall frame
	//out_size = 0;
	//ret = mcfrc_compute_buffer_size(ctx, task, DMA_FROM_DEVICE, &out_size, 0);
	//if (ret < 0)
	//    return ret;
	out_size = task->user_task.img_markup.height*task->user_task.img_markup.stride + task->user_task.img_markup.height_uv * task->user_task.img_markup.stride;
	task->dma_buf_out.plane.bytes_used = out_size;
	task->dma_buf_out.dir = DMA_BIDIRECTIONAL;

	// block parameters (mv, etc.)
	out_size = hb*wb * 16;// 16 == sizeof(BlkParamsInput)
	task->dma_buf_params.plane.bytes_used = out_size;
	task->dma_buf_params.dir = DMA_TO_DEVICE;

	// parameters fpr deblock
	//out_size = 0;
	//ret = mcfrc_compute_buffer_size(ctx, task, DMA_BIDIRECTIONAL, &out_size, 0);
	//if (ret < 0)
	//    return ret;
	out_size = hb*wb * 16;// 16 == 4 * 2 * sizeof(uint16_t)
	task->dma_buf_temp.plane.bytes_used = out_size;
	task->dma_buf_temp.dir = DMA_BIDIRECTIONAL;

	dev_dbg(mcfrc_device->dev
		, "%s: hardcoding result img height/width to be same as input\n"
		, __func__);

	dev_dbg(mcfrc_device->dev, "%s: END\n", __func__);

	return ret;
}

/**
 * Main entry point for preparing a task before it can be processed.
 *
 * Checks the validity of requested formats/sizes and sets the buffers up.
*/
#if 0
static int mcfrc_task_setup(struct mcfrc_dev *mcfrc_device,
	struct mcfrc_ctx *ctx,
	struct mcfrc_task *task)
{
	int ret, i;

	dev_dbg(mcfrc_device->dev, "%s: BEGIN\n", __func__);

	HWMCFRC_PROFILE(ret = mcfrc_prepare_formats(mcfrc_device, ctx, task),
		"PREPARE FORMATS TIME", mcfrc_device->dev);
	if (ret)
		return ret;

	for (i = 0; i < task->user_task.num_of_buf; i++)
	{
		dev_dbg(mcfrc_device->dev, "%s: OUT buf, USER GAVE %zu WE REQUIRE %zu\n"
			, __func__, task->user_task.buf_out[i].len
			, task->dma_buf_out[i].plane.bytes_used);

		HWMCFRC_PROFILE(ret = mcfrc_buffer_setup(ctx, &task->user_task.buf_out[i],
			&task->dma_buf_out[i], DMA_TO_DEVICE),
			"OUT BUFFER SETUP TIME", mcfrc_device->dev);
		if (ret) {
			dev_err(mcfrc_device->dev, "%s: Failed to get output buffer\n",
				__func__);
			if (i > 0) {
				mcfrc_buffer_teardown(mcfrc_device, ctx,
					&task->dma_buf_out[i - 1], DMA_TO_DEVICE);
				dev_err(mcfrc_device->dev, "%s: Failed to get capture buffer\n",
					__func__);
			}
			return ret;
		}
	}

	dev_dbg(mcfrc_device->dev, "%s: END\n", __func__);

	return 0;
}
#else
static int mcfrc_task_setup(struct mcfrc_dev *mcfrc_device,
	struct mcfrc_ctx *ctx,
	struct mcfrc_task *task)
{
	//int ret, i;
	int ret;

	dev_dbg(mcfrc_device->dev, "%s: BEGIN\n", __func__);
	HWMCFRC_PROFILE(ret = mcfrc_prepare_formats(mcfrc_device, ctx, task),
		"PREPARE FORMATS TIME", mcfrc_device->dev);
	if (ret)
		return ret;

	ret = mcfrc_buffer_setup(ctx, &task->user_task.buf_inout[0], &task->dma_buf_in[0], DMA_TO_DEVICE);
	if (ret) {
		dev_err(mcfrc_device->dev, "%s: Failed to get in1 buffer\n", __func__);
		return ret;
	}
	ret = mcfrc_buffer_setup(ctx, &task->user_task.buf_inout[1], &task->dma_buf_in[1], DMA_TO_DEVICE);
	if (ret) {
		dev_err(mcfrc_device->dev, "%s: Failed to get in2 buffer\n", __func__);
		mcfrc_buffer_teardown(mcfrc_device, ctx, &task->dma_buf_in[0], DMA_TO_DEVICE);
		return ret;
	}

	ret = mcfrc_buffer_setup(ctx, &task->user_task.buf_inout[2], &task->dma_buf_out, DMA_BIDIRECTIONAL);
	if (ret) {
		dev_err(mcfrc_device->dev, "%s: Failed to get out buffer\n", __func__);
		mcfrc_buffer_teardown(mcfrc_device, ctx, &task->dma_buf_in[0], DMA_TO_DEVICE);
		mcfrc_buffer_teardown(mcfrc_device, ctx, &task->dma_buf_in[1], DMA_TO_DEVICE);
		return ret;
	}

	ret = mcfrc_buffer_setup(ctx, &task->user_task.buf_inout[3], &task->dma_buf_params, DMA_TO_DEVICE);
	if (ret) {
		dev_err(mcfrc_device->dev, "%s: Failed to get params buffer\n", __func__);
		mcfrc_buffer_teardown(mcfrc_device, ctx, &task->dma_buf_in[0], DMA_TO_DEVICE);
		mcfrc_buffer_teardown(mcfrc_device, ctx, &task->dma_buf_in[1], DMA_TO_DEVICE);
		mcfrc_buffer_teardown(mcfrc_device, ctx, &task->dma_buf_out, DMA_BIDIRECTIONAL);
		return ret;
	}

	ret = mcfrc_buffer_setup(ctx, &task->user_task.buf_inout[4], &task->dma_buf_temp, DMA_BIDIRECTIONAL);
	if (ret) {
		dev_err(mcfrc_device->dev, "%s: Failed to get temp buffer\n", __func__);
		mcfrc_buffer_teardown(mcfrc_device, ctx, &task->dma_buf_in[0], DMA_TO_DEVICE);
		mcfrc_buffer_teardown(mcfrc_device, ctx, &task->dma_buf_in[1], DMA_TO_DEVICE);
		mcfrc_buffer_teardown(mcfrc_device, ctx, &task->dma_buf_out, DMA_BIDIRECTIONAL);
		mcfrc_buffer_teardown(mcfrc_device, ctx, &task->dma_buf_params, DMA_TO_DEVICE);
		return ret;
	}

	dev_dbg(mcfrc_device->dev, "%s: END\n", __func__);

	return ret;
}
#endif


/**
 * Main entry point for task teardown.
 *
 * It takes care of doing all that is needed to teardown the buffers
 * that were used to process the task.
 */
static void mcfrc_task_teardown(struct mcfrc_dev *mcfrc_dev,
	struct mcfrc_ctx *ctx,
	struct mcfrc_task *task)
{
	dev_dbg(mcfrc_dev->dev, "%s: BEGIN\n", __func__);

	mcfrc_buffer_teardown(mcfrc_dev, ctx, &task->dma_buf_in[0], DMA_TO_DEVICE);
	mcfrc_buffer_teardown(mcfrc_dev, ctx, &task->dma_buf_in[1], DMA_TO_DEVICE);
	mcfrc_buffer_teardown(mcfrc_dev, ctx, &task->dma_buf_out, DMA_BIDIRECTIONAL);
	mcfrc_buffer_teardown(mcfrc_dev, ctx, &task->dma_buf_params, DMA_TO_DEVICE);
	mcfrc_buffer_teardown(mcfrc_dev, ctx, &task->dma_buf_temp, DMA_BIDIRECTIONAL);

	dev_dbg(mcfrc_dev->dev, "%s: END\n", __func__);
}


static void mcfrc_destroy_context(struct kref *kreference)
{
	unsigned long flags;
	//^^ this function is called with ctx->kref as parameter,
	//so we can use ptr arithmetics to go back to ctx
	struct mcfrc_ctx *ctx = container_of(kreference,
		struct mcfrc_ctx, kref);
	struct mcfrc_dev *mcfrc_device = ctx->mcfrc_dev;

	dev_dbg(mcfrc_device->dev, "%s: BEGIN\n", __func__);

	spin_lock_irqsave(&mcfrc_device->lock_ctx, flags);
	dev_dbg(mcfrc_device->dev, "%s: deleting context %p from list\n"
		, __func__, ctx);
	list_del(&ctx->node);
	spin_unlock_irqrestore(&mcfrc_device->lock_ctx, flags);

	kfree(ctx);

	dev_dbg(mcfrc_device->dev, "%s: END\n", __func__);
}

/*
static int mcfrc_check_image_align(struct mcfrc_dev *mcfrc_device, struct hwmcfrc_img_info *user_img_info)
{
	unsigned int stride, width;
	unsigned int check_var = 0;

	dev_dbg(mcfrc_device->dev, "%s: BEGIN\n", __func__);
	stride = user_img_info->stride;
	width = user_img_info->width;
	if (stride != 0)
		check_var = stride;
	else
		check_var = width;

	if (user_img_info->cs) {		//yuv422
		if (((check_var % 128) == 0) || (((check_var % 64) == 0) && (((check_var / 64) % 4) == 1)))
			return 0;
		else
			return 1;
	}
	else {							//nv21
		if (((check_var % 64) == 0) || (((check_var % 32) == 0) && (((check_var / 32) % 4) == 1)))
			return 0;
		else
			return 1;
	}
	dev_dbg(mcfrc_device->dev, "%s: END\n", __func__);
}
*/
/**
 * mcfrc_process() - Main entry point for task processing
 * @ctx: context of the task that is about to be processed
 * @task: the task to process
 *
 * Handle all steps needed to process a task, from setup to teardown.
 *
 * First perform the necessary validations and setup of buffers, then
 * wake the H/W up, add the task to the queue, schedule it for execution
 * and wait for the H/W to signal completion.
 *
 * After the @task has been processed (independently of whether it was
 * successful or not), release all the resources and power the H/W down.
 *
 * Return:
 *	 0 on success,
 *	<0 error code on error.
 */
static int mcfrc_process(   struct mcfrc_ctx *ctx,
							struct mcfrc_task *task)
{
	struct mcfrc_dev *mcfrc_device = NULL;
	int ret = 0;
	int num_retries = -1;
	//int i = 0;

	if (!ctx)
	{
		pr_err("%s: invalid context!\n", __func__);
		return -EINVAL;
	}

	mcfrc_device = ctx->mcfrc_dev;
	dev_dbg(mcfrc_device->dev, "%s: BEGIN\n", __func__);

	init_completion(&task->complete);

	dev_dbg(mcfrc_device->dev, "%s: acquiring kref of ctx %p\n",
		__func__, ctx);
	kref_get(&ctx->kref);

	mutex_lock(&ctx->mutex);

	dev_dbg(mcfrc_device->dev, "%s: inside context lock (ctx %p task %p)\n"
		, __func__, ctx, task);

	ret = mcfrc_task_setup(mcfrc_device, ctx, task);

#if (MCFRC_DBG_MODE == 1) // remove debug parameters
	if (!CAVY_TEST_BIT(DBG_CONTROL_FLAG_0, task->user_task.dbg_control_flags)) {
#endif
		if (ret)
			goto err_prepare_task;

		//
		task->ctx = ctx;
		task->state = MCFRC_BUFSTATE_READY;

		//INIT_WORK(&task->work, mcfrc_task_schedule_work);
		INIT_WORK_ONSTACK(&task->work, mcfrc_task_schedule_work);

		do {
			num_retries++;

			if (!queue_work(mcfrc_device->workqueue, &task->work)) {
				dev_err(mcfrc_device->dev
					, "%s: work was already on a workqueue!\n"
					, __func__);
			}

			// wait for the job to be completed, in UNINTERRUPTIBLE mode
			flush_work(&task->work);

		} while (num_retries < 2
			&& task->state == MCFRC_BUFSTATE_RETRY);

		if (task->state == MCFRC_BUFSTATE_READY) {
			dev_err(mcfrc_device->dev
				, "%s: invalid task state after task completion\n"
				, __func__);
		}
#if (MCFRC_DBG_MODE == 1) // remove debug parameters
	}
#endif

	HWMCFRC_PROFILE(mcfrc_task_teardown(mcfrc_device, ctx, task),
		"TASK TEARDOWN TIME",
		mcfrc_device->dev);

err_prepare_task:
	HWMCFRC_PROFILE(

	dev_dbg(mcfrc_device->dev, "%s: releasing lock of ctx %p\n"
			, __func__, ctx);
	mutex_unlock(&ctx->mutex);

	dev_dbg(mcfrc_device->dev, "%s: releasing kref of ctx %p\n"
		, __func__, ctx);

	kref_put(&ctx->kref, mcfrc_destroy_context);

	dev_dbg(mcfrc_device->dev, "%s: returning %d, task state %d\n"
		, __func__, ret, task->state);

	, "FINISH PROCESS", mcfrc_device->dev);

	if (ret)
		return ret;
	else
		return (task->state == MCFRC_BUFSTATE_DONE) ? 0 : -EINVAL;
}

/**

 * mcfrc_open() - Handle device file open calls
 * @inode: the inode struct representing the device file on disk
 * @filp: the kernel file struct representing the abstract device file
 *
 * Function passed as "open" member of the file_operations struct of
 * the device in order to handle dev node open calls.
 *
 * Return:
 *	0 on success,
 *	-ENOMEM if there is not enough memory to allocate the context
 */
static int mcfrc_open(struct inode *inode, struct file *filp)
{
	unsigned long flags;
	//filp->private_data holds mcfrc_dev->misc (misc_open sets it)
	//this uses pointers arithmetics to go back to mcfrc_dev
	struct mcfrc_dev *mcfrc_device = container_of(filp->private_data,
		struct mcfrc_dev, misc);
	struct mcfrc_ctx *ctx;

	dev_dbg(mcfrc_device->dev, "%s: BEGIN\n", __func__);
	// GFP_KERNEL is used when kzalloc is allowed to sleep while
	// the memory we're requesting is not immediately available
	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	INIT_LIST_HEAD(&ctx->node);
	kref_init(&ctx->kref);
	mutex_init(&ctx->mutex);

	ctx->mcfrc_dev = mcfrc_device;
	mcfrc_device->task_irq = 0;

	spin_lock_irqsave(&mcfrc_device->lock_ctx, flags);
	dev_dbg(mcfrc_device->dev, "%s: adding new context %p to contexts list\n"
		, __func__, ctx);
	list_add_tail(&ctx->node, &mcfrc_device->contexts);
	spin_unlock_irqrestore(&mcfrc_device->lock_ctx, flags);

	filp->private_data = ctx;

	dev_dbg(mcfrc_device->dev, "%s: END\n", __func__);

	return 0;
}

/**
 * mcfrc_release() - Handle device file close calls
 * @inode: the inode struct representing the device file on disk
 * @filp: the kernel file struct representing the abstract device file
 *
 * Function passed as "close" member of the file_operations struct of
 * the device in order to handle dev node close calls.
 *
 * Decrease the refcount of the context associated to the given
 * device node.
 */
static int mcfrc_release(struct inode *inode, struct file *filp)
{
	struct mcfrc_ctx *ctx = filp->private_data;

	dev_dbg(ctx->mcfrc_dev->dev, "%s: releasing context %p\n"
		, __func__, ctx);
	dev_info(ctx->mcfrc_dev->dev, "%s: releasing context %p\n"
		, __func__, ctx);

	//the release callback will not be invoked if counter
	//is already 0 before this line
	kref_put(&ctx->kref, mcfrc_destroy_context);

	return 0;
}

/**
 * mcfrc_ioctl() - Handle ioctl calls coming from userspace.
  * @filp: the kernel file struct representing the abstract device file
  * @cmd: the ioctl command
  * @arg: the argument passed to the ioctl sys call
  *
  * Receive the task information from userspace, setup and execute the task.
  * Wait for the task to be completed and then return control to userspace.
  *
  * Return:
  *   0 on success,
  *  <0 error codes otherwise
 */
static long mcfrc_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	struct mcfrc_ctx *ctx = filp->private_data;
	struct mcfrc_dev *mcfrc_device = ctx->mcfrc_dev;
	struct mcfrc_task data;

	dev_dbg(mcfrc_device->dev, "%s: BEGIN\n", __func__);

	switch (cmd)
	{
	case HWMCFRC_IOC_PROCESS:
	{
		//struct mcfrc_task data;
		int ret = 0;

		dev_dbg(mcfrc_device->dev, "%s: PROCESS ioctl received\n"
			, __func__);

		dev_dbg(mcfrc_device->dev, "version = %s, ULONG_MAX = %lu", _DEBUG_VERSION_STR_, ULONG_MAX);

		memset(&data, 0, sizeof(data));

		if ((void __user *)arg == NULL)
			return ret;

		//data.type = MPEG_SQZ;

		if (copy_from_user(&data.user_task
			, (void __user *)arg
			, sizeof(data.user_task))) {
			dev_err(mcfrc_device->dev,
				"%s: Failed to read userdata\n", __func__);
			return -EFAULT;
		}

#if (MCFRC_DBG_MODE == 1) // remove debug parameters
		if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_9, data.user_task.dbg_control_flags)) {
			dev_info(mcfrc_device->dev
				, "%s: (drvver: %s) user data copied, dbg_flags=0x%X, now launching processing...\n"
				, __func__, _DEBUG_VERSION_STR_, data.user_task.dbg_control_flags);
		}
		else {
			dev_dbg(mcfrc_device->dev
				, "%s: user data copied, dbg_flags=0x%X, now launching processing...\n"
				, __func__, data.user_task.dbg_control_flags);
		}
#endif

		//
		// mcfrc_process() does not wake up
		// until the given task finishes
		//
		HWMCFRC_PROFILE(ret = mcfrc_process(ctx, &data),
			"WHOLE PROCESS TIME",
			mcfrc_device->dev);

		data.user_task.results[0] = data.state;

		if (ret) {
			dev_err(mcfrc_device->dev,
				"%s: Failed to process task, error %d\n"
				, __func__, ret);
			//return ret;
		}

		dev_dbg(mcfrc_device->dev
			, "%s: processing done! Copying data back to user space...\n", __func__);

		if (copy_to_user((void __user *)arg, &data.user_task, sizeof(data.user_task))) {
			dev_err(mcfrc_device->dev,
				"%s: Failed to write userdata\n", __func__);
		}

		return ret;
	}
	case HWMCFRC_IOC_PROCESS_DBG_01:
	{
		int ret = 0;
		unsigned int status_msk;
		unsigned int status_msk_save;
		unsigned int status;
		void __iomem *base = mcfrc_device->regs;
		//sw reset by polling
		{
			enable_mcfrc(mcfrc_device);

			status = readl(base + REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS);
			dev_dbg(mcfrc_device->dev
				, "%s: REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS:(%x, %x) before polling rst\n", __func__, REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS, status);

			// mask rst interrupt
			GET_MC_INTR_MASKING_REG(base, status_msk);
			status_msk_save = status_msk;
			INSERT_SRST_DONE_IRQ(status_msk, 0x1);
			SET_MC_INTR_MASKING_REG(base, status_msk);

			//raise rst
			writel(0x0, base + REG_TOP_0x0008_MC_SW_RESET);
			// wait ack
			while (1)
			{
				bool data8;
				status = readl(base + REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS);
				dev_dbg(mcfrc_device->dev
					, "%s: REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS:(%x, %x)\n", __func__, REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS, status);
				data8 = CAVY_TEST_BIT(IRQ_ERRRST_STATE_RST, status);
				if (data8)
					break;
				else
					usleep_range(1000, 1001);
			}
			// take off reset
			writel(0x1, base + REG_TOP_0x0008_MC_SW_RESET);

			// clear mask of rst interrupt (return previous state)
			SET_MC_INTR_MASKING_REG(base, status_msk_save);

			disable_mcfrc(mcfrc_device);
		}
		return ret;
	}
	case HWMCFRC_IOC_PROCESS_DBG_02:
	{
		int ret = 0;
		//sw reset by interrupt
		{
			unsigned long flags;
			void __iomem *base = mcfrc_device->regs;
			unsigned int status;
			unsigned long ret_wait = -1;
			/*
			int timeout = 100; //100 ms timeout
			unsigned long tempJ = msecs_to_jiffies(timeout);
			struct completion sema;
			init_completion(&sema);
			//wait_for_completion_timeout(&sema, tempJ)
			*/
			int timeout = 10; //10 ms timeout
			unsigned long tempJ = msecs_to_jiffies(timeout);
#if (MCFRC_DBG_MODE == 1) // remove debug parameters
			bool dbg_flags_presents = (void __user *)arg != NULL;

			if (dbg_flags_presents)
			{
				if (copy_from_user(&data.user_task
					, (void __user *)arg
					, sizeof(data.user_task))) {
					dev_err(mcfrc_device->dev,
						"%s: Failed to read userdata\n", __func__);
					return -EFAULT;
				}
			}
			else {
				data.user_task.dbg_control_flags = 0;
			}
#endif

#ifdef HWMCFRC_PROFILE_ENABLE
			global_time_start2 = ktime_get();
#endif

			enable_mcfrc(mcfrc_device);

#ifdef HWMCFRC_PROFILE_ENABLE
			global_time_start1 = ktime_get();
#endif

#if (MCFRC_DBG_MODE == 1) // remove debug parameters
#ifdef DEBUG
			if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_8, data.user_task.dbg_control_flags)) {
				mcfrc_print_all_regs(mcfrc_device, mcfrc_device->regs);
			}
#endif

			if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_10, data.user_task.dbg_control_flags)) {
				mcfrc_run_programm(mcfrc_device, &(data.user_task.programms[0]));
			}

			if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_6, data.user_task.dbg_control_flags)) {
				if (!IS_ERR(mcfrc_device->clk_producer)) {
					unsigned long rate;
					rate = clk_get_rate(mcfrc_device->clk_producer);
					dev_info(mcfrc_device->dev, "HWMCFRC_IOC_PROCESS_DBG_02: clk_get_rate() returns: %lu Hz\n", rate);
				}
				else {
					dev_info(mcfrc_device->dev, "HWMCFRC_IOC_PROCESS_DBG_02:IS_ERR(mcfrc->clk_producer)\n");
				}
			}
			if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_7, data.user_task.dbg_control_flags)) {
				if (!IS_ERR(mcfrc_device->clk_producer)) {
					unsigned long rate, rate_round;
					rate = 663000000;
					rate_round = clk_round_rate(mcfrc_device->clk_producer, rate);
					dev_info(mcfrc_device->dev, "HWMCFRC_IOC_PROCESS_DBG_02: clk_round_rate() returns: %lu Hz\n", rate_round);
				}
				else {
					dev_info(mcfrc_device->dev, "HWMCFRC_IOC_PROCESS_DBG_02:IS_ERR(mcfrc->clk_producer)\n");
				}
			}
#endif

			status = readl(base + REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS);

			dev_dbg(mcfrc_device->dev
				, "%s: REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS:(%x, %x) before irq rst\n", __func__, REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS, status);

			spin_lock_irqsave(&mcfrc_device->slock, flags);
			set_bit(1, &mcfrc_device->task_irq);
			mcfrc_device->state_irq = 0;
			init_completion(&mcfrc_device->rst_complete);
			spin_unlock_irqrestore(&mcfrc_device->slock, flags);

#ifdef HWMCFRC_PROFILE_ENABLE
			global_time_start = ktime_get();
#endif
			// raise sw reset
			writel(0x0, base + REG_TOP_0x0008_MC_SW_RESET);

			// wait
			//wait_for_completion(&mcfrc_device->rst_complete);
			ret_wait = wait_for_completion_timeout(&mcfrc_device->rst_complete, tempJ);

#ifdef HWMCFRC_PROFILE_ENABLE
			global_time_end1 = ktime_get();
			dev_info(mcfrc_device->dev, "HW PROCESS TIME (global_time_end1): %lld nanosec\n",
				ktime_to_ns(ktime_sub(global_time_end1, global_time_start1)));
#endif
			disable_mcfrc(mcfrc_device);

#ifdef HWMCFRC_PROFILE_ENABLE
			global_time_end2 = ktime_get();
			dev_info(mcfrc_device->dev, "HW PROCESS TIME (global_time_end2): %lld nanosec\n",
				ktime_to_ns(ktime_sub(global_time_end2, global_time_start2)));
#endif

			dev_dbg(mcfrc_device->dev
				, "%s:mcfrc_device->state_irq = %x, ret_wait = %lu\n", __func__, mcfrc_device->state_irq, ret_wait);
		}
		return ret;
	}
	case HWMCFRC_IOC_PROCESS_DBG_03:
	{
		int ret = 0;
		return ret;
	}
	case HWMCFRC_IOC_PROCESS_DBG_04:
	{
		int ret = 0;
		return ret;
	}
	case HWMCFRC_IOC_PROCESS_DBG_05:
	{
		int ret = 0;
		return ret;
	}
	default:
	{
		dev_err(mcfrc_device->dev, "%s: Unknown ioctl cmd %x, %x\n",
			__func__, cmd, HWMCFRC_IOC_PROCESS);
		return -EINVAL;
	}
	}

	return 0;
}


#ifdef CONFIG_COMPAT

struct compat_hwmcfrc_img_info {
	compat_uint_t height;               /**< height of the image (luma component)*/
	compat_uint_t width;                /**< width of the image (luma component)*/
	compat_uint_t stride;               /**< stride of the image (luma component)*/
	compat_uint_t height_uv;            /**< height of the image (chroma component)*/
	compat_uint_t width_uv;             /**< width of the image (chroma component)*/
	compat_uint_t offset;
	compat_uint_t offset_uv;
	//compat_uint_t frame_stride_uv;      /**< stride of the image (chroma component)*/
};

struct compat_hwmcfrc_buffer {
	union {
		compat_int_t fd;
		compat_ulong_t userptr;
	};
	compat_size_t len;
	__u8 type;
};

struct compat_hwmcfrc_config {
	enum hwMCFRC_processing_mode                mode;
	compat_uint_t                               phase;
	compat_uint_t                               dbl_on;
};

#if (MCFRC_DBG_MODE == 1) // remove debug parameters
struct compat_hwmcfrc_reg_command {
	compat_uint_t                               type;
	compat_uint_t                               address;
	compat_uint_t                               data_write;
	compat_uint_t                               data_read;
};

struct compat_hwmcfrc_reg_programm {
	struct compat_hwmcfrc_reg_command command[HWMCFRC_REG_PROGRAMM_MAX_LEN];
};
#endif

struct compat_hwmcfrc_task {
	//What we're giving as input to the device
	struct compat_hwmcfrc_img_info img_markup;
	struct compat_hwmcfrc_buffer buf_inout[5];
	struct compat_hwmcfrc_config config;
	compat_uint_t                results[RESULTS_LEN];

#if (MCFRC_DBG_MODE == 1) // remove debug parameters
	/* for debugging purposes*/
	compat_uint_t                dbg_control_flags;
	struct compat_hwmcfrc_reg_programm  programms[5];
#endif
};

#define COMPAT_HWMCFRC_IOC_PROCESS	_IOWR('M', 0, struct compat_hwmcfrc_task)

#define COMPAT_HWMCFRC_IOC_PROCESS_DBG_01	_IOWR('M', 1, struct compat_hwmcfrc_task)
#define COMPAT_HWMCFRC_IOC_PROCESS_DBG_02	_IOWR('M', 2, struct compat_hwmcfrc_task)
#define COMPAT_HWMCFRC_IOC_PROCESS_DBG_03	_IOWR('M', 3, struct compat_hwmcfrc_task)
#define COMPAT_HWMCFRC_IOC_PROCESS_DBG_04	_IOWR('M', 4, struct compat_hwmcfrc_task)
#define COMPAT_HWMCFRC_IOC_PROCESS_DBG_05	_IOWR('M', 5, struct compat_hwmcfrc_task)

static long mcfrc_compat_ioctl32(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	int i = 0;
#if (MCFRC_DBG_MODE == 1) // remove debug parameters
	int j = 0;
#endif
	struct mcfrc_ctx *ctx = filp->private_data;
	struct mcfrc_dev *mcfrc_device = ctx->mcfrc_dev;
	struct compat_hwmcfrc_task data;
	struct mcfrc_task task;

	dev_dbg(mcfrc_device->dev, "%s: compat BEGIN\n", __func__);

	switch (cmd) {
	case COMPAT_HWMCFRC_IOC_PROCESS:
	{
		int ret = 0;

		dev_dbg(mcfrc_device->dev, "%s: PROCESS compat_ioctl32 received\n"
			, __func__);

		dev_dbg(mcfrc_device->dev, "version = %s, ULONG_MAX = %lu", _DEBUG_VERSION_STR_, ULONG_MAX);

		memset(&data, 0, sizeof(data));
		memset(&task, 0, sizeof(task));

		if (compat_ptr(arg) == NULL)
			return ret;

		if (copy_from_user(&data, compat_ptr(arg), sizeof(data))) {
			dev_err(mcfrc_device->dev,
				"%s: Failed to read userdata\n", __func__);
			return -EFAULT;
		}

		task.user_task.img_markup.height = data.img_markup.height;
		task.user_task.img_markup.width = data.img_markup.width;
		task.user_task.img_markup.stride = data.img_markup.stride;
		task.user_task.img_markup.height_uv = data.img_markup.height_uv;
		task.user_task.img_markup.width_uv = data.img_markup.width_uv;
		task.user_task.img_markup.offset_uv = data.img_markup.offset_uv;
		task.user_task.img_markup.offset = data.img_markup.offset;

		for (i = 0; i < 5; i++) {
			if (data.buf_inout[i].type == HWMCFRC_BUFFER_DMABUF)
				task.user_task.buf_inout[i].fd = data.buf_inout[i].fd;
			else
				task.user_task.buf_inout[i].userptr = data.buf_inout[i].userptr;
			task.user_task.buf_inout[i].len = data.buf_inout[i].len;
			task.user_task.buf_inout[i].type = data.buf_inout[i].type;
		}

		task.user_task.config.mode = data.config.mode;
		task.user_task.config.phase = data.config.phase;
		task.user_task.config.dbl_on = data.config.dbl_on;
#if (MCFRC_DBG_MODE == 1) // remove debug parameters
		task.user_task.dbg_control_flags = data.dbg_control_flags;
		for (i = 0; i < 5; i++) {
			for (j = 0; j < HWMCFRC_REG_PROGRAMM_MAX_LEN; j++) {
				task.user_task.programms[i].command[j].type = data.programms[i].command[j].type;
				task.user_task.programms[i].command[j].address = data.programms[i].command[j].address;
				task.user_task.programms[i].command[j].data_write = data.programms[i].command[j].data_write;
				task.user_task.programms[i].command[j].data_read = data.programms[i].command[j].data_read;
			}
		}
#endif

		for (i = 0; i < RESULTS_LEN; i++) {
			task.user_task.results[i] = data.results[i];
		}

#if (MCFRC_DBG_MODE == 1) // remove debug parameters
		if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_9, task.user_task.dbg_control_flags)) {
			dev_info(mcfrc_device->dev
				, "%s: (drvver: %s) user data copied, dbg_flags=0x%X, now launching processing...\n"
				, __func__, _DEBUG_VERSION_STR_, task.user_task.dbg_control_flags);
		}
		else {
			dev_dbg(mcfrc_device->dev
				, "%s: user data copied, dbg_flags=0x%X, now launching processing...\n"
				, __func__, task.user_task.dbg_control_flags);
		}
#endif

		//
		// mcfrc_process() does not wake up
		// until the given task finishes
		//
		HWMCFRC_PROFILE(ret = mcfrc_process(ctx, &task),
			"WHOLE PROCESS TIME",
			mcfrc_device->dev);

		task.user_task.results[0] = task.state;

		if (ret) {
			dev_err(mcfrc_device->dev,
				"%s: Failed to process task, error %d\n"
				, __func__, ret);
			//return ret;
		}

		dev_dbg(mcfrc_device->dev
			, "%s: processing done! Copying data back to user space...\n", __func__);

		/*if (copy_to_user((void __user *)arg, &data.user_task, sizeof(data.user_task))) {
			dev_err(mcfrc_device->dev,
				"%s: Failed to write userdata\n", __func__);
		}*/
		// should be:
		for (i = 0; i < RESULTS_LEN; i++) {
			data.results[i] = task.user_task.results[i];
		}
		if (copy_to_user(compat_ptr(arg), &data, sizeof(data))) {
			dev_err(mcfrc_device->dev,
				"%s: Failed to write userdata\n", __func__);
		}

		return ret;
	}
	case COMPAT_HWMCFRC_IOC_PROCESS_DBG_01:
	{
		int ret = 0;
		unsigned int status_msk;
		unsigned int status_msk_save;
		unsigned int status;
		void __iomem *base = mcfrc_device->regs;
		//sw reset by polling
		{
			enable_mcfrc(mcfrc_device);

			status = readl(base + REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS);
			dev_dbg(mcfrc_device->dev
				, "%s: REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS:(%x, %x) before polling rst\n", __func__, REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS, status);

			// mask rst interrupt
			GET_MC_INTR_MASKING_REG(base, status_msk);
			status_msk_save = status_msk;
			INSERT_SRST_DONE_IRQ(status_msk, 0x1);
			SET_MC_INTR_MASKING_REG(base, status_msk);

			//raise rst
			writel(0x0, base + REG_TOP_0x0008_MC_SW_RESET);
			// wait ack
			while (1)
			{
				bool data8;
				status = readl(base + REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS);
				dev_dbg(mcfrc_device->dev
					, "%s: REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS:(%x, %x)\n", __func__, REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS, status);
				data8 = CAVY_TEST_BIT(IRQ_ERRRST_STATE_RST, status);
				if (data8)
					break;
				else
					usleep_range(1000, 1001);
			}
			// take off reset
			writel(0x1, base + REG_TOP_0x0008_MC_SW_RESET);

			// clear mask of rst interrupt (return previous state)
			SET_MC_INTR_MASKING_REG(base, status_msk_save);

			disable_mcfrc(mcfrc_device);
		}
		return ret;
	}
	case COMPAT_HWMCFRC_IOC_PROCESS_DBG_02:
	{
		int ret = 0;
		//sw reset by interrupt
		{
			unsigned long flags;
			void __iomem *base = mcfrc_device->regs;
			unsigned int status;
			unsigned long ret_wait = -1;
			/*
			int timeout = 100; //100 ms timeout
			unsigned long tempJ = msecs_to_jiffies(timeout);
			struct completion sema;
			init_completion(&sema);
			//wait_for_completion_timeout(&sema, tempJ)
			*/
			int timeout = 10; //10 ms timeout
			unsigned long tempJ = msecs_to_jiffies(timeout);

#if (MCFRC_DBG_MODE == 1) // remove debug parameters
			bool dbg_flags_presents = compat_ptr(arg) != NULL;

			if (dbg_flags_presents)
			{
				if (copy_from_user(&data, compat_ptr(arg), sizeof(data))) {
					dev_err(mcfrc_device->dev,
						"%s: Failed to read userdata\n", __func__);
					return -EFAULT;
				}
				task.user_task.dbg_control_flags = data.dbg_control_flags;
				for (i = 0; i < 5; i++) {
					for (j = 0; j < HWMCFRC_REG_PROGRAMM_MAX_LEN; j++) {
						task.user_task.programms[i].command[j].type = data.programms[i].command[j].type;
						task.user_task.programms[i].command[j].address = data.programms[i].command[j].address;
						task.user_task.programms[i].command[j].data_write = data.programms[i].command[j].data_write;
						task.user_task.programms[i].command[j].data_read = data.programms[i].command[j].data_read;
					}
				}
			}
			else {
				task.user_task.dbg_control_flags = 0;
			}
#endif

#ifdef HWMCFRC_PROFILE_ENABLE
			global_time_start2 = ktime_get();
#endif

			enable_mcfrc(mcfrc_device);

#ifdef HWMCFRC_PROFILE_ENABLE
			global_time_start1 = ktime_get();
#endif

#if 0
			if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_8, task.user_task.dbg_control_flags)) {
				mcfrc_print_all_regs(mcfrc_device, mcfrc_device->regs);
			}
#endif
#if (MCFRC_DBG_MODE == 1) // remove debug parameters
			if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_10, task.user_task.dbg_control_flags)) {
				mcfrc_run_programm(mcfrc_device, &(task.user_task.programms[0]));
			}

			if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_6, task.user_task.dbg_control_flags)) {
				if (!IS_ERR(mcfrc_device->clk_producer)) {
					unsigned long rate;
					rate = clk_get_rate(mcfrc_device->clk_producer);
					dev_info(mcfrc_device->dev, "HWMCFRC_IOC_PROCESS_DBG_02: clk_get_rate() returns: %lu Hz\n", rate);
				}
				else {
					dev_info(mcfrc_device->dev, "HWMCFRC_IOC_PROCESS_DBG_02:IS_ERR(mcfrc->clk_producer)\n");
				}
			}
			if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_7, task.user_task.dbg_control_flags)) {
				if (!IS_ERR(mcfrc_device->clk_producer)) {
					unsigned long rate, rate_round;
					rate = 663000000;
					rate_round = clk_round_rate(mcfrc_device->clk_producer, rate);
					dev_info(mcfrc_device->dev, "HWMCFRC_IOC_PROCESS_DBG_02: clk_round_rate() returns: %lu Hz\n", rate_round);
				}
				else {
					dev_info(mcfrc_device->dev, "HWMCFRC_IOC_PROCESS_DBG_02:IS_ERR(mcfrc->clk_producer)\n");
				}
			}
#endif

			status = readl(base + REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS);

			dev_dbg(mcfrc_device->dev
				, "%s: REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS:(%x, %x) before irq rst\n", __func__, REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS, status);

			spin_lock_irqsave(&mcfrc_device->slock, flags);
			set_bit(1, &mcfrc_device->task_irq);
			mcfrc_device->state_irq = 0;
			init_completion(&mcfrc_device->rst_complete);
			spin_unlock_irqrestore(&mcfrc_device->slock, flags);

#ifdef HWMCFRC_PROFILE_ENABLE
			global_time_start = ktime_get();
#endif
			// raise sw reset
			writel(0x0, base + REG_TOP_0x0008_MC_SW_RESET);

			// wait
			//wait_for_completion(&mcfrc_device->rst_complete);
			ret_wait = wait_for_completion_timeout(&mcfrc_device->rst_complete, tempJ);

#ifdef HWMCFRC_PROFILE_ENABLE
			global_time_end1 = ktime_get();
			dev_info(mcfrc_device->dev, "HW PROCESS TIME (global_time_end1): %lld nanosec\n",
				ktime_to_ns(ktime_sub(global_time_end1, global_time_start1)));
#endif
			disable_mcfrc(mcfrc_device);

#ifdef HWMCFRC_PROFILE_ENABLE
			global_time_end2 = ktime_get();
			dev_info(mcfrc_device->dev, "HW PROCESS TIME (global_time_end2): %lld nanosec\n",
				ktime_to_ns(ktime_sub(global_time_end2, global_time_start2)));
#endif

			dev_dbg(mcfrc_device->dev
				, "%s:mcfrc_device->state_irq = %x, ret_wait = %lu\n", __func__, mcfrc_device->state_irq, ret_wait);
		}
		return ret;
	}
	default:
		dev_err(mcfrc_device->dev, "%s: Unknown compat_ioctl cmd %x, %x\n",
			__func__, cmd, COMPAT_HWMCFRC_IOC_PROCESS);

		return -EINVAL;
	}

	return 0;
}
#endif

static const struct file_operations mcfrc_fops = {
	.owner = THIS_MODULE,
	.open = mcfrc_open,
	.release = mcfrc_release,
	.unlocked_ioctl = mcfrc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mcfrc_compat_ioctl32,
#endif
};

static void mcfrc_deregister_device(struct mcfrc_dev *mcfrc_device)
{
	dev_info(mcfrc_device->dev, "%s: deregistering mcfrc device\n", __func__);
	misc_deregister(&mcfrc_device->misc);
}


/**
 * Acquire and "prepare" the clock producer resource.
 * This is the setup needed before enabling the clock source.
 */
 static int mcfrc_clk_devm_get_prepare(struct mcfrc_dev *mcfrc)
 {
	 struct device *dev = mcfrc->dev;
	 int ret = 0;

	 dev_dbg(mcfrc->dev, "%s: acquiring clock with devm\n", __func__);

	 mcfrc->clk_producer = devm_clk_get(dev, clk_producer_name);

	 if (IS_ERR(mcfrc->clk_producer)) {
		 dev_err(dev, "%s clock is not present\n",
			 clk_producer_name);
		 return PTR_ERR(mcfrc->clk_producer);
	 }

	 dev_dbg(mcfrc->dev, "%s: preparing the clock\n", __func__);

	 if (!IS_ERR(mcfrc->clk_producer)) {
		 ret = clk_prepare(mcfrc->clk_producer);
		 if (ret) {
			 dev_err(mcfrc->dev, "%s: clk preparation failed (%d)\n",
				 __func__, ret);

			 //invalidate clk var so that none else will use it
			 mcfrc->clk_producer = ERR_PTR(-EINVAL);

			 //no need to release anything as devm will take care of
			 //if eventually
			 return ret;
		 }
	 }

	 return 0;
 }


 /**
  * Request the system to Enable/Disable the clock source.
  *
  * We cannot read/write to H/W registers or use the device in any way
  * without enabling the clock source first.
  */
static int mcfrc_clock_gating(struct mcfrc_dev *mcfrc, bool on)
{
	int ret = 0;

	if (!IS_ERR(mcfrc->clk_producer)) {
		if (on) {
			ret = clk_enable(mcfrc->clk_producer);
			if (ret) {
				dev_err(mcfrc->dev, "%s failed to enable clock : %d\n"
					, __func__, ret);
				return ret;
			}
			dev_dbg(mcfrc->dev, "mcfrc clock enabled\n");
		}
		else {
			clk_disable(mcfrc->clk_producer);
			if (ret) {
				dev_err(mcfrc->dev, "%s failed to disable clock : %d\n"
					, __func__, ret);
				return ret;
			}
			dev_dbg(mcfrc->dev, "mcfrc clock disabled\n");
		}
	}
	else {
		dev_err(mcfrc->dev, "%s failed to gating clock : %d, %d\n"
			, __func__, ret, on);
		return -1;
	}

	return 0;
}


#if defined(CONFIG_EXYNOS_IOVMM)
static int mcfrc_sysmmu_fault_handler(struct iommu_domain *domain,
	struct device *dev,
	unsigned long fault_addr,
	int fault_flags, void *p)
{
	dev_err(dev, "%s: sysmmu fault!\n", __func__);

	// Dump BUS errors
	return 0;
}
#else
static int mcfrc_sysmmu_fault_handler(struct iommu_fault *fault, void *data)
{
	struct mcfrc_dev *mcfrc = data;
	dev_err(mcfrc->dev, "%s: sysmmu fault!\n", __func__);

	// Dump BUS errors
	return 0;
}
#endif


static irqreturn_t mcfrc_irq_handler_done(int irq, void *priv)
{
	unsigned long flags;
	//Cavy unsigned int int_status;
	struct mcfrc_dev *mcfrc_device = priv;
	struct mcfrc_task *task;
	//bool status = true;
	unsigned int status;
	bool succes = false;
	bool was_int = false;

#ifdef HWMCFRC_PROFILE_ENABLE
	global_time_end = ktime_get();
	dev_info(mcfrc_device->dev, "HW PROCESS TIME (irq_handler_done): %lld nanosec\n",
		ktime_to_ns(ktime_sub(global_time_end, global_time_start)));
#endif

	dev_dbg(mcfrc_device->dev, "%s: BEGIN\n", __func__);

	dev_dbg(mcfrc_device->dev, "%s: getting irq status\n", __func__);

	//Cavy int_status = mcfrc_get_interrupt_status(mcfrc_device->regs);
	//Cavy dev_dbg(mcfrc_device->dev, "%s: irq status : %x\n", __func__, int_status);

	GET_FRC_MC_DONE_IRQ_STATUS_REG(mcfrc_device->regs, status);

	dev_dbg(mcfrc_device->dev
		, "%s: REG_TOP_0x0058_FRC_MC_DONE_IRQ_STATUS:(%x, %x)\n", __func__, REG_TOP_0x0058_FRC_MC_DONE_IRQ_STATUS, status);

	//Check that the board intentionally triggered the interrupt
	//if (int_status) {
	//	dev_warn(mcfrc_device->dev, "bogus interrupt\n");
	//	return IRQ_NONE;
	//}

	if (CAVY_TEST_BIT(IRQ_MC_DONE_NORM, status)) {
		//CAVY_CLI_BIT(IRQ_MC_DONE_NORM, status);
		was_int = true;
		succes = true;
	}
	if (CAVY_TEST_BIT(IRQ_MC_DONE_TIME, status)) {
		//CAVY_CLI_BIT(IRQ_MC_DONE_TIME, status);
		was_int = true;
	}
	if (CAVY_TEST_BIT(IRQ_MC_DONE_FB, status)) {
		//CAVY_CLI_BIT(IRQ_MC_DONE_FB, status);
		was_int = true;
		succes = true;
	}

	if (!was_int) {
		dev_warn(mcfrc_device->dev, "bogus mc_done interrupt\n");
		return IRQ_NONE;
	}

	spin_lock_irqsave(&mcfrc_device->slock, flags);
	//HW is done processing
	clear_bit(DEV_RUN, &mcfrc_device->state);
	spin_unlock_irqrestore(&mcfrc_device->slock, flags);

	dev_dbg(mcfrc_device->dev, "%s: clearing irq status\n", __func__);

	//Cavy mcfrc_interrupt_clear(mcfrc_device->regs);
	SET_MC_INTR_CLEAR_REG(mcfrc_device->regs, 0x1);//0x000C

	spin_lock_irqsave(&mcfrc_device->lock_task, flags);
	task = mcfrc_device->current_task;
	if (!task) {
		spin_unlock_irqrestore(&mcfrc_device->lock_task, flags);
		dev_err(mcfrc_device->dev, "received null task in irq handler\n");
		return IRQ_HANDLED;
	}
	spin_unlock_irqrestore(&mcfrc_device->lock_task, flags);

	dev_dbg(mcfrc_device->dev, "%s: finishing task\n", __func__);

#if (MCFRC_DBG_MODE == 1) // remove debug parameters
	if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_12, task->user_task.dbg_control_flags)) {
		//alternative start
		mcfrc_run_programm(mcfrc_device, &(task->user_task.programms[2]));
	}
#endif

	//signal task completion and schedule next task
	if (mcfrc_task_signal_completion(mcfrc_device, task, succes) < 0) {
		dev_err(mcfrc_device->dev, "%s: error while finishing a task!\n" , __func__);

		return IRQ_HANDLED;
	}

	dev_dbg(mcfrc_device->dev, "%s: END\n", __func__);

	return IRQ_HANDLED;
}

static irqreturn_t mcfrc_irq_handler_err_srst(int irq, void *priv)
{
	unsigned long flags;
	unsigned int var;
	//Cavy unsigned int int_status;
	struct mcfrc_dev *mcfrc = priv;
	//struct mcfrc_task *task;
	//bool status = true;
	unsigned int status;
	//bool was_int = false;
	struct mcfrc_task *task;

#ifdef HWMCFRC_PROFILE_ENABLE
	global_time_end = ktime_get();
	dev_info(mcfrc->dev, "HW PROCESS TIME (handler_err_srst): %lld nanosec\n",
		ktime_to_ns(ktime_sub(global_time_end, global_time_start)));
#endif

	dev_dbg(mcfrc->dev, "%s: BEGIN\n", __func__);

	dev_dbg(mcfrc->dev, "%s: getting irq status\n", __func__);

	//Cavy int_status = mcfrc_get_interrupt_status(mcfrc->regs);
	//Cavy dev_dbg(mcfrc->dev, "%s: irq status : %x\n", __func__, int_status);

	//Check that the board intentionally triggered the interrupt
	//if (int_status) {
	//	dev_warn(mcfrc->dev, "bogus interrupt\n");
	//	return IRQ_NONE;
	//}

	//Cavy spin_lock_irqsave(&mcfrc->slock, flags);
	//Cavy //HW is done processing
	//Cavy clear_bit(DEV_RUN, &mcfrc->state);
	//Cavy spin_unlock_irqrestore(&mcfrc->slock, flags);

	dev_dbg(mcfrc->dev, "%s: clearing irq status\n", __func__);

	//Cavy mcfrc_interrupt_clear(mcfrc->regs);
	//status = readl(mcfrc->regs + REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS);
	GET_FRC_MC_DBL_ERR_IRQ_STATUS_REG(mcfrc->regs, status);

	dev_dbg(mcfrc->dev
		, "%s: REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS:(%x, %x)\n", __func__, REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS, status);

	spin_lock_irqsave(&mcfrc->slock, flags);
	if (CAVY_TEST_BIT(IRQ_ERRRST_STATE_RST, status)) {
		CAVY_SET_BIT(IRQ_ERRRST_STATE_RST, mcfrc->state_irq);
		//was_int = true;
	}
	if (CAVY_TEST_BIT(IRQ_ERRRST_STATE_DMAN, status)) {
		CAVY_SET_BIT(IRQ_ERRRST_STATE_DMAN, mcfrc->state_irq);
		//was_int = true;
	}
	if (CAVY_TEST_BIT(IRQ_ERRRST_STATE_TERR, status)) {
		CAVY_SET_BIT(IRQ_ERRRST_STATE_TERR, mcfrc->state_irq);
		//was_int = true;
	}
	spin_unlock_irqrestore(&mcfrc->slock, flags);


	if(CAVY_TEST_BIT(IRQ_ERRRST_STATE_RST, status)) {
		//writel(0x1, mcfrc->regs + REG_TOP_0x0008_MC_SW_RESET);
		SET_MC_SW_RESET_REG(mcfrc->regs, 0x1);

		spin_lock_irqsave(&mcfrc->slock, flags);
		if (test_bit(1, &mcfrc->task_irq)) {
			clear_bit(1, &mcfrc->task_irq);
			complete(&mcfrc->rst_complete);
		}
		spin_unlock_irqrestore(&mcfrc->slock, flags);
	}
	else if (CAVY_TEST_BIT(IRQ_ERRRST_STATE_DMAN, status)) {
		SET_DBL_INTR_CLEAR_REG(mcfrc->regs, 0x1);
	}
	else {
#ifndef DEBUG
		var = 0;
		GET_MC_ERROR_STATUS_REG(mcfrc->regs, var);
		dev_err(mcfrc->dev, "%s: MC_ERROR_STATUS after: 0x%08X \n", __func__, var);
		var = 0;
		GET_MC_CORE_STATUS_0_REG(mcfrc->regs, var);
		dev_info(mcfrc->dev, "%s: MC_CORE_STATUS_0 after: 0x%08X \n", __func__, var);
		var = 0;
		GET_MC_CORE_STATUS_1_REG(mcfrc->regs, var);
		dev_info(mcfrc->dev, "%s: MC_CORE_STATUS_1 after: 0x%08X \n", __func__, var);

		var = 0;
		GET_FRC_MC_DONE_IRQ_STATUS_REG(mcfrc->regs, var);
		dev_info(mcfrc->dev, "%s: FRC_MC_DONE_IRQ_STATUS after: 0x%08X \n", __func__, var);

		var = 0;
		GET_FRC_MC_DBL_ERR_IRQ_STATUS_REG(mcfrc->regs, var);
		dev_info(mcfrc->dev, "%s:FRC_MC_DBL_ERR_IRQ_STATUS after: 0x%08X \n", __func__, var);
		var = 0;
		GET_RDMA_ERROR_STATUS_REG(mcfrc->regs, 0, var);
		dev_info(mcfrc->dev, "%s:RDMA_ERROR_STATUS 0 after: 0x%08X \n", __func__, var);
		var = 0;
		GET_RDMA_ERROR_STATUS_REG(mcfrc->regs, 1, var);
		dev_info(mcfrc->dev, "%s:RDMA_ERROR_STATUS 1 after: 0x%08X \n", __func__, var);
		var = 0;
		GET_RDMA_ERROR_STATUS_REG(mcfrc->regs, 2, var);
		dev_info(mcfrc->dev, "%s:RDMA_ERROR_STATUS 2 after: 0x%08X \n", __func__, var);
		var = 0;
		GET_RDMA_ERROR_STATUS_REG(mcfrc->regs, 3, var);
		dev_info(mcfrc->dev, "%s:RDMA_ERROR_STATUS 3 after: 0x%08X \n", __func__, var);
		var = 0;
		GET_RDMA_ERROR_STATUS_REG(mcfrc->regs, 4, var);
		dev_info(mcfrc->dev, "%s:RDMA_ERROR_STATUS 4 after: 0x%08X \n", __func__, var);
		var = 0;
		GET_RDMA_CH5_ERROR_STATUS_01_REG(mcfrc->regs, var);
		dev_info(mcfrc->dev, "%s:RDMA_CH5_ERROR_STATUS_01after: 0x%08X \n", __func__, var);

		var = 0;
		GET_WDMA_ERROR_STATUS_REG(mcfrc->regs, 0, var);
		dev_info(mcfrc->dev, "%s:WDMA_ERROR_STATUS 0 after: 0x%08X \n", __func__, var);
		var = 0;
		GET_WDMA_ERROR_STATUS_REG(mcfrc->regs, 1, var);
		dev_info(mcfrc->dev, "%s:WDMA_ERROR_STATUS 1 after: 0x%08X \n", __func__, var);
		var = 0;
		GET_WDMA_ERROR_STATUS_REG(mcfrc->regs, 2, var);
		dev_info(mcfrc->dev, "%s:WDMA_ERROR_STATUS 2 after: 0x%08X \n", __func__, var);
		var = 0;
		GET_WDMA_ERROR_STATUS_REG(mcfrc->regs, 3, var);
		dev_info(mcfrc->dev, "%s:WDMA_ERROR_STATUS 3 after: 0x%08X \n", __func__, var);
#else
		mcfrc_print_all_regs(mcfrc, mcfrc->regs);
#endif

		SET_WDMA_ERROR_CLEAR_REG(mcfrc->regs, 0, 0xffff);
		SET_WDMA_ERROR_CLEAR_REG(mcfrc->regs, 1, 0xffff);
		SET_WDMA_ERROR_CLEAR_REG(mcfrc->regs, 2, 0xffff);
		SET_WDMA_ERROR_CLEAR_REG(mcfrc->regs, 3, 0xffff);

		SET_RDMA_ERROR_CLEAR_REG(mcfrc->regs, 0, 0xffff);
		SET_RDMA_ERROR_CLEAR_REG(mcfrc->regs, 1, 0xffff);
		SET_RDMA_ERROR_CLEAR_REG(mcfrc->regs, 2, 0xffff);
		SET_RDMA_ERROR_CLEAR_REG(mcfrc->regs, 3, 0xffff);
		SET_RDMA_ERROR_CLEAR_REG(mcfrc->regs, 4, 0xffff);
		SET_RDMA0_CH5_ERROR_CLEAR_REG(mcfrc->regs, 0xffff);

		SET_WDMA_ERROR_ENABLE_REG(mcfrc->regs, 0, 0xfffe);
		SET_WDMA_ERROR_ENABLE_REG(mcfrc->regs, 1, 0xfffe);
		SET_WDMA_ERROR_ENABLE_REG(mcfrc->regs, 2, 0xfffe);
		SET_WDMA_ERROR_ENABLE_REG(mcfrc->regs, 3, 0xfffe);

		SET_RDMA_ERROR_ENABLE_REG(mcfrc->regs, 0, 0xfffe);
		SET_RDMA_ERROR_ENABLE_REG(mcfrc->regs, 1, 0xfffe);
		SET_RDMA_ERROR_ENABLE_REG(mcfrc->regs, 2, 0xfffe);
		SET_RDMA_ERROR_ENABLE_REG(mcfrc->regs, 3, 0xfffe);
		SET_RDMA_ERROR_ENABLE_REG(mcfrc->regs, 4, 0xfffe);
		SET_RDMA_CH5_ERROR_ENABLE_01_REG(mcfrc->regs, 0xfffe);

		SET_MC_ERROR_CLEAR_REG(mcfrc->regs, 0xffffffff);
		GET_FRC_MC_DBL_ERR_IRQ_STATUS_REG(mcfrc->regs, status);
		var = 0;
		GET_MC_ERROR_STATUS_REG(mcfrc->regs, var);
		dev_dbg(mcfrc->dev, "%s: MC_ERROR_STATUS after1: 0x%08X \n", __func__, var);
		dev_dbg(mcfrc->dev
			, "%s: after1 SET_MC_ERROR_CLEAR_REG :(%x, %08x)\n", __func__, REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS, status);

		var = 0;
		GET_MC_ERROR_STATUS_REG(mcfrc->regs, var);
		dev_dbg(mcfrc->dev, "%s: MC_ERROR_STATUS after2: 0x%08X \n", __func__, var);
		GET_FRC_MC_DBL_ERR_IRQ_STATUS_REG(mcfrc->regs, status);
		dev_dbg(mcfrc->dev
			, "%s: after2 SET_MC_ERROR_CLEAR_REG :(%x, %08x)\n", __func__, REG_TOP_0x005C_FRC_MC_DBL_ERR_IRQ_STATUS, status);

		spin_lock_irqsave(&mcfrc->slock, flags);
		//HW is done processing
		clear_bit(DEV_RUN, &mcfrc->state);
		spin_unlock_irqrestore(&mcfrc->slock, flags);

		spin_lock_irqsave(&mcfrc->lock_task, flags);
		task = mcfrc->current_task;
		if (!task) {
			spin_unlock_irqrestore(&mcfrc->lock_task, flags);
			dev_err(mcfrc->dev, "received null task in irq handler\n");
			return IRQ_HANDLED;
		}
		spin_unlock_irqrestore(&mcfrc->lock_task, flags);

#if (MCFRC_DBG_MODE == 1) // remove debug parameters
		if (CAVY_TEST_BIT(DBG_CONTROL_FLAG_13, task->user_task.dbg_control_flags)) {
			//additional processing
			mcfrc_run_programm(mcfrc, &(task->user_task.programms[3]));
		}
#endif

		//signal task completion and schedule next task
		if (mcfrc_task_signal_completion(mcfrc, task, false) < 0) {
			dev_err(mcfrc->dev, "%s: error while finishing a task!\n", __func__);

			return IRQ_HANDLED;
		}

	}

	//if (!was_int) {
	//    dev_warn(mcfrc->dev, "bogus err_srst interrupt\n");
	//    return IRQ_NONE;
	//}

   /* writel(0x1, mcfrc->regs + REG_TOP_0x0008_MC_SW_RESET);
	//SET_MC_SW_RESET_REG(mcfrc->regs, 0x1);

	spin_lock_irqsave(&mcfrc->slock, flags);
	if (test_bit(1, &mcfrc->task_irq)) {
		clear_bit(1, &mcfrc->task_irq);
		complete(&mcfrc->rst_complete);
	}
	spin_unlock_irqrestore(&mcfrc->slock, flags);*/

	//Cavy spin_lock_irqsave(&mcfrc->lock_task, flags);
	//Cavy task = mcfrc->current_task;
	//Cavy if (!task) {
	//Cavy     spin_unlock_irqrestore(&mcfrc->lock_task, flags);
	//Cavy     dev_err(mcfrc->dev, "received null task in irq handler\n");
	//Cavy     return IRQ_HANDLED;
	//Cavy }
	//Cavy spin_unlock_irqrestore(&mcfrc->lock_task, flags);

	//Cavy dev_dbg(mcfrc->dev, "%s: finishing task\n", __func__);

	//signal task completion and schedule next task
	//Cavy if (mcfrc_task_signal_completion(mcfrc, task, status) < 0) {
	//Cavy     dev_err(mcfrc->dev, "%s: error while finishing a task!\n" , __func__);

	//Cavy     return IRQ_HANDLED;
	//Cavy }

	dev_dbg(mcfrc->dev, "%s: END\n", __func__);

	return IRQ_HANDLED;
}

/**
 * Initialization routine.
 *
 * This is called at boot time, when the system finds a device that this
 * driver requested to be a handler of (see platform_driver struct below).
 *
 * Set up access to H/W registers, register the device, register the IRQ
 * resource, IOVMM page fault handler, spinlocks, etc.
 */
static int mcfrc_probe(struct platform_device *pdev)
{
	struct mcfrc_dev *mcfrc = NULL;
	struct resource *res = NULL;
	int ret = 0;
	int irq0 = -1;
	int irq1 = -1;

	// This replaces the old pre-device-tree dev.platform_data
	// To be populated with data coming from device tree
	mcfrc = devm_kzalloc(&pdev->dev, sizeof(struct mcfrc_dev), GFP_KERNEL);
	if (!mcfrc) {
		//not enough memory
		return -ENOMEM;
	}

	mcfrc->dev = &pdev->dev;

	spin_lock_init(&mcfrc->slock);

	// Get the memory resource and map it to kernel space.
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mcfrc->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mcfrc->regs)) {
		dev_err(&pdev->dev, "failed to claim/map register region\n");
		return PTR_ERR(mcfrc->regs);
	}

	// Get IRQ resource and register IRQ handler.
	irq0 = platform_get_irq(pdev, 0);//INTREQ__FRC_MC_DONE
	if (irq0 < 0) {
		if (irq0 != -EPROBE_DEFER)
			dev_err(&pdev->dev, "cannot get irq0 (INTREQ__FRC_MC_DONE)\n");
		return irq0;
	}
	irq1 = platform_get_irq(pdev, 1);//INTREQ__FRC_MC_DBL_ERR
	if (irq1 < 0) {
		if (irq1 != -EPROBE_DEFER)
			dev_err(&pdev->dev, "cannot get irq1 (INTREQ__FRC_MC_DBL_ERR)\n");
		return irq1;
	}

	// passing 0 as flags means the flags will be read from DT
	ret = devm_request_irq(&pdev->dev, irq0,
		(void *)mcfrc_irq_handler_done, 0,
		pdev->name, mcfrc);
	if (ret) {
		dev_err(&pdev->dev, "failed to install irq0 (INTREQ__FRC_MC_DONE)\n");
		return ret;
	}
	ret = devm_request_irq(&pdev->dev, irq1,
		(void *)mcfrc_irq_handler_err_srst, 0,
		pdev->name, mcfrc);
	if (ret) {
		dev_err(&pdev->dev, "failed to install irq1 (INTREQ__FRC_MC_DBL_ERR)\n");
		return ret;
	}

	mcfrc->misc.minor = MISC_DYNAMIC_MINOR;
	mcfrc->misc.name = NODE_NAME;
	mcfrc->misc.fops = &mcfrc_fops;
	mcfrc->misc.parent = &pdev->dev;
	ret = misc_register(&mcfrc->misc);
	if (ret)
		return ret;

	INIT_LIST_HEAD(&mcfrc->contexts);

	spin_lock_init(&mcfrc->lock_task);
	spin_lock_init(&mcfrc->lock_ctx);

	//mcfrc->timeout_jiffies = -1; //msecs_to_jiffies(500);

	// clock
	ret = mcfrc_clk_devm_get_prepare(mcfrc);
	if (ret)
		goto err_clkget;

	platform_set_drvdata(pdev, mcfrc);

#ifdef CONFIG_MCFRC_USE_DEVFREQ
	mcfrc->qos_req_level = 663000;
	if (!of_property_read_u32(pdev->dev.of_node, "mcfrc,int_qos_minlock",
		(u32 *)&mcfrc->qos_req_level)) {
		if (mcfrc->qos_req_level > 0) {
			exynos_pm_qos_add_request(&mcfrc->qos_req,
				PM_QOS_INTCAM_THROUGHPUT, 0);

			dev_info(&pdev->dev, "INT Min.Lock Freq. = %u\n",
				mcfrc->qos_req_level);
		}
	}
#endif


	pm_runtime_set_autosuspend_delay(&pdev->dev, 1000);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

#if defined(CONFIG_EXYNOS_IOVMM)
	iovmm_set_fault_handler(&pdev->dev,
				mcfrc_sysmmu_fault_handler, mcfrc);

	ret = iovmm_activate(&pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: Failed to activate iommu (%d)\n",
			__func__, ret);
		goto err_iovmm;
	}
#else
	iommu_register_device_fault_handler(&pdev->dev, mcfrc_sysmmu_fault_handler, mcfrc);
#endif

	//Power device on to access registers
	ret = pm_runtime_get_sync(&pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev
			, "%s: failed to pwr on device with runtime pm (%d)\n",
			__func__, ret);
		goto err_pm;
	}

	pm_runtime_put_sync(&pdev->dev);
	mcfrc->workqueue = alloc_ordered_workqueue(NODE_NAME, WQ_MEM_RECLAIM);
	if (!mcfrc->workqueue) {
		ret = -ENOMEM;
		goto err_pm;
	}

	dev_info(&pdev->dev, "HWmcfrc probe completed successfully.\n");

	return 0;

err_pm:
	pm_runtime_dont_use_autosuspend(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
#if defined(CONFIG_EXYNOS_IOVMM)
	iovmm_deactivate(&pdev->dev);
err_iovmm:
#else
	iommu_unregister_device_fault_handler(&pdev->dev);
#endif
#ifdef CONFIG_MCFRC_USE_DEVFREQ
	if (mcfrc->qos_req_level > 0)
		exynos_pm_qos_remove_request(&mcfrc->qos_req);
#endif
	if (!IS_ERR(mcfrc->clk_producer)) /* devm will call clk_put */
		clk_unprepare(mcfrc->clk_producer);
	platform_set_drvdata(pdev, NULL);
err_clkget:
	mcfrc_deregister_device(mcfrc);

	dev_err(&pdev->dev, "HWMCFRC probe is failed.\n");

	return ret;
}

//Note: drv->remove() will not be called by the kernel framework because this
//      is a builtin driver. (see builtin_platform_driver at the bottom)
static int mcfrc_remove(struct platform_device *pdev)
{
	struct mcfrc_dev *mcfrc = platform_get_drvdata(pdev);

	dev_dbg(mcfrc->dev, "%s: BEGIN removing device\n", __func__);

#ifdef CONFIG_MCFRC_USE_DEVFREQ
	if (mcfrc->qos_req_level > 0 && exynos_pm_qos_request_active(&mcfrc->qos_req))
		exynos_pm_qos_remove_request(&mcfrc->qos_req);
#endif

	//From the docs:
	//Drivers in ->remove() callback should undo the runtime PM changes done
	//in ->probe(). Usually this means calling pm_runtime_disable(),
	//pm_runtime_dont_use_autosuspend() etc.
	//disable runtime pm before releasing the clock,
	//or pm runtime could still use it
	pm_runtime_dont_use_autosuspend(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

#if defined(CONFIG_EXYNOS_IOVMM)
	iovmm_deactivate(&pdev->dev);
#else
	iommu_unregister_device_fault_handler(&pdev->dev);
#endif

	if (!IS_ERR(mcfrc->clk_producer)) /* devm will call clk_put */
		clk_unprepare(mcfrc->clk_producer);

	platform_set_drvdata(pdev, NULL);

	mcfrc_deregister_device(mcfrc);

	dev_dbg(mcfrc->dev, "%s: END removing device\n", __func__);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mcfrc_suspend(struct device *dev)
{
	struct mcfrc_dev *mcfrc = dev_get_drvdata(dev);

	dev_dbg(dev, "%s: BEGIN\n", __func__);

	set_bit(DEV_SUSPEND, &mcfrc->state);

	dev_dbg(dev, "%s: END\n", __func__);

	return 0;
}

static int mcfrc_resume(struct device *dev)
{
	struct mcfrc_dev *mcfrc = dev_get_drvdata(dev);

	dev_dbg(dev, "%s: BEGIN\n", __func__);

	clear_bit(DEV_SUSPEND, &mcfrc->state);

	dev_dbg(dev, "%s: END\n", __func__);

	return 0;
}
#endif

#ifdef CONFIG_PM
static int mcfrc_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mcfrc_dev *mcfrc = platform_get_drvdata(pdev);
	unsigned long flags = 0;

	dev_dbg(dev, "%s: BEGIN\n", __func__);

	spin_lock_irqsave(&mcfrc->slock, flags);
	//According to official runtime PM docs, when using runtime PM
	//autosuspend, it might happen that the autosuspend timer expires,
	//runtime_suspend is entered, and at the same time we receive new job.
	//In that case we return -EBUSY to tell runtime PM core that we're
	//not ready to go in low-power state.
	if (test_bit(DEV_RUN, &mcfrc->state)) {
		dev_info(dev, "deferring suspend, device is still processing!");
		spin_unlock_irqrestore(&mcfrc->slock, flags);

		return -EBUSY;
	}
	spin_unlock_irqrestore(&mcfrc->slock, flags);

	dev_dbg(dev, "%s: gating clock, suspending\n", __func__);

	HWMCFRC_PROFILE(mcfrc_clock_gating(mcfrc, false), "SWITCHING OFF CLOCK", mcfrc->dev);

#ifdef CONFIG_MCFRC_USE_DEVFREQ
	HWMCFRC_PROFILE(
		if (mcfrc->qos_req_level > 0)
			exynos_pm_qos_update_request(&mcfrc->qos_req, 0); ,
			"UPDATE QoS REQUEST", mcfrc->dev
			);
#endif

	dev_dbg(dev, "%s: END\n", __func__);

	return 0;
}

static int mcfrc_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mcfrc_dev *mcfrc = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s: BEGIN\n", __func__);
	dev_dbg(dev, "%s: ungating clock, resuming\n", __func__);

	HWMCFRC_PROFILE(mcfrc_clock_gating(mcfrc, true), "SWITCHING ON CLOCK", mcfrc->dev);

#ifdef CONFIG_MCFRC_USE_DEVFREQ
	HWMCFRC_PROFILE(
	if (mcfrc->qos_req_level > 0)
		exynos_pm_qos_update_request(&mcfrc->qos_req, mcfrc->qos_req_level);,
	"UPDATE QoS REQUEST", mcfrc->dev
	);
#endif

	dev_dbg(dev, "%s: END\n", __func__);

	return 0;
}
#endif


static const struct dev_pm_ops mcfrc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mcfrc_suspend, mcfrc_resume)
	SET_RUNTIME_PM_OPS(mcfrc_runtime_suspend, mcfrc_runtime_resume, NULL)
};

// of == OpenFirmware, aka DeviceTree
// This allows matching devices which are specified as "vendorid,deviceid"
// in the device tree to this driver
static const struct of_device_id exynos_mcfrc_match[] = {
{
	.compatible = "samsung,exynos-mcfrc",
},
{},
};

// Load the kernel module when the device is matched against this driver
MODULE_DEVICE_TABLE(of, exynos_mcfrc_match);

static struct platform_driver mcfrc_driver = {
	.probe = mcfrc_probe,
	.remove = mcfrc_remove,
	.driver = {
	// Any device advertising itself as MODULE_NAME will be given
	// this driver.
	// No ID table needed (this was more relevant in the past,
	// when platform_device struct was used instead of device-tree)
	.name = MODULE_NAME,
	.owner = THIS_MODULE,
	.pm = &mcfrc_pm_ops,
	// Tell the kernel what dev tree names this driver can handle.
	// This is needed because device trees are meant to work with
	// more than one OS, so the driver name might not match the name
	// which is in the device tree.
	.of_match_table = of_match_ptr(exynos_mcfrc_match),
}
};

builtin_platform_driver(mcfrc_driver);

MODULE_AUTHOR("Jungik Seo <jungik.seo@samsung.com>");
MODULE_AUTHOR("Igor Kovliga <i.kovliga@samsung.com>");
MODULE_AUTHOR("Sergei Ilichev <s.ilichev@samsung.com>");
MODULE_DESCRIPTION("Samsung MC FRC Driver");
MODULE_LICENSE("GPL");
