/*
 * linux/drivers/video/fbdev/exynos/dpu30/panels/exynos_panel_drv.c
 *
 * Exynos Common Panel Driver
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

int dpu_panel_log_level = 7;

struct exynos_panel_device *panel_drvdata[MAX_PANEL_DRV_SUPPORT];
EXPORT_SYMBOL(panel_drvdata);

static struct exynos_panel_ops *panel_list[MAX_PANEL_SUPPORT];

static struct of_device_id exynos_panel_of_match[] = {
	{ .compatible = "samsung,exynos-panel" },
	{ }
};

MODULE_DEVICE_TABLE(of, exynos_panel_of_match);

static int exynos_backlight_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static int exynos_backlight_update_status(struct backlight_device *bl)
{
	u32 brightness = bl->props.brightness;
	struct dsim_device *dsim;
	struct exynos_panel_device *panel;

	panel = dev_get_drvdata(&bl->dev);
	dsim = get_dsim_drvdata(panel->id);

	DPU_INFO_PANEL("%s: panel-%d, brightness = %d\n", __func__, panel->id,
			brightness);
#if 0
	if (bl->props.power != FB_BLANK_UNBLANK ||
			bl->props.fb_blank != FB_BLANK_UNBLANK ||
			bl->props.state & BL_CORE_FBBLANK)
		brightness = 0;
#endif

	if (brightness >= 0) {
		/* brightness bit-depth and para order can be different */
		dsim_call_panel_ops(dsim, EXYNOS_PANEL_IOC_SET_LIGHT, &brightness);
	} else {
		/* DO update brightness using dsim_wr_data */
		/* backlight_off ??? */
		return -EINVAL;
	}

	return 0;

}

static const struct backlight_ops exynos_backlight_ops = {
	.get_brightness	= exynos_backlight_get_brightness,
	.update_status	= exynos_backlight_update_status,
};

static int exynos_panel_parse_gpios(struct exynos_panel_device *panel)
{
	struct device_node *n = panel->dev->of_node;
	struct exynos_panel_resources *res = &panel->res;

	DPU_INFO_PANEL("%s +\n", __func__);

	if (of_get_property(n, "gpios", NULL) != NULL)  {
		/* panel reset */
		res->lcd_reset = of_get_gpio(n, 0);
		if (res->lcd_reset < 0) {
			DPU_ERR_PANEL("failed to get lcd reset GPIO");
			return -ENODEV;
		}
		res->lcd_power[0] = of_get_gpio(n, 1);
		if (res->lcd_power[0] < 0) {
			res->lcd_power[0] = -1;
			DPU_INFO_PANEL("not support LCD power GPIO");
		}
		res->lcd_power[1] = of_get_gpio(n, 2);
		if (res->lcd_power[1] < 0) {
			res->lcd_power[1] = -1;
			DPU_INFO_PANEL("not support 2nd LCD power GPIO");
		}
	}

	DPU_INFO_PANEL("%s -\n", __func__);
	return 0;
}

static int exynos_panel_parse_regulators(struct exynos_panel_device *panel)
{
	struct device *dev = panel->dev;
	struct exynos_panel_resources *res = &panel->res;
	char *str_regulator[MAX_REGULATORS];
	int i;

	for (i = 0; i < MAX_REGULATORS; ++i) {
		res->regulator[i] = NULL;
		str_regulator[i] = NULL;
	}

	if(!of_property_read_string(dev->of_node, "regulator_1p8v",
				(const char **)&str_regulator[0])) {
		res->regulator[0] = regulator_get(dev, str_regulator[0]);
		if (IS_ERR(res->regulator[0])) {
			DPU_ERR_PANEL("panel regulator 1.8V get failed\n");
			res->regulator[0] = NULL;
		}
	}

	if(!of_property_read_string(dev->of_node, "regulator_3p3v",
				(const char **)&str_regulator[1])) {
		res->regulator[1] = regulator_get(dev, str_regulator[1]);
		if (IS_ERR(res->regulator[1])) {
			DPU_ERR_PANEL("panel regulator 3.3V get failed\n");
			res->regulator[1] = NULL;
		}
	}

	return 0;
}

static int exynos_panel_reset(struct exynos_panel_device *panel)
{
	struct exynos_panel_resources *res = &panel->res;
	int ret;

	DPU_DEBUG_PANEL("%s +\n", __func__);

	ret = gpio_request_one(res->lcd_reset, GPIOF_OUT_INIT_HIGH, "lcd_reset");
	if (ret < 0) {
		DPU_ERR_PANEL("failed to get LCD reset GPIO\n");
		return -EINVAL;
	}

	usleep_range(5000, 6000);
	gpio_set_value(res->lcd_reset, 0);
	usleep_range(5000, 6000);
	gpio_set_value(res->lcd_reset, 1);

	gpio_free(res->lcd_reset);

	usleep_range(10000, 11000);

	DPU_DEBUG_PANEL("%s -\n", __func__);
	return 0;
}

static int exynos_panel_set_power(struct exynos_panel_device *panel, bool on)
{
	struct exynos_panel_resources *res = &panel->res;
	int ret;

	DPU_DEBUG_PANEL("%s(%d) +\n", __func__, on);

	if (on) {
		if (res->lcd_power[0] > 0) {
			ret = gpio_request_one(res->lcd_power[0],
					GPIOF_OUT_INIT_HIGH, "lcd_power0");
			if (ret < 0) {
				DPU_ERR_PANEL("failed LCD power on\n");
				return -EINVAL;
			}
			gpio_free(res->lcd_power[0]);
			usleep_range(10000, 11000);
		}

		if (res->lcd_power[1] > 0) {
			ret = gpio_request_one(res->lcd_power[1],
					GPIOF_OUT_INIT_HIGH, "lcd_power1");
			if (ret < 0) {
				DPU_ERR_PANEL("failed 2nd LCD power on\n");
				return -EINVAL;
			}
			gpio_free(res->lcd_power[1]);
			usleep_range(10000, 11000);
		}
		if (res->regulator[0] > 0) {
			ret = regulator_enable(res->regulator[0]);
			if (ret) {
				DPU_ERR_PANEL("panel regulator 1.8V enable failed\n");
				return ret;
			}
			usleep_range(5000, 6000);
		}

		if (res->regulator[1] > 0) {
			ret = regulator_enable(res->regulator[1]);
			if (ret) {
				DPU_ERR_PANEL("panel regulator 3.3V enable failed\n");
				return ret;
			}
		}
	} else {
		ret = gpio_request_one(res->lcd_reset, GPIOF_OUT_INIT_LOW,
				"lcd_reset");
		if (ret < 0) {
			DPU_ERR_PANEL("failed LCD reset off\n");
			return -EINVAL;
		}
		gpio_free(res->lcd_reset);

		if (res->lcd_power[0] > 0) {
			ret = gpio_request_one(res->lcd_power[0],
					GPIOF_OUT_INIT_LOW, "lcd_power0");
			if (ret < 0) {
				DPU_ERR_PANEL("failed LCD power off\n");
				return -EINVAL;
			}
			gpio_free(res->lcd_power[0]);
			usleep_range(5000, 6000);
		}

		if (res->lcd_power[1] > 0) {
			ret = gpio_request_one(res->lcd_power[1],
					GPIOF_OUT_INIT_LOW, "lcd_power1");
			if (ret < 0) {
				DPU_ERR_PANEL("failed 2nd LCD power off\n");
				return -EINVAL;
			}
			gpio_free(res->lcd_power[1]);
			usleep_range(5000, 6000);
		}
		if (res->regulator[0] > 0) {
			ret = regulator_disable(res->regulator[0]);
			if (ret) {
				DPU_ERR_PANEL("panel regulator 1.8V disable failed\n");
				return ret;
			}
		}

		if (res->regulator[1] > 0) {
			ret = regulator_disable(res->regulator[1]);
			if (ret) {
				DPU_ERR_PANEL("panel regulator 3.3V disable failed\n");
				return ret;
			}
		}
	}

	DPU_DEBUG_PANEL("%s(%d) -\n", __func__, on);

	return 0;
}

static int exynos_panel_calc_slice_width(u32 dsc_cnt, u32 slice_num, u32 xres)
{
	u32 slice_width;
	u32 width_eff;
	u32 slice_width_byte_unit, comp_slice_width_byte_unit;
	u32 comp_slice_width_pixel_unit;
	u32 compressed_slice_w = 0;
	u32 i, j;

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
		if (info->display_mode[i].dsc_en) {
			info->display_mode[i].dsc_width = be32_to_cpu(mode_item[5]);
			info->display_mode[i].dsc_height = be32_to_cpu(mode_item[6]);
			info->display_mode[i].dsc_enc_sw =
				exynos_panel_calc_slice_width(info->dsc.cnt,
						info->dsc.slice_num,
						info->display_mode[i].mode.width);
			info->display_mode[i].dsc_dec_sw =
				info->display_mode[i].mode.width / info->dsc.slice_num;
		}
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

static void exynos_panel_parse_lcd_info(struct exynos_panel_device *panel,
		struct device_node *np)
{
	u32 res[2];
	struct exynos_panel_info *lcd_info = &panel->lcd_info;
	u32 max_br, dft_br;

	of_property_read_u32(np, "mode", &lcd_info->mode);
	of_property_read_u32_array(np, "resolution", res, 2);
	lcd_info->xres = res[0];
	lcd_info->yres = res[1];
	of_property_read_u32(np, "timing,refresh", &lcd_info->fps);
	DPU_INFO_PANEL("LCD(%s) resolution: %dx%d@%d, %s mode\n",
			np->name, lcd_info->xres, lcd_info->yres,
			lcd_info->fps, lcd_info->mode ? "command" : "video");

	of_property_read_u32_array(np, "size", res, 2);
	lcd_info->width = res[0];
	lcd_info->height = res[1];
	of_property_read_u32(np, "type_of_ddi", &lcd_info->ddi_type);
	DPU_DEBUG_PANEL("LCD size(%dx%d), DDI type(%d)\n", res[0], res[1],
			lcd_info->ddi_type);

	panel->bl->props.max_brightness = DEFAULT_MAX_BRIGHTNESS;
	panel->bl->props.brightness = DEFAULT_BRIGHTNESS;
	if (!of_property_read_u32(np, "max-brightness", &max_br))
		panel->bl->props.max_brightness = max_br;
	if (!of_property_read_u32(np, "dft-brightness", &dft_br))
		panel->bl->props.brightness = dft_br;

	DPU_INFO_PANEL("max brightness : %d\n", panel->bl->props.max_brightness);
	DPU_INFO_PANEL("default brightness : %d\n", panel->bl->props.brightness);

#if defined(CONFIG_EXYNOS_DECON_DQE)
	snprintf(lcd_info->ddi_name, MAX_DDI_NAME_LEN, "%s", np->name);
#endif

	exynos_panel_get_timing_info(lcd_info, np);
	exynos_panel_get_dsc_info(lcd_info, np);
	exynos_panel_get_mres_info(lcd_info, np);
	exynos_panel_get_hdr_info(lcd_info, np);
	exynos_panel_get_display_modes(lcd_info, np);
}

static void exynos_panel_list_up(void)
{
	panel_list[0] = &panel_emul_ops;
	panel_list[1] = &panel_s6e3hab_ops;
	panel_list[2] = &panel_s6e3ha9_ops;
	panel_list[3] = &panel_s6e3ha8_ops;
	panel_list[4] = &panel_s6e3fa0_ops;
	panel_list[5] = &panel_s6e3fab_ops;
	panel_list[6] = &panel_s6e3had_ops;
}

static int exynos_panel_register_ops(struct exynos_panel_device *panel)
{
	int i, j;
	bool found = false;

	for (i = 0; i < MAX_PANEL_SUPPORT; ++i) {
		for (j = 0; j < MAX_PANEL_ID_NUM; j++) {
			if (panel_list[i]->id[j] == panel->lcd_info.id) {
				panel->ops = panel_list[i];
				panel->id_index = j;
				found = true;
				DPU_INFO_PANEL("panel ops is found in panel list\n");
				break;
			}

		}
		if (found == true)
			break;
	}

	if (!found) {
		DPU_ERR_PANEL("panel ops is not found in panel list\n");
		return -EINVAL;
	}

	return 0;
}

static int exynos_panel_register(struct exynos_panel_device *panel, u32 id)
{
	struct device_node *n = panel->dev->of_node;
	struct device_node *np;
	struct property *prop;
	int panel_id, i;
	const __be32 *cur;

	for (i = 0; i < MAX_PANEL_SUPPORT; ++i) {
		np = of_parse_phandle(n, "lcd_info", i);
		if (!np) {
			DPU_INFO_PANEL("panel is not matched\n");
			return -EINVAL;
		}

		of_property_for_each_u32(np, "id", prop, cur, panel_id) {
			DPU_INFO_PANEL("finding... %s(0x%x)\n", np->name, panel_id);

			/*
			 * TODO: The mode(video or command) should be compared
			 * for distinguishing panel DT information which has same DDI ID
			 */
			if (id == panel_id) {
				panel->found = true;
				DPU_INFO_PANEL("matched panel is found(%s:0x%x)\n",
						np->name, id);

				/* parsing lcd info */
				panel->lcd_info.id = id;
				exynos_panel_parse_lcd_info(panel, np);
				if (exynos_panel_register_ops(panel))
					BUG();

				break;
			}
		}

		if(panel->found == true)
			break;
	}

	return 0;
}

static void exynos_panel_find_id(struct exynos_panel_device *panel)
{
	struct device_node *n = panel->dev->of_node;
	int id;

	panel->found = false;

	if(of_property_read_u32(n, "ddi_id", &id)) {
		DPU_INFO_PANEL("connected panel id is not defined in DT\n");
		return;
	}

#if defined(CONFIG_EXYNOS_EMUL_DISP)
	id = 0xFFFFFF; /* emul_display id */
#endif

	DPU_INFO_PANEL("%s: panel id(0x%x) in DT\n", __func__, id);

	exynos_panel_register(panel, id);

	DPU_INFO_PANEL("%s -\n", __func__);
}

static int exynos_panel_parse_dt(struct exynos_panel_device *panel)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(panel->dev->of_node)) {
		DPU_ERR_PANEL("no device tree information of exynos panel\n");
		return -EINVAL;
	}

	panel->id = of_alias_get_id(panel->dev->of_node, "panel");
	DPU_INFO_PANEL("panel-%d parsing DT...\n", panel->id);

	ret = exynos_panel_parse_gpios(panel);
	if (ret)
		goto err;

	ret = exynos_panel_parse_regulators(panel);
	if (ret)
		goto err;

err:
	return ret;
}

static long exynos_panel_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct exynos_panel_device *panel;
	int ret = 0;

	panel = container_of(sd, struct exynos_panel_device, sd);


	switch (cmd) {
	case EXYNOS_PANEL_IOC_REGISTER:
		ret = exynos_panel_register(panel, *(u32 *)arg);
		break;
	case EXYNOS_PANEL_IOC_POWERON:
		ret = exynos_panel_set_power(panel, true);
		break;
	case EXYNOS_PANEL_IOC_POWEROFF:
		ret = exynos_panel_set_power(panel, false);
		break;
	case EXYNOS_PANEL_IOC_RESET:
		ret = exynos_panel_reset(panel);
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
	case EXYNOS_PANEL_IOC_SET_LIGHT:
		call_panel_ops(panel, set_light, panel, *(int *)arg);
		break;
	case EXYNOS_PANEL_IOC_SET_VREFRESH:
		call_panel_ops(panel, set_vrefresh, panel, *(int *)arg);
		break;
	default:
		DPU_ERR_PANEL("not supported ioctl by panel driver\n");
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
	snprintf(sd->name, sizeof(sd->name), "%s.%d", "panel-sd", panel->id);
	v4l2_set_subdevdata(sd, panel);
	DPU_INFO_PANEL("%s: panel sd name(%s)\n", __func__, sd->name);
}

static ssize_t panel_cabc_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	int ret = 0;
	struct exynos_panel_device *panel = dev_get_drvdata(dev);

	ret = call_panel_ops(panel, set_cabc_mode,
				panel, POWER_MODE_READ);

	count = snprintf(buf, PAGE_SIZE, "power_mode = %d, ret = %d\n",
			panel->power_mode, ret);

	return count;
}

static ssize_t panel_cabc_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int value = 0;
	struct exynos_panel_device *panel = dev_get_drvdata(dev);

	ret = kstrtouint(buf, 0, &value);
	if (ret < 0)
		return ret;

	panel->power_mode = value;

	DPU_INFO_PANEL("%s: %d\n", __func__, value);

	call_panel_ops(panel, set_cabc_mode,
			panel, panel->power_mode);

	return count;
}

static DEVICE_ATTR(cabc_mode, 0660, panel_cabc_mode_show,
			panel_cabc_mode_store);

static int exynos_panel_probe(struct platform_device *pdev)
{
	struct exynos_panel_device *panel;
	int ret = 0;
	char name[16];

	DPU_DEBUG_PANEL("%s +\n", __func__);

	panel = devm_kzalloc(&pdev->dev, sizeof(struct exynos_panel_device),
			GFP_KERNEL);
	if (IS_ERR_OR_NULL(panel)) {
		DPU_ERR_PANEL("failed to allocate panel structure\n");
		ret = -ENOMEM;
		goto err;
	}

	panel->dev = &pdev->dev;

	mutex_init(&panel->ops_lock);

	if (IS_ENABLED(CONFIG_EXYNOS_DECON_LCD_CABC))
		panel->cabc_enabled = true;
	else
		panel->cabc_enabled = false;

	if (panel->cabc_enabled) {
		ret = device_create_file(panel->dev, &dev_attr_cabc_mode);
		if (ret) {
			DPU_ERR_PANEL("failed to create cabc sysfs\n");
			goto err;
		}

		panel->power_mode = POWER_SAVE_OFF;
	}

	ret = exynos_panel_parse_dt(panel);
	if (ret) {
		goto err_dev_file;
	}

	snprintf(name, sizeof(name), "panel_%d", panel->id);
	panel->bl = devm_backlight_device_register(panel->dev, name, NULL,
			panel, &exynos_backlight_ops, NULL);
	if (IS_ERR(panel->bl)) {
		DPU_ERR_PANEL("failed to register backlight device\n");
		ret = PTR_ERR(panel->bl);
		goto err_dev_file;
	}

	panel_drvdata[panel->id] = panel;

	exynos_panel_list_up();
	exynos_panel_find_id(panel);

	exynos_panel_init_subdev(panel);
	platform_set_drvdata(pdev, panel);

	DPU_DEBUG_PANEL("%s -\n", __func__);
	return ret;

err_dev_file:
	device_remove_file(panel->dev, &dev_attr_cabc_mode);

err:
	DPU_DEBUG_PANEL("%s -\n", __func__);
	return ret;
}

static void exynos_panel_shutdown(struct platform_device *pdev)
{
	struct exynos_panel_device *panel = platform_get_drvdata(pdev);

	backlight_device_unregister(panel->bl);
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

MODULE_DESCRIPTION("Exynos Common Panel Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jiun Yu <jiun.yu@samsung.com>");
