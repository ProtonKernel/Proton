// SPDX-License-Identifier: GPL-2.0
/*
 * Dma buf container support
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 */

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>
#include <linux/miscdevice.h>
#include <linux/dma-buf-container.h>
#include <linux/module.h>

#include "dma-buf-container.h"

#define MAX_BUFCON_BUFS 64
#define MAX_BUFCON_SRC_BUFS (MAX_BUFCON_BUFS - 1)

static int dmabuf_container_get_user_data(unsigned int cmd, void __user *arg,
					  int fds[])
{
	int __user *ufds;
	int count, ret;

#ifdef CONFIG_COMPAT
	if (cmd == DMABUF_CONTAINER_COMPAT_IOCTL_MERGE) {
		struct compat_dma_buf_merge __user *udata = arg;
		compat_uptr_t compat_ufds;

		ret = get_user(compat_ufds, &udata->dma_bufs);
		ret |= get_user(count, &udata->count);
		ufds = compat_ptr(compat_ufds);
	} else
#endif
	{
		struct dma_buf_merge __user *udata = arg;

		ret = get_user(ufds, &udata->dma_bufs);
		ret |= get_user(count, &udata->count);
	}

	if (ret) {
		pr_err("%s: failed to read data from user\n", __func__);
		return -EFAULT;
	}

	if ((count < 1) || (count > MAX_BUFCON_SRC_BUFS)) {
		pr_err("%s: invalid buffer count %u\n", __func__, count);
		return -EINVAL;
	}

	if (copy_from_user(fds, ufds, sizeof(fds[0]) * count)) {
		pr_err("%s: failed to read %u dma_bufs from user\n",
		       __func__, count);
		return -EFAULT;
	}

	return count;
}

static int dmabuf_container_put_user_data(unsigned int cmd, void __user *arg,
					  struct dma_buf *merged)
{
	int fd = get_unused_fd_flags(O_CLOEXEC);
	int ret;

	if (fd < 0) {
		pr_err("%s: failed to get new fd\n", __func__);
		return fd;
	}

#ifdef CONFIG_COMPAT
	if (cmd == DMABUF_CONTAINER_COMPAT_IOCTL_MERGE) {
		struct compat_dma_buf_merge __user *udata = arg;

		ret = put_user(fd, &udata->dmabuf_container);
	} else
#endif
	{
		struct dma_buf_merge __user *udata = arg;

		ret = put_user(fd, &udata->dmabuf_container);
	}

	if (ret) {
		pr_err("%s: failed to store dmabuf_container fd to user\n",
		       __func__);

		put_unused_fd(fd);
		return ret;
	}

	fd_install(fd, merged->file);

	return 0;
}

/*
 * struct dma_buf_container - container description
 * @table:	dummy sg_table for container
 * @count:	the number of the buffers
 * @dmabuf_mask: bit-mask of dma-bufs in @dmabufs.
 *               @dmabuf_mask is 0(unmasked) on creation of a dma-buf container.
 * @dmabufs:	dmabuf array representing each buffers
 */
struct dma_buf_container {
	struct sg_table	table;
	int		count;
	u64		dmabuf_mask;
	struct dma_buf	*dmabufs[0];
};

static void dmabuf_container_put_dmabuf(struct dma_buf_container *container)
{
	int i;

	for (i = 0; i < container->count; i++)
		dma_buf_put(container->dmabufs[i]);
}

static void dmabuf_container_dma_buf_release(struct dma_buf *dmabuf)
{
	dmabuf_container_put_dmabuf(dmabuf->priv);

	kfree(dmabuf->priv);
}

static struct sg_table *dmabuf_container_map_dma_buf(
				    struct dma_buf_attachment *attachment,
				    enum dma_data_direction direction)
{
	struct dma_buf_container *container = attachment->dmabuf->priv;

	return &container->table;
}

static void dmabuf_container_unmap_dma_buf(struct dma_buf_attachment *attach,
					   struct sg_table *table,
					   enum dma_data_direction direction)
{
}

static void *dmabuf_container_dma_buf_kmap(struct dma_buf *dmabuf,
					   unsigned long offset)
{
	return NULL;
}

static int dmabuf_container_mmap(struct dma_buf *dmabuf,
				 struct vm_area_struct *vma)
{
	pr_err("%s: dmabuf container does not support mmap\n", __func__);

	return -EACCES;
}

static struct dma_buf_ops dmabuf_container_dma_buf_ops = {
	.map_dma_buf = dmabuf_container_map_dma_buf,
	.unmap_dma_buf = dmabuf_container_unmap_dma_buf,
	.release = dmabuf_container_dma_buf_release,
	.map = dmabuf_container_dma_buf_kmap,
	.mmap = dmabuf_container_mmap,
};

static bool is_dmabuf_container(struct dma_buf *dmabuf)
{
	return dmabuf->ops == &dmabuf_container_dma_buf_ops;
}

static struct dma_buf_container *get_container(struct dma_buf *dmabuf)
{
	return dmabuf->priv;
}

static int get_dma_buf_count(struct dma_buf *dmabuf)
{
	return is_dmabuf_container(dmabuf) ? get_container(dmabuf)->count : 1;
}

static struct dma_buf *__dmabuf_container_get_buffer(struct dma_buf *dmabuf,
						     int index)
{
	struct dma_buf *out = is_dmabuf_container(dmabuf)
			      ? get_container(dmabuf)->dmabufs[index] : dmabuf;

	get_dma_buf(out);

	return out;
}

static struct dma_buf *dmabuf_container_export(struct dma_buf_container *bufcon)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	unsigned long size = 0;
	int i;

	for (i = 0; i < bufcon->count; i++)
		size += bufcon->dmabufs[i]->size;

	exp_info.ops = &dmabuf_container_dma_buf_ops;
	exp_info.size = size;
	exp_info.flags = O_RDWR;
	exp_info.priv = bufcon;

	return dma_buf_export(&exp_info);
}

static struct dma_buf *create_dmabuf_container(struct dma_buf *bufs[],
					       int count)
{
	struct dma_buf_container *container;
	struct dma_buf *merged;
	int total = 0;
	int i, j, nelem = 0;

	for (i = 0; i < count; i++)
		total += get_dma_buf_count(bufs[i]);

	if (total > MAX_BUFCON_BUFS) {
		pr_err("%s: too many (%u) dmabuf merge request\n",
		       __func__, total);
		return ERR_PTR(-EINVAL);
	}

	container = kzalloc(sizeof(*container) +
			    sizeof(container->dmabufs[0]) * total, GFP_KERNEL);
	if (!container)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < count; i++)
		for (j = 0; j < get_dma_buf_count(bufs[i]); j++)
			container->dmabufs[nelem++] =
				__dmabuf_container_get_buffer(bufs[i], j);

	container->count = nelem;

	merged = dmabuf_container_export(container);
	if (IS_ERR(merged)) {
		pr_err("%s: failed to export dmabuf container.\n", __func__);
		dmabuf_container_put_dmabuf(container);
		kfree(container);
	}

	return merged;
}

static struct dma_buf *dmabuf_container_create(int fds[], int count)
{
	struct dma_buf *bufs[MAX_BUFCON_SRC_BUFS];
	struct dma_buf *merged;
	int i;

	for (i = 0; i < count; i++) {
		bufs[i] = dma_buf_get(fds[i]);
		if (IS_ERR(bufs[i])) {
			merged = bufs[i];
			pr_err("%s: failed to get dmabuf of fd %d @ %u/%u\n",
			       __func__, fds[i], i, count);

			goto err_get;
		}
	}

	merged = create_dmabuf_container(bufs, count);
	/*
	 * reference count of dma_bufs (file->f_count) in bufs[] are increased
	 * again in create_dmabuf_container(). So they should be decremented
	 * before return.
	 */
err_get:
	while (i-- > 0)
		dma_buf_put(bufs[i]);

	return merged;
}

long dma_buf_merge_ioctl(unsigned int cmd, unsigned long arg)
{
	int fds[MAX_BUFCON_SRC_BUFS];
	int count;
	struct dma_buf *merged;
	long ret;

	count = dmabuf_container_get_user_data(cmd, (void __user *)arg, fds);
	if (count < 0)
		return count;

	merged = dmabuf_container_create(fds, count);
	if (IS_ERR(merged))
		return PTR_ERR(merged);

	ret = dmabuf_container_put_user_data(cmd, (void __user *)arg, merged);
	if (ret)
		dma_buf_put(merged);

	return ret;
}

int dmabuf_container_set_mask_user(unsigned long arg)
{
	struct dma_buf_mask __user *udata = (void __user *)arg;
	struct dma_buf *dmabuf;
	int container, ret;
	u32 mask_array[2];
	u64 mask;

	ret = get_user(container, &udata->dmabuf_container);
	ret |= copy_from_user(mask_array, udata->mask, sizeof(mask_array));
	if (ret) {
		pr_err("%s: failed to data from user\n", __func__);
		return -EFAULT;
	}

	dmabuf = dma_buf_get(container);
	if (IS_ERR(dmabuf)) {
		pr_err("%s: failed to get dmabuf %d\n", __func__, container);
		return PTR_ERR(dmabuf);
	}

	mask = ((u64)mask_array[1] << 32) | mask_array[0];
	ret = dmabuf_container_set_mask(dmabuf, mask);
	dma_buf_put(dmabuf);

	return ret;
}

int dmabuf_container_get_mask_user(unsigned long arg)
{
	struct dma_buf_mask __user *udata = (void __user *)arg;
	struct dma_buf *dmabuf;
	int container, ret;
	u32 mask_array[2];
	u64 mask;

	ret = get_user(container, &udata->dmabuf_container);
	if (ret) {
		pr_err("%s: failed to read data from user\n", __func__);
		return -EFAULT;
	}

	dmabuf = dma_buf_get(container);
	if (IS_ERR(dmabuf)) {
		pr_err("%s: failed to get dmabuf %d\n", __func__, container);
		return PTR_ERR(dmabuf);
	}

	ret = dmabuf_container_get_mask(dmabuf, &mask);
	if (ret)
		goto err;

	mask_array[0] = (mask << 32) >> 32;
	mask_array[1] = mask >> 32;

	ret = copy_to_user(udata->mask, mask_array, sizeof(mask_array));
	if (ret)
		pr_err("%s: failed to write mask to user\n", __func__);
err:
	dma_buf_put(dmabuf);
	return ret;
}

int dmabuf_container_set_mask(struct dma_buf *dmabuf, u64 mask)
{
	struct dma_buf_container *container;

	if (!is_dmabuf_container(dmabuf)) {
		pr_err("%s: given dmabuf is not dma-buf container\n", __func__);
		return -EINVAL;
	}

	container = get_container(dmabuf);

	if (mask & ~((1 << container->count) - 1)) {
		pr_err("%s: invalid mask %#x for %u buffers\n",
		       __func__, mask, container->count);
		return -EINVAL;
	}

	get_container(dmabuf)->dmabuf_mask = mask;

	return 0;
}
EXPORT_SYMBOL_GPL(dmabuf_container_set_mask);

int dmabuf_container_get_mask(struct dma_buf *dmabuf, u64 *mask)
{
	if (!is_dmabuf_container(dmabuf)) {
		pr_err("%s: given dmabuf is not dma-buf container\n", __func__);
		return -EINVAL;
	}

	*mask = get_container(dmabuf)->dmabuf_mask;

	return 0;
}
EXPORT_SYMBOL_GPL(dmabuf_container_get_mask);

int dmabuf_container_get_count(struct dma_buf *dmabuf)
{
	if (!is_dmabuf_container(dmabuf))
		return -EINVAL;

	return get_container(dmabuf)->count;
}
EXPORT_SYMBOL_GPL(dmabuf_container_get_count);

struct dma_buf *dmabuf_container_get_buffer(struct dma_buf *dmabuf, int index)
{
	struct dma_buf_container *container = get_container(dmabuf);

	if (!is_dmabuf_container(dmabuf))
		return NULL;

	if (WARN_ON(index >= container->count))
		return NULL;

	get_dma_buf(container->dmabufs[index]);

	return container->dmabufs[index];
}
EXPORT_SYMBOL_GPL(dmabuf_container_get_buffer);

struct dma_buf *dma_buf_get_any(int fd)
{
	struct dma_buf *dmabuf = dma_buf_get(fd);
	struct dma_buf *anybuf = __dmabuf_container_get_buffer(dmabuf, 0);

	dma_buf_put(dmabuf);

	return anybuf;
}
EXPORT_SYMBOL_GPL(dma_buf_get_any);

static long dmabuf_container_ioctl(struct file *filp, unsigned int cmd,
				   unsigned long arg)
{
	switch (cmd) {
#ifdef CONFIG_COMPAT
	case DMABUF_CONTAINER_COMPAT_IOCTL_MERGE:
#endif
	case DMABUF_CONTAINER_IOCTL_MERGE:
		return dma_buf_merge_ioctl(cmd, arg);
	case DMABUF_CONTAINER_IOCTL_SET_MASK:
		return dmabuf_container_set_mask_user(arg);
	case DMABUF_CONTAINER_IOCTL_GET_MASK:
		return dmabuf_container_get_mask_user(arg);
	default:
		return -ENOTTY;
	}
}

static const struct file_operations dmabuf_container_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = dmabuf_container_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= dmabuf_container_ioctl,
#endif
};

static struct miscdevice dmabuf_container_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "dmabuf_container",
	.fops = &dmabuf_container_fops,
};

static int __init dmabuf_container_init(void)
{
	int ret = misc_register(&dmabuf_container_dev);

	if (ret) {
		pr_err("%s: failed to register %s\n", __func__,
		       dmabuf_container_dev.name);
		return ret;
	}

	return 0;
}
module_init(dmabuf_container_init);
MODULE_LICENSE("GPL v2");
