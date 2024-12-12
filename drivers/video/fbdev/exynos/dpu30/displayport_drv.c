/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung SoC DisplayPort driver.
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
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/pm_runtime.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include <linux/module.h>
#include <video/mipi_display.h>
#include <linux/regulator/consumer.h>
#include <media/v4l2-dv-timings.h>
#include <linux/debugfs.h>
#if defined(CONFIG_CPU_IDLE)
#include <soc/samsung/exynos-cpupm.h>
#endif
#include <linux/smc.h>
#include <linux/iommu.h>
#if IS_ENABLED(CONFIG_ANDROID_SWITCH)
#include <linux/switch.h>
#elif defined(CONFIG_EXTCON)
#include "../../../../drivers/extcon/extcon.h"
#endif
#if IS_ENABLED(CONFIG_PHY_EXYNOS_USBDRD)
#include "../../../drivers/phy/samsung/phy-exynos-usbdrd.h"
#endif
#include "displayport.h"
#include "displayport_hdcp22_if.h"
#include "decon.h"
#ifdef CONFIG_SEC_DISPLAYPORT_SELFTEST
#include "../dp_logger/dp_self_test.h"
#endif

#if defined(CONFIG_UML)
int phy_status = 1;
#endif


#define PIXELCLK_2160P30HZ 297000000 /* UHD 30hz */
#define PIXELCLK_1080P60HZ 148500000 /* FHD 60Hz */
#define PIXELCLK_1080P30HZ 74250000 /* FHD 30Hz */

/* DP debug module sysfs */
int displayport_log_level = 6;
module_param(displayport_log_level, int, 0644);
int displayport_sst1_bist_test = 0;
module_param(displayport_sst1_bist_test, int, 0644);
int displayport_sst2_bist_test = 0;
module_param(displayport_sst2_bist_test, int, 0644);

int displayport_hdp_link_tr_fail = 0;
module_param(displayport_hdp_link_tr_fail, int, 0644);
int displayport_hdp_read_fail = 0;
module_param(displayport_hdp_read_fail, int, 0644);

/* force link rate for test
 * LINK_RATE_1_62Gbps 0x06
 * LINK_RATE_2_7Gbps 0x0A
 * LINK_RATE_5_4Gbps 0x14
 * LINK_RATE_8_1Gbps 0x1E
 */
int forced_linkrate = 0;
module_param(forced_linkrate, int, 0644);

/* force hdcp version for test
 * HDCP_VERSION_1_3 13
 * HDCP_VERSION_2_2 22
 */
int forced_hdcp = 0;
module_param(forced_hdcp, int, 0644);

static u64 reduced_resolution;
struct displayport_debug_param g_displayport_debug_param;

extern enum hdcp22_auth_def hdcp22_auth_state;
struct displayport_device *displayport_drvdata;
EXPORT_SYMBOL(displayport_drvdata);

#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_DISPLAYPORT)
struct blocking_notifier_head dp_ado_notifier_head =
		BLOCKING_NOTIFIER_INIT(dp_ado_notifier_head);
EXPORT_SYMBOL(dp_ado_notifier_head);
#endif

extern u32 phy_tune_parameters[4][4][5];

static int displayport_runtime_suspend(struct device *dev);
static int displayport_runtime_resume(struct device *dev);
static enum displayport_state displayport_check_sst_on(struct displayport_device *displayport);

void displayport_hdcp22_enable(u32 en);
extern void displayport_register_func(void (*func0)(u32 en), int (*func1)(u32 address, u32 length, u8 *data), int (*func2)(u32 address, u32 length, u8 *data));

#define PREFIX_LEN      40
#define ROW_LEN         32
void displayport_print_hex_dump(void __iomem *regs, const void *buf,
		size_t len)
{
	char prefix_buf[PREFIX_LEN];
	unsigned long p;
	size_t i, row;

	for (i = 0; i < len; i += ROW_LEN) {
		p = buf - regs + i;

		if (len - i < ROW_LEN)
			row = len - i;
		else
			row = ROW_LEN;

		snprintf(prefix_buf, sizeof(prefix_buf), "[%08lX] ", p);
		print_hex_dump(KERN_NOTICE, prefix_buf, DUMP_PREFIX_NONE,
				32, 4, buf + i, row, false);
	}
}

void displayport_dump_registers(struct displayport_device *displayport)
{
	displayport_info("=== DisplayPort SFR DUMP ===\n");
	displayport_print_hex_dump(displayport->res.link_regs,
			displayport->res.link_regs, 0xC0);
	displayport_print_hex_dump(displayport->res.link_regs,
			displayport->res.link_regs + 0x100, 0x0C);
	displayport_print_hex_dump(displayport->res.link_regs,
			displayport->res.link_regs + 0x200, 0x08);
	displayport_print_hex_dump(displayport->res.link_regs,
			displayport->res.link_regs + 0x2000, 0x64);
	displayport_print_hex_dump(displayport->res.link_regs,
			displayport->res.link_regs + 0x3000, 0x130);
	displayport_print_hex_dump(displayport->res.link_regs,
			displayport->res.link_regs + 0x5000, 0x104);
	displayport_print_hex_dump(displayport->res.link_regs,
			displayport->res.link_regs + 0x5400, 0x9AC);
	displayport_print_hex_dump(displayport->res.link_regs,
			displayport->res.link_regs + 0x6000, 0x104);
	displayport_print_hex_dump(displayport->res.link_regs,
			displayport->res.link_regs + 0x6400, 0x46C);
}

void displayport_phy_dump_registers(struct displayport_device *displayport)
{
	displayport_info("=== DisplayPort Phy SFR DUMP ===\n");
	displayport_print_hex_dump(displayport->res.phy_regs,
			displayport->res.phy_regs, 0x650);
	displayport_print_hex_dump(displayport->res.phy_regs,
			displayport->res.phy_regs + 0x700, 0xAC);
	displayport_print_hex_dump(displayport->res.phy_regs,
			displayport->res.phy_regs + 0x800, 0xED4 - 0x800);
	displayport_print_hex_dump(displayport->res.phy_regs,
			displayport->res.phy_regs + 0xF00, 0x10B8 - 0xF00);
	displayport_print_hex_dump(displayport->res.phy_regs,
			displayport->res.phy_regs + 0x1400, 0x163C - 0x1400);
	displayport_print_hex_dump(displayport->res.phy_regs,
			displayport->res.phy_regs + 0x1800, 0x1ED4 - 0x1800);
	displayport_print_hex_dump(displayport->res.phy_regs,
			displayport->res.phy_regs + 0x1F00, 0x20B8 - 0x1F00);
	displayport_print_hex_dump(displayport->res.phy_regs,
			displayport->res.phy_regs + 0x2400, 0x263C - 0x2400);
}

static int switch_state;
#if IS_ENABLED(CONFIG_ANDROID_SWITCH)
static struct switch_dev switch_secdp_hpd = {
	.name = "hdmi",
};
static struct switch_dev switch_secdp_msg = {
	.name = "secdp_msg",
};

static void displayport_set_switch_poor_connect(void)
{
	struct displayport_device *displayport = get_displayport_drvdata();

	if (++displayport->poor_connect_count > MAX_POOR_CONNECT_EVENT)
		return;

	displayport_err("set poor connect switch event\n");
	switch_set_state(&switch_secdp_msg, 1);
	switch_set_state(&switch_secdp_msg, 0);
}

/*for HDMI_PLUGGED intent*/
static void displayport_set_switch_hpd_state(int state)
{
	switch_set_state(&switch_secdp_hpd, state);
}
#elif defined(CONFIG_EXTCON)
static void displayport_set_switch_poor_connect(void)
{
	struct displayport_device *displayport = get_displayport_drvdata();
	char *envp[3];

	if (++displayport->poor_connect_count > MAX_POOR_CONNECT_EVENT)
		return;

	/* use same path and state for compatibility with legacy switch event */
	envp[0] = "DEVPATH=/devices/virtual/switch/secdp_msg";
	envp[1] = "SWITCH_STATE=1";
	envp[2] = NULL;

	displayport_err("send poor connect uevent\n");
	kobject_uevent_env(&displayport->dev->kobj, KOBJ_CHANGE, envp);
}

static struct extcon_dev *dp_hpd_extcon;
static const unsigned int dp_hpd_extcon_id[] = {
	EXTCON_DISP_HDMI,
	EXTCON_NONE,
};

/*for HDMI_PLUGGED intent*/
static void displayport_set_switch_hpd_state(int state)
{
	if (dp_hpd_extcon)
		extcon_set_state_sync(dp_hpd_extcon, EXTCON_DISP_HDMI, state);
}
#else
static void displayport_set_switch_poor_connect(void)
{
	displayport_err("ERROR: Need functions for uevents\n");
}
static void displayport_set_switch_hpd_state(int state)
{
	displayport_err("ERROR: Need functions for uevents\n");
}
#endif

static int displayport_remove(struct platform_device *pdev)
{
	struct displayport_device *displayport = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
#if IS_ENABLED(CONFIG_ANDROID_SWITCH)
	switch_dev_unregister(&switch_secdp_msg);
	switch_dev_unregister(&switch_secdp_hpd);
#endif

#if defined(CONFIG_EXTCON)
	devm_extcon_dev_unregister(displayport->dev, displayport->extcon_displayport);
#else
	displayport_info("Not compiled EXTCON driver\n");
#endif
	mutex_destroy(&displayport->cmd_lock);
	mutex_destroy(&displayport->hpd_lock);
	mutex_destroy(&displayport->aux_lock);
	mutex_destroy(&displayport->training_lock);
	mutex_destroy(&displayport->hdcp2_lock);
	destroy_workqueue(displayport->dp_wq);
	destroy_workqueue(displayport->hdcp2_wq);
	displayport_info("displayport driver removed\n");

	return 0;
}

u32 displayport_get_decon_id(u32 sst_id)
{
	u32 decon_id = SST1_DECON_ID;

	switch (sst_id) {
	case SST1:
		decon_id = SST1_DECON_ID;
		break;
	case SST2:
		decon_id = SST2_DECON_ID;
		break;
	default:
		decon_id = SST1_DECON_ID;
	}

	return decon_id;
}

u32 displayport_get_sst_id_with_decon_id(u32 decon_id)
{
	u32 i = 0;
	int ret_val = SST1;
	struct displayport_device *displayport = get_displayport_drvdata();

	for (i = SST1; i < MAX_SST_CNT; i++) {
		if (decon_id == displayport->sst[i]->decon_id) {
			if (displayport_log_level > 8)
				displayport_dbg("SST%d connect DECON%d\n", i + 1, decon_id);
			ret_val = displayport->sst[i]->id;
			break;
		}
	}

	return ret_val;
}

int displayport_get_sst_id_with_port_number(u8 port_number, u8 get_sst_id_type)
{
	int i = 0;
	int ret_val = 0;
	struct displayport_device *displayport = get_displayport_drvdata();

	if (get_sst_id_type == FIND_SST_ID) {
		for (i = SST1; i < MAX_SST_CNT; i++) {
			if (port_number == displayport->sst[i]->vc_config->port_num) {
				displayport_dbg("SST%d's port number = %d\n", i + 1,
						displayport->sst[i]->vc_config->port_num);
				break;
			}
		}
	} else if (get_sst_id_type == ALLOC_SST_ID) {
		for (i = SST1; i < MAX_SST_CNT; i++) {
			if (displayport->sst[i]->hpd_state == HPD_UNPLUG) {
				displayport_dbg("SST%d's port number = %d\n", i + 1, port_number);
				displayport->sst[i]->vc_config->port_num = port_number;
				break;
			}
		}
	}

	if (i >= MAX_SST_CNT) {
		displayport_dbg("Can't get SST ID with port number\n");
		ret_val = -EINVAL;
	} else
		ret_val = displayport->sst[i]->id;

	return ret_val;
}

void displayport_clr_vc_config(u32 ch,
		struct displayport_device *displayport)
{
	displayport->sst[ch]->vc_config->port_num = 0;
	displayport->sst[ch]->vc_config->timeslot = 0;
	displayport->sst[ch]->vc_config->timeslot_start_no = 0;
	displayport->sst[ch]->vc_config->pbn = 0;
	displayport->sst[ch]->vc_config->xvalue = 0;
	displayport->sst[ch]->vc_config->yvalue = 0;
	displayport->sst[ch]->vc_config->mode_enable = 0;
}

int displayport_calc_vc_config(u32 ch,
		struct displayport_device *displayport)
{
	int ret = 0;
	u64 pixelbandwidth = 0;
	u32 bitsperpixel = 24;
	u64 linkbandwidth = 0;
	u64 calc_temp = 0;
	u32 lane_cnt = displayport_reg_get_lane_count();

	displayport->sst[ch]->vc_config->mode_enable = DPTX_MST_FEC_DSC_DISABLE;

	switch (displayport->sst[ch]->bpc) {
	case BPC_8:
		bitsperpixel = 24;
		break;
	case BPC_10:
		bitsperpixel = 30;
		break;
	default:
		bitsperpixel = 18;
		break;
	}

	pixelbandwidth = (displayport_reg_get_video_clk(ch) / 1000) * (bitsperpixel / 8);
	displayport->sst[ch]->vc_config->pbn =
			(((pixelbandwidth * (64000 / 54)) / 1000) * 1006 / 1000) / 1000 + 1;
	displayport_info("CH%d PBN = %d", ch + 1, displayport->sst[ch]->vc_config->pbn);

	linkbandwidth = displayport_reg_get_ls_clk() * lane_cnt;
	displayport->sst[ch]->vc_config->timeslot =
			(pixelbandwidth / (linkbandwidth / 1000000)) * 64 / 1000 + 1;
	displayport_info("CH%d TimeSlot = %d", ch + 1, displayport->sst[ch]->vc_config->timeslot);

	calc_temp = ((displayport->sst[ch]->vc_config->pbn * 54 * 1000)
					/ (linkbandwidth / 1000000)) * lane_cnt;
	displayport->sst[ch]->vc_config->xvalue = calc_temp / 1000;
	displayport->sst[ch]->vc_config->yvalue = calc_temp - (displayport->sst[ch]->vc_config->xvalue * 1000);
	displayport_info("CH%d StreamSymbolTimeSlotsPerMTP = %d.%d", ch + 1,
			displayport->sst[ch]->vc_config->xvalue, displayport->sst[ch]->vc_config->yvalue);

	return ret;
}

static u64 displayport_find_edid_max_pixelclock(void)
{
	int i;
	u64 max_pclk = 0;

	for (i = supported_videos_pre_cnt - 1; i > 0; i--) {
		if (supported_videos[i].edid_support_match &&
				supported_videos[i].dv_timings.bt.pixelclock > max_pclk)
			max_pclk = supported_videos[i].dv_timings.bt.pixelclock;
	}
	displayport_info("find max pclk : %llu\n", max_pclk);
	return max_pclk;

}

static int displayport_check_edid_max_clock(u32 sst_id,
		struct displayport_device *displayport, videoformat video_format, enum bit_depth bpc)
{
	int ret_val = true;
	u64 calc_pixel_clock = 0;

	switch (bpc) {
	case BPC_8:
		calc_pixel_clock = supported_videos[video_format].dv_timings.bt.pixelclock;
		break;
	case BPC_10:
		calc_pixel_clock = supported_videos[video_format].dv_timings.bt.pixelclock * 125 / 100;
		break;
	default:
		calc_pixel_clock = supported_videos[video_format].dv_timings.bt.pixelclock;
		break;
	}

	if (displayport->sst[sst_id]->rx_edid_data.max_support_clk != 0) {
		if (calc_pixel_clock > displayport->sst[sst_id]->rx_edid_data.max_support_clk * MHZ) {
			displayport_info("RX support Max TMDS Clock = %d, but pixel clock = %llu\n",
					displayport->sst[sst_id]->rx_edid_data.max_support_clk * MHZ, calc_pixel_clock);
			ret_val = false;
		}
	} else
		displayport_info("Can't check RX support Max TMDS Clock\n");

	return ret_val;
}

static int displayport_check_link_rate_pixel_clock(u8 link_rate,
		u8 lane_cnt, u64 pixel_clock, enum bit_depth bpc)
{
	u64 calc_pixel_clock = 0;
	u64 pixel_clock_with_bpc = 0;
	int ret_val = false;

	switch (link_rate) {
	case LINK_RATE_1_62Gbps:
		calc_pixel_clock = RBR_PIXEL_CLOCK_PER_LANE * lane_cnt;
		break;
	case LINK_RATE_2_7Gbps:
		calc_pixel_clock = HBR_PIXEL_CLOCK_PER_LANE * lane_cnt;
		break;
	case LINK_RATE_5_4Gbps:
		calc_pixel_clock = HBR2_PIXEL_CLOCK_PER_LANE * lane_cnt;
		break;
	case LINK_RATE_8_1Gbps:
		calc_pixel_clock = HBR3_PIXEL_CLOCK_PER_LANE * lane_cnt;
		break;
	default:
		calc_pixel_clock = HBR2_PIXEL_CLOCK_PER_LANE * lane_cnt;
		break;
	}

	switch (bpc) {
	case BPC_8:
		pixel_clock_with_bpc = pixel_clock;
		break;
	case BPC_10:
		pixel_clock_with_bpc = pixel_clock * 125 / 100;
		break;
	default:
		pixel_clock_with_bpc = pixel_clock;
		break;
	}

	if (calc_pixel_clock >= pixel_clock_with_bpc)
		ret_val = true;

	if (ret_val == false)
		displayport_info("link rate: 0x%x, lane cnt: %d, pixel_clock_with_bpc = %llu, calc_pixel_clock = %llu\n",
				link_rate, lane_cnt, pixel_clock_with_bpc, calc_pixel_clock);

	return ret_val;
}

static int displayport_check_pixel_clock_for_hdr(u32 sst_id,
		struct displayport_device *displayport,	videoformat video_format)
{
	int ret_val = false;

	if (displayport->sst[sst_id]->rx_edid_data.hdr_support) {
		ret_val = displayport_check_edid_max_clock(sst_id, displayport, video_format, BPC_10);

		if (ret_val == true)
			ret_val = displayport_check_link_rate_pixel_clock(displayport_reg_phy_get_link_bw(),
							displayport_reg_get_lane_count(),
							supported_videos[video_format].dv_timings.bt.pixelclock,
							BPC_10);
	}

	return ret_val;
}

static int displayport_get_min_link_rate(u8 rx_link_rate,
		u8 lane_cnt, enum bit_depth bpc)
{
	int i = 0;
	int link_rate[MAX_LINK_RATE_NUM] = {LINK_RATE_1_62Gbps,
			LINK_RATE_2_7Gbps, LINK_RATE_5_4Gbps, LINK_RATE_8_1Gbps};
	u64 max_pclk = 0;
	u8 min_link_rate = 0;

	if (rx_link_rate == LINK_RATE_1_62Gbps)
		return rx_link_rate;

	if (lane_cnt > 4)
		return LINK_RATE_5_4Gbps;

	max_pclk = displayport_find_edid_max_pixelclock();
	for (i = 0; i < MAX_LINK_RATE_NUM; i++) {
		if (displayport_check_link_rate_pixel_clock(link_rate[i],
				lane_cnt, max_pclk, bpc) == true)
			break;
	}

	if (i >= MAX_LINK_RATE_NUM)
		min_link_rate = LINK_RATE_5_4Gbps;
	else
		min_link_rate = link_rate[i] > rx_link_rate ? rx_link_rate : link_rate[i];

	displayport_info("set link late: 0x%x, lane cnt:%d\n", min_link_rate, lane_cnt);

	return min_link_rate;
}

void displayport_get_voltage_and_pre_emphasis_max_reach(u8 *drive_current, u8 *pre_emphasis, u8 *max_reach_value)
{
	int i;

	for (i = 0; i < 4; i++) {
		if (drive_current[i] >= MAX_REACHED_CNT) {
			max_reach_value[i] &= ~(1 << MAX_SWING_REACHED_BIT_POS);
			max_reach_value[i] |= (1 << MAX_SWING_REACHED_BIT_POS);
		} else
			max_reach_value[i] &= ~(1 << MAX_SWING_REACHED_BIT_POS);

		if (pre_emphasis[i] >= MAX_REACHED_CNT) {
			max_reach_value[i] &= ~(1 << MAX_PRE_EMPHASIS_REACHED_BIT_POS);
			max_reach_value[i] |= (1 << MAX_PRE_EMPHASIS_REACHED_BIT_POS);
		} else
			max_reach_value[i] &= ~(1 << MAX_PRE_EMPHASIS_REACHED_BIT_POS);
	}
}

u8 displayport_get_max_lane_count_from_pin_assignment(struct displayport_device *displayport)
{
	u8 lane_count = 4;

	switch (displayport->pdic_notify_dp_conf) {
	case PDIC_NOTIFY_DP_PIN_B:
	case PDIC_NOTIFY_DP_PIN_D:
	case PDIC_NOTIFY_DP_PIN_F:
		displayport_info("support 2 lanes pin_assignment\n");
		lane_count = 2;
		break;
	default:
		break;
	}

	return lane_count;
}

static int displayport_full_link_training(u32 sst_id)
{
	u8 link_rate;
	u8 lane_cnt;
	u8 training_aux_rd_interval;
	u8 pre_emphasis[MAX_LANE_CNT];
	u8 drive_current[MAX_LANE_CNT];
	u8 voltage_swing_lane[MAX_LANE_CNT];
	u8 pre_emphasis_lane[MAX_LANE_CNT];
	u8 max_reach_value[MAX_LANE_CNT];
	int training_retry_no, eq_training_retry_no, i;
	int total_retry_cnt = 0;
	u8 val[DPCD_BUF_SIZE] = {0,};
	u8 eq_val[DPCD_BUF_SIZE] = {0,};
	u8 lane_cr_done;
	u8 lane_channel_eq_done;
	u8 lane_symbol_locked_done;
	u8 interlane_align_done;
	u8 enhanced_frame_cap;
	int ret = 0;
	int tps3_supported = 0;
	int tps4_supported = 0;
	enum bit_depth bpc = BPC_8;
	struct displayport_device *displayport = get_displayport_drvdata();
	struct decon_device *decon = get_decon_drvdata(DEFAULT_DECON_ID);

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	if (displayport->pdic_cable_state == PDIC_NOTIFY_DETACH) {
		displayport_err("pdic cable is detached\n");
		return -ENODEV;
	}
#endif

	ret = displayport_reg_dpcd_read_burst(DPCD_ADD_REVISION_NUMBER, DPCD_BUF_SIZE, val);
	if (ret) {
		displayport_info("aux fail in linktrining\n");
		return -EINVAL;
	}
	displayport_info("Full Link Training Start + : %02x %02x %02x\n", val[1], val[2], val[3]);
	if (!displayport->hpd_current_state) {
		displayport_info("hpd is low in full link training\n");
		return 0;
	}

	link_rate = val[1];
	lane_cnt = val[2] & MAX_LANE_COUNT;
	tps3_supported = val[2] & TPS3_SUPPORTED;
	enhanced_frame_cap = val[2] & ENHANCED_FRAME_CAP;
	tps4_supported = val[3] & TPS4_SUPPORTED;

	lane_cnt = min_t(u8, lane_cnt, displayport_get_max_lane_count_from_pin_assignment(displayport));

	if (link_rate == 0 || lane_cnt == 0) {
		displayport_err("Can't read link rate or lane count in full link training\n");
		return -EINVAL;
	}

#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	secdp_bigdata_save_item(BD_MAX_LANE_COUNT, lane_cnt);
	secdp_bigdata_save_item(BD_MAX_LINK_RATE, link_rate);
#endif

	if (link_rate > LINK_RATE_5_4Gbps) {
		displayport_info("HBR3 not support. reduce to HBR2\n");
		link_rate = LINK_RATE_5_4Gbps;
	}

	if (!displayport->auto_test_mode &&
			!(supported_videos[displayport->sst[sst_id]->best_video].pro_audio_support &&
			edid_support_pro_audio())) {
		if (displayport->sst[sst_id]->rx_edid_data.hdr_support)
			bpc = BPC_10;

		if (displayport->mst_cap == 1) {
			enhanced_frame_cap = 0;
			displayport_info("enhanced_frame_cap disable for MST mode\n");
		} else {
			link_rate = displayport_get_min_link_rate(link_rate, lane_cnt, bpc);
			displayport->auto_test_mode = 0;
		}
	}

	if (g_displayport_debug_param.param_used) {
		link_rate = g_displayport_debug_param.link_rate;
		lane_cnt = g_displayport_debug_param.lane_cnt;
		displayport_info("link training test lane:%d, rate:0x%x, tps4:0x%x\n",
				lane_cnt, link_rate, tps4_supported);
	}

	displayport_reg_dpcd_read(DPCD_ADD_TRAINING_AUX_RD_INTERVAL, 1, val);
	training_aux_rd_interval = val[0] & 0x7F;

	if (forced_linkrate)
		link_rate = forced_linkrate;
Reduce_Link_Rate_Retry:
	displayport_info("Reduce_Link_Rate_Retry(0x%X)\n", link_rate);

	if (!displayport->hpd_current_state) {
		displayport_info("hpd is low in link rate retry\n");
		return 0;
	}

	for (i = 0; i < 4; i++) {
		pre_emphasis[i] = 0;
		drive_current[i] = 0;
		max_reach_value[i] = 0;
	}

	training_retry_no = 0;

	if (decon->state != DECON_STATE_ON
		|| displayport_reg_phy_get_link_bw() != link_rate
		|| displayport_reg_get_lane_count() != lane_cnt) {

		if (decon->state == DECON_STATE_ON) {
			displayport_info("phy_reset not permitted on decon on state\n");
			return -EINVAL;
		}

		displayport_reg_phy_reset(1);
		displayport_reg_phy_init_setting();

		displayport_reg_phy_set_link_bw(link_rate);
		displayport_info("link_rate = %x\n", link_rate);

		displayport_reg_phy_mode_setting();

		displayport_reg_set_lane_count(lane_cnt);
		displayport_info("lane_cnt = %x\n", lane_cnt);

		if (enhanced_frame_cap)
			displayport_write_mask(SST1_MAIN_CONTROL + 0x1000 * sst_id,
					1, ENHANCED_MODE);

		/* wait for 60us */
		usleep_range(60, 61);

		displayport_reg_phy_reset(0);
	} else
		displayport_info("skip phy_reset in link training\n");

	val[0] = link_rate;
	val[1] = lane_cnt;

	if (enhanced_frame_cap)
		val[1] |= ENHANCED_FRAME_CAP;

	displayport_reg_dpcd_write_burst(DPCD_ADD_LINK_BW_SET, 2, val);

	displayport_reg_wait_phy_pll_lock();

	displayport_reg_set_training_pattern(TRAINING_PATTERN_1);

	val[0] = 0x21;	/* SCRAMBLING_DISABLE, TRAINING_PATTERN_1 */
	displayport_reg_dpcd_write(DPCD_ADD_TRANING_PATTERN_SET, 1, val);

#ifdef FEATURE_MANAGE_HMD_LIST
	if (displayport->is_hmd_dev &&
		!strncmp(displayport->mon_name, "PicoVR", MON_NAME_LEN)) {
		displayport_info("increase swing level\n");
		for (i = 0; i < 4; i++)
			drive_current[i] = 2;
	}
#endif
Voltage_Swing_Retry:
	displayport_dbg("Voltage_Swing_Retry\n");

	if (!displayport->hpd_current_state) {
		displayport_info("hpd is low in swing retry\n");
		return 0;
	}

	displayport_reg_set_voltage_and_pre_emphasis((u8 *)drive_current, (u8 *)pre_emphasis);
	displayport_get_voltage_and_pre_emphasis_max_reach((u8 *)drive_current,
			(u8 *)pre_emphasis, (u8 *)max_reach_value);

	val[0] = (pre_emphasis[0]<<3) | drive_current[0] | max_reach_value[0];
	val[1] = (pre_emphasis[1]<<3) | drive_current[1] | max_reach_value[1];
	val[2] = (pre_emphasis[2]<<3) | drive_current[2] | max_reach_value[2];
	val[3] = (pre_emphasis[3]<<3) | drive_current[3] | max_reach_value[3];
	displayport_info("Voltage_Swing_Retry %02x %02x %02x %02x\n", val[0], val[1], val[2], val[3]);
	displayport_reg_dpcd_write_burst(DPCD_ADD_TRANING_LANE0_SET, 4, val);

	if (training_aux_rd_interval != 0)
		usleep_range(training_aux_rd_interval * 4 * 1000,
				training_aux_rd_interval * 4 * 1000 + 1);
	else
		usleep_range(100, 101);

	lane_cr_done = 0;

	displayport_reg_dpcd_read(DPCD_ADD_LANE0_1_STATUS, 2, val);
	lane_cr_done |= ((val[0] & LANE0_CR_DONE) >> 0);
	lane_cr_done |= ((val[0] & LANE1_CR_DONE) >> 3);
	lane_cr_done |= ((val[1] & LANE2_CR_DONE) << 2);
	lane_cr_done |= ((val[1] & LANE3_CR_DONE) >> 1);

	displayport_dbg("lane_cr_done = %x\n", lane_cr_done);

	if (lane_cnt == 0x04) {
		if (lane_cr_done == 0x0F) {
			displayport_dbg("lane_cr_done\n");
			goto EQ_Training_Start;
		} else if (drive_current[0] == MAX_REACHED_CNT
			&& drive_current[1] == MAX_REACHED_CNT
			&& drive_current[2] == MAX_REACHED_CNT
			&& drive_current[3] == MAX_REACHED_CNT) {
			displayport_dbg("MAX_REACHED_CNT\n");
			goto Check_Link_rate;
		}
	} else if (lane_cnt == 0x02) {
		if (lane_cr_done == 0x03) {
			displayport_dbg("lane_cr_done\n");
			goto EQ_Training_Start;
		} else if (drive_current[0] == MAX_REACHED_CNT
			&& drive_current[1] == MAX_REACHED_CNT) {
			displayport_dbg("MAX_REACHED_CNT\n");
			goto Check_Link_rate;
		}
	} else if (lane_cnt == 0x01) {
		if (lane_cr_done == 0x01) {
			displayport_dbg("lane_cr_done\n");
			goto EQ_Training_Start;
		} else if (drive_current[0] == MAX_REACHED_CNT) {
			displayport_dbg("MAX_REACHED_CNT\n");
			goto Check_Link_rate;
		}
	} else {
		val[0] = 0x00;	/* SCRAMBLING_ENABLE, NORMAL_DATA */
		displayport_reg_dpcd_write(DPCD_ADD_TRANING_PATTERN_SET, 1, val);
		displayport_err("Full Link Training Fail : Link Rate %02x, lane Count %02x\n",
				link_rate, lane_cnt);
		return -EINVAL;
	}

	displayport_reg_dpcd_read_burst(DPCD_ADD_ADJUST_REQUEST_LANE0_1, 2, val);
	voltage_swing_lane[0] = (val[0] & VOLTAGE_SWING_LANE0);
	pre_emphasis_lane[0] = (val[0] & PRE_EMPHASIS_LANE0) >> 2;
	voltage_swing_lane[1] = (val[0] & VOLTAGE_SWING_LANE1) >> 4;
	pre_emphasis_lane[1] = (val[0] & PRE_EMPHASIS_LANE1) >> 6;

	voltage_swing_lane[2] = (val[1] & VOLTAGE_SWING_LANE2);
	pre_emphasis_lane[2] = (val[1] & PRE_EMPHASIS_LANE2) >> 2;
	voltage_swing_lane[3] = (val[1] & VOLTAGE_SWING_LANE3) >> 4;
	pre_emphasis_lane[3] = (val[1] & PRE_EMPHASIS_LANE3) >> 6;

	if (drive_current[0] == voltage_swing_lane[0] &&
			drive_current[1] == voltage_swing_lane[1] &&
			drive_current[2] == voltage_swing_lane[2] &&
			drive_current[3] == voltage_swing_lane[3]) {
		if (training_retry_no == 4)
			goto Check_Link_rate;
		else
			training_retry_no++;
	} else
		training_retry_no = 0;

	for (i = 0; i < 4; i++) {
		drive_current[i] = voltage_swing_lane[i];
		pre_emphasis[i] = pre_emphasis_lane[i];
		displayport_dbg("v drive_current[%d] = %x\n",
				i, drive_current[i]);
		displayport_dbg("v pre_emphasis[%d] = %x\n",
				i, pre_emphasis[i]);
	}

	total_retry_cnt++;

	if (total_retry_cnt >= MAX_RETRY_CNT) {
		displayport_err("total_retry_cnt = %d\n", total_retry_cnt);
		goto Check_Link_rate;
	}

	goto Voltage_Swing_Retry;

Check_Link_rate:
	displayport_info("Check_Link_rate\n");

	total_retry_cnt = 0;

	if (link_rate == LINK_RATE_8_1Gbps) {
		link_rate = LINK_RATE_5_4Gbps;
		goto Reduce_Link_Rate_Retry;
	} else if (link_rate == LINK_RATE_5_4Gbps) {
		link_rate = LINK_RATE_2_7Gbps;
		goto Reduce_Link_Rate_Retry;
	} else if (link_rate == LINK_RATE_2_7Gbps) {
		link_rate = LINK_RATE_1_62Gbps;
		goto Reduce_Link_Rate_Retry;
	} else if (link_rate == LINK_RATE_1_62Gbps) {
		val[0] = 0x00;	/* SCRAMBLING_ENABLE, NORMAL_DATA */
		displayport_reg_dpcd_write(DPCD_ADD_TRANING_PATTERN_SET, 1, val);
		displayport_err("Full Link Training Fail : Link_Rate Retry\n");
		return -EINVAL;
	}

EQ_Training_Start:
	displayport_info("EQ_Training_Start\n");

	eq_training_retry_no = 0;
	for (i = 0; i < DPCD_BUF_SIZE; i++)
		eq_val[i] = 0;

	if (tps4_supported) {
		displayport_info("TPS4 set\n");
		displayport_reg_set_training_pattern(TRAINING_PATTERN_4);

		val[0] = 0x07;	/* TRAINING_PATTERN_4 */
		displayport_reg_dpcd_write(DPCD_ADD_TRANING_PATTERN_SET, 1, val);
	} else if (tps3_supported) {
		displayport_reg_set_training_pattern(TRAINING_PATTERN_3);

		val[0] = 0x23;	/* SCRAMBLING_DISABLE, TRAINING_PATTERN_3 */
		displayport_reg_dpcd_write(DPCD_ADD_TRANING_PATTERN_SET, 1, val);
	} else {
		displayport_reg_set_training_pattern(TRAINING_PATTERN_2);

		val[0] = 0x22;	/* SCRAMBLING_DISABLE, TRAINING_PATTERN_2 */
		displayport_reg_dpcd_write(DPCD_ADD_TRANING_PATTERN_SET, 1, val);
	}

EQ_Training_Retry:
	displayport_dbg("EQ_Training_Retry\n");

	if (!displayport->hpd_current_state) {
		displayport_info("hpd is low in eq training retry\n");
		return 0;
	}

	displayport_reg_set_voltage_and_pre_emphasis((u8 *)drive_current, (u8 *)pre_emphasis);
	displayport_get_voltage_and_pre_emphasis_max_reach((u8 *)drive_current,
			(u8 *)pre_emphasis, (u8 *)max_reach_value);

	val[0] = (pre_emphasis[0]<<3) | drive_current[0] | max_reach_value[0];
	val[1] = (pre_emphasis[1]<<3) | drive_current[1] | max_reach_value[1];
	val[2] = (pre_emphasis[2]<<3) | drive_current[2] | max_reach_value[2];
	val[3] = (pre_emphasis[3]<<3) | drive_current[3] | max_reach_value[3];
	displayport_info("EQ_Training_Retry %02x %02x %02x %02x\n", val[0], val[1], val[2], val[3]);
	displayport_reg_dpcd_write_burst(DPCD_ADD_TRANING_LANE0_SET, 4, val);

	for (i = 0; i < 4; i++)
		eq_val[i] = val[i];

	lane_cr_done = 0;
	lane_channel_eq_done = 0;
	lane_symbol_locked_done = 0;
	interlane_align_done = 0;

	if (training_aux_rd_interval != 0)
		usleep_range(training_aux_rd_interval * 1000 * 4,
					training_aux_rd_interval * 1000 * 4 + 1);
	else
		usleep_range(100, 101);

	displayport_reg_dpcd_read_burst(DPCD_ADD_LANE0_1_STATUS, 3, val);
	lane_cr_done |= ((val[0] & LANE0_CR_DONE) >> 0);
	lane_cr_done |= ((val[0] & LANE1_CR_DONE) >> 3);
	lane_channel_eq_done |= ((val[0] & LANE0_CHANNEL_EQ_DONE) >> 1);
	lane_channel_eq_done |= ((val[0] & LANE1_CHANNEL_EQ_DONE) >> 4);
	lane_symbol_locked_done |= ((val[0] & LANE0_SYMBOL_LOCKED) >> 2);
	lane_symbol_locked_done |= ((val[0] & LANE1_SYMBOL_LOCKED) >> 5);

	lane_cr_done |= ((val[1] & LANE2_CR_DONE) << 2);
	lane_cr_done |= ((val[1] & LANE3_CR_DONE) >> 1);
	lane_channel_eq_done |= ((val[1] & LANE2_CHANNEL_EQ_DONE) << 1);
	lane_channel_eq_done |= ((val[1] & LANE3_CHANNEL_EQ_DONE) >> 2);
	lane_symbol_locked_done |= ((val[1] & LANE2_SYMBOL_LOCKED) >> 0);
	lane_symbol_locked_done |= ((val[1] & LANE3_SYMBOL_LOCKED) >> 3);

	interlane_align_done |= (val[2] & INTERLANE_ALIGN_DONE);

	if (lane_cnt == 0x04) {
		if (lane_cr_done != 0x0F)
			goto Check_Link_rate;
	} else if (lane_cnt == 0x02) {
		if (lane_cr_done != 0x03)
			goto Check_Link_rate;
	} else {
		if (lane_cr_done != 0x01)
			goto Check_Link_rate;
	}

	displayport_info("lane_cr_done = %x\n", lane_cr_done);
	displayport_info("lane_channel_eq_done = %x\n", lane_channel_eq_done);
	displayport_info("lane_symbol_locked_done = %x\n", lane_symbol_locked_done);
	displayport_info("interlane_align_done = %x\n", interlane_align_done);

	if (lane_cnt == 0x04) {
		if ((lane_channel_eq_done == 0x0F) && (lane_symbol_locked_done == 0x0F)
				&& (interlane_align_done == 1)) {
			displayport_reg_set_training_pattern(NORAMAL_DATA);

			val[0] = 0x00;	/* SCRAMBLING_ENABLE, NORMAL_DATA */
			displayport_reg_dpcd_write(DPCD_ADD_TRANING_PATTERN_SET, 1, val);

			displayport_info("Full Link Training Finish - : %02x %02x\n", link_rate, lane_cnt);
			displayport_info("LANE_SET [%d] : %02x %02x %02x %02x\n",
					eq_training_retry_no, eq_val[0], eq_val[1], eq_val[2], eq_val[3]);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
			secdp_bigdata_clr_error_cnt(ERR_LINK_TRAIN);
			secdp_bigdata_save_item(BD_CUR_LANE_COUNT, lane_cnt);
			secdp_bigdata_save_item(BD_CUR_LINK_RATE, link_rate);
#endif
			return ret;
		}
	} else if (lane_cnt == 0x02) {
		if ((lane_channel_eq_done == 0x03) && (lane_symbol_locked_done == 0x03)
				&& (interlane_align_done == 1)) {
			displayport_reg_set_training_pattern(NORAMAL_DATA);

			val[0] = 0x00;	/* SCRAMBLING_ENABLE, NORMAL_DATA */
			displayport_reg_dpcd_write(DPCD_ADD_TRANING_PATTERN_SET, 1, val);

			displayport_info("Full Link Training Finish - : %02x %02x\n", link_rate, lane_cnt);
			displayport_info("LANE_SET [%d] : %02x %02x %02x %02x\n",
					eq_training_retry_no, eq_val[0], eq_val[1], eq_val[2], eq_val[3]);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
			secdp_bigdata_clr_error_cnt(ERR_LINK_TRAIN);
			secdp_bigdata_save_item(BD_CUR_LANE_COUNT, lane_cnt);
			secdp_bigdata_save_item(BD_CUR_LINK_RATE, link_rate);
#endif
			return ret;
		}
	} else {
		if ((lane_channel_eq_done == 0x01) && (lane_symbol_locked_done == 0x01)
				&& (interlane_align_done == 1)) {
			displayport_reg_set_training_pattern(NORAMAL_DATA);

			val[0] = 0x00;	/* SCRAMBLING_ENABLE, NORMAL_DATA */
			displayport_reg_dpcd_write(DPCD_ADD_TRANING_PATTERN_SET, 1, val);

			displayport_info("Full Link Training Finish - : %02x %02x\n", link_rate, lane_cnt);
			displayport_info("LANE_SET [%d] : %02x %02x %02x %02x\n",
					eq_training_retry_no, eq_val[0], eq_val[1], eq_val[2], eq_val[3]);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
			secdp_bigdata_clr_error_cnt(ERR_LINK_TRAIN);
			secdp_bigdata_save_item(BD_CUR_LANE_COUNT, lane_cnt);
			secdp_bigdata_save_item(BD_CUR_LINK_RATE, link_rate);
#endif
			return ret;
		}
	}

	if (training_retry_no == 4)
		goto Check_Link_rate;

	if (eq_training_retry_no >= 5) {
		val[0] = 0x00;	/* SCRAMBLING_ENABLE, NORMAL_DATA */
		displayport_reg_dpcd_write(DPCD_ADD_TRANING_PATTERN_SET, 1, val);
		displayport_err("Full Link Training Fail : EQ_training Retry\n");
		return -EINVAL;
	}

	displayport_reg_dpcd_read_burst(DPCD_ADD_ADJUST_REQUEST_LANE0_1, 2, val);
	voltage_swing_lane[0] = (val[0] & VOLTAGE_SWING_LANE0);
	pre_emphasis_lane[0] = (val[0] & PRE_EMPHASIS_LANE0) >> 2;
	voltage_swing_lane[1] = (val[0] & VOLTAGE_SWING_LANE1) >> 4;
	pre_emphasis_lane[1] = (val[0] & PRE_EMPHASIS_LANE1) >> 6;

	voltage_swing_lane[2] = (val[1] & VOLTAGE_SWING_LANE2);
	pre_emphasis_lane[2] = (val[1] & PRE_EMPHASIS_LANE2) >> 2;
	voltage_swing_lane[3] = (val[1] & VOLTAGE_SWING_LANE3) >> 4;
	pre_emphasis_lane[3] = (val[1] & PRE_EMPHASIS_LANE3) >> 6;

	for (i = 0; i < 4; i++) {
		drive_current[i] = voltage_swing_lane[i];
		pre_emphasis[i] = pre_emphasis_lane[i];

		displayport_dbg("eq drive_current[%d] = %x\n", i, drive_current[i]);
		displayport_dbg("eq pre_emphasis[%d] = %x\n", i, pre_emphasis[i]);
	}

	eq_training_retry_no++;
	goto EQ_Training_Retry;
}

static int displayport_check_dfp_type(void)
{
	u8 val = 0;
	int port_type = 0;
	char *dfp[] = {"Displayport", "VGA", "HDMI", "Others"};

	displayport_reg_dpcd_read(DPCD_ADD_DOWN_STREAM_PORT_PRESENT, 1, &val);
	port_type = (val & BIT_DFP_TYPE) >> 1;
	displayport_info("DFP type: %s(0x%X)\n", dfp[port_type], val);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	secdp_bigdata_save_item(BD_ADAPTER_TYPE, dfp[port_type]);
#endif

	return port_type;
}

static int displayport_link_sink_status_read(void)
{
	u8 val[DPCD_BUF_SIZE] = {0, };
	int ret = 0;

	ret = displayport_reg_dpcd_read_burst(DPCD_ADD_REVISION_NUMBER, DPCD_BUF_SIZE, val);

	if (!ret) {
		displayport_info("Read DPCD REV NUM 0_5 %02x %02x %02x %02x %02x %02x\n",
			val[0], val[1], val[2], val[3], val[4], val[5]);
		displayport_info("Read DPCD REV NUM 6_B %02x %02x %02x %02x %02x %02x\n",
			val[6], val[7], val[8], val[9], val[10], val[11]);
	} else
		displayport_info("%s error\n", __func__);

	return ret;
}

static int displayport_read_branch_revision(struct displayport_device *displayport)
{
	int ret = 0;
	u8 val[4] = {0, };

	ret = displayport_reg_dpcd_read_burst(DPCD_BRANCH_HW_REVISION, 3, val);
	if (!ret) {
		displayport_info("Branch revision: HW(0x%X), SW(0x%X, 0x%X)\n",
			val[0], val[1], val[2]);
		displayport->dex_ver[0] = val[1];
		displayport->dex_ver[1] = val[2];
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
		secdp_bigdata_save_item(BD_ADAPTER_HWID, val[0]);
		secdp_bigdata_save_item(BD_ADAPTER_FWVER, (val[1] << 8) | val[2]);
#endif
	}

	return ret;
}

static int displayport_link_status_read(u32 sst_id)
{
	u8 val[DPCP_LINK_SINK_STATUS_FIELD_LENGTH] = {0, };
	int count = 200;
	int ret = 0;
	int i;

	/* for Link CTS : Branch Device Detection*/
	ret = displayport_link_sink_status_read();
	for (i = 0; ret != 0 && i < 4; i++) {
		msleep(50);
		ret = displayport_link_sink_status_read();
	}
	if (ret != 0) {
		displayport_err("link_sink_status_read fail\n");
		return -EINVAL;
	}

	do {
		displayport_reg_dpcd_read(DPCD_ADD_SINK_COUNT, 1, val);
		if ((val[0] & (SINK_COUNT1 | SINK_COUNT2)) != 0)
			break;
		msleep(20);
	} while (--count > 0);

	if (count < 200 - 1)
		displayport_info("%s retry: %d\n", __func__, 200 - count);

	if (count == 0)
		return -EINVAL;

	if (count < 10 && count > 0)
		usleep_range(10000, 11000); /* need delay after SINK count is changed to 1 */

	displayport_reg_dpcd_read_burst(DPCD_ADD_DEVICE_SERVICE_IRQ_VECTOR,
			DPCP_LINK_SINK_STATUS_FIELD_LENGTH - 1, &val[1]);

	displayport_info("SST(%d) Read link status %02x %02x %02x %02x %02x %02x\n",
				sst_id, val[0], val[1], val[2], val[3], val[4], val[5]);

	if ((val[0] & val[1] & val[2] & val[3] & val[4] & val[5]) == 0xff)
		return -EINVAL;

	if (val[1] == AUTOMATED_TEST_REQUEST) {
		u8 data = 0;

		displayport_reg_dpcd_read(DPCD_TEST_REQUEST, 1, &data);

		if ((data & TEST_EDID_READ) == TEST_EDID_READ) {
			val[0] = edid_read_checksum();
			displayport_info("TEST_EDID_CHECKSUM %02x\n", val[0]);

			displayport_reg_dpcd_write(DPCD_TEST_EDID_CHECKSUM, 1, val);

			val[0] = 0x04; /*TEST_EDID_CHECKSUM_WRITE*/
			displayport_reg_dpcd_write(DPCD_TEST_RESPONSE, 1, val);

		}
	}

	return 0;
}

bool displayport_check_dex_ratio(enum video_ratio_t ratio)
{
	switch (ratio) {
	case RATIO_16_9:
	case RATIO_16_10:
	case RATIO_21_9:
		return true;
	default:
		break;
	}

	return false;
}

static void displayport_find_proper_ratio_video_for_dex(struct displayport_device *displayport)
{
	u32 sst_id = displayport_get_sst_id_with_decon_id(DEFAULT_DECON_ID);
	int i = displayport->sst[sst_id]->best_video;
	enum video_ratio_t best_ratio = supported_videos[i].ratio;

	displayport->dex_video_pick = 0;

	if (!displayport->dex_setting)
		return;

	/* for test */
	if (reduced_resolution)
		i = reduced_resolution;

	/* find same or proper ratio timing from best timing */
	if (displayport_check_dex_ratio(best_ratio)) {
		for (; i >= V1600X900P59; i--) {
			if (supported_videos[i].edid_support_match &&
					supported_videos[i].dex_support &&
					supported_videos[i].ratio == best_ratio) {
				displayport->dex_video_pick = i;
				displayport_info("found dex support ratio: %s %d/%d\n",
						supported_videos[i].name, i, best_ratio);
				break;
			}
		}
	} else {
#ifdef FEATURE_IGNORE_PREFER_IF_DEX_RES_EXIST
		for (; i >= V1600X900P59; i--) {
			if (supported_videos[i].edid_support_match &&
					supported_videos[i].dex_support &&
					displayport_check_dex_ratio(supported_videos[i].ratio)) {
				displayport->dex_video_pick = i;
				displayport_info("found dex support ratio: %s %d/%d\n",
						supported_videos[i].name, i, best_ratio);
				break;
			}
		}
#endif
	}

	if (!displayport->dex_video_pick)
		displayport_info("not found dex support ratio\n");
}

#ifdef FEATURE_MANAGE_HMD_LIST
static bool displayport_check_hmd_dev(struct displayport_device *displayport)
{
	bool ret = false;
	int i;
	struct secdp_sink_dev *hmd = displayport->hmd_list;

	mutex_lock(&displayport->hmd_lock);
	for (i = 0; i < MAX_NUM_HMD; i++) {
		if (hmd[i].ven_id == 0 && hmd[i].prod_id == 0)
			continue;
		if (!strncmp(hmd[i].monitor_name, displayport->mon_name, MON_NAME_LEN) &&
				hmd[i].ven_id == (u32)displayport->ven_id &&
				hmd[i].prod_id == (u32)displayport->prod_id) {
			displayport_info("HMD %s\n", hmd[i].monitor_name);
			ret = true;
			break;
		}
	}
	mutex_unlock(&displayport->hmd_lock);

	return ret;
}
#endif

static int displayport_link_training(u32 sst_id)
{
	u8 val;
	struct displayport_device *displayport = get_displayport_drvdata();
	int ret = 0;

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	if (displayport->pdic_cable_state == PDIC_NOTIFY_DETACH) {
		displayport_err("pdic cable is detached\n");
		return -ENODEV;
	}
#endif

	if (!displayport->hpd_current_state) {
		displayport_info("hpd is low in link training\n");
		return -ENODEV;
	}

	mutex_lock(&displayport->training_lock);

	if (displayport->mst_cap == 0) {
		ret = edid_update(sst_id, displayport);
		if (ret < 0) {
			displayport_err("failed to update edid\n");
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
			secdp_bigdata_inc_error_cnt(ERR_EDID);
#endif
		}

#ifdef FEATURE_MANAGE_HMD_LIST
		displayport->is_hmd_dev = displayport_check_hmd_dev(displayport);
#endif
		/* find proper ratio resolution for DEX */
		displayport_find_proper_ratio_video_for_dex(displayport);
	}

	displayport_reg_dpcd_read(DPCD_ADD_MAX_DOWNSPREAD, 1, &val);
	displayport_dbg("DPCD_ADD_MAX_DOWNSPREAD = %x\n", val);

	ret = displayport_full_link_training(sst_id);

	mutex_unlock(&displayport->training_lock);

	return ret;
}

static void displayport_set_extcon_state(u32 sst_id,
		struct displayport_device *displayport, int state)
{
#if defined(CONFIG_EXTCON)
	u32 extcon_dp_id = EXTCON_DISP_DP;
	int ret = 0;

	switch (sst_id) {
	case SST1:
		extcon_dp_id = EXTCON_DISP_DP;
		break;
	case SST2:
		extcon_dp_id = EXTCON_DISP_DP2;
		break;
	default:
		extcon_dp_id = EXTCON_DISP_DP;
	}

	if (state)
		ret = extcon_set_state_sync(displayport->extcon_displayport, extcon_dp_id, 1);
	else
		ret = extcon_set_state_sync(displayport->extcon_displayport, extcon_dp_id, 0);
#else
	displayport_info("Not compiled EXTCON driver\n");
#endif

	if (state) {
		switch_state |= 1 << sst_id;
		displayport_set_switch_hpd_state(1);
	} else {
		switch_state &= ~(1 << sst_id);
		if (switch_state == 0)
			displayport_set_switch_hpd_state(0);
	}
	displayport_info("SST%d HPD status = %d,ret(%d), switch = %d\n",
					sst_id + 1, state, ret, switch_state);
}

static int displayport_get_extcon_state(u32 sst_id,
		struct displayport_device *displayport)
{
#if defined(CONFIG_EXTCON)
	u32 extcon_dp_id = EXTCON_DISP_DP;
	int ret = 0;

	switch (sst_id) {
	case SST1:
		extcon_dp_id = EXTCON_DISP_DP;
		break;
	case SST2:
		extcon_dp_id = EXTCON_DISP_DP2;
		break;
	default:
		extcon_dp_id = EXTCON_DISP_DP;
	}

	ret = extcon_get_state(displayport->extcon_displayport, extcon_dp_id);
	displayport_info("SST%d get extcon state = %#p, ret(%d)\n",
			sst_id + 1, displayport->extcon_displayport, ret);

	return ret;
#else
	displayport_info("Not compiled EXTCON driver\n");

	return 0;
#endif
}

int displayport_check_mst(void)
{
	u8 val = 0;
	int mst_cap = 0;
	int ret;
	int count = 100;
	struct displayport_device *displayport = get_displayport_drvdata();

	ret = displayport_reg_dpcd_read(DPCD_ADD_REVISION_NUMBER, 1, &val);
	while (ret != 0 && count > 0 && displayport->pdic_hpd) {
		ret = displayport_reg_dpcd_read(DPCD_ADD_REVISION_NUMBER, 1, &val);
		usleep_range(10000, 11000);
		count--;
	}

	if (count < 100)
		displayport_info("%s retry: %d\n", __func__, 100 - count);

	displayport_info("DPCD_ADD_REVISION_NUMBER = %x\n", val);

	if (val >= DPCD_VER_1_2) {
		displayport_reg_dpcd_read(DPCD_ADD_MSTM_CAP, 1, &val);
		displayport_info("DPCD_ADD_MSTM_CAP = %x\n", val);

		if (!displayport->mst_mode && (val & MST_CAP)) {
			displayport_info("MST support device, but\n");
			return 0;
		}

		if (val & MST_CAP)
			mst_cap = 1;
	}

	return mst_cap;
}

void displayport_mst_enable(void)
{
	u8 val = 0;

	val = DPCD_MST_EN | UP_REQ_EN | UPSTREAM_IS_SRC;
	displayport_reg_dpcd_write(DPCD_ADD_MSTM_CTRL, 1, &val);

	displayport_reg_set_mst_en(1);
}

int displayport_wait_state_change(u32 sst_id,
		struct displayport_device *displayport,	int max_wait_time,
		enum displayport_state state)
{
	int ret = 0;
	int wait_cnt = max_wait_time;

	displayport_info("SST%d wait_state_change start\n", sst_id + 1);
	displayport_info("max_wait_time = %dms, state = %d\n", max_wait_time, state);

	do {
		wait_cnt--;
		usleep_range(1000, 1030);
	} while ((state != displayport->sst[sst_id]->state) && (wait_cnt > 0));

	displayport_info("wait_state_change time = %dms, state = %d\n",
			max_wait_time - wait_cnt, state);

	if (wait_cnt <= 0)
		displayport_err("SST%d wait state timeout\n", sst_id + 1);

	ret = wait_cnt;

	displayport_info("SST%d wait_state_change end\n", sst_id + 1);

	return ret;
}

int displayport_wait_audio_off_change(u32 sst_id,
		struct displayport_device *displayport,	int max_wait_time)
{
	int ret = 0;
	int wait_cnt = max_wait_time;
	int taken_ms;

	displayport_info("SST%d wait_audio_off_change for %dms\n", sst_id + 1, max_wait_time);

	do {
		wait_cnt--;
		usleep_range(1000, 1030);
	} while ((displayport->sst[sst_id]->audio_state != 0) && (wait_cnt > 0));

	taken_ms = max_wait_time - wait_cnt;
	displayport_dbg("wait_audio_off time = %dms\n", taken_ms);

	if (wait_cnt <= 0)
		displayport_err("SST%d wait audio off timeout\n", sst_id + 1);

	ret = wait_cnt;

	displayport_dbg("SST%d wait_audio_off_change end\n", sst_id + 1);

	return ret;
}

int displayport_wait_decon_run(u32 sst_id,
		struct displayport_device *displayport,	int max_wait_time)
{
	int ret = 0;
	int wait_cnt = max_wait_time;

	displayport_info("SST%d wait_decon_run start\n", sst_id + 1);
	displayport_info("max_wait_time = %dms\n", max_wait_time);

	do {
		wait_cnt--;
		usleep_range(1000, 1030);
	} while ((displayport->sst[sst_id]->decon_run == 0) && (wait_cnt > 0));

	displayport_info("wait_decon_run time = %dms\n", max_wait_time - wait_cnt);

	if (wait_cnt <= 0)
		displayport_err("SST%d wait_decon_run timeout\n", sst_id + 1);

	ret = wait_cnt;

	displayport_info("SST%d wait_decon_run end\n", sst_id + 1);

	return ret;
}

void displayport_on_by_hpd_high(u32 sst_id, struct displayport_device *displayport)
{
	int timeout = 0;

	if (displayport->sst[sst_id]->hpd_state) {
		displayport->sst[sst_id]->bpc = BPC_8;	/* default setting */
		displayport->sst[sst_id]->dyn_range = VESA_RANGE;

		displayport_audio_init_config(sst_id);

		if (displayport->sst[sst_id]->bist_used) {
			displayport->cur_sst_id = sst_id;
			displayport->sst[sst_id]->state = DISPLAYPORT_STATE_INIT;
			displayport_enable(displayport); /* for bist video enable */
		} else {
			if (displayport_get_extcon_state(sst_id, displayport) == 0) {
				if (displayport->sst[sst_id]->bist_used == 0) {
					displayport->sst[sst_id]->state = DISPLAYPORT_STATE_INIT;
					displayport_set_extcon_state(sst_id, displayport, 1);
					timeout = displayport_wait_state_change(sst_id,
									displayport, 3000, DISPLAYPORT_STATE_ON);
				}
			} else {
				displayport_info("displayport_get_extcon_state != 0\n");
			}
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_DISPLAYPORT)
			displayport_wait_decon_run(sst_id, displayport, 3000);
			blocking_notifier_call_chain(&dp_ado_notifier_head,
					(unsigned long)edid_audio_informs(), NULL);
#endif
		}
	}

}

void displayport_off_by_hpd_low(u32 sst_id, struct displayport_device *displayport)
{
	int timeout = 0;
	struct decon_device *decon;

	if (displayport->sst[sst_id]->state == DISPLAYPORT_STATE_ON
		|| displayport->sst[sst_id]->state == DISPLAYPORT_STATE_INIT) {
		displayport_info("%s state on or init\n", __func__);
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_DISPLAYPORT)
		blocking_notifier_call_chain(&dp_ado_notifier_head,
				(unsigned long)-1, NULL);
		displayport_info("audio info = -1\n");
		displayport_wait_audio_off_change(sst_id, displayport, 5000);
#endif
		displayport->sst[sst_id]->hpd_state = HPD_UNPLUG;
		displayport->sst[sst_id]->cur_video = V640X480P60;
		displayport->sst[sst_id]->best_video = V640X480P60;
		displayport->sst[sst_id]->bpc = BPC_8;
		displayport->sst[sst_id]->dyn_range = VESA_RANGE;

		if (displayport->sst[sst_id]->bist_used == 0) {
			displayport_info("%s(%d)\n", __func__, __LINE__);
			displayport_set_extcon_state(sst_id, displayport, 0);

			timeout = displayport_wait_state_change(sst_id,
							displayport, 3000, DISPLAYPORT_STATE_OFF);

			if (timeout <= 0) {
				displayport_err("disable timeout\n");

				decon = get_decon_drvdata(displayport_get_decon_id(sst_id));

				if (displayport->sst[sst_id]->state == DISPLAYPORT_STATE_INIT) {
					displayport_info("SST%d not enabled\n", sst_id + 1);
				} else if (decon->state == DECON_STATE_OFF &&
						displayport->sst[sst_id]->state == DISPLAYPORT_STATE_ON) {
					displayport_err("abnormal state: decon%d:%d, displayport SST%d:%d\n",
							decon->id, decon->state, sst_id + 1,
							displayport->sst[sst_id]->state);
					displayport_disable(displayport);
				}
			}
		} else {
			displayport_info("%s(%d)\n", __func__, __LINE__);
			displayport->cur_sst_id = sst_id;
			displayport_disable(displayport); /* for bist video disable */
		}
	} else {
		displayport_info("%s state off\n", __func__);
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_DISPLAYPORT)
		blocking_notifier_call_chain(&dp_ado_notifier_head,
				(unsigned long)-1, NULL);
		displayport_info("audio info = -1\n");
		displayport_wait_audio_off_change(sst_id, displayport, 5000);
#endif
		/* set the state of extcon to 0 even though in abnormal case */
		displayport_set_extcon_state(sst_id, displayport, 0);
	}
}

void displayport_hpd_changed(int state)
{
	int ret = 0;
	struct displayport_device *displayport = get_displayport_drvdata();
	int i = 0;
	u32 sst_id = displayport_get_sst_id_with_decon_id(DEFAULT_DECON_ID);

	mutex_lock(&displayport->hpd_lock);
	if (displayport->hpd_current_state == state) {
		displayport_info("hpd same state skip %x\n", state);
		mutex_unlock(&displayport->hpd_lock);
		return;
	}

	displayport_info("displayport hpd changed %d\n", state);
	displayport->hpd_current_state = state;

	/* just for test */
	/*displayport->sst[0]->bist_used = displayport_sst1_bist_test;*/
	/*displayport->sst[1]->bist_used = displayport_sst2_bist_test;*/

	if (state) {
		pm_runtime_get_sync(displayport->dev);
		pm_stay_awake(displayport->dev);

		/* PHY power on */
		usleep_range(10000, 11000);
		displayport_reg_sw_reset();
		displayport_reg_init(); /* for AUX ch read/write. */
		displayport_hdcp22_notify_state(DP_CONNECT);

		displayport->auto_test_mode = 0;
		displayport->sst[sst_id]->bist_used = 0;
		displayport->sst[sst_id]->best_video = EDID_DEFAULT_TIMINGS_IDX;

		usleep_range(10000, 11000);

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
		if (displayport->pdic_cable_state == PDIC_NOTIFY_DETACH) {
			displayport_err("pdic cable is detached\n");
			goto HPD_FAIL;
		}
#endif

#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
		if (displayport->dex_state == DEX_OFF)
			secdp_bigdata_save_item(BD_DP_MODE, "MIRROR");
		else
			secdp_bigdata_save_item(BD_DP_MODE, "DEX");
#endif

		/* for Link CTS : (4.2.2.3) EDID Read */
		if (displayport_link_status_read(sst_id) == -EINVAL) {
			displayport_set_switch_poor_connect();
			displayport_err("link_status_read fail %d\n", ++displayport_hdp_read_fail);
			goto HPD_FAIL;
		}

		displayport->mst_cap = displayport_check_mst();

		if (displayport->mst_cap) {
			displayport_mst_enable();
			displayport_topology_clr_vc();
		} else
			displayport->sst[sst_id]->hpd_state = HPD_PLUG; /* default */

		if (displayport_read_branch_revision(displayport))
			displayport_err("branch_revision_read fail\n");

		displayport_info("link training in hpd_changed\n");
		ret = displayport_link_training(sst_id);
		if (ret == -ENODEV) {
			goto HPD_FAIL;
		} else if (ret < 0) {
			displayport_set_switch_poor_connect();
			displayport_info("link training fail %d\n", ++displayport_hdp_link_tr_fail);
			goto HPD_FAIL;
		}

		displayport->dfp_type = displayport_check_dfp_type();
		/* Enable it! if you want to prevent output according to type
		 * if (displayport->dfp_type != DFP_TYPE_DP &&
		 *		displayport->dfp_type != DFP_TYPE_HDMI) {
		 *	displayport_err("not supported DFT type\n");
		 *	goto HPD_FAIL;
		 * }
		 */

		if (displayport->mst_cap == 0) {
			displayport_info("displayport_on_by_hpd_high in hpd changed\n");
			displayport_on_by_hpd_high(sst_id, displayport);
		} else
			displayport_topology_make();
	} else {
		displayport_reg_print_audio_state(sst_id);
#if IS_ENABLED(CONFIG_EXYNOS_HDCP2)
		if (displayport->hdcp_ver == HDCP_VERSION_2_2) {
			hdcp_dplink_cancel_auth();
		}
#endif
		cancel_delayed_work_sync(&displayport->hpd_plug_work);
		cancel_delayed_work_sync(&displayport->hpd_unplug_work);
		cancel_delayed_work_sync(&displayport->hpd_irq_work);
		cancel_delayed_work_sync(&displayport->hdcp13_work);
		cancel_delayed_work_sync(&displayport->hdcp22_work);
		cancel_delayed_work_sync(&displayport->hdcp13_integrity_check_work);

		if (displayport->mst_cap == 0) {
			displayport_off_by_hpd_low(sst_id, displayport);
		} else {
			for (i = SST1; i < MAX_SST_CNT; i++)
				displayport_off_by_hpd_low(i, displayport);
		}

		displayport_reg_deinit();
		displayport_reg_phy_disable();
		pm_runtime_put_sync(displayport->dev);
#if defined(CONFIG_CPU_IDLE)
		/* unblock to enter SICD mode */
		exynos_update_ip_idle_status(displayport->idle_ip_index, 1);
#endif
		pm_relax(displayport->dev);

		displayport->hdcp_ver = 0;
	}

	mutex_unlock(&displayport->hpd_lock);

	return;

HPD_FAIL:
	displayport_err("HPD FAIL Check CCIC or USB!!\n");
	displayport_reg_deinit();
	displayport_reg_phy_disable();
	pm_relax(displayport->dev);
	displayport->dex_state = DEX_OFF;
	displayport->hpd_current_state = HPD_UNPLUG;

	for (i = SST1; i < MAX_SST_CNT; i++) {
		displayport->sst[i]->state = DISPLAYPORT_STATE_INIT;
		displayport->sst[i]->hpd_state = HPD_UNPLUG;
		displayport->sst[i]->cur_video = V640X480P60;
		displayport->sst[i]->bpc = BPC_8;
		displayport->sst[i]->dyn_range = VESA_RANGE;
	}

	pm_runtime_put_sync(displayport->dev);
	/* in error case, add delay to avoid very short interval reconnection */
	msleep(300);
	mutex_unlock(&displayport->hpd_lock);

	return;
}

void displayport_set_reconnection(void)
{
	int ret;
	u32 sst_id = displayport_get_sst_id_with_decon_id(DEFAULT_DECON_ID);

	/* PHY power on */
	displayport_reg_init(); /* for AUX ch read/write. */

	displayport_info("link training in reconnection\n");
	ret = displayport_link_training(sst_id);
	if (ret < 0) {
		displayport_dbg("link training fail\n");
		return;
	}
}

static void displayport_hpd_plug_work(struct work_struct *work)
{
	int i = 0;
	struct displayport_device *displayport = get_displayport_drvdata();
	int ret = 0;

	for (i = SST1; i < MAX_SST_CNT; i++) {
		if (displayport->sst[i]->hpd_state == HPD_PLUG_WORK) {
			displayport_info("hpd_plug_work\n");
			displayport->sst[i]->hpd_state = HPD_PLUG;
			ret = edid_update(i, displayport);
			if (ret < 0) {
				displayport_err("failed to update edid SST%d\n", i);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
				secdp_bigdata_inc_error_cnt(ERR_EDID);
#endif
			}
			if (displayport->mst_cap == 1) {
				displayport_topology_make();
				displayport_on_by_hpd_high(i, displayport);
			}
		}
	}
}

static void displayport_hpd_unplug_work(struct work_struct *work)
{
	int i = 0;
	struct displayport_device *displayport = get_displayport_drvdata();

	for (i = SST1; i < MAX_SST_CNT; i++) {
		if (displayport->sst[i]->hpd_state == HPD_UNPLUG_WORK) {
			displayport_info("hpd_unplug_work\n");
			displayport_topology_delete_vc(i);
			displayport->sst[i]->hpd_state = HPD_UNPLUG;
			if (displayport->dex_state != DEX_OFF) {
				displayport->dex_state = DEX_RECONNECTING;
				displayport_info("dex set to reconnecting\n");
			}
			displayport_off_by_hpd_low(i, displayport);
		}
	}
}

static int displayport_check_dpcd_lane_status(u8 lane0_1_status,
		u8 lane2_3_status, u8 lane_align_status)
{
	u8 val[2] = {0,};
	u32 link_rate = displayport_reg_phy_get_link_bw();
	u32 lane_cnt = displayport_reg_get_lane_count();

	displayport_reg_dpcd_read(DPCD_ADD_LINK_BW_SET, 2, val);

	displayport_info("check lane %02x %02x %02x %02x\n", link_rate, lane_cnt,
			val[0], (val[1] & MAX_LANE_COUNT));

	if ((link_rate != val[0]) || (lane_cnt != (val[1] & MAX_LANE_COUNT))) {
		displayport_err("%s() link rate, lane_cnt error\n", __func__);
		return -EINVAL;
	}

	if (!(lane_align_status & LINK_STATUS_UPDATE))
		return 0;

	displayport_err("%s() link_status_update bit is set\n", __func__);

	if ((lane_align_status & INTERLANE_ALIGN_DONE) != INTERLANE_ALIGN_DONE) {
		displayport_err("%s() interlane align error\n", __func__);
		return -EINVAL;
	}

	if (lane_cnt >= 1) {
		if ((lane0_1_status & (LANE0_CR_DONE | LANE0_CHANNEL_EQ_DONE | LANE0_SYMBOL_LOCKED))
				!= (LANE0_CR_DONE | LANE0_CHANNEL_EQ_DONE | LANE0_SYMBOL_LOCKED)) {
			displayport_err("%s() lane0 status error\n", __func__);
			return -EINVAL;
		}
	}

	if (lane_cnt >= 2) {
		if ((lane0_1_status & (LANE1_CR_DONE | LANE1_CHANNEL_EQ_DONE | LANE1_SYMBOL_LOCKED))
				!= (LANE1_CR_DONE | LANE1_CHANNEL_EQ_DONE | LANE1_SYMBOL_LOCKED)) {
			displayport_err("%s() lane1 status error\n", __func__);
			return -EINVAL;
		}
	}

	if (lane_cnt == 4) {
		if ((lane2_3_status & (LANE2_CR_DONE | LANE2_CHANNEL_EQ_DONE | LANE2_SYMBOL_LOCKED))
				!= (LANE2_CR_DONE | LANE2_CHANNEL_EQ_DONE | LANE2_SYMBOL_LOCKED)) {
			displayport_err("%s() lane2 stat error\n", __func__);
			return -EINVAL;
		}

		if ((lane2_3_status & (LANE3_CR_DONE | LANE3_CHANNEL_EQ_DONE | LANE3_SYMBOL_LOCKED))
				!= (LANE3_CR_DONE | LANE3_CHANNEL_EQ_DONE | LANE3_SYMBOL_LOCKED)) {
			displayport_err("%s() lane3 status error\n", __func__);
			return -EINVAL;
		}
	}

	return 0;
}

static int displayport_automated_test_set_lane_req(u8 *val)
{
	u8 drive_current[MAX_LANE_CNT];
	u8 pre_emphasis[MAX_LANE_CNT];
	u8 voltage_swing_lane[MAX_LANE_CNT];
	u8 pre_emphasis_lane[MAX_LANE_CNT];
	u8 max_reach_value[MAX_LANE_CNT];
	u8 val2[MAX_LANE_CNT];
	int i;

	voltage_swing_lane[0] = (val[0] & VOLTAGE_SWING_LANE0);
	pre_emphasis_lane[0] = (val[0] & PRE_EMPHASIS_LANE0) >> 2;
	voltage_swing_lane[1] = (val[0] & VOLTAGE_SWING_LANE1) >> 4;
	pre_emphasis_lane[1] = (val[0] & PRE_EMPHASIS_LANE1) >> 6;

	voltage_swing_lane[2] = (val[1] & VOLTAGE_SWING_LANE2);
	pre_emphasis_lane[2] = (val[1] & PRE_EMPHASIS_LANE2) >> 2;
	voltage_swing_lane[3] = (val[1] & VOLTAGE_SWING_LANE3) >> 4;
	pre_emphasis_lane[3] = (val[1] & PRE_EMPHASIS_LANE3) >> 6;

	for (i = 0; i < 4; i++) {
		drive_current[i] = voltage_swing_lane[i];
		pre_emphasis[i] = pre_emphasis_lane[i];

		displayport_info("AutoTest: swing[%d] = %x\n", i, drive_current[i]);
		displayport_info("AutoTest: pre_emphasis[%d] = %x\n", i, pre_emphasis[i]);
	}
	displayport_reg_set_voltage_and_pre_emphasis((u8 *)drive_current, (u8 *)pre_emphasis);
	displayport_get_voltage_and_pre_emphasis_max_reach((u8 *)drive_current,
			(u8 *)pre_emphasis, (u8 *)max_reach_value);

	val2[0] = (pre_emphasis[0]<<3) | drive_current[0] | max_reach_value[0];
	val2[1] = (pre_emphasis[1]<<3) | drive_current[1] | max_reach_value[1];
	val2[2] = (pre_emphasis[2]<<3) | drive_current[2] | max_reach_value[2];
	val2[3] = (pre_emphasis[3]<<3) | drive_current[3] | max_reach_value[3];
	displayport_info("AutoTest: set %02x %02x %02x %02x\n", val2[0], val2[1], val2[2], val2[3]);
	displayport_reg_dpcd_write_burst(DPCD_ADD_TRANING_LANE0_SET, 4, val2);

	return 0;
}

static void displayport_automated_test_pattern_set(u32 sst_id, struct displayport_device *displayport,
			u8 dpcd_test_pattern_type)
{
	switch (dpcd_test_pattern_type) {
	case DPCD_TEST_PATTERN_COLOR_RAMPS:
		displayport->sst[sst_id]->bist_type = CTS_COLOR_RAMP;
		break;
	case DPCD_TEST_PATTERN_BW_VERTICAL_LINES:
		displayport->sst[sst_id]->bist_type = CTS_BLACK_WHITE;
		break;
	case DPCD_TEST_PATTERN_COLOR_SQUARE:
		if (displayport->sst[sst_id]->dyn_range == CEA_RANGE)
			displayport->sst[sst_id]->bist_type = CTS_COLOR_SQUARE_CEA;
		else
			displayport->sst[sst_id]->bist_type = CTS_COLOR_SQUARE_VESA;
		break;
	default:
		displayport->sst[sst_id]->bist_type = COLOR_BAR;
		break;
	}
}

static int displayport_Automated_Test_Request(void)
{
	u8 data = 0;
	u8 val[DPCP_LINK_SINK_STATUS_FIELD_LENGTH] = {0, };
	struct displayport_device *displayport = get_displayport_drvdata();
	u32 sst_id = displayport_get_sst_id_with_decon_id(DEFAULT_DECON_ID);
	u8 dpcd_test_pattern_type = 0;

	displayport_reg_dpcd_read(DPCD_TEST_REQUEST, 1, &data);
	displayport_info("TEST_REQUEST %02x\n", data);

	displayport->auto_test_mode = 1;
	val[0] = 0x01; /*TEST_ACK*/
	displayport_reg_dpcd_write(DPCD_TEST_RESPONSE, 1, val);

	if ((data & TEST_LINK_TRAINING) == TEST_LINK_TRAINING) {
		videoformat cur_video = displayport->sst[sst_id]->cur_video;
		u8 bpc = (u8)displayport->sst[sst_id]->bpc;
		u8 bist_type = (u8)displayport->sst[sst_id]->bist_type;
		u8 dyn_range = (u8)displayport->sst[sst_id]->dyn_range;

		/* PHY power on */
		displayport_reg_init();

		displayport_reg_dpcd_read(DPCD_TEST_LINK_RATE, 1, val);
		displayport_info("TEST_LINK_RATE %02x\n", val[0]);
		g_displayport_debug_param.link_rate = (val[0]&TEST_LINK_RATE);

		displayport_reg_dpcd_read(DPCD_TEST_LANE_COUNT, 1, val);
		displayport_info("TEST_LANE_COUNT %02x\n", val[0]);
		g_displayport_debug_param.lane_cnt = (val[0]&TEST_LANE_COUNT);

		g_displayport_debug_param.param_used = 1;
		displayport_link_training(sst_id);
		g_displayport_debug_param.param_used = 0;

		displayport->sst[sst_id]->bist_used = 1;
		displayport->sst[sst_id]->bist_type = COLOR_BAR;
		displayport_reg_set_bist_video_configuration(sst_id,
				cur_video, bpc, bist_type, dyn_range);
		displayport_reg_start(sst_id);
	} else if ((data & TEST_VIDEO_PATTERN) == TEST_VIDEO_PATTERN) {

		if (displayport->sst[sst_id]->state == DISPLAYPORT_STATE_OFF) {
			pm_runtime_get_sync(displayport->dev);

		/* PHY power on */
		displayport_reg_init(); /* for AUX ch read/write. */

		g_displayport_debug_param.param_used = 1;
		displayport_link_training(sst_id);
		g_displayport_debug_param.param_used = 0;
		}
		displayport_reg_dpcd_read(DPCD_TEST_PATTERN, 1, val);
		displayport_info("TEST_PATTERN %02x\n", val[0]);
		dpcd_test_pattern_type = val[0];

		displayport_automated_test_pattern_set(sst_id, displayport, dpcd_test_pattern_type);

		displayport->sst[sst_id]->cur_video = displayport->sst[sst_id]->best_video;

		displayport_reg_set_bist_video_configuration(sst_id, displayport->sst[sst_id]->cur_video,
				displayport->sst[sst_id]->bpc, CTS_COLOR_RAMP, displayport->sst[sst_id]->dyn_range);
		displayport_reg_start(sst_id);

	} else if ((data & TEST_PHY_TEST_PATTERN) == TEST_PHY_TEST_PATTERN) {
		displayport_reg_stop(sst_id);
		msleep(120);
		displayport_reg_dpcd_read(DPCD_ADD_ADJUST_REQUEST_LANE0_1, 2, val);
		displayport_info("ADJUST_REQUEST_LANE0_1 %02x %02x\n", val[0], val[1]);

		/*set swing, preemp*/
		displayport_automated_test_set_lane_req(val);

		displayport_reg_dpcd_read(DCDP_ADD_PHY_TEST_PATTERN, 4, val);
		displayport_info("PHY_TEST_PATTERN %02x %02x %02x %02x\n", val[0], val[1], val[2], val[3]);

		switch (val[0]) {
		case DISABLE_PATTEN:
			displayport_reg_set_qual_pattern(DISABLE_PATTEN, ENABLE_SCRAM);
			break;
		case D10_2_PATTERN:
			displayport_reg_set_qual_pattern(D10_2_PATTERN, DISABLE_SCRAM);
			break;
		case SERP_PATTERN:
			displayport_reg_set_qual_pattern(SERP_PATTERN, ENABLE_SCRAM);
			break;
		case PRBS7:
			displayport_reg_set_qual_pattern(PRBS7, DISABLE_SCRAM);
			break;
		case CUSTOM_80BIT:
			displayport_reg_set_pattern_PLTPAT();
			displayport_reg_set_qual_pattern(CUSTOM_80BIT, DISABLE_SCRAM);
			break;
		case HBR2_COMPLIANCE:
			/*option 0*/
			/*displayport_reg_set_hbr2_scrambler_reset(252);*/

			/*option 1*/
			displayport_reg_set_hbr2_scrambler_reset(252*2);

			/*option 2*/
			/*displayport_reg_set_hbr2_scrambler_reset(252);*/
			/*displayport_reg_set_PN_Inverse_PHY_Lane(1);*/

			/*option 3*/
			/*displayport_reg_set_hbr2_scrambler_reset(252*2);*/
			/*displayport_reg_set_PN_Inverse_PHY_Lane(1);*/

			displayport_reg_set_qual_pattern(HBR2_COMPLIANCE, ENABLE_SCRAM);
			break;
		default:
			displayport_err("not supported link qual pattern");
			break;
		}
	} else if ((data & TEST_AUDIO_PATTERN) == TEST_AUDIO_PATTERN) {
		struct displayport_audio_config_data audio_config_data;

		displayport_reg_dpcd_read(DPCD_TEST_AUDIO_MODE, 1, val);
		displayport_info("TEST_AUDIO_MODE %02x %02x\n",
				(val[0] & TEST_AUDIO_SAMPLING_RATE),
				(val[0] & TEST_AUDIO_CHANNEL_COUNT) >> 4);

		displayport_reg_dpcd_read(DPCD_TEST_AUDIO_PATTERN_TYPE, 1, val);
		displayport_info("TEST_AUDIO_PATTERN_TYPE %02x\n", val[0]);

		msleep(300);

		audio_config_data.audio_enable = 1;
		audio_config_data.audio_fs =  (val[0] & TEST_AUDIO_SAMPLING_RATE);
		audio_config_data.audio_channel_cnt = (val[0] & TEST_AUDIO_CHANNEL_COUNT) >> 4;
		audio_config_data.audio_channel_cnt++;
		audio_config_data.audio_bit = AUDIO_16_BIT;
		audio_config_data.audio_packed_mode = 0;
		audio_config_data.audio_word_length = 0;
		displayport_audio_bist_config(sst_id, audio_config_data);
	} else {
		displayport_err("Not Supported AUTOMATED_TEST_REQUEST\n");
		return -EINVAL;
	}
	return 0;
}

static void displayport_hpd_irq_work(struct work_struct *work)
{
	u8 val[DPCP_LINK_SINK_STATUS_FIELD_LENGTH] = {0, };
	struct displayport_device *displayport = get_displayport_drvdata();
	u32 sst_id = displayport_get_sst_id_with_decon_id(DEFAULT_DECON_ID);

	if (!displayport->hpd_current_state) {
		displayport_info("HPD IRQ work: hpd is low\n");
		return;
	}

	displayport_dbg("detect HPD_IRQ\n");

	if (displayport->hdcp_ver == HDCP_VERSION_2_2) {
		int ret = displayport_reg_dpcd_read_burst(DPCD_ADD_SINK_COUNT,
				DPCP_LINK_SINK_STATUS_FIELD_LENGTH, val);

		displayport_info("HPD IRQ work2 %02x %02x %02x %02x %02x %02x\n",
				val[0], val[1], val[2], val[3], val[4], val[5]);
		if (ret < 0 || (val[0] & val[1] & val[2] & val[3] & val[4] & val[5]) == 0xff) {
			displayport_info("dpcd_read error in HPD IRQ work\n");
			return;
		}

		if ((val[1] & AUTOMATED_TEST_REQUEST) == AUTOMATED_TEST_REQUEST) {
			if (displayport_Automated_Test_Request() == 0)
				return;
		}

		if (displayport_check_dpcd_lane_status(val[2], val[3], val[4]) != 0) {
			displayport_info("link training in HPD IRQ work2\n");
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
			secdp_bigdata_inc_error_cnt(ERR_INF_IRQHPD);
#endif
#if IS_ENABLED(CONFIG_EXYNOS_HDCP2)
			hdcp_dplink_set_reauth();
#endif
			displayport_hdcp22_enable(0);

			displayport_link_training(sst_id);
			queue_delayed_work(displayport->hdcp2_wq,
					&displayport->hdcp22_work, msecs_to_jiffies(2000));
		}

		if (displayport->mst_cap == 1 && (val[1] & UP_REQ_MSG_RDY) == UP_REQ_MSG_RDY) {
			displayport_info("Detect UP_REQ_MSG_RDY IRQ\n");
			displayport_msg_rx(UP_REQ);
		} else if ((val[1] & CP_IRQ) == CP_IRQ) {
			displayport_info("hdcp22: detect CP_IRQ\n");
			ret = displayport_hdcp22_irq_handler();
			if (ret == 0)
				return;
		}
		return;
	}

	if (hdcp13_info.auth_state != HDCP13_STATE_AUTH_PROCESS) {
		int ret = displayport_reg_dpcd_read_burst(DPCD_ADD_SINK_COUNT,
				DPCP_LINK_SINK_STATUS_FIELD_LENGTH, val);

		displayport_info("HPD IRQ work1 %02x %02x %02x %02x %02x %02x\n",
				val[0], val[1], val[2], val[3], val[4], val[5]);
		if (ret < 0 || (val[0] & val[1] & val[2] & val[3] & val[4] & val[5]) == 0xff) {
			displayport_info("dpcd_read error in HPD IRQ work\n");
			return;
		}

		if ((val[1] & CP_IRQ) == CP_IRQ && displayport->hdcp_ver == HDCP_VERSION_1_3) {
			displayport_info("detect CP_IRQ\n");
			hdcp13_info.cp_irq_flag = 1;
			hdcp13_info.link_check = LINK_CHECK_NEED;
			hdcp13_link_integrity_check();

			if (hdcp13_info.auth_state == HDCP13_STATE_FAIL)
				queue_delayed_work(displayport->dp_wq,
						&displayport->hdcp13_work, msecs_to_jiffies(2000));
		}

		if ((val[1] & AUTOMATED_TEST_REQUEST) == AUTOMATED_TEST_REQUEST) {
			if (displayport_Automated_Test_Request() == 0)
				return;
		}

		if (!displayport->hpd_current_state) {
			displayport_info("HPD IRQ work: hpd is low\n");
			return;
		}

		if (displayport_check_dpcd_lane_status(val[2], val[3], val[4]) != 0) {
			displayport_info("link training in HPD IRQ work1\n");
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
			secdp_bigdata_inc_error_cnt(ERR_INF_IRQHPD);
#endif
			displayport_link_training(sst_id);

			hdcp13_info.auth_state = HDCP13_STATE_NOT_AUTHENTICATED;
			queue_delayed_work(displayport->dp_wq,
					&displayport->hdcp13_work, msecs_to_jiffies(2000));
		}

		if (displayport->mst_cap == 1 && (val[1] & UP_REQ_MSG_RDY) == UP_REQ_MSG_RDY) {
			displayport_info("Detect UP_REQ_MSG_RDY IRQ\n");
			displayport_msg_rx(UP_REQ);
		}
	} else {
		displayport_reg_dpcd_read(DPCD_ADD_DEVICE_SERVICE_IRQ_VECTOR, 1, val);

		if ((val[0] & CP_IRQ) == CP_IRQ) {
			displayport_info("detect CP_IRQ\n");
			hdcp13_info.cp_irq_flag = 1;
			displayport_reg_dpcd_read(ADDR_HDCP13_BSTATUS, 1, HDCP13_DPCD.HDCP13_BSTATUS);
		}

		if (displayport->mst_cap == 1 && (val[1] & UP_REQ_MSG_RDY) == UP_REQ_MSG_RDY) {
			displayport_info("Detect UP_REQ_MSG_RDY IRQ\n");
			displayport_msg_rx(UP_REQ);
		}
	}
}

static void displayport_hdcp13_integrity_check_work(struct work_struct *work)
{
	struct displayport_device *displayport = get_displayport_drvdata();

	if (displayport->hdcp_ver == HDCP_VERSION_1_3) {
		hdcp13_info.link_check = LINK_CHECK_NEED;
		hdcp13_link_integrity_check();
	}
}

static irqreturn_t displayport_irq_handler(int irq, void *dev_data)
{
	struct displayport_device *displayport = dev_data;
	struct decon_device *decon = NULL;
	int active;
	u32 irq_status_reg;
	int i = 0;

	spin_lock(&displayport->slock);

	active = pm_runtime_active(displayport->dev);
	if (!active) {
		displayport_info("displayport power(%d)\n", active);
		spin_unlock(&displayport->slock);
		return IRQ_HANDLED;
	}

	/* Common interrupt */
	irq_status_reg = displayport_reg_get_common_interrupt_and_clear();

#if !IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	if (irq_status_reg & HPD_CHG)
		displayport_info("HPD_CHG detect\n");

	if (irq_status_reg & HPD_LOST) {
		displayport_info("HPD_LOST detect\n");
	}

	if (irq_status_reg & HPD_PLUG_INT) {
		displayport_info("HPD_PLUG detect\n");
	}

	if (irq_status_reg & HPD_IRQ_FLAG) {
		displayport_info("HPD IRQ detect\n");
	}
#endif

	if (irq_status_reg & HDCP_LINK_CHK_FAIL) {
		queue_delayed_work(displayport->dp_wq, &displayport->hdcp13_integrity_check_work, 0);
		displayport_info("HDCP_LINK_CHK detect\n");
	}

	if (irq_status_reg & HDCP_R0_CHECK_FLAG) {
		hdcp13_info.r0_read_flag = 1;
		displayport_info("R0_CHECK_FLAG detect\n");
	}

	for (i = SST1; i < MAX_SST_CNT; i++) {
		irq_status_reg = displayport_reg_get_sst_video_interrupt_and_clear(i);

		if ((irq_status_reg & MAPI_FIFO_UNDER_FLOW)
				&& displayport->sst[i]->decon_run == 1)
			displayport_info("SST%d VIDEO FIFO_UNDER_FLOW detect\n", i + 1);

		if (displayport->sst[i]->bist_used == 0) {
			decon = get_decon_drvdata(displayport_get_decon_id(i));

			if (irq_status_reg & VSYNC_DET) {
				/* VSYNC interrupt, accept it */
				decon->frame_cnt++;
				wake_up_interruptible_all(&decon->wait_vstatus);

				if (decon->dt.psr_mode == DECON_VIDEO_MODE) {
					decon->vsync.timestamp = ktime_get();
					wake_up_interruptible_all(&decon->vsync.wait);
				}
			}
		}

		irq_status_reg = displayport_reg_get_sst_audio_interrupt_and_clear(i);

		if (irq_status_reg & MASTER_AUDIO_BUFFER_EMPTY_INT) {
			displayport_dbg("SST%d AFIFO_UNDER detect\n", i + 1);
			displayport->sst[i]->audio_buf_empty_check = 1;
		}
	}

	spin_unlock(&displayport->slock);

	return IRQ_HANDLED;
}

static u8 displayport_get_vic(u32 sst_id)
{
	struct displayport_device *displayport = get_displayport_drvdata();

	return supported_videos[displayport->sst[sst_id]->cur_video].vic;
}

__visible_for_testing int displayport_make_avi_infoframe_data(u32 sst_id,
		struct infoframe *avi_infoframe)
{
	int i;

	avi_infoframe->type_code = INFOFRAME_PACKET_TYPE_AVI;
	avi_infoframe->version_number = AVI_INFOFRAME_VERSION;
	avi_infoframe->length = AVI_INFOFRAME_LENGTH;

	for (i = 0; i < AVI_INFOFRAME_LENGTH; i++)
		avi_infoframe->data[i] = 0x00;

	avi_infoframe->data[0] |= ACTIVE_FORMAT_INFOMATION_PRESENT;
	avi_infoframe->data[1] |= ACITVE_PORTION_ASPECT_RATIO;
	avi_infoframe->data[3] = displayport_get_vic(sst_id);

	return 0;
}

__visible_for_testing int displayport_make_audio_infoframe_data(struct infoframe *audio_infoframe,
		struct displayport_audio_config_data *audio_config_data)
{
	int i;

	audio_infoframe->type_code = INFOFRAME_PACKET_TYPE_AUDIO;
	audio_infoframe->version_number = AUDIO_INFOFRAME_VERSION;
	audio_infoframe->length = AUDIO_INFOFRAME_LENGTH;

	for (i = 0; i < AUDIO_INFOFRAME_LENGTH; i++)
		audio_infoframe->data[i] = 0x00;

	/* Data Byte 1, PCM type and audio channel count */
	audio_infoframe->data[0] = ((u8)audio_config_data->audio_channel_cnt - 1);

	/* Data Byte 4, how various speaker locations are allocated */
	if (audio_config_data->audio_channel_cnt == 8)
		audio_infoframe->data[3] = 0x13;
	else if (audio_config_data->audio_channel_cnt == 6)
		audio_infoframe->data[3] = 0x0b;
	else
		audio_infoframe->data[3] = 0;

	displayport_info("audio_infoframe: type and ch_cnt %02x, SF and bit size %02x, ch_allocation %02x\n",
			audio_infoframe->data[0], audio_infoframe->data[1], audio_infoframe->data[3]);

	return 0;
}

__visible_for_testing int displayport_make_hdr_infoframe_data
	(struct infoframe *hdr_infoframe, struct exynos_hdr_static_info *hdr_info)
{
	int i;

	hdr_infoframe->type_code = INFOFRAME_PACKET_TYPE_HDR;
	hdr_infoframe->version_number = HDR_INFOFRAME_VERSION;
	hdr_infoframe->length = HDR_INFOFRAME_LENGTH;

	for (i = 0; i < HDR_INFOFRAME_LENGTH; i++)
		hdr_infoframe->data[i] = 0x00;

	hdr_infoframe->data[HDR_INFOFRAME_EOTF_BYTE_NUM] = HDR_INFOFRAME_SMPTE_ST_2084;
	hdr_infoframe->data[HDR_INFOFRAME_METADATA_ID_BYTE_NUM]
		= STATIC_MATADATA_TYPE_1;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_X_0_LSB]
		= hdr_info->stype1.mr.x & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_X_0_MSB]
		= (hdr_info->stype1.mr.x & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_Y_0_LSB]
		= hdr_info->stype1.mr.y & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_Y_0_MSB]
		= (hdr_info->stype1.mr.y & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_X_1_LSB]
		= hdr_info->stype1.mg.x & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_X_1_MSB]
		= (hdr_info->stype1.mg.x & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_Y_1_LSB]
		= hdr_info->stype1.mg.y & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_Y_1_MSB]
		= (hdr_info->stype1.mg.y & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_X_2_LSB]
		= hdr_info->stype1.mb.x & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_X_2_MSB]
		= (hdr_info->stype1.mb.x & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_Y_2_LSB]
		= hdr_info->stype1.mb.y & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_DISP_PRI_Y_2_MSB]
		= (hdr_info->stype1.mb.y & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_WHITE_POINT_X_LSB]
		= hdr_info->stype1.mw.x & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_WHITE_POINT_X_MSB]
		= (hdr_info->stype1.mw.x & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_WHITE_POINT_Y_LSB]
		= hdr_info->stype1.mw.y & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_WHITE_POINT_Y_MSB]
		= (hdr_info->stype1.mw.y & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_MAX_LUMI_LSB]
		= hdr_info->stype1.mmax_display_luminance & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_MAX_LUMI_MSB]
		= (hdr_info->stype1.mmax_display_luminance & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_MIN_LUMI_LSB]
		= hdr_info->stype1.mmin_display_luminance & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_MIN_LUMI_MSB]
		= (hdr_info->stype1.mmin_display_luminance & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_MAX_LIGHT_LEVEL_LSB]
		= hdr_info->stype1.mmax_content_light_level & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_MAX_LIGHT_LEVEL_MSB]
		= (hdr_info->stype1.mmax_content_light_level & MSB_MASK) >> SHIFT_8BIT;
	hdr_infoframe->data[HDR_INFOFRAME_MAX_AVERAGE_LEVEL_LSB]
		= hdr_info->stype1.mmax_frame_average_light_level & LSB_MASK;
	hdr_infoframe->data[HDR_INFOFRAME_MAX_AVERAGE_LEVEL_MSB]
		= (hdr_info->stype1.mmax_frame_average_light_level & MSB_MASK) >> SHIFT_8BIT;

	for (i = 0; i < HDR_INFOFRAME_LENGTH; i++) {
		displayport_dbg("hdr_infoframe->data[%d] = 0x%02x", i,
			hdr_infoframe->data[i]);
	}

	print_hex_dump(KERN_INFO, "HDR: ", DUMP_PREFIX_NONE, 32, 1,
			hdr_infoframe->data, HDR_INFOFRAME_LENGTH, false);

	return 0;
}

static int displayport_set_avi_infoframe(u32 sst_id)
{
	struct infoframe avi_infoframe;

	displayport_make_avi_infoframe_data(sst_id, &avi_infoframe);
	displayport_reg_set_avi_infoframe(sst_id, avi_infoframe);

	return 0;
}

__visible_for_testing int displayport_make_spd_infoframe_data(struct infoframe *spd_infoframe)
{
	spd_infoframe->type_code = 0x83;
	spd_infoframe->version_number = 0x1;
	spd_infoframe->length = 25;

	strncpy(&spd_infoframe->data[0], "SEC.MCB", 8);
	strncpy(&spd_infoframe->data[8], "GALAXY", 7);
	/* spd_infoframe->data[24] = 0xA; BD player*/

	return 0;
}

static int displayport_set_spd_infoframe(u32 sst_id)
{
	struct infoframe spd_infoframe;

	memset(&spd_infoframe, 0, sizeof(spd_infoframe));
	displayport_make_spd_infoframe_data(&spd_infoframe);
	displayport_reg_set_spd_infoframe(sst_id, spd_infoframe);

	return 0;
}

static int displayport_set_audio_infoframe(u32 sst_id,
		struct displayport_audio_config_data *audio_config_data)
{
	struct infoframe audio_infoframe;

	displayport_make_audio_infoframe_data(&audio_infoframe, audio_config_data);
	displayport_reg_set_audio_infoframe(sst_id, audio_infoframe, audio_config_data->audio_enable);

	return 0;
}

static int displayport_set_hdr_infoframe(u32 sst_id,
		struct exynos_hdr_static_info *hdr_info)
{
	struct infoframe hdr_infoframe = {0, 0, 0, {0, } };

	if (hdr_info->mid >= 0) {
		displayport_info("SST%d displayport_set_hdr_infoframe 1\n", sst_id + 1);
		displayport_make_hdr_infoframe_data(&hdr_infoframe, hdr_info);
		displayport_reg_set_hdr_infoframe(sst_id, hdr_infoframe, 1);
	} else {
		displayport_info("SST%d displayport_set_hdr_infoframe 0\n", sst_id + 1);
		displayport_reg_set_hdr_infoframe(sst_id, hdr_infoframe, 0);
	}

	return 0;
}

int displayport_audio_config(u32 sst_id, struct displayport_audio_config_data *audio_config_data)
{
	struct displayport_device *displayport = get_displayport_drvdata();
	int ret = 0;

	if (displayport->sst[sst_id]->state == DISPLAYPORT_STATE_OFF || !phy_status) {
		displayport_warn("power status is off timing\n");
		return -EINVAL;
	}

	displayport_info("SST%d audio config(%d ==> %d)\n",
			sst_id + 1, displayport->sst[sst_id]->audio_state, audio_config_data->audio_enable);

	if (displayport_check_sst_on(displayport) == DISPLAYPORT_STATE_OFF)
		return 0;

	if (audio_config_data->audio_enable == displayport->sst[sst_id]->audio_state)
		return 0;

	if (audio_config_data->audio_enable == AUDIO_ENABLE) {
		displayport_info("audio_enable:%d, ch:%d, fs:%d, bit:%d, packed:%d, word_len:%d\n",
				audio_config_data->audio_enable, audio_config_data->audio_channel_cnt,
				audio_config_data->audio_fs, audio_config_data->audio_bit,
				audio_config_data->audio_packed_mode, audio_config_data->audio_word_length);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
		{
			int bit[] = {16, 20, 24};
			int fs[] = {32000, 44100, 48000, 88200, 96000, 176400, 192000};

			secdp_bigdata_save_item(BD_AUD_CH, audio_config_data->audio_channel_cnt);
			if (audio_config_data->audio_fs >= 0 && audio_config_data->audio_fs < 7)
				secdp_bigdata_save_item(BD_AUD_FREQ, fs[audio_config_data->audio_fs]);
			if (audio_config_data->audio_bit >= 0 && audio_config_data->audio_bit < 3)
				secdp_bigdata_save_item(BD_AUD_BIT, bit[audio_config_data->audio_bit]);
		}
#endif
		displayport_audio_enable(sst_id, audio_config_data);
		displayport_set_audio_infoframe(sst_id, audio_config_data);
		displayport->sst[sst_id]->audio_state = AUDIO_ENABLE;
	} else if (audio_config_data->audio_enable == AUDIO_DISABLE) {
		displayport_audio_disable(sst_id);
		displayport_set_audio_infoframe(sst_id, audio_config_data);
		displayport->sst[sst_id]->audio_state = AUDIO_DISABLE;
	} else if (audio_config_data->audio_enable == AUDIO_WAIT_BUF_FULL) {
		displayport_audio_wait_buf_full(sst_id);
		displayport->sst[sst_id]->audio_state = AUDIO_WAIT_BUF_FULL;
	} else if (audio_config_data->audio_enable == AUDIO_DMA_REQ_HIGH) {
		displayport_reg_set_dma_req_gen(sst_id, 1);
		displayport->sst[sst_id]->audio_state = AUDIO_DMA_REQ_HIGH;
	} else
		displayport_info("Not support audio_enable = %d\n", audio_config_data->audio_enable);

	return ret;
}
EXPORT_SYMBOL(displayport_audio_config);

void displayport_audio_bist_config(u32 sst_id,
		struct displayport_audio_config_data audio_config_data)
{
	displayport_info("displayport_audio_bist:sst_id%d\n", sst_id);
	displayport_info("audio_enable = %d\n", audio_config_data.audio_enable);
	displayport_info("audio_channel_cnt = %d\n", audio_config_data.audio_channel_cnt);
	displayport_info("audio_fs = %d\n", audio_config_data.audio_fs);
	displayport_audio_bist_enable(sst_id, audio_config_data);
	displayport_set_audio_infoframe(sst_id, &audio_config_data);
}

int displayport_dpcd_read_for_hdcp22(u32 address, u32 length, u8 *data)
{
	int ret;

	ret = displayport_reg_dpcd_read_burst(address, length, data);

	if (ret != 0)
		displayport_err("dpcd_read_for_hdcp22 fail: 0x%X\n", address);

	return ret;
}

int displayport_dpcd_write_for_hdcp22(u32 address, u32 length, u8 *data)
{
	int ret;

	ret = displayport_reg_dpcd_write_burst(address, length, data);

	if (ret != 0)
		displayport_err("dpcd_write_for_hdcp22 fail: 0x%X\n", address);

	return ret;
}

void displayport_hdcp22_enable(u32 en)
{
	struct decon_device *decon = get_decon_drvdata(DEFAULT_DECON_ID);

	/* wait 2 frames for hdcp encryption enable/disable */
	decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);
	decon_wait_for_vsync(decon, VSYNC_TIMEOUT_MSEC);

	if (en) {
		displayport_reg_set_hdcp22_system_enable(1);
		displayport_reg_set_hdcp22_mode(1);
		displayport_reg_set_hdcp22_encryption_enable(1);
	} else {
		displayport_reg_set_hdcp22_system_enable(0);
		displayport_reg_set_hdcp22_mode(0);
		displayport_reg_set_hdcp22_encryption_enable(0);
	}
}
EXPORT_SYMBOL(displayport_hdcp22_enable);
EXPORT_SYMBOL(displayport_dpcd_write_for_hdcp22);
EXPORT_SYMBOL(displayport_dpcd_read_for_hdcp22);

static void displayport_hdcp13_run(struct work_struct *work)
{
	struct displayport_device *displayport = get_displayport_drvdata();

	if (displayport->hdcp_ver != HDCP_VERSION_1_3 ||
			!displayport->hpd_current_state)
		return;

	displayport_dbg("[HDCP 1.3] run\n");
	hdcp13_run();
	if (hdcp13_info.auth_state == HDCP13_STATE_FAIL) {
		queue_delayed_work(displayport->dp_wq, &displayport->hdcp13_work,
				msecs_to_jiffies(2000));
	}
}

static void displayport_hdcp22_run(struct work_struct *work)
{
#if IS_ENABLED(CONFIG_EXYNOS_HDCP2)
	struct displayport_device *displayport = get_displayport_drvdata();
	u32 ret;
	u8 val[2] = {0, };

	mutex_lock(&displayport->hdcp2_lock);
	if (IS_DISPLAYPORT_HPD_PLUG_STATE() == 0) {
		displayport_info("stop hdcp2 : HPD is low\n");
		goto exit_hdcp;
	}

	displayport_hdcp22_notify_state(DP_HDCP_READY);
	ret = displayport_hdcp22_authenticate();
	if (ret) {
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
		secdp_bigdata_inc_error_cnt(ERR_HDCP_AUTH);
#endif
		goto exit_hdcp;
	}

	if (IS_DISPLAYPORT_HPD_PLUG_STATE() == 0) {
		displayport_info("stop hdcp2 : HPD is low\n");
		goto exit_hdcp;
	}

	displayport_dpcd_read_for_hdcp22(DPCD_HDCP22_RX_INFO, 2, val);
	displayport_info("HDCP2.2 rx_info: 0:0x%X, 8:0x%X\n", val[1], val[0]);

exit_hdcp:
	mutex_unlock(&displayport->hdcp2_lock);
#else
	displayport_info("Not compiled EXYNOS_HDCP2 driver\n");
#endif
}

static int displayport_check_hdcp_version(void)
{
	int ret = 0;
	u8 val[DPCD_HDCP22_RX_CAPS_LENGTH];
	u32 rx_caps = 0;
	int i;

	hdcp13_dpcd_buffer();

	if (hdcp13_read_bcap() != 0)
		displayport_dbg("[HDCP 1.3] NONE HDCP CAPABLE\n");
#if defined(HDCP_SUPPORT)
	else
		ret = HDCP_VERSION_1_3;
#endif
	displayport_dpcd_read_for_hdcp22(DPCD_HDCP22_RX_CAPS, DPCD_HDCP22_RX_CAPS_LENGTH, val);

	for (i = 0; i < DPCD_HDCP22_RX_CAPS_LENGTH; i++)
		rx_caps |= (u32)val[i] << ((DPCD_HDCP22_RX_CAPS_LENGTH - (i + 1)) * 8);

	displayport_info("HDCP2.2 rx_caps = 0x%x\n", rx_caps);

	if ((((rx_caps & VERSION) >> DPCD_HDCP_VERSION_BIT_POSITION) == (HDCP_VERSION_2_2))
			&& ((rx_caps & HDCP_CAPABLE) != 0)) {
#if defined(HDCP_2_2)
		ret = HDCP_VERSION_2_2;
#endif
		displayport_dbg("displayport_rx supports hdcp2.2\n");
	}
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	if (ret == HDCP_VERSION_2_2)
		secdp_bigdata_save_item(BD_HDCP_VER, "hdcp2");
	else if (ret == HDCP_VERSION_1_3)
		secdp_bigdata_save_item(BD_HDCP_VER, "hdcp1");
#endif

	return ret;
}

static void hdcp_start(struct displayport_device *displayport)
{
	displayport->hdcp_ver = displayport_check_hdcp_version();
	if(forced_hdcp == 13) displayport->hdcp_ver = HDCP_VERSION_1_3;
	if(forced_hdcp == 22) displayport->hdcp_ver = HDCP_VERSION_2_2;
	if(forced_hdcp == 99) {
		displayport_info("HDCP is off\n");
		return;
	}
#if defined(HDCP_SUPPORT)
	if (displayport->hdcp_ver == HDCP_VERSION_2_2)
		queue_delayed_work(displayport->hdcp2_wq, &displayport->hdcp22_work,
				msecs_to_jiffies(3500));
	else if (displayport->hdcp_ver == HDCP_VERSION_1_3)
		queue_delayed_work(displayport->dp_wq, &displayport->hdcp13_work,
				msecs_to_jiffies(5500));
	else
		displayport_info("HDCP is not supported\n");
#endif
}

static enum displayport_state displayport_check_sst_on(struct displayport_device *displayport)
{
	int ret = DISPLAYPORT_STATE_OFF;
	int i = 0;

	for (i = SST1; i < MAX_SST_CNT; i++) {
		if (displayport->sst[i]->state == DISPLAYPORT_STATE_ON) {
			ret = DISPLAYPORT_STATE_ON;
			break;
		}
	}

	return ret;
}

int displayport_enable(struct displayport_device *displayport)
{
	int ret = 0;
	u32 sst_id = displayport->cur_sst_id;
	u8 bpc = (u8)displayport->sst[sst_id]->bpc;
	u8 bist_type = (u8)displayport->sst[sst_id]->bist_type;
	u8 dyn_range = (u8)displayport->sst[sst_id]->dyn_range;

	mutex_lock(&displayport->cmd_lock);

	if (!displayport->hpd_current_state) {
		displayport_err("%s() hpd is low\n", __func__);
		mutex_unlock(&displayport->cmd_lock);
		return -ENODEV;
	}

	if (displayport->sst[sst_id]->state == DISPLAYPORT_STATE_ON) {
		displayport_err("SST%d ignore enable, state:%d\n", sst_id + 1,
				displayport->sst[sst_id]->state);
		mutex_unlock(&displayport->cmd_lock);
		return 0;
	}

	if (displayport->sst[sst_id]->state != DISPLAYPORT_STATE_INIT) {
		displayport_err("SST%d is not INIT state, state:%d\n", sst_id + 1,
				displayport->sst[sst_id]->state);
		mutex_unlock(&displayport->cmd_lock);
		return -ENODEV;
	}

	if (forced_resolution >= 0)
		displayport->sst[sst_id]->cur_video = forced_resolution;
	else if (displayport->sst[sst_id]->bist_used)
		displayport->sst[sst_id]->cur_video = displayport->sst[sst_id]->best_video;

	if (displayport->mst_cap) {
		displayport_topology_set_vc(sst_id);
		usleep_range(25, 30);
	}

	if (displayport_check_sst_on(displayport) != DISPLAYPORT_STATE_ON) {
		displayport_info("first sst on\n");
#if defined(CONFIG_CPU_IDLE)
		/* block to enter SICD mode */
		exynos_update_ip_idle_status(displayport->idle_ip_index, 0);
#endif
		enable_irq(displayport->res.irq);
	}

	if (displayport->sst[sst_id]->bist_used) {
		displayport_reg_set_bist_video_configuration(sst_id,
				displayport->sst[sst_id]->cur_video, bpc, bist_type, dyn_range);
	} else {
		if (displayport->sst[sst_id]->bpc == BPC_6 && displayport->dfp_type != DFP_TYPE_DP) {
			bpc = BPC_8;
			displayport->sst[sst_id]->bpc = BPC_8;
		}

		displayport_reg_set_video_configuration(sst_id,
				displayport->sst[sst_id]->cur_video, bpc, dyn_range);
	}

	displayport_info("SST%d cur_video = %s in displayport_enable!!!\n", sst_id + 1,
			supported_videos[displayport->sst[sst_id]->cur_video].name);

	displayport_set_avi_infoframe(sst_id);
	displayport_set_spd_infoframe(sst_id);
#ifdef HDCP_SUPPORT
	displayport_reg_video_mute(0);
#endif
	displayport_reg_start(sst_id);

	if (displayport_check_sst_on(displayport) != DISPLAYPORT_STATE_ON)
		hdcp_start(displayport);

	displayport->sst[sst_id]->state = DISPLAYPORT_STATE_ON;

#ifdef CONFIG_SEC_DISPLAYPORT_SELFTEST
	if (self_test_on_process()) {
		int idx = displayport->sst[sst_id]->cur_video;

		self_test_resolution_update(supported_videos[idx].dv_timings.bt.width,
				supported_videos[idx].dv_timings.bt.height,
				supported_videos[idx].fps);
	}
#endif

	mutex_unlock(&displayport->cmd_lock);

	return ret;
}

int displayport_disable(struct displayport_device *displayport)
{
	u32 sst_id = displayport->cur_sst_id;

	displayport_info("SST%d %s +, state: %d\n", sst_id + 1,
			__func__, displayport->sst[sst_id]->state);

	mutex_lock(&displayport->cmd_lock);

	if (displayport->sst[sst_id]->state != DISPLAYPORT_STATE_ON) {
		displayport_err("ignore disable, state:%d\n", displayport->sst[sst_id]->state);
		mutex_unlock(&displayport->cmd_lock);
		return 0;
	}

	displayport_reg_stop(sst_id);
	displayport_reg_set_video_bist_mode(sst_id, 0);

	if (displayport->mst_cap && displayport->hpd_current_state == HPD_PLUG) {
		displayport_mst_payload_table_delete(sst_id);
		displayport_clr_vc_config(sst_id, displayport);
		displayport_info("SST%d displayport_mst_payload_table_delete\n", sst_id + 1);
	}

	displayport->sst[sst_id]->state = DISPLAYPORT_STATE_OFF_CHECK;

	if (displayport_check_sst_on(displayport) == DISPLAYPORT_STATE_OFF) {
		disable_irq(displayport->res.irq);

		hdcp13_info.auth_state = HDCP13_STATE_NOT_AUTHENTICATED;
		displayport_info("all sst off\n");
	}

	displayport->sst[sst_id]->state = DISPLAYPORT_STATE_OFF;

	displayport_info("%s -\n", __func__);

	mutex_unlock(&displayport->cmd_lock);

	return 0;
}

bool displayport_match_timings(const struct v4l2_dv_timings *t1,
		const struct v4l2_dv_timings *t2,
		unsigned pclock_delta)
{
	if (t1->type != t2->type)
		return false;

	if (t1->bt.width == t2->bt.width &&
			t1->bt.height == t2->bt.height &&
			t1->bt.interlaced == t2->bt.interlaced &&
			t1->bt.polarities == t2->bt.polarities &&
			t1->bt.pixelclock >= t2->bt.pixelclock - pclock_delta &&
			t1->bt.pixelclock <= t2->bt.pixelclock + pclock_delta &&
			t1->bt.hfrontporch == t2->bt.hfrontporch &&
			t1->bt.vfrontporch == t2->bt.vfrontporch &&
			t1->bt.vsync == t2->bt.vsync &&
			t1->bt.vbackporch == t2->bt.vbackporch &&
			(!t1->bt.interlaced ||
			 (t1->bt.il_vfrontporch == t2->bt.il_vfrontporch &&
			  t1->bt.il_vsync == t2->bt.il_vsync &&
			  t1->bt.il_vbackporch == t2->bt.il_vbackporch)))
		return true;

	return false;
}

static int displayport_timing2conf(struct v4l2_dv_timings *timings)
{
	int i;

	/*to select last index when there are same timings, use descending order*/
	/*for FEATURE_USE_PREFERRED_TIMING_1ST*/
	for (i = supported_videos_pre_cnt - 1; i >= 0; i--) {
		if (displayport_match_timings(&supported_videos[i].dv_timings,
					timings, 0))
			return i;
	}

	return -EINVAL;
}

static void displayport_dv_timings_to_str(const struct v4l2_dv_timings *t, char *str, int size)
{
	const struct v4l2_bt_timings *bt = &t->bt;
	u32 htot, vtot;
	u32 fps;

	if (t->type != V4L2_DV_BT_656_1120)
		return;

	htot = V4L2_DV_BT_FRAME_WIDTH(bt);
	vtot = V4L2_DV_BT_FRAME_HEIGHT(bt);
	if (bt->interlaced)
		vtot /= 2;

	fps = (htot * vtot) > 0 ? div_u64((100 * (u64)bt->pixelclock),
			(htot * vtot)) : 0;

	snprintf(str, size - 1, "%ux%u%s%u.%u", bt->width, bt->height,
			bt->interlaced ? "i" : "p", fps / 100, fps % 100);
}

static int displayport_s_dv_timings(struct v4l2_subdev *sd,
		struct v4l2_dv_timings *timings)
{
	struct displayport_device *displayport = container_of(sd, struct displayport_device, sd);
	u32 sst_id = displayport->cur_sst_id;
	struct decon_device *decon = get_decon_drvdata(displayport_get_decon_id(sst_id));
	videoformat displayport_setting_videoformat = V640X480P60;
	int ret = 0;
	char timingstr[32] = {0x0, };

	if (displayport->sst[sst_id]->state == DISPLAYPORT_STATE_OFF || !phy_status) {
		displayport_warn("power status is off timing\n");
		return 0;
	}

	displayport_dv_timings_to_str(timings, timingstr, sizeof(timingstr));
	displayport_info("set timing %s(%s)\n", timingstr, displayport->dex_setting ? "dex":"mirror");
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	secdp_bigdata_save_item(BD_RESOLUTION, timingstr);
#endif
	ret = displayport_timing2conf(timings);
	if (ret < 0) {
		displayport_err("displayport timings not supported\n");
		return -EINVAL;
	}
	displayport_setting_videoformat = ret;

	if (displayport->sst[sst_id]->bist_used == 0) {
		if (displayport_check_pixel_clock_for_hdr(sst_id, displayport,
				displayport_setting_videoformat) == true
				&& displayport_setting_videoformat >= V3840X2160P50
				&& displayport_setting_videoformat < V640X10P60SACRC) {
			displayport_info("sink support HDR\n");
			/* BPC_10 should be enabled when support HDR */
			displayport->sst[sst_id]->bpc = BPC_10;
		} else {
			displayport_info("can't support HDR\n");
			displayport->sst[sst_id]->rx_edid_data.hdr_support = 0;
			displayport->sst[sst_id]->bpc = BPC_8;
		}

		/*fail safe mode (640x480) with 6 bpc*/
		if (displayport_setting_videoformat == V640X480P60)
			displayport->sst[sst_id]->bpc = BPC_6;
	}

	displayport->sst[sst_id]->cur_video = displayport_setting_videoformat;
	displayport->sst[sst_id]->cur_timings = *timings;

	decon_displayport_get_out_sd(decon);

	displayport_dbg("SST%d New cur_video = %s\n", sst_id + 1,
			supported_videos[displayport->sst[sst_id]->cur_video].name);

	return 0;
}

static int displayport_g_dv_timings(struct v4l2_subdev *sd,
		struct v4l2_dv_timings *timings)
{
	struct displayport_device *displayport = container_of(sd, struct displayport_device, sd);

	*timings = displayport->sst[displayport->cur_sst_id]->cur_timings;

	displayport_dbg("displayport_g_dv_timings\n");

	return 0;
}

static int displayport_enum_dv_timings(struct v4l2_subdev *sd,
		struct v4l2_enum_dv_timings *timings)
{
	struct displayport_device *displayport = container_of(sd, struct displayport_device, sd);
	u32 sst_id = displayport->cur_sst_id;

	if (displayport->sst[sst_id]->state == DISPLAYPORT_STATE_OFF || !phy_status) {
		displayport_warn("power status is off timing id(%d)\n", timings->index);
		if (timings->index == 0) {
			timings->timings = supported_videos[timings->index].dv_timings;
			return 0;
		} else
			return -E2BIG;
	}

	if (timings->index >= supported_videos_pre_cnt) {
		displayport_warn("request index %d is too big\n", timings->index);
		return -E2BIG;
	}

	if (!supported_videos[timings->index].edid_support_match) {
		displayport_dbg("not supported video_format : %s(%d)\n",
				supported_videos[timings->index].name, timings->index);
		return -EINVAL;
	}

	/* V640X480P60 is always supported.*/
	/* it's to avoid that any one is not selected */
	if (timings->index == V640X480P60) {
		timings->timings = supported_videos[timings->index].dv_timings;
		return 0;
	}

	/* almost MST adapters do not support 4K@60Hz pixel clock */
	if (displayport->mst_cap && timings->index > MST_MAX_VIDEO_FOR_MIRROR) {
		displayport_dbg("not supported video_format : %s on mst mode\n",
				supported_videos[timings->index].name);
		return -EINVAL;
	}

#ifdef FEATURE_MANAGE_HMD_LIST
	if (displayport->dex_setting && !displayport->is_hmd_dev) {
#else
	if (displayport->dex_setting) {
#endif
		if (displayport->dex_video_pick &&
				timings->index > displayport->dex_video_pick) {
			displayport_info("dex proper ratio video pick %d\n", displayport->dex_video_pick);
			return -E2BIG;
		}
		if (displayport->mst_cap && displayport->mst_mode &&
					timings->index > MST_MAX_VIDEO_FOR_DEX) {
			displayport_dbg("not supported video_format : %s in dex mst mode\n",
					supported_videos[timings->index].name);
			return -EINVAL;
		}
		if (supported_videos[timings->index].dex_support == DEX_NOT_SUPPORT) {
			displayport_dbg("not supported video_format : %s on dex mode\n",
					supported_videos[timings->index].name);
			return -EINVAL;
		}
#ifdef CONFIG_SEC_DISPLAYPORT_SELFTEST
		if (self_test_on_process())
			displayport->dex_adapter_type = self_test_get_dp_adapter_type();
#endif
		if (supported_videos[timings->index].dex_support > displayport->dex_adapter_type) {
			displayport_info("%s not supported, adapter:%d, resolution:%d in dex mode\n",
					supported_videos[timings->index].name,
					displayport->dex_adapter_type,
					supported_videos[timings->index].dex_support);
			return -EINVAL;
		}
	}

	/* reduce the timing by lane count and link rate */
	if (displayport_check_link_rate_pixel_clock(displayport_reg_phy_get_link_bw(),
			displayport_reg_get_lane_count(),
			supported_videos[timings->index].dv_timings.bt.pixelclock,
			BPC_8) == false) {
		displayport_info("can't set index %d (over link rate)\n", timings->index);
		return -EINVAL;
	}

	if (reduced_resolution && reduced_resolution <
			supported_videos[timings->index].dv_timings.bt.pixelclock) {
		displayport_info("reduced_resolution: %llu, idx: %d\n",
				reduced_resolution, timings->index);
		return -E2BIG;
	}

	displayport_dbg("matched video_format: %s(%d)\n",
				supported_videos[timings->index].name, timings->index);
	timings->timings = supported_videos[timings->index].dv_timings;

	return 0;
}

static int displayport_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct displayport_device *displayport = container_of(sd, struct displayport_device, sd);

	if (enable)
		return displayport_enable(displayport);
	else
		return displayport_disable(displayport);
}

int displayport_set_hdr_config(u32 sst_id, struct exynos_hdr_static_info *hdr_info)
{
	int ret = 0;

	displayport_set_hdr_infoframe(sst_id, hdr_info);

	return ret;
}

void displayport_get_hdr_support(u32 sst_id,
		struct displayport_device *displayport, int *hdr_support)
{
	*hdr_support = (int)displayport->sst[sst_id]->rx_edid_data.hdr_support;
	displayport_info("SST%d get hdr_support: %d\n", sst_id + 1, *hdr_support);
}

bool is_displayport_not_running(void)
{
	int i = 0;

	struct displayport_device *displayport = get_displayport_drvdata();

	for (i = SST1; i < MAX_SST_CNT; i++) {
		if (displayport->sst[i]->state == DISPLAYPORT_STATE_ON)
			return false;
	}

	return true;
}

static long displayport_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct displayport_device *displayport = container_of(sd, struct displayport_device, sd);
	int ret = 0;
	struct v4l2_enum_dv_timings *enum_timings;
	struct exynos_hdr_static_info *hdr_info;
	static int *hdr_support;

	switch (cmd) {
	case DISPLAYPORT_IOC_DUMP:
		displayport_dump_registers(displayport);
		break;

	case DISPLAYPORT_IOC_GET_ENUM_DV_TIMINGS:
		enum_timings = (struct v4l2_enum_dv_timings *)arg;

		ret = displayport_enum_dv_timings(sd, enum_timings);
		break;

	case DISPLAYPORT_IOC_SET_RECONNECTION:	/* for restart without hpd change */
		displayport_set_reconnection();
		break;

	case DISPLAYPORT_IOC_SET_HDR_METADATA:
		if (*hdr_support == 0) {
			displayport_dbg("sink device is not support HDR\n");
			break;
		}
		hdr_info = (struct exynos_hdr_static_info *)arg;
		/* set info frame for hdr contents */
		ret = displayport_set_hdr_config(displayport->cur_sst_id, hdr_info);
		if (ret)
			displayport_err("failed to configure hdr info\n");
		break;

	case DISPLAYPORT_IOC_GET_HDR_INFO:
		hdr_support = (int *)arg;

		displayport_get_hdr_support(displayport->cur_sst_id,
				displayport, hdr_support);
		break;

	case EXYNOS_DPU_GET_ACLK:
		return clk_get_rate(displayport->res.aclk);

	default:
		displayport_err("unsupported ioctl");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct v4l2_subdev_core_ops displayport_sd_core_ops = {
	.ioctl = displayport_ioctl,
};

static const struct v4l2_subdev_video_ops displayport_sd_video_ops = {
	.s_dv_timings = displayport_s_dv_timings,
	.g_dv_timings = displayport_g_dv_timings,
	.s_stream = displayport_s_stream,
};

static const struct v4l2_subdev_ops displayport_subdev_ops = {
	.core = &displayport_sd_core_ops,
	.video = &displayport_sd_video_ops,
};

static void displayport_init_subdev(struct displayport_device *displayport)
{
	struct v4l2_subdev *sd = &displayport->sd;

	v4l2_subdev_init(sd, &displayport_subdev_ops);
	sd->owner = THIS_MODULE;
	snprintf(sd->name, sizeof(sd->name), "%s", "displayport-sd");
	v4l2_set_subdevdata(sd, displayport);
}

static int displayport_parse_dt(struct displayport_device *displayport, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret;

	if (IS_ERR_OR_NULL(dev->of_node)) {
		displayport_err("no device tree information\n");
		return -EINVAL;
	}

	displayport->dev = dev;

	displayport->gpio_sw_oe = of_get_named_gpio(np, "dp,aux_sw_oe", 0);
	if (gpio_is_valid(displayport->gpio_sw_oe)) {
		ret = gpio_request(displayport->gpio_sw_oe, "dp_aux_sw_oe");
		if (ret)
			displayport_err("failed to get gpio dp_aux_sw_oe request\n");
		else
			gpio_direction_output(displayport->gpio_sw_oe, 1);
	} else
		displayport_err("failed to get gpio dp_aux_sw_oe validation\n");

	displayport->gpio_sw_sel = of_get_named_gpio(np, "dp,sbu_sw_sel", 0);
	if (gpio_is_valid(displayport->gpio_sw_sel)) {
		ret = gpio_request(displayport->gpio_sw_sel, "dp_sbu_sw_sel");
		if (ret)
			displayport_err("failed to get gpio dp_sbu_sw_sel request\n");
	} else
		displayport_err("failed to get gpio dp_sbu_sw_sel validation\n");

	displayport->gpio_usb_dir = of_get_named_gpio(np, "dp,usb_con_sel", 0);
	if (!gpio_is_valid(displayport->gpio_usb_dir))
		displayport_err("failed to get gpio dp_usb_con_sel validation\n");

	if (of_get_property(np, "dp,dex_fhd_support", NULL))
		displayport->dex_max_resolution = DEX_FHD_SUPPORT;
	else
		displayport->dex_max_resolution = DEX_WQHD_SUPPORT;

	displayport_info("%s done %d, %d, %d\n", __func__,
		displayport->gpio_sw_oe, displayport->gpio_sw_sel, displayport->gpio_usb_dir);

	return 0;
}

static int displayport_init_resources(struct displayport_device *displayport, struct platform_device *pdev)
{
	struct resource *res;
	int ret = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		displayport_err("failed to get mem resource\n");
		return -ENOENT;
	}

	displayport_info("link_regs: start(0x%x), end(0x%x)\n", (u32)res->start, (u32)res->end);

	displayport->res.link_regs = devm_ioremap_resource(displayport->dev, res);
	if (IS_ERR(displayport->res.link_regs)) {
		displayport_err("failed to remap DisplayPort LINK SFR region\n");
		return -EINVAL;
	}

	displayport->res.usbdp_regs = ioremap(USBDP_PHY_CONTROL, SZ_4);
	if (!displayport->res.usbdp_regs) {
		displayport_err("failed to remap USBDP SFR region\n");
		return -EINVAL;
	}
#if IS_ENABLED(CONFIG_PHY_EXYNOS_USBDRD)
	/*displayport->res.phy_regs = phy_exynos_usbdp_get_address();*/
	displayport->res.phy_regs = ioremap((phys_addr_t)0x10AE0000, 0x2800);
	displayport_info("usbdp combo phy_regs: start(0x%x)\n", displayport->res.phy_regs);
	if (!displayport->res.phy_regs) {
		displayport_err("failed to get USBDP combo PHY SFR region\n");
		return -EINVAL;
	}
#endif

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		displayport_err("failed to get irq resource\n");
		return -ENOENT;
	}

	displayport->res.irq = res->start;
	ret = devm_request_irq(displayport->dev, res->start,
			displayport_irq_handler, 0, pdev->name, displayport);
	if (ret) {
		displayport_err("failed to install DisplayPort irq\n");
		return -EINVAL;
	}
	disable_irq(displayport->res.irq);

	displayport->res.aclk = devm_clk_get(displayport->dev, "aclk");
	if (IS_ERR_OR_NULL(displayport->res.aclk)) {
		displayport_err("failed to get aclk\n");
		return PTR_ERR(displayport->res.aclk);
	}

	return 0;
}

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
static int displayport_aux_onoff(struct displayport_device *displayport, int
		onoff)
{
	int rc = 0;

	displayport_info("aux vdd onoff = %d\n", onoff);
	if (gpio_is_valid(displayport->gpio_sw_oe)) {
		if (onoff == 1) {
			gpio_direction_output(displayport->gpio_sw_oe, 0);
				msleep(100);
		} else
			gpio_direction_output(displayport->gpio_sw_oe, 1);
	} else {
		displayport_info("aux switch is not available\n");
		rc = -1;
	}
	return rc;
}

static void displayport_aux_sel(struct displayport_device *displayport)
{
	if (gpio_is_valid(displayport->gpio_usb_dir) &&
			gpio_is_valid(displayport->gpio_sw_sel)) {
		displayport->dp_sw_sel = !gpio_get_value(displayport->gpio_usb_dir);
		gpio_direction_output(displayport->gpio_sw_sel, !(displayport->dp_sw_sel));
		displayport_info("Get direction from pdic %d\n", displayport->dp_sw_sel);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
		secdp_bigdata_save_item(BD_ORIENTATION,	displayport->dp_sw_sel ? "CC2" : "CC1");
#endif
	} else if (gpio_is_valid(displayport->gpio_usb_dir)) {
		/* redriver support case */
		displayport->dp_sw_sel = !gpio_get_value(displayport->gpio_usb_dir);
		displayport_info("Get Direction From pdic %d\n", !displayport->dp_sw_sel);
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
		secdp_bigdata_save_item(BD_ORIENTATION,	displayport->dp_sw_sel ? "CC2" : "CC1");
#endif
	}
}

static void displayport_check_adapter_type(struct displayport_device *displayport)
{
	displayport->dex_adapter_type = displayport->dex_max_resolution;

	if (displayport->ven_id != 0x04e8)
		return;

	switch (displayport->prod_id) {
	case 0xa029: /* PAD */
	case 0xa020: /* Station */
		displayport->dex_adapter_type = DEX_WQHD_SUPPORT;
		break;
	};
}

static int displayport_usb_typec_notification_proceed(struct displayport_device *displayport,
				PD_NOTI_TYPEDEF *usb_typec_info)
{
	displayport_dbg("%s: dump(0x%01x, 0x%01x, 0x%02x, 0x%04x, 0x%04x, 0x%04x)\n",
			__func__, usb_typec_info->src, usb_typec_info->dest, usb_typec_info->id,
			usb_typec_info->sub1, usb_typec_info->sub2, usb_typec_info->sub3);

	switch (usb_typec_info->id) {
	case PDIC_NOTIFY_ID_DP_CONNECT:
		switch (usb_typec_info->sub1) {
		case PDIC_NOTIFY_DETACH:
#if IS_ENABLED(CONFIG_SEC_DISPLAYPORT_LOGGER)
			dp_logger_set_max_count(100);
#endif
			displayport_info("PDIC_NOTIFY_ID_DP_CONNECT, %x\n", usb_typec_info->sub1);
			displayport->pdic_notify_dp_conf = PDIC_NOTIFY_DP_PIN_UNKNOWN;
			displayport->pdic_link_conf = false;
			displayport->pdic_hpd = false;
			displayport_hdcp22_notify_state(DP_DISCONNECT);
			displayport->dex_state = DEX_OFF;
			displayport->dex_ver[0] = 0;
			displayport->dex_ver[1] = 0;
#ifdef FEATURE_MANAGE_HMD_LIST
			displayport->is_hmd_dev = false;
#endif
			displayport_hpd_changed(0);
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
			displayport_aux_onoff(displayport, 0);
#endif
			switch_state = 0;
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
			secdp_bigdata_disconnection();
#endif
			break;
		case PDIC_NOTIFY_ATTACH:
#if IS_ENABLED(CONFIG_SEC_DISPLAYPORT_LOGGER)
			dp_logger_set_max_count(100);
#endif
			displayport_info("PDIC_NOTIFY_ID_DP_CONNECT, %x\n", usb_typec_info->sub1);
			displayport->ven_id = usb_typec_info->sub2;
			displayport->prod_id = usb_typec_info->sub3;
			displayport_check_adapter_type(displayport);
			displayport_info("VID:0x%llX, PID:0x%llX\n", displayport->ven_id, displayport->prod_id);
			displayport->poor_connect_count = 0;
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
			secdp_bigdata_connection();
			secdp_bigdata_save_item(BD_ADT_VID, displayport->ven_id);
			secdp_bigdata_save_item(BD_ADT_PID, displayport->prod_id);
#endif
			break;
		default:
			break;
		}

		break;

	case PDIC_NOTIFY_ID_DP_LINK_CONF:
		displayport_info("PDIC_NOTIFY_ID_DP_LINK_CONF %x\n",
				usb_typec_info->sub1);
		displayport_aux_sel(displayport);
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
		displayport_aux_onoff(displayport, 1);
#endif
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
		secdp_bigdata_save_item(BD_LINK_CONFIGURE, usb_typec_info->sub1 + 'A' - 1);
#endif
		switch (usb_typec_info->sub1) {
		case PDIC_NOTIFY_DP_PIN_UNKNOWN:
			displayport->pdic_notify_dp_conf = PDIC_NOTIFY_DP_PIN_UNKNOWN;
			break;
		case PDIC_NOTIFY_DP_PIN_A:
			displayport->pdic_notify_dp_conf = PDIC_NOTIFY_DP_PIN_A;
			break;
		case PDIC_NOTIFY_DP_PIN_B:
			displayport->dp_sw_sel = !displayport->dp_sw_sel;
			displayport->pdic_notify_dp_conf = PDIC_NOTIFY_DP_PIN_B;
			break;
		case PDIC_NOTIFY_DP_PIN_C:
			displayport->pdic_notify_dp_conf = PDIC_NOTIFY_DP_PIN_C;
			break;
		case PDIC_NOTIFY_DP_PIN_D:
			displayport->pdic_notify_dp_conf = PDIC_NOTIFY_DP_PIN_D;
			break;
		case PDIC_NOTIFY_DP_PIN_E:
			displayport->pdic_notify_dp_conf = PDIC_NOTIFY_DP_PIN_E;
			break;
		case PDIC_NOTIFY_DP_PIN_F:
			displayport->pdic_notify_dp_conf = PDIC_NOTIFY_DP_PIN_F;
			break;
		default:
			displayport->pdic_notify_dp_conf = PDIC_NOTIFY_DP_PIN_UNKNOWN;
			break;
		}

		if (displayport->pdic_notify_dp_conf) {
			displayport->pdic_link_conf = true;
			if (displayport->pdic_hpd)
				displayport_hpd_changed(1);
		}
		break;

	case PDIC_NOTIFY_ID_DP_HPD:
		displayport_info("PDIC_NOTIFY_ID_DP_HPD, %x, %x\n",
				usb_typec_info->sub1, usb_typec_info->sub2);
		switch (usb_typec_info->sub1) {
		case PDIC_NOTIFY_IRQ:
			break;
		case PDIC_NOTIFY_LOW:
			displayport->pdic_hpd = false;
			displayport->dex_state = DEX_OFF;
			displayport_hdcp22_notify_state(DP_DISCONNECT);
			displayport_hpd_changed(0);
			break;
		case PDIC_NOTIFY_HIGH:
			if (displayport->hpd_current_state &&
					usb_typec_info->sub2 == PDIC_NOTIFY_IRQ) {
				queue_delayed_work(displayport->dp_wq, &displayport->hpd_irq_work, 0);
				return 0;
			} else {
				displayport->pdic_hpd = true;
#if IS_ENABLED(CONFIG_SEC_DISPLAYPORT_LOGGER)
			dp_logger_set_max_count(300);
#endif
				if (displayport->pdic_link_conf)
					displayport_hpd_changed(1);
			}
			break;
		default:
			break;
		}

		break;

	default:
		break;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_USE_DISPLAYPORT_PDIC_EVENT_QUEUE)
#define DP_HAL_INIT_TIME	30/*sec*/
static void displayport_hal_ready_wait(struct displayport_device *displayport)
{
	time64_t wait_time = ktime_get_boottime_seconds();

	displayport_info("current time is %lld\n", wait_time);

	if (wait_time < DP_HAL_INIT_TIME) {
		wait_time = DP_HAL_INIT_TIME - wait_time;
		displayport_info("wait for %lld\n", wait_time);

		wait_event_interruptible_timeout(displayport->dp_ready_wait,
		    (displayport->dp_ready_wait_state == DP_READY_YES), msecs_to_jiffies(wait_time * 1000));
	}
	displayport->dp_ready_wait_state = DP_READY_YES;
}

static void displayport_pdic_event_proceed_work(struct work_struct *work)
{
	struct displayport_device *displayport = get_displayport_drvdata();
	struct pdic_event *data;

	if (displayport->dp_ready_wait_state != DP_READY_YES)
		displayport_hal_ready_wait(displayport);

	while (!list_empty(&displayport->list_pd)) {
		PD_NOTI_TYPEDEF pdic_evt;

		mutex_lock(&displayport->pdic_lock);
		data = list_first_entry(&displayport->list_pd, typeof(*data), list);
		memcpy(&pdic_evt, &data->event, sizeof(pdic_evt));
		list_del(&data->list);
		kfree(data);
		mutex_unlock(&displayport->pdic_lock);
		displayport_usb_typec_notification_proceed(displayport, &pdic_evt);
	}
}

static void displayport_pdic_queue_flush(struct displayport_device *displayport)
{
	struct pdic_event *data, *next;

	if (list_empty(&displayport->list_pd))
		return;

	displayport_info("delete hpd event from pdic queue\n");

	mutex_lock(&displayport->pdic_lock);
	list_for_each_entry_safe(data, next, &displayport->list_pd, list) {
		if (data->event.id == PDIC_NOTIFY_ID_DP_HPD) {
			list_del(&data->list);
			kfree(data);
		}
	}
	mutex_unlock(&displayport->pdic_lock);
}
#endif

static int usb_typec_displayport_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct displayport_device *displayport = get_displayport_drvdata();
	PD_NOTI_TYPEDEF usb_typec_info = *(PD_NOTI_TYPEDEF *)data;

	if (usb_typec_info.dest != PDIC_NOTIFY_DEV_DP)
		return 0;

#if IS_ENABLED(CONFIG_USE_DISPLAYPORT_PDIC_EVENT_QUEUE)
	{
		struct pdic_event *pd_data;

		displayport_dbg("PDIC action(%ld) dump(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n",
			action, usb_typec_info.src, usb_typec_info.dest, usb_typec_info.id,
			usb_typec_info.sub1, usb_typec_info.sub2, usb_typec_info.sub3);

		if (usb_typec_info.id == PDIC_NOTIFY_ID_DP_CONNECT)
			displayport->pdic_cable_state = usb_typec_info.sub1;

		pd_data = kzalloc(sizeof(struct pdic_event), GFP_KERNEL);
		if (!pd_data) {
			displayport_err("kzalloc error for pdic event\n");
			return 0;
		}

		switch (usb_typec_info.id) {
		case PDIC_NOTIFY_ID_DP_CONNECT:
			displayport_info("queued CONNECT: %x\n", usb_typec_info.sub1);
			break;
		case PDIC_NOTIFY_ID_DP_LINK_CONF:
			displayport_info("queued LINK_CONF: %x\n", usb_typec_info.sub1);
			break;
		case PDIC_NOTIFY_ID_DP_HPD:
			displayport_info("queued HPD: %x, %x\n", usb_typec_info.sub1, usb_typec_info.sub2);
			break;
		}

		/* if disconnect or hpd low event come, then flush queue */
		if (((usb_typec_info.id == PDIC_NOTIFY_ID_DP_CONNECT &&
				  usb_typec_info.sub1 == PDIC_NOTIFY_DETACH) ||
				 (usb_typec_info.id == PDIC_NOTIFY_ID_DP_HPD &&
				  usb_typec_info.sub1 == PDIC_NOTIFY_LOW))) {
			displayport_pdic_queue_flush(displayport);
		};

		memcpy(&pd_data->event, &usb_typec_info, sizeof(usb_typec_info));

		mutex_lock(&displayport->pdic_lock);
		list_add_tail(&pd_data->list, &displayport->list_pd);
		mutex_unlock(&displayport->pdic_lock);

		queue_delayed_work(displayport->dp_wq, &displayport->pdic_event_proceed_work, 0);
	}
#else
	displayport_dbg("pdic action(%ld) dump(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n",
			action, usb_typec_info.src, usb_typec_info.dest, usb_typec_info.id,
			usb_typec_info.sub1, usb_typec_info.sub2, usb_typec_info.sub3);

	displayport_usb_typec_notification_proceed(displayport, &usb_typec_info);
#endif

	return 0;
}

static void displayport_notifier_register_work(struct work_struct *work)
{
	struct displayport_device *displayport = get_displayport_drvdata();

	if (!displayport->notifier_registered) {
		displayport->notifier_registered = 1;
		displayport_info("notifier registered\n");
		manager_notifier_register(&displayport->dp_typec_nb,
			usb_typec_displayport_notification, MANAGER_NOTIFY_PDIC_DP);
	}
}
#endif

#ifdef DISPLAYPORT_TEST
static ssize_t displayport_link_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	/*struct displayport_device *displayport = get_displayport_drvdata();*/

	return snprintf(buf, PAGE_SIZE, "%s\n", __func__);
}

static ssize_t displayport_link_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	int mode = 0;
	struct displayport_device *displayport = get_displayport_drvdata();
	u32 sst_id = displayport_get_sst_id_with_decon_id(DEFAULT_DECON_ID);

	if (kstrtoint(buf, 10, &mode))
		return size;
	pr_info("%s mode=%d\n", __func__, mode);

	if (mode == 0) {
		displayport_hpd_changed(0);
		displayport_link_sink_status_read();
	} else if (mode == 8) {
		queue_delayed_work(displayport->dp_wq, &displayport->hpd_irq_work, 0);
	} else if (mode == 9) {
		displayport_hpd_changed(1);
	} else {
		u8 link_rate = mode/10;
		u8 lane_cnt = mode%10;

		if ((link_rate >= 1 && link_rate <= 3) &&
				(lane_cnt == 1 || lane_cnt == 2 || lane_cnt == 4)) {
			if (link_rate == 4)
				link_rate = LINK_RATE_8_1Gbps;
			else if (link_rate == 3)
				link_rate = LINK_RATE_5_4Gbps;
			else if (link_rate == 2)
				link_rate = LINK_RATE_2_7Gbps;
			else
				link_rate = LINK_RATE_1_62Gbps;

			pr_info("%s: %02x %02x\n", __func__, link_rate, lane_cnt);
			displayport_reg_init(); /* for AUX ch read/write. */

			displayport_link_status_read(sst_id);

			g_displayport_debug_param.param_used = 1;
			g_displayport_debug_param.link_rate = link_rate;
			g_displayport_debug_param.lane_cnt = lane_cnt;

			displayport_full_link_training(sst_id);

			g_displayport_debug_param.param_used = 0;

			displayport_set_extcon_state(sst_id, displayport, 1);
			displayport_wait_state_change(sst_id, displayport,
					1000, DISPLAYPORT_STATE_ON);
		} else {
			pr_err("%s: Not support command[%d]\n",
					__func__, mode);
		}
	}

	return size;
}

static CLASS_ATTR(link, 0664, displayport_link_show, displayport_link_store);

static ssize_t displayport_test_bpc_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	struct displayport_device *displayport = get_displayport_drvdata();
	u32 sst_id = displayport_get_sst_id_with_decon_id(DEFAULT_DECON_ID);

	return snprintf(buf, PAGE_SIZE, "displayport bpc %d\n",
			(displayport->sst[sst_id]->bpc == BPC_6)?6:8);
}
static ssize_t displayport_test_bpc_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	int mode = 0;
	struct displayport_device *displayport = get_displayport_drvdata();
	u32 sst_id = displayport_get_sst_id_with_decon_id(DEFAULT_DECON_ID);

	if (kstrtoint(buf, 10, &mode))
		return size;
	pr_info("%s mode=%d\n", __func__, mode);

	switch (mode) {
	case 6:
		displayport->sst[sst_id]->bpc = BPC_6;
		break;
	case 8:
		displayport->sst[sst_id]->bpc = BPC_8;
		break;
	default:
		pr_err("%s: Not support command[%d]\n",
				__func__, mode);
		break;
	}

	return size;
}

static CLASS_ATTR(bpc, 0664, displayport_test_bpc_show, displayport_test_bpc_store);

static ssize_t displayport_test_range_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	struct displayport_device *displayport = get_displayport_drvdata();
	u32 sst_id = displayport_get_sst_id_with_decon_id(DEFAULT_DECON_ID);

	return sprintf(buf, "displayport range %s\n",
		(displayport->sst[sst_id]->dyn_range == VESA_RANGE)?"VESA_RANGE":"CEA_RANGE");
}
static ssize_t displayport_test_range_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	int mode = 0;
	struct displayport_device *displayport = get_displayport_drvdata();
	u32 sst_id = displayport_get_sst_id_with_decon_id(DEFAULT_DECON_ID);

	if (kstrtoint(buf, 10, &mode))
		return size;
	pr_info("%s mode=%d\n", __func__, mode);

	switch (mode) {
	case 0:
		displayport->sst[sst_id]->dyn_range = VESA_RANGE;
		break;
	case 1:
		displayport->sst[sst_id]->dyn_range = CEA_RANGE;
		break;
	default:
		pr_err("%s: Not support command[%d]\n",
				__func__, mode);
		break;
	}

	return size;
}

static CLASS_ATTR(range, 0664, displayport_test_range_show, displayport_test_range_store);

static ssize_t displayport_test_edid_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	struct v4l2_dv_timings edid_preset;
	int i;
	struct displayport_device *displayport = get_displayport_drvdata();
	u32 sst_id = displayport_get_sst_id_with_decon_id(DEFAULT_DECON_ID);

	edid_preset = edid_preferred_preset();

	i = displayport_timing2conf(&edid_preset);
	if (i < 0) {
		i = displayport->sst[sst_id]->cur_video;
		pr_err("displayport timings not supported\n");
	}

	return snprintf(buf, PAGE_SIZE, "displayport preferred_preset = %d %d %d\n",
			videoformat_parameters[i].active_pixel,
			videoformat_parameters[i].active_line,
			videoformat_parameters[i].fps);
}

static ssize_t displayport_test_edid_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	struct v4l2_dv_timings edid_preset;
	int i;
	int mode = 0;
	struct displayport_device *displayport = get_displayport_drvdata();
	u32 sst_id = displayport_get_sst_id_with_decon_id(DEFAULT_DECON_ID);

	if (kstrtoint(buf, 10, &mode))
		return size;
	pr_info("%s mode=%d\n", __func__, mode);

	edid_set_preferred_preset(mode);
	edid_preset = edid_preferred_preset();
	i = displayport_timing2conf(&edid_preset);
	if (i < 0) {
		i = displayport->sst[sst_id]->cur_video;
		pr_err("displayport timings not supported\n");
	}

	pr_info("displayport preferred_preset = %d %d %d\n",
			videoformat_parameters[i].active_pixel,
			videoformat_parameters[i].active_line,
			videoformat_parameters[i].fps);

	return size;
}
static CLASS_ATTR(edid, 0664, displayport_test_edid_show, displayport_test_edid_store);

static ssize_t displayport_test_bist_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	struct displayport_device *displayport = get_displayport_drvdata();
	u32 sst_id = displayport_get_sst_id_with_decon_id(DEFAULT_DECON_ID);

	return snprintf(buf, PAGE_SIZE, "displayport bist used %d type %d\n",
			displayport->sst[sst_id]->bist_used,
			displayport->sst[sst_id]->bist_type);
}

static ssize_t displayport_test_bist_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	int mode = 0;
	struct displayport_audio_config_data audio_config_data;
	struct displayport_device *displayport = get_displayport_drvdata();
	u32 sst_id = displayport_get_sst_id_with_decon_id(DEFAULT_DECON_ID);

	if (kstrtoint(buf, 10, &mode))
		return size;
	pr_info("%s mode=%d\n", __func__, mode);

	switch (mode) {
	case 0:
		displayport->sst[sst_id]->bist_used = 0;
		displayport->sst[sst_id]->bist_type = COLOR_BAR;
		break;
	case 1:
		displayport->sst[sst_id]->bist_used = 1;
		displayport->sst[sst_id]->bist_type = COLOR_BAR;
		break;
	case 2:
		displayport->sst[sst_id]->bist_used = 1;
		displayport->sst[sst_id]->bist_type = WGB_BAR;
		break;
	case 3:
		displayport->sst[sst_id]->bist_used = 1;
		displayport->sst[sst_id]->bist_type = MW_BAR;
		break;
	case 4:
		displayport->sst[sst_id]->bist_used = 1;
		displayport->sst[sst_id]->bist_type = CTS_COLOR_RAMP;
		break;
	case 5:
		displayport->sst[sst_id]->bist_used = 1;
		displayport->sst[sst_id]->bist_type = CTS_BLACK_WHITE;
		break;
	case 6:
		displayport->sst[sst_id]->bist_used = 1;
		displayport->sst[sst_id]->bist_type = CTS_COLOR_SQUARE_VESA;
		break;
	case 7:
		displayport->sst[sst_id]->bist_used = 1;
		displayport->sst[sst_id]->bist_type = CTS_COLOR_SQUARE_CEA;
		break;
	case 11:
	case 12:
		audio_config_data.audio_enable = 1;
		audio_config_data.audio_fs = FS_192KHZ;
		audio_config_data.audio_channel_cnt = mode-10;
		audio_config_data.audio_bit = 0;
		audio_config_data.audio_packed_mode = 0;
		audio_config_data.audio_word_length = 0;
		displayport_audio_bist_config(SST1, audio_config_data);
		break;
	default:
		pr_err("%s: Not support command[%d]\n",
				__func__, mode);
		break;
	}

	return size;
}
static CLASS_ATTR(bist, 0664, displayport_test_bist_show, displayport_test_bist_store);

static ssize_t displayport_test_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	struct displayport_device *displayport = get_displayport_drvdata();

	return snprintf(buf, PAGE_SIZE, "displayport gpio oe %d, sel %d, direction %d\n",
			gpio_get_value(displayport->gpio_sw_oe),
			gpio_get_value(displayport->gpio_sw_sel),
			gpio_get_value(displayport->gpio_usb_dir));
}
static ssize_t displayport_test_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	/*	struct displayport_device *displayport = get_displayport_drvdata(); */

	return size;
}

static CLASS_ATTR(dp, 0664, displayport_test_show, displayport_test_store);

static ssize_t displayport_forced_resolution_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int ret = 0;
	int i;

	for (i = 0; i < supported_videos_pre_cnt; i++) {
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%c %2d : %s\n",
				forced_resolution == i ? '*':' ', i,
				supported_videos[i].name);
	}

	return ret;
}

static ssize_t displayport_forced_resolution_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	int val[4] = {0,};

	if (strnchr(buf, size, '-')) {
		pr_err("%s range option not allowed\n", __func__);
		return -EINVAL;
	}

	get_options(buf, 4, val);

	reduced_resolution = 0;

	if (val[1] < 0 || val[1] >= supported_videos_pre_cnt || val[0] < 1)
		forced_resolution = -1;
	else {
		struct displayport_device *displayport = get_displayport_drvdata();
		int hpd_stat = displayport->hpd_current_state;

		forced_resolution = val[1];
		if (hpd_stat) {
			displayport_hpd_changed(0);
			msleep(100);
			displayport_hpd_changed(1);
		}
	}

	return size;
}
static CLASS_ATTR(forced_resolution, 0664, displayport_forced_resolution_show,
		displayport_forced_resolution_store);

static ssize_t displayport_reduced_resolution_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int ret = 0;

	ret = scnprintf(buf, PAGE_SIZE, "%llu\n", reduced_resolution);

	return ret;
}

static ssize_t displayport_reduced_resolution_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	int val[4] = {0,};

	if (strnchr(buf, size, '-')) {
		pr_err("%s range option not allowed\n", __func__);
		return -EINVAL;
	}

	get_options(buf, 4, val);

	forced_resolution = -1;

	if (val[1] < 0 || val[1] >= supported_videos_pre_cnt || val[0] < 1)
		reduced_resolution = 0;
	else {
		switch (val[1]) {
		case 1:
			reduced_resolution = PIXELCLK_2160P30HZ;
			break;
		case 2:
			reduced_resolution = PIXELCLK_1080P60HZ;
			break;
		case 3:
			reduced_resolution = PIXELCLK_1080P30HZ;
			break;
		default:
			reduced_resolution = 0;
		};
	}

	return size;
}
static CLASS_ATTR(reduced_resolution, 0664, displayport_reduced_resolution_show,
		displayport_reduced_resolution_store);
#endif

#ifdef CONFIG_DISPLAYPORT_ENG
static ssize_t phy_tune_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	char str[512] = {0, };
	char *tmp = str;
	int i = 0;
	int *phy_tune_param = (int *)phy_tune_parameters;
	int size = 0;

	for (i = 0; i < 80; i++) {
		if (i % 20 == 0)
			*tmp++ = '\n';
		if (i % 5 == 0)
			*tmp++ = '/';

		size = snprintf(tmp, 8, "%5x,", *phy_tune_param);
		tmp = tmp + size;
		phy_tune_param++;
	}
	*tmp++ = '\n';

	return snprintf(buf, sizeof(str), str);
}

static ssize_t phy_tune_store(struct class *dev,
			struct class_attribute *attr,
			const char *buf, size_t size)
{
	int val[24] = {0,};
	int ind = 0;
	int i = 0;
	int *phy_tune_param;

	if (strnchr(buf, size, '-')) {
		pr_err("%s range option not allowed\n", __func__);
		return -EINVAL;
	}

	get_options(buf, 22, val);
	if (val[0] != 21 || val[1] > 3 || val[1] < 0) {
		displayport_info("phy tune: invalid input %d %d\n", val[0], val[1]);
		return size;
	}

	ind = val[1];
	phy_tune_param = (int *)phy_tune_parameters[ind];
	for (i = 2; i < 22; i++)
		*phy_tune_param++ = val[i];

	return size;
}
static CLASS_ATTR_RW(phy_tune);

extern struct fb_audio *edid_get_test_audio_info(void);
static ssize_t audio_test_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int bit = 0;
	int sample = 0;
	struct fb_audio *aud_info = edid_get_test_audio_info();
	int tmp_ch = aud_info->channel_count;
	int channel = 0;

	while (tmp_ch) {
		channel++;
		tmp_ch >>= 1;
	}

	switch (aud_info->bit_rates) {
	case FB_AUDIO_16BIT:
		bit = 16;
		break;
	case FB_AUDIO_20BIT:
		bit = 20;
		break;
	case FB_AUDIO_24BIT:
		bit = 24;
	}

	switch (aud_info->sample_rates) {
	case FB_AUDIO_32KHZ:
		sample = 32;
		break;
	case FB_AUDIO_44KHZ:
		sample = 44;
		break;
	case FB_AUDIO_48KHZ:
		sample = 48;
		break;
	case FB_AUDIO_88KHZ:
		sample = 88;
		break;
	case FB_AUDIO_96KHZ:
		sample = 96;
		break;
	case FB_AUDIO_176KHZ:
		sample = 176;
		break;
	case FB_AUDIO_192KHZ:
		sample = 192;
		break;
	}

	return sprintf(buf, "channel: %d, bit rate: %d, sample rate: %d\n",
				channel, bit, sample);
}

static ssize_t audio_test_store(struct class *dev,
			struct class_attribute *attr,
			const char *buf, size_t size)
{
	int val[6] = {0,};
	struct fb_audio *aud_info = edid_get_test_audio_info();

	if (strnchr(buf, size, '-')) {
		pr_err("%s range option not allowed\n", __func__);
		return -EINVAL;
	}

	get_options(buf, 4, val);
	if (val[0] != 3) {
		displayport_info("invalid input. set default\n");
		aud_info->channel_count = 0;
		aud_info->bit_rates = 0;
		aud_info->sample_rates = 0;
		return size;
	}

	if (val[1] > 0 && val[1] <= 8)
		aud_info->channel_count = (1 << (val[1] - 1));
	else
		aud_info->channel_count = 2;

	switch (val[2]) {
	case 16:
		aud_info->bit_rates = FB_AUDIO_16BIT;
		break;
	case 20:
		aud_info->bit_rates = FB_AUDIO_20BIT;
		break;
	case 24:
		aud_info->bit_rates = FB_AUDIO_24BIT;
		break;
	default:
		aud_info->bit_rates = FB_AUDIO_16BIT;
	}

	switch (val[3]) {
	case 32:
		aud_info->sample_rates = FB_AUDIO_32KHZ;
		break;
	case 44:
		aud_info->sample_rates = FB_AUDIO_44KHZ;
		break;
	case 48:
		aud_info->sample_rates = FB_AUDIO_48KHZ;
		break;
	case 88:
		aud_info->sample_rates = FB_AUDIO_88KHZ;
		break;
	case 96:
		aud_info->sample_rates = FB_AUDIO_96KHZ;
		break;
	case 176:
		aud_info->sample_rates = FB_AUDIO_176KHZ;
		break;
	case 192:
		aud_info->sample_rates = FB_AUDIO_192KHZ;
		break;
	default:
		aud_info->sample_rates = FB_AUDIO_48KHZ;
	}

	return size;
}
static CLASS_ATTR_RW(audio_test);

#define TEST_BUF_SIZE	512
static u8 edid_test_buf[TEST_BUF_SIZE + 1]; /* 1st index is block count */
static ssize_t edid_test_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	ssize_t size;
	int i;

	if (edid_test_buf[0] < 1 || edid_test_buf[0] > 4)
		return sprintf(buf, "invalid size test edid(%d)\n", edid_test_buf[0]);

	size = sprintf(buf, "edid size: %d\n", edid_test_buf[0]);
	for (i = 1; i <= edid_test_buf[0] * 128; i++) {
		size += sprintf(buf + size, " %02X", edid_test_buf[i]);
		if (i % 16 == 0)
			size += sprintf(buf + size, "\n");
		else if (i % 8 == 0)
			size += sprintf(buf + size, " ");
	}

	return size;
}

static ssize_t edid_test_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	int i;
	int edid_idx = 1, hex_cnt = 0, buf_idx = 0;
	u8 hex = 0;
	u8 temp;
	int max_size = (TEST_BUF_SIZE * 6); /* including comma, space and prefix like ', 0xFF' */

	edid_test_buf[0] = 0;

	if (displayport_log_level >= 7)
		pr_cont("EDID test: ");
	for (i = 0; i < size && i < max_size; i++) {
		temp = *(buf + buf_idx++);
		/* value is separated by comma, space or line feed*/
		if (temp == ',' || temp == ' ' || temp == '\x0A') {
			if (hex_cnt != 0) {
				if (displayport_log_level >= 7) {
					pr_cont("%02X ", hex);
					if (edid_idx % 16 == 0) {
						pr_info("\n");
						pr_cont("EDID test: ");
					}
				}
				edid_test_buf[edid_idx++] = hex;
			}
			hex = 0;
			hex_cnt = 0;
		} else if (hex_cnt == 0 && temp == '0') {
			hex_cnt++;
			continue;
		} else if (temp == 'x' || temp == 'X') {
			hex_cnt = 0;
			hex = 0;
			continue;
		} else if (!temp || temp == '\0') { /* EOL */
			if (displayport_log_level >= 7)
				pr_cont("%02X ", hex);
			displayport_info("parse end. edid cnt: %d\n", edid_idx);
			break;
		} else if (temp >= '0' && temp <= '9') {
			hex = (hex << 4) + (temp - '0');
			hex_cnt++;
		} else if (temp >= 'a' && temp <= 'f') {
			hex = (hex << 4) + (temp - 'a') + 0xa;
			hex_cnt++;
		} else if (temp >= 'A' && temp <= 'F') {
			hex = (hex << 4) + (temp - 'A') + 0xa;
			hex_cnt++;
		} else {
			displayport_info("invalid value %c, %d\n", temp, hex_cnt);
			return size;
		}

		if (hex_cnt > 2 || edid_idx > TEST_BUF_SIZE + 1) {
			displayport_info("wrong input. %d, %d, [%c]\n", hex_cnt, edid_idx, temp);
			return size;
		}
	}

	if (hex_cnt > 0)
		edid_test_buf[edid_idx] = hex;

	if (edid_idx != 1 && edid_idx % 128 == 1)
		edid_test_buf[0] = edid_idx / 128;

	displayport_info("edid size = %d\n", edid_idx);

	return size;
}
static CLASS_ATTR_RW(edid_test);

void displayport_pm_test(int pwr)
{
	u8 val[2] = {SET_POWER_NORMAL, SET_POWER_DOWN};
	u8 val1 = 0;
	struct displayport_device *displayport = get_displayport_drvdata();
	u32 sst_id = displayport_get_sst_id_with_decon_id(DEFAULT_DECON_ID);

	if (!displayport->hpd_current_state)
		return;

	displayport_info("set power state for CTS(%d)", pwr);
	if (pwr) {
		displayport_reg_dpcd_write(DPCD_ADD_SET_POWER, 1, &val[0]);
		displayport_link_training(sst_id);
	} else
		displayport_reg_dpcd_write(DPCD_ADD_SET_POWER, 1, &val[1]);

	displayport_reg_dpcd_read(DPCD_ADD_SET_POWER, 1, &val1);
	displayport_info("set power state for DPCD_ADD_SET_POWER(%d)", val1);
}

static ssize_t dp_test_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	struct displayport_device *displayport = get_displayport_drvdata();
	int size;
#ifdef FEATURE_MANAGE_HMD_LIST
	int i;
#endif

	size = snprintf(buf, PAGE_SIZE, "0: HPD test\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "1: uevent test\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "2: CTS power management test\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "3: set lane count, link rate\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "4: hdcp restart\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "5: link training test\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "6: unplug work test\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "7: MST test\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "8: audio bist mode(on,ch,bit,fs)\n");
	size += snprintf(buf + size, PAGE_SIZE - size, "9: send poor connect event\n");

	if (gpio_is_valid(displayport->gpio_sw_oe) && gpio_is_valid(displayport->gpio_sw_oe))
		size += snprintf(buf + size, PAGE_SIZE - size, "\n# gpio oe %d, sel %d, direction %d\n",
				gpio_get_value(displayport->gpio_sw_oe),
				gpio_get_value(displayport->gpio_sw_sel),
				gpio_get_value(displayport->gpio_usb_dir));
	else
		size += snprintf(buf + size, PAGE_SIZE - size, "\n# gpio direction %d\n",
			gpio_get_value(displayport->gpio_usb_dir));

#ifdef FEATURE_MANAGE_HMD_LIST
	for (i = 0; i < MAX_NUM_HMD; i++) {
		if (strlen(displayport->hmd_list[i].monitor_name) > 1 ||
					displayport->hmd_list[i].ven_id != 0 ||
					displayport->hmd_list[i].prod_id != 0) {
			displayport_info("HMD%02d: %s, 0x%04x, 0x%04x\n", i,
					displayport->hmd_list[i].monitor_name,
					displayport->hmd_list[i].ven_id,
					displayport->hmd_list[i].prod_id);
			size += snprintf(buf + size, PAGE_SIZE - size,
					"HMD%02d: %s, 0x%04x, 0x%04x\n", i,
						displayport->hmd_list[i].monitor_name,
						displayport->hmd_list[i].ven_id,
						displayport->hmd_list[i].prod_id);
		}
	}
#endif
	return size;
}

static ssize_t dp_test_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	struct displayport_device *displayport = get_displayport_drvdata();
	u32 sst_id = displayport_get_sst_id_with_decon_id(DEFAULT_DECON_ID);
	int val[8] = {0,};
	struct displayport_audio_config_data audio_config_data;

	if (strnchr(buf, size, '-')) {
		pr_err("%s range option not allowed\n", __func__);
		return -EINVAL;
	}

	get_options(buf, 6, val);

	switch (val[1]) {
	case 0:
		if (val[2] == 0 || val[2] == 1)
			displayport_hpd_changed(val[2]);
		break;
	case 1:
		if (val[2] == 0) {
			displayport_set_extcon_state(sst_id, displayport, 0);
		} else if (val[2] == 1) {
			edid_update(sst_id, displayport);
			displayport_set_extcon_state(sst_id, displayport, 1);
		}
		break;
	case 2:
		if (val[2] == 0 || val[2] == 1)
			displayport_pm_test(val[2]);
		break;
	case 3:/*lane count, link rate*/
		displayport_info("test lane: %d, link rate: 0x%x\n", val[2], val[3]);
		if ((val[2] == 1 || val[2] == 2 || val[2] == 4) &&
			(val[3] == 6 || val[3] == 0xa || val[3] == 0x14 || val[3] == 0x1e)) {
			g_displayport_debug_param.lane_cnt = val[2];
			g_displayport_debug_param.link_rate = val[3];
			g_displayport_debug_param.param_used = 1;
		} else {
			g_displayport_debug_param.lane_cnt = 0;
			g_displayport_debug_param.link_rate = 0;
			g_displayport_debug_param.param_used = 0;
		}
		break;
	case 4:
		if (displayport->hdcp_ver == HDCP_VERSION_2_2) {
			hdcp_dplink_set_reauth();
			displayport_hdcp22_enable(0);
			queue_delayed_work(displayport->hdcp2_wq,
					&displayport->hdcp22_work, msecs_to_jiffies(0));
		} else if (displayport->hdcp_ver == HDCP_VERSION_1_3) {
			hdcp13_info.auth_state = HDCP13_STATE_FAIL;
			queue_delayed_work(displayport->dp_wq,
					&displayport->hdcp13_work, msecs_to_jiffies(0));
		}
		break;
	case 5:
		displayport_link_training(sst_id);
		break;
	case 6:
		queue_delayed_work(displayport->dp_wq,
				&displayport->hpd_unplug_work, 0);
		break;
	case 7:
		if (val[2])
			displayport->mst_mode = 1;
		else
			displayport->mst_mode = 0;
		break;
	case 8:
		audio_config_data.audio_enable = val[2];
		audio_config_data.audio_fs = FS_48KHZ;
		audio_config_data.audio_channel_cnt = 2;
		audio_config_data.audio_bit = AUDIO_16_BIT;
		audio_config_data.audio_packed_mode = 0;
		audio_config_data.audio_word_length = 0;
		switch (val[3]) {
		case 2:
			audio_config_data.audio_channel_cnt = 2;
			break;
		case 6:
			audio_config_data.audio_channel_cnt = 6;
			break;
		case 8:
			audio_config_data.audio_channel_cnt = 8;
			break;
		default:
			break;
		}

		switch (val[4]) {
		case 16:
			audio_config_data.audio_bit = AUDIO_16_BIT;
			break;
		case 24:
			audio_config_data.audio_bit = AUDIO_24_BIT;
			break;
		default:
			break;
		}

		switch (val[5]) {
		case 48:
			audio_config_data.audio_fs = FS_48KHZ;
			break;
		case 96:
			audio_config_data.audio_fs = FS_96KHZ;
			break;
		case 192:
			audio_config_data.audio_fs = FS_192KHZ;
			break;
		default:
			break;
		}
		displayport_audio_bist_config(sst_id, audio_config_data);
		break;
	case 9:
		displayport_set_switch_poor_connect();
		break;
	default:
		break;
	};

	return size;
}
static CLASS_ATTR_RW(dp_test);
#endif

#ifdef FEATURE_MANAGE_HMD_LIST
/*
 * assume that 1 HMD device has name(14),vid(4),pid(4) each, then
 * max 32 HMD devices(name,vid,pid) need 806 bytes including TAG, NUM, comba
 */
#define MAX_DEX_STORE_LEN	1024
static int displayport_update_hmd_list(struct displayport_device *displayport, const char *buf, size_t size)
{
	int ret = 0;
	char str[MAX_DEX_STORE_LEN] = {0,};
	char *p, *tok;
	u32 num_hmd = 0;
	int j = 0;
	u32 val;

	mutex_lock(&displayport->hmd_lock);

	memcpy(str, buf, size);
	p = str;

	tok = strsep(&p, ",");
	if (strncmp(DEX_TAG_HMD, tok, strlen(DEX_TAG_HMD))) {
		displayport_dbg("not HMD tag %s\n", tok);
		ret = -EINVAL;
		goto not_tag_exit;
	}

	displayport_info("%s\n", __func__);

	tok = strsep(&p, ",");
	if (tok == NULL || *tok == 0xa/*LF*/) {
		ret = -EPERM;
		goto exit;
	}
	ret = kstrtouint(tok, 10, &num_hmd);
	if (ret || num_hmd > MAX_NUM_HMD) {
		displayport_err("invalid list num %d\n", num_hmd);
		num_hmd = 0;
		ret = -EPERM;
		goto exit;
	}

	for (j = 0; j < num_hmd; j++) {
		/* monitor name */
		tok = strsep(&p, ",");
		if (tok == NULL || *tok == 0xa/*LF*/)
			break;
		strlcpy(displayport->hmd_list[j].monitor_name, tok, MON_NAME_LEN);

		/* VID */
		tok  = strsep(&p, ",");
		if (tok == NULL || *tok == 0xa/*LF*/)
			break;
		if (kstrtouint(tok, 16, &val)) {
			ret = -EPERM;
			break;
		}

		displayport->hmd_list[j].ven_id = val;

		/* PID */
		tok  = strsep(&p, ",");
		if (tok == NULL || *tok == 0xa/*LF*/)
			break;
		if (kstrtouint(tok, 16, &val)) {
			ret = -EPERM;
			break;
		}
		displayport->hmd_list[j].prod_id = val;

		displayport_info("HMD%02d: %s, 0x%04x, 0x%04x\n", j,
				displayport->hmd_list[j].monitor_name,
				displayport->hmd_list[j].ven_id,
				displayport->hmd_list[j].prod_id);
	}

exit:
	/* clear rest */
	for (; j < MAX_NUM_HMD; j++) {
		displayport->hmd_list[j].monitor_name[0] = '\0';
		displayport->hmd_list[j].ven_id = 0;
		displayport->hmd_list[j].prod_id = 0;
	}

not_tag_exit:
	mutex_unlock(&displayport->hmd_lock);

	return ret;
}
#endif

#ifdef FEATURE_DEX_ADAPTER_TWEAK
#define DEX_ADATER_TWEAK_LEN	32
#define DEX_TAG_ADAPTER_TWEAK "SkipAdapterCheck"
static int displayport_dex_adapter_tweak(struct displayport_device *displayport, const char *buf, size_t size)
{
	char str[DEX_ADATER_TWEAK_LEN] = {0,};
	char *p, *tok;
	static enum dex_support_type def_value;

	if (size >= DEX_ADATER_TWEAK_LEN)
		return -EINVAL;

	memcpy(str, buf, size);
	p = str;

	tok = strsep(&p, ",");
	if (strncmp(DEX_TAG_ADAPTER_TWEAK, tok, strlen(DEX_TAG_ADAPTER_TWEAK))) {
		return -EINVAL;
	}

	tok = strsep(&p, ",");
	if (tok == NULL || *tok == 0xa/*LF*/) {
		displayport_info("Dex adapter tweak - Invalid value\n");
		return 0;
	}

	if (def_value == 0)
		def_value = displayport->dex_max_resolution;

	switch (*tok) {
	case '0':
		displayport->dex_max_resolution = def_value;
		break;
	case '1':
		displayport->dex_max_resolution = DEX_WQHD_SUPPORT;
		break;
	}
	displayport_info("%s(%c)\n", __func__, *tok);

	return 0;
}
#endif

static ssize_t dex_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct displayport_device *displayport = get_displayport_drvdata();
	int ret = 0;

	displayport_info("dex state:%d, pdic state:%d\n",
			displayport->dex_state, displayport->pdic_hpd);

	if (!displayport->pdic_hpd)
		displayport->dex_state = DEX_OFF;

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", displayport->dex_state);

	if (displayport->dex_state == DEX_RECONNECTING)
		displayport->dex_state = DEX_ON;

	return ret;
}

static ssize_t dex_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	struct displayport_device *displayport = get_displayport_drvdata();
	int val = 0;
	u32 dex_run = 0;
	int need_reconnect = 0;
	u32 sst_id = displayport_get_sst_id_with_decon_id(DEFAULT_DECON_ID);
	int cur_video = displayport->sst[sst_id]->cur_video;
	int best_video = displayport->sst[sst_id]->best_video;
	int best_dex_video = displayport->dex_video_pick;
	int mst_support = displayport->mst_cap;
#ifdef FEATURE_MANAGE_HMD_LIST
	int ret;

	if (size >= MAX_DEX_STORE_LEN) {
		displayport_err("invalid input size %lu\n", size);
		return -EINVAL;
	}

	ret = displayport_update_hmd_list(displayport, buf, size);
	if (ret == 0) /* HMD list update success */
		return size;
	else if (ret != -EINVAL) /* try to update HMD list but error*/
		return ret;
#endif

#ifdef FEATURE_DEX_ADAPTER_TWEAK
	if (!displayport_dex_adapter_tweak(displayport, buf, size))
		return size;
#endif

	if (kstrtouint(buf, 10, &val)) {
		displayport_err("invalid input %s\n", buf);
		return -EINVAL;
	}

	if (val != 0x00 && val != 0x01 && val != 0x10 && val != 0x11) {
		displayport_err("invalid input 0x%X\n", val);
		return -EINVAL;
	}

	displayport->dex_setting = (val & 0xF0) >> 4;
	dex_run = (val & 0x0F);

	displayport_info("dex state:%d, setting:%d, run:%d, hpd:%d\n",
			displayport->dex_state, displayport->dex_setting,
			dex_run, displayport->hpd_current_state);

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	if (displayport->dp_ready_wait_state != DP_READY_YES) {
		displayport_info("dp_ready_wait wakeup\n");
		displayport->dp_ready_wait_state = DP_READY_YES;
		wake_up_interruptible(&displayport->dp_ready_wait);
		goto dex_exit;
	}
#endif
#ifdef FEATURE_MANAGE_HMD_LIST
	if (displayport->is_hmd_dev) {
		displayport_info("HMD dev\n");
		goto dex_exit;
	}
#endif
	/* if current state is not new state, then reconnect */
	if (displayport->dex_state != dex_run &&
				displayport->dex_state != DEX_RECONNECTING &&
				displayport->pdic_hpd != 0) {
		need_reconnect = 1;

		if (mst_support) {
			/* TODO: need to consider about Using MST on dex mode only */
			int cur_sst1_video = displayport->sst[SST1]->cur_video;
			int cur_sst2_video = displayport->sst[SST2]->cur_video;

			if (displayport->dex_setting) {
				if (supported_videos[cur_sst1_video].dex_support != DEX_NOT_SUPPORT &&
					supported_videos[cur_sst2_video].dex_support != DEX_NOT_SUPPORT) {
					displayport_info("current video support dex %d, %d\n",
								cur_sst1_video, cur_sst2_video);
					need_reconnect = 0;
				}
			} else {
				int sst1_best = displayport->sst[SST1]->best_video;
				int sst2_best = displayport->sst[SST2]->best_video;

				if (cur_sst1_video == sst1_best && cur_sst2_video == sst2_best) {
					displayport_info("current video is best %d, %d\n",
								cur_sst1_video, cur_sst2_video);
					need_reconnect = 0;
				}
			}
		} else {
			displayport_dbg("current: %d, best: %d, best_dex: %d\n",
						cur_video, best_video, best_dex_video);
			if (displayport->dex_setting) {
				/* if current resolution is dex supported, then do not reconnect */
				if (supported_videos[cur_video].dex_support != DEX_NOT_SUPPORT &&
#ifdef FEATURE_IGNORE_PREFER_IF_DEX_RES_EXIST
					cur_video == best_dex_video &&
#endif
					supported_videos[cur_video].dex_support <= displayport->dex_adapter_type) {
					displayport_info("current video support dex %d\n", cur_video);
					need_reconnect = 0;
				}
			} else {
				/* if current resolution is best, then do not reconnect */
				if (cur_video == best_video) {
					displayport_info("current video is best %d\n", cur_video);
					need_reconnect = 0;
				}
			}
		}
	}

	displayport->dex_state = dex_run;

	/* reconnect if setting was mirroring(0) and dex is running(1), */
	if (displayport->hpd_current_state && need_reconnect) {
		displayport_info("reconnecting to %s mode\n", dex_run ? "dex":"mirroring");
		displayport->dex_state = DEX_RECONNECTING;
		displayport_hpd_changed(0);
		msleep(1000);
		if (displayport->dex_state != DEX_OFF)
			displayport_hpd_changed(1);
	}

dex_exit:
	displayport_info("dex exit: state:%d, setting:%d\n",
			displayport->dex_state, displayport->dex_setting);

	return size;
}
static CLASS_ATTR_RW(dex);

static ssize_t dex_ver_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int ret = 0;
	struct displayport_device *displayport = get_displayport_drvdata();

	ret = scnprintf(buf, PAGE_SIZE, "%02X%02X\n", displayport->dex_ver[0],
			displayport->dex_ver[1]);

	return ret;
}
static CLASS_ATTR_RO(dex_ver);

static ssize_t monitor_info_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	int ret = 0;
	struct displayport_device *displayport = get_displayport_drvdata();
	u32 sst_id = displayport_get_sst_id_with_decon_id(DEFAULT_DECON_ID);

	ret = scnprintf(buf, PAGE_SIZE, "%s,0x%x,0x%x\n",
			displayport->sst[sst_id]->rx_edid_data.edid_manufacturer,
			displayport->sst[sst_id]->rx_edid_data.edid_product,
			displayport->sst[sst_id]->rx_edid_data.edid_serial);
	displayport_info("manufacturer : %s product : %x serial num : %x\n",
			displayport->sst[sst_id]->rx_edid_data.edid_manufacturer,
			displayport->sst[sst_id]->rx_edid_data.edid_product,
			displayport->sst[sst_id]->rx_edid_data.edid_serial);

	return ret;
}
static CLASS_ATTR_RO(monitor_info);

static ssize_t dp_sbu_sw_sel_store(struct class *dev,
		struct class_attribute *attr, const char *buf, size_t size)
{
	struct displayport_device *displayport = get_displayport_drvdata();
	int val[3] = {0,};
	int aux_sw_sel, aux_sw_oe;

	if (strnchr(buf, size, '-')) {
		pr_err("%s range option not allowed\n", __func__);
		return -EINVAL;
	}

	get_options(buf, 3, val);

	aux_sw_sel = val[1];
	aux_sw_oe = val[2];
	displayport_info("sbu_sw_sel(%d), sbu_sw_oe(%d)\n", aux_sw_sel, aux_sw_oe);

	if ((aux_sw_sel == 0 || aux_sw_sel == 1) && (aux_sw_oe == 0 || aux_sw_oe == 1)) {
		if (gpio_is_valid(displayport->gpio_sw_sel))
			gpio_direction_output(displayport->gpio_sw_sel, aux_sw_sel);
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
		displayport_aux_onoff(displayport, !aux_sw_oe);
#endif
	} else
		displayport_err("invalid aux switch parameter\n");

	return size;
}
static CLASS_ATTR_WO(dp_sbu_sw_sel);

static ssize_t log_level_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "displayport log level %1d\n", displayport_log_level);
}
static ssize_t log_level_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	int mode = 0;

	if (kstrtoint(buf, 10, &mode))
		return size;
	displayport_log_level = mode;
	displayport_err("log level = %d\n", displayport_log_level);

	return size;
}
static CLASS_ATTR_RW(log_level);

__visible_for_testing int displayport_init_sst_info(struct displayport_device *displayport)
{
	int ret = 0;
	int i = 0;
	struct displayport_sst *sst = NULL;
	struct displayport_vc_config *vc_config = NULL;

	for (i = SST1; i < MAX_SST_CNT; i++) {
		if (displayport->sst[i] == NULL) {
			sst = kzalloc(sizeof(struct displayport_sst), GFP_KERNEL);
			if (!sst) {
				displayport_err("could not allocate sst[%d]\n", i);
				ret = -ENOMEM;
				goto err_sst_info;
			}

			displayport->sst[i] = sst;
			displayport->sst[i]->id = i;
			displayport->sst[i]->decon_id = displayport_get_decon_id(i);
			displayport->sst[i]->state = DISPLAYPORT_STATE_INIT;
			displayport->sst[i]->cur_video = V640X480P60;
			displayport->sst[i]->bpc = BPC_8;
			displayport->sst[i]->dyn_range = VESA_RANGE;
			displayport->sst[i]->bist_used = 0;
			displayport->sst[i]->bist_type = COLOR_BAR;
			displayport->sst[i]->audio_state = AUDIO_DISABLE;
			displayport->sst[i]->audio_buf_empty_check = 0;

			vc_config = kzalloc(sizeof(struct displayport_vc_config), GFP_KERNEL);
			if (!vc_config) {
				displayport_err("could not allocate sst[%d] vc_config\n", i);
				ret = -ENOMEM;
				goto err_sst_info;
			}

			displayport->sst[i]->vc_config = vc_config;
		}
	}

err_sst_info:
	return ret;
}

static int displayport_debug_dump_show(struct seq_file *s, void *unused)
{
	struct displayport_device *displayport = s->private;

	if (displayport->sst[0]->state != DISPLAYPORT_STATE_ON) {
		displayport_info("%s: displayport is not ON(%d)\n",
				__func__, displayport->sst[0]->state);
		return 0;
	}
	displayport_dump_registers(displayport);
	displayport_phy_dump_registers(displayport);
	return 0;
}

static int displayport_debug_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, displayport_debug_dump_show, inode->i_private);
}

static const struct file_operations displayport_dump_fops = {
	.open = displayport_debug_dump_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static int displayport_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct displayport_device *displayport = NULL;
	struct class *dp_class;
	char name[MAX_NAME_SIZE];

#if IS_ENABLED(CONFIG_SEC_DISPLAYPORT_LOGGER)
	dp_logger_init();
#endif

	dev_info(dev, "%s start\n", __func__);

	displayport = devm_kzalloc(dev, sizeof(struct displayport_device), GFP_KERNEL);
	if (!displayport) {
		displayport_err("failed to allocate displayport device.\n");
		ret = -ENOMEM;
		goto err;
	}

	ret = displayport_init_sst_info(displayport);
	if (ret)
		goto err_dt;

	dma_set_mask(dev, DMA_BIT_MASK(32));

	ret = displayport_parse_dt(displayport, dev);
	if (ret)
		goto err_dt;

	displayport_drvdata = displayport;

	spin_lock_init(&displayport->slock);
	mutex_init(&displayport->cmd_lock);
	mutex_init(&displayport->hpd_lock);
	mutex_init(&displayport->aux_lock);
	mutex_init(&displayport->training_lock);
	mutex_init(&displayport->hdcp2_lock);

	spin_lock_init(&displayport->spinlock_sfr);

#ifdef FEATURE_MANAGE_HMD_LIST
	mutex_init(&displayport->hmd_lock);
	strlcpy(displayport->hmd_list[0].monitor_name, "PicoVR", MON_NAME_LEN);
	displayport->hmd_list[0].ven_id = 0x2d40;
	displayport->hmd_list[0].prod_id = 0x0000;
#endif
	ret = displayport_init_resources(displayport, pdev);
	if (ret)
		goto err_dt;

	displayport_init_subdev(displayport);
	platform_set_drvdata(pdev, displayport);

	displayport->dp_wq = create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!displayport->dp_wq) {
		displayport_err("create wq failed.\n");
		goto err_dt;
	}

	displayport->hdcp2_wq = create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!displayport->hdcp2_wq) {
		displayport_err("create hdcp2_wq failed.\n");
		goto err_dt;
	}

	INIT_DELAYED_WORK(&displayport->hpd_plug_work, displayport_hpd_plug_work);
	INIT_DELAYED_WORK(&displayport->hpd_unplug_work, displayport_hpd_unplug_work);
	INIT_DELAYED_WORK(&displayport->hpd_irq_work, displayport_hpd_irq_work);
	INIT_DELAYED_WORK(&displayport->hdcp13_work, displayport_hdcp13_run);
	INIT_DELAYED_WORK(&displayport->hdcp22_work, displayport_hdcp22_run);
	INIT_DELAYED_WORK(&displayport->hdcp13_integrity_check_work, displayport_hdcp13_integrity_check_work);
	init_waitqueue_head(&displayport->dp_ready_wait);

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#if IS_ENABLED(CONFIG_USE_DISPLAYPORT_PDIC_EVENT_QUEUE)
	INIT_LIST_HEAD(&displayport->list_pd);
	INIT_DELAYED_WORK(&displayport->pdic_event_proceed_work,
			displayport_pdic_event_proceed_work);
	mutex_init(&displayport->pdic_lock);
#endif
	INIT_DELAYED_WORK(&displayport->notifier_register_work,
			displayport_notifier_register_work);
	queue_delayed_work(displayport->dp_wq, &displayport->notifier_register_work,
			msecs_to_jiffies(10000));
#endif

#if defined(CONFIG_EXTCON)
	/* register the extcon device for HPD */
	displayport->extcon_displayport = devm_extcon_dev_allocate(displayport->dev, extcon_id);
	if (IS_ERR(displayport->extcon_displayport)) {
		displayport_err("displayport extcon dev_allocate failed.\n");
		goto err_dt;
	}

	ret = devm_extcon_dev_register(displayport->dev, displayport->extcon_displayport);
	if (ret) {
		displayport_err("hdmi extcon register failed.\n");
		goto err_dt;
	}
#else
	displayport_info("Not compiled EXTCON driver\n");
#endif

#if IS_ENABLED(CONFIG_ANDROID_SWITCH)
	ret = switch_dev_register(&switch_secdp_msg);
	if (ret)
		displayport_err("Failed to register dp msg switch\n");

	ret = switch_dev_register(&switch_secdp_hpd);
	if (ret)
		displayport_err("Failed to register dp hpd switch\n");
#endif


	pm_runtime_enable(dev);

	iommu_register_device_fault_handler(dev, dpu_sysmmu_fault_handler_displayport, NULL);

#if defined(CONFIG_CPU_IDLE)
	displayport->idle_ip_index =
		exynos_get_idle_ip_index(dev_name(&pdev->dev), 1);
	if (displayport->idle_ip_index < 0)
		displayport_warn("idle ip index is not provided for DP\n");
	exynos_update_ip_idle_status(displayport->idle_ip_index, 1);
#endif

	ret = device_init_wakeup(displayport->dev, true);
	if (ret) {
		dev_err(displayport->dev, "failed to init wakeup device\n");
		return -EINVAL;
	}
#ifdef CONFIG_DISPLAYPORT_ENG
	displayport->edid_test_buf = edid_test_buf;
#endif
	dp_class = class_create(THIS_MODULE, "dp_sec");
	if (IS_ERR(dp_class))
		displayport_err("failed to creat dp_class\n");
	else {
#ifdef DISPLAYPORT_TEST
		ret = class_create_file(dp_class, &class_attr_link);
		if (ret)
			displayport_err("failed to create attr_link\n");
		ret = class_create_file(dp_class, &class_attr_bpc);
		if (ret)
			displayport_err("failed to create attr_bpc\n");
		ret = class_create_file(dp_class, &class_attr_range);
		if (ret)
			displayport_err("failed to create attr_range\n");
		ret = class_create_file(dp_class, &class_attr_edid);
		if (ret)
			displayport_err("failed to create attr_edid\n");
		ret = class_create_file(dp_class, &class_attr_bist);
		if (ret)
			displayport_err("failed to create attr_bist\n");
		ret = class_create_file(dp_class, &class_attr_dp);
		if (ret)
			displayport_err("failed to create attr_test\n");
		ret = class_create_file(dp_class, &class_attr_forced_resolution);
		if (ret)
			displayport_err("failed to create attr_dp_forced_resolution\n");
		ret = class_create_file(dp_class, &class_attr_reduced_resolution);
		if (ret)
			displayport_err("failed to create attr_dp_reduced_resolution\n");
#endif
#ifdef CONFIG_DISPLAYPORT_ENG
		ret = class_create_file(dp_class, &class_attr_audio_test);
		if (ret)
			displayport_err("failed to create attr_audio_test\n");

		ret = class_create_file(dp_class, &class_attr_dp_test);
		if (ret)
			displayport_err("failed to create attr_dp_test\n");

		ret = class_create_file(dp_class, &class_attr_edid_test);
		if (ret)
			displayport_err("failed to create attr_edid\n");

		ret = class_create_file(dp_class, &class_attr_phy_tune);
		if (ret)
			displayport_err("failed to create attr_phy_tune\n");
#endif

		ret = class_create_file(dp_class, &class_attr_dex);
		if (ret)
			displayport_err("failed to create attr_dp_dex\n");
		ret = class_create_file(dp_class, &class_attr_dex_ver);
		if (ret)
			displayport_err("failed to create attr_dp_dex_ver\n");
		ret = class_create_file(dp_class, &class_attr_monitor_info);
		if (ret)
			displayport_err("failed to create attr_dp_monitor_info\n");
		ret = class_create_file(dp_class, &class_attr_dp_sbu_sw_sel);
		if (ret)
			displayport_err("failed to create class_attr_dp_sbu_sw_sel\n");
		ret = class_create_file(dp_class, &class_attr_log_level);
		if (ret)
			displayport_err("failed to create class_attr_log_level\n");
#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
		secdp_bigdata_init(dp_class);
#endif
#ifdef CONFIG_SEC_DISPLAYPORT_SELFTEST
		displayport->hpd_changed = displayport_hpd_changed;
		self_test_init(displayport, dp_class);
#endif

	}
	displayport->debug_root = debugfs_create_dir("displayport", NULL);
	if (!displayport->debug_root) {
		displayport_err("failed to create debugfs root directory.\n");
		ret = -ENOENT;
		goto err_debugfs;
	}

	snprintf(name, MAX_NAME_SIZE, "dp_dump", displayport);
	displayport->debug_dump = debugfs_create_file(name, 0444,
			displayport->debug_root, displayport, &displayport_dump_fops);
	if (!displayport->debug_dump) {
		displayport_err("failed to create SFR dump debugfs file\n");
		goto err_debugfs;
	}

	g_displayport_debug_param.param_used = 0;
	g_displayport_debug_param.link_rate = LINK_RATE_2_7Gbps;
	g_displayport_debug_param.lane_cnt = 0x04;

#if IS_ENABLED(CONFIG_EXYNOS_HDCP2)
	displayport->drm_start_state = DRM_OFF;

	displayport_register_func(displayport_hdcp22_enable, displayport_dpcd_read_for_hdcp22, displayport_dpcd_write_for_hdcp22);
#endif

#if defined(CONFIG_EXTCON) && !IS_ENABLED(CONFIG_ANDROID_SWITCH)
	dp_hpd_extcon = devm_extcon_dev_allocate(dev, dp_hpd_extcon_id);
	if (IS_ERR(dp_hpd_extcon)) {
		displayport_err("Failed to allocate dp hpd extcon\n");
		dp_hpd_extcon = NULL;
	} else {
		dp_hpd_extcon->dev.init_name = "hdmi";
		ret = devm_extcon_dev_register(dev, dp_hpd_extcon);
		if (ret) {
			displayport_err("Failed to register dp hpd extcon\n");
			dp_hpd_extcon = NULL;
		}
	}
#endif
	displayport_info("displayport driver has been probed.\n");

	return 0;

err_debugfs:
err_dt:
	kfree(displayport);
err:
	return ret;
}

static void displayport_shutdown(struct platform_device *pdev)
{
#if 0
	struct displayport_device *displayport = platform_get_drvdata(pdev);
	int i = 0;

	/* DPU_EVENT_LOG(DPU_EVT_DP_SHUTDOWN, &displayport->sd, ktime_set(0, 0)); */
	for (i = SST1; i < MAX_SST_CNT; i++) {
		displayport_info("SST%d %s + state:%d\n", i + 1, __func__, displayport->sst[i]->state);
		displayport_disable(displayport);
	}

	displayport_info("%s -\n", __func__);
#else
	displayport_info("%s +-\n", __func__);
#endif
}

static int displayport_runtime_suspend(struct device *dev)
{
	struct displayport_device *displayport = dev_get_drvdata(dev);

	/* DPU_EVENT_LOG(DPU_EVT_DP_SUSPEND, &displayport->sd, ktime_set(0, 0)); */
	displayport_dbg("%s +\n", __func__);
	clk_disable_unprepare(displayport->res.aclk);
	displayport_dbg("%s -\n", __func__);
	return 0;
}

static int displayport_runtime_resume(struct device *dev)
{
	struct displayport_device *displayport = dev_get_drvdata(dev);

	/* DPU_EVENT_LOG(DPU_EVT_DP_RESUME, &displayport->sd, ktime_set(0, 0)); */
	displayport_dbg("%s: +\n", __func__);
	clk_prepare_enable(displayport->res.aclk);
	displayport_dbg("%s -\n", __func__);
	return 0;
}

static const struct of_device_id displayport_of_match[] = {
	{ .compatible = "samsung,exynos-displayport" },
	{},
};
MODULE_DEVICE_TABLE(of, displayport_of_match);

static const struct dev_pm_ops displayport_pm_ops = {
	.runtime_suspend	= displayport_runtime_suspend,
	.runtime_resume		= displayport_runtime_resume,
};

struct platform_driver displayport_driver __refdata = {
	.probe			= displayport_probe,
	.remove			= displayport_remove,
	.shutdown		= displayport_shutdown,
	.driver = {
		.name		= DISPLAYPORT_MODULE_NAME,
		.owner		= THIS_MODULE,
		.pm		= &displayport_pm_ops,
		.of_match_table	= of_match_ptr(displayport_of_match),
		.suppress_bind_attrs = true,
	}
};

MODULE_AUTHOR("Kwangje Kim <kj1.kim@samsung.com>");
MODULE_DESCRIPTION("Samusung EXYNOS DisplayPort driver");
MODULE_LICENSE("GPL");
