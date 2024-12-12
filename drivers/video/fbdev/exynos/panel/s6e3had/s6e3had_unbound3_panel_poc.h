/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3had/s6e3had_unbound3_panel_poc.h
 *
 * Header file for POC Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAD_UNBOUND3_PANEL_POC_H__
#define __S6E3HAD_UNBOUND3_PANEL_POC_H__

#include "../panel.h"
#include "../panel_poc.h"

#ifdef CONFIG_SUPPORT_GM2_FLASH
#define UNBOUND3_POC_GM2_VBIAS_ADDR		(0xF0000)
#define UNBOUND3_POC_GM2_VBIAS1_DATA_ADDR	(0xF0000)
#define UNBOUND3_POC_GM2_VBIAS1_DATA_SIZE 	(S6E3HAD_GM2_FLASH_VBIAS1_LEN)
#define UNBOUND3_POC_GM2_VBIAS_CHECKSUM_ADDR	(UNBOUND3_POC_GM2_VBIAS1_DATA_ADDR + S6E3HAD_GM2_FLASH_VBIAS_CHECKSUM_OFS)
#define UNBOUND3_POC_GM2_VBIAS_CHECKSUM_SIZE (S6E3HAD_GM2_FLASH_VBIAS_CHECKSUM_LEN)
#define UNBOUND3_POC_GM2_VBIAS_MAGICNUM_ADDR	(UNBOUND3_POC_GM2_VBIAS1_DATA_ADDR + S6E3HAD_GM2_FLASH_VBIAS_MAGICNUM_OFS)
#define UNBOUND3_POC_GM2_VBIAS_MAGICNUM_SIZE (S6E3HAD_GM2_FLASH_VBIAS_MAGICNUM_LEN)
#define UNBOUND3_POC_GM2_VBIAS_TOTAL_SIZE (S6E3HAD_GM2_FLASH_VBIAS_TOTAL_SIZE)
#define UNBOUND3_POC_GM2_VBIAS2_DATA_ADDR	(0xF0057)
#define UNBOUND3_POC_GM2_VBIAS2_DATA_SIZE 	(2)
#endif

/* partition consists of DATA, CHECKSUM and MAGICNUM */
static struct poc_partition unbound3_poc_partition[] = {
#ifdef CONFIG_SUPPORT_GM2_FLASH
	[POC_GM2_VBIAS_PARTITION] = {
		.name = "gamma_mode2_vbias",
		.addr = UNBOUND3_POC_GM2_VBIAS_ADDR,
		.size = UNBOUND3_POC_GM2_VBIAS_TOTAL_SIZE,
		.data = {
			{
				.data_addr = UNBOUND3_POC_GM2_VBIAS1_DATA_ADDR,
				.data_size = UNBOUND3_POC_GM2_VBIAS1_DATA_SIZE,
			},
			{
				.data_addr = UNBOUND3_POC_GM2_VBIAS2_DATA_ADDR,
				.data_size = UNBOUND3_POC_GM2_VBIAS2_DATA_SIZE,
			},
		},
		.checksum_addr = UNBOUND3_POC_GM2_VBIAS_CHECKSUM_ADDR,
		.checksum_size = UNBOUND3_POC_GM2_VBIAS_CHECKSUM_SIZE,
		.magicnum_addr = UNBOUND3_POC_GM2_VBIAS_MAGICNUM_ADDR,
		.magicnum_size = UNBOUND3_POC_GM2_VBIAS_MAGICNUM_SIZE,
		.magicnum = 0x01,
		.need_preload = true,
	},
#endif
};

static struct panel_poc_data s6e3had_unbound3_poc_data = {
	.version = 3,
	.partition = unbound3_poc_partition,
	.nr_partition = ARRAY_SIZE(unbound3_poc_partition),
#ifdef CONFIG_SUPPORT_POC_SPI
	.conn_src = POC_CONN_SRC_SPI,
#endif
};
#endif /* __S6E3HAD_UNBOUND3_PANEL_POC_H__ */
