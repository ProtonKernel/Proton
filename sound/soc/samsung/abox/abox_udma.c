/* sound/soc/samsung/abox/abox_udma.c
 *
 * ALSA SoC Audio Layer - Samsung Abox UDMA driver
 *
 * Copyright (c) 2020 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <sound/pcm_params.h>

#include "abox.h"
#include "abox_cmpnt.h"
#include "abox_dma.h"
#include "abox_udma.h"
#include "abox_memlog.h"

#define VERBOSE 0

static int abox_udma_request_ipc(struct abox_dma_data *data,
		ABOX_IPC_MSG *msg, int atomic, int sync)
{
	return abox_request_ipc(data->dev_abox, msg->ipcid, msg, sizeof(*msg),
			atomic, sync);
}

static bool abox_udma_has_playback(struct abox_dma_data *data)
{
	return !!data->dai_drv[0].playback.formats;
}

static int abox_udma_get_ipcid(struct abox_dma_data *data)
{
	return abox_udma_has_playback(data) ? IPC_USBPLAYBACK : IPC_USBCAPTURE;
}

static irqreturn_t abox_udma_ipc_handler(int ipc, void *dev_id,
		ABOX_IPC_MSG *msg)
{
	struct abox_data *abox_data = dev_id;
	struct abox_dma_data *data;
	struct device *dev;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg->msg.pcmtask;
	int id = pcmtask_msg->channel_id;

	switch (ipc) {
	case IPC_USBPLAYBACK:
		if (id >= ARRAY_SIZE(abox_data->dev_udma_rd))
			return IRQ_NONE;
		dev = abox_data->dev_udma_rd[id];
		break;
	case IPC_USBCAPTURE:
		if (id >= ARRAY_SIZE(abox_data->dev_udma_wr))
			return IRQ_NONE;
		dev = abox_data->dev_udma_wr[id];
		break;
	default:
		return IRQ_NONE;
	}
	data = dev_get_drvdata(dev);

	abox_dbg(dev, "%s(%d)\n", __func__, pcmtask_msg->msgtype);

	switch (pcmtask_msg->msgtype) {
	default:
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

static int abox_udma_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = data->dev;
	struct abox_data *abox_data = data->abox_data;
	int id = data->id;
	int ret;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	abox_dbg(dev, "%s\n", __func__);

	abox_wait_restored(abox_data);

	abox_dma_ops.open(substream);

	msg.ipcid = abox_udma_get_ipcid(data);
	pcmtask_msg->msgtype = PCM_PLTDAI_OPEN;
	msg.task_id = pcmtask_msg->channel_id = id;
	ret = abox_udma_request_ipc(data, &msg, 0, 0);

	return ret;
}

static int abox_udma_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = data->dev;
	struct abox_data *abox_data = data->abox_data;
	int id = data->id;
	int ret;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	abox_dbg(dev, "%s\n", __func__);

	abox_dma_barrier(dev, data, 0);
	abox_dma_ops.close(substream);

	msg.ipcid = abox_udma_get_ipcid(data);
	pcmtask_msg->msgtype = PCM_PLTDAI_CLOSE;
	msg.task_id = pcmtask_msg->channel_id = id;
	ret = abox_udma_request_ipc(data, &msg, 0, 1);

	abox_request_cpu_gear_dai(dev, abox_data, rtd->cpu_dai, 0);

	return ret;
}

static int abox_udma_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(cpu_dai);
	struct device *dev = data->dev;
	struct abox_data *abox_data = data->abox_data;
	struct device *dev_abox = abox_data->dev;
	int id = data->id;
	size_t buffer_bytes = params_buffer_bytes(params);
	unsigned int iova = abox_dma_iova(data);
	phys_addr_t phys;
	int ret;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	abox_dbg(dev, "%s\n", __func__);

	switch (data->buf_type) {
	case BUFFER_TYPE_RAM:
		if (data->ramb.bytes >= buffer_bytes) {
			iova = data->ramb.addr;
			phys = data->ramb.addr;
			break;
		}
		/* fallback to DMA mode */
		abox_info(dev, "fallback to dma buffer\n");
	case BUFFER_TYPE_DMA:
		if (data->dmab.bytes < buffer_bytes) {
			abox_iommu_unmap(dev_abox, iova);
			snd_dma_free_pages(&data->dmab);
			ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV,
					dev, PAGE_ALIGN(buffer_bytes),
					&data->dmab);
			if (ret < 0)
				return ret;
			ret = abox_iommu_map(dev_abox, iova, data->dmab.addr,
					data->dmab.bytes, data->dmab.area);
			if (ret < 0)
				return ret;
			abox_info(dev, "dma buffer changed\n");
		}
		phys = data->dmab.addr;
		break;
	case BUFFER_TYPE_ION:
		if (data->dmab.bytes < buffer_bytes)
			return -ENOMEM;

		abox_info(dev, "ion_buffer %s bytes(%zu) size(%zu)\n",
				__func__, buffer_bytes, data->ion_buf->size);
		/* physical address of scatter-gather ion buffer is useless */
		phys = data->dmab.addr;
		break;
	default:
		abox_err(dev, "buf_type is not defined\n");
		break;
	}

	/* udma is always backend */
	data->backend = true;

	ret = abox_dma_ops.hw_params(substream, params);
	if (ret < 0)
		return ret;

	pcmtask_msg->channel_id = id;
	msg.ipcid = abox_udma_get_ipcid(data);
	msg.task_id = pcmtask_msg->channel_id = id;

	pcmtask_msg->msgtype = PCM_SET_BUFFER;
	pcmtask_msg->param.setbuff.addr = iova;
	pcmtask_msg->param.setbuff.size = params_period_bytes(params);
	pcmtask_msg->param.setbuff.count = params_periods(params);
	pcmtask_msg->param.setbuff.phys = phys;

	ret = abox_udma_request_ipc(data, &msg, 0, 0);
	if (ret < 0)
		return ret;

	pcmtask_msg->msgtype = PCM_PLTDAI_HW_PARAMS;
	pcmtask_msg->param.hw_params.sample_rate = params_rate(params);
	pcmtask_msg->param.hw_params.bit_depth = params_width(params);
	pcmtask_msg->param.hw_params.channels = params_channels(params);
	pcmtask_msg->param.hw_params.packed =
			(params_format(params) == SNDRV_PCM_FORMAT_S24_3LE);
	ret = abox_udma_request_ipc(data, &msg, 0, 0);
	if (ret < 0)
		return ret;

	abox_dbg(dev, "%s:Total=%u PrdSz=%u(%u) #Prds=%u rate=%u, width=%d, channels=%u\n",
			snd_pcm_stream_str(substream),
			params_buffer_bytes(params), params_period_size(params),
			params_period_bytes(params), params_periods(params),
			params_rate(params), params_width(params),
			params_channels(params));

	return 0;
}

static int abox_udma_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = data->dev;
	int id = data->id;
	int ret;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	abox_dbg(dev, "%s\n", __func__);

	data->pointer = abox_dma_iova(data);

	msg.ipcid = abox_udma_get_ipcid(data);
	pcmtask_msg->msgtype = PCM_PLTDAI_PREPARE;
	msg.task_id = pcmtask_msg->channel_id = id;
	ret = abox_udma_request_ipc(data, &msg, 0, 0);

	return ret;
}

static int abox_udma_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(cpu_dai);
	struct device *dev = data->dev;
	int id = data->id;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	abox_dbg(dev, "%s\n", __func__);

	msg.ipcid = abox_udma_get_ipcid(data);
	pcmtask_msg->msgtype = PCM_PLTDAI_HW_FREE;
	msg.task_id = pcmtask_msg->channel_id = id;
	abox_udma_request_ipc(data, &msg, 0, 0);

	abox_dma_ops.hw_free(substream);

	return 0;
}

static snd_pcm_uframes_t abox_udma_pointer(struct snd_pcm_substream *substream)
{
	return abox_dma_ops.pointer(substream);
}

static int abox_udma_mmap(struct snd_pcm_substream *substream,
		struct vm_area_struct *vma)
{
	return abox_dma_ops.mmap(substream, vma);
}

static const struct snd_pcm_ops abox_udma_ops = {
	.open		= abox_udma_open,
	.close		= abox_udma_close,
	.hw_params	= abox_udma_hw_params,
	.hw_free	= abox_udma_hw_free,
	.prepare	= abox_udma_prepare,
	.pointer	= abox_udma_pointer,
	.mmap		= abox_udma_mmap,
};

static int abox_udma_mute_stream(struct snd_soc_dai *dai, int mute, int stream)
{
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(dai);
	struct device *dev = dai->dev;
	int id = data->id;
	int ret;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	abox_info(dev, "%s(%d)\n", __func__, mute);

	if (data->enabled == !mute)
		return 0;
	data->enabled = !mute;

	msg.ipcid = abox_udma_get_ipcid(data);
	msg.task_id = pcmtask_msg->channel_id = id;
	pcmtask_msg->msgtype = PCM_PLTDAI_TRIGGER;
	pcmtask_msg->param.trigger = !mute;
	ret = abox_udma_request_ipc(data, &msg, 0, 0);

	return ret;
}

static const struct snd_soc_dai_ops abox_udma_dai_ops = {
	.mute_stream	= abox_udma_mute_stream,
};

static unsigned int abox_udma_read(struct snd_soc_component *cmpnt,
		unsigned int reg)
{
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	struct abox_data *abox_data = data->abox_data;
	unsigned int base, val = 0;

	if (reg > DMA_REG_MAX) {
		abox_warn(cmpnt->dev, "invalid dma register:%#x\n", reg);
		dump_stack();
	}

	base = (unsigned int)(data->sfr_phys - abox_data->sfr_phys);
	snd_soc_component_read(abox_data->cmpnt, base + reg, &val);

	if (VERBOSE)
		abox_dbg(cmpnt->dev, "%s(%#x): %#x\n", __func__, reg, val);

	return val;
}

static int abox_udma_write(struct snd_soc_component *cmpnt,
		unsigned int reg, unsigned int val)
{
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	struct abox_data *abox_data = data->abox_data;
	unsigned int base;
	int ret;

	if (VERBOSE)
		abox_dbg(cmpnt->dev, "%s(%#x, %#x)\n", __func__, reg, val);

	if (reg > DMA_REG_MAX) {
		abox_warn(cmpnt->dev, "invalid dma register:%#x\n", reg);
		dump_stack();
	}

	base = (unsigned int)(data->sfr_phys - abox_data->sfr_phys);
	ret = snd_soc_component_write(abox_data->cmpnt, base + reg, val);
	if (ret < 0)
		return ret;

	return 0;
}

static int abox_udma_probe(struct snd_soc_component *cmpnt)
{
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	struct device *dev = data->dev;
	struct device *dev_abox = data->abox_data->dev;
	size_t buffer_bytes = data->dmab.bytes;
	int ret = 0;

	abox_dbg(dev, "%s\n", __func__);

	abox_dma.probe(cmpnt);
	/* Set UDMA to discrete operation mode */
	snd_soc_component_update_bits(cmpnt, DMA_REG_CTRL,
			ABOX_DMA_BUF_TYPE_MASK, 1 << ABOX_DMA_BUF_TYPE_L);
	abox_dma_hw_params_set(data->dev, 48000, 16, 2, 48, 2, false);
	abox_register_ipc_handler(data->dev_abox, abox_udma_get_ipcid(data),
			abox_udma_ipc_handler, data->abox_data);

	switch (data->buf_type) {
	case BUFFER_TYPE_ION:
		buffer_bytes = BUFFER_ION_BYTES_MAX;
		data->ion_buf = abox_ion_alloc(dev, data->abox_data,
				abox_dma_iova(data), buffer_bytes, false);
		if (IS_ERR(data->ion_buf))
			return PTR_ERR(data->ion_buf);

		/* update buffer infomation using ion allocated buffer  */
		data->dmab.area = data->ion_buf->kva;
		data->dmab.addr = data->ion_buf->iova;
		data->dmab.bytes  = data->ion_buf->size;
		break;
	case BUFFER_TYPE_DMA:
		if (buffer_bytes < BUFFER_BYTES_MIN)
			buffer_bytes = BUFFER_BYTES_MIN;

		ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, dev,
				buffer_bytes, &data->dmab);
		if (ret < 0)
			return ret;

		ret = abox_iommu_map(dev_abox, abox_dma_iova(data),
				data->dmab.addr, data->dmab.bytes,
				data->dmab.area);
		break;
	case BUFFER_TYPE_RAM:
		ret = abox_of_get_addr(data->abox_data, dev->of_node,
				"samsung,buffer-address",
				(void **)&data->ramb.area,
				&data->ramb.addr, &data->ramb.bytes);
		break;
	}

	return ret;
}

static void abox_udma_remove(struct snd_soc_component *cmpnt)
{
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	struct device *dev = data->dev;
	struct device *dev_abox = data->abox_data->dev;
	int ret = 0;

	abox_dbg(dev, "%s\n", __func__);

	switch (data->buf_type) {
	case BUFFER_TYPE_ION:
		ret = abox_ion_free(dev, data->abox_data, data->ion_buf);
		if (ret < 0)
			abox_err(dev, "abox_ion_free() failed %d\n", ret);
		break;
	case BUFFER_TYPE_DMA:
		abox_iommu_unmap(dev_abox, data->dmab.addr);
		snd_dma_free_pages(&data->dmab);
		break;
	case BUFFER_TYPE_RAM:
		/* nothing to do */
		break;
	}

	abox_dma.remove(cmpnt);
}

static const struct snd_kcontrol_new abox_udma_controls[] = {
	SOC_SINGLE_EXT("Rate", DMA_RATE, 0, 384000, 0,
			abox_dma_hw_params_get, abox_dma_hw_params_put),
	SOC_SINGLE_EXT("Width", DMA_WIDTH, 0, 32, 0,
			abox_dma_hw_params_get, abox_dma_hw_params_put),
	SOC_SINGLE_EXT("Channel", DMA_CHANNEL, 0, 8, 0,
			abox_dma_hw_params_get, abox_dma_hw_params_put),
	SOC_SINGLE_EXT("Period", DMA_PERIOD, 0, INT_MAX, 0,
			abox_dma_hw_params_get, abox_dma_hw_params_put),
	SOC_SINGLE_EXT("Periods", DMA_PERIODS, 0, INT_MAX, 0,
			abox_dma_hw_params_get, abox_dma_hw_params_put),
	SOC_SINGLE_EXT("Packed", DMA_PACKED, 0, 1, 0,
			abox_dma_hw_params_get, abox_dma_hw_params_put),
	SOC_SINGLE("Auto Fade In", DMA_REG_CTRL, ABOX_DMA_AUTO_FADE_IN_L,
			1, 0),
	SOC_SINGLE("Vol Factor", DMA_REG_VOL_FACTOR, ABOX_DMA_VOL_FACTOR_L,
			0xffffff, 0),
	SOC_SINGLE("Vol Change", DMA_REG_VOL_CHANGE, ABOX_DMA_VOL_FACTOR_L,
			0xffffff, 0),
};

static const struct snd_kcontrol_new abox_udma_rd_controls[] = {
	SOC_SINGLE("Expand", DMA_REG_CTRL, ABOX_DMA_EXPAND_L, 1, 0),
};

static const struct snd_soc_dapm_widget abox_udma_rd_widgets[] = {
	SND_SOC_DAPM_INPUT("INPUT"),
};

static const struct snd_soc_dapm_route abox_udma_rd_routes[] = {
	/* sink, control, source */
	{"Capture", NULL, "INPUT"},
};

static int abox_udma_rd_probe(struct snd_soc_component *cmpnt)
{
	struct device *dev = cmpnt->dev;
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);

	abox_dbg(dev, "%s\n", __func__);

	snd_soc_add_component_controls(cmpnt, abox_udma_rd_controls,
			ARRAY_SIZE(abox_udma_rd_controls));
	abox_udma_probe(cmpnt);
	abox_cmpnt_register_udma_rd(data->abox_data->dev, dev, data->id,
			data->dai_drv[DMA_DAI_PCM].name);
	return 0;
}

static const struct snd_soc_component_driver abox_udma_rd = {
	.controls		= abox_udma_controls,
	.num_controls		= ARRAY_SIZE(abox_udma_controls),
	.dapm_widgets		= abox_udma_rd_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(abox_udma_rd_widgets),
	.dapm_routes		= abox_udma_rd_routes,
	.num_dapm_routes	= ARRAY_SIZE(abox_udma_rd_routes),
	.probe			= abox_udma_rd_probe,
	.remove			= abox_udma_remove,
	.read			= abox_udma_read,
	.write			= abox_udma_write,
	.ops			= &abox_udma_ops,
};

static enum abox_irq abox_udma_rd_get_irq(struct abox_dma_data *data,
		enum abox_dma_irq irq)
{
	unsigned int id = data->id;
	enum abox_irq ret;

	switch (irq) {
	case DMA_IRQ_BUF_EMPTY:
		ret = IRQ_UDMA_RD0_BUF_EMPTY + id;
		break;
	case DMA_IRQ_ERR:
		ret = IRQ_UDMA_RD0_ERR + id;
		break;
	case DMA_IRQ_FADE_DONE:
		ret = IRQ_UDMA_RD0_FADE_DONE + id;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum abox_dai abox_udma_rd_get_dai_id(enum abox_dma_dai dai, int id)
{
	return ABOX_UDMA_RD0 + id;
}

static char *abox_udma_rd_get_dai_name(struct device *dev,
		enum abox_dma_dai dai, int id)
{
	return devm_kasprintf(dev, GFP_KERNEL, "UDMA RD%d", id);
}

static const struct snd_soc_dapm_widget abox_udma_wr_widgets[] = {
	SND_SOC_DAPM_OUTPUT("OUTPUT"),
};

static const struct snd_soc_dapm_route abox_udma_wr_routes[] = {
	/* sink, control, source */
	{"OUTPUT", NULL, "Playback"},
};

static int abox_udma_wr_probe(struct snd_soc_component *cmpnt)
{
	struct device *dev = cmpnt->dev;
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);

	abox_dbg(dev, "%s\n", __func__);

	abox_udma_probe(cmpnt);
	abox_cmpnt_register_udma_wr(data->abox_data->dev, dev, data->id,
			data->dai_drv[DMA_DAI_PCM].name);
	return 0;
}

static const struct snd_soc_component_driver abox_udma_wr = {
	.controls		= abox_udma_controls,
	.num_controls		= ARRAY_SIZE(abox_udma_controls),
	.dapm_widgets		= abox_udma_wr_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(abox_udma_wr_widgets),
	.dapm_routes		= abox_udma_wr_routes,
	.num_dapm_routes	= ARRAY_SIZE(abox_udma_wr_routes),
	.probe			= abox_udma_wr_probe,
	.remove			= abox_udma_remove,
	.read			= abox_udma_read,
	.write			= abox_udma_write,
	.ops			= &abox_udma_ops,
};

static enum abox_irq abox_udma_wr_get_irq(struct abox_dma_data *data,
		enum abox_dma_irq irq)
{
	unsigned int id = data->id;
	enum abox_irq ret;

	switch (irq) {
	case DMA_IRQ_BUF_EMPTY:
		ret = IRQ_UDMA_WR0_BUF_FULL + id;
		break;
	case DMA_IRQ_ERR:
		ret = IRQ_UDMA_WR0_ERR + id;
		break;
	case DMA_IRQ_FADE_DONE:
		ret = IRQ_UDMA_WR0_FADE_DONE + id;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum abox_dai abox_udma_wr_get_dai_id(enum abox_dma_dai dai, int id)
{
	return ABOX_UDMA_WR0 + id;
}

static char *abox_udma_wr_get_dai_name(struct device *dev,
		enum abox_dma_dai dai, int id)
{
	return devm_kasprintf(dev, GFP_KERNEL, "UDMA WR%d", id);
}

static const struct snd_soc_dai_driver abox_udma_cap_dai_drv[] = {
	{
		.ops = &abox_udma_dai_ops,
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
	},
};

static const struct snd_soc_dai_driver abox_udma_pla_dai_drv[] = {
	{
		.ops = &abox_udma_dai_ops,
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
	},
};

struct abox_dma_of_data abox_udma_rd_of_data = {
	.get_irq = abox_udma_rd_get_irq,
	.get_dai_id = abox_udma_rd_get_dai_id,
	.get_dai_name = abox_udma_rd_get_dai_name,
	.dai_drv = abox_udma_cap_dai_drv,
	.num_dai = 1,
	.cmpnt_drv = &abox_udma_rd,
};

struct abox_dma_of_data abox_udma_wr_of_data = {
	.get_irq = abox_udma_wr_get_irq,
	.get_dai_id = abox_udma_wr_get_dai_id,
	.get_dai_name = abox_udma_wr_get_dai_name,
	.dai_drv = abox_udma_pla_dai_drv,
	.num_dai = 1,
	.cmpnt_drv = &abox_udma_wr,
};

static const char * const abox_udma_trigger_method_texts[] = {
	"USB", "SFR",
};

static SOC_ENUM_SINGLE_DECL(abox_udma_rd0_trigger_method_enum,
		ABOX_UDMA_TRIGGER_CTRL_RD(0), ABOX_UDMA_TRIGGER_METHOD_L,
		abox_udma_trigger_method_texts);

static SOC_ENUM_SINGLE_DECL(abox_udma_rd1_trigger_method_enum,
		ABOX_UDMA_TRIGGER_CTRL_RD(1), ABOX_UDMA_TRIGGER_METHOD_L,
		abox_udma_trigger_method_texts);

static SOC_ENUM_SINGLE_DECL(abox_udma_wr0_trigger_method_enum,
		ABOX_UDMA_TRIGGER_CTRL_WR(0), ABOX_UDMA_TRIGGER_METHOD_L,
		abox_udma_trigger_method_texts);

static SOC_ENUM_SINGLE_DECL(abox_udma_wr1_trigger_method_enum,
		ABOX_UDMA_TRIGGER_CTRL_WR(1), ABOX_UDMA_TRIGGER_METHOD_L,
		abox_udma_trigger_method_texts);

static const struct snd_kcontrol_new abox_udma_global_controls[] = {
	SOC_ENUM("UDMA RD0 Trigger Method", abox_udma_rd0_trigger_method_enum),
	SOC_SINGLE("UDMA RD0 Trigger Counter", ABOX_UDMA_TRIGGER_CNT_RD(0),
			ABOX_UDMA_TRIGGER_CNT_VAL_L, 0xffff, 0),
	SOC_SINGLE("UDMA RD0 Trigger Offset", ABOX_UDMA_TRIGGER_OFFSET_RD(0),
			ABOX_UDMA_TRIGGER_OFFSET_L, 0xffff, 0),

	SOC_ENUM("UDMA RD1 Trigger Method", abox_udma_rd1_trigger_method_enum),
	SOC_SINGLE("UDMA RD1 Trigger Counter", ABOX_UDMA_TRIGGER_CNT_RD(1),
			ABOX_UDMA_TRIGGER_CNT_VAL_L, 0xffff, 0),
	SOC_SINGLE("UDMA RD1 Trigger Offset", ABOX_UDMA_TRIGGER_OFFSET_RD(1),
			ABOX_UDMA_TRIGGER_OFFSET_L, 0xffff, 0),

	SOC_ENUM("UDMA WR0 Trigger Method", abox_udma_wr0_trigger_method_enum),
	SOC_SINGLE("UDMA WR0 Trigger Counter", ABOX_UDMA_TRIGGER_CNT_WR(0),
			ABOX_UDMA_TRIGGER_CNT_VAL_L, 0xffff, 0),
	SOC_SINGLE("UDMA WR0 Trigger Offset", ABOX_UDMA_TRIGGER_OFFSET_WR(0),
			ABOX_UDMA_TRIGGER_OFFSET_L, 0xffff, 0),

	SOC_ENUM("UDMA WR1 Trigger Method", abox_udma_wr1_trigger_method_enum),
	SOC_SINGLE("UDMA WR1 Trigger Counter", ABOX_UDMA_TRIGGER_CNT_WR(1),
			ABOX_UDMA_TRIGGER_CNT_VAL_L, 0xffff, 0),
	SOC_SINGLE("UDMA WR1 Trigger Offset", ABOX_UDMA_TRIGGER_OFFSET_WR(1),
			ABOX_UDMA_TRIGGER_OFFSET_L, 0xffff, 0),
};

int abox_udma_init(struct abox_data *data)
{
	struct snd_soc_component *cmpnt = data->cmpnt;

	if (!cmpnt)
		return -EAGAIN;

	return snd_soc_add_component_controls(cmpnt, abox_udma_global_controls,
			ARRAY_SIZE(abox_udma_global_controls));
}
