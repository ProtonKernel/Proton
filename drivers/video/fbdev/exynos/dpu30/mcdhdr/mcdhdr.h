/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS MCD HDR driver

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef __MCDHDR_H__
#define __MCDHDR_H__

#include "../decon.h"

#define MCDHDR_DRV_NAME	"mcdhdr"

extern int hdr_log_level;

#define mcdhdr_err(fmt, ...)							\
	do {									\
		if (hdr_log_level >= 0) {					\
			pr_err(pr_fmt(MCDHDR_DRV_NAME ":E:%s: " fmt), __func__, ##__VA_ARGS__);			\
		}								\
	} while (0)

#define mcdhdr_info(fmt, ...)							\
	do {									\
		if (hdr_log_level >= 1)					\
			pr_info(pr_fmt(MCDHDR_DRV_NAME ":I:%s: " fmt), __func__, ##__VA_ARGS__);			\
	} while (0)

#define mcdhdr_dbg(fmt, ...)							\
	do {									\
		if (hdr_log_level >= 2)					\
			pr_info(pr_fmt(MCDHDR_DRV_NAME ":D:%s: " fmt), __func__, ##__VA_ARGS__);			\
	} while (0)


#define SUPPORT_WCG			0x01
#define SUPPORT_HDR10			(0x01 << 1)
#define SUPPORT_HDR10P			(0x01 << 2)

#define IS_SUPPORT_WCG(type)		(type & SUPPORT_WCG)
#define IS_SUPPORT_HDR10(type)		(type & SUPPORT_HDR10)
#define IS_SUPPORT_HDR10P(type)		(type & SUPPORT_HDR10P)


enum mcdhdr_pwr_state {
	MCDHDR_PWR_OFF = 0,
	MCDHDR_PWR_ON,
};

struct mcdhdr_device {
	int id;
	struct device *dev;
	void __iomem *regs;
	struct v4l2_subdev sd;
	int attr;
	enum mcdhdr_pwr_state state;
};

struct mcdhdr_win_config {
	void *hdr_info;
	enum dpp_hdr_standard hdr;
	enum dpp_csc_eq eq_mode;
};


enum HDR_HW_TYPE {
	HDR_HW_NONE = 0,
	HDR_HW_SLSI,
	HDR_HW_MCD,
};


/* fd_hdr_header from HWC */
struct fd_hdr_header {
	unsigned int total_bytesize;
	union fd_hdr_header_type {
		struct fd_hdr_header_type_unpack {
			unsigned char hw_type; // 0: None, 1: S.LSI, 2: MCD
			unsigned char layer_index;
			unsigned char log_level;
			unsigned char optional_flag;
		} unpack;
		unsigned int pack;
	} type;
	unsigned int sfr_con;
	union fd_hdr_header_lut {
		struct fd_hdr_header_lut_unpack {
			unsigned short shall; // # of new set
			unsigned short need; // # of continuous set
		} unpack;
		unsigned int pack;
	} num;
};


#endif //__MCDHDR_H__
