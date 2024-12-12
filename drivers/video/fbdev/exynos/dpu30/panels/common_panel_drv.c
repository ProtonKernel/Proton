/*
 * linux/drivers/video/fbdev/exynos/dpu30/panels/common_panel_drv.c
 *
 * Common Panel Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/byteorder.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include "exynos_panel_drv.h"

int dpu_panel_log_level = 6;

struct exynos_panel_device *panel_drvdata[MAX_PANEL_DRV_SUPPORT];
EXPORT_SYMBOL(panel_drvdata);

static struct exynos_panel_ops *panel_list[MAX_PANEL_SUPPORT];

static struct of_device_id exynos_panel_of_match[] = {
	{ .compatible = "samsung,exynos-panel" },
	{ }
};

MODULE_DEVICE_TABLE(of, exynos_panel_of_match);

int exynos_panel_calc_slice_width(u32 dsc_cnt, u32 slice_num, u32 xres)
{
	u32 slice_width;
	u32 width_eff;
	u32 slice_width_byte_unit, comp_slice_width_byte_unit;
	u32 comp_slice_width_pixel_unit;
	u32 compressed_slice_w = 0;
	u32 i, j;

	if (dsc_cnt == 0) {
		DPU_ERR_PANEL("invalid dsc_cnt(%d)\n", dsc_cnt);
		return -EINVAL;
	}

	if (dsc_cnt == 2)
		width_eff = xres >> 1;
	else
		width_eff = xres;

	if (slice_num / dsc_cnt == 2)
		slice_width = width_eff >> 1;
	else
		slice_width = width_eff;

	/* 3bytes per pixel */
	slice_width_byte_unit = slice_width * 3;
	/* integer value, /3 for 1/3 compression */
	comp_slice_width_byte_unit = slice_width_byte_unit / 3;
	/* integer value, /3 for pixel unit */
	comp_slice_width_pixel_unit = comp_slice_width_byte_unit / 3;

	i = comp_slice_width_byte_unit % 3;
	j = comp_slice_width_pixel_unit % 2;

	if (i == 0 && j == 0) {
		compressed_slice_w = comp_slice_width_pixel_unit;
	} else if (i == 0 && j != 0) {
		compressed_slice_w = comp_slice_width_pixel_unit + 1;
	} else if (i != 0) {
		while (1) {
			comp_slice_width_pixel_unit++;
			j = comp_slice_width_pixel_unit % 2;
			if (j == 0)
				break;
		}
		compressed_slice_w = comp_slice_width_pixel_unit;
	}

	return compressed_slice_w;
}

static void exynos_panel_get_timing_info(struct exynos_panel_info *info,
		struct device_node *np)
{
	u32 res[14];

	of_property_read_u32_array(np, "timing,h-porch", res, 3);
	info->hbp = res[0];
	info->hfp = res[1];
	info->hsa = res[2];
	DPU_DEBUG_PANEL("hbp(%d), hfp(%d), hsa(%d)\n", res[0], res[1], res[2]);

	of_property_read_u32_array(np, "timing,v-porch", res, 3);
	info->vbp = res[0];
	info->vfp = res[1];
	info->vsa = res[2];
	DPU_DEBUG_PANEL("vbp(%d), vfp(%d), vsa(%d)\n", res[0], res[1], res[2]);

	if (of_property_read_u32(np, "timing,v-blank-t", &info->v_blank_t)) {
		info->v_blank_t = 100;
		DPU_INFO_PANEL("WARN: v-blank-t is not defined in DT\n");
	}
	DPU_DEBUG_PANEL("v_blank_time(%d us)\n", info->v_blank_t);

	of_property_read_u32(np, "timing,dsi-hs-clk", &info->hs_clk);
	of_property_read_u32(np, "timing,dsi-escape-clk", &info->esc_clk);
	DPU_DEBUG_PANEL("requested hs clk(%d), esc clk(%d)\n", info->hs_clk,
			info->esc_clk);

#if defined(CONFIG_EXYNOS_DSIM_DITHER)
	of_property_read_u32_array(np, "timing,pmsk", res, 14);
#else
	of_property_read_u32_array(np, "timing,pmsk", res, 4);
#endif
	info->dphy_pms.p = res[0];
	info->dphy_pms.m = res[1];
	info->dphy_pms.s = res[2];
	info->dphy_pms.k = res[3];
	DPU_DEBUG_PANEL("PMSK(%d %d %d %d)\n", res[0], res[1], res[2], res[3]);
#if defined(CONFIG_EXYNOS_DSIM_DITHER)
	info->dphy_pms.mfr = res[4];
	info->dphy_pms.mrr = res[5];
	info->dphy_pms.sel_pf = res[6];
	info->dphy_pms.icp = res[7];
	info->dphy_pms.afc_enb = res[8];
	info->dphy_pms.extafc = res[9];
	info->dphy_pms.feed_en = res[10];
	info->dphy_pms.fsel = res[11];
	info->dphy_pms.fout_mask = res[12];
	info->dphy_pms.rsel = res[13];
	DPU_DEBUG_PANEL(" mfr(%d), mrr(0x%x), sel_pf(%d), icp(%d)\n",
			res[4], res[5], res[6], res[7]);
	DPU_DEBUG_PANEL(" afc_enb(%d), extafc(%d), feed_en(%d), fsel(%d)\n",
			res[8], res[9], res[10], res[11]);
	DPU_DEBUG_PANEL(" fout_mask(%d), rsel(%d)\n", res[12], res[13]);
#endif

	of_property_read_u32(np, "data_lane", &info->data_lane);
	DPU_INFO_PANEL("data lane count(%d)\n", info->data_lane);

	if (info->mode == DECON_VIDEO_MODE) {
		of_property_read_u32(np, "vt_compensation", &info->vt_compensation);
		DPU_INFO_PANEL("vt_compensation(%d)\n", info->vt_compensation);
	}
}

static void exynos_panel_get_dsc_info(struct exynos_panel_info *info,
		struct device_node *np)
{
	of_property_read_u32(np, "dsc_en", &info->dsc.en);
	DPU_INFO_PANEL("DSC %s\n", info->dsc.en ? "enabled" : "disabled");

	if (info->dsc.en) {
		of_property_read_u32(np, "dsc_cnt", &info->dsc.cnt);
		of_property_read_u32(np, "dsc_slice_num", &info->dsc.slice_num);
		of_property_read_u32(np, "dsc_slice_h", &info->dsc.slice_h);

		info->dsc.enc_sw = exynos_panel_calc_slice_width(info->dsc.cnt,
					info->dsc.slice_num, info->xres);
		info->dsc.dec_sw = info->xres / info->dsc.slice_num;

		DPU_INFO_PANEL(" DSC cnt(%d), slice cnt(%d), slice height(%d)\n",
				info->dsc.cnt, info->dsc.slice_num,
				info->dsc.slice_h);
		DPU_INFO_PANEL(" DSC enc_sw(%d), dec_sw(%d)\n",
				info->dsc.enc_sw, info->dsc.dec_sw);
	}
}

static void exynos_panel_get_mres_info(struct exynos_panel_info *info,
		struct device_node *np)
{
	u32 num;
	u32 w[3] = {0, };
	u32 h[3] = {0, };
	u32 dsc_w[3] = {0, };
	u32 dsc_h[3] = {0, };
	u32 dsc_en[3] = {0, };
	int i;

	of_property_read_u32(np, "mres_en", &info->mres.en);
	DPU_INFO_PANEL("Multi Resolution LCD %s\n",
			info->mres.en ? "enabled" : "disabled");

	if (info->mres.en) {
		info->mres.number = 1; /* default = 1 */
		of_property_read_u32(np, "mres_number", &num);
		info->mres.number = num;

		of_property_read_u32_array(np, "mres_width", w, num);
		of_property_read_u32_array(np, "mres_height", h, num);
		of_property_read_u32_array(np, "mres_dsc_width", dsc_w, num);
		of_property_read_u32_array(np, "mres_dsc_height", dsc_h, num);
		of_property_read_u32_array(np, "mres_dsc_en", dsc_en, num);

		for (i = 0; i < num; ++i) {
			info->mres.res_info[i].width = w[i];
			info->mres.res_info[i].height = h[i];
			info->mres.res_info[i].dsc_en = dsc_en[i];
			info->mres.res_info[i].dsc_width = dsc_w[i];
			info->mres.res_info[i].dsc_height = dsc_h[i];

			DPU_INFO_PANEL(" [%dx%d]: DSC(%d))\n",
					info->mres.res_info[i].width,
					info->mres.res_info[i].height,
					info->mres.res_info[i].dsc_en);
		}
	}
}

static void exynos_panel_get_hdr_info(struct exynos_panel_info *info,
		struct device_node *np)
{
	u32 hdr_num = 0;
	u32 hdr_type[HDR_CAPA_NUM] = {0, };
	u32 max, avg, min;
	int i;

	/* HDR info */
	of_property_read_u32(np, "hdr_num", &hdr_num);
	info->hdr.num = hdr_num;
	DPU_INFO_PANEL("hdr_num(%d)\n", hdr_num);

	if (hdr_num != 0) {
		of_property_read_u32_array(np, "hdr_type", hdr_type, hdr_num);
		for (i = 0; i < hdr_num; i++) {
			info->hdr.type[i] = hdr_type[i];
			DPU_INFO_PANEL(" hdr_type[%d] = %d\n", i, hdr_type[i]);
		}

		of_property_read_u32(np, "hdr_max_luma", &max);
		of_property_read_u32(np, "hdr_max_avg_luma", &avg);
		of_property_read_u32(np, "hdr_min_luma", &min);
		info->hdr.max_luma = max;
		info->hdr.max_avg_luma = avg;
		info->hdr.min_luma = min;
		DPU_INFO_PANEL(" luma max/avg/min(%d %d %d)\n", max, avg, min);
	}
}


static void exynos_panel_get_color_mode_info(struct exynos_panel_info *info,
		struct device_node *np)
{
	int i;
	int cnt = 0;

	struct panel_color_mode *color_mode = &info->color_mode;

	cnt = of_property_count_u32_elems(np, "color_mode");

	DPU_INFO_PANEL("supporting color mode : %d\n", cnt);

	if (cnt > 0) {
		if (cnt > MAX_COLOR_MODE) {
			cnt = MAX_COLOR_MODE;
		}
		of_property_read_u32_array(np, "color_mode",
			color_mode->mode, cnt);

		color_mode->cnt = cnt;
	}

	for(i = 0; i < cnt; i++)
		DPU_INFO_PANEL("color mode[%d] : %d\n", i, color_mode->mode[i]);
}


#ifdef CONFIG_EXYNOS_SET_ACTIVE

#define DISPLAY_MODE_ITEM_CNT	7

static void exynos_panel_get_display_modes(struct exynos_panel_info *info,
		struct device_node *np)
{
	int size;
	u32 len;
	int i;
	const unsigned int *addr;
	unsigned int *mode_item;
	u32 disp_group = 0;

	DPU_INFO_PANEL("%s +\n", __func__);

	of_property_read_u32(np, "default_mode", &info->cur_mode_idx);
	DPU_INFO_PANEL("default display mode index(%d)\n", info->cur_mode_idx);

	size = of_property_count_u32_elems(np, "display_mode");
	if (size < 0) {
		DPU_INFO_PANEL("This panel doesn't support display mode\n");
		return;
	}

	info->display_mode_count = size / DISPLAY_MODE_ITEM_CNT;
	DPU_INFO_PANEL("supported display mode count(%d)\n", info->display_mode_count);

	addr = of_get_property(np, "display_mode", &len);

	for (i = 0; i < info->display_mode_count; ++i) {
		mode_item = (unsigned int *)&addr[i * DISPLAY_MODE_ITEM_CNT];
		info->display_mode[i].mode.index = i;
		info->display_mode[i].mode.width = be32_to_cpu(mode_item[0]);
		info->display_mode[i].mode.height = be32_to_cpu(mode_item[1]);
		info->display_mode[i].mode.fps = be32_to_cpu(mode_item[2]);
		info->display_mode[i].mode.mm_width = info->width;
		info->display_mode[i].mode.mm_height = info->height;
		info->display_mode[i].cmd_lp_ref = be32_to_cpu(mode_item[3]);
		info->display_mode[i].dsc_en = be32_to_cpu(mode_item[4]);
		info->display_mode[i].dsc_width = be32_to_cpu(mode_item[5]);
		info->display_mode[i].dsc_height = be32_to_cpu(mode_item[6]);
		info->display_mode[i].dsc_enc_sw =
			exynos_panel_calc_slice_width(info->dsc.cnt,
					info->dsc.slice_num,
					info->display_mode[i].mode.width);
		info->display_mode[i].dsc_dec_sw =
			info->display_mode[i].mode.width / info->dsc.slice_num;
		if ((i > 0) &&
			((info->display_mode[i].mode.width != info->display_mode[i - 1].mode.width) ||
			(info->display_mode[i].mode.height != info->display_mode[i - 1].mode.height)))
				disp_group++;
		info->display_mode[i].mode.group = disp_group;

		DPU_INFO_PANEL("display mode[%d] : %dx%d@%d, %dmm x %dmm, lp_ref(%d)\n",
				info->display_mode[i].mode.index,
				info->display_mode[i].mode.width,
				info->display_mode[i].mode.height,
				info->display_mode[i].mode.fps,
				info->display_mode[i].mode.mm_width,
				info->display_mode[i].mode.mm_height,
				info->display_mode[i].cmd_lp_ref);
		DPU_INFO_PANEL("\t\tdsc %s, dsc size(%dx%d), dsc enc/dec sw(%d/%d)\n",
				info->display_mode[i].dsc_en ? "enabled" : "disabled",
				info->display_mode[i].dsc_width,
				info->display_mode[i].dsc_height,
				info->display_mode[i].dsc_enc_sw,
				info->display_mode[i].dsc_dec_sw);
	}

	DPU_INFO_PANEL("%s -\n", __func__);
}
#endif

void parse_lcd_info(struct device_node *np, EXYNOS_PANEL_INFO *lcd_info)
{
	u32 res[2];

	if (!np) {
		DPU_ERR_PANEL("%s: null device_node\n", __func__);
		return;
	}

	if (!lcd_info) {
		DPU_ERR_PANEL("%s: null lcd_info\n", __func__);
		return;
	}

	DPU_INFO_PANEL("%s +\n", __func__);
	of_property_read_u32(np, "mode", &lcd_info->mode);
	of_property_read_u32_array(np, "resolution", res, 2);
	lcd_info->xres = res[0];
	lcd_info->yres = res[1];
	of_property_read_u32(np, "timing,refresh", &lcd_info->fps);
	DPU_INFO_PANEL("LCD(%s) resolution: %dx%d@%d, %s mode\n",
			np->name, lcd_info->xres, lcd_info->yres,
			lcd_info->fps, lcd_info->mode == DECON_MIPI_COMMAND_MODE ? "command" : "video");

	of_property_read_u32_array(np, "size", res, 2);
	lcd_info->width = res[0];
	lcd_info->height = res[1];
	of_property_read_u32(np, "type_of_ddi", &lcd_info->ddi_type);
	DPU_DEBUG_PANEL("LCD size(%dx%d), DDI type(%d)\n", res[0], res[1],
			lcd_info->ddi_type);

#if defined(CONFIG_EXYNOS_DECON_DQE)
	snprintf(lcd_info->ddi_name, MAX_DDI_NAME_LEN, "%s", np->name);
#endif

	exynos_panel_get_timing_info(lcd_info, np);
	exynos_panel_get_dsc_info(lcd_info, np);
	exynos_panel_get_mres_info(lcd_info, np);
	exynos_panel_get_hdr_info(lcd_info, np);
	exynos_panel_get_color_mode_info(lcd_info, np);
#ifdef CONFIG_EXYNOS_SET_ACTIVE
	exynos_panel_get_display_modes(lcd_info, np);
#endif
	DPU_INFO_PANEL("%s -\n", __func__);
}

static void exynos_panel_list_up(void)
{
	panel_list[0] = &common_panel_ops;
}

static int exynos_panel_register_ops(struct exynos_panel_device *panel)
{
	panel->ops = panel_list[0];

	return 0;
}

static int exynos_panel_register(struct exynos_panel_device *panel, u32 id)
{
	struct device_node *n = panel->dev->of_node;
	struct device_node *np;

	np = of_parse_phandle(n, "lcd_info", 0);
	if (!np) {
		DPU_INFO_PANEL("panel is not matched\n");
		return -EINVAL;
	}

	panel->found = true;
	panel->lcd_info.id = id;
	exynos_panel_register_ops(panel);

	return 0;
}
//Todo: Need to change distinguish usnig kernel version feature
#if 0
static int __match_panel_v4l2_subdev(struct device *dev, void *data)
#else
static int __match_panel_v4l2_subdev(struct device *dev, const void *data)

#endif
{
	struct panel_device *panel_drv;
	struct exynos_panel_device *panel;

	panel = (struct exynos_panel_device *)data;

	DPU_INFO_PANEL("matched panel drv name : %s\n", dev->driver->name);
	panel_drv = (struct panel_device *)dev_get_drvdata(dev);
	if (panel == NULL) {
		DPU_ERR_PANEL("failed to get panel's v4l2 subdev\n");
		return -EINVAL;
	}

	panel->panel_drv_sd = &panel_drv->sd;

	return 0;
}

int __mockable exynos_panel_find_panel_drv(struct exynos_panel_device *panel)
{
	struct device_driver *drv;
	struct device *dev;

	panel->found = false;
	DPU_INFO_PANEL("%s +\n", __func__);

	drv = driver_find(PANEL_DRV_NAME, &platform_bus_type);
	if (IS_ERR_OR_NULL(drv)) {
		DPU_ERR_PANEL("%s:failed to find driver\n", __func__);
		BUG();
		return -ENODEV;
	}

	dev = driver_find_device(drv, NULL, panel, __match_panel_v4l2_subdev);
	if (panel->panel_drv_sd == NULL) {
		DPU_ERR_PANEL("%s:failed to fined panel's v4l2 subdev\n", __func__);
		return -ENODEV;
	}

	panel->found = true;

	DPU_INFO_PANEL("%s -\n", __func__);

	return 0;
}

static void exynos_panel_init_panel_drv(struct exynos_panel_device *panel)
{
	int ret;

	ret = call_panel_ops(panel, init, panel);
	if (ret < 0) {
		DPU_ERR_PANEL("panel-%d failed to call init\n", panel->id);
		return;
	}
}

int __mockable exynos_panel_parse_dt(struct exynos_panel_device *panel)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(panel->dev->of_node)) {
		DPU_ERR_PANEL("no device tree information of exynos panel\n");
		return -EINVAL;
	}

	panel->id = of_alias_get_id(panel->dev->of_node, "panel");
	DPU_INFO_PANEL("panel-%d parsing DT...\n", panel->id);

	return ret;
}

static long exynos_panel_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct exynos_panel_device *panel;
	struct decon_rect *rect;
	int ret = 0;

	panel = container_of(sd, struct exynos_panel_device, sd);

	switch (cmd) {
	case EXYNOS_PANEL_IOC_REGISTER:
		ret = exynos_panel_register(panel, *(u32 *)arg);
		break;
	case EXYNOS_PANEL_IOC_RESET:
		call_panel_ops(panel, reset_panel, panel);
		break;
	case EXYNOS_PANEL_IOC_DISPLAYON:
		call_panel_ops(panel, displayon, panel);
		break;
	case EXYNOS_PANEL_IOC_SUSPEND:
		call_panel_ops(panel, suspend, panel);
		break;
	case EXYNOS_PANEL_IOC_MRES:
		call_panel_ops(panel, mres, panel, *(int *)arg);
		break;
	case EXYNOS_PANEL_IOC_DOZE:
		call_panel_ops(panel, doze, panel);
		break;
	case EXYNOS_PANEL_IOC_DOZE_SUSPEND:
		call_panel_ops(panel, doze_suspend, panel);
		break;
	case EXYNOS_PANEL_IOC_DUMP:
		call_panel_ops(panel, dump, panel);
		break;
	case EXYNOS_PANEL_IOC_READ_STATE:
		ret = call_panel_ops(panel, read_state, panel);
		break;
	case EXYNOS_PANEL_IOC_SET_VREFRESH:
		call_panel_ops(panel, set_vrefresh, panel, (struct vrr_config_data*)arg);
		break;
#if IS_ENABLED(CONFIG_MCD_PANEL)
	case EXYNOS_PANEL_IOC_INIT:
		ret = call_panel_ops(panel, init, panel);
		break;
	case EXYNOS_PANEL_IOC_PROBE:
		ret = call_panel_ops(panel, probe, panel);
		break;
	case EXYNOS_PANEL_IOC_CONNECTED:
		ret = call_panel_ops(panel, connected, panel);
		break;
	case EXYNOS_PANEL_IOC_IS_POWERON:
		ret = call_panel_ops(panel, is_poweron, panel);
		break;
	case EXYNOS_PANEL_IOC_SETAREA:
		rect = (struct decon_rect *)arg;
		ret = call_panel_ops(panel, setarea, panel,
				rect->left, rect->right, rect->top, rect->bottom);
		break;
	case EXYNOS_PANEL_IOC_POWERON:
		ret = call_panel_ops(panel, poweron, panel);
		break;
	case EXYNOS_PANEL_IOC_POWEROFF:
		ret = call_panel_ops(panel, poweroff, panel);
		break;
	case EXYNOS_PANEL_IOC_SLEEPIN:
		ret = call_panel_ops(panel, sleepin, panel);
		break;
	case EXYNOS_PANEL_IOC_SLEEPOUT:
		ret = call_panel_ops(panel, sleepout, panel);
		break;
	case EXYNOS_PANEL_IOC_NOTIFY:
		ret = call_panel_ops(panel, notify, panel, arg);
		break;
	case EXYNOS_PANEL_IOC_SET_ERROR_CB:
		ret = call_panel_ops(panel, set_error_cb, panel, arg);
		break;
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	case EXYNOS_PANEL_IOC_GET_DISPLAY_MODE:
		ret = call_panel_ops(panel, get_display_mode, panel, arg);
		break;
	case EXYNOS_PANEL_IOC_SET_DISPLAY_MODE:
		ret = call_panel_ops(panel, set_display_mode, panel, arg);
		break;
#endif
#endif
	default:
		DPU_ERR_PANEL("not supported ioctl (0x%x) by panel driver\n", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct v4l2_subdev_core_ops exynos_panel_sd_core_ops = {
	.ioctl = exynos_panel_ioctl,
};

static const struct v4l2_subdev_ops exynos_panel_subdev_ops = {
	.core = &exynos_panel_sd_core_ops,
};

static void exynos_panel_init_subdev(struct exynos_panel_device *panel)
{
	struct v4l2_subdev *sd = &panel->sd;

	v4l2_subdev_init(sd, &exynos_panel_subdev_ops);
	sd->owner = THIS_MODULE;
	sd->grp_id = panel->id;
	snprintf(sd->name, sizeof(sd->name), "%s.%d", "common-panel-sd", panel->id);
	v4l2_set_subdevdata(sd, panel);
	DPU_INFO_PANEL("%s: panel sd name(%s)\n", __func__, sd->name);
}

struct exynos_panel_device *exynos_panel_create(void)
{
	struct exynos_panel_device *panel;

	panel = kzalloc(sizeof(struct exynos_panel_device), GFP_KERNEL);
	if (!panel) {
		DPU_ERR_PANEL("failed to allocate panel structure\n");
		return NULL;
	}

	return panel;
}

int exynos_panel_device_init(struct exynos_panel_device *panel)
{
	int ret;

	DPU_DEBUG_PANEL("%s +\n", __func__);
	mutex_init(&panel->ops_lock);

	ret = exynos_panel_parse_dt(panel);
	if (ret < 0) {
		DPU_ERR_PANEL("failed to exynos_panle_parse_dt\n");
		return ret;
	}

	exynos_panel_list_up();
	exynos_panel_register_ops(panel);
	exynos_panel_find_panel_drv(panel);
	exynos_panel_init_subdev(panel);
	exynos_panel_init_panel_drv(panel);

	DPU_DEBUG_PANEL("%s -\n", __func__);
	return 0;
}

static int exynos_panel_probe(struct platform_device *pdev)
{
	struct exynos_panel_device *panel;
	int ret = 0;

	panel = exynos_panel_create();
	if (!panel)
		return -ENOMEM;

	panel->dev = &pdev->dev;
	platform_set_drvdata(pdev, panel);
	panel_drvdata[panel->id] = panel;
	ret = exynos_panel_device_init(panel);
	if (ret < 0) {
		DPU_ERR_PANEL("failed to exynos_panle_device_init\n");
		kfree(panel);
		return ret;
	}

	DPU_DEBUG_PANEL("%s -\n", __func__);
	return 0;
}

static void exynos_panel_shutdown(struct platform_device *pdev)
{
}

struct platform_driver exynos_panel_driver = {
	.probe		= exynos_panel_probe,
	.shutdown	= exynos_panel_shutdown,
	.driver		= {
		.name		= "exynos-panel",
		.of_match_table	= of_match_ptr(exynos_panel_of_match),
		.suppress_bind_attrs = true,
	},
};

MODULE_DESCRIPTION("Common Panel Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gwanghui Lee <gwanghui.lee@samsung.com>");
