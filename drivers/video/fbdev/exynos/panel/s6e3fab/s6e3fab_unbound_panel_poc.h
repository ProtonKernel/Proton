/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fab/s6e3fab_unbound_panel_poc.h
 *
 * Header file for POC Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FAB_UNBOUND_PANEL_POC_H__
#define __S6E3FAB_UNBOUND_PANEL_POC_H__

#include "../panel.h"
#include "../panel_poc.h"

#define UNBOUND_GM2_GAMMA_ADDR	(0x78000)
#define UNBOUND_GM2_GAMMA_SIZE	(0x40D)
#define UNBOUND_GM2_GAMMA_DATA_ADDR	(UNBOUND_GM2_GAMMA_ADDR)
#define UNBOUND_GM2_GAMMA_DATA_SIZE	(0x2B0)


#define UNBOUND_GM2_GAMMA_CHECKSUM_ADDR	(UNBOUND_GM2_GAMMA_ADDR + UNBOUND_GM2_GAMMA_DATA_SIZE)
#define UNBOUND_GM2_GAMMA_CHECKSUM_SIZE	(2)
#define UNBOUND_GM2_GAMMA_MAGICNUM_ADDR	(0x7840C)
#define UNBOUND_GM2_GAMMA_MAGICNUM_SIZE	(1)

#ifdef CONFIG_SUPPORT_POC_SPI
#define UNBOUND_SPI_ER_DONE_MDELAY		(35)
#define UNBOUND_SPI_STATUS_WR_DONE_MDELAY		(15)

#ifdef PANEL_POC_SPI_BUSY_WAIT
#define UNBOUND_SPI_WR_DONE_UDELAY		(50)
#else
#define UNBOUND_SPI_WR_DONE_UDELAY		(400)
#endif
#endif

/* ===================================================================================== */
/* ============================== [S6E3FAB MAPPING TABLE] ============================== */
/* ===================================================================================== */
static struct maptbl unbound_poc_maptbl[] = {
#if 0
#ifdef CONFIG_SUPPORT_POC_SPI
	[POC_SPI_READ_ADDR_MAPTBL] = DEFINE_0D_MAPTBL(unbound_poc_spi_read_table, init_common_table, NULL, copy_poc_rd_addr_maptbl),
	[POC_SPI_WRITE_ADDR_MAPTBL] = DEFINE_0D_MAPTBL(unbound_poc_spi_write_addr_table, init_common_table, NULL, copy_poc_wr_addr_maptbl),
	[POC_SPI_WRITE_DATA_MAPTBL] = DEFINE_0D_MAPTBL(unbound_poc_spi_write_data_table, init_common_table, NULL, copy_poc_wr_data_maptbl),
	[POC_SPI_ERASE_ADDR_MAPTBL] = DEFINE_0D_MAPTBL(unbound_poc_spi_erase_addr_table, init_common_table, NULL, copy_poc_er_addr_maptbl),
#endif
#endif
};

#ifdef CONFIG_SUPPORT_POC_SPI
static u8 UNBOUND_POC_SPI_SET_STATUS1_INIT[] = { 0x01, 0x00 };
static u8 UNBOUND_POC_SPI_SET_STATUS2_INIT[] = { 0x31, 0x00 };
static u8 UNBOUND_POC_SPI_SET_STATUS1_EXIT[] = { 0x01, 0x5C };
static u8 UNBOUND_POC_SPI_SET_STATUS2_EXIT[] = { 0x31, 0x02 };
static DEFINE_STATIC_PACKET(unbound_poc_spi_set_status1_init, SPI_PKT_TYPE_WR, UNBOUND_POC_SPI_SET_STATUS1_INIT, 0);
static DEFINE_STATIC_PACKET(unbound_poc_spi_set_status2_init, SPI_PKT_TYPE_WR, UNBOUND_POC_SPI_SET_STATUS2_INIT, 0);
static DEFINE_STATIC_PACKET(unbound_poc_spi_set_status1_exit, SPI_PKT_TYPE_WR, UNBOUND_POC_SPI_SET_STATUS1_EXIT, 0);
static DEFINE_STATIC_PACKET(unbound_poc_spi_set_status2_exit, SPI_PKT_TYPE_WR, UNBOUND_POC_SPI_SET_STATUS2_EXIT, 0);

static u8 UNBOUND_POC_SPI_STATUS1[] = { 0x05 };
static u8 UNBOUND_POC_SPI_STATUS2[] = { 0x35 };
static u8 UNBOUND_POC_SPI_READ[] = { 0x03, 0x00, 0x00, 0x00 };
static u8 UNBOUND_POC_SPI_ERASE_4K[] = { 0x20, 0x00, 0x00, 0x00 };
static u8 UNBOUND_POC_SPI_ERASE_32K[] = { 0x52, 0x00, 0x00, 0x00 };
static u8 UNBOUND_POC_SPI_ERASE_64K[] = { 0xD8, 0x00, 0x00, 0x00 };
static u8 UNBOUND_POC_SPI_WRITE_ENABLE[] = { 0x06 };
static u8 UNBOUND_POC_SPI_WRITE[260] = { 0x02, 0x00, 0x00, 0x00, };
static DEFINE_STATIC_PACKET(unbound_poc_spi_status1, SPI_PKT_TYPE_SETPARAM, UNBOUND_POC_SPI_STATUS1, 0);
static DEFINE_STATIC_PACKET(unbound_poc_spi_status2, SPI_PKT_TYPE_SETPARAM, UNBOUND_POC_SPI_STATUS2, 0);
static DEFINE_STATIC_PACKET(unbound_poc_spi_write_enable, SPI_PKT_TYPE_WR, UNBOUND_POC_SPI_WRITE_ENABLE, 0);
static DEFINE_PKTUI(unbound_poc_spi_erase_4k, &unbound_poc_maptbl[POC_SPI_ERASE_ADDR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound_poc_spi_erase_4k, SPI_PKT_TYPE_WR, UNBOUND_POC_SPI_ERASE_4K, 0);
static DEFINE_PKTUI(unbound_poc_spi_erase_32k, &unbound_poc_maptbl[POC_SPI_ERASE_ADDR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound_poc_spi_erase_32k, SPI_PKT_TYPE_WR, UNBOUND_POC_SPI_ERASE_32K, 0);
static DEFINE_PKTUI(unbound_poc_spi_erase_64k, &unbound_poc_maptbl[POC_SPI_ERASE_ADDR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound_poc_spi_erase_64k, SPI_PKT_TYPE_WR, UNBOUND_POC_SPI_ERASE_64K, 0);

static DECLARE_PKTUI(unbound_poc_spi_write) = {
	{ .offset = 1, .maptbl = &unbound_poc_maptbl[POC_SPI_WRITE_ADDR_MAPTBL] },
	{ .offset = 4, .maptbl = &unbound_poc_maptbl[POC_SPI_WRITE_DATA_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(unbound_poc_spi_write, SPI_PKT_TYPE_WR, UNBOUND_POC_SPI_WRITE, 0);
static DEFINE_PKTUI(unbound_poc_spi_rd_addr, &unbound_poc_maptbl[POC_SPI_READ_ADDR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(unbound_poc_spi_rd_addr, SPI_PKT_TYPE_SETPARAM, UNBOUND_POC_SPI_READ, 0);
static DEFINE_PANEL_UDELAY_NO_SLEEP(unbound_poc_spi_wait_write, UNBOUND_SPI_WR_DONE_UDELAY);
static DEFINE_PANEL_MDELAY(unbound_poc_spi_wait_erase, UNBOUND_SPI_ER_DONE_MDELAY);
static DEFINE_PANEL_MDELAY(unbound_poc_spi_wait_status, UNBOUND_SPI_STATUS_WR_DONE_MDELAY);
#endif

#ifdef CONFIG_SUPPORT_POC_SPI
static void *unbound_poc_spi_init_cmdtbl[] = {
	&PKTINFO(unbound_poc_spi_write_enable),
	&PKTINFO(unbound_poc_spi_set_status1_init),
	&DLYINFO(unbound_poc_spi_wait_status),
	&PKTINFO(unbound_poc_spi_write_enable),
	&PKTINFO(unbound_poc_spi_set_status2_init),
	&DLYINFO(unbound_poc_spi_wait_status),
};

static void *unbound_poc_spi_exit_cmdtbl[] = {
	&PKTINFO(unbound_poc_spi_write_enable),
	&PKTINFO(unbound_poc_spi_set_status1_exit),
	&DLYINFO(unbound_poc_spi_wait_status),
	&PKTINFO(unbound_poc_spi_write_enable),
	&PKTINFO(unbound_poc_spi_set_status2_exit),
	&DLYINFO(unbound_poc_spi_wait_status),
};

static void *unbound_poc_spi_read_cmdtbl[] = {
	&PKTINFO(unbound_poc_spi_rd_addr),
	&s6e3fab_restbl[RES_POC_SPI_READ],
};

static void *unbound_poc_spi_erase_4k_cmdtbl[] = {
	&PKTINFO(unbound_poc_spi_write_enable),
	&PKTINFO(unbound_poc_spi_erase_4k),
};

static void *unbound_poc_spi_erase_32k_cmdtbl[] = {
	&PKTINFO(unbound_poc_spi_write_enable),
	&PKTINFO(unbound_poc_spi_erase_32k),
};

static void *unbound_poc_spi_erase_64k_cmdtbl[] = {
	&PKTINFO(unbound_poc_spi_write_enable),
	&PKTINFO(unbound_poc_spi_erase_64k),
};

static void *unbound_poc_spi_write_cmdtbl[] = {
	&PKTINFO(unbound_poc_spi_write_enable),
	&PKTINFO(unbound_poc_spi_write),
#ifndef PANEL_POC_SPI_BUSY_WAIT
	&DLYINFO(unbound_poc_spi_wait_write),
#endif
};

static void *unbound_poc_spi_status_cmdtbl[] = {
	&PKTINFO(unbound_poc_spi_status1),
	&s6e3fab_restbl[RES_POC_SPI_STATUS1],
	&PKTINFO(unbound_poc_spi_status2),
	&s6e3fab_restbl[RES_POC_SPI_STATUS2],
};

static void *unbound_poc_spi_wait_write_cmdtbl[] = {
	&DLYINFO(unbound_poc_spi_wait_write),
};

static void *unbound_poc_spi_wait_erase_cmdtbl[] = {
	&DLYINFO(unbound_poc_spi_wait_erase),
};
#endif

static struct seqinfo unbound_poc_seqtbl[MAX_POC_SEQ] = {
#ifdef CONFIG_SUPPORT_POC_SPI
	[POC_SPI_INIT_SEQ] = SEQINFO_INIT("poc-spi-init-seq", unbound_poc_spi_init_cmdtbl),
	[POC_SPI_EXIT_SEQ] = SEQINFO_INIT("poc-spi-exit-seq", unbound_poc_spi_exit_cmdtbl),
	[POC_SPI_READ_SEQ] = SEQINFO_INIT("poc-spi-read-seq", unbound_poc_spi_read_cmdtbl),
	[POC_SPI_ERASE_4K_SEQ] = SEQINFO_INIT("poc-spi-erase-4k-seq", unbound_poc_spi_erase_4k_cmdtbl),
	[POC_SPI_ERASE_32K_SEQ] = SEQINFO_INIT("poc-spi-erase-4k-seq", unbound_poc_spi_erase_32k_cmdtbl),
	[POC_SPI_ERASE_64K_SEQ] = SEQINFO_INIT("poc-spi-erase-32k-seq", unbound_poc_spi_erase_64k_cmdtbl),
	[POC_SPI_WRITE_SEQ] = SEQINFO_INIT("poc-spi-write-seq", unbound_poc_spi_write_cmdtbl),
	[POC_SPI_STATUS_SEQ] = SEQINFO_INIT("poc-spi-status-seq", unbound_poc_spi_status_cmdtbl),
	[POC_SPI_WAIT_WRITE_SEQ] = SEQINFO_INIT("poc-spi-wait-write-seq", unbound_poc_spi_wait_write_cmdtbl),
	[POC_SPI_WAIT_ERASE_SEQ] = SEQINFO_INIT("poc-spi-wait-erase-seq", unbound_poc_spi_wait_erase_cmdtbl),
#endif
};

/* partition consists of DATA, CHECKSUM and MAGICNUM */
static struct poc_partition unbound_poc_partition[] = {
	[POC_GM2_GAMMA_PARTITION] = {
		.name = "gm2_gamma",
		.addr = UNBOUND_GM2_GAMMA_ADDR,
		.size = UNBOUND_GM2_GAMMA_SIZE,
		.data = {
			{
				.data_addr = UNBOUND_GM2_GAMMA_DATA_ADDR,
				.data_size = UNBOUND_GM2_GAMMA_DATA_SIZE,
			},
		},
		.checksum_addr = UNBOUND_GM2_GAMMA_CHECKSUM_ADDR,
		.checksum_size = UNBOUND_GM2_GAMMA_CHECKSUM_SIZE,
		.magicnum_addr = UNBOUND_GM2_GAMMA_MAGICNUM_ADDR,
		.magicnum_size = UNBOUND_GM2_GAMMA_MAGICNUM_SIZE,
		.magicnum = 0x01,
		.need_preload = true
	},
};

static struct panel_poc_data s6e3fab_unbound_poc_data = {
	.version = 3,
	.seqtbl = unbound_poc_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(unbound_poc_seqtbl),
	.maptbl = unbound_poc_maptbl,
	.nr_maptbl = ARRAY_SIZE(unbound_poc_maptbl),
	.partition = unbound_poc_partition,
	.nr_partition = ARRAY_SIZE(unbound_poc_partition),
	.wdata_len = 256,
#ifdef CONFIG_SUPPORT_POC_SPI
	.spi_wdata_len = 256,
	.conn_src = POC_CONN_SRC_SPI,
	.state_mask = 0x025C,
	.state_init = 0x0000,
	.state_uninit = 0x025C,
	.busy_mask = 0x0001,
#endif
};
#endif /* __S6E3FAB_UNBOUND_PANEL_POC_H__ */
