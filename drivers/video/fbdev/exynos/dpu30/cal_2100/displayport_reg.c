/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * SFR access functions for Samsung EXYNOS SoC DisplayPort driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/fb.h>
#include "../displayport.h"
#if defined(CONFIG_PHY_SAMSUNG_USB_CAL)
#include "../../../../drivers/phy/samsung/phy-samsung-usb-cal.h"
#include "../../../../drivers/phy/samsung/phy-exynos-usbdp.h"
#include <linux/usb/otg-fsm.h>
#include "../../../drivers/usb/dwc3/dwc3-exynos.h"
#endif

u32 phy_tune_parameters[4][4][5] = { /* {amp, post, pre, idrv, accdrv} */
	{	/* Swing Level_0 */
		{0x23, 0x10, 0x42, 0x82, 0x00}, /* Pre-emphasis Level_0 */
		{0x28, 0x15, 0x42, 0x82, 0x00}, /* Pre-emphasis Level_1 */
		{0x27, 0x17, 0x43, 0x82, 0x00}, /* Pre-emphasis Level_2 */
		{0x2B, 0x1B, 0x43, 0x83, 0x30}, /* Pre-emphasis Level_3 */
	},
	{	/* Swing Level_1 */
		{0x27, 0x10, 0x42, 0x82, 0x00}, /* Pre-emphasis Level_0 */
		{0x2B, 0x14, 0x42, 0x83, 0x30}, /* Pre-emphasis Level_1 */
		{0x2B, 0x18, 0x43, 0x83, 0x30}, /* Pre-emphasis Level_2 */
		{0x2B, 0x18, 0x43, 0x83, 0x30}, /* Pre-emphasis Level_3 */
	},
	{	/* Swing Level_2 */
		{0x27, 0x10, 0x43, 0x83, 0x30}, /* Pre-emphasis Level_0 */
		{0x2B, 0x17, 0x43, 0x83, 0x38}, /* Pre-emphasis Level_1 */
		{0x2B, 0x17, 0x43, 0x83, 0x38}, /* Pre-emphasis Level_2 */
		{0x2B, 0x17, 0x43, 0x83, 0x38}, /* Pre-emphasis Level_3 */
	},
	{	/* Swing Level_3 */
		{0x2B, 0x10, 0x43, 0x83, 0x30}, /* Pre-emphasis Level_0 */
		{0x2B, 0x10, 0x43, 0x83, 0x30}, /* Pre-emphasis Level_1 */
		{0x2B, 0x10, 0x43, 0x83, 0x30}, /* Pre-emphasis Level_2 */
		{0x2B, 0x10, 0x43, 0x83, 0x30}, /* Pre-emphasis Level_3 */
	},
};

/* supported_videos[] is to be arranged in the order of pixel clock */
struct displayport_supported_preset supported_videos[] = {
	/* video_format,	dv_timings,	fps,	v_sync_pol,	h_sync_pol,	vic, ratio,	name,	dex_support, pro_audio, displayid_timing */
	{V640X480P60,	V4L2_DV_BT_DMT_640X480P60,	60, SYNC_NEGATIVE, SYNC_NEGATIVE, 1, RATIO_4_3, "V640X480P60",	DEX_FHD_SUPPORT},
	{V720X480P60,	V4L2_DV_BT_CEA_720X480P59_94,	60, SYNC_NEGATIVE, SYNC_NEGATIVE, 2, RATIO_16_9, "V720X480P60",	DEX_FHD_SUPPORT},
	{V720X576P50,	V4L2_DV_BT_CEA_720X576P50,	50, SYNC_NEGATIVE, SYNC_NEGATIVE, 17, RATIO_4_3, "V720X576P50",	DEX_FHD_SUPPORT},
	{V1280X800P60RB, V4L2_DV_BT_DMT_1280X800P60_RB,	60, SYNC_NEGATIVE, SYNC_POSITIVE, 0, RATIO_16_10, "V1280X800P60RB",	DEX_FHD_SUPPORT},
	{V1280X720P50,	V4L2_DV_BT_CEA_1280X720P50,	50, SYNC_POSITIVE, SYNC_POSITIVE, 19, RATIO_16_9, "V1280X720P50",	DEX_FHD_SUPPORT},
	{V1280X720P60EXT,	DISPLAYID_720P_EXT, 60, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_16_9, "V1280X720P60EXT",	DEX_FHD_SUPPORT, false, DISPLAYID_EXT},
	{V1280X720P60,	V4L2_DV_BT_CEA_1280X720P60,	60, SYNC_POSITIVE, SYNC_POSITIVE, 4, RATIO_16_9, "V1280X720P60",	DEX_FHD_SUPPORT, true},
	{V1366X768P60,  V4L2_DV_BT_DMT_1366X768P60,     60, SYNC_POSITIVE, SYNC_NEGATIVE,   0, RATIO_16_9, "V1366X768P60", DEX_FHD_SUPPORT},
	{V1280X1024P60,	V4L2_DV_BT_DMT_1280X1024P60,	60, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_4_3, "V1280X1024P60", DEX_FHD_SUPPORT},
	{V1920X1080P24,	V4L2_DV_BT_CEA_1920X1080P24,	24, SYNC_POSITIVE, SYNC_POSITIVE, 32, RATIO_16_9, "V1920X1080P24",	DEX_FHD_SUPPORT},
	{V1920X1080P25,	V4L2_DV_BT_CEA_1920X1080P25,	25, SYNC_POSITIVE, SYNC_POSITIVE, 33, RATIO_16_9, "V1920X1080P25",	DEX_FHD_SUPPORT},
	{V1920X1080P30,	V4L2_DV_BT_CEA_1920X1080P30,	30, SYNC_POSITIVE, SYNC_POSITIVE, 34, RATIO_16_9, "V1920X1080P30",	DEX_FHD_SUPPORT, true},
	{V1600X900P60DTD, VIDEO_DTD_1600X900P60,	60, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_16_9, "V1600X900P60DTD", DEX_FHD_SUPPORT, false, FB_MODE_IS_DETAILED},
	{V1600X900P59, V4L2_DV_BT_CVT_1600X900P59_ADDED, 59, SYNC_POSITIVE,	SYNC_POSITIVE, RATIO_16_9, 0, "V1600X900P59", DEX_FHD_SUPPORT},
	{V1600X900P60RB, V4L2_DV_BT_DMT_1600X900P60_RB,	60, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_16_9, "V1600X900P60RB",	DEX_FHD_SUPPORT},
	{V1920X1080P50,	V4L2_DV_BT_CEA_1920X1080P50,	50, SYNC_POSITIVE, SYNC_POSITIVE, 31, RATIO_16_9, "V1920X1080P50",	DEX_FHD_SUPPORT},
	{V1920X1080P60DTD, VIDEO_DTD_1080P60, 60, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_16_9, "V1920X1080P60DTD", DEX_FHD_SUPPORT, false, FB_MODE_IS_DETAILED},
	{V1920X1080P60EXT,	DISPLAYID_1080P_EXT, 60, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_16_9, "V1920X1080P60EXT", DEX_FHD_SUPPORT, false, DISPLAYID_EXT},
	{V1920X1080P59,	V4L2_DV_BT_CVT_1920X1080P59_ADDED, 59, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_16_9, "V1920X1080P59", DEX_FHD_SUPPORT},
	{V1920X1080P60,	V4L2_DV_BT_CEA_1920X1080P60,	60, SYNC_POSITIVE, SYNC_POSITIVE, 16, RATIO_16_9, "V1920X1080P60",	DEX_FHD_SUPPORT, true},
	{V1920X1200P60RB, V4L2_DV_BT_DMT_1920X1200P60_RB, 60, SYNC_NEGATIVE, SYNC_POSITIVE, 0, RATIO_16_10, "V1920X1200P60RB", DEX_WQHD_SUPPORT},
	{V1920X1200P60,	V4L2_DV_BT_DMT_1920X1200P60,	60, SYNC_POSITIVE, SYNC_NEGATIVE, 0, RATIO_16_10, "V1920X1200P60",	DEX_WQHD_SUPPORT},
	{V2560X1080P60, V4L2_DV_BT_CVT_2560x1080P60_ADDED, 60, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_21_9, "V2560X1080P60", DEX_WQHD_SUPPORT},
	{V2048X1536P60,	V4L2_DV_BT_CVT_2048X1536P60_ADDED, 60, SYNC_NEGATIVE, SYNC_POSITIVE, 0, RATIO_4_3, "V2048X1536P60"},
	{V1920X1440P60,	V4L2_DV_BT_DMT_1920X1440P60,	60, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_4_3, "V1920X1440P60"},
	{V2400X1200P90RELU, DISPLAYID_2400X1200P90_RELUMINO, 90, SYNC_NEGATIVE, SYNC_NEGATIVE, 0, RATIO_ETC, "V2400X1200P90RELU", DEX_NOT_SUPPORT},
	{V2560X1440P60DTD, VIDEO_DTD_1440P60, 60, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_16_9, "V2560X1440P60DTD", DEX_WQHD_SUPPORT, false, FB_MODE_IS_DETAILED},
	{V2560X1440P60EXT,	DISPLAYID_1440P_EXT, 60, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_16_9, "V2560X1440P60EXT", DEX_WQHD_SUPPORT, false, DISPLAYID_EXT},
	{V2560X1440P59,	V4L2_DV_BT_CVT_2560X1440P59_ADDED, 59, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_16_9, "V2560X1440P59", DEX_WQHD_SUPPORT},
	{V1440x2560P60,	V4L2_DV_BT_CVT_1440X2560P60_ADDED, 60, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_ETC, "V1440x2560P60"},
	{V1440x2560P75,	V4L2_DV_BT_CVT_1440X2560P75_ADDED, 75, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_ETC, "V1440x2560P75"},
	{V2560X1440P60,	V4L2_DV_BT_CVT_2560X1440P60_ADDED, 60, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_16_9, "V2560X1440P60", DEX_WQHD_SUPPORT},
	{V2560X1600P60,	V4L2_DV_BT_CVT_2560X1600P60_ADDED, 60, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_16_10, "V2560X1600P60", DEX_WQHD_SUPPORT},
	{V3440X1440P50,	V4L2_DV_BT_CVT_3440X1440P50_ADDED, 50, SYNC_NEGATIVE, SYNC_POSITIVE, 0, RATIO_21_9, "V3440X1440P50", DEX_WQHD_SUPPORT},
	{V3840X1080P60,	V4L2_DV_BT_CEA_3840X1080P60_ADDED,	60, SYNC_NEGATIVE, SYNC_POSITIVE, 0, RATIO_ETC, "V3840X1080P60"},
	{V3840X1200P60,	V4L2_DV_BT_CEA_3840X1200P60_ADDED,	60, SYNC_NEGATIVE,	SYNC_POSITIVE, 0, RATIO_ETC, "V3840X1200P60"},
	{V3440X1440P60,	V4L2_DV_BT_CVT_3440X1440P60_ADDED, 60, SYNC_NEGATIVE, SYNC_POSITIVE, 0, RATIO_21_9, "V3440X1440P60", DEX_WQHD_SUPPORT},
/*	{V3440X1440P100, V4L2_DV_BT_CVT_3440X1440P100_ADDED, 100, SYNC_NEGATIVE, SYNC_POSITIVE, 0, RATIO_21_9, "V3440X1440P100"}, */
	{V3840X2160P24,	V4L2_DV_BT_CEA_3840X2160P24,	24, SYNC_POSITIVE, SYNC_POSITIVE, 93, RATIO_16_9, "V3840X2160P24"},
	{V3840X2160P25,	V4L2_DV_BT_CEA_3840X2160P25,	25, SYNC_POSITIVE, SYNC_POSITIVE, 94, RATIO_16_9, "V3840X2160P25"},
	{V3840X2160P30,	V4L2_DV_BT_CEA_3840X2160P30,	30, SYNC_POSITIVE, SYNC_POSITIVE, 95, RATIO_16_9, "V3840X2160P30",	DEX_NOT_SUPPORT, true},
	{V4096X2160P24,	V4L2_DV_BT_CEA_4096X2160P24,	24, SYNC_POSITIVE, SYNC_POSITIVE, 98, RATIO_16_9, "V4096X2160P24"},
	{V4096X2160P25,	V4L2_DV_BT_CEA_4096X2160P25,	25, SYNC_POSITIVE, SYNC_POSITIVE, 99, RATIO_16_9, "V4096X2160P25"},
	{V4096X2160P30,	V4L2_DV_BT_CEA_4096X2160P30,	30, SYNC_POSITIVE, SYNC_POSITIVE, 100, RATIO_16_9, "V4096X2160P30"},
	{V3840X2160P50,	V4L2_DV_BT_CEA_3840X2160P50,	50, SYNC_POSITIVE, SYNC_POSITIVE, 96, RATIO_16_9, "V3840X2160P50"},
	{V3840X2160P60DTD, VIDEO_DTD_3840X2160P60,	60, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_16_9, "V3840X2160P60DTD", DEX_NOT_SUPPORT, false, FB_MODE_IS_DETAILED},
	{V3840X2160P60EXT,	DISPLAYID_2160P_EXT,	60, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_16_9, "V3840X2160P60EXT", DEX_NOT_SUPPORT, false, DISPLAYID_EXT},
	{V3840X2160P59RB, V4L2_DV_BT_CVT_3840X2160P59_ADDED, 59, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_16_9, "V3840X2160P59RB"},
	{V3840X2160P60,	V4L2_DV_BT_CEA_3840X2160P60,	60, SYNC_POSITIVE, SYNC_POSITIVE, 97, RATIO_16_9, "V3840X2160P60",	DEX_NOT_SUPPORT, true},
	{V4096X2160P50, V4L2_DV_BT_CEA_4096X2160P50,	50, SYNC_POSITIVE, SYNC_POSITIVE, 101, RATIO_16_9, "V4096X2160P50"},
	{V4096X2160P60DTD, VIDEO_DTD_4096X2160P60, 60, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_16_9, "V3840X2160P60DTD", DEX_NOT_SUPPORT, false, FB_MODE_IS_DETAILED},
	{V4096X2160P60,	V4L2_DV_BT_CEA_4096X2160P60,	60, SYNC_POSITIVE, SYNC_POSITIVE, 102, RATIO_16_9, "V4096X2160P60"},
	{V640X10P60SACRC, V4L2_DV_BT_CVT_640x10P60_ADDED, 60, SYNC_POSITIVE, SYNC_POSITIVE, 0, RATIO_ETC, "V640X10P60SACRC"},
	{VDUMMYTIMING, V4L2_DV_BT_CVT_640x10P60_ADDED,	60, SYNC_POSITIVE, SYNC_POSITIVE,  0, RATIO_ETC, "DetailedTiming"},
};

const int supported_videos_pre_cnt = ARRAY_SIZE(supported_videos);

u32 audio_async_m_n[2][4][7] = {
	{	/* M value set */
		{3314, 4567, 4971, 9134, 9942, 18269, 19884},
		{1988, 2740, 2983, 5481, 5695, 10961, 11930},
		{ 994, 1370, 1491, 2740, 2983,  5481,  5965},
		{ 663,  913,  994, 1827, 1988,  3654,  3977},
	},
	{	/* N value set */
		{32768, 32768, 32768, 32768, 32768, 32768, 32768},
		{32768, 32768, 32768, 32768, 32768, 32768, 32768},
		{32768, 32768, 32768, 32768, 32768, 32768, 32768},
		{32768, 32768, 32768, 32768, 32768, 32768, 32768},
	}
};

u32 audio_sync_m_n[2][4][7] = {
	{	/* M value set */
		{1024, 784, 512, 1568, 1024, 3136, 2048},
		{1024, 784, 512, 1568, 1024, 3136, 2048},
		{1024, 784, 512,  784,  512, 1568, 1024},
		{1024, 784, 512, 1568, 1024, 3136, 2048},
	},
	{	/* N value set */
		{10125,  5625,  3375,  5625,  3375,  5625,  3375},
		{16875,  9375,  5625,  9375,  5625,  9375,  5625},
		{33750, 18750, 11250,  9375,  5625,  9375,  5625},
		{50625, 28125, 16875, 28125, 16875, 28125, 16875},
	}
};

u32 m_aud_master[7] = {32000, 44100, 48000, 88200, 96000, 176000, 192000};

u32 n_aud_master[4] = {81000000, 135000000, 270000000, 405000000};

void displayport_reg_sysclk_sel_txclk(void)
{
	displayport_write_mask(SYSTEM_CLK_CONTROL, 0x1, TXCLK_SEL_MODE);
}

void displayport_reg_sysclk_set_osc(void)
{
	displayport_write_mask(SYSTEM_CLK_CONTROL, 0x0, TXCLK_SEL);
}

void displayport_reg_sysclk_set_txclk(void)
{
	displayport_write_mask(SYSTEM_CLK_CONTROL, 0x1, TXCLK_SEL);
}

void displayport_reg_sw_reset(void)
{
	u32 cnt = 10;
	u32 state;

	displayport_info("%s\n", __func__);
	displayport_reg_sysclk_set_osc();
	udelay(1);
	displayport_write_mask(SYSTEM_SW_RESET_CONTROL, ~0, SW_RESET);

	do {
		state = displayport_read(SYSTEM_SW_RESET_CONTROL) & SW_RESET;
		cnt--;
		udelay(1);
	} while (state && cnt);

	if (!cnt)
		displayport_err("%s is timeout.\n", __func__);
}

void displayport_reg_phy_reset(u32 en)
{
	if (en) {
		// PHY reset assertion
		displayport_reg_sysclk_sel_txclk();
		displayport_reg_sysclk_set_osc();
		displayport_phy_write_mask(CMN_REG00BD, 0, DP_INIT_RSTN);
		udelay(2);
		displayport_phy_write_mask(CMN_REG00BD, 0, DP_CMN_RSTN);
		udelay(2);
	} else {
		displayport_phy_write_mask(CMN_REG00BD, ~0, DP_INIT_RSTN);
		udelay(2);
		displayport_phy_write_mask(CMN_REG00BD, ~0, DP_CMN_RSTN);
		udelay(2);
	}
}

void displayport_reg_phy_init_setting(void)
{
	displayport_phy_write_mask(CMN_REG0189, 0xF, DP_TX_CLK_INV_RBR);
	displayport_phy_write_mask(CMN_REG0008, 0, OVRD_AUX_EN);
	displayport_phy_write_mask(CMN_REG000A, 0xF, ANA_AUX_TX_LVL_CTRL);
	displayport_phy_write_mask(CMN_REG005E, 0x1, ANA_ROPLL_ANA_VCI_SEL);

	/* phy tune and dft setting instead of matrix type, phy_default_value */
	displayport_phy_write_mask(CMN_REG0194, 0x1, ANA_ROPLL_CLK_OUT_TO_EXT_IO_EN);
	displayport_phy_write_mask(CMN_REG00B1, 0x10, ANA_ROPLL_RESERVED);
	displayport_phy_write_mask(CMN_REG00AC, 0x2, ROPLL_MISC_CLK_DIV_RBR);
	displayport_phy_write_mask(CMN_REG00AD, 0x2, ROPLL_MISC_CLK_DIV_HBR);
	displayport_phy_write_mask(CMN_REG00AE, 0x2, ROPLL_MISC_CLK_DIV_HBR2);
	displayport_phy_write_mask(CMN_REG00AF, 0x2, ROPLL_MISC_CLK_DIV_HBR3);
	displayport_phy_write_mask(CMN_REG0105, 0x1, DP_LINK_DIV10_CLK_OUT_SEL);
}

void displayport_reg_phy_mode_setting(void)
{
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct displayport_device *displayport = get_displayport_drvdata();
#endif
	u32 lane_config_val = 0;
	u32 lane_config_val2 = 0;
	u32 lane_config_val3 = 0;
	u32 lane_en_val = 0;

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	switch (displayport->pdic_notify_dp_conf) {
	case PDIC_NOTIFY_DP_PIN_UNKNOWN:
		displayport_dbg("PDIC_NOTIFY_DP_PIN_UNKNOWN\n");
		break;

	case PDIC_NOTIFY_DP_PIN_A:
	case PDIC_NOTIFY_DP_PIN_C:
	case PDIC_NOTIFY_DP_PIN_E:
#if defined(CONFIG_PHY_SAMSUNG_USB_CAL)
		exynos_usbdrd_inform_dp_use(1, 4);
#endif
		lane_config_val = LANE_MUX_SEL_DP_LN3 | LANE_MUX_SEL_DP_LN2
				| LANE_MUX_SEL_DP_LN1 | LANE_MUX_SEL_DP_LN0;
		lane_config_val2 = LN1_TX_SER_40BIT_EN_SSP | LN1_TX_SER_40BIT_EN_HBR
				| LN1_TX_SER_RATE_SEL_SP;
		lane_config_val3 = OVRD_LN3_TX_RXD_COMP_EN | OVRD_LN3_TX_RXD_EN
				| LN3_TX_SER_40BIT_EN_SP;
		lane_en_val = DP_LANE_EN_LN3 | DP_LANE_EN_LN2
				| DP_LANE_EN_LN1 | DP_LANE_EN_LN0;
		break;

	case PDIC_NOTIFY_DP_PIN_B:
	case PDIC_NOTIFY_DP_PIN_D:
	case PDIC_NOTIFY_DP_PIN_F:
#if defined(CONFIG_PHY_SAMSUNG_USB_CAL)
		exynos_usbdrd_inform_dp_use(1, 2);
#endif
		if (displayport->dp_sw_sel) {
			lane_config_val = LANE_MUX_SEL_DP_LN3 | LANE_MUX_SEL_DP_LN2;
			lane_config_val2 = LN1_TX_SER_RATE_SEL_SP;
			lane_config_val3 = OVRD_LN3_TX_RXD_COMP_EN | OVRD_LN3_TX_RXD_EN
					| LN3_TX_SER_40BIT_EN_SP;
			lane_en_val = DP_LANE_EN_LN3 | DP_LANE_EN_LN2;
		} else {
			lane_config_val = LANE_MUX_SEL_DP_LN1 | LANE_MUX_SEL_DP_LN0;
			lane_config_val2 = LN1_TX_SER_40BIT_EN_SSP | LN1_TX_SER_40BIT_EN_HBR
					| LN1_TX_SER_RATE_SEL_SP;
			lane_config_val3 = LN3_TX_SER_40BIT_EN_SP;
			lane_en_val = DP_LANE_EN_LN1 | DP_LANE_EN_LN0;
		}
		break;

	default:
		displayport_dbg("PDIC_NOTIFY_DP_PIN_UNKNOWN\n");
		break;
	}
#endif

	displayport_phy_write_mask(CMN_REG00B8, lane_config_val,
			LANE_MUX_SEL_DP_LN3 | LANE_MUX_SEL_DP_LN2
			| LANE_MUX_SEL_DP_LN1 | LANE_MUX_SEL_DP_LN0);

	displayport_phy_write_mask(CMN_REG00B9, lane_en_val,
			DP_LANE_EN_LN3 | DP_LANE_EN_LN2
			| DP_LANE_EN_LN1 | DP_LANE_EN_LN0);

	displayport_phy_write_mask(TRSV_REG0413, lane_config_val2,
			LN1_TX_SER_40BIT_EN_SSP | LN1_TX_SER_40BIT_EN_HBR
			| LN1_TX_SER_RATE_SEL_SP);

	displayport_phy_write_mask(TRSV_REG0813, lane_config_val3,
			OVRD_LN3_TX_RXD_COMP_EN | OVRD_LN3_TX_RXD_EN
			| LN3_TX_SER_40BIT_EN_SP);
}

void displayport_reg_phy_ssc_enable(u32 en)
{
	displayport_phy_write_mask(CMN_REG00B8, en, SSC_EN);
}

void displayport_reg_wait_phy_pll_lock(void)
{
	u32 cnt = 165;	/* wait for 150us + 10% margin */
	u32 state;

	do {
		state = displayport_read(SYSTEM_PLL_LOCK_CONTROL) & PLL_LOCK_STATUS;
		cnt--;
		udelay(1);
	} while (!state && cnt);

	if (!cnt)
		displayport_err("%s is timeout.\n", __func__);
	displayport_reg_sysclk_set_txclk();
}

void displayport_reg_phy_set_link_bw(u8 link_rate)
{
	u32 val = 0;

	switch (link_rate) {
	case LINK_RATE_8_1Gbps:
		val = 0x03;
		break;
	case LINK_RATE_5_4Gbps:
		val = 0x02;
		break;
	case LINK_RATE_2_7Gbps:
		val = 0x01;
		break;
	case LINK_RATE_1_62Gbps:
		val = 0x00;
		break;
	default:
		val = 0x02;
	}

	displayport_phy_write_mask(CMN_REG00B9, val, DP_TX_LINK_BW);
}

u32 displayport_reg_phy_get_link_bw(void)
{
	u32 val = 0;

	val = displayport_phy_read_mask(CMN_REG00B9, DP_TX_LINK_BW) >> 4;

	switch (val) {
	case 0x03:
		val = LINK_RATE_8_1Gbps;
		break;
	case 0x02:
		val = LINK_RATE_5_4Gbps;
		break;
	case 0x01:
		val = LINK_RATE_2_7Gbps;
		break;
	case 0x00:
		val = LINK_RATE_1_62Gbps;
		break;
	default:
		val = LINK_RATE_5_4Gbps;
	}

	return val;
}

void displayport_reg_set_lane_count(u8 lane_cnt)
{
	displayport_write(SYSTEM_MAIN_LINK_LANE_COUNT, lane_cnt);
}

u32 displayport_reg_get_lane_count(void)
{
	return displayport_read(SYSTEM_MAIN_LINK_LANE_COUNT);
}

void displayport_reg_set_training_pattern(displayport_training_pattern pattern)
{
	displayport_write_mask(PCS_TEST_PATTERN_CONTROL, 0, LINK_QUALITY_PATTERN_SET);
	displayport_write_mask(PCS_CONTROL, pattern, LINK_TRAINING_PATTERN_SET);

	if (pattern == NORAMAL_DATA || pattern == TRAINING_PATTERN_4)
		displayport_write_mask(PCS_CONTROL, 0, SCRAMBLE_BYPASS);
	else
		displayport_write_mask(PCS_CONTROL, 1, SCRAMBLE_BYPASS);
}

void displayport_reg_set_qual_pattern(displayport_qual_pattern pattern, displayport_scrambling scramble)
{
	displayport_write_mask(PCS_CONTROL, 0, LINK_TRAINING_PATTERN_SET);
	displayport_write_mask(PCS_TEST_PATTERN_CONTROL, pattern, LINK_QUALITY_PATTERN_SET);
	displayport_write_mask(PCS_CONTROL, scramble, SCRAMBLE_BYPASS);
}

void displayport_reg_set_hbr2_scrambler_reset(u32 uResetCount)
{
	uResetCount /= 2;       /* only even value@Istor EVT1, ?*/
	displayport_write_mask(PCS_HBR2_EYE_SR_CONTROL, uResetCount, HBR2_EYE_SR_COUNT);
}

void displayport_reg_set_pattern_PLTPAT(void)
{
	displayport_write(PCS_TEST_PATTERN_SET0, 0x3E0F83E0);	/* 00111110 00001111 10000011 11100000 */
	displayport_write(PCS_TEST_PATTERN_SET1, 0x0F83E0F8);	/* 00001111 10000011 11100000 11111000 */
	displayport_write(PCS_TEST_PATTERN_SET2, 0x0000F83E);	/* 11111000 00111110 */
}

void displayport_reg_set_phy_tune(u32 phy_lane_num, u32 amp_lvl, u32 pre_emp_lvl)
{
	u32 addr = 0;
	u32 val = 0;
	int i;

	switch (phy_lane_num) {
	case 0:
		addr = TRSV_REG0204;
		break;
	case 1:
		addr = TRSV_REG0404;
		break;
	case 2:
		addr = TRSV_REG0604;
		break;
	case 3:
		addr = TRSV_REG0804;
		break;
	default:
		addr = TRSV_REG0204;
		break;
	}

	for (i = AMP; i <= ACCDRV; i++) {
		val = phy_tune_parameters[amp_lvl][pre_emp_lvl][i];
		displayport_phy_write(addr + i * 4, val);
	}
}

void displayport_reg_set_phy_voltage_and_pre_emphasis(u8 *voltage, u8 *pre_emphasis)
{
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct displayport_device *displayport = get_displayport_drvdata();

	switch (displayport->pdic_notify_dp_conf) {
	case PDIC_NOTIFY_DP_PIN_UNKNOWN:
		break;

	case PDIC_NOTIFY_DP_PIN_A:
		if (displayport->dp_sw_sel) {
			displayport_reg_set_phy_tune(0, voltage[1], pre_emphasis[1]);
			displayport_reg_set_phy_tune(1, voltage[2], pre_emphasis[2]);
			displayport_reg_set_phy_tune(2, voltage[3], pre_emphasis[3]);
			displayport_reg_set_phy_tune(3, voltage[0], pre_emphasis[0]);
		} else {
			displayport_reg_set_phy_tune(0, voltage[0], pre_emphasis[0]);
			displayport_reg_set_phy_tune(1, voltage[3], pre_emphasis[3]);
			displayport_reg_set_phy_tune(2, voltage[2], pre_emphasis[2]);
			displayport_reg_set_phy_tune(3, voltage[1], pre_emphasis[1]);
		}
		break;
	case PDIC_NOTIFY_DP_PIN_B:
		if (displayport->dp_sw_sel) {
			displayport_reg_set_phy_tune(2, voltage[0], pre_emphasis[0]);
			displayport_reg_set_phy_tune(3, voltage[1], pre_emphasis[1]);
		} else {
			displayport_reg_set_phy_tune(0, voltage[1], pre_emphasis[1]);
			displayport_reg_set_phy_tune(1, voltage[0], pre_emphasis[0]);
		}
		break;

	case PDIC_NOTIFY_DP_PIN_C:
	case PDIC_NOTIFY_DP_PIN_E:
		if (displayport->dp_sw_sel) {
			displayport_reg_set_phy_tune(0, voltage[2], pre_emphasis[2]);
			displayport_reg_set_phy_tune(1, voltage[3], pre_emphasis[3]);
			displayport_reg_set_phy_tune(2, voltage[1], pre_emphasis[1]);
			displayport_reg_set_phy_tune(3, voltage[0], pre_emphasis[0]);
		} else {
			displayport_reg_set_phy_tune(0, voltage[0], pre_emphasis[0]);
			displayport_reg_set_phy_tune(1, voltage[1], pre_emphasis[1]);
			displayport_reg_set_phy_tune(2, voltage[3], pre_emphasis[3]);
			displayport_reg_set_phy_tune(3, voltage[2], pre_emphasis[2]);
		}
		break;

	case PDIC_NOTIFY_DP_PIN_D:
	case PDIC_NOTIFY_DP_PIN_F:
		if (displayport->dp_sw_sel) {
			displayport_reg_set_phy_tune(2, voltage[1], pre_emphasis[1]);
			displayport_reg_set_phy_tune(3, voltage[0], pre_emphasis[0]);
		} else {
			displayport_reg_set_phy_tune(0, voltage[0], pre_emphasis[0]);
			displayport_reg_set_phy_tune(1, voltage[1], pre_emphasis[1]);
		}
		break;

	default:
		break;
	}
#endif
}

void displayport_reg_set_voltage_and_pre_emphasis(u8 *voltage, u8 *pre_emphasis)
{
	displayport_reg_set_phy_voltage_and_pre_emphasis(voltage, pre_emphasis);
}

void displayport_reg_function_enable(void)
{
	displayport_write_mask(SYSTEM_COMMON_FUNCTION_ENABLE, 1, PCS_FUNC_EN);
	displayport_write_mask(SYSTEM_COMMON_FUNCTION_ENABLE, 1, AUX_FUNC_EN);
	displayport_write_mask(SYSTEM_SST1_FUNCTION_ENABLE, 1, SST1_VIDEO_FUNC_EN);
	displayport_write_mask(SYSTEM_SST2_FUNCTION_ENABLE, 1, SST2_VIDEO_FUNC_EN);
}

void displayport_reg_set_sst_stream_enable(u32 sst_id, u32 en)
{
	displayport_write_mask(MST_STREAM_1_ENABLE + 0x10 * sst_id, en, STRM_1_EN);
}

void displayport_reg_set_common_interrupt_mask(enum displayport_interrupt_mask param, u8 set)
{
	u32 val = set ? ~0 : 0;

	switch (param) {
	case HOTPLUG_CHG_INT_MASK:
		displayport_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, HPD_CHG_MASK);
		break;
	case HPD_LOST_INT_MASK:
		displayport_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, HPD_LOST_MASK);
		break;
	case PLUG_INT_MASK:
		displayport_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, HPD_PLUG_MASK);
		break;
	case HPD_IRQ_INT_MASK:
		displayport_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, HPD_IRQ_MASK);
		break;
	case RPLY_RECEIV_INT_MASK:
		displayport_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, AUX_REPLY_RECEIVED_MASK);
		break;
	case AUX_ERR_INT_MASK:
		displayport_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, AUX_ERR_MASK);
		break;
	case HDCP_LINK_CHECK_INT_MASK:
		displayport_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, HDCP_R0_CHECK_FLAG_MASK);
		break;
	case HDCP_LINK_FAIL_INT_MASK:
		displayport_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, HDCP_LINK_CHK_FAIL_MASK);
		break;
	case HDCP_R0_READY_INT_MASK:
		displayport_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, HDCP_R0_CHECK_FLAG_MASK);
		break;
	case PLL_LOCK_CHG_INT_MASK:
		displayport_write_mask(SYSTEM_IRQ_COMMON_STATUS_MASK, val, PLL_LOCK_CHG_MASK);
		break;
	case ALL_INT_MASK:
		displayport_write(SYSTEM_IRQ_COMMON_STATUS_MASK, 0xFF);
		break;
	default:
			break;
	}
}

void displayport_reg_set_sst_interrupt_mask(u32 sst_id,
		enum displayport_interrupt_mask param, u8 set)
{
	u32 val = set ? ~0 : 0;

	switch (param) {
	case VIDEO_FIFO_UNDER_FLOW_MASK:
		displayport_write_mask(SST1_INTERRUPT_MASK_SET0 + 0x1000 * sst_id,
				val, MAPI_FIFO_UNDER_FLOW_MASK);
		break;
	case VSYNC_DET_INT_MASK:
		displayport_write_mask(SST1_INTERRUPT_MASK_SET0 + 0x1000 * sst_id,
				val, VSYNC_DET_MASK);
		break;
	case AUDIO_FIFO_UNDER_RUN_INT_MASK:
		displayport_write_mask(SST1_AUDIO_BUFFER_CONTROL + 0x1000 * sst_id,
				val, MASTER_AUDIO_BUFFER_EMPTY_INT_EN);
		displayport_write_mask(SST1_AUDIO_BUFFER_CONTROL + 0x1000 * sst_id,
				val, MASTER_AUDIO_BUFFER_EMPTY_INT_MASK);
		break;
	case ALL_INT_MASK:
		displayport_write(SST1_INTERRUPT_MASK_SET0 + 0x1000 * sst_id, 0xFF);
		displayport_write(SST1_INTERRUPT_STATUS_SET1 + 0x1000 * sst_id, 0xFF);
		break;
	default:
			break;
	}
}

void displayport_reg_set_interrupt(u32 en)
{
	u32 val = en ? ~0 : 0;
	int i = 0;

	displayport_write(SYSTEM_IRQ_COMMON_STATUS, ~0);
	for (i = SST1; i < MAX_SST_CNT; i++) {
		displayport_write(SST1_INTERRUPT_STATUS_SET0 + 0x1000 * i, ~0);
		displayport_write(SST1_INTERRUPT_STATUS_SET1 + 0x1000 * i, ~0);
		displayport_write_mask(SST1_AUDIO_BUFFER_CONTROL + 0x1000 * i,
					1, MASTER_AUDIO_BUFFER_EMPTY_INT);
	}

#if !IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	displayport_reg_set_common_interrupt_mask(HPD_IRQ_INT_MASK, val);
	displayport_reg_set_common_interrupt_mask(HOTPLUG_CHG_INT_MASK, val);
	displayport_reg_set_common_interrupt_mask(HPD_LOST_INT_MASK, val);
	displayport_reg_set_common_interrupt_mask(PLUG_INT_MASK, val);
#endif

	for (i = SST1; i < MAX_SST_CNT; i++) {
		displayport_reg_set_sst_interrupt_mask(i, VSYNC_DET_INT_MASK, val);
		displayport_reg_set_sst_interrupt_mask(i, VIDEO_FIFO_UNDER_FLOW_MASK, val);
		displayport_reg_set_sst_interrupt_mask(i, AUDIO_FIFO_UNDER_RUN_INT_MASK, val);
	}
}

u32 displayport_reg_get_common_interrupt_and_clear(void)
{
	u32 val = 0;

	val = displayport_read(SYSTEM_IRQ_COMMON_STATUS);

	displayport_write(SYSTEM_IRQ_COMMON_STATUS, ~0);

	return val;
}

u32 displayport_reg_get_sst_video_interrupt_and_clear(u32 sst_id)
{
	u32 val = 0;

	val = displayport_read(SST1_INTERRUPT_STATUS_SET0 + 0x1000 * sst_id);

	displayport_write(SST1_INTERRUPT_STATUS_SET0 + 0x1000 * sst_id, ~0);

	return val;
}

u32 displayport_reg_get_sst_audio_interrupt_and_clear(u32 sst_id)
{
	u32 val = 0;

	val = displayport_read_mask(SST1_AUDIO_BUFFER_CONTROL + 0x1000 * sst_id,
			MASTER_AUDIO_BUFFER_EMPTY_INT);

	displayport_write_mask(SST1_AUDIO_BUFFER_CONTROL + 0x1000 * sst_id,
			1, MASTER_AUDIO_BUFFER_EMPTY_INT);

	return val;
}

void displayport_reg_set_daynamic_range(u32 sst_id,
		enum displayport_dynamic_range_type dynamic_range)
{
	displayport_write_mask(SST1_VIDEO_CONTROL + 0x1000 * sst_id,
			dynamic_range, DYNAMIC_RANGE_MODE);
}

void displayport_reg_set_video_bist_mode(u32 sst_id, u32 en)
{
	u32 val = en ? ~0 : 0;

	displayport_write_mask(SST1_VIDEO_CONTROL + 0x1000 * sst_id,
			val, STRM_VALID_FORCE | STRM_VALID_CTRL);
	displayport_write_mask(SST1_VIDEO_BIST_CONTROL + 0x1000 * sst_id,
			val, BIST_EN);
}

void displayport_reg_set_audio_bist_mode(u32 sst_id, u32 en)
{
	u32 val = en ? ~0 : 0;

	displayport_write_mask(SST1_AUDIO_BIST_CONTROL + 0x1000 * sst_id,
			0x0F, SIN_AMPL);
	displayport_write_mask(SST1_AUDIO_BIST_CONTROL + 0x1000 * sst_id,
			val, AUD_BIST_EN);
}

void displayport_reg_video_format_register_setting(u32 sst_id,
		videoformat video_format)
{
	u32 val = 0;

	val += supported_videos[video_format].dv_timings.bt.height;
	val += supported_videos[video_format].dv_timings.bt.vfrontporch;
	val += supported_videos[video_format].dv_timings.bt.vsync;
	val += supported_videos[video_format].dv_timings.bt.vbackporch;
	displayport_write(SST1_VIDEO_VERTICAL_TOTAL_PIXELS + 0x1000 * sst_id, val);
	displayport_dbg("reg set - v total: %d", val);

	val = 0;
	val += supported_videos[video_format].dv_timings.bt.width;
	val += supported_videos[video_format].dv_timings.bt.hfrontporch;
	val += supported_videos[video_format].dv_timings.bt.hsync;
	val += supported_videos[video_format].dv_timings.bt.hbackporch;
	displayport_write(SST1_VIDEO_HORIZONTAL_TOTAL_PIXELS + 0x1000 * sst_id, val);
	displayport_dbg("reg set - h total: %d", val);

	val = supported_videos[video_format].dv_timings.bt.height;
	displayport_write(SST1_VIDEO_VERTICAL_ACTIVE + 0x1000 * sst_id, val);
	displayport_dbg("reg set - v active: %d", val);

	val = supported_videos[video_format].dv_timings.bt.vfrontporch;
	displayport_write(SST1_VIDEO_VERTICAL_FRONT_PORCH + 0x1000 * sst_id, val);
	displayport_dbg("reg set - v front p: %d", val);

	val = supported_videos[video_format].dv_timings.bt.vbackporch;
	displayport_write(SST1_VIDEO_VERTICAL_BACK_PORCH + 0x1000 * sst_id, val);
	displayport_dbg("reg set - v back p: %d", val);

	val = supported_videos[video_format].dv_timings.bt.width;
	displayport_write(SST1_VIDEO_HORIZONTAL_ACTIVE + 0x1000 * sst_id, val);
	displayport_dbg("reg set - h active: %d", val);

	val = supported_videos[video_format].dv_timings.bt.hfrontporch;
	displayport_write(SST1_VIDEO_HORIZONTAL_FRONT_PORCH + 0x1000 * sst_id, val);
	displayport_dbg("reg set - h front p: %d", val);

	val = supported_videos[video_format].dv_timings.bt.hbackporch;
	displayport_write(SST1_VIDEO_HORIZONTAL_BACK_PORCH + 0x1000 * sst_id, val);
	displayport_dbg("reg set - h back p: %d", val);

	val = supported_videos[video_format].v_sync_pol;
	displayport_write_mask(SST1_VIDEO_CONTROL + 0x1000 * sst_id,
			val, VSYNC_POLARITY);
	displayport_dbg("reg set - v pol: %d", val);

	val = supported_videos[video_format].h_sync_pol;
	displayport_write_mask(SST1_VIDEO_CONTROL + 0x1000 * sst_id,
			val, HSYNC_POLARITY);
	displayport_dbg("reg set - h pol: %d", val);
}

u32 displayport_reg_get_video_clk(u32 sst_id)
{
	struct displayport_device *displayport = get_displayport_drvdata();

	return supported_videos[displayport->sst[sst_id]->cur_video].dv_timings.bt.pixelclock;
}

u32 displayport_reg_get_ls_clk(void)
{
	u32 val;
	u32 ls_clk;

	val = displayport_reg_phy_get_link_bw();

	if (val == LINK_RATE_8_1Gbps)
		ls_clk = 810000000;
	else if (val == LINK_RATE_5_4Gbps)
		ls_clk = 540000000;
	else if (val == LINK_RATE_2_7Gbps)
		ls_clk = 270000000;
	else /* LINK_RATE_1_62Gbps */
		ls_clk = 162000000;

	return ls_clk;
}

void displayport_reg_set_video_clock(u32 sst_id)
{
	u32 stream_clk = 0;
	u32 ls_clk = 0;
	u32 mvid_master = 0;
	u32 nvid_master = 0;

	stream_clk = displayport_reg_get_video_clk(sst_id) / 1000;
	ls_clk = displayport_reg_get_ls_clk() / 1000;

	mvid_master = stream_clk >> 2;
	nvid_master = ls_clk;

	displayport_write(SST1_MVID_MASTER_MODE + 0x1000 * sst_id, mvid_master);
	displayport_write(SST1_NVID_MASTER_MODE + 0x1000 * sst_id, nvid_master);

	displayport_write_mask(SST1_MAIN_CONTROL + 0x1000 * sst_id, 1, MVID_MODE);

	displayport_write(SST1_MVID_SFR_CONFIGURE + 0x1000 * sst_id, stream_clk);
	displayport_write(SST1_NVID_SFR_CONFIGURE + 0x1000 * sst_id, ls_clk);
}

void displayport_reg_set_active_symbol(u32 sst_id)
{
	u64 TU_off = 0;	/* TU Size when FEC is off*/
	u64 TU_on = 0;	/* TU Size when FEC is on*/
	u32 bpp = 0;	/* Bit Per Pixel */
	u32 lanecount = 0;
	u32 bandwidth = 0;
	u32 integer_fec_off = 0;
	u32 fraction_fec_off = 0;
	u32 threshold_fec_off = 0;
	u32 integer_fec_on = 0;
	u32 fraction_fec_on = 0;
	u32 threshold_fec_on = 0;
	u32 clk = 0;
	struct displayport_device *displayport = get_displayport_drvdata();

	displayport_write_mask(SST1_ACTIVE_SYMBOL_MODE_CONTROL + 0x1000 * sst_id,
			1, ACTIVE_SYMBOL_MODE_CONTROL);
	displayport_write_mask(SST1_ACTIVE_SYMBOL_THRESHOLD_SEL_FEC_OFF + 0x1000 * sst_id,
			1, ACTIVE_SYMBOL_THRESHOLD_SEL_FEC_OFF);
	displayport_write_mask(SST1_ACTIVE_SYMBOL_THRESHOLD_SEL_FEC_ON + 0x1000 * sst_id,
			1, ACTIVE_SYMBOL_THRESHOLD_SEL_FEC_ON);

	switch (displayport->sst[sst_id]->bpc) {
	case BPC_8:
		bpp = 24;
		break;
	case BPC_10:
		bpp = 30;
		break;
	default:
		bpp = 18;
		break;
	} /* if DSC on, bpp / 3 */

	/* change to Mbps from bps of pixel clock*/
	clk = displayport_reg_get_video_clk(sst_id) / 1000;

	bandwidth = displayport_reg_get_ls_clk() / 1000;
	lanecount = displayport_reg_get_lane_count();

	TU_off = ((clk * bpp * 32) * 10000000000) / (lanecount * bandwidth * 8);
	TU_on = (TU_off * 1000) / 976;

	integer_fec_off = (u32)(TU_off / 10000000000);
	fraction_fec_off = (u32)((TU_off - (integer_fec_off * 10000000000)) / 10);
	integer_fec_on = (u32)(TU_on / 10000000000);
	fraction_fec_on = (u32)((TU_on - (integer_fec_on * 10000000000)) / 10);

	if (integer_fec_off <= 2)
		threshold_fec_off = 7;
	else if (integer_fec_off > 2 && integer_fec_off <= 5)
		threshold_fec_off = 8;
	else if (integer_fec_off > 5)
		threshold_fec_off = 9;

	if (integer_fec_on <= 2)
		threshold_fec_on = 7;
	else if (integer_fec_on > 2 && integer_fec_on <= 5)
		threshold_fec_on = 8;
	else if (integer_fec_on > 5)
		threshold_fec_on = 9;

	displayport_info("fec_off(int: %d, frac: %d, thr: %d), fec_on(int: %d, frac: %d, thr: %d)\n",
			integer_fec_off, fraction_fec_off, threshold_fec_off,
			integer_fec_on, fraction_fec_on, threshold_fec_on);

	displayport_write_mask(SST1_ACTIVE_SYMBOL_INTEGER_FEC_OFF + 0x1000 * sst_id,
			integer_fec_off, ACTIVE_SYMBOL_INTEGER_FEC_OFF);
	displayport_write_mask(SST1_ACTIVE_SYMBOL_FRACTION_FEC_OFF + 0x1000 * sst_id,
			fraction_fec_off, ACTIVE_SYMBOL_FRACTION_FEC_OFF);
	displayport_write_mask(SST1_ACTIVE_SYMBOL_THRESHOLD_FEC_OFF + 0x1000 * sst_id,
			threshold_fec_off, ACTIVE_SYMBOL_FRACTION_FEC_OFF);

	displayport_write_mask(SST1_ACTIVE_SYMBOL_INTEGER_FEC_ON + 0x1000 * sst_id,
			integer_fec_on, ACTIVE_SYMBOL_INTEGER_FEC_ON);
	displayport_write_mask(SST1_ACTIVE_SYMBOL_FRACTION_FEC_ON + 0x1000 * sst_id,
			fraction_fec_on, ACTIVE_SYMBOL_FRACTION_FEC_OFF);
	displayport_write_mask(SST1_ACTIVE_SYMBOL_THRESHOLD_FEC_ON + 0x1000 * sst_id,
			threshold_fec_on, ACTIVE_SYMBOL_THRESHOLD_FEC_ON);
}

void displayport_reg_enable_interface_crc(u32 en)
{
	u32 val = en ? ~0 : 0;

	displayport_write_mask(SST1_STREAM_IF_CRC_CONTROL_1, val, IF_CRC_EN);
	displayport_write_mask(SST1_STREAM_IF_CRC_CONTROL_1, val, IF_CRC_SW_COMPARE);

	if (val == 0) {
		displayport_write_mask(SST1_STREAM_IF_CRC_CONTROL_1, 1, IF_CRC_CLEAR);
		displayport_write_mask(SST1_STREAM_IF_CRC_CONTROL_1, 0, IF_CRC_CLEAR);
	}
}

void displayport_reg_get_interface_crc(u32 *crc_r_result, u32 *crc_g_result, u32 *crc_b_result)
{
	*crc_r_result = displayport_read_mask(SST1_STREAM_IF_CRC_CONTROL_2, IF_CRC_R_RESULT);
	*crc_g_result = displayport_read_mask(SST1_STREAM_IF_CRC_CONTROL_3, IF_CRC_G_RESULT);
	*crc_b_result = displayport_read_mask(SST1_STREAM_IF_CRC_CONTROL_4, IF_CRC_B_RESULT);
}

void displayport_reg_enable_stand_alone_crc(u32 en)
{
	u32 val = en ? ~0 : 0;

	displayport_write_mask(PCS_SA_CRC_CONTROL_1, val,
		SA_CRC_LANE_0_ENABLE | SA_CRC_LANE_1_ENABLE |
		SA_CRC_LANE_2_ENABLE | SA_CRC_LANE_3_ENABLE);
	displayport_write_mask(PCS_SA_CRC_CONTROL_1, val, SA_CRC_SW_COMPARE);

	if (val == 0) {
		displayport_write_mask(PCS_SA_CRC_CONTROL_1, 1, SA_CRC_CLEAR);
		displayport_write_mask(PCS_SA_CRC_CONTROL_1, 0, SA_CRC_CLEAR);
	}
}

void displayport_reg_get_stand_alone_crc(u32 *ln0, u32 *ln1, u32 *ln2, u32 *ln3)
{
	*ln0 = displayport_read_mask(PCS_SA_CRC_CONTROL_2, SA_CRC_LN0_RESULT);
	*ln1 = displayport_read_mask(PCS_SA_CRC_CONTROL_3, SA_CRC_LN1_RESULT);
	*ln2 = displayport_read_mask(PCS_SA_CRC_CONTROL_4, SA_CRC_LN2_RESULT);
	*ln3 = displayport_read_mask(PCS_SA_CRC_CONTROL_5, SA_CRC_LN3_RESULT);
}

void displayport_reg_aux_ch_buf_clr(void)
{
	displayport_write_mask(AUX_BUFFER_CLEAR, 1, AUX_BUF_CLR);
}

void displayport_reg_aux_defer_ctrl(u32 en)
{
	u32 val = en ? ~0 : 0;

	displayport_write_mask(AUX_COMMAND_CONTROL, val, DEFER_CTRL_EN);
}

void displayport_reg_set_aux_reply_timeout(void)
{
	displayport_write_mask(AUX_CONTROL, AUX_TIMEOUT_1800us, AUX_REPLY_TIMER_MODE);
}

void displayport_reg_set_aux_ch_command(enum displayport_aux_ch_command_type aux_ch_mode)
{
	displayport_write_mask(AUX_REQUEST_CONTROL, aux_ch_mode, REQ_COMM);
}

void displayport_reg_set_aux_ch_address(u32 aux_ch_address)
{
	displayport_write_mask(AUX_REQUEST_CONTROL, aux_ch_address, REQ_ADDR);
}

void displayport_reg_set_aux_ch_length(u32 aux_ch_length)
{
	displayport_write_mask(AUX_REQUEST_CONTROL, aux_ch_length - 1, REQ_LENGTH);
}

void displayport_reg_aux_ch_send_buf(u8 *aux_ch_send_buf, u32 aux_ch_length)
{
	int i;

	for (i = 0; i < aux_ch_length; i++) {
		displayport_write_mask(AUX_TX_DATA_SET0 + ((i / 4) * 4),
			aux_ch_send_buf[i], (0x000000FF << ((i % 4) * 8)));
	}
}

void displayport_reg_aux_ch_received_buf(u8 *aux_ch_received_buf, u32 aux_ch_length)
{
	int i;

	for (i = 0; i < aux_ch_length; i++) {
		aux_ch_received_buf[i] =
			(displayport_read_mask(AUX_RX_DATA_SET0 + ((i / 4) * 4),
			0xFF << ((i % 4) * 8)) >> (i % 4) * 8);
	}
}

int displayport_reg_set_aux_ch_operation_enable(void)
{
	u32 cnt = 5000;
	u32 state;
	u32 val0, val1;

	displayport_write_mask(AUX_TRANSACTION_START, 1, AUX_TRAN_START);

	do {
		state = displayport_read(AUX_TRANSACTION_START) & AUX_TRAN_START;
		cnt--;
		udelay(10);
	} while (state && cnt);

	if (!cnt) {
		displayport_err("AUX_TRAN_START waiting timeout.\n");
		return -ETIME;
	}

	val0 = displayport_read(AUX_MONITOR_1);
	val1 = displayport_read(AUX_MONITOR_2);

	if ((val0 & AUX_CMD_STATUS) != 0x00 || val1 != 0x00) {
		displayport_dbg("AUX_MONITOR_1 : 0x%X, AUX_MONITOR_2 : 0x%X\n", val0, val1);
		displayport_dbg("AUX_CONTROL : 0x%X, AUX_REQUEST_CONTROL : 0x%X, AUX_COMMAND_CONTROL : 0x%X\n",
				displayport_read(AUX_CONTROL),
				displayport_read(AUX_REQUEST_CONTROL),
				displayport_read(AUX_COMMAND_CONTROL));

		usleep_range(400, 401);
		return -EIO;
	}

	return 0;
}

void displayport_reg_set_aux_ch_address_only_command(u32 en)
{
	displayport_write_mask(AUX_ADDR_ONLY_COMMAND, en, ADDR_ONLY_CMD);
}

int displayport_reg_dpcd_write(u32 address, u32 length, u8 *data)
{
	int ret;
	int retry_cnt = AUX_RETRY_COUNT;
	struct displayport_device *displayport = get_displayport_drvdata();

	mutex_lock(&displayport->aux_lock);
	while(retry_cnt > 0) {
		displayport_reg_aux_ch_buf_clr();
		displayport_reg_aux_defer_ctrl(1);
		displayport_reg_set_aux_reply_timeout();
		displayport_reg_set_aux_ch_command(DPCD_WRITE);
		displayport_reg_set_aux_ch_address(address);
		displayport_reg_set_aux_ch_length(length);
		displayport_reg_aux_ch_send_buf(data, length);
		ret = displayport_reg_set_aux_ch_operation_enable();
		if (ret == 0)
			break;

		retry_cnt--;
	}

#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	if (ret == 0)
		secdp_bigdata_clr_error_cnt(ERR_AUX);
	else if (displayport->pdic_hpd)
		secdp_bigdata_inc_error_cnt(ERR_AUX);
#endif

	mutex_unlock(&displayport->aux_lock);

	return ret;
}

int displayport_reg_dpcd_read(u32 address, u32 length, u8 *data)
{
	int ret;
	struct displayport_device *displayport = get_displayport_drvdata();
	int retry_cnt = AUX_RETRY_COUNT;

	mutex_lock(&displayport->aux_lock);
	while(retry_cnt > 0) {
		displayport_reg_set_aux_ch_command(DPCD_READ);
		displayport_reg_set_aux_ch_address(address);
		displayport_reg_set_aux_ch_length(length);
		displayport_reg_aux_ch_buf_clr();
		displayport_reg_aux_defer_ctrl(1);
		displayport_reg_set_aux_reply_timeout();
		ret = displayport_reg_set_aux_ch_operation_enable();

		if (ret == 0)
			break;
		retry_cnt--;
	}

	if (ret == 0)
		displayport_reg_aux_ch_received_buf(data, length);

#ifdef CONFIG_SEC_DISPLAYPORT_BIGDATA
	if (ret == 0)
		secdp_bigdata_clr_error_cnt(ERR_AUX);
	else if (displayport->pdic_hpd)
		secdp_bigdata_inc_error_cnt(ERR_AUX);
#endif

	mutex_unlock(&displayport->aux_lock);

	return ret;
}

int displayport_reg_dpcd_write_burst(u32 address, u32 length, u8 *data)
{
	int ret = 0;
	u32 i, buf_length, length_calculation;

	length_calculation = length;
	for (i = 0; i < length; i += AUX_DATA_BUF_COUNT) {
		if (length_calculation >= AUX_DATA_BUF_COUNT) {
			buf_length = AUX_DATA_BUF_COUNT;
			length_calculation -= AUX_DATA_BUF_COUNT;
		} else {
			buf_length = length % AUX_DATA_BUF_COUNT;
			length_calculation = 0;
		}

		ret = displayport_reg_dpcd_write(address + i, buf_length, data + i);
		if (ret != 0) {
			displayport_err("displayport_reg_dpcd_write_burst fail\n");
			break;
		}
	}

	return ret;
}

int displayport_reg_dpcd_read_burst(u32 address, u32 length, u8 *data)
{
	int ret = 0;
	u32 i, buf_length, length_calculation;

	length_calculation = length;

	for (i = 0; i < length; i += AUX_DATA_BUF_COUNT) {
		if (length_calculation >= AUX_DATA_BUF_COUNT) {
			buf_length = AUX_DATA_BUF_COUNT;
			length_calculation -= AUX_DATA_BUF_COUNT;
		} else {
			buf_length = length % AUX_DATA_BUF_COUNT;
			length_calculation = 0;
		}

		ret = displayport_reg_dpcd_read(address + i, buf_length, data + i);

		if (ret != 0) {
			displayport_err("displayport_reg_dpcd_read_burst fail\n");
			break;
		}
	}

	return ret;
}

int displayport_reg_i2c_write(u32 address, u32 length, u8 *data)
{
	int ret;
	struct displayport_device *displayport = get_displayport_drvdata();
	int retry_cnt = AUX_RETRY_COUNT;

	mutex_lock(&displayport->aux_lock);

	while (retry_cnt > 0) {
		displayport_reg_set_aux_ch_command(I2C_WRITE);
		displayport_reg_set_aux_ch_address(address);
		displayport_reg_set_aux_ch_address_only_command(1);
		ret = displayport_reg_set_aux_ch_operation_enable();
		displayport_reg_set_aux_ch_address_only_command(0);

		displayport_reg_aux_ch_buf_clr();
		displayport_reg_aux_defer_ctrl(1);
		displayport_reg_set_aux_reply_timeout();
		displayport_reg_set_aux_ch_address_only_command(0);
		displayport_reg_set_aux_ch_command(I2C_WRITE);
		displayport_reg_set_aux_ch_address(address);
		displayport_reg_set_aux_ch_length(length);
		displayport_reg_aux_ch_send_buf(data, length);
		ret = displayport_reg_set_aux_ch_operation_enable();

		if (ret == 0) {
			displayport_reg_set_aux_ch_command(I2C_WRITE);
			displayport_reg_set_aux_ch_address(EDID_ADDRESS);
			displayport_reg_set_aux_ch_address_only_command(1);
			ret = displayport_reg_set_aux_ch_operation_enable();
			displayport_reg_set_aux_ch_address_only_command(0);
			displayport_dbg("address only request in i2c write\n");
		}

		if (ret == 0)
			break;

		retry_cnt--;
	}

	mutex_unlock(&displayport->aux_lock);

	return ret;
}

int displayport_reg_i2c_read(u32 address, u32 length, u8 *data)
{
	int ret;
	struct displayport_device *displayport = get_displayport_drvdata();
	int retry_cnt = AUX_RETRY_COUNT;

	mutex_lock(&displayport->aux_lock);
	while (retry_cnt > 0) {
		displayport_reg_set_aux_ch_command(I2C_READ);
		displayport_reg_set_aux_ch_address(address);
		displayport_reg_set_aux_ch_length(length);
		displayport_reg_aux_ch_buf_clr();
		displayport_reg_aux_defer_ctrl(1);
		displayport_reg_set_aux_reply_timeout();
		ret = displayport_reg_set_aux_ch_operation_enable();

		if (ret == 0)
			break;
		retry_cnt--;
	}

	if (ret == 0)
		displayport_reg_aux_ch_received_buf(data, length);

	mutex_unlock(&displayport->aux_lock);

	return ret;
}

int displayport_reg_edid_write(u8 edid_addr_offset, u32 length, u8 *data)
{
	u32 i, buf_length, length_calculation;
	int ret;
	int retry_cnt = AUX_RETRY_COUNT;

	while(retry_cnt > 0) {
		displayport_reg_aux_ch_buf_clr();
		displayport_reg_aux_defer_ctrl(1);
		displayport_reg_set_aux_reply_timeout();
		displayport_reg_set_aux_ch_command(I2C_WRITE);
		displayport_reg_set_aux_ch_address(EDID_ADDRESS);
		displayport_reg_set_aux_ch_length(1);
		displayport_reg_aux_ch_send_buf(&edid_addr_offset, 1);
		ret = displayport_reg_set_aux_ch_operation_enable();

		if (ret == 0) {
			length_calculation = length;


			/*	displayport_write_mask(AUX_Ch_MISC_Ctrl_1, 0x3, 3 << 6); */
			for (i = 0; i < length; i += AUX_DATA_BUF_COUNT) {
				if (length_calculation >= AUX_DATA_BUF_COUNT) {
					buf_length = AUX_DATA_BUF_COUNT;
					length_calculation -= AUX_DATA_BUF_COUNT;
				} else {
					buf_length = length%AUX_DATA_BUF_COUNT;
					length_calculation = 0;
				}

				displayport_reg_set_aux_ch_length(buf_length);
				displayport_reg_aux_ch_send_buf(data+((i/AUX_DATA_BUF_COUNT)*AUX_DATA_BUF_COUNT), buf_length);
				ret = displayport_reg_set_aux_ch_operation_enable();

				if (ret == 0)
					break;
			}
		}

		if (ret == 0) {
			displayport_reg_set_aux_ch_address_only_command(1);
			ret = displayport_reg_set_aux_ch_operation_enable();
			displayport_reg_set_aux_ch_address_only_command(0);
		}
		if (ret == 0)
			break;

		retry_cnt--;
	}


	return ret;
}

#define DDC_SEGMENT_ADDR 0x30
int displayport_reg_edid_read(u8 block_cnt, u32 length, u8 *data)
{
	u32 i, buf_length, length_calculation;
	int ret;
	struct displayport_device *displayport = get_displayport_drvdata();
	int retry_cnt = AUX_RETRY_COUNT;
	u8 offset = (block_cnt & 1) * EDID_BLOCK_SIZE;

	mutex_lock(&displayport->aux_lock);

	while(retry_cnt > 0) {
		/* for 3rd,4th block */
		if (block_cnt > 1) {
			u8 segment = 1;

			displayport_reg_aux_ch_buf_clr();
			displayport_reg_aux_defer_ctrl(1);
			displayport_reg_set_aux_reply_timeout();
			displayport_reg_set_aux_ch_address_only_command(0);
			displayport_dbg("read block%d\n", block_cnt);
			displayport_reg_set_aux_ch_command(I2C_WRITE);
			displayport_reg_set_aux_ch_address(DDC_SEGMENT_ADDR);
			displayport_reg_set_aux_ch_length(1);
			displayport_reg_aux_ch_send_buf(&segment, 1);
			ret = displayport_reg_set_aux_ch_operation_enable();
			if (ret)
				displayport_info("sending segment failed\n");
		}

		displayport_reg_set_aux_ch_command(I2C_WRITE);
		displayport_reg_set_aux_ch_address(EDID_ADDRESS);
		displayport_reg_set_aux_ch_address_only_command(1);
		ret = displayport_reg_set_aux_ch_operation_enable();
		displayport_reg_set_aux_ch_address_only_command(0);
		displayport_dbg("1st address only request in EDID read\n");

		displayport_reg_aux_ch_buf_clr();
		displayport_reg_aux_defer_ctrl(1);
		displayport_reg_set_aux_reply_timeout();
		displayport_reg_set_aux_ch_address_only_command(0);
		displayport_reg_set_aux_ch_command(I2C_WRITE);
		displayport_reg_set_aux_ch_address(EDID_ADDRESS);
		displayport_reg_set_aux_ch_length(1);
		displayport_reg_aux_ch_send_buf(&offset, 1);
		ret = displayport_reg_set_aux_ch_operation_enable();

		displayport_dbg("EDID address command in EDID read\n");

		if (ret == 0) {
			displayport_reg_set_aux_ch_command(I2C_READ);
			length_calculation = length;

			for (i = 0; i < length; i += AUX_DATA_BUF_COUNT) {
				if (length_calculation >= AUX_DATA_BUF_COUNT) {
					buf_length = AUX_DATA_BUF_COUNT;
					length_calculation -= AUX_DATA_BUF_COUNT;
				} else {
					buf_length = length%AUX_DATA_BUF_COUNT;
					length_calculation = 0;
				}

				displayport_reg_set_aux_ch_length(buf_length);
				displayport_reg_aux_ch_buf_clr();
				ret = displayport_reg_set_aux_ch_operation_enable();

				if (ret == 0) {
					displayport_reg_aux_ch_received_buf(data+((i/AUX_DATA_BUF_COUNT)*AUX_DATA_BUF_COUNT), buf_length);
					displayport_dbg("AUX buffer read count = %d in EDID read\n", i);
				} else {
					displayport_dbg("AUX buffer read fail in EDID read\n");
					break;
				}
			}
		}

		if (ret == 0) {
			displayport_reg_set_aux_ch_command(I2C_WRITE);
			displayport_reg_set_aux_ch_address(EDID_ADDRESS);
			displayport_reg_set_aux_ch_address_only_command(1);
			ret = displayport_reg_set_aux_ch_operation_enable();
			displayport_reg_set_aux_ch_address_only_command(0);

			displayport_dbg("2nd address only request in EDID read\n");
		}

		if (ret == 0)
			break;

		retry_cnt--;
	}

	mutex_unlock(&displayport->aux_lock);

	return ret;
}

void displayport_reg_set_lane_map(u32 lane0, u32 lane1, u32 lane2, u32 lane3)
{
	displayport_write_mask(PCS_LANE_CONTROL, lane0, LANE0_MAP);
	displayport_write_mask(PCS_LANE_CONTROL, lane1, LANE1_MAP);
	displayport_write_mask(PCS_LANE_CONTROL, lane2, LANE2_MAP);
	displayport_write_mask(PCS_LANE_CONTROL, lane3, LANE3_MAP);
}

void displayport_reg_set_lane_map_config(void)
{
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct displayport_device *displayport = get_displayport_drvdata();

	switch (displayport->pdic_notify_dp_conf) {
	case PDIC_NOTIFY_DP_PIN_UNKNOWN:
		break;

	case PDIC_NOTIFY_DP_PIN_A:
		if (displayport->dp_sw_sel)
			displayport_reg_set_lane_map(3, 1, 2, 0);
		else
			displayport_reg_set_lane_map(2, 0, 3, 1);
		break;

	case PDIC_NOTIFY_DP_PIN_B:
		if (displayport->dp_sw_sel)
			displayport_reg_set_lane_map(3, 2, 1, 0);
		else
			displayport_reg_set_lane_map(1, 0, 2, 3);
		break;

	case PDIC_NOTIFY_DP_PIN_C:
	case PDIC_NOTIFY_DP_PIN_E:
	case PDIC_NOTIFY_DP_PIN_D:
	case PDIC_NOTIFY_DP_PIN_F:
		if (displayport->dp_sw_sel)
			displayport_reg_set_lane_map(3, 2, 0, 1);
		else
			displayport_reg_set_lane_map(0, 1, 3, 2);
		break;

	default:
		break;
	}
#endif
}

void displayport_reg_lh_p_ch_power(u32 sst_id, u32 en)
{
	u32 cnt = 20 * 1000;	/* wait 20ms */
	u32 state;
	u32 reg_offset = 0;

	switch (sst_id) {
	case SST1:
		reg_offset = SYSTEM_SST1_FUNCTION_ENABLE;
		break;
	case SST2:
		reg_offset = SYSTEM_SST2_FUNCTION_ENABLE;
		break;
	default:
		reg_offset = SYSTEM_SST1_FUNCTION_ENABLE;
	}

	if (en) {
		displayport_write_mask(reg_offset, 1, SST1_LH_PWR_ON);
		do {
			state = displayport_read_mask(reg_offset, SST1_LH_PWR_ON_STATUS);
			cnt--;
			udelay(1);
		} while (!state && cnt);

		if (!cnt)
			displayport_err("%s on is timeout[%d].\n", __func__, state);
	} else {
		displayport_write_mask(reg_offset, 0, SST1_LH_PWR_ON);
		do {
			state = displayport_read_mask(reg_offset, SST1_LH_PWR_ON_STATUS);
			cnt--;
			udelay(1);
		} while (state && cnt);

		if (!cnt) {
			displayport_err("SYSTEM_CLK_CONTROL[0x%08x]\n",
					displayport_read(SYSTEM_CLK_CONTROL));
			displayport_err("SYSTEM_PLL_LOCK_CONTROL[0x%08x]\n",
					displayport_read(SYSTEM_PLL_LOCK_CONTROL));
			displayport_err("SYSTEM_DEBUG[0x%08x]\n",
					displayport_read(SYSTEM_DEBUG));
			displayport_err("SYSTEM_DEBUG_LH_PCH[0x%08x]\n",
					displayport_read(SYSTEM_DEBUG_LH_PCH));
			displayport_err("SST%d_VIDEO_CONTROL[0x%08x]\n", sst_id + 1,
					displayport_read(SST1_VIDEO_CONTROL + 0x1000 * sst_id));
			displayport_err("SST%d_VIDEO_DEBUG_FSM_STATE[0x%08x]\n", sst_id + 1,
					displayport_read(SST1_VIDEO_DEBUG_FSM_STATE + 0x1000 * sst_id));
			displayport_err("SST%d_VIDEO_DEBUG_MAPI[0x%08x]\n", sst_id + 1,
					displayport_read(SST1_VIDEO_DEBUG_MAPI + 0x1000 * sst_id));
			displayport_err("SYSTEM_SW_FUNCTION_ENABLE[0x%08x]\n",
					displayport_read(SYSTEM_SW_FUNCTION_ENABLE));
			displayport_err("SYSTEM_COMMON_FUNCTION_ENABLE[0x%08x]\n",
					displayport_read(SYSTEM_COMMON_FUNCTION_ENABLE));
			displayport_err("SYSTEM_SST1_FUNCTION_ENABLE[0x%08x]\n",
					displayport_read(SYSTEM_SST1_FUNCTION_ENABLE));
			displayport_err("SYSTEM_SST2_FUNCTION_ENABLE[0x%08x]\n",
					displayport_read(SYSTEM_SST2_FUNCTION_ENABLE));
		}
	}
}

void displayport_reg_sw_function_en(u32 en)
{
	if (en)
		displayport_write_mask(SYSTEM_SW_FUNCTION_ENABLE, 1, SW_FUNC_EN);
	else
		displayport_write_mask(SYSTEM_SW_FUNCTION_ENABLE, 0, SW_FUNC_EN);
}

void displayport_reg_phy_init(void)
{
#if defined(CONFIG_PHY_SAMSUNG_USB_CAL)
	dwc3_exynos_phy_enable(1, 1);
#endif
	displayport_reg_phy_reset(1);
	displayport_reg_phy_init_setting();
	displayport_reg_phy_mode_setting();
	displayport_reg_phy_ssc_enable(0);
	displayport_reg_phy_reset(0);
	displayport_reg_wait_phy_pll_lock();
}

void displayport_reg_phy_disable(void)
{
	displayport_reg_phy_reset(1);

#if defined(CONFIG_PHY_SAMSUNG_USB_CAL)
	exynos_usbdrd_inform_dp_use(0, displayport_reg_get_lane_count());
	dwc3_exynos_phy_enable(1, 0);
#endif
}

void displayport_reg_init(void)
{
	displayport_info("%s\n", __func__);
	displayport_reg_phy_init();
	displayport_reg_function_enable();
	displayport_reg_sw_function_en(1);
	displayport_reg_set_lane_map_config();
	displayport_reg_set_interrupt(1);
}

void displayport_reg_deinit(void)
{
	int i = 0;

	for (i = SST1; i < MAX_SST_CNT; i++)
		displayport_reg_set_sst_stream_enable(i, 0);

	displayport_reg_set_interrupt(0);
	displayport_reg_sw_function_en(0);
}

void displayport_reg_set_video_configuration(u32 sst_id,
		videoformat video_format, u8 bpc, u8 range)
{
	displayport_info("SST%d, color range: %d, bpc: %d\n", sst_id + 1, range, bpc);
	displayport_reg_set_daynamic_range(sst_id, (range)?CEA_RANGE:VESA_RANGE);
	displayport_write_mask(SST1_VIDEO_CONTROL + 0x1000 * sst_id, bpc, BPC);	/* 0 : 6bits, 1 : 8bits */
	displayport_write_mask(SST1_VIDEO_CONTROL + 0x1000 * sst_id, 0, COLOR_FORMAT);	/* RGB */
	displayport_reg_video_format_register_setting(sst_id, video_format);
	displayport_reg_set_video_clock(sst_id);
	displayport_reg_set_active_symbol(sst_id);
	displayport_write_mask(SST1_VIDEO_MASTER_TIMING_GEN + 0x1000 * sst_id,
			1, VIDEO_MASTER_TIME_GEN);
	displayport_write_mask(SST1_MAIN_CONTROL + 0x1000 * sst_id,
			0, VIDEO_MODE);
}

void displayport_reg_set_bist_video_configuration(u32 sst_id,
		videoformat video_format, u8 bpc, u8 type, u8 range)
{
	if (type < CTS_COLOR_RAMP) {
		displayport_reg_set_video_configuration(sst_id, video_format, bpc, range);
		displayport_write_mask(SST1_VIDEO_BIST_CONTROL + 0x1000 * sst_id, type, BIST_TYPE);
		displayport_write_mask(SST1_VIDEO_BIST_CONTROL + 0x1000 * sst_id, 0, CTS_BIST_EN);
	} else {
		if (type == CTS_COLOR_SQUARE_CEA)
			displayport_reg_set_video_configuration(sst_id, video_format, bpc, CEA_RANGE);
		else
			displayport_reg_set_video_configuration(sst_id, video_format, bpc, VESA_RANGE);

		displayport_write_mask(SST1_VIDEO_BIST_CONTROL + 0x1000 * sst_id,
				type - CTS_COLOR_RAMP, CTS_BIST_TYPE);
		displayport_write_mask(SST1_VIDEO_BIST_CONTROL + 0x1000 * sst_id,
				1, CTS_BIST_EN);
	}

	displayport_reg_set_video_bist_mode(sst_id, 1);

	displayport_info("SST%d set bist video config format:%d range:%d bpc:%d type:%d\n",
			sst_id + 1, video_format, (range)?1:0, (bpc)?1:0, type);
}

void displayport_reg_set_bist_video_configuration_for_blue_screen(u32 sst_id,
		videoformat video_format)
{
	displayport_reg_set_video_configuration(sst_id, video_format, BPC_8, CEA_RANGE); /* 8 bits */
	displayport_write(SST1_VIDEO_BIST_USER_DATA_R + 0x1000 * sst_id, 0x00);
	displayport_write(SST1_VIDEO_BIST_USER_DATA_G + 0x1000 * sst_id, 0x00);
	displayport_write(SST1_VIDEO_BIST_USER_DATA_B + 0x1000 * sst_id, 0xFF);
	displayport_write_mask(SST1_VIDEO_BIST_CONTROL + 0x1000 * sst_id, 1, BIST_USER_DATA_EN);
	displayport_write_mask(SST1_VIDEO_BIST_CONTROL + 0x1000 * sst_id, 0, CTS_BIST_EN);
	displayport_reg_set_video_bist_mode(sst_id, 1);

	displayport_dbg("SST%dset bist video config for blue screen\n", sst_id + 1);
}

void displayport_reg_set_avi_infoframe(u32 sst_id, struct infoframe avi_infoframe)
{
	u32 avi_infoframe_data = 0;

	avi_infoframe_data = ((u32)avi_infoframe.data[3] << 24) | ((u32)avi_infoframe.data[2] << 16)
			| ((u32)avi_infoframe.data[1] << 8) | (u32)avi_infoframe.data[0];
	displayport_write(SST1_INFOFRAME_AVI_PACKET_DATA_SET0 + 0x1000 * sst_id,
			avi_infoframe_data);

	avi_infoframe_data = ((u32)avi_infoframe.data[7] << 24) | ((u32)avi_infoframe.data[6] << 16)
			| ((u32)avi_infoframe.data[5] << 8) | (u32)avi_infoframe.data[4];
	displayport_write(SST1_INFOFRAME_AVI_PACKET_DATA_SET1 + 0x1000 * sst_id,
			avi_infoframe_data);

	avi_infoframe_data = ((u32)avi_infoframe.data[11] << 24) | ((u32)avi_infoframe.data[10] << 16)
			| ((u32)avi_infoframe.data[9] << 8) | (u32)avi_infoframe.data[8];
	displayport_write(SST1_INFOFRAME_AVI_PACKET_DATA_SET2 + 0x1000 * sst_id,
			avi_infoframe_data);

	avi_infoframe_data = (u32)avi_infoframe.data[12];
	displayport_write(SST1_INFOFRAME_AVI_PACKET_DATA_SET3 + 0x1000 * sst_id,
			avi_infoframe_data);

	displayport_write_mask(SST1_INFOFRAME_UPDATE_CONTROL + 0x1000 * sst_id,
			1, AVI_INFO_UPDATE);
	displayport_write_mask(SST1_INFOFRAME_SEND_CONTROL + 0x1000 * sst_id,
			1, AVI_INFO_SEND);
}

void displayport_reg_set_spd_infoframe(u32 sst_id, struct infoframe spd_infoframe)
{
	int i, j;
	int data_ind = 0;
	u32 spd_infoframe_data;

	displayport_write(SST1_INFOFRAME_SPD_PACKET_TYPE + 0x1000 * sst_id,
			spd_infoframe.type_code);

	for (i = 0; i < 24; i += 4) {
		spd_infoframe_data = 0;

		for (j = 0; j < 32; j += 8)
			spd_infoframe_data |= spd_infoframe.data[data_ind++] << j;

		displayport_write(SST1_INFOFRAME_SPD_PACKET_DATA_SET0 + 0x1000 * sst_id + i,
				spd_infoframe_data);
	}

	displayport_write(SST1_INFOFRAME_SPD_PACKET_DATA_SET6 + 0x1000 * sst_id,
			spd_infoframe.data[24]);

	displayport_write_mask(SST1_INFOFRAME_UPDATE_CONTROL + 0x1000 * sst_id,
			1, SPD_INFO_UPDATE);
	displayport_write_mask(SST1_INFOFRAME_SEND_CONTROL + 0x1000 * sst_id,
			1, SPD_INFO_SEND);
}

void displayport_reg_set_audio_infoframe(u32 sst_id, struct infoframe audio_infoframe, u32 en)
{
	u32 audio_infoframe_data = 0;

	audio_infoframe_data = ((u32)audio_infoframe.data[3] << 24) | ((u32)audio_infoframe.data[2] << 16)
			| ((u32)audio_infoframe.data[1] << 8) | (u32)audio_infoframe.data[0];
	displayport_write(SST1_INFOFRAME_AUDIO_PACKET_DATA_SET0 + 0x1000 * sst_id,
			audio_infoframe_data);

	audio_infoframe_data = ((u32)audio_infoframe.data[7] << 24) | ((u32)audio_infoframe.data[6] << 16)
			| ((u32)audio_infoframe.data[5] << 8) | (u32)audio_infoframe.data[4];
	displayport_write(SST1_INFOFRAME_AUDIO_PACKET_DATA_SET1 + 0x1000 * sst_id,
			audio_infoframe_data);

	audio_infoframe_data = ((u32)audio_infoframe.data[9] << 8) | (u32)audio_infoframe.data[8];
	displayport_write(SST1_INFOFRAME_AUDIO_PACKET_DATA_SET2 + 0x1000 * sst_id,
			audio_infoframe_data);

	displayport_write_mask(SST1_INFOFRAME_UPDATE_CONTROL + 0x1000 * sst_id,
			en, AUDIO_INFO_UPDATE);
	displayport_write_mask(SST1_INFOFRAME_SEND_CONTROL + 0x1000 * sst_id,
			en, AUDIO_INFO_SEND);
}

void displayport_reg_set_hdr_infoframe(u32 sst_id, struct infoframe hdr_infoframe, u32 en)
{
	int i, j;
	u32 hdr_infoframe_data = 0;

	if (en == 1) {
		for (i = 0; i < HDR_INFOFRAME_LENGTH; i++) {
			for (j = 0; j < DATA_NUM_PER_REG; j++) {
				hdr_infoframe_data |=
					(u32)hdr_infoframe.data[i]
					<< ((j % DATA_NUM_PER_REG) * INFOFRAME_DATA_SIZE);

				if (j < DATA_NUM_PER_REG - 1)
					i++;

				if (i >= HDR_INFOFRAME_LENGTH)
					break;
			}

			displayport_write((SST1_HDR_PACKET_DATA_SET_0 +
				i / DATA_NUM_PER_REG * DATA_NUM_PER_REG) + 0x1000 * sst_id,
				hdr_infoframe_data);

			hdr_infoframe_data = 0;
		}
	}

	for (i = 0; i <= SST1_HDR_PACKET_DATA_SET_7 - SST1_HDR_PACKET_DATA_SET_0;
		i += DATA_NUM_PER_REG) {
		displayport_dbg("SST%d_HDR_PACKET_DATA_SET_%d = 0x%x",
			sst_id,
			i / DATA_NUM_PER_REG,
			displayport_read(SST1_HDR_PACKET_DATA_SET_0 + i + 0x1000 * sst_id));
	}

	displayport_write_mask(SST1_INFOFRAME_UPDATE_CONTROL + 0x1000 * sst_id,
			en, HDR_INFO_UPDATE);
	displayport_write_mask(SST1_INFOFRAME_SEND_CONTROL + 0x1000 * sst_id,
			en, HDR_INFO_SEND);
}

void displayport_reg_start(u32 sst_id)
{
	displayport_reg_set_sst_stream_enable(sst_id, 1);
	displayport_reg_set_sst_interrupt_mask(sst_id, VIDEO_FIFO_UNDER_FLOW_MASK, 1);
	displayport_write_mask(SST1_VIDEO_ENABLE + 0x1000 * sst_id, 1, VIDEO_EN);
}

void displayport_reg_video_mute(u32 en)
{
/*	displayport_dbg("set mute %d\n", en);
	displayport_write_mask(SST1_VIDEO_MUTE, en, VIDEO_MUTE);
	displayport_write_mask(SST2_VIDEO_MUTE, en, VIDEO_MUTE);
 */
}

void displayport_reg_stop(u32 sst_id)
{
	displayport_reg_set_sst_interrupt_mask(sst_id, VIDEO_FIFO_UNDER_FLOW_MASK, 0);
	displayport_write_mask(SST1_VIDEO_ENABLE + 0x1000 * sst_id, 0, VIDEO_EN);
}

/* Set SA CRC, For Sorting Vector */
void displayport_reg_set_stand_alone_crc(u32 crc_ln0_ref, u32 crc_ln1_ref, u32 crc_ln2_ref, u32 crc_ln3_ref)
{
	displayport_write_mask(PCS_SA_CRC_CONTROL_2, crc_ln0_ref, SA_CRC_LN0_REF);
	displayport_write_mask(PCS_SA_CRC_CONTROL_3, crc_ln1_ref, SA_CRC_LN1_REF);
	displayport_write_mask(PCS_SA_CRC_CONTROL_4, crc_ln2_ref, SA_CRC_LN2_REF);
	displayport_write_mask(PCS_SA_CRC_CONTROL_5, crc_ln3_ref, SA_CRC_LN3_REF);
}

void displayport_reg_set_result_flag_clear(void)
{
	displayport_write_mask(PCS_SA_CRC_CONTROL_1, 1, SA_CRC_CLEAR);
	displayport_write_mask(PCS_SA_CRC_CONTROL_1, 0, SA_CRC_CLEAR);
}

void displayport_reg_enable_stand_alone_crc_hw(u32 en)
{
	u32 val = en ? ~0 : 0;

	displayport_write_mask(PCS_SA_CRC_CONTROL_1, 0, SA_CRC_SW_COMPARE);	/* use H/W compare */

	displayport_write_mask(PCS_SA_CRC_CONTROL_1, val,
		SA_CRC_LANE_0_ENABLE | SA_CRC_LANE_1_ENABLE | SA_CRC_LANE_2_ENABLE | SA_CRC_LANE_3_ENABLE);
}

int displayport_reg_get_stand_alone_crc_result(void)
{
	u32 val;
	int err = 0;

	val = displayport_read_mask(PCS_SA_CRC_CONTROL_1, 0x00000FF0);
	val = val >> 4;

	if (val == 0xF0) {
		displayport_info("DisplayPort SA CRC Pass !!!\n");
	} else {
		err = -1;
		displayport_info("DisplayPort SA CRC Fail : 0x%02X !!!\n", val);
	}

	return  err;
}

/* SA CRC Condition : 8bpc, 4lane, 640x10 size, BIST_TYPE=0, BIST_WIDTH =0 */
int displayport_reg_stand_alone_crc_sorting(void)
{
	int ret;

	displayport_reg_init();
	displayport_reg_set_lane_count(4);
	displayport_reg_set_bist_video_configuration(SST1,
			V640X10P60SACRC, BPC_8, COLOR_BAR, VESA_RANGE);
	displayport_reg_set_stand_alone_crc(0x135E, 0x135E, 0x135E, 0x135E);
	displayport_reg_enable_stand_alone_crc_hw(1);
	displayport_reg_start(SST1);

	msleep(20);

	displayport_reg_set_result_flag_clear();

	msleep(20);

	ret =  displayport_reg_get_stand_alone_crc_result();

	displayport_reg_set_result_flag_clear();
	displayport_reg_enable_stand_alone_crc_hw(0);

	displayport_reg_set_video_bist_mode(SST1, 0);
	displayport_reg_stop(SST1);

	return ret;
}

void displayport_reg_set_audio_m_n(u32 sst_id,
		audio_sync_mode audio_sync_mode, enum audio_sampling_frequency audio_sampling_freq)
{
	u32 link_bandwidth_set;
	u32 array_set;
	u32 m_value;
	u32 n_value;

	link_bandwidth_set = displayport_reg_phy_get_link_bw();
	if (link_bandwidth_set == LINK_RATE_1_62Gbps)
		array_set = 0;
	else if (link_bandwidth_set == LINK_RATE_2_7Gbps)
		array_set = 1;
	else/* if (link_bandwidth_set == LINK_RATE_5_4Gbps)*/
		array_set = 2;

	if (audio_sync_mode == ASYNC_MODE) {
		m_value = audio_async_m_n[0][array_set][audio_sampling_freq];
		n_value = audio_async_m_n[1][array_set][audio_sampling_freq];
		displayport_write_mask(SST1_MAIN_CONTROL + 0x1000 * sst_id, 0, MAUD_MODE);
	} else {
		m_value = audio_sync_m_n[0][array_set][audio_sampling_freq];
		n_value = audio_sync_m_n[1][array_set][audio_sampling_freq];
		displayport_write_mask(SST1_MAIN_CONTROL + 0x1000 * sst_id, 1, MAUD_MODE);
	}

	displayport_write(SST1_MAUD_SFR_CONFIGURE + 0x1000 * sst_id, m_value);
	displayport_write(SST1_NAUD_SFR_CONFIGURE + 0x1000 * sst_id, n_value);
}

void displayport_reg_set_audio_function_enable(u32 sst_id, u32 en)
{
	u32 reg_offset = 0;

	switch (sst_id) {
	case SST1:
		reg_offset = SYSTEM_SST1_FUNCTION_ENABLE;
		break;
	case SST2:
		reg_offset = SYSTEM_SST2_FUNCTION_ENABLE;
		break;
	default:
		reg_offset = SYSTEM_SST1_FUNCTION_ENABLE;
	}

	displayport_write_mask(reg_offset, en, SST1_AUDIO_FUNC_EN);
}

void displayport_reg_set_init_dma_config(u32 sst_id)
{
	displayport_write_mask(SST1_AUDIO_CONTROL + 0x1000 * sst_id,
			1, AUD_DMA_IF_MODE_CONFIG);
	displayport_write_mask(SST1_AUDIO_CONTROL + 0x1000 * sst_id,
			0, AUD_DMA_IF_LTNCY_TRG_MODE);
}

void displayport_reg_set_dma_force_req_low(u32 sst_id, u32 en)
{
	if (en == 1) {
		displayport_write_mask(SST1_AUDIO_DMA_REQUEST_LATENCY_CONFIG + 0x1000 * sst_id,
				0, AUD_DMA_FORCE_REQ_VAL);
		displayport_write_mask(SST1_AUDIO_DMA_REQUEST_LATENCY_CONFIG + 0x1000 * sst_id,
				1, AUD_DMA_FORCE_REQ_SEL);
	} else
		displayport_write_mask(SST1_AUDIO_DMA_REQUEST_LATENCY_CONFIG + 0x1000 * sst_id,
				0, AUD_DMA_FORCE_REQ_SEL);

	displayport_info("SST%d_AUDIO_DMA_REQUEST_LATENCY_CONFIG = 0x%x\n", sst_id + 1,
			displayport_read(SST1_AUDIO_DMA_REQUEST_LATENCY_CONFIG + 0x1000 * sst_id));
}

void displayport_reg_set_dma_burst_size(u32 sst_id,
		enum audio_dma_word_length word_length)
{
	displayport_write_mask(SST1_AUDIO_CONTROL + 0x1000 * sst_id,
			word_length, DMA_BURST_SEL);
}

void displayport_reg_set_dma_pack_mode(u32 sst_id,
		enum audio_16bit_dma_mode dma_mode)
{
	displayport_write_mask(SST1_AUDIO_CONTROL + 0x1000 * sst_id,
			dma_mode, AUDIO_BIT_MAPPING_TYPE);
}

void displayport_reg_set_pcm_size(u32 sst_id,
		enum audio_bit_per_channel audio_bit_size)
{
	displayport_write_mask(SST1_AUDIO_CONTROL + 0x1000 * sst_id,
			audio_bit_size, PCM_SIZE);
}

void displayport_reg_set_audio_ch_status_same(u32 sst_id, u32 en)
{
	displayport_write_mask(SST1_AUDIO_CONTROL + 0x1000 * sst_id,
			en, AUDIO_CH_STATUS_SAME);
}

void displayport_reg_set_audio_ch(u32 sst_id, u32 audio_ch_cnt)
{
	displayport_write_mask(SST1_AUDIO_BUFFER_CONTROL + 0x1000 * sst_id,
				audio_ch_cnt - 1, MASTER_AUDIO_CHANNEL_COUNT);
}

void displayport_reg_set_audio_ch_mapping(u32 sst_id,
		u8 pkt_1, u8 pkt_2, u8 pkt_3, u8 pkt_4,
		u8 pkt_5, u8 pkt_6, u8 pkt_7, u8 pkt_8)
{
	displayport_write_mask(SST1_AUDIO_CHANNEL_1_4_REMAP + 0x1000 * sst_id,
			pkt_1, AUD_CH_01_REMAP);
	displayport_write_mask(SST1_AUDIO_CHANNEL_1_4_REMAP + 0x1000 * sst_id,
			pkt_2, AUD_CH_02_REMAP);
	displayport_write_mask(SST1_AUDIO_CHANNEL_1_4_REMAP + 0x1000 * sst_id,
			pkt_3, AUD_CH_03_REMAP);
	displayport_write_mask(SST1_AUDIO_CHANNEL_1_4_REMAP + 0x1000 * sst_id,
			pkt_4, AUD_CH_04_REMAP);

	displayport_write_mask(SST1_AUDIO_CHANNEL_5_8_REMAP + 0x1000 * sst_id,
			pkt_5, AUD_CH_05_REMAP);
	displayport_write_mask(SST1_AUDIO_CHANNEL_5_8_REMAP + 0x1000 * sst_id,
			pkt_6, AUD_CH_06_REMAP);
	displayport_write_mask(SST1_AUDIO_CHANNEL_5_8_REMAP + 0x1000 * sst_id,
			pkt_7, AUD_CH_07_REMAP);
	displayport_write_mask(SST1_AUDIO_CHANNEL_5_8_REMAP + 0x1000 * sst_id,
			pkt_8, AUD_CH_08_REMAP);

	displayport_dbg("SST%d audio 1~4 channel mapping = 0x%X\n", sst_id + 1,
			displayport_read(SST1_AUDIO_CHANNEL_1_4_REMAP + 0x1000 * sst_id));
	displayport_dbg("SST%d audio 5~8 channel mapping = 0x%X\n", sst_id + 1,
			displayport_read(SST1_AUDIO_CHANNEL_5_8_REMAP + 0x1000 * sst_id));
}

void displayport_reg_set_audio_fifo_function_enable(u32 sst_id, u32 en)
{
	u32 reg_offset = 0;

	switch (sst_id) {
	case SST1:
		reg_offset = SYSTEM_SST1_FUNCTION_ENABLE;
		break;
	case SST2:
		reg_offset = SYSTEM_SST2_FUNCTION_ENABLE;
		break;
	default:
		reg_offset = SYSTEM_SST1_FUNCTION_ENABLE;
	}

	displayport_write_mask(reg_offset, en, SST1_AUDIO_FIFO_FUNC_EN);
}

void displayport_reg_set_audio_sampling_frequency(u32 sst_id,
		enum audio_sampling_frequency audio_sampling_freq)
{
	u32 link_bandwidth_set;
	u32 n_aud_master_set;

	link_bandwidth_set = displayport_reg_phy_get_link_bw();
	if (link_bandwidth_set == LINK_RATE_1_62Gbps)
		n_aud_master_set = 0;
	else if (link_bandwidth_set == LINK_RATE_2_7Gbps)
		n_aud_master_set = 1;
	else/* if (link_bandwidth_set == LINK_RATE_5_4Gbps)*/
		n_aud_master_set = 2;

	displayport_write(SST1_MAUD_MASTER_MODE + 0x1000 * sst_id, m_aud_master[audio_sampling_freq]);
	displayport_write(SST1_NAUD_MASTER_MODE + 0x1000 * sst_id, n_aud_master[n_aud_master_set]);
}

void displayport_reg_set_dp_audio_enable(u32 sst_id, u32 en)
{
	u32 val = en ? ~0 : 0;

	displayport_write_mask(SST1_AUDIO_ENABLE + 0x1000 * sst_id, val, AUDIO_EN);
}

void displayport_reg_set_audio_master_mode_enable(u32 sst_id, u32 en)
{
	u32 val = en ? ~0 : 0;

	displayport_write_mask(SST1_AUDIO_MASTER_TIMING_GEN + 0x1000 * sst_id,
			val, AUDIO_MASTER_TIME_GEN);
}

void displayport_reg_set_ch_status_ch_cnt(u32 sst_id, u32 audio_ch_cnt)
{
	displayport_write_mask(SST1_AUDIO_BIST_CHANNEL_STATUS_SET0 + 0x1000 * sst_id,
				audio_ch_cnt, CH_NUM);

	displayport_write_mask(SST1_AUDIO_BIST_CHANNEL_STATUS_SET0 + 0x1000 * sst_id,
				audio_ch_cnt, SOURCE_NUM);

	displayport_write_mask(SST1_AUDIO_CHANNEL_1_2_STATUS_CTRL_0 + 0x1000 * sst_id,
				audio_ch_cnt, CH_NUM);

	displayport_write_mask(SST1_AUDIO_CHANNEL_1_2_STATUS_CTRL_0 + 0x1000 * sst_id,
				audio_ch_cnt, SOURCE_NUM);
}

void displayport_reg_set_ch_status_word_length(u32 sst_id,
		enum audio_bit_per_channel audio_bit_size)
{
	u32 word_max = 0;
	u32 sample_word_length = 0;

	switch (audio_bit_size) {
	case AUDIO_24_BIT:
		word_max = 1;
		sample_word_length = 0x05;
		break;

	case AUDIO_16_BIT:
		word_max = 0;
		sample_word_length = 0x01;
		break;

	case AUDIO_20_BIT:
		word_max = 0;
		sample_word_length = 0x05;
		break;

	default:
		word_max = 0;
		sample_word_length = 0x00;
		break;
	}

	displayport_write_mask(SST1_AUDIO_BIST_CHANNEL_STATUS_SET1 + 0x1000 * sst_id,
			word_max, WORD_MAX);

	displayport_write_mask(SST1_AUDIO_BIST_CHANNEL_STATUS_SET1 + 0x1000 * sst_id,
			sample_word_length, WORD_LENGTH);

	displayport_write_mask(SST1_AUDIO_CHANNEL_1_2_STATUS_CTRL_1 + 0x1000 * sst_id,
			word_max, WORD_MAX);

	displayport_write_mask(SST1_AUDIO_CHANNEL_1_2_STATUS_CTRL_1 + 0x1000 * sst_id,
			sample_word_length, WORD_LENGTH);
}

void displayport_reg_set_ch_status_sampling_frequency(u32 sst_id,
		enum audio_sampling_frequency audio_sampling_freq)
{
	u32 fs_freq = 0;

	switch (audio_sampling_freq) {
	case FS_32KHZ:
		fs_freq = 0x03;
		break;
	case FS_44KHZ:
		fs_freq = 0x00;
		break;
	case FS_48KHZ:
		fs_freq = 0x02;
		break;
	case FS_88KHZ:
		fs_freq = 0x08;
		break;
	case FS_96KHZ:
		fs_freq = 0x0A;
		break;
	case FS_176KHZ:
		fs_freq = 0x0C;
		break;
	case FS_192KHZ:
		fs_freq = 0x0E;
		break;
	default:
		fs_freq = 0x00;
		break;
	}

	displayport_write_mask(SST1_AUDIO_BIST_CHANNEL_STATUS_SET0 + 0x1000 * sst_id,
			fs_freq, FS_FREQ);
	displayport_write_mask(SST1_AUDIO_CHANNEL_1_2_STATUS_CTRL_0 + 0x1000 * sst_id,
			fs_freq, FS_FREQ);
}

void displayport_reg_set_ch_status_clock_accuracy(u32 sst_id,
		enum audio_clock_accuracy clock_accuracy)
{
	displayport_write_mask(SST1_AUDIO_BIST_CHANNEL_STATUS_SET0 + 0x1000 * sst_id,
			clock_accuracy, CLK_ACCUR);
	displayport_write_mask(SST1_AUDIO_CHANNEL_1_2_STATUS_CTRL_0 + 0x1000 * sst_id,
			clock_accuracy, CLK_ACCUR);
}

void displayport_reg_wait_buf_full(u32 sst_id)
{
	u32 cnt = 2000;
	u32 state = 0;

	do {
		state = (displayport_read(SST1_AUDIO_BUFFER_CONTROL + 0x1000 * sst_id)
			& MASTER_AUDIO_BUFFER_LEVEL) >> MASTER_AUDIO_BUFFER_LEVEL_BIT_POS;
		cnt--;
		udelay(1);
	} while ((state < AUDIO_BUF_FULL_SIZE) && cnt);

	if (!cnt)
		displayport_err("SST%d %s is timeout.\n", sst_id + 1, __func__);
}

void displayport_reg_print_audio_state(u32 sst_id)
{
	u32 val1 = 0;
	u32 val2 = 0;
	u32 val3 = 0;
	u32 val4 = 0;
	u32 val5 = 0;
	u32 reg_offset = 0;

	switch (sst_id) {
	case SST1:
		reg_offset = SYSTEM_SST1_FUNCTION_ENABLE;
		break;
	case SST2:
		reg_offset = SYSTEM_SST2_FUNCTION_ENABLE;
		break;
	default:
		reg_offset = SYSTEM_SST1_FUNCTION_ENABLE;
	}

	val1 = displayport_read(reg_offset);
	val2 = displayport_read(SST1_AUDIO_ENABLE + 0x1000 * sst_id);
	val3 = displayport_read(SST1_AUDIO_MASTER_TIMING_GEN + 0x1000 * sst_id);
	val4 = displayport_read(SST1_AUDIO_DMA_REQUEST_LATENCY_CONFIG + 0x1000 * sst_id);
	val5 = displayport_read(SST1_AUDIO_CONTROL + 0x1000 * sst_id);
	displayport_info("SST%d audio state: func_en=0x%x, aud_en=0x%x, master_t_gen=0x%x, dma_req=0x%x, aud_con=0x%X\n",
			sst_id + 1, val1, val2, val3, val4, val5);
}

void displayport_reg_set_dma_req_gen(u32 sst_id, u32 en)
{
	u32 val = en ? ~0 : 0;

	displayport_write_mask(SST1_AUDIO_CONTROL + 0x1000 * sst_id, val, DMA_REQ_GEN_EN);
}

void displayport_reg_set_clear_audio_fifo(u32 sst_id)
{
	displayport_write_mask(SST1_MAIN_FIFO_CONTROL + 0x1000 * sst_id, 1, CLEAR_AUDIO_FIFO);
}

void displayport_set_audio_ch_status(u32 sst_id,
		struct displayport_audio_config_data *audio_config_data)
{
	displayport_reg_set_ch_status_ch_cnt(sst_id, audio_config_data->audio_channel_cnt);
	displayport_reg_set_ch_status_word_length(sst_id, audio_config_data->audio_bit);
	displayport_reg_set_ch_status_sampling_frequency(sst_id, audio_config_data->audio_fs);
	displayport_reg_set_ch_status_clock_accuracy(sst_id, NOT_MATCH);
}

void displayport_wait_audio_buf_empty(u32 sst_id,
		struct displayport_device *displayport)
{
	u32 cnt = 1000;

	do {
		cnt--;
		udelay(1);
	} while (!displayport->sst[sst_id]->audio_buf_empty_check && cnt);

	if (!cnt)
		displayport_err("SST%d %s is timeout.\n", sst_id + 1, __func__);
}

void displayport_audio_enable(u32 sst_id,
		struct displayport_audio_config_data *audio_config_data)
{
	displayport_reg_set_audio_m_n(sst_id, ASYNC_MODE, audio_config_data->audio_fs);
	displayport_reg_set_audio_function_enable(sst_id, audio_config_data->audio_enable);
	displayport_reg_set_dma_burst_size(sst_id, audio_config_data->audio_word_length);
	displayport_reg_set_pcm_size(sst_id, audio_config_data->audio_bit);
	displayport_reg_set_dma_pack_mode(sst_id, audio_config_data->audio_packed_mode);
	displayport_reg_set_audio_ch(sst_id, audio_config_data->audio_channel_cnt);
	displayport_reg_set_audio_fifo_function_enable(sst_id, audio_config_data->audio_enable);
	displayport_reg_set_audio_sampling_frequency(sst_id, audio_config_data->audio_fs);
	/* channel mapping: FL, FR, C, SW, RL, RR */
	displayport_reg_set_audio_ch_mapping(sst_id, 1, 2, 4, 3, 5, 6, 7, 8);
	displayport_reg_set_dp_audio_enable(sst_id, audio_config_data->audio_enable);
	displayport_set_audio_ch_status(sst_id, audio_config_data);
	displayport_reg_set_audio_ch_status_same(sst_id, 1);
	displayport_reg_set_audio_master_mode_enable(sst_id, audio_config_data->audio_enable);
	displayport_reg_print_audio_state(sst_id);
}

void displayport_audio_disable(u32 sst_id)
{
	if (displayport_read_mask(SST1_AUDIO_ENABLE, AUDIO_EN + 0x1000 * sst_id) == 1) {
		udelay(1000); /* can't use sleep() in atomic context */
		displayport_reg_set_dp_audio_enable(sst_id, 0);
		displayport_reg_set_audio_fifo_function_enable(sst_id, 0);
		displayport_reg_set_clear_audio_fifo(sst_id);
		displayport_info("SST%d audio_disable\n", sst_id + 1);
	} else
		displayport_info("SST%d audio_disable, AUDIO_EN = 0\n", sst_id + 1);
}

void displayport_audio_wait_buf_full(u32 sst_id)
{
	displayport_reg_set_audio_master_mode_enable(sst_id, 0);
	displayport_reg_set_dma_req_gen(sst_id, 0);
	displayport_info("SST%d displayport_audio_wait_buf_full\n", sst_id + 1);
}

void displayport_audio_bist_enable(u32 sst_id,
		struct displayport_audio_config_data audio_config_data)
{
	displayport_info("SST%d displayport_audio_bist\n", sst_id + 1);
	displayport_info("audio_enable = %d\n", audio_config_data.audio_enable);
	displayport_info("audio_channel_cnt = %d\n", audio_config_data.audio_channel_cnt);
	displayport_info("audio_fs = %d\n", audio_config_data.audio_fs);

	if (audio_config_data.audio_enable == 1) {
		displayport_reg_set_audio_m_n(sst_id, ASYNC_MODE, audio_config_data.audio_fs);
		displayport_reg_set_audio_function_enable(sst_id, audio_config_data.audio_enable);

		displayport_reg_set_audio_ch(sst_id, audio_config_data.audio_channel_cnt);
		displayport_reg_set_audio_fifo_function_enable(sst_id, audio_config_data.audio_enable);
		displayport_reg_set_audio_ch_status_same(sst_id, 1);
		displayport_reg_set_audio_sampling_frequency(sst_id, audio_config_data.audio_fs);
		displayport_reg_set_dp_audio_enable(sst_id, audio_config_data.audio_enable);
		displayport_reg_set_audio_bist_mode(sst_id, 1);
		displayport_set_audio_ch_status(sst_id, &audio_config_data);
		displayport_reg_set_audio_master_mode_enable(sst_id, audio_config_data.audio_enable);
	} else {
		displayport_reg_set_audio_master_mode_enable(sst_id, 0);
		displayport_audio_disable(sst_id);
	}
}

void displayport_audio_init_config(u32 sst_id)
{
	displayport_reg_set_audio_m_n(sst_id, ASYNC_MODE, FS_48KHZ);
	displayport_reg_set_audio_function_enable(sst_id, 1);
	displayport_reg_set_audio_sampling_frequency(sst_id, FS_48KHZ);
	displayport_reg_set_dp_audio_enable(sst_id, 1);
	displayport_reg_set_audio_master_mode_enable(sst_id, 1);
	displayport_info("SST%d displayport_audio_init_config\n", sst_id + 1);
}

void displayport_reg_set_hdcp22_system_enable(u32 en)
{
	u32 val = en ? ~0 : 0;

	displayport_write_mask(HDCP22_SYS_EN, val, SYSTEM_ENABLE);
}

void displayport_reg_set_hdcp22_mode(u32 en)
{
	u32 val = en ? ~0 : 0;

	displayport_write_mask(SYSTEM_COMMON_FUNCTION_ENABLE, val, HDCP22_FUNC_EN);
}

void displayport_reg_set_hdcp22_encryption_enable(u32 en)
{
	u32 val = en ? ~0 : 0;

	displayport_write_mask(HDCP22_CONTROL, val, HDCP22_ENC_EN);
}

u32 displayport_reg_get_hdcp22_encryption_enable(void)
{
	return displayport_read_mask(HDCP22_CONTROL, HDCP22_ENC_EN);
}

void displayport_reg_set_aux_pn_inv(u32 val)
{
	displayport_write_mask(AUX_CONTROL, val, AUX_PN_INV);
}

void displayport_reg_set_mst_en(u32 en)
{
	u32 val = en ? ~0 : 0;

	displayport_write_mask(MST_ENABLE, val, MST_EN);

	displayport_dbg("MST_ENABLE = 0x%x\n",
				displayport_read(MST_ENABLE));
}

void displayport_reg_set_vc_payload_update_flag(void)
{
	u32 cnt = 100;
	u32 state;

	displayport_write_mask(MST_VC_PAYLOAD_UPDATE_FLAG, 1, VC_PAYLOAD_UPDATE_FLAG);

	do {
		state = displayport_read_mask(MST_VC_PAYLOAD_UPDATE_FLAG, VC_PAYLOAD_UPDATE_FLAG);
		cnt--;
		udelay(1);
	} while (!state && cnt);

	if (!cnt)
		displayport_err("%s is timeout.\n", __func__);
}

void displayport_reg_set_strm_x_y(u32 sst_id, u32 x_val, u32 y_val)
{

	u32 reg_offset = 0;

	switch (sst_id) {
	case SST1:
		reg_offset = MST_STREAM_1_X_VALUE;
		break;
	case SST2:
		reg_offset = MST_STREAM_2_X_VALUE;
		break;
	default:
		reg_offset = MST_STREAM_1_X_VALUE;
	}

	displayport_write_mask(reg_offset, x_val, STRM_1_X_VALUE);
	displayport_write_mask(reg_offset + 0x0004, y_val, STRM_1_Y_VALUE);
}

void displyaport_reg_set_vc_payload_id_timeslot(u32 ch, u32 start, u32 size)
{
	int i;
	u32 val[8] = {0, };
	int timeslot_start = start - 1;
	int timeslot_end = timeslot_start + size;
	int reg_start_offset = timeslot_start / 8;
	int reg_end_offset = timeslot_end / 8;

	if (timeslot_end <= MAX_VC_PAYLOAD_TIMESLOT) {
		val[reg_start_offset] = displayport_read(MST_VC_PAYLOAD_ID_TIMESLOT_01_08
									+ 4 * reg_start_offset);

		for (i = timeslot_start; i < timeslot_end; i++)
			val[i / 8] |= (ch + 1) << (28 - ((i * 4) % 32));

		for (i = reg_start_offset; i <= reg_end_offset; i++)
			displayport_write(MST_VC_PAYLOAD_ID_TIMESLOT_01_08 + 4 * i, val[i]);
	} else
		displayport_err("Over MAX_VC_PAYLOAD_TIMESLOT\n");

	for (i = reg_start_offset; i <= reg_end_offset; i++)
		displayport_dbg("set MST_VC_PAYLOAD_ID_TIMESLOT val[%d] = 0x%08x\n", i, val[i]);
}

void displyaport_reg_set_vc_payload_id_timeslot_delete(u32 ch,
		struct displayport_device *displayport)
{
	int i = 0;
	u32 val[16] = {0, };
	int byte_offset = 0;
	int bit_shift = 0;
	u32 move_val = 0;
	u32 mask_val = 0x0000000F;
	int timeslot_delete_position = displayport->sst[ch]->vc_config->timeslot_start_no - 1;
	int timeslot_move_postion = timeslot_delete_position + displayport->sst[ch]->vc_config->timeslot;

	for (i = 0; i < 8; i++) {
		val[i] = displayport_read(MST_VC_PAYLOAD_ID_TIMESLOT_01_08 + 4 * i);
		displayport_dbg("read MST_VC_PAYLOAD_ID_TIMESLOT val[%d] = 0x%08x\n", i, val[i]);
	}

	for (i = timeslot_move_postion; i < MAX_VC_PAYLOAD_TIMESLOT * 2; i++) {
		byte_offset = i / 8;
		bit_shift = 28 - ((i * 4) % 32);
		move_val = (val[byte_offset] & (mask_val << bit_shift)) >> bit_shift;
		val[byte_offset] &= ~(mask_val << bit_shift);

		byte_offset = timeslot_delete_position / 8;
		bit_shift = 28 - ((timeslot_delete_position * 4) % 32);
		val[byte_offset] = (val[byte_offset] & ~(mask_val << bit_shift))
								| (move_val << bit_shift);

		timeslot_delete_position++;

		if (timeslot_delete_position > MAX_VC_PAYLOAD_TIMESLOT)
			break;
	}

	for (i = 0; i < MAX_VC_CNT; i++) {
		if (displayport->sst[i]->vc_config->timeslot_start_no
				>= timeslot_move_postion)
			displayport->sst[i]->vc_config->timeslot_start_no
					-= displayport->sst[ch]->vc_config->timeslot;
	}

	for (i = 0; i < 8; i++) {
		displayport_write(MST_VC_PAYLOAD_ID_TIMESLOT_01_08 + 4 * i, val[i]);
		displayport_dbg("write MST_VC_PAYLOAD_ID_TIMESLOT val[%d] = 0x%08x\n", i, val[i]);
	}
}
