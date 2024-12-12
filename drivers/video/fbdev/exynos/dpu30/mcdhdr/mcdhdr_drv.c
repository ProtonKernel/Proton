/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS MCD HDR driver

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/console.h>
#include <linux/platform_device.h>

#include "mcdhdr_drv.h"
#include "../decon.h"


/*log level for hdr default : 0*/
int hdr_log_level;


static inline u32 mcdhdr_reg_read(struct mcdhdr_device *mcdhdr, u32 reg_id)
{
	if (!mcdhdr)
		return 0;

	return readl(mcdhdr->regs + reg_id);
}

static inline void mcdhdr_reg_write(struct mcdhdr_device *mcdhdr, u32 reg_id, u32 val)
{
	writel(val, mcdhdr->regs + reg_id);
}

static int mcdhdr_stop(struct mcdhdr_device *mcdhdr)
{
	mcdhdr_info("%d: stop\n", mcdhdr->id);

	mcdhdr_reg_write(mcdhdr, MCDHDR_REG_CON, 0);
	mcdhdr->state = MCDHDR_PWR_OFF;

	return 0;
}

static int mcdhdr_dump(struct mcdhdr_device *mcdhdr)
{
	int ret = 0;
	int length = SDR_TOTAL_REG_CNT;

	if (IS_SUPPORT_HDR10(mcdhdr->attr))
		length = HDR_TOTAL_REG_CNT;

	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
		mcdhdr->regs, length - 4, false);

	return ret;
}

static int mcdhdr_update_lut(struct mcdhdr_device *mcdhdr, u32 *lut, int lut_offset)
{
	int i;
	u32 reg_offset = 0;
	u32 reg_length = 0;

	if (!lut) {
		mcdhdr_err("lut is null\n");
		goto exit_update;
	}

	reg_offset = lut[lut_offset++];
	reg_length = lut[lut_offset++];
	mcdhdr_info("offset: %d, length: %d\n", reg_offset, reg_length);

	for (i = 0; i < reg_length; i++) {
		if (reg_offset + (i * 4) >= MCDHDR_ADDR_RANGE) {
			mcdhdr_err("out of address range: %x(offset:%d: length:%d)\n",
				reg_offset + (i * 4), reg_offset, reg_length);
			goto exit_update;
		}

		mcdhdr_dbg("offset %d: value %x\n", reg_offset + (i * 4), lut[lut_offset + i]);
		mcdhdr_reg_write(mcdhdr, reg_offset + (i * 4), lut[lut_offset + i]);
	}
	return lut_offset + reg_length;

exit_update:
	return 0;
}

#define MAX_UNPACK_COUNT	6

static void mcdhdr_dump_lut(struct fd_hdr_header *hdr_header)
{
	int size;

	if (!hdr_header) {
		mcdhdr_err("hdr_header is null\n");
		return;
	}
	mcdhdr_err("dump hdr size: %d, shall: %d, need:%d\n", hdr_header->total_bytesize,
		hdr_header->num.unpack.shall, hdr_header->num.unpack.need);

	size = hdr_header->total_bytesize;
	if (size > MCDHDR_ADDR_RANGE)
		size = MCDHDR_ADDR_RANGE;

	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
		hdr_header, size, false);
}

static int mcdhdr_win_config(struct mcdhdr_device *mcdhdr, void *arg)
{
	int i;
	int ret = 0;
	int lut_offset;
	u32 con_reg = 0;
	int shall, need;
	struct mcdhdr_win_config *config = NULL;
	struct fd_hdr_header *hdr_header = NULL;

	config = (struct mcdhdr_win_config *)arg;
	if (!config) {
		mcdhdr_err("config is null\n");
		return -EINVAL;
	}

	hdr_header = (struct fd_hdr_header *)config->hdr_info;
	if (!hdr_header) {
		mcdhdr_err("failed to get hdr_header\n");
		return -EINVAL;
	}

	if (mcdhdr->id != hdr_header->type.unpack.layer_index) {
		mcdhdr_err("wrong layer index!! (hdr id: %d, header id: %d)\n", mcdhdr->id,
			hdr_header->type.unpack.layer_index);
		goto exit_config;
	}

	if (hdr_log_level != hdr_header->type.unpack.log_level) {
		mcdhdr_info("log level is updated %d -> %d\n", hdr_log_level, hdr_header->type.unpack.log_level);
		hdr_log_level = hdr_header->type.unpack.log_level;
	}

	mcdhdr_info("%d: hdr header info -> size: %d, log_level: %d, shall: %d, need: %d, con_reg: %x\n",
		mcdhdr->id, hdr_header->total_bytesize, hdr_header->type.unpack.log_level,
		hdr_header->num.unpack.shall, hdr_header->num.unpack.need, hdr_header->sfr_con);

	lut_offset = sizeof(struct fd_hdr_header) / sizeof(u32);

	shall = hdr_header->num.unpack.shall;
	if ((shall < 0) || (shall > MAX_UNPACK_COUNT)) {
		mcdhdr_err("%d exceed unpack shall: %d\b", mcdhdr->id, shall);
		goto exit_config;
	}

	for (i = 0; i < shall; i++) {
		lut_offset = mcdhdr_update_lut(mcdhdr, (u32 *)config->hdr_info, lut_offset);
		if (lut_offset == 0) {
			mcdhdr_err("failed to update lut\n");
			goto exit_config_dump;
		}
	}

	/* in case of the first frame update after hibernation */
	con_reg = mcdhdr_reg_read(mcdhdr, MCDHDR_REG_CON);
	if (!(con_reg & MCDHDR_REG_CON_EN)) {
		need = hdr_header->num.unpack.need;
		if ((need < 0) || (need > MAX_UNPACK_COUNT)) {
			mcdhdr_err("%d exceed unpack shall: %d\b", mcdhdr->id, need);
			goto exit_config;
		}

		for (i = 0; i < need; i++) {
			lut_offset = mcdhdr_update_lut(mcdhdr, (u32 *)config->hdr_info, lut_offset);
			if (lut_offset == 0)
				mcdhdr_err("failed to update lut\n");
		}
	}
	mcdhdr_reg_write(mcdhdr, MCDHDR_REG_CON, hdr_header->sfr_con);

	mcdhdr->state = MCDHDR_PWR_ON;

	return ret;

exit_config_dump:
	mcdhdr_dump_lut(hdr_header);
exit_config:
	return ret;
}


static long mcdhdr_v4l2_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct mcdhdr_device *mcdhdr = v4l2_get_subdevdata(sd);

	switch (cmd) {

	case HDR_WIN_CONFIG:
		ret = mcdhdr_win_config(mcdhdr, arg);
		break;

	case HDR_RESET:
	case HDR_STOP:
		ret = mcdhdr_stop(mcdhdr);
		break;

	case HDR_DUMP:
		ret = mcdhdr_dump(mcdhdr);
		break;

	case HDR_GET_CAPA:
		mcdhdr_dbg("id: %d GET_ATTR: %x\n", mcdhdr->id, mcdhdr->attr);
		if (arg)
			*((int *)arg) = mcdhdr->attr;
		break;
	default:
		break;
	}

	return ret;
}


static const struct v4l2_subdev_core_ops mcdhdr_v4l2_core_ops = {
	.ioctl = mcdhdr_v4l2_ioctl,
};


static struct v4l2_subdev_ops mcdhdr_v4l2_ops = {
	.core = &mcdhdr_v4l2_core_ops,
};

static void mcdhdr_init_v4l2subdev(struct mcdhdr_device *mcdhdr)
{
	struct v4l2_subdev *sd = &mcdhdr->sd;

	v4l2_subdev_init(sd, &mcdhdr_v4l2_ops);
	sd->owner = THIS_MODULE;
	sd->grp_id = mcdhdr->id;
	snprintf(sd->name, sizeof(sd->name), "%s.%d", "hdr-sd", mcdhdr->id);

	v4l2_set_subdevdata(sd, mcdhdr);
}


static int mcdhdr_parse_dt(struct mcdhdr_device *mcdhdr, struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct device *dev;

	if (!pdev) {
		mcdhdr_err("invalid param pdev is null\n");
		return -EINVAL;
	}

	dev = &pdev->dev;
	if (!dev) {
		mcdhdr_err("invalid param dev is null\n");
		return -EINVAL;
	}

	of_property_read_u32(dev->of_node, "id", &mcdhdr->id);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		mcdhdr_err("failed to get MEM resource\n");
		return -ENOENT;
	}

	mcdhdr_dbg("reg: 0x%x ~ 0x%x\n", (u32)(res->start), (u32)(res->end));

	mcdhdr->regs = devm_ioremap_resource(dev, res);
	if (!mcdhdr->regs) {
		mcdhdr_err("failed to ioremap\n");
		return -EINVAL;
	}

	of_property_read_u32(dev->of_node, "attr", &mcdhdr->attr);
	mcdhdr_info("mcdhdr id: %d wcg: %s, hdr10: %s, hdr10+: %s\n", mcdhdr->id,
		IS_SUPPORT_WCG(mcdhdr->attr) ? "support" : "n/a",
		IS_SUPPORT_HDR10(mcdhdr->attr) ? "support" : "n/a",
		IS_SUPPORT_HDR10P(mcdhdr->attr) ? "support" : "n/a");

	return ret;
}


static int mcdhdr_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mcdhdr_device *mcdhdr;
	struct device *dev = &pdev->dev;

	mcdhdr_dbg("probe start");

	mcdhdr = devm_kzalloc(dev, sizeof(struct mcdhdr_device), GFP_KERNEL);
	if (!mcdhdr) {
		mcdhdr_err("failed to alloc mem for hdr device\n");
		ret = -ENOMEM;
		goto probe_err;
	}

	mcdhdr->dev = dev;
	mcdhdr->state = MCDHDR_PWR_OFF;

	ret = mcdhdr_parse_dt(mcdhdr, pdev);
	if (ret) {
		mcdhdr_err("failed to parse dt\n");
		goto probe_err_free;
	}

	mcdhdr_init_v4l2subdev(mcdhdr);
	platform_set_drvdata(pdev, mcdhdr);

	ret = mcdhdr_probe_sysfs(mcdhdr);
	if (ret)
		mcdhdr_err("failed to prove sysfs\n");

	mcdhdr_dbg("probe done\n");
	return 0;
probe_err_free:
	kfree(mcdhdr);

probe_err:
	return ret;
}

static int mcdhdr_remove(struct platform_device *pdev)
{
	int ret = 0;

	mcdhdr_dbg("called\n");

	return ret;
}

static const struct of_device_id mcdhdr_match_tbl[] = {
	{.compatible = "samsung,exynos-mcdhdr" },
	{},
};

struct platform_driver mcdhdr_driver __refdata = {
	.probe = mcdhdr_probe,
	.remove = mcdhdr_remove,
	.driver = {
		.name = MCDHDR_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mcdhdr_match_tbl),
		.suppress_bind_attrs = true,
	}
};

MODULE_AUTHOR("Minwoo Kim <minwoo7945.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung Exynos HDR Driver for MCD");
MODULE_LICENSE("GPL");
