/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Samsung EXYNOS SoC MIPI-DSI Master driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __SAMSUNG_DSIM_H__
#define __SAMSUNG_DSIM_H__

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <media/v4l2-subdev.h>
#include <video/mipi_display.h>

#include "./panels/exynos_panel.h"
#include "./cal_2100/regs-dsim.h"
#include "./cal_2100/dsim_cal.h"

#if IS_ENABLED(CONFIG_MCD_PANEL)
#include "disp_err.h"
#include "mcd_dsim.h"
#endif

extern int dsim_log_level;

#define DSIM_MODULE_NAME			"exynos-dsim"
#define MAX_DSIM_CNT				2
#define DSIM_DDI_ID_LEN				3

#define DSIM_PIXEL_FORMAT_RGB24			0x3E
#define DSIM_PIXEL_FORMAT_RGB18_PACKED		0x1E
#define DSIM_PIXEL_FORMAT_RGB18			0x2E
#define DSIM_PIXEL_FORMAT_RGB30_PACKED		0x0D
#define DSIM_RX_FIFO_MAX_DEPTH			64
#define MAX_DSIM_DATALANE_CNT			4

#if defined(CONFIG_EXYNOS_EMUL_DISP)
#define EMUL_DISP_SLOW_DEGREE			1500
#define MIPI_WR_TIMEOUT				msecs_to_jiffies(EMUL_DISP_SLOW_DEGREE * 50)
#define MIPI_RD_TIMEOUT				msecs_to_jiffies(EMUL_DISP_SLOW_DEGREE * 100)
#else
#define MIPI_WR_TIMEOUT				msecs_to_jiffies(50)
#define MIPI_RD_TIMEOUT				msecs_to_jiffies(100)
#endif
#define DSIM_PL_FIFO_THRESHOLD			2048	/*this value depends on H/W */

#define dsim_err(fmt, ...)							\
	do {									\
		if (dsim_log_level >= 3) {					\
			pr_err(pr_fmt(fmt), ##__VA_ARGS__);			\
		}								\
	} while (0)

#define dsim_warn(fmt, ...)							\
	do {									\
		if (dsim_log_level >= 4) {					\
			pr_warn(pr_fmt(fmt), ##__VA_ARGS__);			\
		}								\
	} while (0)

#define dsim_info(fmt, ...)							\
	do {									\
		if (dsim_log_level >= 6)					\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

#define dsim_dbg(fmt, ...)							\
	do {									\
		if (dsim_log_level >= 7)					\
			pr_info(pr_fmt(fmt), ##__VA_ARGS__);			\
	} while (0)

extern struct dsim_device *dsim_drvdata[MAX_DSIM_CNT];

/*
 * for GKI
 * This is for building for kernel module
 */
enum {
	MIPI_DSI_DSC_PRA    = 0x07,
	MIPI_DSI_DSC_PPS    = 0x0a,
};

/* define video timer interrupt */
enum {
	DSIM_VBP = 0,
	DSIM_VSYNC,
	DSIM_V_ACTIVE,
	DSIM_VFP,
};

/* define dsi bist pattern */
enum {
	DSIM_COLOR_BAR = 0,
	DSIM_GRAY_GRADATION,
	DSIM_USER_DEFINED,
	DSIM_PRB7_RANDOM,
};

/* define DSI lane types. */
enum {
	DSIM_LANE_CLOCK	= (1 << 0),
	DSIM_LANE_DATA0	= (1 << 1),
	DSIM_LANE_DATA1	= (1 << 2),
	DSIM_LANE_DATA2	= (1 << 3),
	DSIM_LANE_DATA3	= (1 << 4),
};

/* DSI Error report bit definitions */
enum {
	MIPI_DSI_ERR_SOT			= (1 << 0),
	MIPI_DSI_ERR_SOT_SYNC			= (1 << 1),
	MIPI_DSI_ERR_EOT_SYNC			= (1 << 2),
	MIPI_DSI_ERR_ESCAPE_MODE_ENTRY_CMD	= (1 << 3),
	MIPI_DSI_ERR_LOW_POWER_TRANSMIT_SYNC	= (1 << 4),
	MIPI_DSI_ERR_HS_RECEIVE_TIMEOUT		= (1 << 5),
	MIPI_DSI_ERR_FALSE_CONTROL		= (1 << 6),
	/* Bit 7 is reserved */
	MIPI_DSI_ERR_ECC_SINGLE_BIT		= (1 << 8),
	MIPI_DSI_ERR_ECC_MULTI_BIT		= (1 << 9),
	MIPI_DSI_ERR_CHECKSUM			= (1 << 10),
	MIPI_DSI_ERR_DATA_TYPE_NOT_RECOGNIZED	= (1 << 11),
	MIPI_DSI_ERR_VCHANNEL_ID_INVALID	= (1 << 12),
	MIPI_DSI_ERR_INVALID_TRANSMIT_LENGTH	= (1 << 13),
	/* Bit 14 is reserved */
	MIPI_DSI_ERR_PROTOCAL_VIOLATION		= (1 << 15),
	/* DSI_PROTOCAL_VIOLATION[15] is for protocol violation that is caused EoTp
	 * missing So this bit is egnored because of not supportung @S.LSI AP */
	/* FALSE_ERROR_CONTROL[6] is for detect invalid escape or turnaround sequence.
	 * This bit is not supporting @S.LSI AP because of non standard
	 * ULPS enter/exit sequence during power-gating */
	/* Bit [14],[7] is reserved */
	MIPI_DSI_ERR_BIT_MASK			= (0x3f3f), /* Error_Range[13:0] */
};

/* operation state of dsim driver */
enum dsim_state {
	DSIM_STATE_INIT,
	DSIM_STATE_ON,			/* HS clock was enabled. */
	DSIM_STATE_DOZE,		/* HS clock was enabled. */
	DSIM_STATE_ULPS,		/* DSIM was entered ULPS state */
	DSIM_STATE_DOZE_SUSPEND,	/* DSIM is suspend state */
	DSIM_STATE_OFF			/* DSIM is suspend state */
};

enum dphy_charic_value {
	M_PLL_CTRL1,
	M_PLL_CTRL2,
	B_DPHY_CTRL2,
	B_DPHY_CTRL3,
	B_DPHY_CTRL4,
	M_DPHY_CTRL1,
	M_DPHY_CTRL2,
	M_DPHY_CTRL3,
	M_DPHY_CTRL4
};

enum dsim_datalane_status {
	DSIM_DATALANE_STATUS_STOPDATA,
	DSIM_DATALANE_STATUS_HSDT,
	DSIM_DATALANE_STATUS_LPDT,
	DSIM_DATALANE_STATUS_TRIGGER,
	DSIM_DATALANE_STATUS_ULPSDATA,
	DSIM_DATALANE_STATUS_SEWCALDATA,
	DSIM_DATALANE_STATUS_BTA
};

struct dsim_pll_param {
	u32 p;
	u32 m;
	u32 s;
	u32 k;
	u32 pll_freq; /* in/out parameter: Mhz */
};

struct dphy_timing_value {
	u32 bps;
	u32 clk_prepare;
	u32 clk_zero;
	u32 clk_post;
	u32 clk_trail;
	u32 hs_prepare;
	u32 hs_zero;
	u32 hs_trail;
	u32 lpx;
	u32 hs_exit;
	u32 b_dphyctl;
};

struct dsim_resources {
	struct clk *pclk;
	struct clk *dphy_esc;
	struct clk *dphy_byte;
	struct clk *rgb_vclk0;
	struct clk *pclk_disp;
	struct clk *aclk;
	int lcd_power[2];
	int lcd_reset;
	int irq;
	void __iomem *regs;
	void __iomem *ss_regs;
	void __iomem *phy_regs;
	void __iomem *phy_regs_ex;
	struct regulator *regulator_1p8v;
	struct regulator *regulator_3p3v;
};

struct dsim_fb_handover {
	/* true  - fb reserved     */
	/* false - fb not reserved */
	bool reserved;
	phys_addr_t phys_addr;
	size_t phys_size;
};

struct exynos_dsim_cmd {
	u8 type;
	size_t data_len;
	const unsigned char *data_buf;
};

struct exynos_dsim_cmd_set {
	u32 cnt;
	u32 index[32];
};

#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
struct dsim_dma_buf_data {
	struct dma_buf			*dma_buf;
	struct dma_buf_attachment	*attachment;
	struct sg_table			*sg_table;
	dma_addr_t			dma_addr;
	struct dma_fence		*fence;
};
#endif

struct dsim_device {
	int id;
	enum dsim_state state;
#if 1// defined(SYSFS_UNITTEST_INTERFACE)
	u64 irq_err_state;
#endif
	struct device *dev;
	struct dsim_resources res;
	struct exynos_pm_domain *pd;

	unsigned int data_lane;
	u32 data_lane_cnt;
	struct phy *phy;
	struct phy *phy_ex;
	spinlock_t slock;

	struct exynos_panel_device *panel;

	struct v4l2_subdev sd;
	struct dsim_clks clks;
	struct timer_list cmd_timer;
#if IS_ENABLED(CONFIG_MCD_PANEL)
	struct workqueue_struct *wq;
	struct work_struct wr_timeout_work;
	struct mcd_dsim_device mcd_dsim;
#endif

	struct mutex cmd_lock;
	struct mutex rd_cmd_lock;

	struct completion ph_wr_comp;
	struct completion rd_comp;

#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
	struct fcmd_device *fcmd;
	struct completion fcmd_wr_comp;
	struct timer_list fcmd_timer;
	struct dsim_dma_buf_data fcmd_buf_data;
	struct dma_buf *fcmd_buf;
	void *fcmd_buf_vaddr;
	bool fcmd_buf_allocated;
#endif

	int total_underrun_cnt;
	int idle_ip_index;

	int pl_cnt;
	int line_cnt;

	struct dsim_fb_handover fb_handover;
#if defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
	int esd_test;
	bool esd_recovering;
#endif
};

int dsim_call_panel_ops(struct dsim_device *dsim, u32 cmd, void *arg);
int dsim_write_data(struct dsim_device *dsim, u32 id, unsigned long d0, u32 d1, bool wait_empty);
int dsim_read_data(struct dsim_device *dsim, u32 id, u32 addr, u32 cnt, u8 *buf);
int dsim_wait_for_cmd_done(struct dsim_device *dsim);

int dsim_reset_panel(struct dsim_device *dsim);
int dsim_set_panel_power(struct dsim_device *dsim, bool on);

void dsim_to_regs_param(struct dsim_device *dsim, struct dsim_regs *regs);

void dsim_reg_recovery_process(struct dsim_device *dsim);

#if IS_ENABLED(CONFIG_MCD_PANEL)
int dsim_write_cmd_set(struct dsim_device *dsim, struct exynos_dsim_cmd cmd_list[],
		int cmd_cnt, bool wait_vsync);

int dsim_sr_write_data(struct dsim_device *dsim, const u8 *cmd, u32 size, u32 align);
#endif

static inline struct dsim_device *get_dsim_drvdata(u32 id)
{
	return dsim_drvdata[id];
}

static inline int dsim_rd_data(u32 id, u32 cmd_id, u32 addr, u32 size, u8 *buf)
{
	int ret;
	struct dsim_device *dsim = get_dsim_drvdata(id);

	ret = dsim_read_data(dsim, cmd_id, addr, size, buf);
	if (ret)
		return ret;

	return 0;
}

static inline int dsim_wr_data(struct dsim_device *dsim, u32 type, u8 data[],
		u32 len, bool wait_empty)
{
	u32 t;
	int ret = 0;

	switch (len) {
	case 0:
		return -EINVAL;
	case 1:
		t = type ? type : MIPI_DSI_DCS_SHORT_WRITE;
		ret = dsim_write_data(dsim, t, (unsigned long)data[0], 0, wait_empty);
		break;
	case 2:
		t = type ? type : MIPI_DSI_DCS_SHORT_WRITE_PARAM;
		ret = dsim_write_data(dsim, t, (unsigned long)data[0], (u32)data[1], wait_empty);
		break;
	default:
		t = type ? type : MIPI_DSI_DCS_LONG_WRITE;
		ret = dsim_write_data(dsim, t, (unsigned long)data, len, wait_empty);
		break;
	}

	return ret;
}

#define dsim_write_data_seq(dsim, wait_empty, seq...) do {		\
	u8 d[] = { seq };						\
	int ret;							\
	ret = dsim_wr_data(dsim, 0, d, ARRAY_SIZE(d), wait_empty);	\
	if (ret < 0)							\
		dsim_err("failed to write cmd(%d)\n", ret);		\
} while (0)

#define dsim_write_data_seq_delay(dsim, delay, seq...) do {	\
	dsim_write_data_seq(dsim, true, seq);			\
	msleep(delay);						\
} while (0)

#define dsim_write_data_table(dsim, table) do {				\
	int ret;							\
	ret = dsim_wr_data(dsim, 0, table, ARRAY_SIZE(table), false);	\
	if (ret < 0)							\
		dsim_err("failed to write cmd(%d)\n", ret);		\
} while (0)

#define dsim_write_data_type_seq(dsim, type, seq...) do {	\
	u8 d[] = { seq };					\
	int ret;						\
	ret = dsim_wr_data(dsim, type, d, ARRAY_SIZE(d), false);\
	if (ret < 0)						\
		dsim_err("failed to write cmd(%d)\n", ret);	\
} while (0)

#define dsim_write_data_type_table(dsim, type, table) do {		\
	int ret;							\
	ret = dsim_wr_data(dsim, type, table, ARRAY_SIZE(table), false);\
	if (ret < 0)							\
		dsim_err("failed to write cmd(%d)\n", ret);		\
} while (0)

static inline int dsim_wait_for_cmd_completion(u32 id)
{
	int ret;
	struct dsim_device *dsim = get_dsim_drvdata(id);

	ret = dsim_wait_for_cmd_done(dsim);

	return ret;
}

/* register access subroutines */
static inline u32 dsim_read(u32 id, u32 reg_id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	return readl(dsim->res.regs + reg_id);
}

static inline u32 dsim_read_mask(u32 id, u32 reg_id, u32 mask)
{
	u32 val = dsim_read(id, reg_id);
	val &= (mask);
	return val;
}

static inline void dsim_write(u32 id, u32 reg_id, u32 val)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	writel(val, dsim->res.regs + reg_id);
}

static inline void dsim_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	u32 old = dsim_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, dsim->res.regs + reg_id);
}

/* DPHY register access subroutines */
static inline u32 dsim_phy_read(u32 id, u32 reg_id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);

	return readl(dsim->res.phy_regs + reg_id);
}

static inline u32 dsim_phy_read_mask(u32 id, u32 reg_id, u32 mask)
{
	u32 val = dsim_phy_read(id, reg_id);

	val &= (mask);
	return val;
}
static inline void dsim_phy_extra_write(u32 id, u32 reg_id, u32 val)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);

	writel(val, dsim->res.phy_regs_ex + reg_id);
}
static inline void dsim_phy_write(u32 id, u32 reg_id, u32 val)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);

	writel(val, dsim->res.phy_regs + reg_id);
}

static inline void dsim_phy_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	u32 old = dsim_phy_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, dsim->res.phy_regs + reg_id);
	/* printk("offset : 0x%8x, value : 0x%x\n", reg_id, val); */
}

/* DPHY loop back for test */
#ifdef DPHY_LOOP
void dsim_reg_set_dphy_loop_back_test(u32 id);
#endif

static inline bool IS_DSIM_ON_STATE(struct dsim_device *dsim)
{
#ifdef CONFIG_EXYNOS_DOZE
	return (dsim->state == DSIM_STATE_ON ||
			dsim->state == DSIM_STATE_DOZE);
#else
	return (dsim->state == DSIM_STATE_ON);
#endif
}

static inline bool IS_DSIM_OFF_STATE(struct dsim_device *dsim)
{
	return (dsim->state == DSIM_STATE_ULPS ||
#ifdef CONFIG_EXYNOS_DOZE
			dsim->state == DSIM_STATE_DOZE_SUSPEND ||
#endif
			dsim->state == DSIM_STATE_OFF);
}

#define DSIM_IOC_ENTER_ULPS		_IOW('D', 0, u32)
#define DSIM_IOC_GET_LCD_INFO		_IOW('D', 5, struct exynos_panel_info *)
#define DSIM_IOC_DUMP			_IOW('D', 8, u32)
#define DSIM_IOC_GET_WCLK		_IOW('D', 9, u32)
#define DSIM_IOC_SET_CONFIG		_IOW('D', 10, u32)
#define DSIM_IOC_FREE_FB_RES		_IOW('D', 11, u32)
#define DSIM_IOC_DOZE			_IOW('D', 20, u32)
#define DSIM_IOC_DOZE_SUSPEND		_IOW('D', 21, u32)
#define DSIM_IOC_SET_FREQ_HOP		_IOW('D', 30, u32)
#define DSIM_IOC_RECOVERY_PROC		_IOW('D', 40, u32)

#if defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
#define DSIM_ESD_OK			0
#define DSIM_ESD_ERROR			1
#define DSIM_ESD_CHECK_ERROR		2
#endif

#if IS_ENABLED(CONFIG_MCD_PANEL)
#define DSIM_IOC_NOTIFY         _IOW('D', 50, u32)
#define DSIM_IOC_SET_ERROR_CB   _IOW('D', 51, struct disp_error_cb_info *)
#define DSIM_IOC_PANEL_DUMP			_IOW('D', 52, u32)
#ifdef CONFIG_DYNAMIC_MIPI
#define DSIM_IOC_MCD_DM_PRE_CHANGE_FREQ  _IOW('D', 60, u32)
#define DSIM_IOC_MCD_DM_POST_CHANGE_FREQ  _IOW('D', 61, u32)
#endif
#endif
#endif /* __SAMSUNG_DSIM_H__ */
