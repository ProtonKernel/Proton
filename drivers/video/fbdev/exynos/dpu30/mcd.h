/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS MCD HDR driver

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MCD_H__
#define __MCD_H__

#include <media/v4l2-device.h>


#define MAX_MCDHDR_CNT		16

#ifdef CONFIG_MCDHDR
struct saved_hdr_info {
	int fd;
	struct dma_buf *dma_buf;
	struct fd_hdr_header *header;
};
#endif

/*struct for dpp */
struct mcd_dpp_config {
#ifdef CONFIG_MCDHDR
	void *hdr_info;
#endif
};

/* struct for decon */
struct mcd_decon_reg_data {

#ifdef CONFIG_MCDHDR
	void *hdr_info[MAX_MCDHDR_CNT];
#endif
	int dm_update_flag;
};

struct mcd_decon_device {
#ifdef CONFIG_DYNAMIC_MIPI
	struct dynamic_mipi_info *dm_info;
#endif
#ifdef CONFIG_MCDHDR
	struct saved_hdr_info saved_hdr[MAX_MCDHDR_CNT];
#endif
};

/* struct for dpp */
struct mcd_dpp_device {
	struct v4l2_subdev *hdr_sd;
};

#endif //__MCD_H__
