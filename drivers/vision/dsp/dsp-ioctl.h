/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_IOCTL_H__
#define __DSP_IOCTL_H__

#include <linux/fs.h>

#include "dsp-common-type.h"

#define DSP_MAX_KERNEL_COUNT		(8)
#define DSP_MAX_KERNEL_NAME_SIZE	(32)
#define DSP_MAX_KERNEL_SIZE		\
	((DSP_MAX_KERNEL_NAME_SIZE + sizeof(int)) * DSP_MAX_KERNEL_COUNT)

struct dsp_context;

struct dsp_ioc_boot {
	unsigned int			pm_level;
	int				reserved[3];
	struct timespec			timestamp[4];
};

struct dsp_ioc_boot_secure {
	unsigned int			pm_level;
	unsigned int			mem_size;
	unsigned int			mem_addr;
	int				reserved[3];
	struct timespec			timestamp[4];
};

struct dsp_ioc_load_graph {
	unsigned int			version;
	unsigned int			param_size;
	unsigned long			param_addr;
	unsigned int			kernel_count;
	unsigned int			kernel_size;
	unsigned long			kernel_addr;
	unsigned char			request_qos;
	unsigned char			reserved1[3];
	int				reserved2;
	struct timespec			timestamp[4];
};

struct dsp_ioc_unload_graph {
	unsigned int			global_id;
	unsigned char			request_qos;
	unsigned char			reserved1[3];
	int				reserved2;
	struct timespec			timestamp[4];
};

struct dsp_ioc_execute_msg {
	unsigned int			version;
	unsigned int			size;
	unsigned long			addr;
	unsigned char			request_qos;
	unsigned char			reserved1[3];
	int				reserved2;
	struct timespec			timestamp[4];
};

struct dsp_ioc_control {
	unsigned int			version;
	unsigned int			control_id;
	unsigned int			size;
	unsigned long			addr;
	int				reserved[2];
	struct timespec			timestamp[4];
};

#define DSP_IOC_BOOT		_IOWR('D', 0, struct dsp_ioc_boot)
#define DSP_IOC_LOAD_GRAPH	_IOWR('D', 1, struct dsp_ioc_load_graph)
#define DSP_IOC_UNLOAD_GRAPH	_IOWR('D', 2, struct dsp_ioc_unload_graph)
#define DSP_IOC_EXECUTE_MSG	_IOWR('D', 3, struct dsp_ioc_execute_msg)
#define DSP_IOC_CONTROL		_IOWR('D', 4, struct dsp_ioc_control)
#define DSP_IOC_BOOT_SECURE	_IOWR('D', 7, struct dsp_ioc_boot_secure)

enum dsp_ioc_version {
	DSP_IOC_VBASE,
	DSP_IOC_V1,
	DSP_IOC_V2,
	DSP_IOC_V3,
};

union dsp_ioc_arg {
	struct dsp_ioc_boot			boot;
	struct dsp_ioc_load_graph		load;
	struct dsp_ioc_unload_graph		unload;
	struct dsp_ioc_execute_msg		execute;
	struct dsp_ioc_control			control;
	struct dsp_ioc_boot_secure		boot_sec;
};

struct dsp_ioctl_ops {
	int (*boot)(struct dsp_context *dctx, struct dsp_ioc_boot *args);
	int (*load_graph)(struct dsp_context *dctx,
			struct dsp_ioc_load_graph *args);
	int (*unload_graph)(struct dsp_context *dctx,
			struct dsp_ioc_unload_graph *args);
	int (*execute_msg)(struct dsp_context *dctx,
			struct dsp_ioc_execute_msg *args);
	int (*control)(struct dsp_context *dctx,
			struct dsp_ioc_control *args);
	int (*boot_secure)(struct dsp_context *dctx,
			struct dsp_ioc_boot_secure *args);
};

long dsp_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#if defined(CONFIG_COMPAT)
long dsp_compat_ioctl32(struct file *file, unsigned int cmd, unsigned long arg);
#else
static inline long dsp_compat_ioctl32(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	return 0;
}
#endif

#endif
