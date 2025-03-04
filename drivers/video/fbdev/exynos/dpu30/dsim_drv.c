/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung SoC MIPI-DSIM driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/pm_runtime.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/iommu.h>
#include <linux/dma-iommu.h>
#include <video/mipi_display.h>
#if !defined(CONFIG_UML)
#include <soc/samsung/cal-if.h>
#endif
#include <dt-bindings/soc/samsung/exynos2100-devfreq.h>
#include <soc/samsung/exynos-devfreq.h>

#if defined(CONFIG_CPU_IDLE)
#include <soc/samsung/exynos-cpupm.h>
#endif
#if !defined(CONFIG_UML)
#include <soc/samsung/exynos-pmu-if.h>
#endif

#include <linux/of_reserved_mem.h>

#include "decon.h"
#include "dsim.h"
#include "./panels/exynos_panel_drv.h"

#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
#include "fcmd.h"
#endif

#include <soc/samsung/exynos-pd.h>

#if defined(SYSFS_UNITTEST_INTERFACE)
#include "sysfs_error.h"
#endif

#if IS_ENABLED(CONFIG_EXYNOS_MIGOV)
#include <soc/samsung/exynos-migov.h>
#endif

#if IS_ENABLED(CONFIG_MCD_PANEL)
#include "mcd_helper.h"
#include "mcd_dsim.h"
#ifdef CONFIG_DYNAMIC_MIPI
#include "dynamic_mipi/dynamic_mipi.h"
#endif
#endif

#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
#define FCMD_SEND_START		0x4c
#define FCMD_SEND_CONTINUE	0x5c
#define FCMD_DATA_MAX_SIZE	0x00100000
#endif

int dsim_log_level = 6;
module_param(dsim_log_level, int, 0644);

struct dsim_device *dsim_drvdata[MAX_DSIM_CNT];
EXPORT_SYMBOL(dsim_drvdata);

#if IS_ENABLED(CONFIG_EXYNOS_MIGOV)
#define MIGOV_PERIOD_NSEC	100000000UL
#define FPS_UNIT_NSEC		1000000000UL	/* 1sec */
extern u64 frame_vsync_cnt;
extern ktime_t frame_vsync_cnt_time;
void get_frame_vsync_cnt(u64 *cnt, ktime_t *time)
{
	struct decon_device *decon = get_decon_drvdata(0);
	u64 lcd_fps;
	u64 vsync_period_nsec;
	static u64 vsync_cnt;
	static ktime_t prev_ktime;
	ktime_t vsync_time;
	ktime_t cur_ktime = ktime_get();
	ktime_t ktime_diff;

	if (!IS_ERR_OR_NULL(decon->lcd_info)) {
		lcd_fps = decon->lcd_info->fps;
		vsync_period_nsec = FPS_UNIT_NSEC / lcd_fps;

		if (vsync_cnt) {
			if (prev_ktime) {
				ktime_diff = cur_ktime - prev_ktime;
				vsync_cnt += DIV_ROUND_CLOSEST(lcd_fps * ktime_diff, FPS_UNIT_NSEC);
			} else
				vsync_cnt += lcd_fps * MIGOV_PERIOD_NSEC / FPS_UNIT_NSEC;
		} else
			vsync_cnt = frame_vsync_cnt;

		if ((cur_ktime - frame_vsync_cnt_time) > vsync_period_nsec)
			vsync_time = frame_vsync_cnt_time + vsync_period_nsec;
		else
			vsync_time = frame_vsync_cnt_time;
	} else {
		vsync_cnt = frame_vsync_cnt;
		vsync_time = frame_vsync_cnt_time;
	}

	*cnt = vsync_cnt;
	*time = vsync_time;
	prev_ktime = cur_ktime;
}
#endif

/*
 * This global mutex lock protects to initialize or de-initialize DSIM and DPHY
 * hardware when multi display is in operation
 */
DEFINE_MUTEX(g_dsim_lock);

static char *dsim_state_names[] = {
	"INIT",
	"ON",
	"DOZE",
	"ULPS",
	"DOZE_SUSPEND",
	"OFF",
};

static int dsim_runtime_suspend(struct device *dev);
static int dsim_runtime_resume(struct device *dev);
static struct exynos_pm_domain *dpu_get_pm_domain(void);
static int dpu_power_on(struct dsim_device *dsim);
static int dpu_power_off(struct dsim_device *dsim);

int dsim_call_panel_ops(struct dsim_device *dsim, u32 cmd, void *arg)
{
	struct v4l2_subdev *sd;

	if (IS_ERR_OR_NULL(dsim->panel)) {
		dsim_err("%s: panel ptr is NULL\n", __func__);
		return -ENOMEM;
	}

	sd = &dsim->panel->sd;
	return v4l2_subdev_call(sd, core, ioctl, cmd, arg);
}

static void dsim_dump(struct dsim_device *dsim)
{
	struct dsim_regs regs;

	dsim_info("=== DSIM SFR DUMP ===\n");

	dsim_to_regs_param(dsim, &regs);
	__dsim_dump(dsim->id, &regs);

#if !IS_ENABLED(CONFIG_MCD_PANEL)
	/* Show panel status */
	dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_DUMP, NULL);
#endif
}

#if IS_ENABLED(CONFIG_MCD_PANEL)
static void dsim_panel_dump(struct dsim_device *dsim)
{
	/* Show panel status */
	dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_DUMP, NULL);
}
#endif

static void dsim_long_data_wr(struct dsim_device *dsim, unsigned long d0, u32 d1)
{
	unsigned int data_cnt = 0, payload = 0;

	/* in case that data count is more then 4 */
	for (data_cnt = 0; data_cnt < d1; data_cnt += 4) {
		/*
		 * after sending 4bytes per one time,
		 * send remainder data less then 4.
		 */
		if ((d1 - data_cnt) < 4) {
			if ((d1 - data_cnt) == 3) {
				payload = *(u8 *)(d0 + data_cnt) |
				    (*(u8 *)(d0 + (data_cnt + 1))) << 8 |
					(*(u8 *)(d0 + (data_cnt + 2))) << 16;
			dsim_dbg("count = 3 payload = %x, %x %x %x\n",
				payload, *(u8 *)(d0 + data_cnt),
				*(u8 *)(d0 + (data_cnt + 1)),
				*(u8 *)(d0 + (data_cnt + 2)));
			} else if ((d1 - data_cnt) == 2) {
				payload = *(u8 *)(d0 + data_cnt) |
					(*(u8 *)(d0 + (data_cnt + 1))) << 8;
			dsim_dbg("count = 2 payload = %x, %x %x\n", payload,
				*(u8 *)(d0 + data_cnt),
				*(u8 *)(d0 + (data_cnt + 1)));
			} else if ((d1 - data_cnt) == 1) {
				payload = *(u8 *)(d0 + data_cnt);
			}

			dsim_reg_wr_tx_payload(dsim->id, payload);
		/* send 4bytes per one time. */
		} else {
			payload = *(u8 *)(d0 + data_cnt) |
				(*(u8 *)(d0 + (data_cnt + 1))) << 8 |
				(*(u8 *)(d0 + (data_cnt + 2))) << 16 |
				(*(u8 *)(d0 + (data_cnt + 3))) << 24;

			dsim_dbg("count = 4 payload = %x, %x %x %x %x\n",
				payload, *(u8 *)(d0 + data_cnt),
				*(u8 *)(d0 + (data_cnt + 1)),
				*(u8 *)(d0 + (data_cnt + 2)),
				*(u8 *)(d0 + (data_cnt + 3)));

			dsim_reg_wr_tx_payload(dsim->id, payload);
		}
	}
	dsim->pl_cnt += ALIGN(d1, 4);
}

static int dsim_wait_for_cmd_fifo_empty(struct dsim_device *dsim, bool must_wait)
{
	int ret = 0;
	struct dsim_regs regs;

	if (!must_wait) {
		/* timer is running, but already command is transferred */
		if (dsim_reg_header_fifo_is_empty(dsim->id))
			del_timer(&dsim->cmd_timer);

		dsim_dbg("%s Doesn't need to wait fifo_completion\n", __func__);
		return ret;
	} else {
		del_timer(&dsim->cmd_timer);
		dsim_dbg("%s Waiting for fifo_completion...\n", __func__);
	}

	if (!wait_for_completion_timeout(&dsim->ph_wr_comp, MIPI_WR_TIMEOUT)) {
		if (dsim_reg_header_fifo_is_empty(dsim->id)) {
			reinit_completion(&dsim->ph_wr_comp);
			dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PH_FIFO_EMPTY);
			return 0;
		}
		ret = -ETIMEDOUT;
	}

	if (IS_DSIM_ON_STATE(dsim) && (ret == -ETIMEDOUT)) {
		dsim_err("%s have timed out\n", __func__);
		dsim_to_regs_param(dsim, &regs);
		__dsim_dump(dsim->id, &regs);
	}
	return ret;
}

/* wait for until SFR fifo is empty */
int dsim_wait_for_cmd_done(struct dsim_device *dsim)
{
	int ret = 0;
	/* FIXME: hiber only support for DECON0 */
	struct decon_device *decon = get_decon_drvdata(0);

	decon_hiber_block_exit(decon);

	mutex_lock(&dsim->cmd_lock);
	ret = dsim_wait_for_cmd_fifo_empty(dsim, true);
	mutex_unlock(&dsim->cmd_lock);

	decon_hiber_unblock(decon);

	return ret;
}

static bool dsim_is_writable_pl_fifo_status(struct dsim_device *dsim, u32 word_cnt)
{
	if ((DSIM_PL_FIFO_THRESHOLD - dsim->pl_cnt) > word_cnt)
		return true;
	else
		return false;
}

static bool dsim_is_fifo_empty_status(struct dsim_device *dsim)
{
	if (dsim_reg_header_fifo_is_empty(dsim->id) && dsim_reg_payload_fifo_is_empty(dsim->id)) {
		dsim->pl_cnt = 0;
		return true;
	} else
		return false;
}

int dsim_check_ph_threshold(struct dsim_device *dsim, u32 cmd_cnt)
{
	int cnt = 5000;
	u32 available = 0;

	available = dsim_reg_is_writable_ph_fifo_state(dsim->id, cmd_cnt);

	/* Wait FIFO empty status during 50ms */
	if (!available) {
		do {
			if (dsim_reg_header_fifo_is_empty(dsim->id))
				break;
			usleep_range(10, 11);
			cnt--;
		} while (cnt);
	}
	return cnt;
}

int dsim_check_linecount(struct dsim_device *dsim)
{
	int cnt = 5000;
	bool fifo_empty = 0;
	int line_cnt = 0;

	dsim->line_cnt = dsim_reg_get_linecount(dsim->id, dsim->panel->lcd_info.mode);
	if (dsim->line_cnt == 0) {
		do {
			fifo_empty = dsim_is_fifo_empty_status(dsim);
			line_cnt = dsim_reg_get_linecount(dsim->id, dsim->panel->lcd_info.mode);
			if (fifo_empty || line_cnt)
				break;
			usleep_range(10, 11);
			cnt--;
		} while (cnt);
	}
	return cnt;
}

int dsim_check_pl_threshold(struct dsim_device *dsim, u32 d1)
{
	int cnt = 5000;

	if (!dsim_is_writable_pl_fifo_status(dsim, d1)) {
		do {
			if (dsim_reg_payload_fifo_is_empty(dsim->id)) {
				dsim->pl_cnt = 0;
				break;
			}
			usleep_range(10, 11);
			cnt--;
		} while (cnt);
	}

	return cnt;
}

#if IS_ENABLED(CONFIG_MCD_PANEL)
static void dsim_wr_payload(struct dsim_device *dsim, unsigned char *buf, u32 size)
{
	unsigned int data_cnt = 0;

	for (data_cnt = 0; data_cnt < size; data_cnt += 4) {
		dsim_reg_wr_tx_payload(dsim->id, *((unsigned int *)(buf + data_cnt)));
	}
	dsim->pl_cnt += size;
}

int __mockable dsim_sr_write_data(struct dsim_device *dsim, const u8 *cmd, u32 size, u32 align)
{
	int cnt;
	u8 c_start = 0, c_next = 0;
	/* TODO: 512 NEED TO CHANGE AS DSIM_FIFO_SIZE */
	u8 cmdbuf[2048];
	int tx_size, ret = 0, len = 0;
	int remained = size;


	mutex_lock(&dsim->cmd_lock);
	if (!IS_DSIM_ON_STATE(dsim)) {
		dsim_err("%s dsim%d not ready (%s)\n",
				__func__, dsim->id, dsim_state_names[dsim->state]);
		ret = -EINVAL;
		goto err_exit;
	}

	dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PH_FIFO_EMPTY);

	/* Check available status of PH FIFO before writing command */
	if (!dsim_check_ph_threshold(dsim, 1)) {
		ret = -EINVAL;
		dsim_err("ID(%d): DSIM cmd wr timeout @ don't available ph 0x%x\n",
			dsim->id, cmd[0]);
		goto err_exit;
	}

	/* Check linecount value for seperating idle and active range */
	if (!dsim_check_linecount(dsim)) {
		ret = -EINVAL;
		dsim_err("ID(%d): DSIM cmd wr timeout @ line count '0' pl_cnt = %d\n",
			dsim->id, dsim->pl_cnt);
		goto err_exit;
	}

	dsim_info("%s : size : %d align: %d\n", __func__, size, align);

	c_start = MIPI_DCS_WRITE_SIDE_RAM_START;
	c_next = MIPI_DCS_WRITE_SIDE_RAM_CONTINUE;

	do {
		cmdbuf[0] = (size == remained) ? c_start : c_next;
		tx_size = min(remained, 2047);

		if ((tx_size % align) > 0) {
			if (tx_size > align) {
				tx_size -= (tx_size % align);
			} else {
				dsim_warn("%s: byte align mismatch! data %d align %d\n",
					__func__, tx_size, align);
			}
		}

		memcpy(cmdbuf + 1, cmd + len, tx_size);

		dsim_reg_enable_packetgo(dsim->id, true);
		//decon_systrace(get_decon_drvdata(0), 'C', "mafpc", 1);
		dsim_wr_payload(dsim, cmdbuf, tx_size + 1);
		//decon_systrace(get_decon_drvdata(0), 'C', "mafpc", 0);
		dsim_reg_wr_tx_header(dsim->id, MIPI_DSI_DCS_LONG_WRITE, (tx_size + 1) & 0xff,
				((tx_size + 1) & 0xff00) >> 8, false);
		dsim_reg_enable_packetgo(dsim->id, false);

		len += tx_size;
		remained -= tx_size;

		cnt = 5000;
		do {
			if (dsim_is_fifo_empty_status(dsim))
				break;
			usleep_range(10, 11);
			cnt--;
		} while (cnt);

		if (!cnt) {
			dsim_err("ID(%d): DSIM command(%x) fail\n", dsim->id, cmd[0]);
			ret = -EINVAL;
		}
	} while (remained > 0);

err_exit:
	mutex_unlock(&dsim->cmd_lock);
	return ret;
}
#endif

int dsim_cal_pl_sum(struct exynos_dsim_cmd set_cmd[], int cmd_cnt, struct exynos_dsim_cmd_set *set)
{
	int i;
	int pl_size;
	int pl_sum_total = 0;
	int pl_sum = 0;
	struct exynos_dsim_cmd *cmd;

	set->cnt = 1;
	set->index[0] = cmd_cnt - 1;

	for (i = 0; i < cmd_cnt; i++) {
		cmd = &set_cmd[i];

		switch (cmd->type) {
			/* long packet types of packet types for command. */
			case MIPI_DSI_GENERIC_LONG_WRITE:
			case MIPI_DSI_DCS_LONG_WRITE:
			case MIPI_DSI_DSC_PPS:
				if (cmd->data_len > DSIM_PL_FIFO_THRESHOLD) {
					dsim_err("Don't support for pl size exceeding %d\n",
							DSIM_PL_FIFO_THRESHOLD);
					return -EINVAL;
				}
				pl_size = ALIGN(cmd->data_len, 4);
				pl_sum += pl_size;
				pl_sum_total += pl_size;
				if (pl_sum > DSIM_PL_FIFO_THRESHOLD) {
					set->index[set->cnt-1] = i - 1;
					set->cnt++;
					pl_sum = ALIGN(cmd->data_len, 4);
				}
				break;
		}
		dsim_dbg("pl_sum_total : %d\n", pl_sum_total);
	}

	return pl_sum_total;
}

int __mockable dsim_write_cmd_set(struct dsim_device *dsim, struct exynos_dsim_cmd cmd_list[],
		int cmd_cnt, bool wait_vsync)
{
	int i, j = 0;
	int ret = 0;
	int cnt = 5000;
	int pl_sum;
	u32 ph_num1, ph_num2;
	struct decon_device *decon = get_decon_drvdata(dsim->id);
	struct exynos_dsim_cmd *cmd;
	struct exynos_dsim_cmd_set set;
	struct dsim_regs regs;

	decon_hiber_block_exit(decon);
	mutex_lock(&dsim->cmd_lock);

	if (!IS_DSIM_ON_STATE(dsim)) {
		dsim_warn("DSIM is not ready. state(%d)\n", dsim->state);
		ret = -EINVAL;
		goto err_exit;
	}

	/* force to exit pll sleep before starting command transfer */
	dpu_pll_sleep_mask(decon);

	/* check PH available */
	if (!dsim_check_ph_threshold(dsim, cmd_cnt)) {
		ret = -EINVAL;
		dsim_err("DSIM_%d cmd wr timeout @ don't available ph\n", dsim->id);
		goto err_exit;
	}

	/* check PL available */
	pl_sum = dsim_cal_pl_sum(cmd_list, cmd_cnt, &set);
	if (!dsim_check_pl_threshold(dsim, pl_sum)) {
		ret = -EINVAL;
		dsim_err("DSIM don't available pl, pl_cnt @ fifo : %d, pl_sum : %d",
				dsim->pl_cnt, pl_sum);
		goto err_exit;
	}
	/* check line cnt value */
	if (!dsim_check_linecount(dsim)) {
		ret = -EINVAL;
		dsim_err("DSIM cmd wr timeout @ line count '0', pl_cnt @ = %d\n", dsim->pl_cnt);
		goto err_exit;
	}

	for (i = 0; i < set.cnt; i++) {

		/* packet go enable */
		dsim_reg_enable_packetgo(dsim->id, 1);

		for (; j < cmd_cnt; j++) {
			cmd = &cmd_list[j];
			if (!cmd->data_len)
				break;

			switch (cmd->type) {
			/* short packet types of packet types for command. */
			case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
			case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
			case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
			case MIPI_DSI_DCS_SHORT_WRITE:
			case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
			case MIPI_DSI_DSC_PRA:
			case MIPI_DSI_COLOR_MODE_OFF:
			case MIPI_DSI_COLOR_MODE_ON:
			case MIPI_DSI_SHUTDOWN_PERIPHERAL:
			case MIPI_DSI_TURN_ON_PERIPHERAL:
				if (cmd->data_len == 1)
					dsim_reg_wr_tx_header(dsim->id,
						cmd->type, cmd->data_buf[0],
						0, false);
				else
					dsim_reg_wr_tx_header(dsim->id,
						cmd->type, cmd->data_buf[0],
						cmd->data_buf[1], false);
				break;

				/* long packet types of packet types for command. */
			case MIPI_DSI_GENERIC_LONG_WRITE:
			case MIPI_DSI_DCS_LONG_WRITE:
			case MIPI_DSI_DSC_PPS:
				dsim_long_data_wr(dsim, (unsigned long)cmd->data_buf, cmd->data_len);
				dsim_reg_wr_tx_header(dsim->id, cmd->type, cmd->data_len & 0xff,
						(cmd->data_len & 0xff00) >> 8, false);
				break;

			default:
					dsim_info("data id %x is not supported.\n", cmd->type);
					ret = -EINVAL;
			}

			if (j == set.index[i])
				break;
		}

		if (set.cnt == 1) {
			if(wait_vsync)
				decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
		}
		ph_num1 = dsim_reg_get_ph_num(dsim->id);
		/* set packet go ready*/
		dsim_reg_set_packetgo_ready(dsim->id);

		do {
			if (dsim_is_fifo_empty_status(dsim))
				break;
			usleep_range(10, 11);
			cnt--;
		} while (cnt);
		if (!cnt) {
			ph_num2 = dsim_reg_get_ph_num(dsim->id);
			dsim_err("DSIM command set fail, cmd_cnt: %d(ok_cnt: %d)\n",
					cmd_cnt, (ph_num1 - ph_num2));
			dsim_to_regs_param(dsim, &regs);
			__dsim_dump(dsim->id, &regs);
			ret = -EINVAL;
			goto err_exit;
		}
	}
	dsim_reg_enable_packetgo(dsim->id, 0);

err_exit:
	mutex_unlock(&dsim->cmd_lock);
	decon_hiber_unblock(decon);

	return ret;

}

int __mockable dsim_write_data(struct dsim_device *dsim, u32 id, unsigned long d0, u32 d1, bool wait_empty)
{
	int ret = 0;
	struct decon_device *decon = get_decon_drvdata(0);
	int cnt = 5000; /* for wating empty status during 50ms */

	decon_hiber_block_exit(decon);

	mutex_lock(&dsim->cmd_lock);
	if (!IS_DSIM_ON_STATE(dsim)) {
		dsim_warn("DSIM is not ready. state(%d)\n", dsim->state);
		ret = -EINVAL;
		goto err_exit;
	}

	/* force to exit pll sleep before starting command transfer */
	dpu_pll_sleep_mask(decon);

	reinit_completion(&dsim->ph_wr_comp);
	dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PH_FIFO_EMPTY);

	/* Check available status of PH FIFO before writing command */
	if (!dsim_check_ph_threshold(dsim, 1)) {
		ret = -EINVAL;
		dsim_err("ID(%d): DSIM cmd wr timeout @ don't available ph 0x%lx\n", id, d0);
		goto err_exit;
	}

	/* Check linecount value for seperating idle and active range */
	if (!dsim_check_linecount(dsim)) {
		ret = -EINVAL;
		dsim_err("ID(%d): DSIM cmd wr timeout @ line count '0' 0x%lx, pl_cnt = %d\n", id, d0, dsim->pl_cnt);
		goto err_exit;
	}

	/* Run write-fail dectector */
	mod_timer(&dsim->cmd_timer, jiffies + MIPI_WR_TIMEOUT);

	switch (id) {
	/* short packet types of packet types for command. */
	case MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM:
	case MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM:
	case MIPI_DSI_DCS_SHORT_WRITE:
	case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
	case MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE:
	case MIPI_DSI_DSC_PRA:
	case MIPI_DSI_COLOR_MODE_OFF:
	case MIPI_DSI_COLOR_MODE_ON:
	case MIPI_DSI_SHUTDOWN_PERIPHERAL:
	case MIPI_DSI_TURN_ON_PERIPHERAL:
		/* Enable packet go for blocking command transfer */
		dsim_reg_enable_packetgo(dsim->id, true);
		dsim_reg_wr_tx_header(dsim->id, id, d0, d1, false);
		break;

	case MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM:
	case MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM:
	case MIPI_DSI_DCS_READ:
		/* Enable packet go for blocking command transfer */
		dsim_reg_enable_packetgo(dsim->id, true);
		dsim_reg_wr_tx_header(dsim->id, id, d0, d1, true);
		break;

	/* long packet types of packet types for command. */
	case MIPI_DSI_GENERIC_LONG_WRITE:
	case MIPI_DSI_DCS_LONG_WRITE:
	case MIPI_DSI_DSC_PPS:
		if (d1 > DSIM_PL_FIFO_THRESHOLD) {
			dsim_err("Don't support payload size that exceeds 2048byte\n");
			ret = -EINVAL;
			goto err_exit;
		}
		if (!dsim_check_pl_threshold(dsim, ALIGN(d1, 4))) {
			ret = -EINVAL;
			dsim_err("ID(%d): DSIM don't available pl 0x%lx\n, pl_cnt : %d, wc : %d",
					id, d0, dsim->pl_cnt, d1);
			goto err_exit;
		}

		/* Enable packet go for blocking command transfer */
		dsim_reg_enable_packetgo(dsim->id, true);
		dsim_long_data_wr(dsim, d0, d1);
		dsim_reg_wr_tx_header(dsim->id, id, d1 & 0xff,
				(d1 & 0xff00) >> 8, false);
		break;

	default:
		dsim_info("data id %x is not supported.\n", id);
		ret = -EINVAL;
	}

	dsim_reg_enable_packetgo(dsim->id, false);

	if (wait_empty) {
		do {
			if (dsim_is_fifo_empty_status(dsim))
				break;
			usleep_range(10, 11);
			cnt--;
		} while (cnt);

		if (!cnt) {
			dsim_err("ID(%d): DSIM command(%lx) fail\n", id, d0);
			ret = -EINVAL;
		}
	}

err_exit:
	DPU_EVENT_LOG_CMD(&dsim->sd, id, d0, d1);
	mutex_unlock(&dsim->cmd_lock);
	decon_hiber_unblock(decon);

	return ret;
}

int __mockable dsim_read_data(struct dsim_device *dsim, u32 id, u32 addr, u32 cnt, u8 *buf)
{
	u32 rx_fifo, rx_size = 0;
	int i, j, ret = 0;
	u32 rx_fifo_depth = DSIM_RX_FIFO_MAX_DEPTH;
	struct decon_device *decon = get_decon_drvdata(0);
	struct dsim_regs regs;

	decon_hiber_block_exit(decon);

	mutex_lock(&dsim->rd_cmd_lock);
	if (IS_DSIM_OFF_STATE(dsim)) {
		dsim_err("DSIM is not ready. state(%d)\n", dsim->state);
		mutex_unlock(&dsim->rd_cmd_lock);
		decon_hiber_unblock(decon);
		return -EINVAL;
	}

	reinit_completion(&dsim->rd_comp);

	/* Init RX FIFO before read and clear DSIM_INTSRC */
	dsim_reg_clear_int(dsim->id, DSIM_INTSRC_RX_DATA_DONE);

	/* Set the maximum packet size returned */
	dsim_write_data(dsim,
		MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE, cnt, 0, false);

	/* Read request */
	dsim_write_data(dsim, id, addr, 0, true);

	dsim_wait_for_cmd_done(dsim);

	if (!wait_for_completion_timeout(&dsim->rd_comp, MIPI_RD_TIMEOUT)) {
		dsim_err("MIPI DSIM read Timeout!\n");
		if (dsim_reg_get_datalane_status(dsim->id) == DSIM_DATALANE_STATUS_BTA) {
			if (decon_reg_get_run_status(decon->id)) {
				dsim_reset_panel(dsim);
				dpu_hw_recovery_process(decon);
			} else {
				dsim_reset_panel(dsim);
				dsim_reg_recovery_process(dsim);
			}
		} else
			dsim_err("datalane status is %d\n", dsim_reg_get_datalane_status(dsim->id));

		mutex_unlock(&dsim->rd_cmd_lock);
		decon_hiber_unblock(decon);
		return -ETIMEDOUT;
	}

	mutex_lock(&dsim->cmd_lock);
	DPU_EVENT_LOG_CMD(&dsim->sd, id, (char)addr, 0);

	do {
		rx_fifo = dsim_reg_get_rx_fifo(dsim->id);

		/* Parse the RX packet data types */
		switch (rx_fifo & 0xff) {
		case MIPI_DSI_RX_ACKNOWLEDGE_AND_ERROR_REPORT:
			ret = dsim_reg_rx_err_handler(dsim->id, rx_fifo);
			if (ret < 0) {
				dsim_to_regs_param(dsim, &regs);
				__dsim_dump(dsim->id, &regs);
				goto exit;
			}
			break;
		case MIPI_DSI_RX_END_OF_TRANSMISSION:
			dsim_dbg("EoTp was received from LCD module.\n");
			break;
		case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_1BYTE:
		case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_1BYTE:
			dsim_dbg("1byte Short Packet was received from LCD\n");
			buf[0] = (rx_fifo >> 8) & 0xff;
			rx_size = 1;
			break;
		case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_2BYTE:
		case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_2BYTE:
			dsim_dbg("2bytes Short Packet was received from LCD\n");
			for (i = 0; i < 2; i++)
				buf[i] = (rx_fifo >> (8 + i * 8)) & 0xff;
			rx_size = 2;
			break;
		case MIPI_DSI_RX_DCS_LONG_READ_RESPONSE:
		case MIPI_DSI_RX_GENERIC_LONG_READ_RESPONSE:
			dsim_dbg("Long Packet was received from LCD module.\n");
			rx_size = (rx_fifo & 0x00ffff00) >> 8;
			dsim_dbg("rx fifo : %8x, response : %x, rx_size : %d\n",
					rx_fifo, rx_fifo & 0xff, rx_size);
			/* Read data from RX packet payload */
			for (i = 0; i < rx_size >> 2; i++) {
				rx_fifo = dsim_reg_get_rx_fifo(dsim->id);
				for (j = 0; j < 4; j++)
					buf[(i*4)+j] = (u8)(rx_fifo >> (j * 8)) & 0xff;
			}
			if (rx_size % 4) {
				rx_fifo = dsim_reg_get_rx_fifo(dsim->id);
				for (j = 0; j < rx_size % 4; j++)
					buf[4 * i + j] =
						(u8)(rx_fifo >> (j * 8)) & 0xff;
			}
			break;
		default:
			dsim_err("Packet format is invaild.\n");
			dsim_to_regs_param(dsim, &regs);
			__dsim_dump(dsim->id, &regs);
			ret = -EBUSY;
			goto exit;
		}
	} while (!dsim_reg_rx_fifo_is_empty(dsim->id) && --rx_fifo_depth);

	ret = rx_size;
	if (!rx_fifo_depth) {
		dsim_err("Check DPHY values about HS clk.\n");
		dsim_to_regs_param(dsim, &regs);
		__dsim_dump(dsim->id, &regs);
		ret = -EBUSY;
	}
exit:
	mutex_unlock(&dsim->cmd_lock);
	mutex_unlock(&dsim->rd_cmd_lock);
	decon_hiber_unblock(decon);

	return ret;
}

#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
int dsim_call_fcmd_ops(struct dsim_device *dsim, u32 cmd, void *arg)
{
	struct v4l2_subdev *sd;

	if (IS_ERR_OR_NULL(dsim->fcmd)) {
		dsim_err("%s: fcmd ptr is NULL\n", __func__);
		return -ENOMEM;
	}

	sd = &dsim->fcmd->sd;
	return v4l2_subdev_call(sd, core, ioctl, cmd, arg);
}

int dsim_wait_fifo_empty(struct dsim_device *dsim, u32 frames)
{
	int cnt = frames * 1000000 / dsim->panel->lcd_info.fps / 10;
#if defined(CONFIG_EXYNOS_EMUL_DISP)
	cnt = cnt * EMUL_DISP_SLOW_DEGREE;
#endif
	do {
		if (dsim_is_fifo_empty_status(dsim))
			break;
		usleep_range(10, 11);
		cnt--;
	} while (cnt);

	return cnt;
}

int dsim_wait_for_fcmd_xfer_done(struct dsim_device *dsim)
{
	int ret;
	u32 fcmd_timeout = 10 * 1000000 / dsim->panel->lcd_info.fps / 1000;

#if defined(CONFIG_EXYNOS_EMUL_DISP)
	fcmd_timeout = fcmd_timeout * EMUL_DISP_SLOW_DEGREE;
#endif

	ret = wait_event_interruptible_timeout(dsim->fcmd->xferdone_wq,
			dsim->fcmd->fcmd_config->done, msecs_to_jiffies(fcmd_timeout));
	if (ret == 0) {
		dsim_err("timeout of dsim%d fcmd xferdone\n", dsim->id);
		return -ETIMEDOUT;
	} else
		dsim_info("dsim%d fcmd xferdone\n", dsim->id);

	return 0;
}

int dsim_write_data_fcmd(struct dsim_device *dsim, u32 id, dma_addr_t d0, u32 d1, bool wait_empty)
{
	int ret = 0;
	struct decon_device *decon = get_decon_drvdata(0);
	struct fcmd_config fcmd_config;
	u32 pkt_unit = DSIM_PL_FIFO_THRESHOLD;

	/* Check command id : FCMD support only below 2 types */
	if ((id != MIPI_DSI_GENERIC_LONG_WRITE) && (id != MIPI_DSI_DCS_LONG_WRITE)) {
		dsim_info("data id %x is not supported in dma access mode.\n", id);
		return -EINVAL;
	}

	decon_hiber_block_exit(decon);

	mutex_lock(&dsim->cmd_lock);
	if (!IS_DSIM_ON_STATE(dsim)) {
		dsim_warn("DSIM is not ready. state(%d)\n", dsim->state);
		ret = -EINVAL;
		goto err_exit;
	}

	/* Wait 3 frames for ph & pl fifo to be empty before writing command */
	if (!dsim_wait_fifo_empty(dsim, 3)) {
		ret = -EINVAL;
		dsim_err("ID(%d): DSIM cmd wr timeout @ don't available ph or pl 0x%lx\n", id, d0);
		goto err_exit;
	}

	if (d1 < pkt_unit)
		pkt_unit = d1;

	fcmd_config.bta = 0;
	fcmd_config.type = 0; /* obsolete */
	fcmd_config.di = (u8)id;
	fcmd_config.cmd = 0; /* obsolete */
	fcmd_config.size = d1;
	fcmd_config.unit = pkt_unit;
	fcmd_config.buf = d0;
	fcmd_config.done = 0;

	ret = dsim_call_fcmd_ops(dsim, FCMD_SET_CONFIG, &fcmd_config);
	if (ret) {
		dsim_err("failed to config fcmd\n", __func__, ret);
		goto err_exit;
	}

	dsim_reg_set_cmd_access_mode(dsim->id, 1);

	/* force to exit pll sleep before starting command transfer */
	dpu_pll_sleep_mask(decon);

	ret = dsim_call_fcmd_ops(dsim, FCMD_START, NULL);
	if (ret) {
		dsim_err("failed to start fcmd\n", __func__, ret);
		goto err_fcmd_exit;
	}

	reinit_completion(&dsim->fcmd_wr_comp);
	dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PL_FIFO_EMPTY);

	/* Run write-fail dectector */
	mod_timer(&dsim->fcmd_timer, jiffies + MIPI_WR_TIMEOUT);

	ret = dsim_wait_for_fcmd_xfer_done(dsim);
	if (ret)
		goto err_fcmd_exit;

	if (wait_empty) {
		/* wating time for empty : 10 frames  */
		if (!dsim_wait_fifo_empty(dsim, 10)) {
			dsim_err("ID(%d): DSIM command(%lx) fail\n", id, d0);
			ret = -EINVAL;
		}
	}

err_fcmd_exit:
	dsim_reg_set_cmd_access_mode(dsim->id, 0);
	ret = dsim_call_fcmd_ops(dsim, FCMD_STOP, NULL);

err_exit:
	DPU_EVENT_LOG_CMD(&dsim->sd, id, (unsigned long)dsim->fcmd_buf_vaddr, d1);
	mutex_unlock(&dsim->cmd_lock);
	decon_hiber_unblock(decon);

	return ret;
}

static void dsim_fcmd_fail_detector(struct timer_list *arg)
{
	struct dsim_device *dsim = from_timer(dsim, arg, fcmd_timer);
	struct decon_device *decon = get_decon_drvdata(0);
	struct dsim_regs regs;

	decon_hiber_block(decon);

	dsim_dbg("%s +\n", __func__);
	if (IS_DSIM_OFF_STATE(dsim)) {
		dsim_err("%s: DSIM is not ready. state(%d)\n", __func__,
				dsim->state);
		goto exit;
	}

	/* If already FIFO empty even though the timer is no pending */
	if (!timer_pending(&dsim->fcmd_timer)
			&& dsim_reg_payload_fifo_is_empty(dsim->id)) {
		reinit_completion(&dsim->fcmd_wr_comp);
		dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PL_FIFO_EMPTY);
		goto exit;
	}

	dsim_to_regs_param(dsim, &regs);
	__dsim_dump(dsim->id, &regs);

exit:
	decon_hiber_unblock(decon);
	dsim_dbg("%s -\n", __func__);
	return;
}

static unsigned int dsim_map_ion_handle(struct device *dev,
		struct dsim_dma_buf_data *dma,
		struct dma_buf *buf)
{
	dma->fence = NULL;
	dma->dma_buf = buf;

	if (IS_ERR_OR_NULL(dev)) {
		dsim_err("%s: dev ptr is invalid\n", __func__);
		goto err_buf_map_attach;
	}

	dma->attachment = dma_buf_attach(dma->dma_buf, dev);
	if (IS_ERR_OR_NULL(dma->attachment)) {
		dsim_err("dma_buf_attach() failed: %ld\n",
				PTR_ERR(dma->attachment));
		goto err_buf_map_attach;
	}

	dma->sg_table = dma_buf_map_attachment(dma->attachment,
			DMA_TO_DEVICE);
	if (IS_ERR_OR_NULL(dma->sg_table)) {
		dsim_err("dma_buf_map_attachment() failed: %ld\n",
				PTR_ERR(dma->sg_table));
		goto err_buf_map_attachment;
	}

	/* This is DVA(Device Virtual Address) for setting base address SFR */
	dma->dma_addr = sg_dma_address(dma->sg_table->sgl);
	if (IS_ERR_VALUE(dma->dma_addr)) {
		dsim_err("sg_dma_address() failed: %pa\n", &dma->dma_addr);
		goto err_iovmm_map;
	}

	return dma->dma_buf->size;

err_iovmm_map:
err_buf_map_attachment:
err_buf_map_attach:
	return 0;
}

static int dsim_alloc_fcmd_memory(u32 id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	dma_addr_t map_dma;
	unsigned int ret;
	u32 size = FCMD_DATA_MAX_SIZE;

	dsim_dbg("%s +\n", __func__);

	dev_info(dsim->dev, "allocating memory for fcmd%d\n", id);

	size = PAGE_ALIGN(size);

	dev_info(dsim->dev, "want %u bytes\n", size);

	dsim->fcmd_buf = ion_alloc((size_t)size, ION_HEAP_SYSTEM, 0);
	if (IS_ERR(dsim->fcmd_buf)) {
		dev_err(dsim->dev, "ion_alloc() failed\n");
		goto err_share_dma_buf;
	}

	dsim->fcmd_buf_vaddr = dma_buf_vmap(dsim->fcmd_buf);
	if (IS_ERR_OR_NULL(dsim->fcmd_buf_vaddr)) {
		dev_err(dsim->dev, "dma_buf_vmap() failed\n");
		goto err_map;
	}

	ret = dsim_map_ion_handle(dsim->dev, &dsim->fcmd_buf_data, dsim->fcmd_buf);
	if (!ret)
		goto err_map;

	map_dma = dsim->fcmd_buf_data.dma_addr;

	dev_info(dsim->dev, "alloated memory for fcmd%d\n", id);
	dev_info(dsim->dev, "fcmd start addr = 0x%x\n", (u32)map_dma);

	dsim->fcmd_buf_allocated = true;

	dsim_dbg("%s -\n", __func__);

	return 0;

err_map:
	dma_buf_put(dsim->fcmd_buf);
err_share_dma_buf:
	return -ENOMEM;
}

static ssize_t dsim_fcmd_write_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dsim_device *dsim = dev_get_drvdata(dev);
	int ret = 0;
	unsigned long size;
	int xfer_cnt, xfer_sz, xfer_unit;
	u8 *fcmd_data;
	int i;

	dsim_info("%s +\n", __func__);

	if (count <= 0) {
		dsim_err("fcmd count input error\n");
		return ret;
	}

	ret = kstrtoul(buf, 0, &size);
	if (ret) {
		dsim_err("kstrtoul error\n");
		return ret;
	}

	xfer_unit = DSIM_PL_FIFO_THRESHOLD;
	if (size <= (xfer_unit - 1))
		xfer_cnt = 1;
	else {
		xfer_cnt = size / (xfer_unit - 1);
		if (size % (xfer_unit - 1))
			xfer_cnt++;
	}

	xfer_sz = size + xfer_cnt;
	if (xfer_sz > FCMD_DATA_MAX_SIZE) {
		dsim_err("fcmd transfer size error(%d, %d)\n", size, xfer_sz);
		return -1;
	}

	if (dsim->fcmd_buf_allocated)
		dev_info(dsim->dev, "memory for fcmd%d has been already allocated\n", dsim->id);
	else {
		ret = dsim_alloc_fcmd_memory(dsim->id);
		if (ret) {
			dsim_err("dsim%d : dsim_alloc_fcmd_memory fail\n", dsim->id);
			return -1;
		}
	}

	fcmd_data = (u8 *)dsim->fcmd_buf_vaddr;
	fcmd_data[0] = FCMD_SEND_START;
	for (i = 1; i < xfer_sz; i++) {
		if ((i % xfer_unit) == 0)
			fcmd_data[i] = FCMD_SEND_CONTINUE;
		else
			fcmd_data[i] = i % 256;
	}

	dsim_info("%s : input size=%d, xfer size=%d, fcmd_buf_addr=0x%lx\n",
			__func__, size, xfer_sz, dsim->fcmd_buf_data.dma_addr);

	mutex_lock(&dsim->panel->ops_lock);

	ret = dsim_write_data_fcmd(dsim, MIPI_DSI_DCS_LONG_WRITE,
					dsim->fcmd_buf_data.dma_addr, xfer_sz, true);
	if (ret)
		dsim_err("fcmd write error(%d)\n", ret);

	mutex_unlock(&dsim->panel->ops_lock);
	dsim_info("%s -\n", __func__);
	return count;
}

static DEVICE_ATTR(fcmd_wr, 0200,
	NULL,
	dsim_fcmd_write_sysfs_store);
#endif


#if IS_ENABLED(CONFIG_MCD_PANEL)

static void dsim_write_timeout_fn(struct work_struct *work)
{
	struct dsim_device *dsim =
		container_of(work, struct dsim_device, wr_timeout_work);
	struct dsim_regs regs;
	struct decon_device *decon = get_decon_drvdata(0);

	dsim_dbg("%s +\n", __func__);
	decon_hiber_block(decon);
	mutex_lock(&dsim->cmd_lock);
	if (IS_DSIM_OFF_STATE(dsim)) {
		dsim_err("%s dsim%d not ready (%s)\n",
				__func__, dsim->id, dsim_state_names[dsim->state]);
		goto exit;
	}

	/* If already FIFO empty even though the timer is no pending */
	if (dsim_reg_header_fifo_is_empty(dsim->id)) {
		reinit_completion(&dsim->ph_wr_comp);
		dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PH_FIFO_EMPTY);
		goto exit;
	}

	dsim_to_regs_param(dsim, &regs);
	__dsim_dump(dsim->id, &regs);

exit:
	mutex_unlock(&dsim->cmd_lock);
	decon_hiber_unblock(decon);
	dsim_dbg("%s -\n", __func__);
	return;
}
#endif

static void dsim_cmd_fail_detector(struct timer_list *arg)
{
	struct dsim_device *dsim = from_timer(dsim, arg, cmd_timer);
	struct decon_device *decon = get_decon_drvdata(0);
	struct dsim_regs regs;

	decon_hiber_block(decon);

	dsim_dbg("%s +\n", __func__);
	if (IS_DSIM_OFF_STATE(dsim)) {
		dsim_err("%s: DSIM is not ready. state(%d)\n", __func__,
				dsim->state);
		goto exit;
	}

	/* If already FIFO empty even though the timer is no pending */
	if (!timer_pending(&dsim->cmd_timer)
			&& dsim_reg_header_fifo_is_empty(dsim->id)) {
		reinit_completion(&dsim->ph_wr_comp);
		dsim_reg_clear_int(dsim->id, DSIM_INTSRC_SFR_PH_FIFO_EMPTY);
		goto exit;
	}

#if IS_ENABLED(CONFIG_MCD_PANEL)
	queue_work(dsim->wq, &dsim->wr_timeout_work);
#endif

	dsim_to_regs_param(dsim, &regs);
	__dsim_dump(dsim->id, &regs);

exit:
	decon_hiber_unblock(decon);
	dsim_dbg("%s -\n", __func__);
	return;
}

#if IS_ENABLED(CONFIG_EXYNOS_BTS) || IS_ENABLED(CONFIG_EXYNOS_BTS_MODULE)
static void dsim_bts_print_info(struct bts_decon_info *info)
{
	int i;

	for (i = 0; i < BTS_DPP_MAX; ++i) {
		if (!info->dpp[i].used)
			continue;

		dsim_info("\t\tDPP[%d] b(%d) s(%d %d) d(%d %d %d %d) r(%d) afbc(%d)\n",
				i, info->dpp[i].bpp,
				info->dpp[i].src_w, info->dpp[i].src_h,
				info->dpp[i].dst.x1, info->dpp[i].dst.x2,
				info->dpp[i].dst.y1, info->dpp[i].dst.y2,
				info->dpp[i].rotation, info->dpp[i].compression);
	}
}
#endif

static void dsim_underrun_info(struct dsim_device *dsim)
{
#if IS_ENABLED(CONFIG_EXYNOS_BTS) || IS_ENABLED(CONFIG_EXYNOS_BTS_MODULE)
	struct decon_device *decon;
	int i, decon_cnt;
	static ktime_t bts_info_print_block_ts;
	bool bts_info_print_blocked = true;
	u32 line_cnt;
	struct dsim_regs regs;

	if (ktime_after(ktime_get(), bts_info_print_block_ts)) {
		bts_info_print_block_ts = ktime_add_ms(ktime_get(),
				BTS_INFO_PRINT_BLOCK_TIMEOUT);
		bts_info_print_blocked = false;
	}

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	dsim_info("\tMIF(%lu), INT(%lu), DISP(%lu)\n",
			exynos_devfreq_get_domain_freq(DEVFREQ_MIF),
			exynos_devfreq_get_domain_freq(DEVFREQ_INT),
			exynos_devfreq_get_domain_freq(DEVFREQ_DISP));
#endif

	decon_cnt = get_decon_drvdata(0)->dt.decon_cnt;
	for (i = 0; i < decon_cnt; ++i) {
		decon = get_decon_drvdata(i);

		if (!decon)
			continue;

		if (!IS_DECON_ON_STATE(decon))
			continue;

		if (decon->dt.out_type == DECON_OUT_DSI) {
			line_cnt = dsim_reg_get_linecount(dsim->id, dsim->panel->lcd_info.mode);
			dsim->total_underrun_cnt++;
			dsim_info("dsim%d underrun irq occurs(%d) at line %d\n", dsim->id,
					dsim->total_underrun_cnt, line_cnt);
			dsim_info("\tDECON%d: bw(%u %u), disp(%u %u), p(%u)\n",
					decon->id,
					decon->bts.prev_total_bw,
					decon->bts.total_bw,
					decon->bts.prev_max_disp_freq,
					decon->bts.max_disp_freq,
					decon->bts.peak);

			if (!bts_info_print_blocked) {
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
				show_exynos_pm_qos_data(PM_QOS_BUS_THROUGHPUT);
				show_exynos_pm_qos_data(PM_QOS_BUS_THROUGHPUT_MAX);
				show_exynos_pm_qos_data(PM_QOS_DISPLAY_THROUGHPUT);
				show_exynos_pm_qos_data(PM_QOS_DEVICE_THROUGHPUT);
#endif
				dsim_bts_print_info(&decon->bts.bts_info);
				if (!line_cnt) {
					dsim_to_regs_param(dsim, &regs);
					__dsim_dump(dsim->id, &regs);
				}
			}
		}
	}
#endif
}

static irqreturn_t dsim_irq_handler(int irq, void *dev_id)
{
	unsigned int int_src;
	struct dsim_device *dsim = dev_id;
	struct decon_device *decon = get_decon_drvdata(0);
#if IS_ENABLED(CONFIG_EXYNOS_PD)
	int active;
#endif

	spin_lock(&dsim->slock);

#if IS_ENABLED(CONFIG_EXYNOS_PD)
	active = pm_runtime_active(dsim->dev);
	if (!active) {
		dsim_info("dsim power(%d), state(%d)\n", active, dsim->state);
		spin_unlock(&dsim->slock);
		return IRQ_HANDLED;
	}
#endif

	int_src = dsim_reg_get_int_and_clear(dsim->id);
#if defined(SYSFS_UNITTEST_INTERFACE)
	// DSIM_INTSRC_ERR_RX_ECC
	// DSIM_INTSRC_UNDER_RUN
	if (int_src & DSIM_INTSRC_UNDER_RUN)
		dsim->irq_err_state |= SYSFS_ERR_DSIM_UNDER_RUN;

	if (int_src & DSIM_INTSRC_ERR_RX_ECC)
		dsim->irq_err_state |= SYSFS_ERR_DSIM_ERR_RX_ECC;
#endif
	if (int_src & DSIM_INTSRC_SFR_PH_FIFO_EMPTY) {
		del_timer(&dsim->cmd_timer);
		complete(&dsim->ph_wr_comp);
		dsim_dbg("dsim%d PH_FIFO_EMPTY irq occurs\n", dsim->id);
	}
#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
	if (int_src & DSIM_INTSRC_SFR_PL_FIFO_EMPTY) {
		del_timer(&dsim->fcmd_timer);
		complete(&dsim->fcmd_wr_comp);
		dsim_dbg("dsim%d PL_FIFO_EMPTY irq occurs\n", dsim->id);
	}
#endif
	if (int_src & DSIM_INTSRC_RX_DATA_DONE)
		complete(&dsim->rd_comp);
	if (int_src & DSIM_INTSRC_FRAME_DONE)
		dsim_dbg("dsim%d framedone irq occurs\n", dsim->id);
	if (int_src & DSIM_INTSRC_ERR_RX_ECC)
		dsim_err("RX ECC Multibit error was detected!\n");

	if (int_src & DSIM_INTSRC_UNDER_RUN)
		dsim_underrun_info(dsim);

	if (int_src & DSIM_INTSRC_VT_STATUS) {
		dsim_dbg("dsim%d vt_status(vsync) irq occurs\n", dsim->id);
#if IS_ENABLED(CONFIG_EXYNOS_MIGOV)
		frame_vsync_cnt++;
		frame_vsync_cnt_time = ktime_get();
#endif
		if (decon) {
			decon->vsync.timestamp = ktime_get();
			wake_up_interruptible_all(&decon->vsync.wait);
		}
	}

	spin_unlock(&dsim->slock);

	return IRQ_HANDLED;
}

static int dsim_get_clocks(struct dsim_device *dsim)
{
#if !defined(CONFIG_EXYNOS_EMUL_DISP)
	dsim->res.aclk = devm_clk_get(dsim->dev, "aclk");
	if (IS_ERR_OR_NULL(dsim->res.aclk)) {
		dsim_err("failed to get aclk\n");
		return PTR_ERR(dsim->res.aclk);
	}
#endif
	return 0;
}

int dsim_reset_panel(struct dsim_device *dsim)
{
	struct v4l2_subdev *sd;

	if (IS_ERR_OR_NULL(dsim->panel)) {
		dsim_err("%s: panel ptr is NULL\n", __func__);
		return -ENOMEM;
	}

	sd = &dsim->panel->sd;
	return v4l2_subdev_call(sd, core, ioctl, EXYNOS_PANEL_IOC_RESET, NULL);
}

int dsim_set_panel_power(struct dsim_device *dsim, bool on)
{
	struct v4l2_subdev *sd;
	int ret = 0;

	if (IS_ERR_OR_NULL(dsim->panel)) {
		dsim_err("%s: panel ptr is NULL\n", __func__);
		return -ENOMEM;
	}

	sd = &dsim->panel->sd;
	if (on)
		ret = v4l2_subdev_call(sd, core, ioctl, EXYNOS_PANEL_IOC_POWERON, NULL);
	else
		ret = v4l2_subdev_call(sd, core, ioctl, EXYNOS_PANEL_IOC_POWEROFF, NULL);

	return ret;
}

#if !defined(CONFIG_UML)
static char *rpm_status_name[] = {
	"RPM_ACTIVE",
	"RPM_RESUMING",
	"RPM_SUSPENDED",
	"RPM_SUSPENDING",
};
#endif

static void dsim_print_phy_info(struct dsim_device *dsim)
{
#if !defined(CONFIG_UML)
	dsim_info("[PHY] power_count(%d), disable_depth(%d), runtime_status(%s)\n",
			dsim->phy->power_count, dsim->phy->dev.power.disable_depth,
			rpm_status_name[dsim->phy->dev.power.runtime_status]);

	dsim_info("[PHY_EX] power_count(%d), disable_depth(%d), runtime_status(%s)\n",
			dsim->phy_ex->power_count, dsim->phy_ex->dev.power.disable_depth,
			rpm_status_name[dsim->phy_ex->dev.power.runtime_status]);
#endif
}

#if !defined(CONFIG_EXYNOS_EMUL_DISP)
static int dsim_phy_power_on(struct dsim_device *dsim)
{
	int ret = 0;

	ret = phy_power_on(dsim->phy);
	if (ret < 0) {
		dsim_err("failed to enable phy(%d)\n", ret);
		goto err;
	}
	if (dsim->phy_ex) {
		ret = phy_power_on(dsim->phy_ex);
		if (ret < 0) {
			dsim_err("failed to enable extra phy(%d)\n", ret);
			goto err;
		}
	}

	return 0;

err:
	dsim_print_phy_info(dsim);
	return ret;
}
#endif

static int dsim_phy_power_off(struct dsim_device *dsim)
{
	int ret = 0;

	ret = phy_power_off(dsim->phy);
	if (ret < 0) {
		dsim_err("failed to enable phy(%d)\n", ret);
		goto err;
	}
	if (dsim->phy_ex) {
		ret = phy_power_off(dsim->phy_ex);
		if (ret < 0) {
			dsim_err("failed to enable extra phy(%d)\n", ret);
			goto err;
		}
	}

	return 0;

err:
	dsim_print_phy_info(dsim);
	return ret;
}

#if !defined(CONFIG_UML)
extern int exynos_pd_booton_rel(const char *pd_name);
#endif

static int _dsim_enable(struct dsim_device *dsim, enum dsim_state state)
{
	bool panel_ctrl;

	if (IS_DSIM_ON_STATE(dsim)) {
		dsim_warn("%s dsim already on(%s)\n",
				__func__, dsim_state_names[dsim->state]);
		dsim->state = state;
		return 0;
	}

	dsim_dbg("%s %s +\n", __func__, dsim_state_names[dsim->state]);

#if !defined(CONFIG_UML)
	exynos_pd_booton_rel("pd-dpuf0");
	exynos_pd_booton_rel("pd-dpuf1");
#endif

#if defined(CONFIG_CPU_IDLE)
	exynos_update_ip_idle_status(dsim->idle_ip_index, 0);
#endif

	pm_runtime_get_sync(dsim->dev);

#if !defined(CONFIG_EXYNOS_EMUL_DISP)
	/* DPHY power on : iso release */
	dsim_phy_power_on(dsim);
#endif

	mutex_lock(&g_dsim_lock);

#if IS_ENABLED(CONFIG_MCD_PANEL)
	panel_ctrl = (state == DSIM_STATE_ON || state == DSIM_STATE_DOZE) ? true : false;
#else
	panel_ctrl = (((dsim->state == DSIM_STATE_OFF) || (dsim->state == DSIM_STATE_INIT)) &&
			((state == DSIM_STATE_ON) || (state == DSIM_STATE_DOZE))) ?
			 true : false;
#endif

	dsim_reg_init(dsim->id, &dsim->panel->lcd_info, &dsim->clks, panel_ctrl);
	dsim_reg_start(dsim->id);

#if IS_ENABLED(CONFIG_MCD_PANEL)
#ifdef CONFIG_DYNAMIC_MIPI
	mcd_dsim_md_set_default_freq(&dsim->mcd_dsim, DM_REQ_CTX_PWR_ON);
#endif
#endif
	mutex_unlock(&g_dsim_lock);

	dsim->state = state;
	enable_irq(dsim->res.irq);

	return 0;
}

static int dsim_enable(struct dsim_device *dsim)
{
	int ret;
	enum dsim_state prev_state = dsim->state;
	enum dsim_state next_state = DSIM_STATE_ON;

	if (prev_state == next_state) {
		dsim_warn("dsim-%d %s already %s state\n", dsim->id,
				__func__, dsim_state_names[dsim->state]);
		return 0;
	}

	dsim_info("dsim-%d %s +\n", dsim->id, __func__);
	ret = _dsim_enable(dsim, next_state);
	if (ret < 0) {
		dsim_err("dsim-%d failed to set %s (ret %d)\n",
				dsim->id, dsim_state_names[next_state], ret);
		goto out;
	}

	if (prev_state != DSIM_STATE_INIT) {
#if IS_ENABLED(CONFIG_MCD_PANEL)
		ret = dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_SLEEPOUT, NULL);
#else
		ret = dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_DISPLAYON, NULL);
#endif
		if (ret < 0) {
			dsim_err("dsim-%d failed to set %s (ret %d)\n",
					dsim->id, dsim_state_names[next_state], ret);
			goto out;
		}
	}
	dsim_info("dsim-%d %s - (state:%s -> %s)\n", dsim->id, __func__,
			dsim_state_names[prev_state],
			dsim_state_names[dsim->state]);

out:
	return ret;
}

static int dsim_doze(struct dsim_device *dsim)
{
	int ret;
	enum dsim_state prev_state = dsim->state;
	enum dsim_state next_state = DSIM_STATE_DOZE;

	if (prev_state == next_state) {
		dsim_warn("dsim-%d %s already %s state\n", dsim->id,
				__func__, dsim_state_names[dsim->state]);
		return 0;
	}

	dsim_info("dsim-%d %s +\n", dsim->id, __func__);
	ret = _dsim_enable(dsim, next_state);
	if (ret < 0) {
		dsim_err("dsim-%d failed to set %s (ret %d)\n",
				dsim->id, dsim_state_names[next_state], ret);
		goto out;
	}
#if IS_ENABLED(CONFIG_MCD_PANEL)
	if (prev_state != DSIM_STATE_INIT) {
		ret = dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_DOZE, NULL);
		if (ret < 0) {
			dsim_err("dsim-%d failed to set %s (ret %d)\n",
					dsim->id, dsim_state_names[next_state], ret);
			goto out;
		}
	}
#else	
	if (prev_state == DSIM_STATE_OFF)

		dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_DOZE, NULL);
#endif

	dsim_info("dsim-%d %s - (state:%s -> %s)\n", dsim->id, __func__,
			dsim_state_names[prev_state],
			dsim_state_names[dsim->state]);

out:
	return ret;
}

static int _dsim_disable(struct dsim_device *dsim, enum dsim_state state)
{
	struct dsim_regs regs;

	if (IS_DSIM_OFF_STATE(dsim)) {
		dsim_warn("%s dsim already off(%s)\n",
				__func__, dsim_state_names[dsim->state]);
		if (state == DSIM_STATE_OFF)
			dsim_set_panel_power(dsim, 0);
		dsim->state = state;
		return 0;
	}

	dsim_dbg("%s %s +\n", __func__, dsim_state_names[dsim->state]);

	/* Wait for current read & write CMDs. */
	mutex_lock(&dsim->rd_cmd_lock);
	mutex_lock(&dsim->cmd_lock);
	del_timer(&dsim->cmd_timer);
#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
	del_timer(&dsim->fcmd_timer);
#endif
	dsim->state = state;
	mutex_unlock(&dsim->cmd_lock);
	mutex_unlock(&dsim->rd_cmd_lock);

	mutex_lock(&g_dsim_lock);

	if (dsim_reg_stop(dsim->id, dsim->data_lane) < 0) {
		dsim_to_regs_param(dsim, &regs);
		__dsim_dump(dsim->id, &regs);
	}

	mutex_unlock(&g_dsim_lock);

	disable_irq(dsim->res.irq);

	/* HACK */
#if !defined(CONFIG_EXYNOS_EMUL_DISP)
	dsim_phy_power_off(dsim);
#endif

	if (state == DSIM_STATE_OFF)
		dsim_set_panel_power(dsim, 0);

	pm_runtime_put_sync(dsim->dev);

	dsim_dbg("%s %s -\n", __func__, dsim_state_names[dsim->state]);
#if defined(CONFIG_CPU_IDLE)
	exynos_update_ip_idle_status(dsim->idle_ip_index, 1);
#endif

	return 0;
}

static int dsim_disable(struct dsim_device *dsim)
{
	int ret;
	enum dsim_state prev_state = dsim->state;
	enum dsim_state next_state = DSIM_STATE_OFF;

	if (prev_state == next_state) {
		dsim_warn("dsim-%d %s already %s state\n", dsim->id,
				__func__, dsim_state_names[dsim->state]);
		return 0;
	}

	dsim_info("dsim-%d %s +\n", dsim->id, __func__);
	dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_SUSPEND, NULL);
	ret = _dsim_disable(dsim, next_state);
	if (ret < 0) {
		dsim_err("dsim-%d failed to set %s (ret %d)\n",
				dsim->id, dsim_state_names[next_state], ret);
		goto out;
	}
	dsim_info("dsim-%d %s - (state:%s -> %s)\n", dsim->id, __func__,
			dsim_state_names[prev_state],
			dsim_state_names[dsim->state]);

out:
	return ret;
}

static int dsim_doze_suspend(struct dsim_device *dsim)
{
	int ret;
	enum dsim_state prev_state = dsim->state;
	enum dsim_state next_state = DSIM_STATE_DOZE_SUSPEND;

	if (prev_state == next_state) {
		dsim_warn("dsim-%d %s already %s state\n", dsim->id,
				__func__, dsim_state_names[dsim->state]);
		return 0;
	}

	dsim_info("dsim-%d %s +\n", dsim->id, __func__);
	dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_DOZE_SUSPEND, NULL);
	ret = _dsim_disable(dsim, next_state);
	if (ret < 0) {
		dsim_err("dsim-%d failed to set %s (ret %d)\n",
				dsim->id, dsim_state_names[next_state], ret);
		goto out;
	}
	dsim_info("dsim-%d %s - (state:%s -> %s)\n", dsim->id, __func__,
			dsim_state_names[prev_state],
			dsim_state_names[dsim->state]);

out:
	return ret;
}

static int dsim_enter_ulps(struct dsim_device *dsim)
{
	int ret = 0;

	DPU_EVENT_START();
	dsim_dbg("%s +\n", __func__);

	if (!IS_DSIM_ON_STATE(dsim)) {
		ret = -EBUSY;
		goto err;
	}

	/* Wait for current read & write CMDs. */
	mutex_lock(&dsim->rd_cmd_lock);
	mutex_lock(&dsim->cmd_lock);
	dsim->state = DSIM_STATE_ULPS;
	mutex_unlock(&dsim->cmd_lock);
	mutex_unlock(&dsim->rd_cmd_lock);

	disable_irq(dsim->res.irq);

	mutex_lock(&g_dsim_lock);

	ret = dsim_reg_stop_and_enter_ulps(dsim->id, dsim->panel->lcd_info.ddi_type,
			dsim->data_lane);

	mutex_unlock(&g_dsim_lock);

	dsim_phy_power_off(dsim);

	dpu_power_off(dsim);

#if defined(CONFIG_CPU_IDLE)
	exynos_update_ip_idle_status(dsim->idle_ip_index, 1);
#endif

	DPU_EVENT_LOG(DPU_EVT_ENTER_ULPS, &dsim->sd, start);
err:
	dsim_dbg("%s -\n", __func__);
	return ret;
}

static int dsim_exit_ulps(struct dsim_device *dsim)
{
	int ret = 0;

	DPU_EVENT_START();
	dsim_dbg("%s +\n", __func__);

	if (dsim->state != DSIM_STATE_ULPS) {
		ret = -EBUSY;
		goto err;
	}
#if defined(CONFIG_CPU_IDLE)
	exynos_update_ip_idle_status(dsim->idle_ip_index, 0);
#endif

	dpu_power_on(dsim);

#if !defined(CONFIG_EXYNOS_EMUL_DISP)
	/* DPHY power on : iso release */
	dsim_phy_power_on(dsim);
#endif

	mutex_lock(&g_dsim_lock);

	dsim_reg_init(dsim->id, &dsim->panel->lcd_info, &dsim->clks, false);
	ret = dsim_reg_exit_ulps_and_start(dsim->id, dsim->panel->lcd_info.ddi_type,
			dsim->data_lane);
	if (ret < 0)
		dsim_dump(dsim);

#if IS_ENABLED(CONFIG_MCD_PANEL)
#ifdef CONFIG_DYNAMIC_MIPI
	mcd_dsim_md_set_default_freq(&dsim->mcd_dsim, DM_REQ_CTX_HIBER);
#endif
#endif
	mutex_unlock(&g_dsim_lock);

	enable_irq(dsim->res.irq);

	dsim->state = DSIM_STATE_ON;
	DPU_EVENT_LOG(DPU_EVT_EXIT_ULPS, &dsim->sd, start);
err:
	dsim_dbg("%s -\n", __func__);

	return 0;
}

static int dsim_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct dsim_device *dsim = container_of(sd, struct dsim_device, sd);

	if (enable)
		return dsim_enable(dsim);
	else
		return dsim_disable(dsim);
}

static int dsim_set_freq_hop(struct dsim_device *dsim, struct decon_freq_hop *freq)
{
#if defined(CONFIG_EXYNOS_FREQ_HOP)
	struct stdphy_pms *pms;

	if (!IS_DSIM_ON_STATE(dsim)) {
		dsim_err("%s: dsim%d is off state\n", __func__, dsim->id);
		return -EINVAL;
	}

	pms = &dsim->panel->lcd_info.dphy_pms;
	/* If target M value is 0, frequency hopping will be disabled */
	dsim_reg_set_dphy_freq_hopping(dsim->id, pms->p, freq->target_m,
			freq->target_k, (freq->target_m > 0) ? 1 : 0);
#endif

	return 0;
}

static int dsim_free_fb_resource(struct dsim_device *dsim)
{
#if 0 /* Need fix in GKI */
	/* unmap */
	iovmm_unmap_oto(dsim->dev, dsim->fb_handover.phys_addr);
#endif
	/* unreserve memory */
	of_reserved_mem_device_release(dsim->dev);

	/* update state */
	dsim->fb_handover.reserved = false;
	dsim->fb_handover.phys_addr = 0xdead;
	dsim->fb_handover.phys_size = 0;

	return 0;
}

static int dsim_acquire_fb_resource(struct dsim_device *dsim)
{
	int ret = 0;
	/*
	 * If of_reserved_mem_device_init_by_idx returns error, it means
	 * framebuffer handover feature is disabled or reserved memory is
	 * not defined in DT.
	 *
	 * And phys_addr and phys_size is not initialized, becuase
	 * rmem_device_init callback is not called.
	 */
	ret = of_reserved_mem_device_init_by_idx(dsim->dev, dsim->dev->of_node, 0);
	if (ret) {
		dsim_warn("fb handover memory is not reserved(%d)\n", ret);
		dsim_warn("check whether fb handover memory is not reserved");
		dsim_warn("or DT definition is missed\n");
		dsim->fb_handover.reserved = false;
		return 0;
	} else {
		dsim->fb_handover.reserved = true;
	}
#if 0 /* Need fix in GKI */
	/* phys_addr and phys_size must be aligned to page size */
	ret = iovmm_map_oto(dsim->dev, dsim->fb_handover.phys_addr,
			dsim->fb_handover.phys_size);
	if (ret) {
		dsim_err("failed one to one mapping: %d\n", ret);
		BUG();
	}
#endif
	return ret;
}

static long dsim_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct dsim_device *dsim = container_of(sd, struct dsim_device, sd);
	int ret = 0;

	switch (cmd) {
	case DSIM_IOC_GET_LCD_INFO:
		v4l2_set_subdev_hostdata(sd, &dsim->panel->lcd_info);
		break;

	case DSIM_IOC_ENTER_ULPS:
		if ((unsigned long)arg)
			ret = dsim_enter_ulps(dsim);
		else
			ret = dsim_exit_ulps(dsim);
		break;

	case DSIM_IOC_DUMP:
		dsim_dump(dsim);
		break;

#if IS_ENABLED(CONFIG_MCD_PANEL)
	case DSIM_IOC_PANEL_DUMP:
		dsim_panel_dump(dsim);
		break;
#endif

	case DSIM_IOC_GET_WCLK:
		v4l2_set_subdev_hostdata(sd, &dsim->clks.word_clk);
		break;

	case EXYNOS_DPU_GET_ACLK:
#if defined(CONFIG_EXYNOS_EMUL_DISP)
		return 600000000;
#else
		return clk_get_rate(dsim->res.aclk);
#endif

	case DSIM_IOC_DOZE:
		ret = dsim_doze(dsim);
		break;

	case DSIM_IOC_DOZE_SUSPEND:
		ret = dsim_doze_suspend(dsim);
		break;

	case DSIM_IOC_SET_FREQ_HOP:
		ret = dsim_set_freq_hop(dsim, (struct decon_freq_hop *)arg);
		break;

	case DSIM_IOC_FREE_FB_RES:
		ret = dsim_free_fb_resource(dsim);
		break;

	case DSIM_IOC_RECOVERY_PROC:
		dsim_reg_recovery_process(dsim);
		break;

#if IS_ENABLED(CONFIG_MCD_PANEL)
	case DSIM_IOC_NOTIFY:
		dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_NOTIFY, arg);
		break;

	case DSIM_IOC_SET_ERROR_CB:
		if (arg == NULL) {
			dsim_err("%s invalid arg\n", __func__);
			ret = -EINVAL;
			break;
		}
		dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_SET_ERROR_CB, arg);
		break;

#ifdef CONFIG_DYNAMIC_MIPI
	case DSIM_IOC_MCD_DM_PRE_CHANGE_FREQ:
		ret = mcd_dsim_md_set_pre_freq(&dsim->mcd_dsim, (struct dm_param_info *) arg);
		break;

	case DSIM_IOC_MCD_DM_POST_CHANGE_FREQ:
		ret = mcd_dsim_md_set_post_freq(&dsim->mcd_dsim, (struct dm_param_info *)arg);
		break;
#endif
#endif

	default:
		dsim_err("unsupported ioctl");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct v4l2_subdev_core_ops dsim_sd_core_ops = {
	.ioctl = dsim_ioctl,
};

static const struct v4l2_subdev_video_ops dsim_sd_video_ops = {
	.s_stream = dsim_s_stream,
};

static const struct v4l2_subdev_ops dsim_subdev_ops = {
	.core = &dsim_sd_core_ops,
	.video = &dsim_sd_video_ops,
};

static void dsim_init_subdev(struct dsim_device *dsim)
{
	struct v4l2_subdev *sd = &dsim->sd;

	v4l2_subdev_init(sd, &dsim_subdev_ops);
	sd->owner = THIS_MODULE;
	sd->grp_id = dsim->id;
	snprintf(sd->name, sizeof(sd->name), "%s.%d", "dsim-sd", dsim->id);
	v4l2_set_subdevdata(sd, dsim);
}

#if defined(CONFIG_EXYNOS_READ_ESD_SOLUTION) && defined(CONFIG_EXYNOS_READ_ESD_SOLUTION_TEST)
static ssize_t dsim_esd_test_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned long cmd;
	struct dsim_device *dsim = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 0, &cmd);
	if (ret)
		return ret;

	dsim->esd_test = cmd;

	return count;
}
static DEVICE_ATTR(esd_test, 0644, NULL, dsim_esd_test_store);

int dsim_create_esd_test_sysfs(struct dsim_device *dsim)
{
	int ret = 0;

	ret = device_create_file(dsim->dev, &dev_attr_esd_test);
	if (ret)
		dsim_err("failed to create command read & write sysfs\n");

	return ret;
}
#endif

static int dsim_cmd_sysfs_write(struct dsim_device *dsim, bool on)
{
	int ret = 0;

	if (on)
		ret = dsim_write_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
			MIPI_DCS_SET_DISPLAY_ON, 0, false);
	else
		ret = dsim_write_data(dsim, MIPI_DSI_DCS_SHORT_WRITE,
			MIPI_DCS_SET_DISPLAY_OFF, 0, false);
	if (ret < 0)
		dsim_err("Failed to write test data!\n");
	else
		dsim_dbg("Succeeded to write test data!\n");

	return ret;
}

static int dsim_read_panel_id(struct dsim_device *dsim, u32 *id)
{
	int ret = 0;
	u8 buf[4];

	memset(buf, 0, sizeof(buf));

	/* dsim sends the request for the lcd id and gets it buffer */
#if !defined(CONFIG_EXYNOS_EMUL_DISP)
	ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ,
			MIPI_DCS_GET_DISPLAY_ID, DSIM_DDI_ID_LEN, buf);
#else
	buf[0] = 0xFF;
	buf[1] = 0xFF;
	buf[2] = 0xFF;
	buf[3] = 0x00;
#endif
	if (ret < 0) {
		dsim_err("failed to read panel id(%d)\n", ret);
		return ret;
	}

	memcpy(id, (unsigned int *)buf, sizeof(u32));
	dsim_info("suceeded to read panel id : 0x%08x\n", *id);

	return ret;
}

static int dsim_cmd_sysfs_read(struct dsim_device *dsim)
{
	u32 panel_id;

	return dsim_read_panel_id(dsim, &panel_id);
}

static ssize_t dsim_cmd_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t dsim_cmd_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned long cmd;
	struct dsim_device *dsim = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 0, &cmd);
	if (ret)
		dsim_err("%s(%d):kstrtoul failed\n", __func__, __LINE__);

	switch (cmd) {
	case 1:
		ret = dsim_cmd_sysfs_read(dsim);
		dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_DUMP, NULL);
		if (ret < 0)
			dsim_err("%s(%d):EXYNOS_PANEL_IOC_DUMP failed\n",
					__func__, __LINE__);
		break;
	case 2:
		ret = dsim_cmd_sysfs_write(dsim, true);
		dsim_info("Dsim write command, display on!!\n");
		if (ret)
			dsim_err("%s(%d):Display on cmd failed\n", __func__, __LINE__);
		break;
	case 3:
		ret = dsim_cmd_sysfs_write(dsim, false);
		dsim_info("Dsim write command, display off!!\n");
		if (ret)
			dsim_err("%s(%d):Display off cmd failed\n", __func__, __LINE__);
		break;
	default :
		dsim_info("unsupportable command\n");
		break;
	}

	return count;
}
static DEVICE_ATTR(cmd_rw, 0644, dsim_cmd_sysfs_show, dsim_cmd_sysfs_store);

int dsim_create_cmd_rw_sysfs(struct dsim_device *dsim)
{
	int ret = 0;

	ret = device_create_file(dsim->dev, &dev_attr_cmd_rw);
	if (ret)
		dsim_err("failed to create command read & write sysfs\n");

#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
	ret = device_create_file(dsim->dev, &dev_attr_fcmd_wr);
	if (ret)
		dsim_err("failed to create fast command write sysfs\n");
#endif

	return ret;
}

#if defined(SYSFS_UNITTEST_INTERFACE)
static ssize_t dsim_irq_err_sysfs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim = dev_get_drvdata(dev);
	int size = 0;
	int count;

	size = (ssize_t)sprintf(buf, "0x%X", dsim->irq_err_state);
	dsim_info("DPP(%d) IRQ State : 0x%X\n", dsim->id, dsim->irq_err_state);

	count = strlen(buf);
	return count;
}

static ssize_t dsim_irq_err_sysfs_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned long cmd;
	struct dsim_device *dsim = dev_get_drvdata(dev);

	ret = kstrtoul(buf, 0, &cmd);
	if (ret)
		return ret;

	if (cmd == 0) {
		dsim_info("DSIM(%d) IRQ State: Clear cmd\n", dsim->id);
		dsim->irq_err_state = 0;
	}
	else {
		dsim_info("DSIM(%d) IRQ State: Unknown cmd = %d\n", dsim->id, cmd);
	}

	return count;
}
static DEVICE_ATTR(dsim_irq_err, 0600, dsim_irq_err_sysfs_show, dsim_irq_err_sysfs_store);

int dsim_create_irq_err_sysfs(struct dsim_device *dsim)
{
	int ret = 0;

	ret = device_create_file(dsim->dev, &dev_attr_dsim_irq_err);
	if (ret) {
		dsim_err("failed to create dsim irq err sysfs\n");
		goto error;
	}

	error:

	return ret;
}
#endif

static int dsim_parse_dt(struct dsim_device *dsim, struct device *dev)
{
	if (IS_ERR_OR_NULL(dev->of_node)) {
		dsim_err("no device tree information\n");
		return -EINVAL;
	}

	dsim->id = of_alias_get_id(dev->of_node, "dsim");
	dsim_info("dsim(%d) probe start..\n", dsim->id);

	dsim->phy = devm_phy_get(dev, "dsim_dphy");
	if (IS_ERR_OR_NULL(dsim->phy)) {
		dsim_err("failed to get phy\n");
		return PTR_ERR(dsim->phy);
	}

	dsim->phy_ex = devm_phy_get(dev, "dsim_dphy_extra");
	if (IS_ERR_OR_NULL(dsim->phy_ex)) {
		dsim_err("failed to get extra phy. It's not mandatary.\n");
		dsim->phy_ex = NULL;
	}

	dsim->pd = dpu_get_pm_domain();

	dsim->dev = dev;

	return 0;
}

static int dsim_get_data_lanes(struct dsim_device *dsim)
{
	int i;

	if (dsim->data_lane_cnt > MAX_DSIM_DATALANE_CNT) {
		dsim_err("%d data lane couldn't be supported\n",
				dsim->data_lane_cnt);
		return -EINVAL;
	}

	dsim->data_lane = DSIM_LANE_CLOCK;
	for (i = 1; i < dsim->data_lane_cnt + 1; ++i)
		dsim->data_lane |= 1 << i;

	dsim_info("%s: lanes(0x%x)\n", __func__, dsim->data_lane);

	return 0;
}

static int dsim_init_resources(struct dsim_device *dsim, struct platform_device *pdev)
{
	struct resource *res;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dsim_err("failed to get mem resource\n");
		return -ENOENT;
	}
	dsim_info("res: start(0x%x), end(0x%x)\n", (u32)res->start, (u32)res->end);

	dsim->res.regs = devm_ioremap_resource(dsim->dev, res);
	if (IS_ERR(dsim->res.regs)) {
		dsim_err("failed to remap DSIM SFR region\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dsim_info("no 2nd mem resource\n");
		dsim->res.phy_regs = NULL;
	} else {
		dsim_info("dphy res: start(0x%x), end(0x%x)\n",
				(u32)res->start, (u32)res->end);

		dsim->res.phy_regs = devm_ioremap_resource(dsim->dev, res);
		if (IS_ERR(dsim->res.phy_regs)) {
			dsim_err("failed to remap DSIM DPHY SFR region\n");
			return -EINVAL;
		}
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res) {
		dsim_info("no extra dphy resource\n");
		dsim->res.phy_regs_ex = NULL;
	} else {
		dsim_info("dphy_extra res: start(0x%x), end(0x%x)\n",
				(u32)res->start, (u32)res->end);

		dsim->res.phy_regs_ex = devm_ioremap(dsim->dev, res->start,
							resource_size(res));
		if (IS_ERR(dsim->res.phy_regs_ex)) {
			dsim_err("failed to remap DSIM DPHY(EXTRA) SFR region\n");
			return -EINVAL;
		}
	}

#if defined(CONFIG_EXYNOS_SUPPORT_VOTF_IN)
	/* vOTF_MSCL SFR 1:1 mapping*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!res) {
		dsim_info("no vOTF_MSCL resource\n");
	} else {
		dsim_info("vOTF_MSCL: start(0x%x), end(0x%x)\n",
				(u32)res->start, (u32)res->end);

		ret = iommu_map(iommu_get_domain_for_dev(dsim->dev),
				res->start, res->start,
				res->end - res->start + 1, 0);
		if (ret) {
			dsim_err("failed to iommu_map MSCL votf SFR region\n");
			return -EINVAL;
		}

		ret = iommu_dma_reserve_iova(dsim->dev, res->start,
				res->end - res->start + 1);
		if (ret) {
			dsim_err("failed to dma_reserve MSCL votf SFR region\n");
			return -EINVAL;
		}
	}
#endif

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dsim_err("failed to get irq resource\n");
		return -ENOENT;
	}

	dsim->res.irq = res->start;
	ret = devm_request_irq(dsim->dev, res->start,
			dsim_irq_handler, 0, pdev->name, dsim);
	if (ret) {
		dsim_err("failed to install DSIM irq\n");
		return -EINVAL;
	}
	disable_irq(dsim->res.irq);

	dsim->res.ss_regs = dpu_get_sysreg_addr();
	if (IS_ERR_OR_NULL(dsim->res.ss_regs)) {
		dsim_err("failed to get sysreg addr\n");
		return -EINVAL;
	}

	return 0;
}

static int dsim_register_panel(struct dsim_device *dsim)
{

	struct dsim_regs regs;

	u32 panel_id;
	int ret;

	dsim_info("%s +\n", __func__);

	dsim->panel = get_panel_drvdata(dsim->id);
	if (dsim->panel->found == true) {
		dsim_info("%s: xres: %d, yres: %d\n",
			__func__, dsim->panel->lcd_info.xres, dsim->panel->lcd_info.yres);
		/* clock and data lane count are stored for DSIM init */
		dsim->clks.hs_clk = dsim->panel->lcd_info.hs_clk;
		dsim->clks.esc_clk = dsim->panel->lcd_info.esc_clk;
		dsim->data_lane_cnt = dsim->panel->lcd_info.data_lane;
		dsim_info("panel is already found in panel driver\n");
		return 0;
	}

#if IS_ENABLED(CONFIG_MCD_PANEL)
	dsim_err("%s can't found panel\n", __func__);
	BUG();
#endif
	pm_runtime_get_sync(dsim->dev);

#if !defined(CONFIG_EXYNOS_EMUL_DISP)
	/* DPHY power on : iso release */
	phy_power_on(dsim->phy);
	if (dsim->phy_ex)
		phy_power_on(dsim->phy_ex);
#endif

	dsim_reg_preinit(dsim->id);

	dsim_reg_start(dsim->id);

	dsim->state = DSIM_STATE_ON;
	enable_irq(dsim->res.irq);

	dsim_read_panel_id(dsim, &panel_id);

	dsim_info("panel_id = 0x%x\n", panel_id);

	ret = dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_REGISTER, &panel_id);
	if (ret) {
		dsim_err("%s: cannot find proper panel\n", __func__);
		BUG();
	}

	dsim->clks.hs_clk = dsim->panel->lcd_info.hs_clk;
	dsim->clks.esc_clk = dsim->panel->lcd_info.esc_clk;
	dsim->data_lane_cnt = dsim->panel->lcd_info.data_lane;
	dsim->data_lane = 0x1F; /* 4 data lane + 1 clock lane */

	dsim->state = DSIM_STATE_OFF;

	if (dsim_reg_stop(dsim->id, dsim->data_lane) < 0) {
		dsim_to_regs_param(dsim, &regs);
		__dsim_dump(dsim->id, &regs);
		return -EBUSY;
	}
	disable_irq(dsim->res.irq);

	phy_power_off(dsim->phy);
	if (dsim->phy_ex)
		phy_power_off(dsim->phy_ex);

	pm_runtime_put_sync(dsim->dev);

	dsim_info("%s -\n", __func__);

	return 0;
}

static struct exynos_pm_domain *dpu_get_pm_domain(void)
{
	struct platform_device *pdev = NULL;
	struct device_node *np = NULL;
	struct exynos_pm_domain *pd_temp, *pd = NULL;

	if (!IS_ENABLED(CONFIG_EXYNOS_DIRECT_PD_CTRL)) {
		dsim_info("DPU direct power domain control is disabled\n");
		return NULL;
	}

	for_each_compatible_node(np, NULL, "samsung,exynos-pd") {
		if (!of_device_is_available(np))
			continue;

		pdev = of_find_device_by_node(np);
		pd_temp = (struct exynos_pm_domain *)platform_get_drvdata(pdev);
		if (!strcmp("pd-dpu", (const char *)(pd_temp->genpd.name))) {
			pd = pd_temp;
			break;
		}
	}

	if(pd == NULL)
		dsim_err("%s: dpu pm_domain is null\n", __func__);

	dsim_info("DPU direct power domain control is enabled\n");

	return pd;
}

static int dpu_power_on(struct dsim_device *dsim)
{
#if !defined(CONFIG_EXYNOS_EMUL_DISP)
	int status;
#endif

	if (!dsim->pd) {
		pm_runtime_get_sync(dsim->dev);
		return 0;
	}

	mutex_lock(&dsim->pd->access_lock);

#if !defined(CONFIG_EXYNOS_EMUL_DISP)
	status = cal_pd_status(dsim->pd->cal_pdid);
	if (status) {
		dsim_info("%s: Already dpu power on\n",__func__);
		mutex_unlock(&dsim->pd->access_lock);
		return 0;
	}

	if (cal_pd_control(dsim->pd->cal_pdid, 1) != 0) {
		dsim_err("%s: failed to dpu power on\n", __func__);
		mutex_unlock(&dsim->pd->access_lock);
		return -1;
	}

	status = cal_pd_status(dsim->pd->cal_pdid);
	if (!status) {
		dsim_err("%s: status error : dpu power on\n", __func__);
		mutex_unlock(&dsim->pd->access_lock);
		return -1;
	}
#endif

	dsim_runtime_resume(dsim->dev);

	mutex_unlock(&dsim->pd->access_lock);
	dsim_info("dpu power on\n");

	return 0;
}

static int dpu_power_off(struct dsim_device *dsim)
{
#if !defined(CONFIG_EXYNOS_EMUL_DISP)
	int status;
#endif

	if (!dsim->pd) {
		pm_runtime_put_sync(dsim->dev);
		return 0;
	}

	mutex_lock(&dsim->pd->access_lock);

#if !defined(CONFIG_EXYNOS_EMUL_DISP)
	status = cal_pd_status(dsim->pd->cal_pdid);
	if (!status) {
		dsim_info("%s: Already dpu power off\n",__func__);
		mutex_unlock(&dsim->pd->access_lock);
		return 0;
	}

	dsim_runtime_suspend(dsim->dev);

	if (cal_pd_control(dsim->pd->cal_pdid, 0) != 0) {
		dsim_err("%s: failed to dpu power off\n", __func__);
		mutex_unlock(&dsim->pd->access_lock);
		return -1;
	}

	status = cal_pd_status(dsim->pd->cal_pdid);
	if (status) {
		dsim_err("%s: status error : dpu power off\n", __func__);
		mutex_unlock(&dsim->pd->access_lock);
		return -1;
	}
#endif

	mutex_unlock(&dsim->pd->access_lock);
	dsim_info("dpu power off\n");

	return 0;
}

static int dsim_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct dsim_device *dsim = NULL;
#if defined(CONFIG_EXYNOS_EMUL_DISP)
	void __iomem *reg;
#endif
#if IS_ENABLED(CONFIG_MCD_PANEL)
	char name[32];
	struct mcd_dsim_device *mcd_dsim;
#endif

	dsim = devm_kzalloc(dev, sizeof(struct dsim_device), GFP_KERNEL);
	if (!dsim) {
		dsim_err("failed to allocate dsim device.\n");
		ret = -ENOMEM;
		goto err;
	}

	dma_set_mask(dev, DMA_BIT_MASK(32));

	ret = dsim_parse_dt(dsim, dev);
	if (ret)
		goto err_dt;

	dsim_drvdata[dsim->id] = dsim;
	ret = dsim_get_clocks(dsim);
	if (ret)
		goto err_dt;

	spin_lock_init(&dsim->slock);
	mutex_init(&dsim->rd_cmd_lock);
	mutex_init(&dsim->cmd_lock);
	mutex_init(&dsim->rd_cmd_lock);
	init_completion(&dsim->ph_wr_comp);
	init_completion(&dsim->rd_comp);
#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
	init_completion(&dsim->fcmd_wr_comp);
#endif

	ret = dsim_init_resources(dsim, pdev);
	if (ret)
		goto err_dt;

	dsim_init_subdev(dsim);
	platform_set_drvdata(pdev, dsim);
	timer_setup(&dsim->cmd_timer, dsim_cmd_fail_detector, 0);
#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
	timer_setup(&dsim->fcmd_timer, dsim_fcmd_fail_detector, 0);
	dsim->fcmd = get_fcmd_drvdata(dsim->id);
#endif

#if IS_ENABLED(CONFIG_MCD_PANEL)
	snprintf(name, 32, "dsim%d-wq", dsim->id);
	INIT_WORK(&dsim->wr_timeout_work, dsim_write_timeout_fn);
	dsim->wq = create_workqueue(name);
#endif

#if defined(CONFIG_CPU_IDLE)
	dsim->idle_ip_index = exynos_get_idle_ip_index(dev_name(&pdev->dev), 1);
	dsim_info("dsim idle_ip_index[%d]\n", dsim->idle_ip_index);
	if (dsim->idle_ip_index < 0)
		dsim_warn("idle ip index is not provided for dsim\n");
	exynos_update_ip_idle_status(dsim->idle_ip_index, 1);
#endif

	pm_runtime_enable(dev);

	dsim_acquire_fb_resource(dsim);

	iommu_register_device_fault_handler(dev, dpu_sysmmu_fault_handler_dsim, NULL);

#if !defined(CONFIG_EXYNOS_EMUL_DISP)
	phy_init(dsim->phy);
	if (dsim->phy_ex)
		phy_init(dsim->phy_ex);
#else
	reg = ioremap(0x15860710, 0x10);
	writel(0x1, reg);
#endif

	dsim_register_panel(dsim);

	ret = dsim_get_data_lanes(dsim);
	if (ret)
		goto err_dt;

	dsim->state = DSIM_STATE_INIT;

#if !IS_ENABLED(CONFIG_MCD_PANEL)
	dsim_reg_init(dsim->id, &dsim->panel->lcd_info, &dsim->clks, false);
	dsim_reg_start(dsim->id);

	enable_irq(dsim->res.irq);
#endif
	if (!IS_ENABLED(CONFIG_EXYNOS_VIRTUAL_DISPLAY))
		dsim_enable(dsim);

#if IS_ENABLED(CONFIG_MCD_PANEL)
	dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_PROBE, NULL);
	dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_SLEEPOUT, NULL);

#ifdef CONFIG_DYNAMIC_MIPI
	mcd_dsim = &dsim->mcd_dsim;
	ret = mcd_dsim_get_panel_v4l2_subdev(mcd_dsim);
	if (ret) {
		dsim_err("ERR:%s:failed to get panel v4l2 subdev\n");
		goto skip_get_dm_info;
	}

	mcd_dsim->dm_info = mcd_dm_get_info(mcd_dsim->panel_drv_sd);
	if (mcd_dsim->dm_info != NULL) {
		dsim_info("%s success get dm info\n", __func__);
		dsim_info("%s default hs: %d\n", __func__,
			mcd_dsim->dm_info->dm_dt.dm_hs_list[mcd_dsim->dm_info->dm_dt.dm_default].hs_clk);
	}
skip_get_dm_info:
#endif
#endif
	

#if defined(BRINGUP_DSIM_BIST)
	/* TODO: This is for dsim BIST mode in zebu emulator. only for test*/
	dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_DISPLAYON, NULL);
	dsim_reg_set_bist(dsim->id, true);
#endif


#if defined(SYSFS_UNITTEST_INTERFACE)
	dsim_create_irq_err_sysfs(dsim);
#endif

	/* for debug */
	/* dsim_dump(dsim); */

	dsim_create_cmd_rw_sysfs(dsim);
	

#if defined(CONFIG_EXYNOS_READ_ESD_SOLUTION)
	dsim->esd_recovering = false;
#if defined(CONFIG_EXYNOS_READ_ESD_SOLUTION_TEST)
	dsim_create_esd_test_sysfs(dsim);
#endif
#endif

#ifdef DPHY_LOOP
	dsim_reg_set_dphy_loop_back_test(dsim->id);
#endif

#if IS_ENABLED(CONFIG_EXYNOS_MIGOV)
	exynos_migov_register_vsync_cnt(get_frame_vsync_cnt);
#endif

	dsim_info("dsim%d driver(%s mode) has been probed.\n", dsim->id,
		dsim->panel->lcd_info.mode == DECON_MIPI_COMMAND_MODE ? "cmd" : "video");
	return 0;

err_dt:
	kfree(dsim);
err:
	return ret;
}

static int dsim_remove(struct platform_device *pdev)
{
	struct dsim_device *dsim = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	mutex_destroy(&dsim->rd_cmd_lock);
	mutex_destroy(&dsim->cmd_lock);
	mutex_destroy(&dsim->rd_cmd_lock);
	dsim_info("dsim%d driver removed\n", dsim->id);

	return 0;
}

static void dsim_shutdown(struct platform_device *pdev)
{
#if 0
	struct dsim_device *dsim = platform_get_drvdata(pdev);

	DPU_EVENT_LOG(DPU_EVT_DSIM_SHUTDOWN, &dsim->sd, ktime_set(0, 0));
	dsim_info("%s + state:%d\n", __func__, dsim->state);

	dsim_disable(dsim);

	dsim_info("%s -\n", __func__);
#else
	dsim_info("%s +-\n", __func__);
#endif
}

static int dsim_runtime_suspend(struct device *dev)
{
	struct dsim_device *dsim = dev_get_drvdata(dev);

	DPU_EVENT_LOG(DPU_EVT_DSIM_SUSPEND, &dsim->sd, ktime_set(0, 0));
	dsim_dbg("%s +\n", __func__);
#if defined(CONFIG_EXYNOS_DIRECT_PD_CTRL)
	exynos_sysmmu_control(dsim->dev, false);
#endif
#if !defined(CONFIG_EXYNOS_EMUL_DISP)
	clk_disable_unprepare(dsim->res.aclk);
#endif
	dsim_dbg("%s -\n", __func__);
	return 0;
}

static int dsim_runtime_resume(struct device *dev)
{
	struct dsim_device *dsim = dev_get_drvdata(dev);

	DPU_EVENT_LOG(DPU_EVT_DSIM_RESUME, &dsim->sd, ktime_set(0, 0));
	dsim_dbg("%s: +\n", __func__);
#if defined(CONFIG_EXYNOS_DIRECT_PD_CTRL)
	exynos_sysmmu_control(dsim->dev, true);
#endif
#if !defined(CONFIG_EXYNOS_EMUL_DISP)
	clk_prepare_enable(dsim->res.aclk);
#endif
	dsim_dbg("%s -\n", __func__);
	return 0;
}

static const struct of_device_id dsim_of_match[] = {
	{ .compatible = "samsung,exynos9-dsim" },
	{},
};
MODULE_DEVICE_TABLE(of, dsim_of_match);

static const struct dev_pm_ops dsim_pm_ops = {
	.runtime_suspend	= dsim_runtime_suspend,
	.runtime_resume		= dsim_runtime_resume,
};

struct platform_driver dsim_driver __refdata = {
	.probe			= dsim_probe,
	.remove			= dsim_remove,
	.shutdown		= dsim_shutdown,
	.driver = {
		.name		= DSIM_MODULE_NAME,
		.owner		= THIS_MODULE,
		.pm		= &dsim_pm_ops,
		.of_match_table	= of_match_ptr(dsim_of_match),
		.suppress_bind_attrs = true,
	}
};

/*
 * rmem_device_init is called in of_reserved_mem_device_init_by_idx function
 * when reserved memory is required.
 */
static int rmem_device_init(struct reserved_mem *rmem, struct device *dev)
{
	struct dsim_device *dsim = dev_get_drvdata(dev);

	dsim_info("%s +\n", __func__);
	dsim->fb_handover.phys_addr = rmem->base;
	dsim->fb_handover.phys_size = rmem->size;
	dsim_info("%s -\n", __func__);

	return 0;
}

/*
 * rmem_device_release is called in of_reserved_mem_device_release function
 * when reserved memory is no longer required.
 */
static void rmem_device_release(struct reserved_mem *rmem, struct device *dev)
{
	struct page *first = phys_to_page(PAGE_ALIGN(rmem->base));
	struct page *last = phys_to_page((rmem->base + rmem->size) & PAGE_MASK);
	struct page *page;

	pr_info("%s: base=%pa, size=%pa, first=%pa, last=%pa\n",
			__func__, &rmem->base, &rmem->size, first, last);

	for (page = first; page != last; page++) {
		__ClearPageReserved(page);
		set_page_count(page, 1);
		__free_pages(page, 0);
		adjust_managed_page_count(page, 1);
	}
}

static const struct reserved_mem_ops rmem_ops = {
	.device_init	= rmem_device_init,
	.device_release = rmem_device_release,
};

static int __init fb_handover_setup(struct reserved_mem *rmem)
{
	pr_info("%s: base=%pa, size=%pa\n", __func__, &rmem->base, &rmem->size);

	rmem->ops = &rmem_ops;
	return 0;
}
RESERVEDMEM_OF_DECLARE(fb_handover, "exynos,fb_handover", fb_handover_setup);

MODULE_AUTHOR("Yeongran Shin <yr613.shin@samsung.com>");
MODULE_DESCRIPTION("Samusung EXYNOS DSIM driver");
MODULE_LICENSE("GPL");
