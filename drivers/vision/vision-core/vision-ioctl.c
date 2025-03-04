/*
 * Samsung Exynos SoC series VPU driver
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/ktime.h>

#include "vision-config.h"
#include "vision-dev.h"
#include "vision-ioctl.h"

static int get_vs4l_ctrl64(struct vs4l_ctrl *kp, struct vs4l_ctrl __user *up)
{
	int ret;

	if (!access_ok(up, sizeof(struct vs4l_ctrl))) {
		vision_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err_ctrl;
	}

	ret = copy_from_user(kp, (void __user *)up, sizeof(struct vs4l_ctrl));
	if (ret) {
		vision_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err_ctrl;
	}

p_err_ctrl:
	return ret;
}

static void put_vs4l_ctrl64(struct vs4l_ctrl *kp, struct vs4l_ctrl __user *up)
{
}


static int get_vs4l_graph64(struct vs4l_graph *kp, struct vs4l_graph __user *up)
{
	int ret;

	if (!access_ok(up, sizeof(struct vs4l_graph))) {
		vision_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err_graph;
	}

	ret = copy_from_user(kp, (void __user *)up, sizeof(struct vs4l_graph));
	if (ret) {
		vision_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err_graph;
	}

p_err_graph:
	return ret;
}

static void put_vs4l_graph64(struct vs4l_graph *kp, struct vs4l_graph __user *up)
{
}

static int get_vs4l_sched_param64(struct vs4l_sched_param *kp, struct vs4l_sched_param __user *up)
{
	int ret;

	if (!access_ok(up, sizeof(struct vs4l_sched_param))) {
		vision_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err;
	}

	ret = copy_from_user(kp, (void __user *)up, sizeof(struct vs4l_sched_param));
	if (ret) {
		vision_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err;
	}

p_err:
	return ret;
}

static void put_vs4l_sched_param64(struct vs4l_sched_param *kp, struct vs4l_sched_param __user *up)
{
}

static int get_vs4l_format64(struct vs4l_format_list *kp, struct vs4l_format_list __user *up)
{
	int ret = 0;
	size_t size = 0;
	struct vs4l_format *kformats_ptr;

	if (!access_ok(up, sizeof(struct vs4l_format_list))) {
		vision_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err_format;
	}

	ret = copy_from_user((void *)kp, (void __user *)up,
				sizeof(struct vs4l_format_list));
	if (ret) {
		vision_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err_format;
	}

	if (kp->count > VISION_MAX_BUFFER) {
		vision_err("kp->count(%u) cannot be greater to VISION_MAX_BUFFER(%d)\n", kp->count, VISION_MAX_BUFFER);
		ret = -EINVAL;
		goto p_err_format;
	}

	size = kp->count * sizeof(struct vs4l_format);
	if (!access_ok((void __user *)kp->formats, size)) {
		vision_err("acesss is fail\n");
		ret = -EFAULT;
		goto p_err_format;
	}

	kformats_ptr = kmalloc(size, GFP_KERNEL);
	if (!kformats_ptr) {
		ret = -ENOMEM;
		goto p_err_format;
	}

	ret = copy_from_user(kformats_ptr, (void __user *)kp->formats, size);
	if (ret) {
		vision_err("copy_from_user failed(%d)\n", ret);
		kfree(kformats_ptr);
		ret = -EFAULT;
		goto p_err_format;
	}

	kp->formats = kformats_ptr;

p_err_format:
	return ret;
}

static void put_vs4l_format64(struct vs4l_format_list *kp, struct vs4l_format_list __user *up)
{
	kfree(kp->formats);
}

static int get_vs4l_param64(struct vs4l_param_list *kp, struct vs4l_param_list __user *up)
{
	int ret;
	size_t size = 0;
	struct vs4l_param *kparams_ptr;

	if (!access_ok(up, sizeof(struct vs4l_param_list))) {
		vision_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err_param;
	}

	ret = copy_from_user(kp, (void __user *)up, sizeof(struct vs4l_param_list));
	if (ret) {
		vision_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err_param;
	}

	if (kp->count > VISION_MAX_BUFFER) {
		vision_err("kp->count(%u) cannot be greater to VISION_MAX_BUFFER(%d)\n", kp->count, VISION_MAX_BUFFER);
		ret = -EINVAL;
		goto p_err_param;
	}

	size = kp->count * sizeof(struct vs4l_param);
	if (!access_ok((void __user *)kp->params, size)) {
		vision_err("access failed\n");
		ret = -EFAULT;
		goto p_err_param;
	}

	kparams_ptr = kmalloc(size, GFP_KERNEL);
	if (!kparams_ptr) {
		ret = -ENOMEM;
		goto p_err_param;
	}

	ret = copy_from_user(kparams_ptr, (void __user *)kp->params, size);
	if (ret) {
		vision_err("copy_from_user failed(%d)\n", ret);
		kfree(kparams_ptr);
		ret = -EFAULT;
		goto p_err_param;
	}

	kp->params = kparams_ptr;

p_err_param:
	return ret;
}

static void put_vs4l_param64(struct vs4l_param_list *kp, struct vs4l_param_list __user *up)
{
	kfree(kp->params);
}

static int get_vs4l_container64(struct vs4l_container_list *kp, struct vs4l_container_list __user *up)
{
	int ret, i, free_buf_num;
	size_t size = 0;
	struct vs4l_container *kcontainer_ptr;
	struct vs4l_buffer *kbuffer_ptr = NULL;

	if (!access_ok(up, sizeof(struct vs4l_container_list))) {
		vision_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err;
	}

	ret = copy_from_user(kp, (void __user *)up, sizeof(struct vs4l_container_list));
	if (ret) {
		vision_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err;
	}

	/* container_list -> (vs4l_container)containers[count] -> (vs4l_buffer)buffers[count] */
	if (kp->count > VISION_MAX_CONTAINERLIST) {
		vision_err("kp->count(%u) cannot be greater to VISION_MAX_CONTAINERLIST(%d)\n", kp->count, VISION_MAX_CONTAINERLIST);
		ret = -EINVAL;
		goto p_err;
	}

	size = kp->count * sizeof(struct vs4l_container);
	if (!access_ok((void __user *)kp->containers, size)) {
		vision_err("access to containers ptr failed (%pK)\n",
				kp->containers);
		ret = -EFAULT;
		goto p_err;
	}

	/* NOTE: hoon98.choi: check if devm_kzalloc() for memory management */
	kcontainer_ptr = kzalloc(size, GFP_KERNEL);
	if (!kcontainer_ptr) {
		ret = -ENOMEM;
		goto p_err;
	}

	ret = copy_from_user(kcontainer_ptr, (void __user *)kp->containers, size);
	if (ret) {
		vision_err("error from copy_from_user(%d), size(%zu)\n", ret, size);
		ret = -EFAULT;
		goto p_err_container;
	}

	kp->containers = kcontainer_ptr;

	/* fill each container from user's request */
	for (i = 0, free_buf_num = 0; i < kp->count; i++, free_buf_num++) {
		size = kp->containers[i].count * sizeof(struct vs4l_buffer);

		if (!access_ok((void __user *)
					kp->containers[i].buffers, size)) {
			vision_err("access to containers ptr failed (%pK)\n",
					kp->containers[i].buffers);
			ret = -EFAULT;
			goto p_err_buffer;
		}

		kbuffer_ptr = kmalloc(size, GFP_KERNEL);
		if (!kbuffer_ptr) {
			ret = -ENOMEM;
			goto p_err_buffer;
		}

		ret = copy_from_user(kbuffer_ptr,
				(void __user *)kp->containers[i].buffers, size);
		if (ret) {
			vision_err("error from copy_from_user(idx: %d, ret: %d, size: %zu)\n",
					i, ret, size);
			ret = -EFAULT;
			goto p_err_buffer_malloc;
		}

		kp->containers[i].buffers = kbuffer_ptr;
	}

	return ret;

p_err_buffer_malloc:
	kfree(kbuffer_ptr);
	kbuffer_ptr = NULL;

p_err_buffer:
	for (i = 0; i < free_buf_num; i++) {
		kfree(kp->containers[i].buffers);
		kp->containers[i].buffers = NULL;
	}

p_err_container:
	kfree(kcontainer_ptr);
	kp->containers = NULL;

p_err:
	vision_err("Return with fail... (%d)\n", ret);
	return ret;
}

static void put_vs4l_container64(struct vs4l_container_list *kp, struct vs4l_container_list __user *up)
{
	int i;

	if (!access_ok(up, sizeof(struct vs4l_container_list)))
		vision_err("Cannot access to user ptr(%pK)\n", up);

	if (put_user(kp->flags, &up->flags) ||
		put_user(kp->index, &up->index) ||
		put_user(kp->id, &up->id) ||
		put_user(kp->timestamp[0].tv_sec, &up->timestamp[0].tv_sec) ||
		put_user(kp->timestamp[0].tv_usec, &up->timestamp[0].tv_usec) ||
		put_user(kp->timestamp[1].tv_sec, &up->timestamp[1].tv_sec) ||
		put_user(kp->timestamp[1].tv_usec, &up->timestamp[1].tv_usec) ||
		put_user(kp->timestamp[2].tv_sec, &up->timestamp[2].tv_sec) ||
		put_user(kp->timestamp[2].tv_usec, &up->timestamp[2].tv_usec) ||
		put_user(kp->timestamp[3].tv_sec, &up->timestamp[3].tv_sec) ||
		put_user(kp->timestamp[3].tv_usec, &up->timestamp[3].tv_usec) ||
		put_user(kp->timestamp[4].tv_sec, &up->timestamp[4].tv_sec) ||
		put_user(kp->timestamp[4].tv_usec, &up->timestamp[4].tv_usec) ||
		put_user(kp->timestamp[5].tv_sec, &up->timestamp[5].tv_sec) ||
		put_user(kp->timestamp[5].tv_usec, &up->timestamp[5].tv_usec)) {
		vision_err("Copy_to_user failed (%pK -> %pK)\n", kp, up);
	}

	for (i = 0; i < kp->count; ++i)
		kfree(kp->containers[i].buffers);

	kfree(kp->containers);
	kp->containers = NULL;
}

static int get_vs4l_profiler(struct vs4l_profiler *kp, struct vs4l_profiler __user *up)
{
	int ret;
	size_t size;
	struct vs4l_profiler_node *kprofiler_node = NULL;
	struct vs4l_profiler_node **kprofiler_pchild = NULL;
	struct vs4l_profiler_node *kprofiler_child = NULL;

	if (!access_ok(up, sizeof(struct vs4l_profiler))) {
		vision_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err;
	}

	ret = copy_from_user(kp, (void __user *)up, sizeof(struct vs4l_profiler));
	if (ret) {
		vision_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err;
	}

	if (kp->node != NULL) {
		size = sizeof(struct vs4l_profiler_node);
		if (!access_ok((void __user *)kp->node, size)) {
			vision_err("access to profiler node ptr failed (%pK)\n",
					kp->node);
			ret = -EFAULT;
			goto p_err;
		}

		kprofiler_node = kzalloc(size, GFP_KERNEL);
		if (!kprofiler_node) {
			ret = -ENOMEM;
			goto p_err_profiler_node;
		}

		ret = copy_from_user(kprofiler_node, (void __user *)kp->node, size);
		if (ret) {
			vision_err("error from copy_from_user(%d), size(%zu) about profiler\n", ret, size);
			ret = -EFAULT;
			goto p_err_profiler_node;
		}
		kp->node = kprofiler_node;

		if (kp->node->child != NULL) {
			size = sizeof(struct vs4l_profiler_node*)*2;
			if (!access_ok((void __user *)kp->node->child, size)) {
				vision_err("access to profiler node ptr failed (%pK)\n",
					kp->node->child);
				ret = -EFAULT;
				goto p_err_profiler_node;
			}

			kprofiler_pchild = kzalloc(size, GFP_KERNEL);
			if (!kprofiler_pchild) {
				ret = -ENOMEM;
				goto p_err_profiler_pchild;
			}

			ret = copy_from_user(kprofiler_pchild, (void __user *)kp->node->child, size);
			if (ret) {
				vision_err("error from copy_from_user(%d), size(%zu) about profiler\n", ret, size);
				ret = -EFAULT;
				goto p_err_profiler_pchild;
			}

			kp->node->child = kprofiler_pchild;

			size = sizeof(struct vs4l_profiler_node);
			if (!access_ok((void __user *)kp->node->child[0], size)) {
				vision_err("access to profiler node ptr failed (%pK)\n",
					kp->node->child[0]);
				ret = -EFAULT;
				goto p_err_profiler_pchild;
			}
			kprofiler_child = kzalloc(size, GFP_KERNEL);
			if (!kprofiler_child) {
				ret = -ENOMEM;
				goto p_err_profiler_child;
			}
			ret = copy_from_user(kprofiler_child, (void __user *)kp->node->child[0], size);
			if (ret) {
				vision_err("error from copy_from_user(%d), size(%zu) about profiler\n", ret, size);
				ret = -EFAULT;
				goto p_err_profiler_child;
			}
			kp->node->child[0] = kprofiler_child;
		}
	}

	return ret;

p_err_profiler_child:
	kfree(kprofiler_child);
	kp->node->child[0] = NULL;

p_err_profiler_pchild:
	kfree(kprofiler_pchild);
	kp->node->child = NULL;

p_err_profiler_node:
	kfree(kprofiler_node);
	kp->node = NULL;

p_err:
	vision_err("Return with fail... (%d)\n", ret);
	return ret;
}

static int put_vs4l_profiler(struct vs4l_profiler *kp, struct vs4l_profiler __user *up)
{
	int ret;
	size_t size;
	struct vs4l_profiler temp;
	struct vs4l_profiler_node **kprofiler_pchild;
	struct vs4l_profiler_node *kprofiler_node;

	if (!access_ok(up, sizeof(struct vs4l_profiler))) {
		vision_err("access failed from user ptr(%pK)\n", up);
		ret = -EFAULT;
		goto p_err;
	}

	ret = copy_from_user(&temp, (void __user *)up, sizeof(struct vs4l_profiler));
	if (ret) {
		vision_err("copy_from_user failed(%d) from %pK\n", ret, up);
		goto p_err;
	}

	if (kp->node != NULL) {
		size = sizeof(struct vs4l_profiler_node);
		if (!access_ok((void __user *)temp.node, size)) {
			vision_err("access to profiler node ptr failed (%pK)\n",
					temp.node);
			ret = -EFAULT;
			goto p_err;
		}

		// copy to user - Firmware duration
		put_user(kp->node->duration, &temp.node->duration);

		kprofiler_node = kzalloc(size, GFP_KERNEL);
		if (!kprofiler_node) {
			ret = -ENOMEM;
			goto p_err_profiler_node;
		}

		ret = copy_from_user(kprofiler_node, (void __user *)temp.node, size);
		if (ret) {
			vision_err("error from copy_from_user(%d), size(%zu) about profiler\n", ret, size);
			ret = -EFAULT;
			goto p_err_profiler_node;
		}
		temp.node = kprofiler_node;

		if (temp.node->child != NULL) {
			size = sizeof(struct vs4l_profiler_node*)*2;
			if (!access_ok((void __user *)temp.node->child, size)) {
				vision_err("access to profiler node ptr failed (%pK)\n",
					temp.node->child);
				ret = -EFAULT;
				goto p_err_profiler_node;
			}

			kprofiler_pchild = kzalloc(size, GFP_KERNEL);
			if (!kprofiler_pchild) {
				ret = -ENOMEM;
				goto p_err_profiler_pchild;
			}

			ret = copy_from_user(kprofiler_pchild, (void __user *)temp.node->child, size);
			if (ret) {
				vision_err("error from copy_from_user(%d), size(%zu) about profiler\n", ret, size);
				ret = -EFAULT;
				goto p_err_profiler_pchild;
			}

			temp.node->child = kprofiler_pchild;

			// copy to user - HW duration
			put_user(kp->node->child[0]->duration, &temp.node->child[0]->duration);
			kfree(temp.node->child);
			temp.node->child = NULL;
			kfree(kp->node->child);
			kp->node->child = NULL;
		}

		kfree(temp.node);
		temp.node = NULL;
		kfree(kp->node);
		kp->node = NULL;
	}

	return ret;

p_err_profiler_pchild:
	kfree(kprofiler_pchild);
	temp.node->child = NULL;

p_err_profiler_node:
	kfree(kprofiler_node);
	temp.node = NULL;

p_err:
	vision_err("Return with fail... (%d)\n", ret);
	return ret;
}

long vertex_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	static s64 buf_time = 0;
	s64 now;
	int get_time = 0;
	int ret = 0;
	static int p_flag = 0;
	static int npu_fw_time = 0;
	static int npu_hw_time = 0;
	struct vision_device *vdev = vision_devdata(file);
	const struct vertex_ioctl_ops *ops = vdev->ioctl_ops;

	/* temp var to support each ioctl */
	union {
		struct vs4l_graph vsg;
		struct vs4l_format_list vsf;
		struct vs4l_param_list vsp;
		struct vs4l_ctrl vsc;
		struct vs4l_sched_param vsprm;
		struct vs4l_container_list vscl;
		struct vs4l_profiler vspr;
	} vs4l_kvar;

	now = ktime_to_ns(ktime_get_boottime());
	switch (cmd) {
	case VS4L_VERTEXIOC_S_GRAPH:
		ret = get_vs4l_graph64(&vs4l_kvar.vsg,
				(struct vs4l_graph __user *)arg);
		if (ret) {
			vision_err("get_vs4l_graph64 (%d)\n", ret);
			break;
		}

		ret = ops->vertexioc_s_graph(file, &vs4l_kvar.vsg);
		if (ret)
			vision_err("vertexioc_s_graph is fail(%d)\n", ret);

		put_vs4l_graph64(&vs4l_kvar.vsg,
				(struct vs4l_graph __user *)arg);
		break;

	case VS4L_VERTEXIOC_S_FORMAT:
		ret = get_vs4l_format64(&vs4l_kvar.vsf,
				(struct vs4l_format_list __user *)arg);
		if (ret) {
			vision_err("get_vs4l_format64 (%d)\n", ret);
			break;
		}

		ret = ops->vertexioc_s_format(file, &vs4l_kvar.vsf);
		if (ret)
			vision_err("vertexioc_s_format (%d)\n", ret);

		put_vs4l_format64(&vs4l_kvar.vsf,
				(struct vs4l_format_list __user *)arg);
		break;

	case VS4L_VERTEXIOC_S_PARAM:
		ret = get_vs4l_param64(&vs4l_kvar.vsp,
				(struct vs4l_param_list __user *)arg);
		if (ret) {
			vision_err("get_vs4l_param64 (%d)\n", ret);
			break;
		}

		ret = ops->vertexioc_s_param(file, &vs4l_kvar.vsp);
		if (ret)
			vision_err("vertexioc_s_param (%d)\n", ret);

		put_vs4l_param64(&vs4l_kvar.vsp,
				(struct vs4l_param_list __user *)arg);
		break;

	case VS4L_VERTEXIOC_S_CTRL:
		ret = get_vs4l_ctrl64(&vs4l_kvar.vsc,
				(struct vs4l_ctrl __user *)arg);
		if (ret) {
			vision_err("get_vs4l_ctrl64 (%d)\n", ret);
			break;
		}

		ret = ops->vertexioc_s_ctrl(file, &vs4l_kvar.vsc);
		if (ret)
			vision_err("vertexioc_s_ctrl is fail(%d)\n", ret);

		put_vs4l_ctrl64(&vs4l_kvar.vsc, (struct vs4l_ctrl __user *)arg);
		break;

	case VS4L_VERTEXIOC_STREAM_ON:
		ret = ops->vertexioc_streamon(file);
		if (ret)
			vision_err("vertexioc_streamon failed(%d)\n", ret);
		vdev->tpf = 0;
		break;

	case VS4L_VERTEXIOC_STREAM_OFF:
		ret = ops->vertexioc_streamoff(file);
		if (ret)
			vision_err("vertexioc_streamoff failed(%d)\n", ret);
		break;

	case VS4L_VERTEXIOC_QBUF:
		ret = get_vs4l_container64(&vs4l_kvar.vscl,
				(struct vs4l_container_list __user *)arg);
		if (ret)
			break;
		if (vs4l_kvar.vscl.timestamp[0].tv_sec &&
				vs4l_kvar.vscl.timestamp[0].tv_usec)
			buf_time = now;

		vs4l_kvar.vscl.timestamp[5].tv_sec = p_flag;
		vs4l_kvar.vscl.timestamp[5].tv_usec = npu_hw_time;
		vs4l_kvar.vscl.timestamp[4].tv_usec = npu_fw_time;

		ret = ops->vertexioc_qbuf(file, &vs4l_kvar.vscl);
		if (ret)
			vision_err("vertexioc_qbuf failed(%d)\n", ret);

		put_vs4l_container64(&vs4l_kvar.vscl,
				(struct vs4l_container_list __user *)arg);
		break;

	case VS4L_VERTEXIOC_DQBUF:
		ret = get_vs4l_container64(&vs4l_kvar.vscl,
				(struct vs4l_container_list __user *)arg);
		if (ret)
			break;
		if (vs4l_kvar.vscl.timestamp[0].tv_sec &&
				vs4l_kvar.vscl.timestamp[0].tv_usec)
			get_time = 1;

		ret = ops->vertexioc_dqbuf(file, &vs4l_kvar.vscl);
		if (ret != 0 && ret != -EWOULDBLOCK)
			vision_err("vertexioc_dqbuf failed(%d)\n", ret);

		put_vs4l_container64(&vs4l_kvar.vscl,
				(struct vs4l_container_list __user *)arg);
		if (get_time) {
			now = ktime_to_ns(ktime_get_boottime());
			vdev->tpf = now - buf_time;
			npu_hw_time = vs4l_kvar.vscl.timestamp[5].tv_usec;
			npu_fw_time = vs4l_kvar.vscl.timestamp[4].tv_usec;
			get_time = 0;
		}
		break;
	case VS4L_VERTEXIOC_PREPARE:
		ret = get_vs4l_container64(&vs4l_kvar.vscl,
				(struct vs4l_container_list __user *)arg);
		if (ret)
			break;

		ret = ops->vertexioc_prepare(file, &vs4l_kvar.vscl);
		if (ret)
			vision_err("vertexioc_prepare failed(%d)\n", ret);

		put_vs4l_container64(&vs4l_kvar.vscl,
				(struct vs4l_container_list __user *)arg);
		break;
	case VS4L_VERTEXIOC_UNPREPARE:
		ret = get_vs4l_container64(&vs4l_kvar.vscl,
				(struct vs4l_container_list __user *)arg);
		if (ret)
			break;

		ret = ops->vertexioc_unprepare(file, &vs4l_kvar.vscl);
		if (ret)
			vision_err("vertexioc_unprepare failed(%d)\n", ret);

		put_vs4l_container64(&vs4l_kvar.vscl,
				(struct vs4l_container_list __user *)arg);
		break;
	case VS4L_VERTEXIOC_SCHED_PARAM:
		ret = get_vs4l_sched_param64(&vs4l_kvar.vsprm,
				(struct vs4l_sched_param __user *)arg);
		if (ret)
			break;

		ret = ops->vertexioc_sched_param(file, &vs4l_kvar.vsprm);
		if (ret)
			vision_err("vertexioc_sched_param failed(%d)\n", ret);

		put_vs4l_sched_param64(&vs4l_kvar.vsprm,
				(struct vs4l_sched_param __user *)arg);
		break;
	case VS4L_VERTEXIOC_PROFILE_ON:
		ret = get_vs4l_profiler(&vs4l_kvar.vspr,
				(struct vs4l_profiler __user *)arg);
		p_flag = vs4l_kvar.vspr.level;
		npu_fw_time = 0;
		npu_hw_time = 0;

		if (ret) {
			vision_err("get_vs4l_profiler failed(%d)\n", ret);
			break;
		}
		ret = ops->vertexioc_profileon(file, &vs4l_kvar.vspr);
		if (ret) {
			vision_err("vertexioc_profileon failed(%d)\n", ret);
			break;
		}

		ret = put_vs4l_profiler(&vs4l_kvar.vspr,
				(struct vs4l_profiler __user *)arg);
		if (ret)
			vision_err("put_vs4l_profiler failed(%d)\n", ret);
		break;
	case VS4L_VERTEXIOC_PROFILE_OFF:
		ret = get_vs4l_profiler(&vs4l_kvar.vspr,
				(struct vs4l_profiler __user *)arg);
		if (ret) {
			vision_err("get_vs4l_profiler failed(%d)\n", ret);
			break;
		}
		ret = ops->vertexioc_profileoff(file, &vs4l_kvar.vspr);
		if (ret) {
			vision_err("vertexioc_profileoff failed(%d)\n", ret);
			break;
		}

		if (vs4l_kvar.vspr.node) {
			(*vs4l_kvar.vspr.node).duration = npu_fw_time;
			if ((*vs4l_kvar.vspr.node).child != NULL)
				(*vs4l_kvar.vspr.node).child[0]->duration = npu_hw_time;
		}

		ret = put_vs4l_profiler(&vs4l_kvar.vspr,
				(struct vs4l_profiler __user *)arg);
		p_flag = 0;
		if (ret) {
			vision_err("put_vs4l_profiler failed(%d)\n", ret);
		}
		break;
	default:
		vision_err("ioctl(%u) is not supported(usr arg: %lx)\n",
				cmd, arg);
		break;
	}

	return ret;
}
