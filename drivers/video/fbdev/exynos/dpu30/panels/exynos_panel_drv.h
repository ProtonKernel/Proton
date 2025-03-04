/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Samsung EXYNOS Panel Driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_PANEL_DRV_H__
#define __EXYNOS_PANEL_DRV_H__

#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/backlight.h>
#include <linux/regulator/consumer.h>
#include <video/mipi_display.h>
#include <media/v4l2-subdev.h>
#include "exynos_panel.h"
#include "../dsim.h"
#include "../decon.h"

#if IS_ENABLED(CONFIG_MCD_PANEL)
#include "../disp_err.h"
#include "panel_drv.h"
#endif

extern int dpu_panel_log_level;

#define DPU_DEBUG_PANEL(fmt, args...)					\
	do {								\
		if (dpu_panel_log_level >= 7)				\
			dpu_debug_printk("PANEL", fmt,  ##args);	\
	} while (0)

#define DPU_INFO_PANEL(fmt, args...)					\
	do {								\
		if (dpu_panel_log_level >= 6)				\
			dpu_debug_printk("PANEL", fmt,  ##args);	\
	} while (0)

#define DPU_ERR_PANEL(fmt, args...)					\
	do {								\
		if (dpu_panel_log_level >= 3)				\
			dpu_debug_printk("PANEL", fmt, ##args);		\
	} while (0)

#define MAX_REGULATORS		3
#define MAX_PANEL_SUPPORT	10
#define MAX_PANEL_ID_NUM	4

/* for dual display */
#define MAX_PANEL_DRV_SUPPORT	3

#define DEFAULT_MAX_BRIGHTNESS	255
#define DEFAULT_BRIGHTNESS	127

extern struct exynos_panel_device *panel_drvdata[MAX_PANEL_DRV_SUPPORT];
#if IS_ENABLED(CONFIG_MCD_PANEL)
extern struct exynos_panel_ops common_panel_ops;
#else
extern struct exynos_panel_ops panel_emul_ops;
extern struct exynos_panel_ops panel_s6e3hab_ops;
extern struct exynos_panel_ops panel_s6e3ha9_ops;
extern struct exynos_panel_ops panel_s6e3ha8_ops;
extern struct exynos_panel_ops panel_s6e3fa0_ops;
extern struct exynos_panel_ops panel_s6e3fab_ops;
extern struct exynos_panel_ops panel_s6e3had_ops;
#endif

struct exynos_panel_resources {
	int lcd_reset;
	int lcd_power[2];
	struct regulator *regulator[MAX_REGULATORS];
};

struct exynos_panel_ops {
	u32 id[MAX_PANEL_ID_NUM];
	int (*suspend)(struct exynos_panel_device *panel);
	int (*displayon)(struct exynos_panel_device *panel);
	int (*mres)(struct exynos_panel_device *panel, u32 mode_idx);
	int (*doze)(struct exynos_panel_device *panel);
	int (*doze_suspend)(struct exynos_panel_device *panel);
	int (*dump)(struct exynos_panel_device *panel);
	int (*read_state)(struct exynos_panel_device *panel);
	int (*set_cabc_mode)(struct exynos_panel_device *panel, int mode);
	int (*set_light)(struct exynos_panel_device *panel, u32 br_val);
#if !IS_ENABLED(CONFIG_MCD_PANEL)
	int (*set_vrefresh)(struct exynos_panel_device *panel, u32 refresh);
#else
	int (*set_vrefresh)(struct exynos_panel_device *panel, struct vrr_config_data *vrr_info);
#endif

#if IS_ENABLED(CONFIG_MCD_PANEL)
	int (*probe)(struct exynos_panel_device *panel);
	int (*resume)(struct exynos_panel_device *panel);
	int (*init)(struct exynos_panel_device *panel);
	int (*connected)(struct exynos_panel_device *panel);
	int (*is_poweron)(struct exynos_panel_device *panel);
	int (*setarea)(struct exynos_panel_device *panel, u32 l, u32 r, u32 t, u32 b);
	int (*poweron)(struct exynos_panel_device *panel);
	int (*poweroff)(struct exynos_panel_device *panel);
	int (*sleepin)(struct exynos_panel_device *panel);
	int (*sleepout)(struct exynos_panel_device *panel);
	int (*notify)(struct exynos_panel_device *panel, void *data);
	int (*set_error_cb)(struct exynos_panel_device *panel, void *data);
	int (*reset_panel)(struct exynos_panel_device *panel);
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	int (*get_display_mode)(struct exynos_panel_device *panel, void *data);
	int (*set_display_mode)(struct exynos_panel_device *panel, void *data);
#endif
#endif
};

/*
 * cabc_mode[1:0] must be re-mapped according to DDI command
 * 3FA0 is OLED, so CABC command is not supported.
 * Following values represent for ACL2 control of 3FA0.
 * - [2'b00] ACL off
 * - [2'b01] ACL low
 * - [2'b10] ACL mid
 * - [2'b11] ACL high
 */
enum cabc_mode {
	CABC_OFF = 0,
	CABC_USER_IMAGE,
	CABC_STILL_PICTURE,
	CABC_MOVING_IMAGE,
};

enum power_mode {
	POWER_SAVE_OFF = 0,
	POWER_SAVE_LOW = 1,
	POWER_SAVE_MEDIUM = 2,
	POWER_SAVE_HIGH = 3,
	POWER_SAVE_MAX = 4,
	POWER_MODE_READ = 0x80,
};

struct exynos_panel_device {
	u32 id;		/* panel device id */
	u32 id_index;
	bool found;	/* found connected panel or not */
	struct device *dev;
	struct v4l2_subdev sd;
#if IS_ENABLED(CONFIG_MCD_PANEL)
	struct v4l2_subdev *panel_drv_sd;
	struct disp_error_cb_info error_cb_info;
	struct disp_check_cb_info check_cb_info;
#endif
	struct mutex ops_lock;
	struct exynos_panel_resources res;
	struct backlight_device *bl;
	struct exynos_panel_info lcd_info;
	struct exynos_panel_ops *ops;
	bool cabc_enabled;
	enum power_mode power_mode;
};

static inline struct exynos_panel_device *get_panel_drvdata(u32 panel_idx)
{
	return panel_drvdata[panel_idx];
}

int exynos_panel_calc_slice_width(u32 dsc_cnt, u32 slice_num, u32 xres);

#define call_panel_ops(q, op, args...)				\
	(((q)->ops->op) ? ((q)->ops->op(args)) : 0)

#define EXYNOS_PANEL_IOC_REGISTER	_IOW('P', 0, u32)
#define EXYNOS_PANEL_IOC_POWERON	_IOW('P', 1, u32)
#define EXYNOS_PANEL_IOC_POWEROFF	_IOW('P', 2, u32)
#define EXYNOS_PANEL_IOC_RESET		_IOW('P', 3, u32)
#define EXYNOS_PANEL_IOC_DISPLAYON	_IOW('P', 4, u32)
#define EXYNOS_PANEL_IOC_SUSPEND	_IOW('P', 5, u32)
#define EXYNOS_PANEL_IOC_MRES		_IOW('P', 6, u32)
#define EXYNOS_PANEL_IOC_DOZE		_IOW('P', 7, u32)
#define EXYNOS_PANEL_IOC_DOZE_SUSPEND	_IOW('P', 8, u32)
#define EXYNOS_PANEL_IOC_DUMP		_IOW('P', 9, u32)
#define EXYNOS_PANEL_IOC_READ_STATE	_IOR('P', 10, u32)
#define EXYNOS_PANEL_IOC_SET_LIGHT	_IOW('P', 11, u32)
#if !IS_ENABLED(CONFIG_MCD_PANEL)
#define EXYNOS_PANEL_IOC_SET_VREFRESH	_IOW('P', 12, u32)
#else
#define EXYNOS_PANEL_IOC_SET_VREFRESH	_IOW('P', 12, struct vrr_config_data *)
#endif

#if IS_ENABLED(CONFIG_MCD_PANEL)
#define EXYNOS_PANEL_IOC_INIT	_IOW('P', 21, u32)
#define EXYNOS_PANEL_IOC_CONNECTED	_IOR('P', 22, u32)
#define EXYNOS_PANEL_IOC_IS_POWERON	_IOR('P', 23, u32)
#define EXYNOS_PANEL_IOC_SETAREA	_IOW('P', 24, struct decon_rect *)
#define EXYNOS_PANEL_IOC_SLEEPIN	_IOW('P', 25, u32)
#define EXYNOS_PANEL_IOC_SLEEPOUT	_IOW('P', 26, u32)
#define EXYNOS_PANEL_IOC_NOTIFY	_IOW('P', 27, u32)
#define EXYNOS_PANEL_IOC_SET_ERROR_CB	_IOW('P', 28, u32)
#define EXYNOS_PANEL_IOC_PROBE	_IOW('P', 29, u32)
#if defined(CONFIG_PANEL_DISPLAY_MODE)
#define EXYNOS_PANEL_IOC_GET_DISPLAY_MODE	_IOR('P', 30, void *)
#define EXYNOS_PANEL_IOC_SET_DISPLAY_MODE	_IOW('P', 31, void *)
#endif
#endif

#endif /* __EXYNOS_PANEL_DRV_H__ */
