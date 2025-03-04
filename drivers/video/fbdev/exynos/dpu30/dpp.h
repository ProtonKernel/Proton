/* linux/drivers/video/fbdev/exynos/dpu/dpp.h
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *
 * header file for Samsung EXYNOS SoC DPP driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SAMSUNG_DPP_H__
#define __SAMSUNG_DPP_H__

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/ion.h>
#include <linux/dma-buf.h>
#if IS_ENABLED(CONFIG_EXYNOS_BTS) || IS_ENABLED(CONFIG_EXYNOS_BTS_MODULE)
#include <soc/samsung/bts.h>
#endif

#include "decon.h"
/* TODO: SoC dependency will be removed */
#include "./cal_2100/regs-dpp.h"
#include "./cal_2100/dpp_cal.h"

#if IS_ENABLED(CONFIG_MCD_PANEL)
#include "mcd.h"
#endif

extern int dpp_log_level;

#define DPP_MODULE_NAME		"exynos-dpp"
#define MAX_DPP_CNT		MAX_DPP_SUBDEV
#define MAX_FMT_CNT		64
#define DEFAULT_FMT_CNT		8

/* about 1msec @ ACLK=630MHz */
#define INIT_RCV_NUM		630000

#if defined(CONFIG_EXYNOS_SUPPORT_HWFC)
#define HWFC_LINE_NUM	32
#endif

#if defined(CONFIG_EXYNOS_SUPPORT_VOTF_IN)
#define DPUF0_FIRST_DPP_ID	0
#define DPUF1_FIRST_DPP_ID	8
#endif

#define P010_Y_SIZE(w, h)		((w) * (h) * 2)
#define P010_CBCR_SIZE(w, h)		((w) * (h))
#define P010_CBCR_BASE(base, w, h)	((base) + P010_Y_SIZE((w), (h)))
/* ceil function for positive value */
#define p_ceil(n, d)	((n)/(d) + ((n)%(d) != 0))

#define check_align(width, height, align_w, align_h)\
	(IS_ALIGNED(width, align_w) && IS_ALIGNED(height, align_h))

#define is_normal(config) (DPP_ROT_NORMAL)
#define is_rotation(config) (config->dpp_parm.rot > DPP_ROT_180)
#define is_afbc(config) (config->compression)

#define IS_WB(attr)	(test_bit(DPP_ATTR_WBMUX, &attr) &&		\
			test_bit(DPP_ATTR_ODMA, &attr))

#define dpp_err(fmt, ...)							\
	do {									\
		if (dpp_log_level >= 3) {					\
			pr_err(pr_fmt(fmt), ##__VA_ARGS__);			\
		}								\
	} while (0)

#define dpp_warn(fmt, ...)							\
	do {									\
		if (dpp_log_level >= 4) {					\
			pr_warn(pr_fmt(fmt), ##__VA_ARGS__);			\
		}								\
	} while (0)

#define dpp_info(fmt, ...)							\
	do {									\
		if (dpp_log_level >= 6)					\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

#define dpp_dbg(fmt, ...)							\
	do {									\
		if (dpp_log_level >= 7)					\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

/* TODO: This will be removed */

enum dpp_csc_defs {
	/* csc_type */
	DPP_CSC_BT_601 = 0,
	DPP_CSC_BT_709 = 1,
	/* csc_range */
	DPP_CSC_NARROW = 0,
	DPP_CSC_WIDE = 1,
	/* csc_mode */
	CSC_COEF_HARDWIRED = 0,
	CSC_COEF_CUSTOMIZED = 1,
	/* csc_id used in csc_3x3_t[] : increase by even value */
	DPP_CSC_ID_BT_2020 = 0,
	DPP_CSC_ID_DCI_P3 = 2,
	CSC_CUSTOMIZED_START = 4,
};

enum dpp_state {
	DPP_STATE_ON,
	DPP_STATE_OFF,
};

/* WB MUX is output device of DECON in case of writeback operation */
enum wbmux_state {
	WBMUX_STATE_ON,		/* power on, clock on, sysmmu enable */
	WBMUX_STATE_OFF,	/* power off, clock off, sysmmu disable */
};

enum dpp_reg_area {
	REG_AREA_DPP = 0,
	REG_AREA_DMA,
	REG_AREA_DMA_COM,
	REG_AREA_ODMA,
};

enum dpp_attr {
	DPP_ATTR_AFBC		= 0,
	DPP_ATTR_BLOCK		= 1,
	DPP_ATTR_FLIP		= 2,
	DPP_ATTR_ROT		= 3,
	DPP_ATTR_CSC		= 4,
	DPP_ATTR_SCALE		= 5,
	DPP_ATTR_HDR		= 6,
	DPP_ATTR_C_HDR		= 7,
	DPP_ATTR_C_HDR10_PLUS	= 8,
	DPP_ATTR_WCG		= 9,
	DPP_ATTR_SBWC		= 10,

	DPP_ATTR_IDMA		= 16,
	DPP_ATTR_ODMA		= 17,
	DPP_ATTR_DPP		= 18,
	DPP_ATTR_WBMUX		= 19,
};

struct dpp_resources {
	struct clk *aclk;
	void __iomem *regs;
	void __iomem *dma_regs;
	void __iomem *dma_com_regs;
#if defined(CONFIG_EXYNOS_SUPPORT_VOTF_IN)
	void __iomem *votf_regs;
	u32 votf_base_addr;
#endif
	int irq;
	int dma_irq;
};

struct dpp_debug {
	u32 done_count;
	u32 recovery_cnt;
};

struct dpp_config {
	struct decon_win_config config;
	unsigned long rcv_num;
#if defined(CONFIG_EXYNOS_SUPPORT_HWFC)
	bool hwfc_enable;
	u32 hwfc_idx;
#endif
#if IS_ENABLED(CONFIG_EXYNOS_SBWC_LIBREQ)
	bool lib_requested;
#endif

#if IS_ENABLED(CONFIG_MCD_PANEL)
	struct mcd_dpp_config mcd_config;
#endif
};

struct dpp_size_range {
	u32 min;
	u32 max;
	u32 align;
};


enum dpp_restriction_rsvd {
	SRC_W_ROT_MAX = 0,
	LIB_RESERVED = 1,
};

struct dpp_restriction {
	struct dpp_size_range src_f_w;
	struct dpp_size_range src_f_h;
	struct dpp_size_range src_w;
	struct dpp_size_range src_h;
	u32 src_x_align;
	u32 src_y_align;

	struct dpp_size_range dst_f_w;
	struct dpp_size_range dst_f_h;
	struct dpp_size_range dst_w;
	struct dpp_size_range dst_h;
	u32 dst_x_align;
	u32 dst_y_align;

	struct dpp_size_range blk_w;
	struct dpp_size_range blk_h;
	u32 blk_x_align;
	u32 blk_y_align;

	u32 src_h_rot_max; /* limit of source img height in case of rotation */

	u32 format[MAX_FMT_CNT]; /* supported format list for each DPP channel */
	int format_cnt;

	u32 scale_down;
	u32 scale_up;

	u32 reserved[6];
};

struct dpp_ch_restriction {
	int id;
	unsigned long attr;

	struct dpp_restriction restriction;
	u32 reserved[4];
};

struct dpp_restrictions_info {
	u32 ver; /* version of dpp_restrictions_info structure */
	struct dpp_ch_restriction dpp_ch[MAX_DPP_CNT];
	int dpp_cnt;
	u32 reserved[4];
};

struct dpp_device {
	int id;
	int port;
	unsigned long attr;
	enum dpp_state state;
#if defined(SYSFS_UNITTEST_INTERFACE)
	u64 dpp_irq_err_state;
	u64 dma_irq_err_state;
#endif
	enum wbmux_state wb_state;	/* only for writeback */
	struct device *dev;
	struct v4l2_subdev sd;
	struct dpp_resources res;
	struct dpp_debug d;
	wait_queue_head_t framedone_wq;
	struct dpp_config *dpp_config;
	spinlock_t slock;
	spinlock_t dma_slock;
	struct mutex lock;
	struct dpp_restriction restriction;
#if IS_ENABLED(CONFIG_MCD_PANEL)
	struct mcd_dpp_device mcd_dpp;
#endif
};

extern struct dpp_device *dpp_drvdata[MAX_DPP_CNT];

static inline struct dpp_device *get_dpp_drvdata(u32 id)
{
	return dpp_drvdata[id];
}

static inline u32 dpp_read(u32 id, u32 reg_id)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	return readl(dpp->res.regs + reg_id);
}

static inline u32 dpp_read_mask(u32 id, u32 reg_id, u32 mask)
{
	u32 val = dpp_read(id, reg_id);
	val &= (mask);
	return val;
}

static inline void dpp_write(u32 id, u32 reg_id, u32 val)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	writel(val, dpp->res.regs + reg_id);
}

static inline void dpp_write_relaxed(u32 id, u32 reg_id, u32 val)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);

	writel_relaxed(val, dpp->res.regs + reg_id);
}

static inline void dpp_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	u32 old = dpp_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, dpp->res.regs + reg_id);
}

/* DPU_DMA Common part */
static inline u32 dma_com_read(u32 id, u32 reg_id)
{
	struct dpp_device *dpp = get_dpp_drvdata(0);
	return readl(dpp->res.dma_com_regs + reg_id);
}

static inline u32 dma_com_read_mask(u32 id, u32 reg_id, u32 mask)
{
	u32 val = dma_com_read(id, reg_id);
	val &= (~mask);
	return val;
}

static inline void dma_com_write(u32 id, u32 reg_id, u32 val)
{
	/* get reliable address when probing IDMA_G0 */
	struct dpp_device *dpp = get_dpp_drvdata(0);
	writel(val, dpp->res.dma_com_regs + reg_id);
}

static inline void dma_com_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	struct dpp_device *dpp = get_dpp_drvdata(0);
	u32 old = dma_com_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, dpp->res.dma_com_regs + reg_id);
}

/* DPU_DMA */
static inline u32 dma_read(u32 id, u32 reg_id)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	return readl(dpp->res.dma_regs + reg_id);
}

static inline u32 dma_read_mask(u32 id, u32 reg_id, u32 mask)
{
	u32 val = dma_read(id, reg_id);
	val &= (~mask);
	return val;
}

static inline void dma_write(u32 id, u32 reg_id, u32 val)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	writel(val, dpp->res.dma_regs + reg_id);
}

static inline void dma_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	u32 old = dma_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, dpp->res.dma_regs + reg_id);
}

#if defined(CONFIG_EXYNOS_SUPPORT_VOTF_IN)
/* DPU C2SERV Global(vOTF) */
static inline u32 votf_read(u32 id, u32 reg_id)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	return readl(dpp->res.votf_regs + reg_id);
}

static inline u32 votf_read_mask(u32 id, u32 reg_id, u32 mask)
{
	u32 val = votf_read(id, reg_id);
	val &= (~mask);
	return val;
}

static inline void votf_write(u32 id, u32 reg_id, u32 val)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	writel(val, dpp->res.votf_regs + reg_id);
}

static inline void votf_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	struct dpp_device *dpp = get_dpp_drvdata(id);
	u32 old = votf_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, dpp->res.votf_regs + reg_id);
}
#endif

static inline void dpp_select_format(struct dpp_device *dpp,
			struct dpp_img_format *vi, struct dpp_params_info *p)
{
	vi->normal = is_normal(dpp);
	vi->rot = p->rot;
	vi->scale = p->is_scale;
	vi->format = p->format;
	vi->afbc_en = p->is_comp;
	vi->wb = test_bit(DPP_ATTR_ODMA, &dpp->attr);
}

void dpp_dump(struct dpp_device *dpp);

#define DPP_WIN_CONFIG			_IOW('P', 0, struct dpp_config)
#define DPP_STOP			_IOW('P', 1, unsigned long)
#define DPP_DUMP			_IOW('P', 2, u32)
#define DPP_WB_WAIT_FOR_FRAMEDONE	_IOR('P', 3, u32)
#define DPP_WAIT_IDLE			_IOR('P', 4, unsigned long)
#define DPP_SET_RECOVERY_NUM		_IOR('P', 5, unsigned long)
#define DPP_AFBC_ATTR_ENABLED		_IOR('P', 6, unsigned long)
#define DPP_GET_PORT_NUM		_IOR('P', 7, unsigned long)
#define DPP_GET_RESTRICTION		_IOR('P', 8, unsigned long)
#define DPP_GET_SHD_ADDR		_IOR('P', 9, unsigned long)
#define DPP_CURSOR_WIN_CONFIG		_IOW('P', 10, struct dpp_config)

#endif /* __SAMSUNG_DPP_H__ */
