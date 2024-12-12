// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/uaccess.h>

#include "dsp-log.h"
#include "dsp-core.h"
#include "dsp-context.h"
#include "dsp-ioctl.h"

struct dsp_ioc_boot32 {
	unsigned int			pm_level;
	int				reserved[3];
	struct compat_timespec		timestamp[4];
};

struct dsp_ioc_load_graph32 {
	unsigned int			version;
	unsigned int			param_size;
	compat_caddr_t			param_addr;
	unsigned int			kernel_count;
	unsigned int			kernel_size;
	compat_caddr_t			kernel_addr;
	unsigned char			request_qos;
	unsigned char			reserved1[3];
	int				reserved2;
	struct compat_timespec		timestamp[4];
};

struct dsp_ioc_boot_secure32 {
	unsigned int			pm_level;
	unsigned int			mem_size;
	unsigned int			mem_addr;
	int				reserved[3];
	struct compat_timespec		timestamp[4];
};

struct dsp_ioc_unload_graph32 {
	unsigned int			global_id;
	unsigned char			request_qos;
	unsigned char			reserved1[3];
	int				reserved2;
	struct compat_timespec		timestamp[4];
};

struct dsp_ioc_execute_msg32 {
	unsigned int			version;
	unsigned int			size;
	compat_caddr_t			addr;
	unsigned char			request_qos;
	unsigned char			reserved1[3];
	int				reserved2;
	struct compat_timespec		timestamp[4];
};

struct dsp_ioc_control32 {
	unsigned int			version;
	unsigned int			control_id;
	unsigned int			size;
	compat_caddr_t			addr;
	int				reserved[2];
	struct compat_timespec		timestamp[4];
};

#define DSP_IOC_BOOT32		_IOWR('D', 0, struct dsp_ioc_boot32)
#define DSP_IOC_LOAD_GRAPH32	_IOWR('D', 1, struct dsp_ioc_load_graph32)
#define DSP_IOC_UNLOAD_GRAPH32	_IOWR('D', 2, struct dsp_ioc_unload_graph32)
#define DSP_IOC_EXECUTE_MSG32	_IOWR('D', 3, struct dsp_ioc_execute_msg32)
#define DSP_IOC_CONTROL32	_IOWR('D', 4, struct dsp_ioc_control32)
#define DSP_IOC_BOOT_SECURE32	_IOWR('D', 7, struct dsp_ioc_boot_secure32)

static int __dsp_ioctl_get_boot32(struct dsp_ioc_boot *karg,
		struct dsp_ioc_boot32 __user *uarg)
{
	int ret;

	dsp_enter();
	if (get_user(karg->pm_level, &uarg->pm_level)) {
		ret = -EFAULT;
		dsp_err("Failed to copy from user at boot32\n");
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_ioctl_put_boot32(struct dsp_ioc_boot *karg,
		struct dsp_ioc_boot32 __user *uarg)
{
	dsp_enter();
	if (put_user(karg->timestamp[0].tv_sec,
				&uarg->timestamp[0].tv_sec) ||
			put_user(karg->timestamp[0].tv_nsec,
				&uarg->timestamp[0].tv_nsec) ||
			put_user(karg->timestamp[1].tv_sec,
				&uarg->timestamp[1].tv_sec) ||
			put_user(karg->timestamp[1].tv_nsec,
				&uarg->timestamp[1].tv_nsec) ||
			put_user(karg->timestamp[2].tv_sec,
				&uarg->timestamp[2].tv_sec) ||
			put_user(karg->timestamp[2].tv_nsec,
				&uarg->timestamp[2].tv_nsec) ||
			put_user(karg->timestamp[3].tv_sec,
				&uarg->timestamp[3].tv_sec) ||
			put_user(karg->timestamp[3].tv_nsec,
				&uarg->timestamp[3].tv_nsec)) {
		dsp_err("Failed to copy to user at boot32\n");
	}

	dsp_leave();
}

static int __dsp_ioctl_get_load_graph32(struct dsp_ioc_load_graph *karg,
		struct dsp_ioc_load_graph32 __user *uarg)
{
	int ret;

	dsp_enter();
	if (get_user(karg->version, &uarg->version) ||
			get_user(karg->param_size, &uarg->param_size) ||
			get_user(karg->param_addr, &uarg->param_addr) ||
			get_user(karg->kernel_count, &uarg->kernel_count) ||
			get_user(karg->kernel_size, &uarg->kernel_size) ||
			get_user(karg->kernel_addr, &uarg->kernel_addr) ||
			get_user(karg->request_qos, &uarg->request_qos)) {
		ret = -EFAULT;
		dsp_err("Failed to copy from user at load32\n");
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_ioctl_put_load_graph32(struct dsp_ioc_load_graph *karg,
		struct dsp_ioc_load_graph32 __user *uarg)
{
	dsp_enter();
	if (put_user(karg->timestamp[0].tv_sec,
				&uarg->timestamp[0].tv_sec) ||
			put_user(karg->timestamp[0].tv_nsec,
				&uarg->timestamp[0].tv_nsec) ||
			put_user(karg->timestamp[1].tv_sec,
				&uarg->timestamp[1].tv_sec) ||
			put_user(karg->timestamp[1].tv_nsec,
				&uarg->timestamp[1].tv_nsec) ||
			put_user(karg->timestamp[2].tv_sec,
				&uarg->timestamp[2].tv_sec) ||
			put_user(karg->timestamp[2].tv_nsec,
				&uarg->timestamp[2].tv_nsec) ||
			put_user(karg->timestamp[3].tv_sec,
				&uarg->timestamp[3].tv_sec) ||
			put_user(karg->timestamp[3].tv_nsec,
				&uarg->timestamp[3].tv_nsec)) {
		dsp_err("Failed to copy to user at load32\n");
	}

	dsp_leave();
}

static int __dsp_ioctl_get_unload_graph32(struct dsp_ioc_unload_graph *karg,
		struct dsp_ioc_unload_graph32 __user *uarg)
{
	int ret;

	dsp_enter();
	if (get_user(karg->global_id, &uarg->global_id) ||
			get_user(karg->request_qos, &uarg->request_qos)) {
		ret = -EFAULT;
		dsp_err("Failed to copy from user at unload32\n");
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_ioctl_put_unload_graph32(struct dsp_ioc_unload_graph *karg,
		struct dsp_ioc_unload_graph32 __user *uarg)
{
	dsp_enter();
	if (put_user(karg->timestamp[0].tv_sec,
				&uarg->timestamp[0].tv_sec) ||
			put_user(karg->timestamp[0].tv_nsec,
				&uarg->timestamp[0].tv_nsec) ||
			put_user(karg->timestamp[1].tv_sec,
				&uarg->timestamp[1].tv_sec) ||
			put_user(karg->timestamp[1].tv_nsec,
				&uarg->timestamp[1].tv_nsec) ||
			put_user(karg->timestamp[2].tv_sec,
				&uarg->timestamp[2].tv_sec) ||
			put_user(karg->timestamp[2].tv_nsec,
				&uarg->timestamp[2].tv_nsec) ||
			put_user(karg->timestamp[3].tv_sec,
				&uarg->timestamp[3].tv_sec) ||
			put_user(karg->timestamp[3].tv_nsec,
				&uarg->timestamp[3].tv_nsec)) {
		dsp_err("Failed to copy to user at unload32\n");
	}

	dsp_leave();
}

static int __dsp_ioctl_get_execute_msg32(struct dsp_ioc_execute_msg *karg,
		struct dsp_ioc_execute_msg32 __user *uarg)
{
	int ret;

	dsp_enter();
	if (get_user(karg->version, &uarg->version) ||
			get_user(karg->size, &uarg->size) ||
			get_user(karg->addr, &uarg->addr) ||
			get_user(karg->request_qos, &uarg->request_qos)) {
		ret = -EFAULT;
		dsp_err("Failed to copy from user at execute32\n");
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_ioctl_put_execute_msg32(struct dsp_ioc_execute_msg *karg,
		struct dsp_ioc_execute_msg32 __user *uarg)
{
	dsp_enter();
	if (put_user(karg->timestamp[0].tv_sec,
				&uarg->timestamp[0].tv_sec) ||
			put_user(karg->timestamp[0].tv_nsec,
				&uarg->timestamp[0].tv_nsec) ||
			put_user(karg->timestamp[1].tv_sec,
				&uarg->timestamp[1].tv_sec) ||
			put_user(karg->timestamp[1].tv_nsec,
				&uarg->timestamp[1].tv_nsec) ||
			put_user(karg->timestamp[2].tv_sec,
				&uarg->timestamp[2].tv_sec) ||
			put_user(karg->timestamp[2].tv_nsec,
				&uarg->timestamp[2].tv_nsec) ||
			put_user(karg->timestamp[3].tv_sec,
				&uarg->timestamp[3].tv_sec) ||
			put_user(karg->timestamp[3].tv_nsec,
				&uarg->timestamp[3].tv_nsec)) {
		dsp_err("Failed to copy to user at execute32\n");
	}

	dsp_leave();
}

static int __dsp_ioctl_get_control32(struct dsp_ioc_control *karg,
		struct dsp_ioc_control32 __user *uarg)
{
	int ret;

	dsp_enter();
	if (get_user(karg->version, &uarg->version) ||
			get_user(karg->control_id, &uarg->control_id) ||
			get_user(karg->size, &uarg->size) ||
			get_user(karg->addr, &uarg->addr)) {
		ret = -EFAULT;
		dsp_err("Failed to copy from user at control32\n");
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_ioctl_put_control32(struct dsp_ioc_control *karg,
		struct dsp_ioc_control32 __user *uarg)
{
	dsp_enter();
	if (put_user(karg->timestamp[0].tv_sec,
				&uarg->timestamp[0].tv_sec) ||
			put_user(karg->timestamp[0].tv_nsec,
				&uarg->timestamp[0].tv_nsec) ||
			put_user(karg->timestamp[1].tv_sec,
				&uarg->timestamp[1].tv_sec) ||
			put_user(karg->timestamp[1].tv_nsec,
				&uarg->timestamp[1].tv_nsec) ||
			put_user(karg->timestamp[2].tv_sec,
				&uarg->timestamp[2].tv_sec) ||
			put_user(karg->timestamp[2].tv_nsec,
				&uarg->timestamp[2].tv_nsec) ||
			put_user(karg->timestamp[3].tv_sec,
				&uarg->timestamp[3].tv_sec) ||
			put_user(karg->timestamp[3].tv_nsec,
				&uarg->timestamp[3].tv_nsec)) {
		dsp_err("Failed to copy to user at control32\n");
	}

	dsp_leave();
}

static int __dsp_ioctl_get_boot_secure32(struct dsp_ioc_boot_secure *karg,
		struct dsp_ioc_boot_secure32 __user *uarg)
{
	int ret;

	dsp_enter();
	if (get_user(karg->pm_level, &uarg->pm_level) ||
			get_user(karg->mem_size, &uarg->mem_size)) {
		ret = -EFAULT;
		dsp_err("Failed to copy from user at boot_secure32\n");
		goto p_err;
	}

	karg->mem_addr = 0;
	memset(karg->timestamp, 0, sizeof(karg->timestamp));

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_ioctl_put_boot_secure32(struct dsp_ioc_boot_secure *karg,
		struct dsp_ioc_boot_secure32 __user *uarg)
{
	dsp_enter();
	if (put_user(karg->mem_addr, &uarg->mem_addr) ||
			put_user(karg->timestamp[0].tv_sec,
				&uarg->timestamp[0].tv_sec) ||
			put_user(karg->timestamp[0].tv_nsec,
				&uarg->timestamp[0].tv_nsec) ||
			put_user(karg->timestamp[1].tv_sec,
				&uarg->timestamp[1].tv_sec) ||
			put_user(karg->timestamp[1].tv_nsec,
				&uarg->timestamp[1].tv_nsec) ||
			put_user(karg->timestamp[2].tv_sec,
				&uarg->timestamp[2].tv_sec) ||
			put_user(karg->timestamp[2].tv_nsec,
				&uarg->timestamp[2].tv_nsec) ||
			put_user(karg->timestamp[3].tv_sec,
				&uarg->timestamp[3].tv_sec) ||
			put_user(karg->timestamp[3].tv_nsec,
				&uarg->timestamp[3].tv_nsec)) {
		dsp_err("Failed to copy to user at boot_secure32\n");
	}

	dsp_leave();
}

long dsp_compat_ioctl32(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct dsp_context *dctx;
	const struct dsp_ioctl_ops *ops;
	union dsp_ioc_arg karg;
	void __user *compat_arg;

	dsp_enter();
	dctx = file->private_data;
	ops = dctx->core->ioctl_ops;
	compat_arg = (void __user *)arg;

	switch (cmd) {
	case DSP_IOC_BOOT32:
		ret = __dsp_ioctl_get_boot32(&karg.boot, compat_arg);
		if (ret)
			goto p_err;

		ret = ops->boot(dctx, &karg.boot);
		__dsp_ioctl_put_boot32(&karg.boot, compat_arg);
		break;
	case DSP_IOC_LOAD_GRAPH32:
		ret = __dsp_ioctl_get_load_graph32(&karg.load, compat_arg);
		if (ret)
			goto p_err;

		ret = ops->load_graph(dctx, &karg.load);
		__dsp_ioctl_put_load_graph32(&karg.load, compat_arg);
		break;
	case DSP_IOC_UNLOAD_GRAPH32:
		ret = __dsp_ioctl_get_unload_graph32(&karg.unload, compat_arg);
		if (ret)
			goto p_err;

		ret = ops->unload_graph(dctx, &karg.unload);
		__dsp_ioctl_put_unload_graph32(&karg.unload, compat_arg);
		break;
	case DSP_IOC_EXECUTE_MSG32:
		ret = __dsp_ioctl_get_execute_msg32(&karg.execute, compat_arg);
		if (ret)
			goto p_err;

		ret = ops->execute_msg(dctx, &karg.execute);
		__dsp_ioctl_put_execute_msg32(&karg.execute, compat_arg);
		break;
	case DSP_IOC_CONTROL32:
		ret = __dsp_ioctl_get_control32(&karg.control, compat_arg);
		if (ret)
			goto p_err;

		ret = ops->control(dctx, &karg.control);
		__dsp_ioctl_put_control32(&karg.control, compat_arg);
		break;
	case DSP_IOC_BOOT_SECURE32:
		ret = __dsp_ioctl_get_boot_secure32(&karg.boot_sec, compat_arg);
		if (ret)
			goto p_err;

		ret = ops->boot_secure(dctx, &karg.boot_sec);
		__dsp_ioctl_put_boot_secure32(&karg.boot_sec, compat_arg);
		break;
	default:
		ret = -EINVAL;
		dsp_err("compat ioc command(%c/%u/%u) is not supported\n",
				_IOC_TYPE(cmd), _IOC_NR(cmd), _IOC_SIZE(cmd));
		goto p_err;
	}

	dsp_leave();
p_err:
	return ret;
}
