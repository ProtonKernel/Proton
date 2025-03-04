/* sound/soc/samsung/abox/abox_vdma.c
 *
 * ALSA SoC Audio Layer - Samsung Abox Virtual DMA driver
 *
 * Copyright (c) 2017 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/memblock.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "abox_util.h"
#include "abox.h"
#include "abox_ion.h"
#include "abox_shm.h"
#include "abox_vdma.h"
#include "abox_memlog.h"

#undef TEST
#define VDMA_COUNT_MAX SZ_32
#define NAME_LENGTH SZ_32
#define DEVICE_NAME "abox-vdma"

struct abox_vdma_rtd {
	struct snd_dma_buffer buffer;
	struct snd_pcm_hardware hardware;
	struct snd_pcm_substream *substream;
	struct abox_ion_buf *ion_buf;
	struct snd_hwdep *hwdep;
	unsigned long iova;
	size_t pointer;
	bool iommu_mapped;
	bool dma_alloc;
};

struct abox_vdma_info {
	struct device *dev;
	int id;
	bool legacy;
	char name[NAME_LENGTH];
	struct abox_vdma_rtd rtd[SNDRV_PCM_STREAM_LAST + 1];
};

static struct device *abox_vdma_dev_abox;
static struct abox_vdma_info abox_vdma_list[VDMA_COUNT_MAX];

static int abox_vdma_get_idx(int id)
{
	return id - PCMTASK_VDMA_ID_BASE;
}

static unsigned long abox_vdma_get_iova(int id, int stream)
{
	int idx = abox_vdma_get_idx(id);
	long ret;

	switch (stream) {
	case SNDRV_PCM_STREAM_PLAYBACK:
	case SNDRV_PCM_STREAM_CAPTURE:
		ret = IOVA_VDMA_BUFFER(idx) + (SZ_512K * stream);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct abox_vdma_info *abox_vdma_get_info(int id)
{
	int idx = abox_vdma_get_idx(id);

	if (idx < 0 || idx >= ARRAY_SIZE(abox_vdma_list))
		return NULL;

	return &abox_vdma_list[idx];
}

static struct abox_vdma_rtd *abox_vdma_get_rtd(struct abox_vdma_info *info,
		int stream)
{
	if (!info || stream < 0 || stream >= ARRAY_SIZE(info->rtd))
		return NULL;

	return &info->rtd[stream];
}

static int abox_vdma_request_ipc(ABOX_IPC_MSG *msg, int atomic, int sync)
{
	return abox_request_ipc(abox_vdma_dev_abox, msg->ipcid, msg,
			sizeof(*msg), atomic, sync);
}

int abox_vdma_period_elapsed(struct abox_vdma_info *info,
		struct abox_vdma_rtd *rtd, size_t pointer)
{
	abox_dbg(info->dev, "%s[%c](%zx)\n", __func__,
			substream_to_char(rtd->substream), pointer);

	rtd->pointer = pointer;
	snd_pcm_period_elapsed(rtd->substream);

	return 0;
}

static int abox_vdma_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *soc_rtd = substream->private_data;
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_vdma_rtd *rtd = abox_vdma_get_rtd(info, substream->stream);
	struct device *dev = info->dev;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	abox_info(dev, "%s[%c]\n", __func__, substream_to_char(substream));

	abox_wait_restored(dev_get_drvdata(abox_vdma_dev_abox));

	rtd->substream = substream;
	snd_soc_set_runtime_hwparams(substream, &rtd->hardware);

	msg.ipcid = abox_stream_to_ipcid(substream->stream);
	msg.task_id = pcmtask_msg->channel_id = id;
	pcmtask_msg->msgtype = PCM_PLTDAI_OPEN;
	return abox_vdma_request_ipc(&msg, 0, 0);
}

static int abox_vdma_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *soc_rtd = substream->private_data;
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_vdma_rtd *rtd = abox_vdma_get_rtd(info, substream->stream);
	struct device *dev = info->dev;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	abox_info(dev, "%s[%c]\n", __func__, substream_to_char(substream));

	rtd->substream = NULL;

	msg.ipcid = abox_stream_to_ipcid(substream->stream);
	msg.task_id = pcmtask_msg->channel_id = id;
	pcmtask_msg->msgtype = PCM_PLTDAI_CLOSE;
	return abox_vdma_request_ipc(&msg, 0, 1);
}

static int abox_vdma_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *soc_rtd = substream->private_data;
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_vdma_rtd *rtd = abox_vdma_get_rtd(info, substream->stream);
	struct device *dev = info->dev;
	struct snd_dma_buffer *dmab = &substream->dma_buffer;
	size_t buffer_bytes = params_buffer_bytes(params);
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;
	int ret;

	abox_dbg(dev, "%s[%c]\n", __func__, substream_to_char(substream));

	if (!dmab->area && dmab->bytes >= buffer_bytes) {
		snd_pcm_set_runtime_buffer(substream, dmab);
		substream->runtime->dma_bytes = buffer_bytes;
	} else {
		ret = snd_pcm_lib_malloc_pages(substream, buffer_bytes);
		if (ret < 0)
			return ret;
	}

	msg.ipcid = abox_stream_to_ipcid(substream->stream);
	msg.task_id = pcmtask_msg->channel_id = id;

	pcmtask_msg->msgtype = PCM_SET_BUFFER;
	pcmtask_msg->param.setbuff.addr = rtd->iova;
	pcmtask_msg->param.setbuff.size = params_period_bytes(params);
	pcmtask_msg->param.setbuff.count = params_periods(params);
	ret = abox_vdma_request_ipc(&msg, 0, 0);
	if (ret < 0)
		return ret;

	pcmtask_msg->msgtype = PCM_PLTDAI_HW_PARAMS;
	pcmtask_msg->param.hw_params.sample_rate = params_rate(params);
	pcmtask_msg->param.hw_params.bit_depth = params_width(params);
	pcmtask_msg->param.hw_params.channels = params_channels(params);
	ret = abox_vdma_request_ipc(&msg, 0, 0);
	if (ret < 0)
		return ret;

	abox_info(dev, "%s:Total=%u PrdSz=%u(%u) #Prds=%u rate=%u, width=%d, channels=%u\n",
			snd_pcm_stream_str(substream),
			params_buffer_bytes(params), params_period_size(params),
			params_period_bytes(params), params_periods(params),
			params_rate(params), params_width(params),
			params_channels(params));

	return ret;
}

static int abox_vdma_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *soc_rtd = substream->private_data;
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct device *dev = info->dev;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	abox_dbg(dev, "%s[%c]\n", __func__, substream_to_char(substream));

	msg.ipcid = abox_stream_to_ipcid(substream->stream);
	msg.task_id = pcmtask_msg->channel_id = id;
	pcmtask_msg->msgtype = PCM_PLTDAI_HW_FREE;
	abox_vdma_request_ipc(&msg, 0, 0);

	return snd_pcm_lib_free_pages(substream);
}

static int abox_vdma_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *soc_rtd = substream->private_data;
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_vdma_rtd *rtd = abox_vdma_get_rtd(info, substream->stream);
	struct device *dev = info->dev;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	abox_dbg(dev, "%s[%c]\n", __func__, substream_to_char(substream));

	rtd->pointer = 0;
	abox_shm_write_vdma_appl_ptr(abox_vdma_get_idx(id), 0);

	msg.ipcid = abox_stream_to_ipcid(substream->stream);
	msg.task_id = pcmtask_msg->channel_id = id;
	pcmtask_msg->msgtype = PCM_PLTDAI_PREPARE;
	return abox_vdma_request_ipc(&msg, 0, 0);
}

static int abox_vdma_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *soc_rtd = substream->private_data;
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct device *dev = info->dev;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;
	int ret;

	abox_info(dev, "%s[%c](%d)\n", __func__,
			substream_to_char(substream), cmd);

	msg.ipcid = abox_stream_to_ipcid(substream->stream);
	msg.task_id = pcmtask_msg->channel_id = id;
	pcmtask_msg->msgtype = PCM_PLTDAI_TRIGGER;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		pcmtask_msg->param.trigger = 1;
		ret = abox_vdma_request_ipc(&msg, 1, 0);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		pcmtask_msg->param.trigger = 0;
		ret = abox_vdma_request_ipc(&msg, 1, 0);
		break;
	default:
		abox_err(dev, "invalid command: %d\n", cmd);
		ret = -EINVAL;
	}

	return ret;
}

static snd_pcm_uframes_t abox_vdma_pointer(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *soc_rtd = substream->private_data;
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_vdma_rtd *rtd = abox_vdma_get_rtd(info, substream->stream);
	struct device *dev = info->dev;
	size_t pointer;

	abox_dbg(dev, "%s[%c]\n", __func__, substream_to_char(substream));

	pointer = abox_shm_read_vdma_hw_ptr(abox_vdma_get_idx(id));

	if (!pointer)
		pointer = rtd->pointer;

	if (pointer >= rtd->iova)
		pointer -= rtd->iova;

	return bytes_to_frames(substream->runtime, pointer);
}

static int abox_vdma_mmap(struct snd_pcm_substream *substream,
		struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_rtd = substream->private_data;
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_vdma_rtd *rtd = abox_vdma_get_rtd(info, substream->stream);
	struct device *dev = info->dev;

	abox_info(dev, "%s\n", __func__);

	if (!IS_ERR_OR_NULL(rtd->ion_buf))
		return dma_buf_mmap(rtd->ion_buf->dma_buf, vma, 0);
	else
		return dma_mmap_wc(dev, vma, runtime->dma_area,
				runtime->dma_addr, runtime->dma_bytes);
}

static int abox_vdma_copy_user_dma_area(struct snd_pcm_substream *substream,
		int channel, unsigned long pos, void __user *buf,
		unsigned long bytes)
{
	struct snd_pcm_runtime *pcm_rtd = substream->runtime;
	struct snd_soc_pcm_runtime *soc_rtd = substream->private_data;
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	void *ptr;

	abox_dbg(info->dev, "%s[%c](%ld, %ld)\n", __func__,
			substream_to_char(substream), pos, bytes);

	ptr = pcm_rtd->dma_area + pos + channel *
			(pcm_rtd->dma_bytes / pcm_rtd->channels);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (copy_from_user(ptr, buf, bytes))
			return -EFAULT;
	} else {
		if (copy_to_user(buf, ptr, bytes))
			return -EFAULT;
	}

	return 0;
}

static int abox_vdma_copy_user(struct snd_pcm_substream *substream, int channel,
		unsigned long pos, void __user *buf, unsigned long bytes)
{
	struct snd_pcm_runtime *pcm_rtd = substream->runtime;
	struct snd_soc_pcm_runtime *soc_rtd = substream->private_data;
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct device *dev = info->dev;
	unsigned long appl_bytes = (pos + bytes) % pcm_rtd->dma_bytes;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	abox_vdma_copy_user_dma_area(substream, channel, pos, buf, bytes);

	/* Firmware doesn't need ack of capture stream. */
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		return 0;

	abox_dbg(dev, "%s[%c]: %ld\n", __func__,
			substream_to_char(substream), appl_bytes);

	abox_shm_write_vdma_appl_ptr(abox_vdma_get_idx(id), (u32)appl_bytes);

	msg.ipcid = abox_stream_to_ipcid(substream->stream);
	msg.task_id = pcmtask_msg->channel_id = id;
	pcmtask_msg->msgtype = PCM_PLTDAI_ACK;
	pcmtask_msg->param.pointer = (unsigned int)appl_bytes;

	return abox_vdma_request_ipc(&msg, 1, 0);
}

static struct snd_pcm_ops abox_vdma_ops = {
	.open		= abox_vdma_open,
	.close		= abox_vdma_close,
	.hw_params	= abox_vdma_hw_params,
	.hw_free	= abox_vdma_hw_free,
	.prepare	= abox_vdma_prepare,
	.trigger	= abox_vdma_trigger,
	.pointer	= abox_vdma_pointer,
	.copy_user	= abox_vdma_copy_user,
	.mmap		= abox_vdma_mmap,
};

static int abox_vdma_probe(struct snd_soc_component *component)
{
	struct device *dev = component->dev;
	int id = to_platform_device(dev)->id;

	abox_dbg(dev, "%s\n", __func__);

	snd_soc_component_set_drvdata(component, abox_vdma_get_info(id));
	return 0;
}

static int abox_vdma_alloc_ion_buf(struct snd_soc_pcm_runtime *soc_rtd,
		struct abox_vdma_info *info, int stream)
{
	struct device *dev = info->dev;
	struct abox_vdma_rtd *rtd = &info->rtd[stream];
	struct abox_data *abox_data = dev_get_drvdata(abox_vdma_dev_abox);
	bool playback = (stream == SNDRV_PCM_STREAM_PLAYBACK);
	int ret;

	rtd->ion_buf = abox_ion_alloc(dev, abox_data, rtd->iova,
			rtd->buffer.bytes, playback);
	if (IS_ERR(rtd->ion_buf)) {
		ret = PTR_ERR(rtd->ion_buf);
		rtd->ion_buf = NULL;
		return ret;
	}

	ret = abox_ion_new_hwdep(soc_rtd, rtd->ion_buf, &rtd->hwdep);
	if (ret < 0) {
		abox_err(dev, "adding hwdep failed: %d\n", ret);
		abox_ion_free(dev, abox_data, rtd->ion_buf);
		return ret;
	}

	/* update buffer using ion allocated buffer */
	rtd->buffer.area = rtd->ion_buf->kva;
	rtd->buffer.addr = rtd->ion_buf->iova;

	return 0;
}

static int abox_vdma_alloc_dma_buf(struct snd_soc_pcm_runtime *soc_rtd,
		struct abox_vdma_info *info, int stream)
{
	struct device *dev = info->dev;
	struct abox_vdma_rtd *rtd = &info->rtd[stream];

	rtd->dma_alloc = true;
	return snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, dev, rtd->buffer.bytes,
			&rtd->buffer);
}

static int abox_vdma_pcm_new(struct snd_soc_pcm_runtime *soc_rtd)
{
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct device *dev = info->dev;
	struct device *dev_abox = abox_vdma_dev_abox;
	struct snd_pcm *pcm = soc_rtd->pcm;
	int i, ret;

	abox_dbg(dev, "%s\n", __func__);

	for (i = 0; i <= SNDRV_PCM_STREAM_LAST; i++) {
		struct snd_pcm_substream *substream = pcm->streams[i].substream;
		struct abox_vdma_rtd *rtd = &info->rtd[i];

		if (!substream)
			continue;

		if (rtd->iova == 0)
			rtd->iova = abox_vdma_get_iova(id, i);

		if (rtd->buffer.bytes == 0)
			rtd->buffer.bytes = BUFFER_BYTES_MIN;

		if (!rtd->buffer.addr) {
			ret = abox_vdma_alloc_ion_buf(soc_rtd, info, i);
			if (ret < 0) {
				abox_info(dev, "fallback to dma alloc\n");
				ret = abox_vdma_alloc_dma_buf(soc_rtd, info, i);
			}
			if (ret < 0)
				return ret;
		}
		substream->dma_buffer = rtd->buffer;

		if (abox_iova_to_phys(dev_abox, rtd->iova) == 0) {
			ret = abox_iommu_map(dev_abox, rtd->iova,
					substream->dma_buffer.addr,
					substream->dma_buffer.bytes,
					substream->dma_buffer.area);
			if (ret < 0)
				return ret;

			rtd->iommu_mapped = true;
		}

		rtd->substream = substream;
	}

	return 0;
}

static void abox_vdma_pcm_free(struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *soc_rtd = pcm->private_data;
	int id = soc_rtd->dai_link->id;
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct device *dev = info->dev;
	struct device *dev_abox = abox_vdma_dev_abox;
	struct abox_data *abox_data = dev_get_drvdata(dev_abox);
	int i;

	abox_dbg(dev, "%s\n", __func__);

	for (i = 0; i <= SNDRV_PCM_STREAM_LAST; i++) {
		struct snd_pcm_substream *substream = pcm->streams[i].substream;

		if (!substream)
			continue;

		info->rtd[i].substream = NULL;

		if (info->rtd[i].iommu_mapped) {
			abox_iommu_unmap(dev_abox, info->rtd[i].iova);
			info->rtd[i].iommu_mapped = false;
		}

		if (info->rtd[i].ion_buf)
			abox_ion_free(dev, abox_data, info->rtd[i].ion_buf);
		else if (info->rtd[i].dma_alloc)
			snd_dma_free_pages(&info->rtd[i].buffer);
		else
			substream->dma_buffer.area = NULL;
	}
}

static const struct snd_soc_component_driver abox_vdma_component = {
	.probe		= abox_vdma_probe,
	.ops		= &abox_vdma_ops,
	.pcm_new	= abox_vdma_pcm_new,
	.pcm_free	= abox_vdma_pcm_free,
};

static irqreturn_t abox_vdma_ipc_handler(int ipc_id, void *dev_id,
		ABOX_IPC_MSG *msg)
{
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg->msg.pcmtask;
	int id = pcmtask_msg->channel_id;
	int stream = abox_ipcid_to_stream(ipc_id);
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_vdma_rtd *rtd = abox_vdma_get_rtd(info, stream);

	if (!info || !rtd)
		return IRQ_NONE;

	switch (pcmtask_msg->msgtype) {
	case PCM_PLTDAI_POINTER:
		abox_vdma_period_elapsed(info, rtd, pcmtask_msg->param.pointer);
		break;
	case PCM_PLTDAI_ACK:
		/* Ignore ack request and always send ack IPC to firmware. */
		break;
	case PCM_PLTDAI_REGISTER:
	{
		struct PCMTASK_HARDWARE *hardware;
		struct device *dev_abox = dev_id;
		struct abox_data *data = dev_get_drvdata(dev_abox);
		void *area;
		phys_addr_t addr;

		hardware = &pcmtask_msg->param.hardware;
		area = abox_addr_to_kernel_addr(data, hardware->addr);
		addr = abox_addr_to_phys_addr(data, hardware->addr);
		abox_vdma_register(dev_abox, id, stream, area, addr, hardware);
		break;
	}
	default:
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

static struct snd_soc_dai_link abox_vdma_dai_links[VDMA_COUNT_MAX];

static struct snd_soc_card abox_vdma_card = {
	.name = "abox_vdma",
	.owner = THIS_MODULE,
	.dai_link = abox_vdma_dai_links,
	.num_links = 0,
};

SND_SOC_DAILINK_DEF(dailink_comp_dummy, DAILINK_COMP_ARRAY(COMP_DUMMY(),));

static void abox_vdma_register_card_work_func(struct work_struct *work)
{
	int i;

	abox_dbg(abox_vdma_dev_abox, "%s\n", __func__);

	for (i = 0; i < abox_vdma_card.num_links; i++) {
		struct snd_soc_dai_link *link = &abox_vdma_card.dai_link[i];

		if (link->name)
			continue;

		link->name = link->stream_name =
				kasprintf(GFP_KERNEL, "dummy%d", i);
		link->cpus = dailink_comp_dummy;
		link->num_cpus = ARRAY_SIZE(dailink_comp_dummy);
		link->codecs = dailink_comp_dummy;
		link->num_codecs = ARRAY_SIZE(dailink_comp_dummy);
		link->no_pcm = 1;
	}
	abox_register_extra_sound_card(abox_vdma_card.dev, &abox_vdma_card, 1);
}

static DECLARE_DELAYED_WORK(abox_vdma_register_card_work,
		abox_vdma_register_card_work_func);

static int abox_vdma_add_dai_link(struct device *dev)
{
	int id = to_platform_device(dev)->id;
	int idx = abox_vdma_get_idx(id);
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct snd_soc_dai_link *link = &abox_vdma_dai_links[idx];
	bool playback, capture;

	abox_dbg(dev, "%s\n", __func__);

	if (idx > ARRAY_SIZE(abox_vdma_dai_links)) {
		abox_err(dev, "Too many request\n");
		return -ENOMEM;
	}

	cancel_delayed_work_sync(&abox_vdma_register_card_work);

	kfree(link->name);
	link->name = link->stream_name = kstrdup(info->name, GFP_KERNEL);
	link->id = id;
	link->cpus = dailink_comp_dummy;
	link->num_cpus = ARRAY_SIZE(dailink_comp_dummy);
	link->platforms = devm_kcalloc(dev, 1, sizeof(*link->platforms),
			GFP_KERNEL);
	if (!link->platforms)
		return -ENOMEM;
	link->platforms->name = dev_name(dev);
	link->num_platforms = 1;
	link->codecs = dailink_comp_dummy;
	link->num_codecs = ARRAY_SIZE(dailink_comp_dummy);
	link->ignore_suspend = 1;
	link->ignore_pmdown_time = 1;
	link->no_pcm = 0;
	playback = !!info->rtd[SNDRV_PCM_STREAM_PLAYBACK].hardware.info;
	capture = !!info->rtd[SNDRV_PCM_STREAM_CAPTURE].hardware.info;
	link->playback_only = playback && !capture;
	link->capture_only = !playback && capture;

	if (abox_vdma_card.num_links <= idx)
		abox_vdma_card.num_links = idx + 1;

	schedule_delayed_work(&abox_vdma_register_card_work, 10 * HZ);

	return 0;
}

void abox_vdma_register_work_func(struct work_struct *work)
{
	int id, ret;
	struct abox_vdma_info *info;
	struct device *dev;

	abox_dbg(abox_vdma_dev_abox, "%s\n", __func__);

	for (info = abox_vdma_list; (info - abox_vdma_list) <
			ARRAY_SIZE(abox_vdma_list); info++) {
		dev = info->dev;
		id = info->id;
		if (info->legacy) {
			abox_dbg(dev, "%s[%d]\n", __func__, id);
			ret = abox_vdma_add_dai_link(dev);
			if (ret < 0)
				abox_err(dev, "add dai link failed: %d\n", ret);
		}
	}
}

static DECLARE_WORK(abox_vdma_register_work, abox_vdma_register_work_func);

int abox_vdma_register(struct device *dev, int id, int stream,
		void *area, phys_addr_t addr,
		const struct PCMTASK_HARDWARE *pcm_hardware)
{
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_vdma_rtd *rtd = abox_vdma_get_rtd(info, stream);
	struct snd_dma_buffer *buffer = &rtd->buffer;
	struct snd_pcm_hardware *hardware = &rtd->hardware;

	if (!info || !rtd)
		return -EINVAL;

	if (buffer->dev.type)
		return -EEXIST;

	abox_info(dev, "%s(%d, %s, %d, %u)\n", __func__, id, pcm_hardware->name,
			stream, pcm_hardware->buffer_bytes_max);

	info->id = id;
	info->legacy = true;
	strncpy(info->name, pcm_hardware->name, sizeof(info->name) - 1);

	rtd->iova = pcm_hardware->addr;

	buffer->dev.type = SNDRV_DMA_TYPE_DEV;
	buffer->dev.dev = dev;
	buffer->area = area;
	buffer->addr = addr;
	buffer->bytes = pcm_hardware->buffer_bytes_max;

	hardware->info = SNDRV_PCM_INFO_INTERLEAVED
			| SNDRV_PCM_INFO_BLOCK_TRANSFER
			| SNDRV_PCM_INFO_MMAP
			| SNDRV_PCM_INFO_MMAP_VALID
			| SNDRV_PCM_INFO_NO_PERIOD_WAKEUP;
	hardware->formats = width_range_to_bits(pcm_hardware->width_min,
			pcm_hardware->width_max);
	hardware->rates = (pcm_hardware->rate_max > 192000) ?
			SNDRV_PCM_RATE_KNOT : snd_pcm_rate_range_to_bits(
			pcm_hardware->rate_min, pcm_hardware->rate_max);
	hardware->rate_min = pcm_hardware->rate_min;
	hardware->rate_max = pcm_hardware->rate_max;
	hardware->channels_min = pcm_hardware->channels_min;
	hardware->channels_max = pcm_hardware->channels_max;
	hardware->buffer_bytes_max = pcm_hardware->buffer_bytes_max;
	hardware->period_bytes_min = pcm_hardware->period_bytes_min;
	hardware->period_bytes_max = pcm_hardware->period_bytes_max;
	hardware->periods_min = pcm_hardware->periods_min;
	hardware->periods_max = pcm_hardware->periods_max;

	schedule_work(&abox_vdma_register_work);

	return 0;
}

struct device *abox_vdma_register_component(struct device *dev,
		int id, const char *name,
		struct snd_pcm_hardware *playback,
		struct snd_pcm_hardware *capture)
{
	struct abox_vdma_info *info = abox_vdma_get_info(id);
	struct abox_vdma_rtd *rtd;

	if (!info)
		return ERR_PTR(-EINVAL);

	abox_dbg(dev, "%s(%d, %s, %d, %d)\n", __func__, id, name,
			!!playback, !!capture);

	info->id = id;
	info->legacy = false;
	strlcpy(info->name, name, sizeof(info->name));

	if (playback) {
		rtd = abox_vdma_get_rtd(info, SNDRV_PCM_STREAM_PLAYBACK);
		rtd->hardware = *playback;
		rtd->hardware.info = SNDRV_PCM_INFO_INTERLEAVED
			| SNDRV_PCM_INFO_BLOCK_TRANSFER
			| SNDRV_PCM_INFO_MMAP
			| SNDRV_PCM_INFO_MMAP_VALID;
		rtd->buffer.dev.type = SNDRV_DMA_TYPE_DEV;
		rtd->buffer.dev.dev = dev;
		rtd->buffer.bytes = playback->buffer_bytes_max;
	}

	if (capture) {
		rtd = abox_vdma_get_rtd(info, SNDRV_PCM_STREAM_CAPTURE);
		rtd->hardware = *capture;
		rtd->hardware.info = SNDRV_PCM_INFO_INTERLEAVED
			| SNDRV_PCM_INFO_BLOCK_TRANSFER
			| SNDRV_PCM_INFO_MMAP
			| SNDRV_PCM_INFO_MMAP_VALID;
		rtd->buffer.dev.type = SNDRV_DMA_TYPE_DEV;
		rtd->buffer.dev.dev = dev;
		rtd->buffer.bytes = capture->buffer_bytes_max;
	}

	return info->dev;
}

static int samsung_abox_vdma_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int id = pdev->id;
	struct abox_vdma_info *info;
	int ret = 0;

	abox_dbg(dev, "%s\n", __func__);

	if (id < 0) {
		abox_vdma_card.dev = dev;
		schedule_delayed_work(&abox_vdma_register_card_work, 0);
	} else {
		info = abox_vdma_get_info(id);
		if (!info) {
			abox_err(dev, "invalid id: %d\n", id);
			return -EINVAL;
		}

		info->dev = dev;
		dma_set_mask_and_coherent(dev, DMA_BIT_MASK(36));
		pm_runtime_no_callbacks(dev);
		pm_runtime_enable(dev);

		ret = devm_snd_soc_register_component(dev, &abox_vdma_component,
				NULL, 0);
		if (ret < 0)
			abox_err(dev, "register component failed: %d\n", ret);
	}

	return ret;
}

static int samsung_abox_vdma_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	abox_dbg(dev, "%s\n", __func__);

	return 0;
}

static void samsung_abox_vdma_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	abox_dbg(dev, "%s\n", __func__);
	pm_runtime_disable(dev);
}

static const struct platform_device_id samsung_abox_vdma_driver_ids[] = {
	{
		.name = DEVICE_NAME,
	},
	{},
};
MODULE_DEVICE_TABLE(platform, samsung_abox_vdma_driver_ids);

struct platform_driver samsung_abox_vdma_driver = {
	.probe  = samsung_abox_vdma_probe,
	.remove = samsung_abox_vdma_remove,
	.shutdown = samsung_abox_vdma_shutdown,
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
	},
	.id_table = samsung_abox_vdma_driver_ids,
};

#ifdef TEST
static unsigned char test_buf[4096];

static void test_work_func(struct work_struct *work)
{
	struct abox_vdma_info *info = &abox_vdma_list[2];
	static unsigned char i;
	int j;

	pr_debug("%s: %d\n", __func__, i);

	for (j = 0; j < 1024; j++, i++)
		test_buf[i % ARRAY_SIZE(test_buf)] = i;

	abox_vdma_period_elapsed(info, &info->rtd[0], i % ARRAY_SIZE(test_buf));
	abox_vdma_period_elapsed(info, &info->rtd[1], i % ARRAY_SIZE(test_buf));
	schedule_delayed_work(to_delayed_work(work), msecs_to_jiffies(1000));
}
DECLARE_DELAYED_WORK(test_work, test_work_func);

static const struct PCMTASK_HARDWARE test_hardware = {
	.name = "test01",
	.addr = 0x12345678,
	.width_min = 16,
	.width_max = 24,
	.rate_min = 48000,
	.rate_max = 192000,
	.channels_min = 2,
	.channels_max = 2,
	.buffer_bytes_max = 4096,
	.period_bytes_min = 1024,
	.period_bytes_max = 2048,
	.periods_min = 2,
	.periods_max = 4,
};
#endif

void abox_vdma_register_card(struct device *dev_abox)
{
	if (!dev_abox)
		return;

	platform_device_register_data(dev_abox, DEVICE_NAME, -1, NULL, 0);
}

void abox_vdma_init(struct device *dev_abox)
{
	struct platform_device *pdev;
	int i;

	abox_info(dev_abox, "%s\n", __func__);

	abox_vdma_dev_abox = dev_abox;
	abox_register_ipc_handler(dev_abox, IPC_PCMPLAYBACK,
			abox_vdma_ipc_handler, dev_abox);
	abox_register_ipc_handler(dev_abox, IPC_PCMCAPTURE,
			abox_vdma_ipc_handler, dev_abox);

	for (i = 0; i < VDMA_COUNT_MAX; i++) {
		pdev = platform_device_register_data(dev_abox, DEVICE_NAME,
				PCMTASK_VDMA_ID_BASE + i, NULL, 0);
		if (IS_ERR(pdev))
			abox_err(dev_abox, "vdma(%d) register failed: %ld\n",
					i, PTR_ERR(pdev));
	}
#ifdef TEST
	abox_vdma_register(dev_abox, 102, SNDRV_PCM_STREAM_PLAYBACK, test_buf,
			virt_to_phys(test_buf), &test_hardware);
	abox_vdma_register(dev_abox, 102, SNDRV_PCM_STREAM_CAPTURE, test_buf,
			virt_to_phys(test_buf), &test_hardware);
	schedule_delayed_work(&test_work, HZ * 5);
#endif
}
