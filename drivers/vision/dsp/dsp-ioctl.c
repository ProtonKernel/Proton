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

static int __dsp_ioctl_get_boot(struct dsp_ioc_boot *karg,
		struct dsp_ioc_boot __user *uarg)
{
	int ret;

	dsp_enter();
	if (copy_from_user(karg, uarg, sizeof(*uarg))) {
		ret = -EFAULT;
		dsp_err("Failed to copy from user at boot(%d)\n", ret);
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_ioctl_put_boot(struct dsp_ioc_boot *karg,
		struct dsp_ioc_boot __user *uarg)
{
	int ret;

	dsp_enter();
	ret = copy_to_user(uarg, karg, sizeof(*karg));
	if (ret)
		dsp_err("Failed to copy to user at boot(%d)\n", ret);

	dsp_leave();
}

static int __dsp_ioctl_get_load_graph(struct dsp_ioc_load_graph *karg,
		struct dsp_ioc_load_graph __user *uarg)
{
	int ret;

	dsp_enter();
	if (copy_from_user(karg, uarg, sizeof(*uarg))) {
		ret = -EFAULT;
		dsp_err("Failed to copy from user at load(%d)\n", ret);
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_ioctl_put_load_graph(struct dsp_ioc_load_graph *karg,
		struct dsp_ioc_load_graph __user *uarg)
{
	int ret;

	dsp_enter();
	ret = copy_to_user(uarg, karg, sizeof(*karg));
	if (ret)
		dsp_err("Failed to copy to user at load(%d)\n", ret);

	dsp_leave();
}

static int __dsp_ioctl_get_unload_graph(struct dsp_ioc_unload_graph *karg,
		struct dsp_ioc_unload_graph __user *uarg)
{
	int ret;

	dsp_enter();
	if (copy_from_user(karg, uarg, sizeof(*uarg))) {
		ret = -EFAULT;
		dsp_err("Failed to copy from user at unload(%d)\n", ret);
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_ioctl_put_unload_graph(struct dsp_ioc_unload_graph *karg,
		struct dsp_ioc_unload_graph __user *uarg)
{
	int ret;

	dsp_enter();
	ret = copy_to_user(uarg, karg, sizeof(*karg));
	if (ret)
		dsp_err("Failed to copy to user at unload(%d)\n", ret);

	dsp_leave();
}

static int __dsp_ioctl_get_execute_msg(struct dsp_ioc_execute_msg *karg,
		struct dsp_ioc_execute_msg __user *uarg)
{
	int ret;

	dsp_enter();
	if (copy_from_user(karg, uarg, sizeof(*uarg))) {
		ret = -EFAULT;
		dsp_err("Failed to copy from user at execute(%d)\n", ret);
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_ioctl_put_execute_msg(struct dsp_ioc_execute_msg *karg,
		struct dsp_ioc_execute_msg __user *uarg)
{
	int ret;

	dsp_enter();
	ret = copy_to_user(uarg, karg, sizeof(*karg));
	if (ret)
		dsp_err("Failed to copy to user at execute(%d)\n", ret);

	dsp_leave();
}

static int __dsp_ioctl_get_control(struct dsp_ioc_control *karg,
		struct dsp_ioc_control __user *uarg)
{
	int ret;

	dsp_enter();
	if (copy_from_user(karg, uarg, sizeof(*uarg))) {
		ret = -EFAULT;
		dsp_err("Failed to copy from user at control(%d)\n", ret);
		goto p_err;
	}

	memset(karg->timestamp, 0, sizeof(karg->timestamp));

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_ioctl_put_control(struct dsp_ioc_control *karg,
		struct dsp_ioc_control __user *uarg)
{
	int ret;

	dsp_enter();
	ret = copy_to_user(uarg, karg, sizeof(*karg));
	if (ret)
		dsp_err("Failed to copy to user at control(%d)\n", ret);

	dsp_leave();
}

static int __dsp_ioctl_get_boot_secure(struct dsp_ioc_boot_secure *karg,
		struct dsp_ioc_boot_secure __user *uarg)
{
	int ret;

	dsp_enter();
	if (copy_from_user(karg, uarg, sizeof(*uarg))) {
		ret = -EFAULT;
		dsp_err("Failed to copy from user at boot_secure(%d)\n", ret);
		goto p_err;
	}

	karg->mem_addr = 0;
	memset(karg->timestamp, 0, sizeof(karg->timestamp));

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_ioctl_put_boot_secure(struct dsp_ioc_boot_secure *karg,
		struct dsp_ioc_boot_secure __user *uarg)
{
	int ret;

	dsp_enter();
	ret = copy_to_user(uarg, karg, sizeof(*karg));
	if (ret)
		dsp_err("Failed to copy to user at boot_secure(%d)\n", ret);

	dsp_leave();
}

long dsp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct dsp_context *dctx;
	const struct dsp_ioctl_ops *ops;
	union dsp_ioc_arg karg;
	void __user *uarg;

	dsp_enter();
	dctx = file->private_data;
	ops = dctx->core->ioctl_ops;
	uarg = (void __user *)arg;

	switch (cmd) {
	case DSP_IOC_BOOT:
		ret = __dsp_ioctl_get_boot(&karg.boot, uarg);
		if (ret)
			goto p_err;

		ret = ops->boot(dctx, &karg.boot);
		__dsp_ioctl_put_boot(&karg.boot, uarg);
		break;
	case DSP_IOC_LOAD_GRAPH:
		ret = __dsp_ioctl_get_load_graph(&karg.load, uarg);
		if (ret)
			goto p_err;

		ret = ops->load_graph(dctx, &karg.load);
		__dsp_ioctl_put_load_graph(&karg.load, uarg);
		break;
	case DSP_IOC_UNLOAD_GRAPH:
		ret = __dsp_ioctl_get_unload_graph(&karg.unload, uarg);
		if (ret)
			goto p_err;

		ret = ops->unload_graph(dctx, &karg.unload);
		__dsp_ioctl_put_unload_graph(&karg.unload, uarg);
		break;
	case DSP_IOC_EXECUTE_MSG:
		ret = __dsp_ioctl_get_execute_msg(&karg.execute, uarg);
		if (ret)
			goto p_err;

		ret = ops->execute_msg(dctx, &karg.execute);
		__dsp_ioctl_put_execute_msg(&karg.execute, uarg);
		break;
	case DSP_IOC_CONTROL:
		ret = __dsp_ioctl_get_control(&karg.control, uarg);
		if (ret)
			goto p_err;

		ret = ops->control(dctx, &karg.control);
		__dsp_ioctl_put_control(&karg.control, uarg);
		break;
	case DSP_IOC_BOOT_SECURE:
		ret = __dsp_ioctl_get_boot_secure(&karg.boot_sec, uarg);
		if (ret)
			goto p_err;

		ret = ops->boot_secure(dctx, &karg.boot_sec);
		__dsp_ioctl_put_boot_secure(&karg.boot_sec, uarg);
		break;
	default:
		ret = -EINVAL;
		dsp_err("ioc command(%c/%u/%u) is not supported\n",
				_IOC_TYPE(cmd), _IOC_NR(cmd), _IOC_SIZE(cmd));
		goto p_err;
	}

	dsp_leave();
p_err:
	return ret;
}
