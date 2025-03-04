/* sound/soc/samsung/abox/abox_ion.c
 *
 * ALSA SoC - Samsung Abox ION buffer module
 *
 * Copyright (c) 2018 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <sound/samsung/abox.h>
#include <sound/sounddev_abox.h>

#include <linux/ion.h>
#include <linux/dma-buf.h>

#include "abox.h"
#include "abox_ion.h"
#include "abox_memlog.h"

int abox_ion_get_mmap_fd(struct device *dev,
		struct abox_ion_buf *buf,
		struct snd_pcm_mmap_fd *mmap_fd)
{
	struct dma_buf *temp_buf;

	abox_dbg(dev, "%s\n", __func__);

	if (buf->fd < 0)
		buf->fd = dma_buf_fd(buf->dma_buf, O_CLOEXEC);

	if (buf->fd < 0) {
		abox_err(dev, "%s dma_buf_fd is failed\n", __func__);
		return -EFAULT;
	}

	abox_info(dev, "%s fd(%d)\n", __func__, buf->fd);

	mmap_fd->dir = (buf->direction != DMA_FROM_DEVICE) ?
			SNDRV_PCM_STREAM_PLAYBACK : SNDRV_PCM_STREAM_CAPTURE;
	mmap_fd->size = buf->size;
	mmap_fd->actual_size = buf->size;
	mmap_fd->fd = buf->fd;

	temp_buf = dma_buf_get(buf->fd);
	if (IS_ERR(temp_buf))
		abox_err(dev, "dma_buf_get(%d) failed: %ld\n", buf->fd,
				PTR_ERR(temp_buf));

	return 0;
}

static int abox_ion_hwdep_ioctl_common(struct snd_hwdep *hw, struct file *filp,
		unsigned int cmd, void __user *arg)
{
	struct abox_ion_buf *buf = hw->private_data;
	struct device *dev = buf->dev;
	struct snd_pcm_mmap_fd mmap_fd;
	int ret;

	abox_dbg(dev, "%s(%#x)\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_IOCTL_MMAP_DATA_FD:
		ret = abox_ion_get_mmap_fd(dev, buf, &mmap_fd);
		if (ret < 0) {
			abox_err(dev, "MMAP_DATA_FD failed: %d\n", ret);
			break;
		}

		if (copy_to_user(arg, &mmap_fd, sizeof(mmap_fd)))
			ret = -EFAULT;
		break;
	default:
		abox_err(dev, "unknown ioctl = %#x\n", cmd);
		ret = -ENOTTY;
		break;
	}

	return ret;
}

static int abox_ion_hwdep_ioctl(struct snd_hwdep *hw, struct file *file,
		unsigned int cmd, unsigned long arg)
{
	return abox_ion_hwdep_ioctl_common(hw, file, cmd, (void __user *)arg);
}

static int abox_ion_hwdep_ioctl_compat(struct snd_hwdep *hw, struct file *file,
		unsigned int cmd, unsigned long arg)
{
	return abox_ion_hwdep_ioctl_common(hw, file, cmd, compat_ptr(arg));
}

int abox_ion_new_hwdep(struct snd_soc_pcm_runtime *runtime,
		struct abox_ion_buf *buf, struct snd_hwdep **hwdep)
{
	struct device *dev = runtime->cpu_dai->dev;
	char *id;
	int device = runtime->pcm->device;
	int ret;

	abox_dbg(dev, "%s\n", __func__);

	if (!buf)
		return -EINVAL;

	id = kasprintf(GFP_KERNEL, "ABOX_MMAP_FD_%d", device);
	if (!id)
		return -ENOMEM;

	ret = snd_hwdep_new(runtime->card->snd_card, id, device, hwdep);
	if (ret < 0) {
		abox_err(dev, "failed to create hwdep %s: %d\n", id, ret);
		goto out;
	}

	buf->dev = dev;
	(*hwdep)->iface = SNDRV_CTL_ELEM_IFACE_HWDEP;
	(*hwdep)->private_data = buf;
	(*hwdep)->ops.ioctl = abox_ion_hwdep_ioctl;
	(*hwdep)->ops.ioctl_compat = abox_ion_hwdep_ioctl_compat;
out:
	kfree(id);
	return ret;
}

struct abox_ion_buf *abox_ion_alloc(struct device *dev,
		struct abox_data *data,
		unsigned long iova,
		size_t size,
		bool playback)
{
	struct device *dev_abox = data->dev;
	struct abox_ion_buf *buf;
	int ret;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto error;
	}

	buf->direction = playback ? DMA_TO_DEVICE : DMA_FROM_DEVICE;
	buf->size = PAGE_ALIGN(size);
	buf->iova = iova;
	buf->fd = -EINVAL;

	buf->dma_buf = ion_alloc(buf->size, ION_HEAP_SYSTEM, 0);
	if (IS_ERR(buf->dma_buf)) {
		ret = PTR_ERR(buf->dma_buf);
		abox_err(dev, "failed to ion_alloc(): %d\n", ret);
		goto error_alloc;
	}

	buf->attachment = dma_buf_attach(buf->dma_buf, dev_abox);
	if (IS_ERR(buf->attachment)) {
		ret = PTR_ERR(buf->attachment);
		abox_err(dev, "failed to dma_buf_attach(): %d\n", ret);
		goto error_attach;
	}

	buf->sgt = dma_buf_map_attachment(buf->attachment, buf->direction);
	if (IS_ERR(buf->sgt)) {
		ret = PTR_ERR(buf->sgt);
		abox_err(dev, "failed to dma_buf_map_attachment(): %d\n", ret);
		goto error_map_dmabuf;
	}

	buf->kva = dma_buf_vmap(buf->dma_buf);
	if (!buf->kva)
		dev_warn_once(dev, "failed to dma_buf_vmap()\n");

	ret = abox_iommu_map_sg(dev_abox,
			buf->iova,
			buf->sgt->sgl,
			buf->sgt->orig_nents,
			buf->direction,
			buf->size,
			buf->kva);
	if (ret < 0) {
		abox_err(dev, "Failed to iommu_map(%pad): %d\n",
				&buf->iova, ret);
		goto error_iommu_map_sg;
	}

	return buf;

error_iommu_map_sg:
	dma_buf_vunmap(buf->dma_buf, buf->kva);
	dma_buf_unmap_attachment(buf->attachment, buf->sgt, buf->direction);
error_map_dmabuf:
	dma_buf_detach(buf->dma_buf, buf->attachment);
error_attach:
	dma_buf_put(buf->dma_buf);
error_alloc:
	kfree(buf);
error:
	abox_err(dev, "%s: Error occured while allocating\n", __func__);
	return ERR_PTR(ret);
}

int abox_ion_free(struct device *dev,
		struct abox_data *data,
		struct abox_ion_buf *buf)
{
	int ret;

	ret = abox_iommu_unmap(data->dev, buf->iova);
	if (ret < 0)
		abox_err(dev, "Failed to iommu_unmap: %d\n", ret);

	dma_buf_vunmap(buf->dma_buf, buf->kva);

	if (buf->fd >= 0)
		dma_buf_put(buf->dma_buf);

	dma_buf_unmap_attachment(buf->attachment, buf->sgt, buf->direction);
	dma_buf_detach(buf->dma_buf, buf->attachment);
	dma_buf_put(buf->dma_buf);

	kfree(buf);

	return ret;
}
