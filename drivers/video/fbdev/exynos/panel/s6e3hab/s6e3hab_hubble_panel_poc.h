/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3hab/s6e3hab_hubble_panel_poc.h
 *
 * Header file for POC Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAB_HUBBLE_PANEL_POC_H__
#define __S6E3HAB_HUBBLE_PANEL_POC_H__

#include "../panel.h"
#include "../panel_poc.h"

#define HUBBLE_POC_IMG_ADDR	(0x8F000)
#define HUBBLE_POC_IMG_SIZE	(115719)

#ifdef CONFIG_SUPPORT_DIM_FLASH
#define HUBBLE_POC_DIM_DATA_ADDR	(0xF0000)
#define HUBBLE_POC_DIM_DATA_SIZE (S6E3HAB_DIM_FLASH_DATA_SIZE)
#define HUBBLE_POC_DIM_CHECKSUM_ADDR	(HUBBLE_POC_DIM_DATA_ADDR + S6E3HAB_DIM_FLASH_CHECKSUM_OFS)
#define HUBBLE_POC_DIM_CHECKSUM_SIZE (S6E3HAB_DIM_FLASH_CHECKSUM_LEN)
#define HUBBLE_POC_DIM_MAGICNUM_ADDR	(HUBBLE_POC_DIM_DATA_ADDR + S6E3HAB_DIM_FLASH_MAGICNUM_OFS)
#define HUBBLE_POC_DIM_MAGICNUM_SIZE (S6E3HAB_DIM_FLASH_MAGICNUM_LEN)
#define HUBBLE_POC_DIM_TOTAL_SIZE (S6E3HAB_DIM_FLASH_TOTAL_SIZE)

#define HUBBLE_POC_MTP_DATA_ADDR	(0xF2800)
#define HUBBLE_POC_MTP_DATA_SIZE (S6E3HAB_DIM_FLASH_MTP_DATA_SIZE)
#define HUBBLE_POC_MTP_CHECKSUM_ADDR	(HUBBLE_POC_MTP_DATA_ADDR + S6E3HAB_DIM_FLASH_MTP_CHECKSUM_OFS)
#define HUBBLE_POC_MTP_CHECKSUM_SIZE (S6E3HAB_DIM_FLASH_MTP_CHECKSUM_LEN)
#define HUBBLE_POC_MTP_TOTAL_SIZE (S6E3HAB_DIM_FLASH_MTP_TOTAL_SIZE)

#define HUBBLE_POC_DIM_120HZ_DATA_ADDR	(0xF3000)
#define HUBBLE_POC_DIM_120HZ_DATA_SIZE (S6E3HAB_DIM_FLASH_120HZ_DATA_SIZE)
#define HUBBLE_POC_DIM_120HZ_CHECKSUM_ADDR	(HUBBLE_POC_DIM_120HZ_DATA_ADDR + S6E3HAB_DIM_FLASH_120HZ_CHECKSUM_OFS)
#define HUBBLE_POC_DIM_120HZ_CHECKSUM_SIZE (S6E3HAB_DIM_FLASH_120HZ_CHECKSUM_LEN)
#define HUBBLE_POC_DIM_120HZ_MAGICNUM_ADDR	(HUBBLE_POC_DIM_120HZ_DATA_ADDR + S6E3HAB_DIM_FLASH_120HZ_MAGICNUM_OFS)
#define HUBBLE_POC_DIM_120HZ_MAGICNUM_SIZE (S6E3HAB_DIM_FLASH_120HZ_MAGICNUM_LEN)
#define HUBBLE_POC_DIM_120HZ_TOTAL_SIZE (S6E3HAB_DIM_FLASH_120HZ_TOTAL_SIZE)

#define HUBBLE_POC_MTP_120HZ_DATA_ADDR	(0xF5800)
#define HUBBLE_POC_MTP_120HZ_DATA_SIZE (S6E3HAB_DIM_FLASH_120HZ_MTP_DATA_SIZE)
#define HUBBLE_POC_MTP_120HZ_CHECKSUM_ADDR	(HUBBLE_POC_MTP_120HZ_DATA_ADDR + S6E3HAB_DIM_FLASH_120HZ_MTP_CHECKSUM_OFS)
#define HUBBLE_POC_MTP_120HZ_CHECKSUM_SIZE (S6E3HAB_DIM_FLASH_120HZ_MTP_CHECKSUM_LEN)
#define HUBBLE_POC_MTP_120HZ_TOTAL_SIZE (S6E3HAB_DIM_FLASH_120HZ_MTP_TOTAL_SIZE)

#define HUBBLE_POC_DIM_60HZ_HS_DATA_ADDR	(0xF6000)
#define HUBBLE_POC_DIM_60HZ_HS_DATA_SIZE (S6E3HAB_DIM_FLASH_60HZ_HS_DATA_SIZE)
#define HUBBLE_POC_DIM_60HZ_HS_CHECKSUM_ADDR	(HUBBLE_POC_DIM_60HZ_HS_DATA_ADDR + S6E3HAB_DIM_FLASH_60HZ_HS_CHECKSUM_OFS)
#define HUBBLE_POC_DIM_60HZ_HS_CHECKSUM_SIZE (S6E3HAB_DIM_FLASH_60HZ_HS_CHECKSUM_LEN)
#define HUBBLE_POC_DIM_60HZ_HS_MAGICNUM_ADDR	(HUBBLE_POC_DIM_60HZ_HS_DATA_ADDR + S6E3HAB_DIM_FLASH_60HZ_HS_MAGICNUM_OFS)
#define HUBBLE_POC_DIM_60HZ_HS_MAGICNUM_SIZE (S6E3HAB_DIM_FLASH_60HZ_HS_MAGICNUM_LEN)
#define HUBBLE_POC_DIM_60HZ_HS_TOTAL_SIZE (S6E3HAB_DIM_FLASH_60HZ_HS_TOTAL_SIZE)

#define HUBBLE_POC_MTP_60HZ_HS_DATA_ADDR	(0xF8800)
#define HUBBLE_POC_MTP_60HZ_HS_DATA_SIZE (S6E3HAB_DIM_FLASH_60HZ_HS_MTP_DATA_SIZE)
#define HUBBLE_POC_MTP_60HZ_HS_CHECKSUM_ADDR	(HUBBLE_POC_MTP_60HZ_HS_DATA_ADDR + S6E3HAB_DIM_FLASH_60HZ_HS_MTP_CHECKSUM_OFS)
#define HUBBLE_POC_MTP_60HZ_HS_CHECKSUM_SIZE (S6E3HAB_DIM_FLASH_60HZ_HS_MTP_CHECKSUM_LEN)
#define HUBBLE_POC_MTP_60HZ_HS_TOTAL_SIZE (S6E3HAB_DIM_FLASH_60HZ_HS_MTP_TOTAL_SIZE)
#endif

#define HUBBLE_POC_MCD_ADDR	(0xB8000)
#define HUBBLE_POC_MCD_SIZE (S6E3HAB_FLASH_MCD_LEN)

/* partition consists of DATA, CHECKSUM and MAGICNUM */
static struct poc_partition hubble_preliminary_poc_partition[] = {
	[POC_IMG_PARTITION] = {
		.name = "image",
		.addr = HUBBLE_POC_IMG_ADDR,
		.size = HUBBLE_POC_IMG_SIZE,
		.need_preload = false
	},
#ifdef CONFIG_SUPPORT_DIM_FLASH
	[POC_DIM_PARTITION] = {
		.name = "dimming",
		.addr = HUBBLE_POC_DIM_DATA_ADDR,
		.size = HUBBLE_POC_DIM_TOTAL_SIZE,
		.data = { { .data_addr = HUBBLE_POC_DIM_DATA_ADDR, .data_size = HUBBLE_POC_DIM_DATA_SIZE } },
		.checksum_addr = HUBBLE_POC_DIM_CHECKSUM_ADDR,
		.checksum_size = HUBBLE_POC_DIM_CHECKSUM_SIZE,
		.magicnum_addr = HUBBLE_POC_DIM_MAGICNUM_ADDR,
		.magicnum_size = HUBBLE_POC_DIM_MAGICNUM_SIZE,
		.need_preload = false,
		.magicnum = 1,
	},
	[POC_MTP_PARTITION] = {
		.name = "mtp",
		.addr = HUBBLE_POC_MTP_DATA_ADDR,
		.size = HUBBLE_POC_MTP_TOTAL_SIZE,
		.data = { { .data_addr = HUBBLE_POC_MTP_DATA_ADDR, .data_size = HUBBLE_POC_MTP_DATA_SIZE } },
		.checksum_addr = HUBBLE_POC_MTP_CHECKSUM_ADDR,
		.checksum_size = HUBBLE_POC_MTP_CHECKSUM_SIZE,
		.magicnum_addr = 0,
		.magicnum_size = 0,
		.need_preload = false,
	},
	[POC_DIM_PARTITION_1] = {
		.name = "dimming",
		.addr = HUBBLE_POC_DIM_120HZ_DATA_ADDR,
		.size = HUBBLE_POC_DIM_120HZ_TOTAL_SIZE,
		.data = { { .data_addr = HUBBLE_POC_DIM_120HZ_DATA_ADDR, .data_size = HUBBLE_POC_DIM_120HZ_DATA_SIZE } },
		.checksum_addr = HUBBLE_POC_DIM_120HZ_CHECKSUM_ADDR,
		.checksum_size = HUBBLE_POC_DIM_120HZ_CHECKSUM_SIZE,
		.magicnum_addr = HUBBLE_POC_DIM_120HZ_MAGICNUM_ADDR,
		.magicnum_size = HUBBLE_POC_DIM_120HZ_MAGICNUM_SIZE,
		.need_preload = false,
		.magicnum = 1,
	},
	[POC_MTP_PARTITION_1] = {
		.name = "mtp",
		.addr = HUBBLE_POC_MTP_120HZ_DATA_ADDR,
		.size = HUBBLE_POC_MTP_120HZ_TOTAL_SIZE,
		.data = { { .data_addr = HUBBLE_POC_MTP_120HZ_DATA_ADDR, .data_size = HUBBLE_POC_MTP_120HZ_DATA_SIZE } },
		.checksum_addr = HUBBLE_POC_MTP_120HZ_CHECKSUM_ADDR,
		.checksum_size = HUBBLE_POC_MTP_120HZ_CHECKSUM_SIZE,
		.magicnum_addr = 0,
		.magicnum_size = 0,
		.need_preload = false,
	},
#endif
	[POC_MCD_PARTITION] = {
		.name = "mcd",
		.addr = HUBBLE_POC_MCD_ADDR,
		.size = HUBBLE_POC_MCD_SIZE,
		.need_preload = false
	},
};

static struct poc_partition hubble_poc_partition[] = {
	[POC_IMG_PARTITION] = {
		.name = "image",
		.addr = HUBBLE_POC_IMG_ADDR,
		.size = HUBBLE_POC_IMG_SIZE,
		.need_preload = false
	},
#ifdef CONFIG_SUPPORT_DIM_FLASH
	[POC_DIM_PARTITION] = {
		.name = "dimming",
		.addr = HUBBLE_POC_DIM_DATA_ADDR,
		.size = HUBBLE_POC_DIM_TOTAL_SIZE,
		.data = { { .data_addr = HUBBLE_POC_DIM_DATA_ADDR, .data_size = HUBBLE_POC_DIM_DATA_SIZE } },
		.checksum_addr = HUBBLE_POC_DIM_CHECKSUM_ADDR,
		.checksum_size = HUBBLE_POC_DIM_CHECKSUM_SIZE,
		.magicnum_addr = HUBBLE_POC_DIM_MAGICNUM_ADDR,
		.magicnum_size = HUBBLE_POC_DIM_MAGICNUM_SIZE,
		.need_preload = false,
		.magicnum = 1,
	},
	[POC_MTP_PARTITION] = {
		.name = "mtp",
		.addr = HUBBLE_POC_MTP_DATA_ADDR,
		.size = HUBBLE_POC_MTP_TOTAL_SIZE,
		.data = { { .data_addr = HUBBLE_POC_MTP_DATA_ADDR, .data_size = HUBBLE_POC_MTP_DATA_SIZE } },
		.checksum_addr = HUBBLE_POC_MTP_CHECKSUM_ADDR,
		.checksum_size = HUBBLE_POC_MTP_CHECKSUM_SIZE,
		.magicnum_addr = 0,
		.magicnum_size = 0,
		.need_preload = false,
	},
	[POC_DIM_PARTITION_1] = {
		.name = "dimming",
		.addr = HUBBLE_POC_DIM_120HZ_DATA_ADDR,
		.size = HUBBLE_POC_DIM_120HZ_TOTAL_SIZE,
		.data = { { .data_addr = HUBBLE_POC_DIM_120HZ_DATA_ADDR, .data_size = HUBBLE_POC_DIM_120HZ_DATA_SIZE } },
		.checksum_addr = HUBBLE_POC_DIM_120HZ_CHECKSUM_ADDR,
		.checksum_size = HUBBLE_POC_DIM_120HZ_CHECKSUM_SIZE,
		.magicnum_addr = HUBBLE_POC_DIM_120HZ_MAGICNUM_ADDR,
		.magicnum_size = HUBBLE_POC_DIM_120HZ_MAGICNUM_SIZE,
		.need_preload = false,
		.magicnum = 1,
	},
	[POC_MTP_PARTITION_1] = {
		.name = "mtp",
		.addr = HUBBLE_POC_MTP_120HZ_DATA_ADDR,
		.size = HUBBLE_POC_MTP_120HZ_TOTAL_SIZE,
		.data = { { .data_addr = HUBBLE_POC_MTP_120HZ_DATA_ADDR, .data_size = HUBBLE_POC_MTP_120HZ_DATA_SIZE } },
		.checksum_addr = HUBBLE_POC_MTP_120HZ_CHECKSUM_ADDR,
		.checksum_size = HUBBLE_POC_MTP_120HZ_CHECKSUM_SIZE,
		.magicnum_addr = 0,
		.magicnum_size = 0,
		.need_preload = false,
	},
	[POC_DIM_PARTITION_2] = {
		.name = "dimming",
		.addr = HUBBLE_POC_DIM_60HZ_HS_DATA_ADDR,
		.size = HUBBLE_POC_DIM_60HZ_HS_TOTAL_SIZE,
		.data = { { .data_addr = HUBBLE_POC_DIM_60HZ_HS_DATA_ADDR, .data_size = HUBBLE_POC_DIM_60HZ_HS_DATA_SIZE } },
		.checksum_addr = HUBBLE_POC_DIM_60HZ_HS_CHECKSUM_ADDR,
		.checksum_size = HUBBLE_POC_DIM_60HZ_HS_CHECKSUM_SIZE,
		.magicnum_addr = HUBBLE_POC_DIM_60HZ_HS_MAGICNUM_ADDR,
		.magicnum_size = HUBBLE_POC_DIM_60HZ_HS_MAGICNUM_SIZE,
		.need_preload = false,
		.magicnum = 1,
	},
	[POC_MTP_PARTITION_2] = {
		.name = "mtp",
		.addr = HUBBLE_POC_MTP_60HZ_HS_DATA_ADDR,
		.size = HUBBLE_POC_MTP_60HZ_HS_TOTAL_SIZE,
		.data = { { .data_addr = HUBBLE_POC_MTP_60HZ_HS_DATA_ADDR, .data_size = HUBBLE_POC_MTP_60HZ_HS_DATA_SIZE } },
		.checksum_addr = HUBBLE_POC_MTP_60HZ_HS_CHECKSUM_ADDR,
		.checksum_size = HUBBLE_POC_MTP_60HZ_HS_CHECKSUM_SIZE,
		.magicnum_addr = 0,
		.magicnum_size = 0,
		.need_preload = false,
	},
#endif
	[POC_MCD_PARTITION] = {
		.name = "mcd",
		.addr = HUBBLE_POC_MCD_ADDR,
		.size = HUBBLE_POC_MCD_SIZE,
		.need_preload = false
	},
};

static struct panel_poc_data s6e3hab_hubble_preliminary_poc_data = {
	.version = 3,
	.partition = hubble_preliminary_poc_partition,
	.nr_partition = ARRAY_SIZE(hubble_preliminary_poc_partition),
	.wdata_len = 256,
#ifdef CONFIG_SUPPORT_POC_SPI
	.conn_src = POC_CONN_SRC_SPI,
#endif
};

static struct panel_poc_data s6e3hab_hubble_poc_data = {
	.version = 3,
	.partition = hubble_poc_partition,
	.nr_partition = ARRAY_SIZE(hubble_poc_partition),
	.wdata_len = 256,
#ifdef CONFIG_SUPPORT_POC_SPI
	.conn_src = POC_CONN_SRC_SPI,
#endif
};
#endif /* __S6E3HAB_HUBBLE_PANEL_POC_H__ */
