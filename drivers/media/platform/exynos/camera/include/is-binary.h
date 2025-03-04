/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_BINARY_H
#define IS_BINARY_H

#include "is-config.h"

#ifdef VENDER_PATH
#define IS_FW_PATH 			"/system/vendor/firmware/"
#define IS_FW_DUMP_PATH			"/data/vendor/camera/"
#define IS_SETFILE_SDCARD_PATH		"/data/vendor/camera/"
#define IS_FW_SDCARD			"/data/vendor/camera/is_fw.bin"
#define IS_FW				"is_fw.bin"
#define IS_LIB_PATH			"/system/vendor/firmware/"
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
#define IS_ISP_LIB_SDCARD_PATH		NULL
#else
#define IS_ISP_LIB_SDCARD_PATH		"/data/vendor/camera/"
#endif
#define IS_REAR_CAL_SDCARD_PATH		"/data/vendor/camera/"
#define IS_FRONT_CAL_SDCARD_PATH		"/data/media/0/"
#else
#define IS_FW_PATH 			"/system/vendor/firmware/"
#define IS_FW_DUMP_PATH			"/data/"
#define IS_SETFILE_SDCARD_PATH		"/data/vendor/camera/"
#define IS_FW_SDCARD			"/data/is_fw2.bin"
#define IS_FW				"is_fw2.bin"
#define IS_ISP_LIB_SDCARD_PATH		"/data/vendor/camera/"
#define IS_REAR_CAL_SDCARD_PATH		"/data/vendor/camera/"
#define IS_FRONT_CAL_SDCARD_PATH		"/data/vendor/camera/"
#endif

#ifdef USE_ONE_BINARY
#define IS_ISP_LIB				"is_lib.bin"
#else
#define IS_ISP_LIB				"is_lib_isp.bin"
#define IS_VRA_LIB				"is_lib_vra.bin"
#endif

#define IS_RTA_LIB				"is_rta.bin"

#define FD_SW_BIN_NAME				"is_fd.bin"
#define FD_SW_SDCARD				"/data/is_fd.bin"

/* use for LED calibration */
#define IS_LED_CAL_DATA_PATH              "/mnt/vendor/persist/camera/ledcal/rear"

/* static reserved memory for libraries */
#define CDH_SIZE		SZ_128K		/* CDH : Camera Debug Helper */

#ifdef CONFIG_KASAN
#define LIB_OFFSET		(VMALLOC_START + 0xF6000000 - 0x8000000)
#else
#define LIB_OFFSET		(VMALLOC_START + 0x1000000000UL + 0xF6000000 - 0x8000000)
#endif

#define __LIB_START		(LIB_OFFSET + 0x04000000 - CDH_SIZE)
#define LIB_START		(__LIB_START)

#define VRA_LIB_ADDR		(LIB_START + CDH_SIZE)
#define VRA_LIB_SIZE		(SZ_512K + SZ_256K)

#define DDK_LIB_ADDR		(LIB_START + VRA_LIB_SIZE + CDH_SIZE)
#define DDK_LIB_SIZE		((SZ_2M + SZ_1M + SZ_256K) + SZ_1M + RCHECKER_SIZE)

#define RTA_LIB_ADDR		(LIB_START + VRA_LIB_SIZE + DDK_LIB_SIZE + CDH_SIZE)
#define RTA_LIB_SIZE		(SZ_2M + SZ_2M)

/* Rule checker size for DDK */
#define RCHECKER_SIZE		(IS_RCHECKER_SIZE_RO + IS_RCHECKER_SIZE_RW)

#ifdef USE_RTA_BINARY
#define LIB_SIZE		(VRA_LIB_SIZE + DDK_LIB_SIZE +  RTA_LIB_SIZE + CDH_SIZE)
#else
#define LIB_SIZE		(VRA_LIB_SIZE + DDK_LIB_SIZE + CDH_SIZE)
#endif

#define HEAP_START		(LIB_START + SZ_16M)
#define HEAP_SIZE		(IS_HEAP_SIZE)

/* reserved memory for FIMC-IS */
#define SETFILE_SIZE		(IS_SETFILE_SIZE)
#define CALDATA_SIZE		(0x00010000)
#define LED_CAL_SIZE		(0x00000400)
#define TOTAL_CAL_DATA_SIZE	(CALDATA_SIZE + LED_CAL_SIZE)
#define DEBUG_REGION_SIZE	(0x0007D000)
#define EVENT_REGION_SIZE	(0x0007D000)
#define FSHARED_REGION_SIZE	(0x00010000)
#define DATA_REGION_SIZE	(0x00010000)
#define PARAM_REGION_SIZE	(0x00005000)	/* 20KB * instance(4) */

#define HEAP_RTA_START		(HEAP_START + SZ_64M + SZ_32M)		/* HEAP_SIZE(for DDK) should be smaller than 96MB */
#define HEAP_RTA_SIZE		(IS_RESERVE_LIB_SIZE)	/* 6MB ~ */
#define TAAISP_DMA_SIZE		(IS_TAAISP_SIZE)	/* 512KB */
#define MEDRC_DMA_SIZE		(TAAISP_MEDRC_SIZE)
#define ORBMCH_DMA_SIZE		(TAAISP_ORBMCH_SIZE)
#define TNR_DMA_SIZE		(TAAISP_TNR_SIZE)
#define TNR_S_DMA_SIZE		(TAAISP_TNR_S_SIZE)
#define CLAHE_DMA_SIZE		(IS_CLAHE_SIZE)
#define LHFD_MAP_SIZE		(0x009F0000)	/* 9.9375MB */
#define VRA_DMA_SIZE		(IS_VRA_SIZE)	/* 8MB */
#ifdef ENABLE_VRA_NETARRAY
#define VRA_NET_SIZE		(VRA_NET_ARR_SIZE)
#endif
#define MCSC_DNR_DMA_SIZE	(IS_MCSC_DNR_SIZE)
#define DUMMY_DMA_SIZE		(0)

/* for compatibility */
#define DEBUGCTL_OFFSET		(0)
#define DEBUG_REGION_OFFSET	(0)
#define DEBUG_REGION_SIZE	(0x0007D000) /* 500KB */
#define DEBUG_DUMP_SIZE	(0x00040000) /* 256KB */
#define DEBUG_HWDUMP_SIZE	(0x0003E800) /* 250KB */
#define TTB_OFFSET		(0)
#define TTB_SIZE		(0)
#define CAL_OFFSET0		(0x01FD0000)
#define CAL_OFFSET1		(0x01FE0000)

/* EEPROM offset */
#define EEPROM_HEADER_BASE	(0)
#define EEPROM_OEM_BASE		(0x100)
#define EEPROM_AWB_BASE		(0x200)
#define EEPROM_SHADING_BASE	(0x300)
#define EEPROM_PDAF_BASE	(0x0)

/* FROM offset */
#define FROM_HEADER_BASE	(0)
#define FROM_OEM_BASE		(0x1000)
#define FROM_AWB_BASE		(0x2000)
#define FROM_SHADING_BASE	(0x3000)
#define FROM_PDAF_BASE		(0x5000)

#ifdef USE_BINARY_PADDING_DATA_ADDED
#define IS_SIGNATURE_LEN	528
#else
#define IS_SIGNATURE_LEN	0
#endif

#define LIBRARY_VER_LEN		44
#define LIBRARY_VER_OFS		(LIBRARY_VER_LEN + IS_SIGNATURE_LEN)
#define SETFILE_VER_LEN		52
#define SETFILE_VER_OFS		64

#define IS_REAR_CAL			"rear_cal_data.bin"
#define IS_FRONT_CAL			"front_cal_data.bin"
#define IS_CAL_SDCARD			"/data/cal_data.bin"
#define IS_CAL_RETRY_CNT			(2)
#define IS_FW_RETRY_CNT			(2)
#define IS_CAL_VER_SIZE			(12)

/*
 * binary types for IS
 *
 * library based on type(DDK, RTA)
 * setfile based onsensor positon
 */
enum is_bin_type {
	IS_BIN_LIBRARY,
	IS_BIN_SETFILE,
};

struct is_bin_ver_info {
	enum is_bin_type type;
	unsigned int maxhint;
	unsigned int offset;
	unsigned int length;
	void *s;

	char *(*get_buf)(const struct is_bin_ver_info *info, unsigned int hint);
	unsigned int (*get_name_idx)(unsigned int hint);
};

#define IS_BIN_LIB_HINT_DDK	0
#define IS_BIN_LIB_HINT_RTA	1

struct is_binary {
	void *data;
	size_t size;

	const struct firmware *fw;

	unsigned long customized;

	/* request_firmware retry */
	unsigned int retry_cnt;
	int	retry_err;

	/* custom memory allocation */
	void *(*alloc)(unsigned long size);
	void (*free)(const void *buf);
};

ssize_t write_data_to_file(char *name, char *buf, size_t count, loff_t *pos);
int get_filesystem_binary(const char *filename, struct is_binary *bin);
int put_filesystem_binary(const char *filename, struct is_binary *bin, u32 flags);
void setup_binary_loader(struct is_binary *bin,
				unsigned int retry_cnt, int retry_err,
				void *(*alloc)(unsigned long size),
				void (*free)(const void *buf));
int request_binary(struct is_binary *bin, const char *path,
				const char *name, struct device *device);
void release_binary(struct is_binary *bin);
int was_loaded_by(struct is_binary *bin);

void carve_binary_version(enum is_bin_type type, unsigned int hint, struct is_binary *bin);
char *get_binary_version(enum is_bin_type type, unsigned int hint);

#endif
