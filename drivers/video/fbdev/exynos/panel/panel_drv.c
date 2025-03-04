// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Author: Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_reserved_mem.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/ctype.h>
#include <video/mipi_display.h>
#include <linux/regulator/consumer.h>
#include "../../../../../kernel/irq/internals.h"
#ifdef CONFIG_EXYNOS_DPU30_DUAL
#include "../dpu30/panel_modes.h"
#include "../dpu30_dual/decon.h"
#else
#include "../dpu30/decon.h"
#endif
#include "kunit_test.h"
#include "panel.h"
#include "panel_bl.h"
#include "panel_vrr.h"
#include "panel_drv.h"
#include "panel_debug.h"

#include "dpui.h"

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
#include "mdnie.h"
#endif

#ifdef CONFIG_EXYNOS_DECON_LCD_SPI
#include "spi.h"
#endif

#ifdef CONFIG_SUPPORT_DDI_FLASH
#include "panel_poc.h"
#endif

#ifdef CONFIG_EXTEND_LIVE_CLOCK
#include "./aod/aod_drv.h"
#endif

#ifdef CONFIG_SUPPORT_MAFPC
#include "./mafpc/mafpc_drv.h"
#endif

#ifdef CONFIG_SUPPORT_POC_SPI
#include "panel_spi.h"
#endif

#if defined(CONFIG_TDMB_NOTIFIER)
#include <linux/tdmb_notifier.h>
#endif

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

//#include <linux/input/sec_cmd.h>

#ifdef CONFIG_DYNAMIC_MIPI
#include "./dynamic_mipi/dynamic_mipi.h"
#endif

static struct class *lcd_class;

static char *panel_state_names[] = {
	"OFF",		/* POWER OFF */
	"ON",		/* POWER ON */
	"NORMAL",	/* SLEEP OUT */
	"LPM",		/* LPM */
};

/* panel workqueue */
static char *panel_work_names[] = {
	[PANEL_WORK_DISP_DET] = "disp-det",
	[PANEL_WORK_PCD] = "pcd",
	[PANEL_WORK_ERR_FG] = "err-fg",
	[PANEL_WORK_CONN_DET] = "conn-det",
#ifdef CONFIG_SUPPORT_DIM_FLASH
	[PANEL_WORK_DIM_FLASH] = "dim-flash",
#endif
	[PANEL_WORK_CHECK_CONDITION] = "panel-condition-check",
	[PANEL_WORK_UPDATE] = "panel-update",
#ifdef CONFIG_EVASION_DISP_DET
	[PANEL_WORK_EVASION_DISP_DET] = "evasion-disp-det",
#endif
};

static void disp_det_handler(struct work_struct *data);
static void conn_det_handler(struct work_struct *data);
static void err_fg_handler(struct work_struct *data);
static void panel_condition_handler(struct work_struct *work);
#ifdef CONFIG_SUPPORT_DIM_FLASH
static void dim_flash_handler(struct work_struct *work);
#endif
static void panel_update_handler(struct work_struct *work);
#ifdef CONFIG_EVASION_DISP_DET
static void evasion_disp_det_handler(struct work_struct *work);
#endif

static panel_wq_handler panel_wq_handlers[] = {
	[PANEL_WORK_DISP_DET] = disp_det_handler,
	[PANEL_WORK_PCD] = NULL,
	[PANEL_WORK_ERR_FG] = err_fg_handler,
	[PANEL_WORK_CONN_DET] = conn_det_handler,
#ifdef CONFIG_SUPPORT_DIM_FLASH
	[PANEL_WORK_DIM_FLASH] = dim_flash_handler,
#endif
	[PANEL_WORK_CHECK_CONDITION] = panel_condition_handler,
	[PANEL_WORK_UPDATE] = panel_update_handler,
#ifdef CONFIG_EVASION_DISP_DET
	[PANEL_WORK_EVASION_DISP_DET] = evasion_disp_det_handler,
#endif
};

static char *panel_thread_names[PANEL_THREAD_MAX] = {
#ifdef CONFIG_PANEL_VRR_BRIDGE
	[PANEL_THREAD_VRR_BRIDGE] = "panel-vrr-bridge",
#endif
};

static panel_thread_fn panel_thread_fns[PANEL_THREAD_MAX] = {
#ifdef CONFIG_PANEL_VRR_BRIDGE
	[PANEL_THREAD_VRR_BRIDGE] = panel_vrr_bridge_thread,
#endif
};

/* panel gpio */
static char *panel_gpio_names[PANEL_GPIO_MAX] = {
	[PANEL_GPIO_RESET] = PANEL_GPIO_NAME_RESET,
	[PANEL_GPIO_DISP_DET] = PANEL_GPIO_NAME_DISP_DET,
	[PANEL_GPIO_PCD] = PANEL_GPIO_NAME_PCD,
	[PANEL_GPIO_ERR_FG] = PANEL_GPIO_NAME_ERR_FG,
	[PANEL_GPIO_CONN_DET] = PANEL_GPIO_NAME_CONN_DET,
};

/* panel regulator */
static char *panel_regulator_names[PANEL_REGULATOR_MAX] = {
	[PANEL_REGULATOR_DDI_VCI] = PANEL_REGULATOR_NAME_DDI_VCI,
	[PANEL_REGULATOR_DDI_VDD3] = PANEL_REGULATOR_NAME_DDI_VDD3,
	[PANEL_REGULATOR_DDR_VDDR] = PANEL_REGULATOR_NAME_DDR_VDDR,
	[PANEL_REGULATOR_SSD] = PANEL_REGULATOR_NAME_SSD,
#ifdef CONFIG_EXYNOS_DPU30_DUAL
	[PANEL_SUB_REGULATOR_DDI_VCI] = PANEL_SUB_REGULATOR_NAME_DDI_VCI,
	[PANEL_SUB_REGULATOR_DDI_VDD3] = PANEL_SUB_REGULATOR_NAME_DDI_VDD3,
	[PANEL_SUB_REGULATOR_DDR_VDDR] = PANEL_SUB_REGULATOR_NAME_DDR_VDDR,
	[PANEL_SUB_REGULATOR_SSD] = PANEL_SUB_REGULATOR_NAME_SSD,
#endif
};

static int boot_panel_id;
int panel_log_level = 6;
EXPORT_SYMBOL(panel_log_level);
int panel_cmd_log;
EXPORT_SYMBOL(panel_cmd_log);
#ifdef CONFIG_SUPPORT_PANEL_SWAP
int panel_reprobe(struct panel_device *panel);
#endif
int __mockable panel_parse_lcd_info(struct panel_device *panel);

inline char *get_panel_state_names(enum panel_active_state idx)
{
	if (idx < MAX_PANEL_STATE)
		return panel_state_names[idx];
	return NULL;
}
#if 0
static int __init get_boot_panel_id(char *arg)
{
	get_option(&arg, &boot_panel_id);
	panel_info("boot_panel_id : 0x%x\n", boot_panel_id);

	return 0;
}
early_param("lcdtype", get_boot_panel_id);
#else
module_param(boot_panel_id, int, S_IRUGO);
#endif

/**
 * get_lcd info - get lcd global information.
 * @arg: key string of lcd information
 *
 * if get lcd info successfully, return 0 or positive value.
 * if not, return -EINVAL.
 */
int get_lcd_info(char *arg)
{
	if (!arg) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}

	if (!strncmp(arg, "connected", 9))
		return (boot_panel_id < 0) ? 0 : 1;
	else if (!strncmp(arg, "id", 2))
		return (boot_panel_id < 0) ? 0 : boot_panel_id;
	else if (!strncmp(arg, "window_color", 12))
		return (boot_panel_id < 0) ? 0 : ((boot_panel_id & 0x0F0000) >> 16);
	else
		return -EINVAL;
}
EXPORT_SYMBOL(get_lcd_info);

bool __mockable panel_gpio_valid(struct panel_gpio *gpio)
{
	if ((!gpio) || (gpio->num < 0))
		return false;
	if (!gpio->name || !gpio_is_valid(gpio->num)) {
		panel_err("invalid gpio(%d)\n", gpio->num);
		return false;
	}
	return true;
}

int __mockable panel_gpio_state(struct panel_gpio *gpio)
{
	if (!panel_gpio_valid(gpio))
		return -EINVAL;
	if (gpio->active_low)
		return gpio_get_value(gpio->num) ?
			PANEL_STATE_OK : PANEL_STATE_NOK;
	else
		return gpio_get_value(gpio->num) ?
			PANEL_STATE_NOK : PANEL_STATE_OK;
}

static int panel_disp_det_state(struct panel_device *panel)
{
	int state;

	state = panel_gpio_state(&panel->gpio[PANEL_GPIO_DISP_DET]);
	if (state >= 0)
		panel_dbg("disp_det:%s\n", state ? "EL-OFF" : "EL-ON");

	return state;
}

#ifdef CONFIG_SUPPORT_ERRFG_RECOVERY
static int panel_err_fg_state(struct panel_device *panel)
{
	int state;

	state = panel_gpio_state(&panel->gpio[PANEL_GPIO_ERR_FG]);
	if (state >= 0)
		panel_info("err_fg:%s\n", state ? "ERR_FG OK" : "ERR_FG NOK");

	return state;
}
#endif

int __mockable panel_conn_det_state(struct panel_device *panel)
{
	int state;

	state = panel_gpio_state(&panel->gpio[PANEL_GPIO_CONN_DET]);
	if (state >= 0)
		panel_dbg("conn_det:%s\n", state ? "connected" : "disconnected");

	return state;
}

static void panel_set_cur_state(struct panel_device *panel, enum panel_active_state state)
{
#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
	struct profiler_log pp;

	memset(&pp, 0, sizeof(struct profiler_log));
	if (panel->profiler.initialized) {
		ktime_get_ts64(&pp.time);
		PLOG_SET_TYPE(pp.header, PLOG_PANEL);
		PLOG_SET_SUBTYPE(pp.header, PLOG_PANEL_STATE);
		pp.state.prev = panel->state.cur_state;
		pp.state.prev_name = panel_state_names[panel->state.cur_state];
		pp.state.cur = state;
		pp.state.cur_name = panel_state_names[state];
		v4l2_subdev_call(&panel->profiler.sd, core, ioctl, PROFILE_LOG, &pp);
	}
#endif
	panel->state.cur_state = state;

}
bool ub_con_disconnected(struct panel_device *panel)
{
	int state;

	state = panel_conn_det_state(panel);
	if (state < 0)
		return false;

	return !state;
}

void clear_pending_bit(int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (!desc) {
		panel_err("desc is null\n");
		return;
	}

	if (desc->irq_data.chip->irq_ack) {
		desc->irq_data.chip->irq_ack(&desc->irq_data);
		desc->istate &= ~IRQS_PENDING;
	}
};

int panel_get_gpio_irq(struct panel_gpio *gpio)
{
	if (!panel_gpio_valid(gpio))
		return -EINVAL;
	return gpio->irq_enable;
}

int __mockable panel_set_gpio_irq(struct panel_gpio *gpio, bool enable)
{
#ifdef CONFIG_EVASION_DISP_DET
	struct panel_device *panel;
#endif

	if (!panel_gpio_valid(gpio))
		return -EINVAL;

	if (enable == gpio->irq_enable) {
		panel_info("%s(%d) irq is already %s(%d), so skip!\n",
			gpio->name, gpio->num, gpio->irq_enable ? "enable" : "disable", gpio->irq_enable);
		return 0;
	}

	if (enable) {
		clear_pending_bit(gpio->irq);
		panel_info("enable_irq %s\n", gpio->name);
		enable_irq(gpio->irq);
	} else {
		panel_info("disable_irq %s\n", gpio->name);
		disable_irq(gpio->irq);
#ifdef CONFIG_EVASION_DISP_DET
		if (!strcmp(gpio->name, PANEL_GPIO_NAME_DISP_DET)) {
			panel = container_of(gpio, struct panel_device, gpio[PANEL_GPIO_DISP_DET]);
			if (panel)
				cancel_delayed_work_sync(&panel->work[PANEL_WORK_EVASION_DISP_DET].dwork);
		}
#endif
	}
	gpio->irq_enable = enable;
	return 0;
}

static int panel_regulator_enable(struct panel_device *panel)
{
	struct panel_regulator *regulator = panel->regulator;
	int i, ret;

	for (i = 0; i < PANEL_REGULATOR_MAX; i++) {
		if (IS_ERR_OR_NULL(regulator[i].reg))
			continue;

		if (regulator[i].type != PANEL_REGULATOR_TYPE_PWR)
			continue;

		ret = regulator_enable(regulator[i].reg);
		if (ret != 0) {
			panel_err("failed to enable regulator(%d:%s), ret:%d\n", i, regulator[i].name, ret);
			return ret;
		}

		panel_info("enable regulator(%d:%s)\n", i, regulator[i].name);
	}

	return 0;
}

static int panel_regulator_disable(struct panel_device *panel)
{
	struct panel_regulator *regulator = panel->regulator;
	int i, ret;

	for (i = PANEL_REGULATOR_MAX - 1; i >= 0; i--) {
		if (IS_ERR_OR_NULL(regulator[i].reg))
			continue;

		if (regulator[i].type != PANEL_REGULATOR_TYPE_PWR)
			continue;

		ret = regulator_disable(regulator[i].reg);
		if (ret != 0) {
			panel_err("failed to disable regulator(%d:%s), ret:%d\n", i, regulator[i].name, ret);
			return ret;
		}
		panel_info("disable regulator(%d:%s)\n", i, regulator[i].name);
	}

	return 0;
}

static int panel_regulator_set_voltage(struct panel_device *panel, int state)
{
	struct panel_regulator *regulator = panel->regulator;
	int i, old_uv, new_uv;
	int ret = 0;

	for (i = 0; i < PANEL_REGULATOR_MAX; i++) {
		if (IS_ERR_OR_NULL(regulator[i].reg))
			continue;

		if (regulator[i].type != PANEL_REGULATOR_TYPE_PWR)
			continue;

		new_uv = (state == PANEL_STATE_ALPM) ?
			regulator[i].lpm_voltage : regulator[i].def_voltage;
		if (new_uv == 0)
			continue;

		old_uv = regulator_get_voltage(regulator[i].reg);
		if (old_uv < 0) {
			panel_err("failed to get regulator(%d:%s) voltage, ret:%d\n",
					i, regulator[i].name, old_uv);
			ret = -EINVAL;
			return ret;
		}

		if (new_uv == old_uv)
			continue;

		ret = regulator_set_voltage(regulator[i].reg, new_uv, new_uv);
		if (ret < 0) {
			panel_err("failed to set regulator(%d:%s) target voltage(%d), ret:%d\n",
					i, regulator[i].name, new_uv, ret);
			ret = -EINVAL;
			return ret;
		}

		panel_info("voltage:%duV\n", new_uv);
	}

	return ret;
}

#ifdef CONFIG_PANEL_NOTIFY
static inline void panel_send_ubconn_notify(u32 state)
{
	struct panel_ub_con_event_data data;

	data.state = state;
	panel_notifier_call_chain(PANEL_EVENT_UB_CON_CHANGED, &data);
	panel_info("call EVENT_UB_CON notifier %d\n", data.state);
}
#endif

#ifdef CONFIG_SUPPORT_MAFPC
static int cmd_v4l2_mafpc_dev(struct panel_device *panel, int cmd, void *param)
{
	int ret = 0;

	if (panel->mafpc_sd) {
		ret = v4l2_subdev_call(panel->mafpc_sd, core, ioctl, cmd, param);
		if (ret)
			panel_err("failed to v4l2 subdev call\n");
	}

	return ret;
}

static int __abc_match_dev(struct device *dev, const void *data)
{
	struct mafpc_device *mafpc;
	struct panel_device *panel = (struct panel_device *)data;

	mafpc = (struct mafpc_device *)dev_get_drvdata(dev);
	if (!mafpc) {
		panel_err("MCD:ABC: failed to get abc\n");
		return 0;
	}
	panel->mafpc_sd = &mafpc->sd;
	mafpc->panel = panel;

	return 1;
}

int panel_get_v4l2_abc_dev(struct panel_device *panel, struct mafpc_info *info)
{
	int ret;
	struct device *dev;
	struct device_driver *drv;

	if ((!panel) || (!info)) {
		panel_err("MCD:ABC: null\n");
		return -EINVAL;
	}

	drv = driver_find(MAFPC_DEV_NAME, &platform_bus_type);
	if (IS_ERR_OR_NULL(drv)) {
		panel_err("failed to find driver\n");
		return -ENODEV;
	}

	dev = driver_find_device(drv, NULL, panel, __abc_match_dev);
	if (IS_ERR_OR_NULL(dev)) {
		panel_err("failed to find device\n");
		return -ENODEV;
	}

	ret = cmd_v4l2_mafpc_dev(panel, V4L2_IOCTL_PROBE_ABC, (void *)info);
	if (unlikely(ret < 0)) {
		panel_err("failed to v4l2 mafpc probe\n");
		return -ENODEV;
	}

	return 0;
}
#endif


#ifdef CONFIG_DISP_PMIC_SSD
static int panel_regulator_set_short_detection(struct panel_device *panel, int state)
{
	struct panel_regulator *regulator = panel->regulator;
	int i, ret, new_ua = 0;

	for (i = 0; i < PANEL_REGULATOR_MAX; i++) {
		if (regulator[i].type != PANEL_REGULATOR_TYPE_SSD)
			continue;

		new_ua = (state == PANEL_STATE_ALPM) ?
			regulator[i].from_lpm_current : regulator[i].from_off_current;

		ret = regulator_set_current_limit(regulator[i].reg, new_ua, new_ua);
		if (ret < 0) {
			panel_err("failed to set short detection regulator(%d:%s), ret:%d state:%d\n",
					i, regulator[i].name, new_ua, state);
			return ret;
		}
		ret = regulator_get_current_limit(regulator[i].reg);
		panel_info("set regulator(%s) SSD:%duA, state:%d read:%d\n", regulator[i].name, new_ua, state, ret);

	}

	return 0;
}
#else
static inline int panel_regulator_set_short_detection(struct panel_device *panel, int state)
{
	return 0;
}
#endif

int __mockable __set_panel_power(struct panel_device *panel, int power)
{
	int ret = 0;
	struct panel_gpio *gpio = panel->gpio;

	if (panel->state.power == power) {
		panel_warn("same status.. skip..\n");
		return 0;
	}

	if (power == PANEL_POWER_ON) {
		ret = panel_regulator_set_short_detection(panel, PANEL_STATE_ON);
		if (ret < 0)
			panel_err("failed to set ssd current, ret:%d\n", ret);
		ret = panel_regulator_enable(panel);
		if (ret < 0) {
			panel_err("failed to panel_regulator_enable, ret:%d\n", ret);
			return ret;
		}
		if (panel->panel_data.ddi_props.after_panel_reset == false) {
			usleep_range(15000, 15000 + 10);
			gpio_direction_output(gpio[PANEL_GPIO_RESET].num, 1);
			usleep_range(10000, 10000 + 10);
		}
	} else {
		gpio_direction_output(gpio[PANEL_GPIO_RESET].num, 0);
		// add delay for mangna spec, it will change to decon_board
		usleep_range(11000, 12000);
		ret = panel_regulator_disable(panel);
		if (ret < 0) {
			panel_err("failed to panel_regulator_disable, ret:%d\n", ret);
			return ret;
		}
	}
	panel_info("power(%s) gpio_reset(%s)\n",
			power == PANEL_POWER_ON ? "on" : "off",
			gpio_get_value(gpio[PANEL_GPIO_RESET].num) ? "high" : "low");
	panel->state.power = power;

	return 0;
}

static int __panel_seq_display_on(struct panel_device *panel)
{
	int ret;

	panel_info("PANEL_DISPLAY_ON_SEQ\n");

	ret = panel_do_seqtbl_by_index(panel, PANEL_DISPLAY_ON_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to seqtbl(PANEL_DISPLAY_ON_SEQ)\n");
		return ret;
	}

	return 0;
}

static int __panel_seq_display_off(struct panel_device *panel)
{
	int ret;

	ret = panel_do_seqtbl_by_index(panel, PANEL_DISPLAY_OFF_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to seqtbl(PANEL_DISPLAY_OFF_SEQ)\n");
		return ret;
	}

	return 0;
}

static int __panel_seq_id_read(struct panel_device *panel)
{
	int ret;

	ret = panel_do_seqtbl_by_index(panel, PANEL_ID_READ_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to seqtbl(PANEL_ID_READ_SEQ)\n");
		return ret;
	}

	return 0;
}

static int __panel_seq_res_init(struct panel_device *panel)
{
	int ret;

	ret = panel_do_seqtbl_by_index(panel, PANEL_RES_INIT_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to seqtbl(PANEL_RES_INIT_SEQ)\n");
		return ret;
	}
#ifdef CONFIG_SUPPORT_GM2_FLASH
	ret = panel_do_seqtbl_by_index(panel, PANEL_GM2_FLASH_RES_INIT_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to seqtbl(PANEL_GM2_FLASH_RES_INIT_SEQ)\n");
		return ret;
	}
#endif

	return 0;
}

#ifdef CONFIG_SUPPORT_DIM_FLASH
static int __panel_seq_dim_flash_res_init(struct panel_device *panel)
{
	int ret;

	ret = panel_do_seqtbl_by_index(panel, PANEL_DIM_FLASH_RES_INIT_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to seqtbl(PANEL_DIM_FLASH_RES_INIT_SEQ)\n");
		return ret;
	}

	return 0;
}
#endif

static int __panel_seq_init(struct panel_device *panel)
{
	int ret = 0;
	int retry = 20;
	s64 time_diff;
	ktime_t timestamp = ktime_get();
	struct panel_bl_device *panel_bl = &panel->panel_bl;

#if !defined(CONFIG_UML)
	if (panel_disp_det_state(panel) == PANEL_STATE_OK) {
		panel_warn("panel already initialized\n");
		return 0;
	}
#endif

#ifdef CONFIG_SUPPORT_PANEL_SWAP
	ret = panel_reprobe(panel);
	if (ret < 0) {
		panel_err("failed to panel_reprobe\n");
		return ret;
	}
#endif

	mutex_lock(&panel_bl->lock);
	mutex_lock(&panel->op_lock);
	ret = panel_regulator_set_voltage(panel, PANEL_STATE_NORMAL);
	if (ret < 0)
		panel_err("failed to set voltage\n");

	panel_bl_set_subdev(panel_bl, PANEL_BL_SUBDEV_TYPE_DISP);

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_INIT_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write init seqtbl\n");
		goto err_init_seq;
	}

#ifdef CONFIG_SUPPORT_MAFPC
	cmd_v4l2_mafpc_dev(panel, V4L2_IOCTL_MAFPC_PANEL_INIT, NULL);
#endif

	time_diff = ktime_to_us(ktime_sub(ktime_get(), timestamp));
	panel_info("Time for Panel Init : %llu\n", time_diff);

	mutex_unlock(&panel->op_lock);
	mutex_unlock(&panel_bl->lock);

check_disp_det:
	if (panel_disp_det_state(panel) == PANEL_STATE_NOK) {
		usleep_range(1000, 1000 + 10);
		if (--retry >= 0)
			goto check_disp_det;
		panel_err("check disp det .. not ok\n");
		return -EAGAIN;
	}

	ret = panel_set_gpio_irq(&panel->gpio[PANEL_GPIO_DISP_DET], true);
	if (ret < 0)
		panel_warn("do not support irq\n");

	time_diff = ktime_to_us(ktime_sub(ktime_get(), timestamp));
	panel_info("check disp det .. success %llu\n", time_diff);

#ifdef CONFIG_EXTEND_LIVE_CLOCK
	ret = panel_aod_init_panel(panel, INIT_WITH_LOCK);
	if (ret)
		panel_err("failed to aod init_panel\n");
#endif

	return 0;

err_init_seq:
	mutex_unlock(&panel->op_lock);
	mutex_unlock(&panel_bl->lock);
	return -EAGAIN;
}

static int __panel_seq_exit(struct panel_device *panel)
{
	int ret;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	ret = panel_set_gpio_irq(&panel->gpio[PANEL_GPIO_DISP_DET], false);
	if (ret < 0)
		panel_warn("do not support irq\n");

	mutex_lock(&panel_bl->lock);
	mutex_lock(&panel->op_lock);

	panel_bl_set_subdev(panel_bl, PANEL_BL_SUBDEV_TYPE_DISP);

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_EXIT_SEQ);
	if (unlikely(ret < 0))
		panel_err("failed to write exit seqtbl\n");

#ifdef CONFIG_SUPPORT_MAFPC
	cmd_v4l2_mafpc_dev(panel, V4L2_IOCTL_MAFPC_PANEL_EXIT, NULL);
#endif

	mutex_unlock(&panel->op_lock);
	mutex_unlock(&panel_bl->lock);

	return ret;
}

#ifdef CONFIG_SUPPORT_HMD
static int __panel_seq_hmd_on(struct panel_device *panel)
{
	int ret = 0;
	struct panel_state *state;

	if (!panel) {
		panel_err("pane is null\n");
		return 0;
	}
	state = &panel->state;

	panel_info("hmd was on, setting hmd on seq\n");
	ret = panel_do_seqtbl_by_index(panel, PANEL_HMD_ON_SEQ);
	if (ret)
		panel_err("failed to set hmd on seq\n");

	return ret;
}
#endif

#ifdef CONFIG_EXYNOS_DOZE
static int __panel_seq_exit_alpm(struct panel_device *panel)
{
	int ret = 0;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	panel_info("was called\n");
	ret = panel_regulator_set_short_detection(panel, PANEL_STATE_ALPM);
	if (ret < 0)
		panel_err("failed to set ssd current, ret:%d\n", ret);

#ifdef CONFIG_EXTEND_LIVE_CLOCK
	ret = panel_aod_exit_from_lpm(panel);
	if (ret)
		panel_err("failed to exit_lpm ops\n");
#endif

	mutex_lock(&panel_bl->lock);
	mutex_lock(&panel->op_lock);

#ifdef CONFIG_EVASION_DISP_DET
	ret = panel_set_gpio_irq(&panel->gpio[PANEL_GPIO_DISP_DET], false);
	if (ret < 0)
		panel_warn("do not support irq\n");
#endif
	ret = panel_regulator_set_voltage(panel, PANEL_STATE_NORMAL);
	if (ret < 0)
		panel_err("failed to set voltage\n");

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_ALPM_EXIT_SEQ);
	if (ret)
		panel_err("failed to alpm-exit\n");

	panel->panel_data.props.lpm_brightness = -1;
	panel_bl_set_subdev(panel_bl, PANEL_BL_SUBDEV_TYPE_DISP);

	if (panel->panel_data.props.panel_partial_disp != -1)
		panel->panel_data.props.panel_partial_disp = 0;
	mutex_unlock(&panel->op_lock);
	mutex_unlock(&panel_bl->lock);
	panel_update_brightness(panel);

	msleep(34);

#ifdef CONFIG_EVASION_DISP_DET
	queue_delayed_work(panel->work[PANEL_WORK_EVASION_DISP_DET].wq,
		&panel->work[PANEL_WORK_EVASION_DISP_DET].dwork, msecs_to_jiffies(100));
#endif

	return ret;
}
#ifdef CONFIG_SEC_FACTORY
inline int panel_seq_exit_alpm(struct panel_device *panel)
{
	return __panel_seq_exit_alpm(panel);
}
#endif
/* delay to prevent current leackage when alpm */
/* according to ha6 opmanual, the dealy value is 126msec */
static void __delay_normal_alpm(struct panel_device *panel)
{
	u32 gap;
	u32 delay = 0;
	struct seqinfo *seqtbl;
	struct delayinfo *delaycmd;

	if (!check_seqtbl_exist(&panel->panel_data, PANEL_ALPM_DELAY_SEQ))
		goto exit_delay;

	seqtbl = find_index_seqtbl(&panel->panel_data, PANEL_ALPM_DELAY_SEQ);
	if (unlikely(!seqtbl))
		goto exit_delay;

	delaycmd = (struct delayinfo *)seqtbl->cmdtbl[0];
	if (delaycmd->type != CMD_TYPE_DELAY) {
		panel_err("can't find value\n");
		goto exit_delay;
	}

	if (ktime_after(ktime_get(), panel->ktime_panel_on)) {
		gap = ktime_to_us(ktime_sub(ktime_get(), panel->ktime_panel_on));
		if (gap > delaycmd->usec)
			goto exit_delay;

		delay = delaycmd->usec - gap;
		usleep_range(delay, delay + 10);
	}
	panel_info("total elapsed time : %d\n",
		(int)ktime_to_us(ktime_sub(ktime_get(), panel->ktime_panel_on)));
exit_delay:
	return;
}

static int __panel_seq_set_alpm(struct panel_device *panel)
{
	int ret;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	panel_info("%s was called\n", __func__);
	__delay_normal_alpm(panel);

	mutex_lock(&panel_bl->lock);
	mutex_lock(&panel->op_lock);

#ifdef CONFIG_EVASION_DISP_DET
	ret = panel_set_gpio_irq(&panel->gpio[PANEL_GPIO_DISP_DET], false);
	if (ret < 0)
		panel_warn("do not support irq\n");
#endif

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_ALPM_ENTER_SEQ);
	if (ret)
		panel_err("failed to alpm-enter\n");

#ifdef CONFIG_SUPPORT_AOD_BL
	panel_bl_set_subdev(panel_bl, PANEL_BL_SUBDEV_TYPE_AOD);
#endif

	ret = panel_regulator_set_voltage(panel, PANEL_STATE_ALPM);
	if (ret < 0)
		panel_err("failed to set voltage\n");
	mutex_unlock(&panel->op_lock);
	mutex_unlock(&panel_bl->lock);

#ifdef CONFIG_EXTEND_LIVE_CLOCK
	ret = panel_aod_enter_to_lpm(panel);
	if (ret) {
		panel_err("failed to enter to lpm\n");
		return ret;
	}
#endif

#ifdef CONFIG_EVASION_DISP_DET
	queue_delayed_work(panel->work[PANEL_WORK_EVASION_DISP_DET].wq,
			&panel->work[PANEL_WORK_EVASION_DISP_DET].dwork, msecs_to_jiffies(34*2));
#endif
	return 0;
}
#ifdef CONFIG_SEC_FACTORY
inline int panel_seq_set_alpm(struct panel_device *panel)
{
	return __panel_seq_set_alpm(panel);
}
#endif
#endif

static int __panel_seq_dump(struct panel_device *panel)
{
	int ret;

	ret = panel_do_seqtbl_by_index(panel, PANEL_DUMP_SEQ);
	if (unlikely(ret < 0))
		panel_err("failed to write dump seqtbl\n");

	return ret;
}

static int panel_debug_dump(struct panel_device *panel)
{
	int ret;

	if (unlikely(!panel)) {
		panel_err("panel is null\n");
		goto dump_exit;
	}

	if (!IS_PANEL_ACTIVE(panel)) {
		panel_info("Current state:%d\n", panel->state.cur_state);
		goto dump_exit;
	}

	ret = __panel_seq_dump(panel);
	if (ret) {
		panel_err("failed to dump\n");
		return ret;
	}

	panel_info("disp_det_state:%s\n",
			panel_disp_det_state(panel) == PANEL_STATE_OK ? "OK" : "NOK");
dump_exit:
	return 0;
}

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
int panel_seq_cmdlog_dump(struct panel_device *panel)
{
	int ret;

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_CMDLOG_DUMP_SEQ);
	if (unlikely(ret < 0))
		panel_err("failed to write cmdlog dump seqtbl\n");

	return ret;
}
#endif

int panel_display_on(struct panel_device *panel)
{
	int ret = 0;
	struct panel_state *state = &panel->state;

	if (state->connect_panel == PANEL_DISCONNECT) {
		panel_warn("panel no use\n");
		goto do_exit;
	}

	if (state->cur_state == PANEL_STATE_OFF ||
		state->cur_state == PANEL_STATE_ON) {
		panel_warn("invalid state\n");
		goto do_exit;
	}

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	mdnie_enable(&panel->mdnie);
#endif

#ifdef CONFIG_EXTEND_LIVE_CLOCK
	// Transmit Black Frame
	if (panel->state.cur_state == PANEL_STATE_ALPM) {
		ret = panel_aod_black_grid_on(panel);
		if (ret)
			panel_info("PANEL_ERR:failed to black grid on\n");
	}
#endif

#ifdef CONFIG_SUPPORT_TIG
	if (panel->state.cur_state == PANEL_STATE_ALPM) {
		ret = panel_do_seqtbl_by_index(panel, PANEL_TIG_ENABLE_SEQ);
		if (unlikely(ret < 0)) {
			panel_err("failed to seqtbl(PANEL_TIG_ENABLE_SEQ)\n");
			return ret;
		}
		usleep_range(17000, 17100);
	}
#endif
	ret = __panel_seq_display_on(panel);
	if (ret) {
		panel_err("failed to display on\n");
		return ret;
	}
	state->disp_on = PANEL_DISPLAY_ON;

#ifdef CONFIG_SUPPORT_TIG
	if (panel->state.cur_state == PANEL_STATE_ALPM) {
		usleep_range(33400, 33500);
		ret = panel_do_seqtbl_by_index(panel, PANEL_TIG_DISABLE_SEQ);
		if (unlikely(ret < 0)) {
			panel_err("failed to seqtbl(PANEL_TIG_DISABLE_SEQ)\n");
			return ret;
		}
	}
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	if (panel->state.cur_state == PANEL_STATE_ALPM) {
		usleep_range(33400, 33500);
		ret = panel_aod_black_grid_off(panel);
		if (ret)
			panel_info("PANEL_ERR:failed to black grid on\n");
	}
#endif

#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	copr_enable(&panel->copr);
#endif

	return 0;

do_exit:
	return ret;
}

static int panel_display_off(struct panel_device *panel)
{
	int ret = 0;
	struct panel_state *state = &panel->state;

	if (state->connect_panel == PANEL_DISCONNECT) {
		panel_warn("panel no use\n");
		goto do_exit;
	}

	if (state->cur_state == PANEL_STATE_OFF ||
		state->cur_state == PANEL_STATE_ON) {
		panel_warn("invalid state\n");
		goto do_exit;
	}

	ret = __panel_seq_display_off(panel);
	if (unlikely(ret < 0))
		panel_err("failed to write init seqtbl\n");
	state->disp_on = PANEL_DISPLAY_OFF;

	return 0;
do_exit:
	return ret;
}

static struct common_panel_info *panel_detect(struct panel_device *panel)
{
	u8 id[3];
	u32 panel_id;
	int ret = 0;
	struct common_panel_info *info;
	struct panel_info *panel_data;
	bool detect = true;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return NULL;
	}
	panel_data = &panel->panel_data;

	memset(id, 0, sizeof(id));
	ret = read_panel_id(panel, id);
	if (unlikely(ret < 0)) {
		panel_err("failed to read id(ret %d)\n", ret);
		detect = false;
	}

	panel_id = boot_panel_id;
	panel_info("panel id : %x\n", panel_id);

	info = find_panel(panel, panel_id);
	if (unlikely(!info)) {
		panel_err("panel not found (id 0x%08X)\n", panel_id);
		return NULL;
	}

	return info;
}

static int panel_prepare(struct panel_device *panel, struct common_panel_info *info)
{
	int i;
	struct panel_info *panel_data = &panel->panel_data;

	mutex_lock(&panel->op_lock);
	panel_data->maptbl = info->maptbl;
	panel_data->nr_maptbl = info->nr_maptbl;

	panel_data->seqtbl = info->seqtbl;
	panel_data->nr_seqtbl = info->nr_seqtbl;
	panel_data->rditbl = info->rditbl;
	panel_data->nr_rditbl = info->nr_rditbl;
	panel_data->restbl = info->restbl;
	panel_data->nr_restbl = info->nr_restbl;
	panel_data->dumpinfo = info->dumpinfo;
	panel_data->nr_dumpinfo = info->nr_dumpinfo;
	for (i = 0; i < MAX_PANEL_BL_SUBDEV; i++)
		panel_data->panel_dim_info[i] = info->panel_dim_info[i];
	for (i = 0; i < panel_data->nr_maptbl; i++)
		panel_data->maptbl[i].pdata = panel;
	for (i = 0; i < panel_data->nr_restbl; i++)
		panel_data->restbl[i].state = RES_UNINITIALIZED;
	memcpy(&panel_data->ddi_props, &info->ddi_props, sizeof(panel_data->ddi_props));
	memcpy(&panel_data->ddi_ops, &info->ddi_ops, sizeof(panel_data->ddi_ops));
	memcpy(&panel_data->mres, &info->mres, sizeof(panel_data->mres));
	panel_data->vrrtbl = info->vrrtbl;
	panel_data->nr_vrrtbl = info->nr_vrrtbl;
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	panel_data->common_panel_modes = info->common_panel_modes;
#endif

	mutex_unlock(&panel->op_lock);

	return 0;
}

static int panel_resource_init(struct panel_device *panel)
{
	__panel_seq_res_init(panel);

	return 0;
}

static int panel_id_init(struct panel_device *panel)
{
	struct panel_info *panel_data = &panel->panel_data;
	char buf[32] = { 0, };
	int i, len = 0;

	__panel_seq_id_read(panel);

	resource_copy_by_name(panel_data, panel_data->id, "id");
	for (i = 0; i < PANEL_ID_LEN; i++)
		len += snprintf(buf + len, 32 - len, "%02X", panel_data->id[i]);

	panel_info("id from resource: %s\n", buf);

	return 0;
}

#ifdef CONFIG_SUPPORT_DIM_FLASH
static int panel_dim_flash_resource_init(struct panel_device *panel)
{
	return __panel_seq_dim_flash_res_init(panel);
}
#endif

static int panel_maptbl_init(struct panel_device *panel)
{
	int i;
	struct panel_info *panel_data = &panel->panel_data;

	mutex_lock(&panel->op_lock);
	for (i = 0; i < panel_data->nr_maptbl; i++)
		maptbl_init(&panel_data->maptbl[i]);
	mutex_unlock(&panel->op_lock);

	return 0;
}

int panel_is_changed(struct panel_device *panel)
{
	struct panel_info *panel_data = &panel->panel_data;
	u8 date[7] = { 0, }, coordinate[4] = { 0, };
	int ret;

	ret = resource_copy_by_name(panel_data, date, "date");
	if (ret < 0)
		return ret;

	ret = resource_copy_by_name(panel_data, coordinate, "coordinate");
	if (ret < 0)
		return ret;

	panel_info("cell_id(old) : %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
			panel_data->date[0], panel_data->date[1],
			panel_data->date[2], panel_data->date[3], panel_data->date[4],
			panel_data->date[5], panel_data->date[6], panel_data->coordinate[0],
			panel_data->coordinate[1], panel_data->coordinate[2],
			panel_data->coordinate[3]);

	panel_info("cell_id(new) : %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
			date[0], date[1], date[2], date[3], date[4], date[5],
			date[6], coordinate[0], coordinate[1], coordinate[2],
			coordinate[3]);

	if (memcmp(panel_data->date, date, sizeof(panel_data->date)) ||
		memcmp(panel_data->coordinate, coordinate, sizeof(panel_data->coordinate))) {
		memcpy(panel_data->date, date, sizeof(panel_data->date));
		memcpy(panel_data->coordinate, coordinate, sizeof(panel_data->coordinate));
		panel_info("panel is changed\n");
		return 1;
	}

	return 0;
}

/*
 * panel_mtp_gamma_check - call gamma_check function defined in ddi.
 * Do not use op_lock in the function defined in ddi. A deadlock may occur.
 */
int panel_mtp_gamma_check(struct panel_device *panel)
{
	struct ddi_ops *ops = &panel->panel_data.ddi_ops;

	if (!ops->mtp_gamma_check) {
		panel_warn("not supported");
		return -1;
	}
	return ops->mtp_gamma_check(panel, NULL, 0);
}

/*
 * panel_flash_checksum_calc - call gamma_flash_checksum function defined in ddi.
 * Do not use op_lock in the function defined in ddi. A deadlock may occur.
 */
int panel_flash_checksum_calc(struct panel_device *panel)
{
	struct ddi_ops *ops = &panel->panel_data.ddi_ops;

	if (!ops->gamma_flash_checksum) {
		panel_warn("not supported");
		return 0;
	}
	return ops->gamma_flash_checksum(panel, &panel->flash_checksum_result, sizeof(panel->flash_checksum_result));
}

#ifdef CONFIG_SUPPORT_DIM_FLASH
int panel_update_dim_type(struct panel_device *panel, u32 dim_type)
{
	struct dim_flash_result *result;
	u8 mtp_reg[64];
	int sz_mtp_reg = 0;
	int state, state_all = 0;
	int index, result_idx = 0;
	int ret;

	if (dim_type == DIM_TYPE_DIM_FLASH) {
		if (!panel->dim_flash_result) {
			panel_err("dim buffer not found\n");
			return -ENOMEM;
		}

		memset(panel->dim_flash_result, 0,
			sizeof(struct dim_flash_result) * panel->max_nr_dim_flash_result);

		for (index = 0; index < panel->max_nr_dim_flash_result; index++) {
			if (get_poc_partition_size(&panel->poc_dev,
						POC_DIM_PARTITION + index) == 0) {
				continue;
			}
			result = &panel->dim_flash_result[result_idx++];
			state = 0;
			do {
				/* DIM */
				ret = set_panel_poc(&panel->poc_dev, POC_OP_DIM_READ, &index);
				if (unlikely(ret < 0)) {
					panel_err("failed to read gamma flash(ret %d)\n", ret);
					state = GAMMA_FLASH_ERROR_READ_FAIL;
					break;
				}

#if !defined(CONFIG_SEC_PANEL_DIM_FLASH_NO_VALIDATION)
				ret = check_poc_partition_exists(&panel->poc_dev,
						POC_DIM_PARTITION + index);
				if (unlikely(ret < 0)) {
					panel_err("failed to check dim_flash exist\n");
					state = GAMMA_FLASH_ERROR_READ_FAIL;
					break;
				}

				if (ret == PARTITION_WRITE_CHECK_NOK) {
					panel_err("dim partition not exist(%d)\n", ret);
					state = GAMMA_FLASH_ERROR_NOT_EXIST;
					break;
				}
#endif
				ret = get_poc_partition_chksum(&panel->poc_dev,
						POC_DIM_PARTITION + index,
						&result->dim_chksum_ok,
						&result->dim_chksum_by_calc,
						&result->dim_chksum_by_read);
#if !defined(CONFIG_SEC_PANEL_DIM_FLASH_NO_VALIDATION)
				if (unlikely(ret < 0)) {
					panel_err("failed to get chksum(ret %d)\n", ret);
					state = GAMMA_FLASH_ERROR_READ_FAIL;
					break;
				}
				if (result->dim_chksum_by_calc !=
					result->dim_chksum_by_read) {
					panel_err("dim flash checksum(%04X,%04X) mismatch\n",
							result->dim_chksum_by_calc,
							result->dim_chksum_by_read);
					state = GAMMA_FLASH_ERROR_CHECKSUM_MISMATCH;
					break;
				}
#endif
				/* MTP */
				ret = set_panel_poc(&panel->poc_dev, POC_OP_MTP_READ, &index);
				if (unlikely(ret)) {
					panel_err("failed to read mtp flash(ret %d)\n", ret);
					state = GAMMA_FLASH_ERROR_READ_FAIL;
					break;
				}

				ret = get_poc_partition_chksum(&panel->poc_dev,
						POC_MTP_PARTITION + index,
						&result->mtp_chksum_ok,
						&result->mtp_chksum_by_calc,
						&result->mtp_chksum_by_read);
#if !defined(CONFIG_SEC_PANEL_DIM_FLASH_NO_VALIDATION)
				if (unlikely(ret < 0)) {
					panel_err("failed to get chksum(ret %d)\n", ret);
					state = GAMMA_FLASH_ERROR_READ_FAIL;
					break;
				}

				if (result->mtp_chksum_by_calc != result->mtp_chksum_by_read) {
					panel_err("mtp flash checksum(%04X,%04X) mismatch\n",
							result->mtp_chksum_by_calc, result->mtp_chksum_by_read);
					state = GAMMA_FLASH_ERROR_MTP_OFFSET;
					break;
				}
#endif

				ret = get_resource_size_by_name(&panel->panel_data, "mtp");
				if (unlikely(ret < 0)) {
					panel_err("failed to get resource mtp size (ret %d)\n", ret);
					state = GAMMA_FLASH_ERROR_READ_FAIL;
					break;
				}
				sz_mtp_reg = ret;

				ret = resource_copy_by_name(&panel->panel_data, mtp_reg, "mtp");
				if (unlikely(ret < 0)) {
					panel_err("failed to copy resource mtp (ret %d)\n", ret);
					state = GAMMA_FLASH_ERROR_READ_FAIL;
					break;
				}

#if !defined(CONFIG_SEC_PANEL_DIM_FLASH_NO_VALIDATION)
				if (cmp_poc_partition_data(&panel->poc_dev,
					POC_MTP_PARTITION + index, 0, mtp_reg, sz_mtp_reg)) {
					panel_err("mismatch mtp(ret %d)\n", ret);
					state = GAMMA_FLASH_ERROR_MTP_OFFSET;
					break;
				}
#endif
				result->mtp_chksum_by_reg = calc_checksum_16bit(mtp_reg, sz_mtp_reg);
			} while (0);

			if (state_all == 0)
				state_all = state;

			if (state == 0)
				result->state = GAMMA_FLASH_SUCCESS;
			else
				result->state = state;

		}
		panel->nr_dim_flash_result = result_idx;

		if (state_all != 0)
			return state_all;
		/* update dimming flash, mtp, hbm_gamma resources */
		ret = panel_dim_flash_resource_init(panel);
		if (unlikely(ret)) {
			panel_err("failed to resource init\n");
			state_all = GAMMA_FLASH_ERROR_READ_FAIL;
		}
	}

	mutex_lock(&panel->op_lock);
	panel->panel_data.props.cur_dim_type = dim_type;
	mutex_unlock(&panel->op_lock);

	ret = panel_maptbl_init(panel);
	if (unlikely(ret)) {
		panel_err("failed to resource init\n");
		state_all = -ENODEV;
	}

	return state_all;
}
#endif

int panel_reprobe(struct panel_device *panel)
{
	struct common_panel_info *info;
	int ret;

	info = panel_detect(panel);
	if (unlikely(!info)) {
		panel_err("panel detection failed\n");
		return -ENODEV;
	}

	ret = panel_prepare(panel, info);
	if (unlikely(ret)) {
		panel_err("failed to panel_prepare\n");
		return ret;
	}

	ret = panel_id_init(panel);
	if (unlikely(ret)) {
		panel_err("failed to id init\n");
		return ret;
	}

	ret = panel_resource_init(panel);
	if (unlikely(ret)) {
		panel_err("failed to resource init\n");
		return ret;
	}

#ifdef CONFIG_SUPPORT_DDI_FLASH
	ret = panel_poc_probe(panel, info->poc_data);
	if (unlikely(ret)) {
		panel_err("failed to probe poc driver\n");
		return -ENODEV;
	}
#endif /* CONFIG_SUPPORT_DDI_FLASH */

	ret = panel_maptbl_init(panel);
	if (unlikely(ret)) {
		panel_err("failed to maptbl init\n");
		return -ENODEV;
	}

	ret = panel_bl_probe(panel);
	if (unlikely(ret)) {
		panel_err("failed to probe backlight driver\n");
		return -ENODEV;
	}

	return 0;
}

#ifdef CONFIG_SUPPORT_DIM_FLASH
static void dim_flash_handler(struct work_struct *work)
{
	struct panel_work *w = container_of(to_delayed_work(work),
			struct panel_work, dwork);
	struct panel_device *panel =
		container_of(w, struct panel_device, work[PANEL_WORK_DIM_FLASH]);
	int ret;

	mutex_lock(&panel->panel_bl.lock);
	if (atomic_read(&w->running) >= 2) {
		panel_info("already running\n");
		mutex_unlock(&panel->panel_bl.lock);
		return;
	}
	atomic_set(&w->running, 2);
	panel_info("+\n");
	ret = panel_update_dim_type(panel, DIM_TYPE_DIM_FLASH);
	if (ret < 0) {
		panel_err("failed to update dim_flash %d\n", ret);
		w->ret = ret;
	} else {
		panel_info("update dim_flash done %d\n", ret);
		w->ret = ret;
	}
	panel_info("-\n");
	atomic_set(&w->running, 0);
	mutex_unlock(&panel->panel_bl.lock);
	panel_update_brightness(panel);
}
#endif

void clear_check_wq_var(struct panel_condition_check *pcc)
{
	pcc->check_state = NO_CHECK_STATE;
	pcc->is_panel_check = false;
	pcc->frame_cnt = 0;
}

bool show_copr_value(struct panel_device *panel, int frame_cnt)
{
	bool retVal = false;
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	int ret = 0;
	struct copr_info *copr = &panel->copr;
	char write_buf[200] = {0, };
	int c = 0, i = 0, len = 0;

	if (copr_is_enabled(copr)) {
		ret = copr_get_value(copr);
		if (ret < 0) {
			panel_err("failed to get copr\n");
			return retVal;
		}
		len += snprintf(write_buf + len, sizeof(write_buf) - len, "cur:%d avg:%d ",
			copr->props.cur_copr, copr->props.avg_copr);
		if (copr->props.nr_roi > 0) {
			len += snprintf(write_buf + len, sizeof(write_buf) - len, "roi:");
			for (i = 0; i < copr->props.nr_roi; i++) {
				for (c = 0; c < 3; c++) {
					if (sizeof(write_buf) - len > 0) {
						len += snprintf(write_buf + len, sizeof(write_buf) - len, "%d%s",
							copr->props.copr_roi_r[i][c],
							((i == copr->props.nr_roi - 1) && c == 2) ? "\n" : " ");
					}
				}
			}
		} else {
			len += snprintf(write_buf + len, sizeof(write_buf) - len, "%s", "\n");
		}
		panel_info("copr(frame_cnt=%d) -> %s", frame_cnt, write_buf);
		if (copr->props.cur_copr > 0) /* not black */
			retVal = true;
	} else {
		panel_info("copr do not support\n");
	}
#else
	panel_info("copr feature is disabled\n");
#endif
	return retVal;
}

static void panel_condition_handler(struct work_struct *work)
{
	int ret = 0;
	struct panel_work *w = container_of(to_delayed_work(work),
			struct panel_work, dwork);
	struct panel_device *panel =
		container_of(w, struct panel_device, work[PANEL_WORK_CHECK_CONDITION]);
	struct panel_condition_check *condition = &panel->condition_check;

	if (atomic_read(&w->running)) {
		panel_info("already running\n");
		return;
	}
	panel_wake_lock(panel, WAKE_TIMEOUT_MSEC);
	atomic_set(&w->running, 1);
	mutex_lock(&w->lock);
	panel_info("%s\n", condition->str_state[condition->check_state]);

	switch (condition->check_state) {
	case PRINT_NORMAL_PANEL_INFO:
		// print rddpm
		ret = panel_do_seqtbl_by_index(panel, PANEL_CHECK_CONDITION_SEQ);
		if (unlikely(ret < 0))
			panel_err("failed to write panel check\n");
		if (show_copr_value(panel, condition->frame_cnt))
			clear_check_wq_var(condition);
		else
			condition->check_state = CHECK_NORMAL_PANEL_INFO;
		break;
	case PRINT_DOZE_PANEL_INFO:
		ret = panel_do_seqtbl_by_index(panel, PANEL_CHECK_CONDITION_SEQ);
		if (unlikely(ret < 0))
			panel_err("failed to write panel check\n");
		clear_check_wq_var(condition);
		break;
	case CHECK_NORMAL_PANEL_INFO:
		if (show_copr_value(panel, condition->frame_cnt))
			clear_check_wq_var(condition);
		break;
	default:
		panel_info("state %d\n", condition->check_state);
		clear_check_wq_var(condition);
		break;
	}
	mutex_unlock(&w->lock);
	atomic_set(&w->running, 0);
	panel_wake_unlock(panel);
}

int init_check_wq(struct panel_condition_check *condition)
{
	clear_check_wq_var(condition);
	strcpy(condition->str_state[NO_CHECK_STATE], STR_NO_CHECK);
	strcpy(condition->str_state[PRINT_NORMAL_PANEL_INFO], STR_NOMARL_ON);
	strcpy(condition->str_state[CHECK_NORMAL_PANEL_INFO], STR_NOMARL_100FRAME);
	strcpy(condition->str_state[PRINT_DOZE_PANEL_INFO], STR_AOD_ON);

	return 0;
}

void panel_check_ready(struct panel_device *panel)
{
	struct panel_condition_check *pcc = &panel->condition_check;
	struct panel_work *pw = &panel->work[PANEL_WORK_CHECK_CONDITION];

	mutex_lock(&pw->lock);
	pcc->is_panel_check = true;
	if (panel->state.cur_state == PANEL_STATE_NORMAL)
		pcc->check_state = PRINT_NORMAL_PANEL_INFO;
	if (panel->state.cur_state == PANEL_STATE_ALPM)
		pcc->check_state = PRINT_DOZE_PANEL_INFO;
	mutex_unlock(&pw->lock);
}

static void panel_check_start(struct panel_device *panel)
{
	struct panel_condition_check *pcc = &panel->condition_check;
	struct panel_work *pw = &panel->work[PANEL_WORK_CHECK_CONDITION];

	mutex_lock(&pw->lock);
	if (pcc->frame_cnt < 100) {
		pcc->frame_cnt++;
		switch (pcc->check_state) {
		case PRINT_NORMAL_PANEL_INFO:
		case PRINT_DOZE_PANEL_INFO:
			if (pcc->frame_cnt == 1)
				queue_delayed_work(pw->wq, &pw->dwork, msecs_to_jiffies(0));
			break;
		case CHECK_NORMAL_PANEL_INFO:
			if (pcc->frame_cnt % 10 == 0)
				queue_delayed_work(pw->wq, &pw->dwork, msecs_to_jiffies(0));
			break;
		case NO_CHECK_STATE:
			// do nothing
			break;
		default:
			panel_warn("state is invalid %d %d %d\n",
					pcc->is_panel_check, pcc->frame_cnt, pcc->check_state);
			clear_check_wq_var(pcc);
			break;
		}
	} else {
		if (pcc->check_state == CHECK_NORMAL_PANEL_INFO)
			panel_warn("screen is black in first 100 frame %d %d %d\n",
					pcc->is_panel_check, pcc->frame_cnt, pcc->check_state);
		else
			panel_warn("state is invalid %d %d %d\n",
					pcc->is_panel_check, pcc->frame_cnt, pcc->check_state);
		clear_check_wq_var(pcc);
	}
	mutex_unlock(&pw->lock);
}

static void panel_update_handler(struct work_struct *work)
{
	struct panel_work *w = container_of(to_delayed_work(work),
			struct panel_work, dwork);
	struct panel_device *panel =
		container_of(w, struct panel_device, work[PANEL_WORK_UPDATE]);
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	struct panel_properties *props = &panel->panel_data.props;
	bool vrr_updated = false;
	int ret = 0;

	mutex_lock(&w->lock);
	mutex_lock(&panel_bl->lock);
	ret = update_vrr_lfd(&props->vrr_lfd_info);
	if (panel_bl->bd && ret == VRR_LFD_UPDATED) {
		props->vrr_updated = true;
		vrr_updated = true;
	}
	mutex_unlock(&panel_bl->lock);

	if (vrr_updated) {
		ret = panel_update_brightness(panel);
		if (ret < 0)
			panel_err("failed to update vrr & brightness\n");
	}
	mutex_unlock(&w->lock);
}

#ifdef CONFIG_EVASION_DISP_DET
static void evasion_disp_det_handler(struct work_struct *work)
{
	int ret;
	struct panel_work *w = container_of(to_delayed_work(work),
			struct panel_work, dwork);
	struct panel_device *panel =
		container_of(w, struct panel_device, work[PANEL_WORK_EVASION_DISP_DET]);

	panel_info("++\n");

	ret = panel_set_gpio_irq(&panel->gpio[PANEL_GPIO_DISP_DET], true);
	if (ret < 0)
		panel_warn("do not support irq\n");

}
#endif

int panel_probe(struct panel_device *panel)
{
	int i, ret = 0;
	struct panel_info *panel_data;
	struct common_panel_info *info;
	char name[MAX_PANEL_DEV_NAME_SIZE];

	panel_info("+\n");

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;

	info = panel_detect(panel);
	if (unlikely(!info)) {
		panel_err("panel detection failed\n");
		return -ENODEV;
	}

#ifdef CONFIG_EXYNOS_DECON_LCD_SPI
	/*
	 * TODO : move to parse dt function
	 * 1. get panel_spi device node.
	 * 2. get spi_device of node
	 */
	panel->spi = of_find_panel_spi_by_node(NULL);
	if (!panel->spi)
		panel_warn("panel spi device unsupported\n");
#endif

	mutex_lock(&panel->op_lock);
	panel_data->props.temperature = NORMAL_TEMPERATURE;
	panel_data->props.alpm_mode = ALPM_OFF;
	panel_data->props.cur_alpm_mode = ALPM_OFF;
	panel_data->props.lpm_opr = 250;		/* default LPM OPR 2.5 */
	panel_data->props.cur_lpm_opr = 250;	/* default LPM OPR 2.5 */
	panel_data->props.panel_partial_disp = 0;
	panel_data->props.dia_mode = 1;
	panel_data->props.irc_mode = 0;
	panel_data->props.panel_partial_disp =
		(info->ddi_props.support_partial_disp) ? 0 : -1;

	memset(panel_data->props.mcd_rs_range, -1,
			sizeof(panel_data->props.mcd_rs_range));

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
	panel_data->props.gct_on = GRAM_TEST_OFF;
	panel_data->props.gct_vddm = VDDM_ORIG;
	panel_data->props.gct_pattern = GCT_PATTERN_NONE;
#endif
#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
	panel_data->props.dynamic_hlpm = 0;
#endif
#ifdef CONFIG_SUPPORT_TDMB_TUNE
	panel_data->props.tdmb_on = false;
	panel_data->props.cur_tdmb_on = false;
#endif
#ifdef CONFIG_SUPPORT_DIM_FLASH
	panel_data->props.cur_dim_type = DIM_TYPE_AID_DIMMING;
#endif
	for (i = 0; i < MAX_CMD_LEVEL; i++)
		panel_data->props.key[i] = 0;
	mutex_unlock(&panel->op_lock);

	mutex_lock(&panel->panel_bl.lock);
	panel_data->props.adaptive_control = 1;
#ifdef CONFIG_SUPPORT_XTALK_MODE
	panel_data->props.xtalk_mode = XTALK_OFF;
#endif
	panel_data->props.poc_onoff = POC_ONOFF_ON;
	mutex_unlock(&panel->panel_bl.lock);

	panel_data->props.mres_mode = 0;
	panel_data->props.old_mres_mode = 0;
	panel_data->props.mres_updated = false;
	panel_data->props.ub_con_cnt = 0;
	panel_data->props.conn_det_enable = 0;

	/* variable refresh rate */
	panel_data->props.vrr_fps = 60;
	panel_data->props.vrr_mode = VRR_NORMAL_MODE;
	panel_data->props.vrr_idx = 0;
	panel_data->props.vrr_origin_fps = 60;
	panel_data->props.vrr_origin_mode = VRR_NORMAL_MODE;
	panel_data->props.vrr_origin_idx = 0;
#ifdef CONFIG_PANEL_VRR_BRIDGE
	panel_data->props.vrr_bridge_enable = true;
#endif

#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	panel_data->props.enable_fd = 1;
#endif
#ifdef CONFIG_SEC_FACTORY
	/*
	 * set vrr_lfd min/max high frequency to
	 * disable lfd in factory binary as default.
	 */
	for (i = 0; i < MAX_VRR_LFD_SCOPE; i++)
		panel_data->props.vrr_lfd_info.req[VRR_LFD_CLIENT_FAC][i].fix = VRR_LFD_FREQ_HIGH;
	queue_delayed_work(panel->work[PANEL_WORK_UPDATE].wq,
			&panel->work[PANEL_WORK_UPDATE].dwork, msecs_to_jiffies(0));

#endif
	ret = panel_prepare(panel, info);
	if (unlikely(ret)) {
		panel_err("failed to prepare common panel driver\n");
		return -ENODEV;
	}

	if (panel->id == 0)
		snprintf(name, MAX_PANEL_DEV_NAME_SIZE,
				"%s", PANEL_DEV_NAME);
	else
		snprintf(name, MAX_PANEL_DEV_NAME_SIZE,
				"%s-%d", PANEL_DEV_NAME, panel->id);

	panel->lcd_dev = device_create(lcd_class,
			panel->dev, 0, panel, name);
	if (IS_ERR_OR_NULL(panel->lcd_dev)) {
		panel_err("failed to create lcd device\n");
		return PTR_ERR(panel->lcd_dev);
	}

#ifdef CONFIG_SUPPORT_POC_SPI
	ret = panel_spi_drv_probe(panel, info->spi_data_tbl, info->nr_spi_data_tbl);
	if (unlikely(ret))
		panel_err("failed to probe panel spi driver\n");

#endif

#ifdef CONFIG_SUPPORT_DDI_FLASH
	ret = panel_poc_probe(panel, info->poc_data);
	if (unlikely(ret))
		panel_err("failed to probe poc driver\n");

#endif /* CONFIG_SUPPORT_DDI_FLASH */

	ret = panel_id_init(panel);
	if (unlikely(ret)) {
		panel_err("failed to id init\n");
		return -ENODEV;
	}

	ret = panel_resource_init(panel);
	if (unlikely(ret)) {
		panel_err("failed to resource init\n");
		return -ENODEV;
	}

	resource_copy_by_name(panel_data, panel_data->date, "date");
	resource_copy_by_name(panel_data, panel_data->coordinate, "coordinate");

	ret = panel_maptbl_init(panel);
	if (unlikely(ret)) {
		panel_err("failed to resource init\n");
		return -ENODEV;
	}

	ret = panel_bl_probe(panel);
	if (unlikely(ret)) {
		panel_err("failed to probe backlight driver\n");
		return -ENODEV;
	}

#ifdef CONFIG_EXYNOS_DECON_LCD_SYSFS
	ret = panel_sysfs_probe(panel);
	if (unlikely(ret)) {
		panel_err("failed to init sysfs\n");
		return -ENODEV;
	}
#endif

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	ret = mdnie_probe(panel, info->mdnie_tune);
	if (unlikely(ret)) {
		panel_err("failed to probe mdnie driver\n");
		return -ENODEV;
	}
#endif


#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	ret = copr_probe(panel, info->copr_data);
	if (unlikely(ret)) {
		panel_err("failed to probe copr driver\n");
		BUG();
		return -ENODEV;
	}
#endif

#ifdef CONFIG_EXTEND_LIVE_CLOCK
	ret = aod_drv_probe(panel, info->aod_tune);
	if (unlikely(ret)) {
		panel_err("failed to probe aod driver\n");
		BUG();
		return -ENODEV;
	}
#endif

#ifdef CONFIG_SUPPORT_MAFPC
	ret = panel_get_v4l2_abc_dev(panel, info->mafpc_info);
	if (unlikely(ret < 0)) {
		panel_err("failed to probe mafpc driver\n");
		return -ENODEV;
	}
#endif

#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
	ret = profiler_probe(panel, info->profile_tune);
	if (unlikely(ret))
		panel_err("failed to probe profiler\n");
#endif

#ifdef CONFIG_DYNAMIC_MIPI
	ret = dynamic_mipi_probe(panel, info->dm_total_band);
	if (ret)
		panel_err("failed to register dynamic mipi module\n");
#endif

#ifdef CONFIG_SUPPORT_DIM_FLASH
	mutex_lock(&panel->panel_bl.lock);
	mutex_lock(&panel->op_lock);
	for (i = 0; i < MAX_PANEL_BL_SUBDEV; i++) {
		if (panel_data->panel_dim_info[i] == NULL)
			continue;

		if (panel_data->panel_dim_info[i]->dim_flash_on) {
			panel_data->props.dim_flash_on = true;
			panel_info("dim_flash : on\n");
			break;
		}
	}
	mutex_unlock(&panel->op_lock);
	mutex_unlock(&panel->panel_bl.lock);

	if (panel_data->props.dim_flash_on)
		queue_delayed_work(panel->work[PANEL_WORK_DIM_FLASH].wq,
				&panel->work[PANEL_WORK_DIM_FLASH].dwork,
				msecs_to_jiffies(500));
#endif /* CONFIG_SUPPORT_DIM_FLASH */
	init_check_wq(&panel->condition_check);

#if defined(CONFIG_PANEL_DISPLAY_MODE)
	mutex_lock(&panel->op_lock);
	ret = panel_display_mode_get_native_mode(panel);
	if (ret >= 0) {
		panel_data->props.panel_mode = ret;
#if defined(CONFIG_PANEL_VRR_BRIDGE)
		panel->panel_data.props.target_panel_mode = ret;
#endif
		panel_info("apply default panel_mode %d\n", ret);

	} else {
		panel_err("failed to update panel_mode\n");
	}
	mutex_unlock(&panel->op_lock);

	panel_update_brightness_cmd_skip(panel);

	ret = panel_update_display_mode(panel);
	if (ret < 0)
		panel_err("failed to panel_update_display_mode\n");

#endif

	panel_info("-\n");

	return 0;
}

int panel_remove(struct panel_device *panel)
{
	int ret;

#ifdef CONFIG_SUPPORT_DIM_FLASH
	/* TODO: dim flash need to flush before sleep in */
	if (panel->panel_data.props.dim_flash_on)
		flush_delayed_work(&panel->work[PANEL_WORK_DIM_FLASH].dwork);
#endif

#ifdef CONFIG_DYNAMIC_MIPI
	/* TODO: remove dynamic_freq */
#endif

#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
	ret = profiler_remove(panel);
	if (ret < 0) {
		panel_err("failed to remove profiler\n");
		return ret;
	}
#endif

#ifdef CONFIG_EXTEND_LIVE_CLOCK
	ret = aod_drv_remove(panel);
	if (ret < 0) {
		panel_err("failed to remove aod driver\n");
		return ret;
	}
#endif

#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	ret = copr_remove(panel);
	if (ret < 0) {
		panel_err("failed to remove copr driver\n");
		return ret;
	}
#endif

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	ret = mdnie_remove(panel);
	if (ret < 0) {
		panel_err("failed to remove mdnie driver\n");
		return ret;
	}
#endif

#ifdef CONFIG_EXYNOS_DECON_LCD_SYSFS
	ret = panel_sysfs_remove(panel);
	if (ret < 0) {
		panel_err("failed to remove panel-sysfs driver\n");
		return ret;
	}
#endif

	ret = panel_bl_remove(panel);
	if (ret < 0) {
		panel_err("failed to remove panel-bl driver\n");
		return ret;
	}

#ifdef CONFIG_SUPPORT_DDI_FLASH
	ret = panel_poc_remove(panel);
	if (ret < 0) {
		panel_err("failed to remove poc driver\n");
		return ret;
	}
#endif /* CONFIG_SUPPORT_DDI_FLASH */

#ifdef CONFIG_SUPPORT_POC_SPI
	ret = panel_spi_drv_remove(panel);
	if (ret < 0) {
		panel_err("failed to remove panel spi driver\n");
		return ret;
	}
#endif
	device_destroy(lcd_class, 0);
	panel->lcd_dev = NULL;

	return 0;
}

static int panel_sleep_in(struct panel_device *panel)
{
	int ret = 0;
	struct panel_state *state = &panel->state;
	enum panel_active_state prev_state = state->cur_state;

	if (state->connect_panel == PANEL_DISCONNECT) {
		panel_warn("panel no use\n");
		goto do_exit;
	}

	switch (state->cur_state) {
	case PANEL_STATE_ON:
		panel_warn("panel already %s state\n", panel_state_names[state->cur_state]);
		goto do_exit;
	case PANEL_STATE_NORMAL:
	case PANEL_STATE_ALPM:
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
		copr_disable(&panel->copr);
#endif

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
		mdnie_disable(&panel->mdnie);
#endif
		ret = panel_display_off(panel);
		if (unlikely(ret < 0))
			panel_err("failed to write display_off seqtbl\n");
		ret = __panel_seq_exit(panel);
		if (unlikely(ret < 0))
			panel_err("failed to write exit seqtbl\n");
		break;
	default:
		panel_err("invalid state(%d)\n", state->cur_state);
		goto do_exit;
	}

	panel_set_cur_state(panel, PANEL_STATE_ON);
	panel_info("panel_state:%s -> %s\n",
			panel_state_names[prev_state], panel_state_names[state->cur_state]);

#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
	panel->profiler.flag_font = 0;
#endif
	return 0;

do_exit:
	return ret;
}

static int panel_power_on(struct panel_device *panel)
{
	int ret = 0;
	struct panel_state *state = &panel->state;
	enum panel_active_state prev_state = state->cur_state;

	if (panel->state.connect_panel == PANEL_DISCONNECT) {
		panel_warn("panel no use\n");
		ret = -ENODEV;
		goto do_exit;
	}

	panel->state.connected = panel_conn_det_state(panel);
	if (panel->state.connected < 0) {
		panel_dbg("conn unsupported\n");
	} else if (panel->state.connected == PANEL_STATE_NOK) {
		panel_warn("ub disconnected\n");
#if defined(CONFIG_SUPPORT_PANEL_SWAP)
		ret = -ENODEV;
		goto do_exit;
#endif
	} else {
		panel_dbg("ub connected\n");
	}

	if (state->cur_state == PANEL_STATE_OFF) {
		ret = __set_panel_power(panel, PANEL_POWER_ON);
		if (ret) {
			panel_err("failed to panel power on\n");
			goto do_exit;
		}
		panel_set_cur_state(panel, PANEL_STATE_ON);
	}
#ifdef CONFIG_PANEL_NOTIFY
	panel_send_ubconn_notify(panel->state.connected == PANEL_STATE_OK ?
		PANEL_EVENT_UB_CON_CONNECTED : PANEL_EVENT_UB_CON_DISCONNECTED);
#endif

	panel_info("panel_state:%s -> %s\n",
		panel_state_names[prev_state], panel_state_names[state->cur_state]);

	return 0;

do_exit:
#ifdef CONFIG_PANEL_NOTIFY
	panel_send_ubconn_notify(PANEL_EVENT_UB_CON_DISCONNECTED);
#endif
	return ret;
}

static int panel_power_off(struct panel_device *panel)
{
	int ret = -EINVAL;
	struct panel_state *state = &panel->state;
	enum panel_active_state prev_state = state->cur_state;

	if (state->connect_panel == PANEL_DISCONNECT) {
		panel_warn("panel no use\n");
		goto do_exit;
	}

	switch (state->cur_state) {
	case PANEL_STATE_OFF:
		panel_info("panel already %s state\n", panel_state_names[state->cur_state]);
		goto do_exit;
	case PANEL_STATE_ALPM:
	case PANEL_STATE_NORMAL:
		panel_sleep_in(panel);
	case PANEL_STATE_ON:
		ret = __set_panel_power(panel, PANEL_POWER_OFF);
		if (ret) {
			panel_err("failed to panel power off\n");
			goto do_exit;
		}
		break;
	default:
		panel_err("invalid state(%d)\n", state->cur_state);
		goto do_exit;
	}
	panel_set_cur_state(panel, PANEL_STATE_OFF);
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	ret = panel_aod_power_off(panel);
	if (ret)
		panel_err("failed to aod power off\n");
#endif

	panel_info("panel_state:%s -> %s\n",
			panel_state_names[prev_state], panel_state_names[state->cur_state]);

	return 0;

do_exit:
	return ret;
}

static int panel_sleep_out(struct panel_device *panel)
{
	int ret = 0;
	static int retry = 3;
	struct panel_state *state = &panel->state;
	enum panel_active_state prev_state = state->cur_state;

	if (panel->state.connect_panel == PANEL_DISCONNECT) {
		panel_warn("panel no use\n");
		return -ENODEV;
	}

	panel->state.connected = panel_conn_det_state(panel);
	if (panel->state.connected < 0) {
		panel_dbg("conn unsupported\n");
	} else if (panel->state.connected == PANEL_STATE_NOK) {
		panel_warn("ub disconnected\n");
#if defined(CONFIG_SUPPORT_PANEL_SWAP)
		return -ENODEV;
#endif
	} else {
		panel_info("ub connected\n");
	}

	switch (state->cur_state) {
	case PANEL_STATE_NORMAL:
		panel_warn("panel already %s state\n",
				panel_state_names[state->cur_state]);
		goto do_exit;
	case PANEL_STATE_ALPM:
#ifdef CONFIG_EXYNOS_DOZE
		ret = __panel_seq_exit_alpm(panel);
		if (ret) {
			panel_err("failed to panel exit alpm\n");
			goto do_exit;
		}
#endif
		break;
	case PANEL_STATE_OFF:
		ret = panel_power_on(panel);
		if (ret) {
			panel_err("failed to power on\n");
			goto do_exit;
		}
	case PANEL_STATE_ON:
		ret = __panel_seq_init(panel);
		if (ret) {
			if (--retry >= 0 && ret == -EAGAIN) {
				panel_power_off(panel);
				msleep(100);
				goto do_exit;
			}
			panel_err("failed to panel init seq\n");
			BUG();
		}
		retry = 3;
		break;
	default:
		panel_err("invalid state(%d)\n", state->cur_state);
		goto do_exit;
	}
	panel_set_cur_state(panel, PANEL_STATE_NORMAL);
	state->disp_on = PANEL_DISPLAY_OFF;
	panel->ktime_panel_on = ktime_get();

	mutex_lock(&panel->work[PANEL_WORK_CHECK_CONDITION].lock);
	clear_check_wq_var(&panel->condition_check);
	mutex_unlock(&panel->work[PANEL_WORK_CHECK_CONDITION].lock);
#ifdef CONFIG_PANEL_VRR_BRIDGE
	if (prev_state == PANEL_STATE_ALPM) {
		mutex_lock(&panel->op_lock);
		panel->panel_data.props.panel_mode =
			panel->panel_data.props.target_panel_mode;
		mutex_unlock(&panel->op_lock);
		ret = panel_update_display_mode(panel);
		if (ret < 0)
			panel_err("failed to panel_update_display_mode\n");
	}
#endif
#ifdef CONFIG_SUPPORT_HMD
	if (state->hmd_on == PANEL_HMD_ON) {
		panel_info("hmd was on, setting hmd on seq\n");
		ret = __panel_seq_hmd_on(panel);
		if (ret)
			panel_err("failed to set hmd on seq\n");

		ret = panel_bl_set_brightness(&panel->panel_bl,
				PANEL_BL_SUBDEV_TYPE_HMD, SEND_CMD);
		if (ret)
			panel_err("fail to set hmd brightness\n");
	}
#endif

	panel_info("panel_state:%s -> %s\n",
			panel_state_names[prev_state],
			panel_state_names[state->cur_state]);

	return 0;

do_exit:
	return ret;
}

#ifdef CONFIG_EXYNOS_DOZE
static int panel_doze(struct panel_device *panel, unsigned int cmd)
{
	int ret = 0;
	struct panel_state *state = &panel->state;
	enum panel_active_state prev_state = state->cur_state;

	if (state->connect_panel == PANEL_DISCONNECT) {
		panel_warn("panel no use\n");
		goto do_exit;
	}

	switch (state->cur_state) {
	case PANEL_STATE_ALPM:
		panel_warn("panel already %s state\n",
				panel_state_names[state->cur_state]);
		goto do_exit;
	case PANEL_POWER_ON:
	case PANEL_POWER_OFF:
		ret = panel_sleep_out(panel);
		if (ret) {
			panel_err("failed to set normal\n");
			goto do_exit;
		}
	case PANEL_STATE_NORMAL:
		ret = __panel_seq_set_alpm(panel);
		if (ret)
			panel_err("failed to write alpm\n");
		panel_set_cur_state(panel, PANEL_STATE_ALPM);
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
		panel_mdnie_update(panel);
#endif
		break;
	default:
		break;
	}
	mutex_lock(&panel->work[PANEL_WORK_CHECK_CONDITION].lock);
	clear_check_wq_var(&panel->condition_check);
	mutex_unlock(&panel->work[PANEL_WORK_CHECK_CONDITION].lock);
	panel_info("panel_state:%s -> %s\n",
			panel_state_names[prev_state], panel_state_names[state->cur_state]);

do_exit:
	return ret;
}
#endif //CONFIG_EXYNOS_DOZE

static int panel_register_cb(struct v4l2_subdev *sd, int cb_id)
{
	struct panel_device *panel = container_of(sd, struct panel_device, sd);
	struct disp_cb_info *cb_info;

	if (cb_id >= MAX_PANEL_CB) {
		panel_err("out of range (cb_id:%d)\n", cb_id);
		return -EINVAL;
	}

	cb_info = (struct disp_cb_info *)v4l2_get_subdev_hostdata(sd);
	if (!cb_info) {
		panel_err("failed to get cb_info (cb_id:%d)\n", cb_id);
		return -EINVAL;
	}
	memcpy(&panel->cb_info[cb_id], cb_info, sizeof(struct disp_cb_info));

	return 0;
}

static int panel_register_vrr_cb(struct v4l2_subdev *sd)
{
	int ret;

	ret = panel_register_cb(sd, PANEL_CB_VRR);
	if (ret < 0) {
		panel_err("failed to register callback(PANEL_CB_VRR)\n");
		return ret;
	}

	return 0;
}

int panel_vrr_cb(struct panel_device *panel)
{
	struct disp_cb_info *vrr_cb_info = &panel->cb_info[PANEL_CB_VRR];
	struct vrr_config_data vrr_info;
#ifdef CONFIG_PANEL_NOTIFY
	struct panel_dms_data dms_data;
	static struct panel_dms_data old_dms_data;
#endif
	struct panel_properties *props;
	struct panel_vrr *vrr;
	int ret = 0;

	props = &panel->panel_data.props;
	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return -EINVAL;

	vrr_info.fps = vrr->fps;
	vrr_info.mode = vrr->mode;
	if (vrr_cb_info->cb) {
		ret = vrr_cb_info->cb((void *)vrr_cb_info->data, (void *)&vrr_info);
		if (ret)
			panel_err("failed to vrr callback\n");
	}

#ifdef CONFIG_PANEL_NOTIFY
	dms_data.fps = vrr->fps /
		max(TE_SKIP_TO_DIV(vrr->te_sw_skip_count,
					vrr->te_hw_skip_count), MIN_VRR_DIV_COUNT);
	dms_data.lfd_min_freq = props->vrr_lfd_info.status[VRR_LFD_SCOPE_NORMAL].lfd_min_freq;
	dms_data.lfd_max_freq = props->vrr_lfd_info.status[VRR_LFD_SCOPE_NORMAL].lfd_max_freq;

	/* notify clients that vrr has changed */
	if (old_dms_data.fps != dms_data.fps) {
		panel_notifier_call_chain(PANEL_EVENT_VRR_CHANGED, &dms_data);
		panel_info("PANEL_EVENT_VRR_CHANGED fps:%d lfd_freq:%d~%dHz\n",
				dms_data.fps, dms_data.lfd_min_freq, dms_data.lfd_max_freq);
	}

	/* notify clients that fps or lfd has changed */
	if (memcmp(&old_dms_data, &dms_data, sizeof(struct panel_dms_data))) {
		panel_notifier_call_chain(PANEL_EVENT_LFD_CHANGED, &dms_data);
		panel_info("PANEL_EVENT_LFD_CHANGED fps:%d lfd_freq:%d~%dHz\n",
				dms_data.fps, dms_data.lfd_min_freq, dms_data.lfd_max_freq);
	}

	memcpy(&old_dms_data, &dms_data, sizeof(struct panel_dms_data));
#endif

	return ret;
}

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static int panel_register_display_mode_cb(struct v4l2_subdev *sd)
{
	int ret;

	ret = panel_register_cb(sd, PANEL_CB_DISPLAY_MODE);
	if (ret < 0) {
		panel_err("failed to register callback(PANEL_CB_DISPLAY_MODE)\n");
		return ret;
	}

	return 0;
}

int panel_display_mode_cb(struct panel_device *panel)
{
	struct disp_cb_info *cb_info = &panel->cb_info[PANEL_CB_DISPLAY_MODE];
	struct common_panel_display_modes *common_panel_modes =
		panel->panel_data.common_panel_modes;
	struct panel_properties *props = &panel->panel_data.props;
	int ret = 0, panel_mode = props->panel_mode;

	if (!panel_display_mode_is_supported(panel)) {
		panel_err("panel_display_mode not supported\n");
		return -EINVAL;
	}

	if (panel_mode < 0 ||
			panel_mode >= common_panel_modes->num_modes) {
		panel_err("invalid panel_mode %d\n", panel_mode);
		return -EINVAL;
	}

	if (cb_info->cb) {
		ret = cb_info->cb(cb_info->data,
				common_panel_modes->modes[panel_mode]);
		if (ret)
			panel_err("failed to display_mode callback\n");
	}
	panel_vrr_cb(panel);

	return ret;
}

static bool check_display_mode_cond(struct panel_device *panel)
{
	struct panel_properties *props = &panel->panel_data.props;

#if defined(CONFIG_SEC_FACTORY)
	if (props->alpm_mode != ALPM_OFF) {
		panel_warn("could not change display mode in lpm(%d) state\n",
			   props->alpm_mode);
		return false;
	}
#endif
	if (props->mcd_on == true) {
		panel_warn("could not change display mode in mcd(%d) state\n",
			   props->mcd_on);
		return false;
	}

	return true;
}

static struct common_panel_display_mode *
panel_find_common_panel_display_mode(struct panel_device *panel,
		struct panel_display_mode *pdm)
{
	struct common_panel_display_modes *common_panel_modes =
		panel->panel_data.common_panel_modes;
	int i;

	if (!common_panel_modes) {
		panel_err("common_panel_modes not prepared\n");
		return NULL;
	}

	for (i = 0; i < common_panel_modes->num_modes; i++) {
		if (common_panel_modes->modes[i] == NULL)
			continue;

		if (!strncmp(common_panel_modes->modes[i]->name,
					pdm->name, sizeof(pdm->name))) {
			panel_dbg("pdm:%s cpdm:%d:%s\n",
					pdm->name, i, common_panel_modes->modes[i]->name);
			return common_panel_modes->modes[i];
		}
	}

	return NULL;
}

int find_panel_mode_by_common_panel_display_mode(struct panel_device *panel,
		struct common_panel_display_mode *cpdm)
{
	struct common_panel_display_modes *common_panel_modes =
		panel->panel_data.common_panel_modes;
	int i;

	for (i = 0; i < common_panel_modes->num_modes; i++)
		if (cpdm == common_panel_modes->modes[i])
			break;

	if (i == common_panel_modes->num_modes)
		return -EINVAL;

	return i;
}

int panel_display_mode_find_panel_mode(struct panel_device *panel,
		struct panel_display_mode *pdm)
{
	struct common_panel_display_mode *cpdm;

	if (!panel_display_mode_is_supported(panel)) {
		panel_err("panel_display_mode not supported\n");
		return -EINVAL;
	}

	cpdm = panel_find_common_panel_display_mode(panel, pdm);
	if (!cpdm) {
		panel_err("panel_display_mode(%s) not found\n", pdm->name);
		return -EINVAL;
	}
	panel_dbg("%s:%s\n", pdm->name, cpdm->name);

	return find_panel_mode_by_common_panel_display_mode(panel, cpdm);
}

int panel_display_mode_get_native_mode(struct panel_device *panel)
{
	struct panel_display_modes *panel_modes =
		panel->panel_modes;
	struct panel_display_mode *pdm;
	int panel_mode;

	if (!panel_display_mode_is_supported(panel)) {
		panel_err("panel_display_mode not supported\n");
		return -EINVAL;
	}

	if (!panel_modes) {
		panel_err("panel_modes is null\n");
		return -EINVAL;
	}

	if (panel_modes->num_modes <= 0) {
		panel_err("panel_modes->num_modes is 0\n");
		return -EINVAL;
	}

	if (panel_modes->native_mode >=
			panel_modes->num_modes) {
		panel_err("panel_modes->native_mode(%d) is out of range\n",
				panel_modes->native_mode);
		return -EINVAL;
	}

	pdm = panel_modes->modes[panel_modes->native_mode];
	panel_mode =
		panel_display_mode_find_panel_mode(panel, pdm);
	if (panel_mode < 0) {
		panel_err("could not find panel_display_mode(%s)\n", pdm->name);
		return -EINVAL;
	}

	return panel_mode;
}

int panel_display_mode_get_mres_mode(struct panel_device *panel, int panel_mode)
{
	struct common_panel_display_modes *common_panel_modes =
		panel->panel_data.common_panel_modes;
	struct panel_mres *mres = &panel->panel_data.mres;
	struct panel_resol *resol;
	int i;

	if (!panel_display_mode_is_supported(panel)) {
		panel_err("panel_display_mode not supported\n");
		return -EINVAL;
	}

	if (panel_mode < 0 ||
			panel_mode >= common_panel_modes->num_modes) {
		panel_err("invalid panel_mode %d\n", panel_mode);
		return -EINVAL;
	}

	resol = common_panel_modes->modes[panel_mode]->resol;
	for (i = 0; i < mres->nr_resol; i++)
		if (resol == &mres->resol[i])
			break;

	return i;
}

bool panel_display_mode_is_mres_mode_changed(struct panel_device *panel, int panel_mode)
{
	struct panel_properties *props =
		&panel->panel_data.props;
	int mres_mode;

	mres_mode = panel_display_mode_get_mres_mode(panel, panel_mode);
	if (mres_mode < 0)
		return mres_mode;

	return (props->mres_mode != mres_mode);
}

int panel_display_mode_get_vrr_idx(struct panel_device *panel, int panel_mode)
{
	struct common_panel_display_modes *common_panel_modes =
		panel->panel_data.common_panel_modes;
	struct panel_vrr *vrr, **vrrtbl = panel->panel_data.vrrtbl;
	int i, num_vrrs = panel->panel_data.nr_vrrtbl;

	if (!panel_display_mode_is_supported(panel)) {
		panel_err("panel_display_mode not supported\n");
		return -EINVAL;
	}

	if (panel_mode < 0 ||
			panel_mode >= common_panel_modes->num_modes) {
		panel_err("invalid panel_mode %d\n", panel_mode);
		return -EINVAL;
	}

	vrr = common_panel_modes->modes[panel_mode]->vrr;
	if (vrr == NULL) {
		panel_err("vrr is null of panel_mode(%d)\n", panel_mode);
		return -EINVAL;
	}

	for (i = 0; i < num_vrrs; i++)
		if (vrr == vrrtbl[i])
			break;

	if (i == num_vrrs) {
		panel_warn("panel_mode(%d) vrr(%d%s) not found\n",
				panel_mode, vrr->fps, REFRESH_MODE_STR(vrr->mode));
		return -EINVAL;
	}

	return i;
}

static int panel_update_display_mode_props(struct panel_device *panel, int panel_mode)
{
	struct panel_properties *props = &panel->panel_data.props;
	struct panel_mres *mres = &panel->panel_data.mres;
	struct panel_vrr *vrr;
	struct panel_resol *resol;
	int mres_mode, vrr_idx;

	props->panel_mode = panel_mode;

	mres_mode = panel_display_mode_get_mres_mode(panel, panel_mode);
	if (mres_mode < 0) {
		panel_err("failed to get mres_mode from panel_mode(%d)\n", panel_mode);
		return -EINVAL;
	}

	if (mres_mode >= mres->nr_resol) {
		panel_err("out of range mres_mode(%d)\n", mres_mode);
		return -EINVAL;
	}

	resol = &mres->resol[mres_mode];
	if (props->mres_mode == mres_mode) {
		panel_dbg("same resolution(%d:%dx%d)\n",
				mres_mode, resol->w, resol->h);
	} else {
		props->old_mres_mode = props->mres_mode;
		props->mres_mode = mres_mode;
		props->mres_updated = true;
	}

	vrr_idx = panel_display_mode_get_vrr_idx(panel, panel_mode);
	if (vrr_idx < 0) {
		panel_err("failed to get vrr_idx from panel_mode(%d)\n",
				panel_mode);
		return -EINVAL;
	}

	if (vrr_idx >= panel->panel_data.nr_vrrtbl) {
		panel_err("out of range vrr_idx(%d)\n", vrr_idx);
		return -EINVAL;
	}

	vrr = panel->panel_data.vrrtbl[vrr_idx];

	/* keep origin data */
	props->vrr_origin_fps = props->vrr_fps;
	props->vrr_origin_mode = props->vrr_mode;
	props->vrr_origin_idx = props->vrr_idx;

	/* update vrr data */
	props->vrr_fps = vrr->fps;
	props->vrr_mode = vrr->mode;
	props->vrr_idx = vrr_idx;

	panel_info("updated mres(%d:%dx%d) vrr(%d:%d%s,te:%dHz)\n",
			props->mres_mode, resol->w, resol->h,
			props->vrr_idx, props->vrr_fps,
			REFRESH_MODE_STR(props->vrr_mode),
			props->vrr_fps / (vrr->te_hw_skip_count + 1));

	return 0;
}

static int panel_seq_display_mode(struct panel_device *panel)
{
	int ret;

	if (unlikely(!panel)) {
		panel_err("panel_device is null!!\n");
		return -EINVAL;
	}

	if (!panel_display_mode_is_supported(panel)) {
		panel_err("panel_display_mode not supported\n");
		return -EINVAL;
	}

	if (!check_display_mode_cond(panel))
		return -EINVAL;

	ret = panel_do_seqtbl_by_index_nolock(panel,
			PANEL_DISPLAY_MODE_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to do display-mode-seq\n");
		return ret;
	}

	return 0;
}

int panel_set_display_mode_nolock(struct panel_device *panel, int panel_mode)
{
	struct common_panel_display_modes *common_panel_modes;
	struct panel_properties *props;
	int ret = 0;

	if (unlikely(!panel)) {
		panel_err("panel_device is null!!\n");
		return -EINVAL;
	}

	props = &panel->panel_data.props;
	if (!panel_display_mode_is_supported(panel)) {
		panel_err("panel_display_mode not supported\n");
		return -EINVAL;
	}

	common_panel_modes = panel->panel_data.common_panel_modes;
	if (panel_mode < 0 ||
			panel_mode >= common_panel_modes->num_modes) {
		panel_err("invalid panel_mode %d\n", panel_mode);
		return -EINVAL;
	}

	/*
	 * apply panel device dependent display mode
	 */
	ret = panel_update_display_mode_props(panel, panel_mode);
	if (ret < 0) {
		panel_err("failed to update display mode properties\n");
		return ret;
	}

	ret = panel_seq_display_mode(panel);
	if (unlikely(ret < 0)) {
		panel_err("failed to panel_seq_display_mode\n");
		return ret;
	}

	props->vrr_origin_fps = props->vrr_fps;
	props->vrr_origin_mode = props->vrr_mode;
	props->vrr_origin_idx = props->vrr_idx;

	return ret;
}

#ifdef CONFIG_PANEL_VRR_BRIDGE
static int panel_run_vrr_bridge_thread(struct panel_device *panel)
{
	wake_up_interruptible_all(&panel->thread[PANEL_THREAD_VRR_BRIDGE].wait);

	return 0;
}
#endif

static int panel_set_display_mode(struct panel_device *panel, void *arg)
{
	int ret = 0, panel_mode;
	struct panel_display_mode *pdm = arg;
	struct panel_properties *props =
		&panel->panel_data.props;

	if (unlikely(!pdm)) {
		panel_err("panel_display_mode is null\n");
		return -EINVAL;
	}

	mutex_lock(&panel->op_lock);
	panel_mode =
		panel_display_mode_find_panel_mode(panel, pdm);
	if (panel_mode < 0) {
		panel_err("could not find panel_display_mode(%s)\n", pdm->name);
		ret = -EINVAL;
		goto out;
	}

	if (props->panel_mode == panel_mode) {
		panel_info("same panel_mode(%d)\n", panel_mode);
		goto out;
	}

#ifdef CONFIG_PANEL_VRR_BRIDGE
	props->target_panel_mode = panel_mode;
	if (panel_vrr_bridge_changeable(panel) &&
		!panel_display_mode_is_mres_mode_changed(panel, panel_mode)) {
		/* run vrr-bridge thread */
		ret = panel_run_vrr_bridge_thread(panel);
		if (ret < 0)
			panel_err("failed to run vrr-bridge thread\n");
		goto out;
	}
#endif

	ret = panel_set_display_mode_nolock(panel, panel_mode);
	if (ret < 0)
		panel_err("failed to panel_set_display_mode_nolock\n");

out:
	mutex_unlock(&panel->op_lock);

	/* callback to notify current display mode */
	if (!ret)
		panel_display_mode_cb(panel);

	return ret;
}

/**
 * panel_update_display_mode - update display mode
 * @panel: panel device
 *
 * excute DISPLAY_MODE seq with current display mode.
 */
int panel_update_display_mode(struct panel_device *panel)
{
	int ret;
	struct panel_properties *props =
		&panel->panel_data.props;

	mutex_lock(&panel->op_lock);
	ret = panel_set_display_mode_nolock(panel, props->panel_mode);
	if (ret < 0)
		panel_err("failed to panel_set_display_mode_nolock\n");
	mutex_unlock(&panel->op_lock);

	/* callback to notify current display mode */
	panel_display_mode_cb(panel);

	return ret;
}
#endif /* CONFIG_PANEL_DISPLAY_MODE */

#ifdef CONFIG_SUPPORT_DSU
static int panel_set_mres(struct panel_device *panel, int *arg)
{
	int ret = 0;
	int mres_idx;
	struct panel_properties *props;
	struct panel_mres *mres;

	if (unlikely(!panel)) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	props = &panel->panel_data.props;
	mres = &panel->panel_data.mres;
	mres_idx = *arg;

	if (mres->nr_resol == 0 || mres->resol == NULL) {
		panel_err("multi-resolution unsupported!!\n");
		return -EINVAL;
	}

	if (mres_idx >= mres->nr_resol) {
		panel_err("invalid mres idx:%d, number:%d\n",
				mres_idx, mres->nr_resol);
		return -EINVAL;
	}

	props->old_mres_mode = props->mres_mode;
	props->mres_mode = mres_idx;
	props->mres_updated = true;
	ret = panel_do_seqtbl_by_index(panel, PANEL_DSU_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write dsu seqtbl\n");
		goto do_exit;
	}
	props->xres = mres->resol[mres_idx].w;
	props->yres = mres->resol[mres_idx].h;

	return 0;

do_exit:
	return ret;
}
#endif /* CONFIG_SUPPORT_DSU */

#define MAX_DSIM_CNT_FOR_PANEL (MAX_DSIM_CNT)
static int panel_ioctl_dsim_probe(struct v4l2_subdev *sd, void *arg)
{
	int *param = (int *)arg;
	int ret;
	struct panel_device *panel = container_of(sd, struct panel_device, sd);

	panel_info("PANEL_IOC_DSIM_PROBE\n");
	if (param == NULL || *param >= MAX_DSIM_CNT_FOR_PANEL) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}
	panel->dsi_id = *param;

	ret = panel_parse_lcd_info(panel);
	if (ret < 0) {
		panel_err("failed to parse_lcd_info\n");
		return ret;
	}

	panel_info("panel id : %d, dsim id : %d\n",
			panel->id, panel->dsi_id);

	return 0;
}

static int panel_ioctl_dsim_ops(struct v4l2_subdev *sd)
{
	struct mipi_drv_ops *mipi_ops;
	struct panel_device *panel = container_of(sd, struct panel_device, sd);

	mipi_ops = (struct mipi_drv_ops *)v4l2_get_subdev_hostdata(sd);
	if (mipi_ops == NULL) {
		panel_err("mipi_ops is null\n");
		return -EINVAL;
	}
	memcpy(&panel->mipi_drv, mipi_ops, sizeof(struct mipi_drv_ops));

	return 0;
}

static int panel_ioctl_display_on(struct panel_device *panel, void *arg)
{
	int ret = 0;
	int *disp_on = (int *)arg;

	if (disp_on == NULL) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}
	if (*disp_on == 0)
		ret = panel_display_off(panel);
	else
		ret = panel_display_on(panel);

	return ret;
}

static int panel_ioctl_set_power(struct panel_device *panel, void *arg)
{
	int ret = 0;
	int *disp_on = (int *)arg;

	if (disp_on == NULL) {
		panel_err("invalid arg\n");
		return -EINVAL;
	}
	if (*disp_on == 0)
		ret = panel_power_off(panel);
	else
		ret = panel_power_on(panel);

	return ret;
}

static int panel_ioctl_panel_reset(struct panel_device *panel)
{
	struct panel_gpio *gpio = panel->gpio;

	if (panel->panel_data.ddi_props.after_panel_reset == true) {
		usleep_range(15000, 15000 + 10);
		gpio_direction_output(gpio[PANEL_GPIO_RESET].num, 1);
		usleep_range(10000, 10000 + 10);
		panel_info("gpio_reset(%s)\n",
			gpio_get_value(gpio[PANEL_GPIO_RESET].num) ? "high" : "low");
	} else {
		panel_info("already panel reset(%s)\n",
				gpio_get_value(gpio[PANEL_GPIO_RESET].num) ? "high" : "low");
	}

	return 0;
}


static int panel_set_error_cb(struct v4l2_subdev *sd)
{
	struct panel_device *panel = container_of(sd, struct panel_device, sd);
	struct disp_error_cb_info *error_cb_info;

	error_cb_info = (struct disp_error_cb_info *)v4l2_get_subdev_hostdata(sd);
	if (!error_cb_info) {
		panel_err("error error_cb info is null\n");
		return -EINVAL;
	}
	panel->error_cb_info.error_cb = error_cb_info->error_cb;
	panel->error_cb_info.powerdown_cb = error_cb_info->powerdown_cb;
	panel->error_cb_info.data = error_cb_info->data;

	return 0;
}

static int panel_check_cb(void *data)
{
	struct panel_device *panel = data;
	int status = DISP_CHECK_STATUS_OK;

	if (panel_conn_det_state(panel) == PANEL_STATE_NOK)
		status |= DISP_CHECK_STATUS_NODEV;
	if (panel_disp_det_state(panel) == PANEL_STATE_NOK)
		status |= DISP_CHECK_STATUS_ELOFF;
#ifdef CONFIG_SUPPORT_ERRFG_RECOVERY
	if (panel_err_fg_state(panel) == PANEL_STATE_NOK
		&& panel->panel_data.ddi_props.err_fg_powerdown)
		status |= DISP_CHECK_STATUS_NODEV;
#endif
	return status;
}

static int panel_error_cb(struct panel_device *panel)
{
	struct disp_error_cb_info *error_cb_info = &panel->error_cb_info;
	struct disp_check_cb_info panel_check_info = {
		.check_cb = (disp_check_cb *)panel_check_cb,
		.data = panel,
	};
	int ret = 0;

	if (error_cb_info->error_cb) {
		ret = error_cb_info->error_cb((void *)(error_cb_info->data),
				&panel_check_info);
		if (ret)
			panel_err("failed to recovery panel\n");
	}

	return ret;
}

#ifdef CONFIG_SUPPORT_ERRFG_RECOVERY
static int panel_powerdown_cb(struct panel_device *panel)
{
	struct disp_error_cb_info *error_cb_info = &panel->error_cb_info;
	struct disp_check_cb_info panel_check_info = {
		.check_cb = (disp_check_cb *)panel_check_cb,
		.data = panel,
	};
	int ret = 0;

	if (error_cb_info->powerdown_cb) {
		ret = error_cb_info->powerdown_cb((void *)(error_cb_info->data),
				&panel_check_info);
		if (ret)
			panel_err("failed to powerdown panel\n");
	}

	return ret;
}
#endif

#ifdef CONFIG_SUPPORT_MASK_LAYER
static int panel_set_mask_layer(struct panel_device *panel, void *arg)
{
	int ret = 0;
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	struct mask_layer_data *req_data = (struct mask_layer_data *)arg;

	if (!panel->lcd_dev) {
		panel_err("lcd device not exist\n");
		return -EINVAL;
	}

	if (req_data->req_mask_layer == MASK_LAYER_ON) {
		if (req_data->trigger_time  == MASK_LAYER_TRIGGER_BEFORE) {

			/*
			 * W/A - During smooth dimming transition,
			 * Smooth dimming transition should stop here.
			 */

			/* 0. STOP SMOOTH DIMMING */
			mutex_lock(&panel_bl->lock);
			if (check_seqtbl_exist(&panel->panel_data, PANEL_MASK_LAYER_BEFORE_SEQ))
				panel_do_seqtbl_by_index(panel, PANEL_MASK_LAYER_BEFORE_SEQ);
			if (check_seqtbl_exist(&panel->panel_data, PANEL_MASK_LAYER_STOP_DIMMING_SEQ)) {
				panel_bl->props.smooth_transition = SMOOTH_TRANS_OFF;
				panel_do_seqtbl_by_index(panel, PANEL_MASK_LAYER_STOP_DIMMING_SEQ);
			}

			/* 1. REQ ON + FRAME START BEFORE */
			if (check_seqtbl_exist(&panel->panel_data, PANEL_MASK_LAYER_BEFORE_SEQ))
				panel_do_seqtbl_by_index(panel, PANEL_MASK_LAYER_BEFORE_SEQ);
			panel_bl->props.mask_layer_br_hook = MASK_LAYER_HOOK_ON;
			panel_bl->props.smooth_transition = SMOOTH_TRANS_OFF;
			panel_bl->props.acl_opr = ACL_OPR_OFF;
			panel_bl->props.acl_pwrsave = ACL_PWRSAVE_OFF;
			if (check_seqtbl_exist(&panel->panel_data, PANEL_MASK_LAYER_ENTER_BR_SEQ)
				&&(panel->state.cur_state != PANEL_STATE_ALPM)) {
				panel_bl->props.brightness = panel_bl->props.mask_layer_br_target;
				panel_info("mask_layer_br enter (%d)->(%d)\n",
					panel_bl->bd->props.brightness, panel_bl->props.mask_layer_br_target);
				panel_do_seqtbl_by_index(panel, PANEL_MASK_LAYER_ENTER_BR_SEQ);
				mutex_unlock(&panel_bl->lock);
			} else {
				mutex_unlock(&panel_bl->lock);
				panel_update_brightness(panel);
			}
			if (check_seqtbl_exist(&panel->panel_data, PANEL_MASK_LAYER_AFTER_SEQ))
				panel_do_seqtbl_by_index(panel, PANEL_MASK_LAYER_AFTER_SEQ);

		} else if  (req_data->trigger_time  == MASK_LAYER_TRIGGER_AFTER)  {
			/* 2. REQ ON + FRAME START AFTER */
			panel_bl->props.mask_layer_br_actual = panel_bl->props.mask_layer_br_target;
			sysfs_notify(&panel->lcd_dev->kobj, NULL, "actual_mask_brightness");

		}
	} else if (req_data->req_mask_layer == MASK_LAYER_OFF) {
		if (req_data->trigger_time  == MASK_LAYER_TRIGGER_BEFORE) {

			/* 3. REQ OFF + FRAME START BEFORE */
			mutex_lock(&panel_bl->lock);
			if (check_seqtbl_exist(&panel->panel_data, PANEL_MASK_LAYER_BEFORE_SEQ))
				panel_do_seqtbl_by_index(panel, PANEL_MASK_LAYER_BEFORE_SEQ);
			panel_bl->props.mask_layer_br_hook = MASK_LAYER_HOOK_OFF;

			if (check_seqtbl_exist(&panel->panel_data, PANEL_MASK_LAYER_EXIT_BR_SEQ)
				&& (panel->state.cur_state != PANEL_STATE_ALPM)) {
				panel_info("mask_layer_br exit (%d)->(%d)\n",
					panel_bl->props.mask_layer_br_target, panel_bl->bd->props.brightness);
				panel_bl->props.brightness = panel_bl->bd->props.brightness;
				panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_DISP].brightness = panel_bl->props.brightness;
#ifdef CONFIG_SUPPORT_AOD_BL
				panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness = panel_bl->props.brightness;
#endif
				panel_do_seqtbl_by_index(panel, PANEL_MASK_LAYER_EXIT_BR_SEQ);
				mutex_unlock(&panel_bl->lock);
			} else {
				mutex_unlock(&panel_bl->lock);
				panel_update_brightness(panel);
			}
			if (check_seqtbl_exist(&panel->panel_data, PANEL_MASK_LAYER_AFTER_SEQ))
				panel_do_seqtbl_by_index(panel, PANEL_MASK_LAYER_AFTER_SEQ);
		} else if  (req_data->trigger_time  == MASK_LAYER_TRIGGER_AFTER)  {

			/* 4. REQ OFF + FRAME START AFTER */
			panel_bl->props.smooth_transition = SMOOTH_TRANS_ON;
			panel_bl->props.mask_layer_br_actual = 0;
			sysfs_notify(&panel->lcd_dev->kobj, NULL, "actual_mask_brightness");
		}
	}
	return ret;
}
#endif

#ifdef CONFIG_SUPPORT_MAFPC
static int panel_notify_frame_done_mafpc(struct panel_device *panel)
{
	int ret = 0;

	ret = cmd_v4l2_mafpc_dev(panel, V4L2_IOCL_MAFPC_FRAME_DONE, panel);

	return ret;
}
#endif

static long panel_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct panel_device *panel = container_of(sd, struct panel_device, sd);
#ifdef CONFIG_SUPPORT_DSU
	static int mres_updated_frame_cnt;
#endif

	mutex_lock(&panel->io_lock);

	switch (cmd) {
	case PANEL_IOC_DSIM_PROBE:
		ret = panel_ioctl_dsim_probe(sd, arg);
		break;

	case PANEL_IOC_DSIM_PUT_MIPI_OPS:
		panel_info("PANEL_IOC_DSIM_PUT_MIPI_OPS\n");
		ret = panel_ioctl_dsim_ops(sd);
		break;

	case PANEL_IOC_REG_RESET_CB:
		panel_info("PANEL_IOC_REG_PANEL_RESET\n");
		ret = panel_set_error_cb(sd);
		break;

	case PANEL_IOC_REG_VRR_CB:
		panel_info("PANEL_IOC_REG_VRR_CB\n");
		ret = panel_register_vrr_cb(sd);
		break;

	case PANEL_IOC_GET_PANEL_STATE:
		panel_info("PANEL_IOC_GET_PANEL_STATE\n");
		panel->state.connected = panel_conn_det_state(panel);
		//Todo..
		panel_info("connected : %d\n", panel->state.connected);
		panel->state.connected = 1;

		v4l2_set_subdev_hostdata(sd, &panel->state);
		break;

	case PANEL_IOC_PANEL_PROBE:
		panel_info("PANEL_IOC_PANEL_PROBE\n");
		ret = panel_probe(panel);
		break;

	case PANEL_IOC_SLEEP_IN:
		panel_info("PANEL_IOC_SLEEP_IN\n");
		ret = panel_sleep_in(panel);
		break;

	case PANEL_IOC_SLEEP_OUT:
		panel_info("PANEL_IOC_SLEEP_OUT\n");
		ret = panel_sleep_out(panel);
		break;

	case PANEL_IOC_SET_POWER:
		panel_info("PANEL_IOC_SET_POWER\n");
		ret = panel_ioctl_set_power(panel, arg);
		break;

	case PANEL_IOC_PANEL_RESET:
		panel_info("PANEL_IOC_PANEL_RESET\n");
		ret = panel_ioctl_panel_reset(panel);
		break;

	case PANEL_IOC_PANEL_DUMP:
		panel_info("PANEL_IOC_PANEL_DUMP\n");
		ret = panel_debug_dump(panel);
		break;
#ifdef CONFIG_EXYNOS_DOZE
	case PANEL_IOC_DOZE:
	case PANEL_IOC_DOZE_SUSPEND:
		panel_info("PANEL_IOC_%s\n",
			cmd == PANEL_IOC_DOZE ? "DOZE" : "DOZE_SUSPEND");
		ret = panel_doze(panel, cmd);
		break;
#endif
#ifdef CONFIG_SUPPORT_DSU
	case PANEL_IOC_SET_MRES:
		panel_info("PANEL_IOC_SET_MRES\n");
		ret = panel_set_mres(panel, arg);
		break;
#endif
	case PANEL_IOC_GET_MRES:
		panel_info("PANEL_IOC_GET_MRES\n");
		v4l2_set_subdev_hostdata(sd, &panel->panel_data.mres);
		break;

	case PANEL_IOC_SET_VRR_INFO:
		panel_err("PANEL_IOC_SET_VRR_INFO not supported\n");
		break;

#if defined(CONFIG_PANEL_DISPLAY_MODE)
	case PANEL_IOC_REG_DISPLAY_MODE_CB:
		panel_info("PANEL_IOC_REG_DISPLAY_MODE_CB\n");
		ret = panel_register_display_mode_cb(sd);
		break;

	case PANEL_IOC_GET_DISPLAY_MODE:
		panel_info("PANEL_IOC_GET_DISPLAY_MODE\n");
		v4l2_set_subdev_hostdata(sd, panel->panel_modes);
		break;

	case PANEL_IOC_SET_DISPLAY_MODE:
		panel_info("PANEL_IOC_SET_DISPLAY_MODE\n");
		panel_set_display_mode(panel, arg);
		break;
#endif

	case PANEL_IOC_DISP_ON:
		panel_info("PANEL_IOC_DISP_ON\n");
		ret = panel_ioctl_display_on(panel, arg);
		break;

	case PANEL_IOC_EVT_FRAME_DONE:
		if (panel->state.cur_state != PANEL_STATE_NORMAL &&
				panel->state.cur_state != PANEL_STATE_ALPM) {
			panel_warn("FRAME_DONE (panel_state:%s, disp_on:%s)\n",
					panel_state_names[panel->state.cur_state],
					panel->state.disp_on ? "on" : "off");
			break;
		}

		if (unlikely(panel->state.disp_on == PANEL_DISPLAY_OFF)) {
			panel_info("FRAME_DONE (panel_state:%s, display on)\n",
					panel_state_names[panel->state.cur_state]);
			ret = panel_display_on(panel);
			panel_check_ready(panel);
		}
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
		copr_update_start(&panel->copr, 3);
#endif

#ifdef CONFIG_SUPPORT_DSU
		if (panel->panel_data.props.mres_updated &&
				(++mres_updated_frame_cnt > 1)) {
			panel->panel_data.props.mres_updated = false;
			mres_updated_frame_cnt = 0;
		}
#endif
		if (panel->condition_check.is_panel_check)
			panel_check_start(panel);
#ifdef CONFIG_SUPPORT_MAFPC
		panel_notify_frame_done_mafpc(panel);
#endif
		break;
	case PANEL_IOC_EVT_VSYNC:
		panel_info("PANEL_IOC_EVT_VSYNC\n");
		break;

#ifdef CONFIG_SUPPORT_MASK_LAYER
	case PANEL_IOC_SET_MASK_LAYER:
		ret = panel_set_mask_layer(panel, arg);
		break;
#endif

#ifdef CONFIG_DYNAMIC_MIPI
	case PANEL_IOC_DM_GET_INFO:
		v4l2_set_subdev_hostdata(sd, &panel->dynamic_mipi);
		break;

	case PANEL_IOC_DM_SET_PANEL_FFC:
		ret = panel_dm_set_ffc(panel);
		break;

	case PANEL_IOC_DM_OFF_PANEL_FFC:
		ret = panel_dm_off_ffc(panel);
		break;
#endif
	default:
		panel_err("undefined command\n");
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		panel_err("failed to ioctl panel cmd : %d\n",
				_IOC_NR(cmd));
	}
	mutex_unlock(&panel->io_lock);

	return (long)ret;
}

static const struct v4l2_subdev_core_ops panel_v4l2_sd_core_ops = {
	.ioctl = panel_core_ioctl,
};

static const struct v4l2_subdev_ops panel_subdev_ops = {
	.core = &panel_v4l2_sd_core_ops,
};

void panel_init_v4l2_subdev(struct panel_device *panel)
{
	struct v4l2_subdev *sd = &panel->sd;

	v4l2_subdev_init(sd, &panel_subdev_ops);
	sd->owner = THIS_MODULE;
	sd->grp_id = 0;
	snprintf(sd->name, sizeof(sd->name), "%s.%d", "panel-sd", panel->id);
	v4l2_set_subdevdata(sd, panel);
}

int panel_drv_set_gpios(struct panel_device *panel)
{
	int rst_val = -1, det_val = -1;
	struct panel_gpio *gpio = panel->gpio;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (!gpio_is_valid(gpio[PANEL_GPIO_RESET].num)) {
		panel_err("gpio(%s) not exist\n",
				panel_gpio_names[PANEL_GPIO_RESET]);
		return -EINVAL;
	}
	rst_val = gpio_get_value(gpio[PANEL_GPIO_RESET].num);

	if (!gpio_is_valid(gpio[PANEL_GPIO_DISP_DET].num)) {
		panel_err("gpio(%s) not exist\n",
				panel_gpio_names[PANEL_GPIO_DISP_DET]);
		return -EINVAL;
	}
	det_val = gpio_get_value(gpio[PANEL_GPIO_DISP_DET].num);

	/*
	 * panel state is decided by rst, conn_det and disp_det pin
	 *
	 * @rst_val
	 *  0 : need to init panel in kernel
	 *  1 : already initialized in bootloader
	 *
	 * @conn_det
	 *  < 0 : unsupported
	 *  = 0 : ub connector is disconnected
	 *  = 1 : ub connector is connected
	 *
	 * @det_val
	 *  0 : panel is "sleep in" state
	 *  1 : panel is "sleep out" state
	 */
	panel->state.init_at = (rst_val == 1) ?
		PANEL_INIT_BOOT : PANEL_INIT_KERNEL;


	panel->state.connected = panel_conn_det_state(panel);

	/* connect_panel : decide to use or ignore panel */
	if ((panel->state.init_at == PANEL_INIT_BOOT) &&
		(panel->state.connected != 0) && (det_val == 1)) {
		/*
		 * connect panel condition
		 * conn_det is normal(not zero)
		 * disp_det is nomal(1)
		 * init panel in bootloader(rst == 1)
		 */
		panel->state.connect_panel = PANEL_CONNECT;
		panel_set_cur_state(panel, PANEL_STATE_NORMAL);
		panel->state.power = PANEL_POWER_ON;
		panel->state.disp_on = PANEL_DISPLAY_ON;
		gpio_direction_output(gpio[PANEL_GPIO_RESET].num, 1);
	} else {
		panel->state.connect_panel = PANEL_DISCONNECT;
		panel_set_cur_state(panel, PANEL_STATE_OFF);
		panel->state.power = PANEL_POWER_OFF;
		panel->state.disp_on = PANEL_DISPLAY_OFF;
		gpio_direction_output(gpio[PANEL_GPIO_RESET].num, 0);
	}
#ifdef CONFIG_PANEL_NOTIFY
	panel_send_ubconn_notify(panel->state.connected == PANEL_STATE_OK ?
		PANEL_EVENT_UB_CON_CONNECTED : PANEL_EVENT_UB_CON_DISCONNECTED);
#endif
	panel_info("rst:%d, disp_det:%d (init_at:%s, ub_con:%d(%s) panel:%d(%s))\n",
			rst_val, det_val, (panel->state.init_at ? "BL" : "KERNEL"),
			panel->state.connected,
			(panel->state.connected < 0 ? "UNSUPPORTED" :
			 (panel->state.connected == true ? "CONNECTED" : "DISCONNECTED")),
			panel->state.connect_panel,
			(panel->state.connect_panel == PANEL_CONNECT ? "USE" : "NO USE"));

	return 0;
}

int panel_drv_set_regulators(struct panel_device *panel)
{
	int ret;

	if (panel->state.init_at == PANEL_INIT_BOOT) {
		ret = panel_regulator_set_short_detection(panel, PANEL_STATE_NORMAL);
		if (ret < 0)
			panel_err("failed to set ssd current, ret:%d\n", ret);

		ret = panel_regulator_set_voltage(panel, PANEL_STATE_NORMAL);
		if (ret < 0)
			panel_err("failed to set voltage\n");

		ret = panel_regulator_enable(panel);
		if (ret < 0) {
			panel_err("failed to panel_regulator_enable, ret:%d\n", ret);
			return ret;
		}

		if (panel->state.connect_panel == PANEL_DISCONNECT) {
			ret = panel_regulator_disable(panel);
			if (ret < 0) {
				panel_err("failed to panel_regulator_disable, ret:%d\n", ret);
				return ret;
			}
		}
	}

	return 0;
}

#if !defined(CONFIG_UML)
static int panel_parse_pinctrl(struct panel_device *panel)
{
	int ret = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel->pinctrl = devm_pinctrl_get(panel->dev);
	if (IS_ERR(panel->pinctrl)) {
		panel_err("failed to get device's pinctrl\n");
		goto exit_parse_pinctrl;
	}

	panel->default_gpio_pinctrl = pinctrl_lookup_state(panel->pinctrl, "default");
	if (IS_ERR(panel->default_gpio_pinctrl)) {
		panel_err("can't get default pinctrl setting\n");
		goto exit_parse_pinctrl;
	}

	if (pinctrl_select_state(panel->pinctrl, panel->default_gpio_pinctrl)) {
		panel_err("%s failed to set default pinctrl\n");
		goto exit_parse_pinctrl;
	}

	panel_info("pinctrl setting done\n");

exit_parse_pinctrl:
	return ret;
}
#endif

static int of_get_panel_gpio(struct device_node *np, struct panel_gpio *gpio)
{
	struct device_node *pend_np;
	enum of_gpio_flags flags;
	int ret;

	if (of_gpio_count(np) < 1)
		return -ENODEV;

	if (of_property_read_string(np, "name",
			(const char **)&gpio->name)) {
		panel_err("%s:property('name') not found\n", np->name);
		return -EINVAL;
	}

	gpio->num = of_get_gpio_flags(np, 0, &flags);
	if (!gpio_is_valid(gpio->num)) {
		panel_err("%s:invalid gpio %s:%d\n", np->name, gpio->name, gpio->num);
		return -ENODEV;
	}
	gpio->active_low = flags & OF_GPIO_ACTIVE_LOW;

	if (of_property_read_u32(np, "dir", &gpio->dir))
		panel_warn("%s:property('dir') not found\n", np->name);

	if ((gpio->dir & GPIOF_DIR_IN) == GPIOF_DIR_OUT) {
		ret = gpio_request(gpio->num, gpio->name);
		if (ret < 0) {
			panel_err("failed to request gpio(%s:%d)\n", gpio->name, gpio->num);
			return ret;
		}
	} else {
		ret = gpio_request_one(gpio->num, GPIOF_IN, gpio->name);
		if (ret < 0) {
			panel_err("failed to request gpio(%s:%d)\n", gpio->name, gpio->num);
			return ret;
		}
	}

	if (of_property_read_u32(np, "irq-type", &gpio->irq_type))
		panel_warn("%s:property('irq-type') not found\n", np->name);

	if (gpio->irq_type > 0) {
		gpio->irq = gpio_to_irq(gpio->num);

		pend_np = of_get_child_by_name(np, "irq-pend");
		if (pend_np) {
			gpio->irq_pend_reg = of_iomap(pend_np, 0);
			if (gpio->irq_pend_reg == NULL)
				panel_err("%s:%s:property('reg') not found\n", np->name, pend_np->name);

			if (of_property_read_u32(pend_np, "bit",
						&gpio->irq_pend_bit)) {
				panel_err("%s:%s:property('bit') not found\n", np->name, pend_np->name);
				gpio->irq_pend_bit = -EINVAL;
			}
			of_node_put(pend_np);
		}
	}
	gpio->irq_enable = false;
	return 0;
}

static int panel_parse_gpio(struct panel_device *panel)
{
	struct device *dev = panel->dev;
	struct device_node *gpios_np, *np;
	struct panel_gpio *gpio = panel->gpio;
	int i;

	for (i = 0; i < PANEL_GPIO_MAX; i++)
		gpio[i].num = -1;

	gpios_np = of_get_child_by_name(dev->of_node, "gpios");
	if (!gpios_np) {
		panel_err("'gpios' node not found\n");
		return -EINVAL;
	}

	for_each_child_of_node(gpios_np, np) {
		for (i = 0; i < PANEL_GPIO_MAX; i++)
			if (!of_node_cmp(np->name,
						panel_gpio_names[i]))
				break;

		if (i == PANEL_GPIO_MAX) {
			panel_warn("%s not found in panel_gpio list\n", np->name);
			continue;
		}

		if (of_get_panel_gpio(np, &gpio[i]))
			panel_err("failed to get gpio %s\n", np->name);

		panel_info("gpio[%d] num:%d name:%s active:%s dir:%d irq_type:%d\n",
				i, gpio[i].num, gpio[i].name, gpio[i].active_low ? "low" : "high",
				gpio[i].dir, gpio[i].irq_type);
	}
	of_node_put(gpios_np);

	return 0;
}

static int of_get_panel_regulator(struct device_node *np,
		struct panel_regulator *regulator)
{
	struct device_node *reg_np;

	reg_np = of_parse_phandle(np, "regulator", 0);
	if (!reg_np) {
		panel_err("%s:'regulator' node not found\n", np->name);
		return -EINVAL;
	}

	if (of_property_read_string(reg_np, "regulator-name",
				(const char **)&regulator->name)) {
		panel_err("%s:%s:property('regulator-name') not found\n",
				np->name, reg_np->name);
		of_node_put(reg_np);
		return -EINVAL;
	}
	of_node_put(reg_np);

	regulator->reg = regulator_get(NULL, regulator->name);
	if (IS_ERR(regulator->reg)) {
		panel_err("failed to get regulator %s\n", regulator->name);
		return -EINVAL;
	}

	of_property_read_u32(np, "def-voltage", &regulator->def_voltage);
	of_property_read_u32(np, "lpm-voltage", &regulator->lpm_voltage);
	of_property_read_u32(np, "from-off", &regulator->from_off_current);
	of_property_read_u32(np, "from-lpm", &regulator->from_lpm_current);

	return 0;
}

static int panel_parse_regulator(struct panel_device *panel)
{
	int i;
	struct device *dev = panel->dev;
	struct panel_regulator *regulator = panel->regulator;
	struct device_node *regulators_np, *np;

	regulators_np = of_get_child_by_name(dev->of_node, "regulators");
	if (!regulators_np) {
		panel_err("'regulators' node not found\n");
		return -EINVAL;
	}

	for_each_child_of_node(regulators_np, np) {
		for (i = 0; i < PANEL_REGULATOR_MAX; i++)
			if (!of_node_cmp(np->name,
					panel_regulator_names[i]))
				break;

		if (i == PANEL_REGULATOR_MAX) {
			panel_warn("%s not found in panel_regulator list\n", np->name);
			continue;
		}

		if (i == PANEL_REGULATOR_SSD)
			regulator[i].type = PANEL_REGULATOR_TYPE_SSD;
		else
			regulator[i].type = PANEL_REGULATOR_TYPE_PWR;

		if (of_get_panel_regulator(np, &regulator[i])) {
			panel_err("failed to get regulator %s\n", np->name);
			of_node_put(regulators_np);
			return -EINVAL;
		}
		panel_info("regulator[%d] name:%s type:%d\n",
				i, regulator[i].name, regulator[i].type);
	}
	of_node_put(regulators_np);

	panel_dbg("done\n");

	return 0;
}

static irqreturn_t panel_work_isr(int irq, void *dev_id)
{
	struct panel_work *w = (struct panel_work *)dev_id;

	queue_delayed_work(w->wq, &w->dwork, msecs_to_jiffies(0));

	return IRQ_HANDLED;
}

int panel_register_isr(struct panel_device *panel)
{
	int i, iw, ret;
	struct panel_gpio *gpio = panel->gpio;
	char *name = NULL;

	if (panel == NULL) {
		panel_err("panel is NULL\n");
		return -EINVAL;
	}

	if (panel->dev == NULL) {
		panel_err("panel->dev is NULL\n");
		return -ENODEV;
	}

	if (panel->state.connect_panel == PANEL_DISCONNECT) {
		panel_err("panel is disconnected\n");
		return -ENODEV;
	}

	for (i = 0; i < PANEL_GPIO_MAX; i++) {
		if (!panel_gpio_valid(&gpio[i]) || gpio[i].irq_type <= 0)
			continue;

		for (iw = 0; iw < PANEL_WORK_MAX; iw++) {
			if (!strncmp(panel_gpio_names[i],
					panel_work_names[iw], 32))
				break;
		}

		if (iw == PANEL_WORK_MAX)
			continue;
		name = kzalloc(sizeof(char) * 64, GFP_KERNEL);
		if (!name) {
			panel_err("failed to alloc name buffer(%d)\n", iw);
			ret = -ENOMEM;
			break;
		}

		snprintf(name, 64, "panel%d:%s",
				panel->id, panel_work_names[iw]);

		/* W/A: clear pending irq before request_irq */
		irq_set_irq_type(gpio[i].irq, gpio[i].irq_type);

		ret = devm_request_irq(panel->dev, gpio[i].irq, panel_work_isr,
				gpio[i].irq_type, name, &panel->work[iw]);
		if (ret < 0) {
			panel_err("failed to register irq(%s:%d)\n", name, gpio[i].irq);
			return ret;
		}
		gpio[i].irq_enable = true;
	}

	return 0;
}

int __mockable panel_parse_lcd_info(struct panel_device *panel)
{
	struct device_node *node;
	struct device *dev = panel->dev;
	EXYNOS_PANEL_INFO *lcd_info;

	if (!dev)
		return -EINVAL;

	if (!panel->mipi_drv.get_lcd_info) {
		panel_err("get_lcd_info not exist\n");
		return -EINVAL;
	}

	lcd_info = panel->mipi_drv.get_lcd_info(panel->dsi_id);
	if (!lcd_info) {
		panel_err("failed to get lcd_info\n");
		return -EINVAL;
	}
	panel_info("PANEL_INFO:panel id : %x\n", boot_panel_id);

#if defined(CONFIG_PANEL_DISPLAY_MODE)
	node = find_panel_modes_node(panel, boot_panel_id);
	if (node != NULL) {
		panel->panel_modes =
			of_get_panel_display_modes(node);
		if (panel->panel_modes == NULL)
			panel_warn("failed to get panel_modes\n");
		else
			panel_info("get panel display modes(%d)\n",
					panel->panel_modes->num_modes);
	} else {
		panel_warn("panel modes not found (boot_panel_id 0x%08X)\n", boot_panel_id);
	}
#endif

	node = find_panel_ddi_node(panel, boot_panel_id);
	if (!node) {
		panel_err("panel not found (boot_panel_id 0x%08X)\n", boot_panel_id);
		node = of_parse_phandle(dev->of_node, "ddi-info", 0);
		if (!node) {
			panel_err("failed to get phandle of ddi-info\n");
			return -EINVAL;
		}
	}
	panel->ddi_node = node;

	if (!panel->mipi_drv.parse_dt) {
		panel_err("parse_dt not exist\n");
		return -EINVAL;
	}

	panel->mipi_drv.parse_dt(node, lcd_info);

	return 0;
}

static int panel_parse_panel_lookup(struct panel_device *panel)
{
	struct device *dev = panel->dev;
	struct panel_info *panel_data = &panel->panel_data;
	struct panel_lut_info *lut_info = &panel_data->lut_info;
	struct device_node *np;
	int ret, i, sz, sz_lut;

	np = of_get_child_by_name(dev->of_node, "panel-lookup");
	if (unlikely(!np)) {
		panel_warn("No DT node for panel-lookup\n");
		return -EINVAL;
	}

	sz = of_property_count_strings(np, "panel-name");
	if (sz <= 0) {
		panel_warn("No panel-name property\n");
		return -EINVAL;
	}

	if (sz >= ARRAY_SIZE(lut_info->names)) {
		panel_warn("exceeded MAX_PANEL size\n");
		return -EINVAL;
	}

	for (i = 0; i < sz; i++) {
		ret = of_property_read_string_index(np,
				"panel-name", i, &lut_info->names[i]);
		if (ret) {
			panel_warn("failed to read panel-name[%d]\n", i);
			return -EINVAL;
		}
	}
	lut_info->nr_panel = sz;

	/* parse panel-ddi-info */
	sz = of_property_count_u32_elems(np, "panel-ddi-info");
	if (sz <= 0) {
		panel_warn("No ddi-info property\n");
		return -EINVAL;
	}

	if (sz >= ARRAY_SIZE(lut_info->ddi_node)) {
		panel_warn("exceeded MAX_PANEL_DDI size\n");
		return -EINVAL;
	}

	for (i = 0; i < sz; i++) {
		lut_info->ddi_node[i] = of_parse_phandle(np, "panel-ddi-info", i);
		if (!lut_info->ddi_node[i]) {
			panel_warn("failed to get phandle of ddi-info[%d]\n", i);
			return -EINVAL;
		}
	}
	lut_info->nr_panel_ddi = sz;

	/* parse panel-display-modes */
	sz = of_property_count_u32_elems(np, "panel-display-modes");
	if (sz > 0) {
		if (sz >= ARRAY_SIZE(lut_info->panel_modes_node)) {
			panel_warn("%d exceed size of panel_modes_node\n", sz);
			return -EINVAL;
		}

		for (i = 0; i < sz; i++) {
			lut_info->panel_modes_node[i] =
				of_parse_phandle(np, "panel-display-modes", i);
			if (!lut_info->panel_modes_node[i]) {
				panel_warn("failed to get phandle of panel-display-modes %d\n", i);
				return -EINVAL;
			}
		}
		lut_info->nr_panel_modes = sz;
	} else {
		panel_warn("could not find panel-display-modes property\n");
	}

	/* parse panel-lut */
	sz_lut = of_property_count_u32_elems(np, "panel-lut");
	if (sz_lut >= MAX_PANEL_LUT) {
		panel_warn("sz_lut(%d) exceeded MAX_PANEL_LUT(%d)\n",
				sz_lut, MAX_PANEL_LUT);
		return -EINVAL;
	}

	if (sz_lut % 4) {
		panel_warn("sz_lut(%d) not aligned by 4\n", sz_lut);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, "panel-lut",
			(u32 *)lut_info->lut, sz_lut);
	if (ret) {
		panel_warn("failed to read panel-lut\n");
		return -EINVAL;
	}

	for (i = 0; i < sz_lut / 4; i++) {
		if (lut_info->lut[i].index >= lut_info->nr_panel) {
			panel_warn("invalid panel index(%d)\n", lut_info->lut[i].index);
			return -EINVAL;
		}
	}
	lut_info->nr_lut = sz_lut / 4;

	print_panel_lut(lut_info);

	return 0;
}

int __mockable panel_parse_dt(struct panel_device *panel)
{
	struct device *dev;
	int ret = 0;

	if (panel == NULL)
		return -EINVAL;

	dev = panel->dev;
	if (dev == NULL)
		return -ENODEV;

	if (IS_ERR_OR_NULL(dev->of_node)) {
		panel_err("failed to get dt info\n");
		return -EINVAL;
	}

	if (of_property_read_u32(dev->of_node, "panel,id", &panel->id))
		panel_err("Invalid panel's id : %d\n", panel->id);
	panel_dbg("panel-id: %d\n", panel->id);

#if !defined(CONFIG_UML)
	ret = panel_parse_pinctrl(panel);
	if (ret < 0) {
		panel_err("panel-%d:failed to parse pinctrl\n", panel->id);
		return ret;
	}
#endif

	ret = panel_parse_gpio(panel);
	if (ret < 0) {
		panel_err("panel-%d:failed to parse gpio\n", panel->id);
		return ret;
	}

	ret = panel_parse_regulator(panel);
	if (ret < 0) {
		panel_err("panel-%d:failed to parse regulator\n", panel->id);
		return ret;
	}

	ret = panel_parse_panel_lookup(panel);
	if (ret < 0) {
		panel_err("panel-%d:failed to parse panel lookup\n", panel->id);
		return ret;
	}

	return ret;
}

static void disp_det_handler(struct work_struct *work)
{
	int ret, disp_det_state;
	struct panel_work *w = container_of(to_delayed_work(work),
			struct panel_work, dwork);
	struct panel_device *panel =
		container_of(w, struct panel_device, work[PANEL_WORK_DISP_DET]);
	struct panel_gpio *gpio = panel->gpio;
	struct panel_state *state = &panel->state;

	disp_det_state = panel_disp_det_state(panel);
	panel_info("disp_det_state:%s\n",
			disp_det_state == PANEL_STATE_OK ? "OK" : "NOK");

	switch (state->cur_state) {
	case PANEL_STATE_ALPM:
	case PANEL_STATE_NORMAL:
		if (disp_det_state == PANEL_STATE_NOK) {

			ret = panel_set_gpio_irq(&gpio[PANEL_GPIO_DISP_DET], false);
			if (ret < 0)
				panel_warn("do not support irq\n");

			/* delay for disp_det deboundce */
			usleep_range(10000, 11000);

			panel_err("disp_det is abnormal state\n");
			ret = panel_error_cb(panel);

			if (ret)
				panel_err("failed to recover panel\n");

			if (panel_get_gpio_irq(&gpio[PANEL_GPIO_DISP_DET]) == false) {
				ret = panel_set_gpio_irq(&gpio[PANEL_GPIO_DISP_DET], true);
				if (ret < 0)
					panel_warn("do not support irq\n");
			}
		}
		break;
	default:
		break;
	}
}

void panel_send_ubconn_uevent(struct panel_device *panel)
{
	char *uevent_conn_str[3] = {
		"CONNECTOR_NAME=UB_CONNECT",
		"CONNECTOR_TYPE=HIGH_LEVEL",
		NULL,
	};

	if (!panel->lcd_dev)
		return;

	kobject_uevent_env(&panel->lcd_dev->kobj, KOBJ_CHANGE, uevent_conn_str);
	panel_info("%s, %s\n", uevent_conn_str[0], uevent_conn_str[1]);
}

#if IS_ENABLED(CONFIG_SEC_ABC)
void panel_send_ubconn_to_abc(struct panel_device *panel)
{
	sec_abc_send_event("MODULE=ub_main@ERROR=ub_disconnected");
	panel_info("done\n");
}
#endif

void conn_det_handler(struct work_struct *data)
{
	struct panel_work *w = container_of(to_delayed_work(data),
		struct panel_work, dwork);
	struct panel_device *panel =
		container_of(w, struct panel_device, work[PANEL_WORK_CONN_DET]);
	bool is_disconnected;

	is_disconnected = ub_con_disconnected(panel);
	panel_info("state:%d cnt:%d\n", is_disconnected, panel->panel_data.props.ub_con_cnt);

#ifdef CONFIG_PANEL_NOTIFY
	panel_send_ubconn_notify((is_disconnected ?
		PANEL_EVENT_UB_CON_DISCONNECTED : PANEL_EVENT_UB_CON_CONNECTED));
#endif

	if (!is_disconnected)
		return;

	if (panel->panel_data.props.conn_det_enable) {
		panel_send_ubconn_uevent(panel);
#if IS_ENABLED(CONFIG_SEC_ABC)
		panel_send_ubconn_to_abc(panel);
#endif
	}
	panel->panel_data.props.ub_con_cnt++;
}

void err_fg_handler(struct work_struct *data)
{
#ifdef CONFIG_SUPPORT_ERRFG_RECOVERY
	int ret, err_fg_state;
	bool err_fg_recovery = false, err_fg_powerdown = false;
	struct panel_work *w = container_of(to_delayed_work(data),
			struct panel_work, dwork);
	struct panel_device *panel =
		container_of(w, struct panel_device, work[PANEL_WORK_ERR_FG]);
	struct panel_gpio *gpio = panel->gpio;
	struct panel_state *state = &panel->state;

	err_fg_recovery = panel->panel_data.ddi_props.err_fg_recovery;
	err_fg_powerdown = panel->panel_data.ddi_props.err_fg_powerdown;

	err_fg_state = panel_err_fg_state(panel);
	panel_info("err_fg_state:%s recover:%s powerdown: %s\n",
			err_fg_state == PANEL_STATE_OK ? "OK" : "NOK",
			err_fg_recovery ? "true" : "false",
			err_fg_powerdown ? "true" : "false");

	if (!(err_fg_recovery || err_fg_powerdown))
		return;

	switch (state->cur_state) {
	case PANEL_STATE_ALPM:
	case PANEL_STATE_NORMAL:
		if (err_fg_state == PANEL_STATE_NOK) {
			ret = panel_set_gpio_irq(&gpio[PANEL_GPIO_ERR_FG], false);
			if (ret < 0)
				panel_warn("do not support irq\n");

			/* delay for disp_det deboundce */
			usleep_range(10000, 11000);
			if (err_fg_powerdown) {
				panel_err("powerdown: err_fg is abnormal state\n");
#ifdef CONFIG_PANEL_NOTIFY
				panel_send_ubconn_notify(PANEL_EVENT_UB_CON_DISCONNECTED);
#endif
				ret = panel_powerdown_cb(panel);
				if (ret)
					panel_err("failed to powerdown_cb\n");
#ifdef CONFIG_PANEL_NOTIFY
				panel_send_ubconn_notify(PANEL_EVENT_UB_CON_CONNECTED);
#endif
			} else if (err_fg_recovery) {
				panel_err("recovery: err_fg is abnormal state\n");
				ret = panel_error_cb(panel);
				if (ret)
					panel_err("failed to recover_cb\n");
			}
			ret = panel_set_gpio_irq(&gpio[PANEL_GPIO_ERR_FG], true);
			if (ret < 0)
				panel_warn("do not support irq\n");
		}
		break;
	default:
		break;
	}
#endif
	panel_info("done\n");
}

static int panel_fb_notifier(struct notifier_block *self, unsigned long event, void *data)
{
#if 0
	int *blank = NULL;
	struct panel_device *panel;

	struct fb_event *fb_event = data;

	switch (event) {
	case FB_EARLY_EVENT_BLANK:
	case FB_EVENT_BLANK:
		break;
	case FB_EVENT_FB_REGISTERED:
		panel_dbg("FB Registeted\n");
		return 0;
	default:
		return 0;
	}

	panel = container_of(self, struct panel_device, fb_notif);
	blank = fb_event->data;
	if (!blank || !panel) {
		panel_err("blank is null\n");
		return 0;
	}

	switch (*blank) {
	case FB_BLANK_POWERDOWN:
	case FB_BLANK_NORMAL:
		if (event == FB_EARLY_EVENT_BLANK)
			panel_dbg("EARLY BLANK POWER DOWN\n");
		else
			panel_dbg("BLANK POWER DOWN\n");
		break;
	case FB_BLANK_UNBLANK:
		if (event == FB_EARLY_EVENT_BLANK)
			panel_dbg("EARLY UNBLANK\n");
		else
			panel_dbg("UNBLANK\n");
		break;
	}
#endif
	return 0;
}

#ifdef CONFIG_DISPLAY_USE_INFO
unsigned int g_rddpm = 0xFF;
unsigned int g_rddsm = 0xFF;

unsigned int get_panel_bigdata(void)
{
	unsigned int val = 0;

	val = (g_rddsm << 8) | g_rddpm;

	return val;
}

static int panel_dpui_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	struct common_panel_info *info;
	int panel_id;
	struct dpui_info *dpui = data;
	char tbuf[MAX_DPUI_VAL_LEN];
	u8 panel_datetime[7] = { 0, };
	u8 panel_coord[4] = { 0, };
	int i, site, rework, poc;
	u8 cell_id[16], octa_id[PANEL_OCTA_ID_LEN] = { 0, };
	bool cell_id_exist = true;
	int size;

	if (dpui == NULL) {
		panel_err("dpui is null\n");
		return 0;
	}

	panel = container_of(self, struct panel_device, panel_dpui_notif);
	panel_data = &panel->panel_data;
	panel_id = panel_data->id[0] << 16 | panel_data->id[1] << 8 | panel_data->id[2];

	info = find_panel(panel, panel_id);
	if (unlikely(!info)) {
		panel_err("panel not found\n");
		return -ENODEV;
	}

	resource_copy_by_name(panel_data, panel_datetime, "date");
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%04d%02d%02d %02d%02d%02d",
			((panel_datetime[0] & 0xF0) >> 4) + 2011, panel_datetime[0] & 0xF, panel_datetime[1] & 0x1F,
			panel_datetime[2] & 0x1F, panel_datetime[3] & 0x3F, panel_datetime[4] & 0x3F);
	set_dpui_field(DPUI_KEY_MAID_DATE, tbuf, size);

	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", panel_data->id[0]);
	set_dpui_field(DPUI_KEY_LCDID1, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", panel_data->id[1]);
	set_dpui_field(DPUI_KEY_LCDID2, tbuf, size);
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d", panel_data->id[2]);
	set_dpui_field(DPUI_KEY_LCDID3, tbuf, size);

	resource_copy_by_name(panel_data, panel_coord, "coordinate");
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
		panel_datetime[0], panel_datetime[1], panel_datetime[2], panel_datetime[3],
		panel_datetime[4], panel_datetime[5], panel_datetime[6],
		panel_coord[0], panel_coord[1], panel_coord[2], panel_coord[3]);
	set_dpui_field(DPUI_KEY_CELLID, tbuf, size);

	/* OCTAID */
	resource_copy_by_name(panel_data, octa_id, "octa_id");
	site = (octa_id[0] >> 4) & 0x0F;
	rework = octa_id[0] & 0x0F;
	poc = octa_id[1] & 0x0F;

	for (i = 0; i < 16; i++) {
		cell_id[i] = isalnum(octa_id[i + 4]) ? octa_id[i + 4] : '\0';
		if (cell_id[i] == '\0') {
			cell_id_exist = false;
			break;
		}
	}
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN, "%d%d%d%02x%02x",
			site, rework, poc, octa_id[2], octa_id[3]);
	if (cell_id_exist) {
		for (i = 0; i < 16; i++)
			size += snprintf(tbuf + size, MAX_DPUI_VAL_LEN - size, "%c", cell_id[i]);
	}
	set_dpui_field(DPUI_KEY_OCTAID, tbuf, size);

#ifdef CONFIG_SUPPORT_DIM_FLASH
	size = snprintf(tbuf, MAX_DPUI_VAL_LEN,
			"%d", panel->work[PANEL_WORK_DIM_FLASH].ret);
	set_dpui_field(DPUI_KEY_PNGFLS, tbuf, size);
#endif
	inc_dpui_u32_field(DPUI_KEY_UB_CON, panel->panel_data.props.ub_con_cnt);
	panel->panel_data.props.ub_con_cnt = 0;

	return 0;
}
#endif /* CONFIG_DISPLAY_USE_INFO */

#ifdef CONFIG_SUPPORT_TDMB_TUNE
static int panel_tdmb_notifier_callback(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	struct tdmb_notifier_struct *value = data;
	int ret;

	panel = container_of(nb, struct panel_device, tdmb_notif);
	panel_data = &panel->panel_data;

	mutex_lock(&panel->io_lock);
	mutex_lock(&panel->op_lock);
	switch (value->event) {
	case TDMB_NOTIFY_EVENT_TUNNER:
		panel_data->props.tdmb_on = value->tdmb_status.pwr;
		if (!IS_PANEL_ACTIVE(panel)) {
			panel_info("keep tdmb state (%s) and affect later\n",
					panel_data->props.tdmb_on ? "on" : "off");
			break;
		}
		panel_info("tdmb state (%s)\n",
				panel_data->props.tdmb_on ? "on" : "off");
		ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_TDMB_TUNE_SEQ);
		if (unlikely(ret < 0))
			panel_err("failed to write tdmb-tune seqtbl\n");
		panel_data->props.cur_tdmb_on = panel_data->props.tdmb_on;
		break;
	default:
		break;
	}
	mutex_unlock(&panel->op_lock);
	mutex_unlock(&panel->io_lock);
	return 0;
}
#endif

//Todo..
#if 0
#if defined(CONFIG_INPUT_TOUCHSCREEN)
static int panel_input_notifier_callback(struct notifier_block *nb,
		unsigned long data, void *v)
{
	struct panel_device *panel =
		container_of(nb, struct panel_device, input_notif);
	struct panel_info *panel_data = &panel->panel_data;

	switch (data) {
	case NOTIFIER_LCD_VRR_LFD_LOCK_REQUEST:	/* set LFD min lock */
	case NOTIFIER_LCD_VRR_LFD_LOCK_RELEASE: /* unset LFD min lock */
		/* TODO : input scalability need to be passed by dtsi or panel config */
		if (panel_vrr_is_supported(panel)) {
			panel_data->props.vrr_lfd_info.req[VRR_LFD_CLIENT_INPUT][VRR_LFD_SCOPE_NORMAL].scalability =
				(data == NOTIFIER_LCD_VRR_LFD_LOCK_REQUEST) ?
				VRR_LFD_SCALABILITY_2 : VRR_LFD_SCALABILITY_NONE;
			queue_delayed_work(panel->work[PANEL_WORK_UPDATE].wq,
					&panel->work[PANEL_WORK_UPDATE].dwork, msecs_to_jiffies(0));
		}
		break;
	}
	return 0;
}
#endif
#endif

#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
int panel_fast_discharge_set(struct panel_device *panel)
{
	int ret = 0;

	mutex_lock(&panel->io_lock);
	mutex_lock(&panel->op_lock);

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_FD_SEQ);
	if (unlikely(ret < 0))
		panel_err("failed to write fast discharge seqtbl\n");

	mutex_unlock(&panel->op_lock);
	mutex_unlock(&panel->io_lock);
	return ret;
}
#endif

static int panel_init_work(struct panel_work *w,
		char *name, panel_wq_handler handler)
{
	if (w == NULL || name == NULL || handler == NULL) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}

	mutex_init(&w->lock);
	INIT_DELAYED_WORK(&w->dwork, handler);
	w->wq = create_singlethread_workqueue(name);
	if (w->wq == NULL) {
		panel_err("failed to create %s workqueue\n", name);
		return -ENOMEM;
	}
	atomic_set(&w->running, 0);
	panel_info("%s:done\n", name);

	return 0;
}

static int panel_drv_init_work(struct panel_device *panel)
{
	int i, ret;

	for (i = 0; i < PANEL_WORK_MAX; i++) {
		if (!panel_wq_handlers[i])
			continue;

		ret = panel_init_work(&panel->work[i],
				panel_work_names[i], panel_wq_handlers[i]);
		if (ret < 0) {
			panel_err("failed to initialize panel_work(%s)\n",
					panel_work_names[i]);
			return ret;
		}
	}

	return 0;
}

static int panel_create_thread(struct panel_device *panel)
{
	size_t i;

	if (unlikely(!panel)) {
		panel_warn("panel is null\n");
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(panel->thread); i++) {
		if (panel_thread_fns[i] == NULL)
			continue;

		panel_info("%s-%d:init\n",
				panel_thread_names[i], panel->id);
		panel->thread[i].should_stop = false;
		init_waitqueue_head(&panel->thread[i].wait);
		panel->thread[i].thread =
			kthread_run(panel_thread_fns[i], panel,
					"%s-%d", panel_thread_names[i], panel->id);
		if (IS_ERR_OR_NULL(panel->thread[i].thread)) {
			panel_err("failed to run %s-%d thread\n",
					panel_thread_names[i], panel->id);
			panel->thread[i].thread = NULL;
			return PTR_ERR(panel->thread[i].thread);
		}
		panel_info("%s-%d:done\n",
				panel_thread_names[i], panel->id);
	}

	return 0;
}

static int panel_destroy_thread(struct panel_device *panel)
{
	size_t i;

	if (unlikely(!panel)) {
		panel_warn("panel is null\n");
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(panel->thread); i++) {
		if (panel_thread_fns[i] == NULL)
			continue;

		if (IS_ERR_OR_NULL(panel->thread[i].thread))
			continue;

		panel->thread[i].should_stop = true;
		/* wake up waitqueue to stop */
		wake_up_interruptible_all(&panel->thread[i].wait);
		/* kthread_should_stop() == true */
		kthread_stop(panel->thread[i].thread);
	}

	return 0;
}

struct panel_device *panel_device_create(void)
{
	struct panel_device *panel;

	panel = kzalloc(sizeof(struct panel_device), GFP_KERNEL);
	if (!panel)
		return NULL;

	return panel;
}
EXPORT_SYMBOL(panel_device_create);

int panel_device_init(struct panel_device *panel)
{
	int ret;

	if (!panel)
		return -EINVAL;

	panel->cmdq.top = -1;
	mutex_init(&panel->cmdq.lock);

	panel->state.init_at = PANEL_INIT_BOOT;
	panel->state.connect_panel = PANEL_CONNECT;
	panel->state.connected = true;
	panel_set_cur_state(panel, PANEL_STATE_OFF);
	panel->state.power = PANEL_POWER_OFF;
	panel->state.disp_on = PANEL_DISPLAY_OFF;
	panel->ktime_panel_on = ktime_get();
#ifdef CONFIG_SUPPORT_HMD
	panel->state.hmd_on = PANEL_HMD_OFF;
#endif

	mutex_init(&panel->op_lock);
	mutex_init(&panel->data_lock);
	mutex_init(&panel->io_lock);
	mutex_init(&panel->panel_bl.lock);

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	mutex_init(&panel->mdnie.lock);
#endif

#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	mutex_init(&panel->copr.lock);
#endif
	panel_parse_dt(panel);

	panel_drv_set_gpios(panel);
	panel_drv_set_regulators(panel);
	panel_init_v4l2_subdev(panel);

	panel_drv_init_work(panel);
	panel_create_thread(panel);

//remove debugfs
#if 1
#ifdef CONFIG_PANEL_DEBUG
	panel_create_debugfs(panel);
#endif
#endif
	panel->fb_notif.notifier_call = panel_fb_notifier;
	ret = fb_register_client(&panel->fb_notif);
	if (ret < 0) {
		panel_err("failed to register fb notifier callback\n");
		return ret;
	}

#ifdef CONFIG_DISPLAY_USE_INFO
	panel->panel_dpui_notif.notifier_call = panel_dpui_notifier_callback;
	ret = dpui_logging_register(&panel->panel_dpui_notif, DPUI_TYPE_PANEL);
	if (ret < 0) {
		panel_err("failed to register dpui notifier callback\n");
		return ret;
	}
#endif
#ifdef CONFIG_SUPPORT_TDMB_TUNE
	ret = tdmb_notifier_register(&panel->tdmb_notif,
			panel_tdmb_notifier_callback, TDMB_NOTIFY_DEV_LCD);
	if (ret < 0) {
		panel_err("failed to register tdmb notifier callback\n");
		return ret;
	}
#endif
//Todo
#if 0
#if defined(CONFIG_INPUT_TOUCHSCREEN)
	sec_input_register_notify(&panel->input_notif,
			panel_input_notifier_callback, 3);
#endif
#endif

	panel_register_isr(panel);

	panel_info("done\n");

	return 0;
}
EXPORT_SYMBOL(panel_device_init);

int panel_device_exit(struct panel_device *panel)
{
	int ret;

	ret = panel_remove(panel);
	if (ret < 0) {
		panel_err("failed to remove panel\n");
		return ret;
	}

	/* TODO: unregister isr */
//	if (panel->max_nr_dim_flash_result > 0)
//		kfree(panel->dim_flash_result);

	panel_destroy_thread(panel);
#if 0
#if defined(CONFIG_INPUT_TOUCHSCREEN)
	sec_input_unregister_notify(&panel->input_notif);
#endif
#endif
#ifdef CONFIG_DISPLAY_USE_INFO
	dpui_logging_unregister(&panel->panel_dpui_notif);
#endif
	fb_unregister_client(&panel->fb_notif);
#if 0
#ifdef CONFIG_PANEL_DEBUG
	panel_destroy_debugfs(panel);
#endif
#endif

	return 0;
}
EXPORT_SYMBOL(panel_device_exit);

static int panel_drv_probe(struct platform_device *pdev)
{
	struct panel_device *panel;
	int ret;

	panel_info("...");
	if (!pdev)
		return -EINVAL;

	panel = panel_device_create();
	if (!panel)
		return -ENOMEM;

	panel->id = -1;
	panel->dev = &pdev->dev;
	platform_set_drvdata(pdev, panel);
	ret = panel_device_init(panel);
	if (ret < 0) {
		panel_err("failed to initialize panel device\n");
		return ret;
	}

	panel_info("done");

	return 0;
}

static const struct of_device_id panel_drv_of_match_table[] = {
	{ .compatible = "samsung,panel-drv", },
	{ },
};
MODULE_DEVICE_TABLE(of, panel_drv_of_match_table);

struct platform_driver panel_driver = {
	.probe = panel_drv_probe,
	.driver = {
		.name = PANEL_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(panel_drv_of_match_table),
	}
};

#ifdef CONFIG_SUPPORT_MAFPC
extern struct platform_driver mafpc_driver;
#endif
static int __init panel_drv_init(void)
{
	int ret;
	lcd_class = class_create(THIS_MODULE, "lcd");
	if (IS_ERR_OR_NULL(lcd_class)) {
		panel_err("failed to create lcd class\n");
		return -EINVAL;
	}

#ifdef CONFIG_SUPPORT_MAFPC
	ret = platform_driver_register(&mafpc_driver);
	if (ret)
		panel_err("failed to register mafpc driver\n");
#endif
	ret = platform_driver_register(&panel_driver);
	if (ret)
		panel_err("failed to register panel driver\n");

	return ret;
}

static void __exit panel_drv_exit(void)
{
	platform_driver_unregister(&panel_driver);
#ifdef CONFIG_SUPPORT_MAFPC
	platform_driver_unregister(&mafpc_driver);
#endif
	class_destroy(lcd_class);

}

#ifdef CONFIG_EXYNOS_DPU30_DUAL
device_initcall_sync(panel_drv_init);
#else
module_init(panel_drv_init);
#endif
module_exit(panel_drv_exit);


MODULE_SOFTDEP("pre: s2dos05_regulator s2mps24-regulator spi-s3c64xx");

MODULE_DESCRIPTION("Samsung's Panel Driver");
MODULE_AUTHOR("<minwoo7945.kim@samsung.com>");
MODULE_LICENSE("GPL");
