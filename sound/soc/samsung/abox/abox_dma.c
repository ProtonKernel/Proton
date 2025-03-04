/* sound/soc/samsung/abox/abox_dma.c
 *
 * ALSA SoC Audio Layer - Samsung Abox DMA driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <linux/regmap.h>
#include <linux/iommu.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>

#include <sound/samsung/abox.h>
#include "abox_util.h"
#include "abox_gic.h"
#include "abox_cmpnt.h"
#include "abox.h"
#include "abox_dump.h"
#include "abox_udma.h"
#include "abox_dma.h"
#include "abox_memlog.h"

static const struct snd_pcm_hardware abox_dma_hardware = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED
				| SNDRV_PCM_INFO_BLOCK_TRANSFER
				| SNDRV_PCM_INFO_MMAP
				| SNDRV_PCM_INFO_MMAP_VALID,
	.formats		= ABOX_SAMPLE_FORMATS,
	.channels_min		= 1,
	.channels_max		= 8,
	.buffer_bytes_max	= BUFFER_BYTES_MAX,
	.period_bytes_min	= PERIOD_BYTES_MIN,
	.period_bytes_max	= PERIOD_BYTES_MAX,
	.periods_min		= BUFFER_BYTES_MAX / PERIOD_BYTES_MAX,
	.periods_max		= BUFFER_BYTES_MAX / PERIOD_BYTES_MIN,
};

unsigned int abox_dma_iova(struct abox_dma_data *data)
{
	unsigned int id = data->dai_drv->id;
	unsigned int ret;

	if (id >= ABOX_RDMA0 && id < ABOX_WDMA0)
		ret = IOVA_RDMA_BUFFER(id - ABOX_RDMA0);
	else if (id >= ABOX_WDMA0 && id < ABOX_WDMA0_DUAL)
		ret = IOVA_WDMA_BUFFER(id - ABOX_WDMA0);
	else if (id >= ABOX_WDMA0_DUAL && id < ABOX_DDMA0)
		ret = IOVA_DUAL_BUFFER(id - ABOX_WDMA0_DUAL);
	else if (id >= ABOX_DDMA0 && id < ABOX_UAIF0)
		ret = IOVA_DDMA_BUFFER(id - ABOX_DDMA0);
	else if (id >= ABOX_UDMA_RD0 && id < ABOX_UDMA_WR0)
		ret = IOVA_UDMA_RD_BUFFER(id - ABOX_UDMA_RD0);
	else if (id >= ABOX_UDMA_WR0 && id < ABOX_UDMA_WR0_DUAL)
		ret = IOVA_UDMA_WR_BUFFER(id - ABOX_UDMA_WR0);
	else if (id >= ABOX_UDMA_WR0_DUAL && id < ABOX_UDMA_WR_DBG0)
		ret = IOVA_UDMA_WR_DUAL_BUFFER(id - ABOX_UDMA_WR0_DUAL);
	else if (id >= ABOX_UDMA_WR_DBG0 && id < ABOX_BI_PDI0)
		ret = IOVA_UDMA_WR_DBG_BUFFER(id - ABOX_UDMA_WR_DBG0);
	else
		ret = 0;

	return ret;
}

bool abox_dma_is_sync_mode(struct abox_dma_data *data)
{
	struct abox_data *abox_data = data->abox_data;
	int id = data->id;
	unsigned int val = 0;

	if (id >= ARRAY_SIZE(abox_data->dev_rdma) || !abox_data->dev_rdma[id])
		return false;

	data = dev_get_drvdata(abox_data->dev_rdma[id]);
	snd_soc_component_read(data->cmpnt, DMA_REG_CTRL0, &val);

	return ((val & ABOX_DMA_SYNC_MODE_MASK) >> ABOX_DMA_SYNC_MODE_L);
}

enum abox_irq abox_dma_get_irq(struct abox_dma_data *data,
		enum abox_dma_irq irq)
{
	if (!data || !data->of_data || !data->of_data->get_irq)
		return -EINVAL;

	return data->of_data->get_irq(data, irq);
}

void abox_dma_acquire_irq(struct abox_dma_data *data, enum abox_dma_irq dma_irq)
{
	struct device *dev_gic = data->abox_data->dev_gic;
	enum abox_irq irq;

	/* Acquire IRQ from the firmware */
	irq = abox_dma_get_irq(data, dma_irq);
	if (irq < 0 || irq > IRQ_COUNT)
		return;

	abox_gic_target(dev_gic, irq, ABOX_GIC_AP);
	abox_gic_enable(dev_gic, irq, true);
}

void abox_dma_release_irq(struct abox_dma_data *data, enum abox_dma_irq dma_irq)
{
	struct device *dev_gic = data->abox_data->dev_gic;
	enum abox_irq irq;

	/* Return back IRQ to the firmware */
	irq = abox_dma_get_irq(data, dma_irq);
	if (irq < 0 || irq > IRQ_COUNT)
		return;

	abox_gic_enable(dev_gic, irq, false);
	abox_gic_target(dev_gic, irq, ABOX_GIC_CORE0);
}

static void abox_dma_acquire_irq_all(struct abox_dma_data *data)
{
	int i;

	/* Acquire IRQ from the firmware */
	for (i = DMA_IRQ_BUF_DONE; i < DMA_IRQ_COUNT; i++)
		abox_dma_acquire_irq(data, i);
}

static void abox_dma_release_irq_all(struct abox_dma_data *data)
{
	int i;

	/* Return back IRQ to the firmware */
	for (i = DMA_IRQ_BUF_DONE; i < DMA_IRQ_COUNT; i++)
		abox_dma_release_irq(data, i);
}

int abox_dma_register_irq(struct abox_dma_data *data,
		enum abox_dma_irq irq, irq_handler_t handler, void *dev_id)
{
	struct device *dev_gic = data->abox_data->dev_gic;
	enum abox_irq _irq;

	_irq = abox_dma_get_irq(data, irq);
	if (_irq < 0 || _irq > IRQ_COUNT)
		return -EINVAL;

	return abox_gic_register_irq_handler(dev_gic, _irq, handler, dev_id);
}

void abox_dma_unregister_irq(struct abox_dma_data *data,
		enum abox_dma_irq irq)
{
	struct device *dev_gic = data->abox_data->dev_gic;
	enum abox_irq _irq;

	_irq = abox_dma_get_irq(data, irq);
	if (_irq < 0 || _irq > IRQ_COUNT)
		return;

	abox_gic_unregister_irq_handler(dev_gic, _irq);
}

int abox_dma_mixer_control_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	int sign_bit = mc->sign_bit;
	unsigned int mask = (1 << fls(max)) - 1;
	int val;
	int ret;

	if (sign_bit)
		mask = (unsigned int)(BIT(sign_bit + 1) - 1);

	ret = snd_soc_component_read(cmpnt, reg, &val);
	if (ret < 0)
		return ret;

	val >>= shift;
	val &= mask;

	if (sign_bit) {
		/* use shift for sign extension */
		val <<= 31 - sign_bit;
		val >>= 31 - sign_bit;
	}

	ucontrol->value.integer.value[0] = val;

	return 0;
}

int abox_dma_mixer_control_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int sign_bit = mc->sign_bit;
	unsigned int mask = (1 << fls(max)) - 1;
	int err;
	unsigned int val, val_mask;

	if (sign_bit)
		mask = (unsigned int)(BIT(sign_bit + 1) - 1);

	val = (unsigned int)(ucontrol->value.integer.value[0] << shift);
	val_mask = mask << shift;
	err = snd_soc_component_update_bits(cmpnt, reg, val_mask, val);
	if (err < 0)
		return err;

	return 0;
}

static int abox_dma_progress(struct snd_soc_component *cmpnt)
{
	unsigned int val = 0;

	snd_soc_component_read(cmpnt, DMA_REG_STATUS, &val);

	return !!(val & ABOX_DMA_PROGRESS_MASK);
}

void abox_dma_barrier(struct device *dev, struct abox_dma_data *data,
		int enable)
{
	const int wait_ns = 10000000; /* 10ms */
	u64 timeout = local_clock() + wait_ns;

	while (abox_dma_progress(data->cmpnt) != enable) {
		if (local_clock() <= timeout) {
			udelay(10);
			continue;
		}
		dev_warn_ratelimited(dev, "%s timeout\n",
				enable ? "enable" : "disable");

		/* Disable DMA by force */
		snd_soc_component_update_bits(data->cmpnt, DMA_REG_CTRL,
				ABOX_DMA_ENABLE_MASK, 1 << ABOX_DMA_ENABLE_L);
		snd_soc_component_update_bits(data->cmpnt, DMA_REG_CTRL,
				ABOX_DMA_ENABLE_MASK, 0 << ABOX_DMA_ENABLE_L);
		break;
	}
}

static irqreturn_t abox_dma_buf_done(int irq, void *dev_id)
{
	struct device *dev = dev_id;
	struct abox_dma_data *data = dev_get_drvdata(dev);

	abox_dbg(dev, "%s(%d)\n", __func__, irq);

	snd_pcm_period_elapsed(data->substream);

	return IRQ_HANDLED;
}

static irqreturn_t abox_dma_fade_done(int irq, void *dev_id)
{
	struct device *dev = dev_id;
	struct abox_dma_data *data = dev_get_drvdata(dev);

	abox_dbg(dev, "%s(%d)\n", __func__, irq);

	complete(&data->func_changed);

	return IRQ_HANDLED;
}

static irqreturn_t abox_dma_err(int irq, void *dev_id)
{
	struct device *dev = dev_id;
	struct abox_dma_data *data = dev_get_drvdata(dev);
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int reg, val;
	int ret;

	abox_dbg(dev, "%s(%d)\n", __func__, irq);

	abox_err(dev, "Error\n");
	for (reg = DMA_REG_CTRL0; reg <= DMA_REG_MAX; reg++) {
		ret = snd_soc_component_read(cmpnt, reg, &val);
		if (ret < 0)
			continue;

		abox_err(dev, "%08x: %08x\n", reg, val);
	}

	return IRQ_HANDLED;
}

static const irq_handler_t abox_dma_irq_handlers[DMA_IRQ_COUNT] = {
	abox_dma_buf_done,
	abox_dma_fade_done,
	abox_dma_err,
};

static int abox_dma_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct snd_soc_component *cmpnt = data->cmpnt;
	struct device *dev = data->dev;
	struct device *dev_abox = data->dev_abox;
	unsigned int iova = abox_dma_iova(data);
	unsigned int reg, mask, val;
	int ret;

	abox_dbg(dev, "%s\n", __func__);

	data->hw_params = *params;

	if (!rtd->dai_link->no_pcm) {
		ret = snd_pcm_lib_malloc_pages(substream,
				params_buffer_bytes(params));
		if (ret < 0) {
			abox_err(dev, "memory allocation fail: %d\n", ret);
			return ret;
		} else if (ret > 0) {
			abox_iommu_unmap(dev_abox, iova);
			ret = abox_iommu_map(dev_abox, iova, runtime->dma_addr,
					PAGE_ALIGN(runtime->dma_bytes),
					runtime->dma_area);
			if (ret < 0) {
				abox_err(dev, "memory mapping fail: %d\n", ret);
				snd_pcm_lib_free_pages(substream);
				return ret;
			}
		}
	}

	if (data->buf_type == BUFFER_TYPE_RAM) {
		if (data->ramb.bytes >= params_buffer_bytes(params))
			iova = data->ramb.addr;
	}

	reg = DMA_REG_BUF_STR;
	mask = ABOX_DMA_BUF_STR_MASK;
	val = iova;
	ret = snd_soc_component_update_bits_async(cmpnt, reg, mask, val);
	if (ret < 0)
		return ret;

	reg = DMA_REG_BUF_END;
	mask = ABOX_DMA_BUF_END_MASK;
	val = iova + params_buffer_bytes(params);
	ret = snd_soc_component_update_bits_async(cmpnt, reg, mask, val);
	if (ret < 0)
		return ret;

	reg = DMA_REG_BUF_OFFSET;
	mask = ABOX_DMA_BUF_OFFSET_MASK;
	val = params_period_bytes(params);
	ret = snd_soc_component_update_bits_async(cmpnt, reg, mask, val);
	if (ret < 0)
		return ret;

	reg = DMA_REG_STR_POINT;
	mask = ABOX_DMA_STR_POINT_MASK;
	val = iova;
	ret = snd_soc_component_update_bits_async(cmpnt, reg, mask, val);
	if (ret < 0)
		return ret;

	reg = DMA_REG_CTRL;
	mask = ABOX_DMA_WIDTH_MASK;
	val = ((params_width(params) / 8) - 1) << ABOX_DMA_WIDTH_L;
	ret = snd_soc_component_update_bits_async(cmpnt, reg, mask, val);
	if (ret < 0)
		return ret;

	reg = DMA_REG_CTRL;
	mask = ABOX_DMA_PACKED_MASK;
	val = (params_physical_width(params) == 24) << ABOX_DMA_PACKED_L;
	ret = snd_soc_component_update_bits_async(cmpnt, reg, mask, val);
	if (ret < 0)
		return ret;

	reg = DMA_REG_CTRL;
	mask = ABOX_DMA_CHANNELS_MASK;
	val = (params_channels(params) - 1) << ABOX_DMA_CHANNELS_L;
	ret = snd_soc_component_update_bits_async(cmpnt, reg, mask, val);
	if (ret < 0)
		return ret;

	snd_soc_component_async_complete(cmpnt);

	if (!rtd->dai_link->no_pcm)
		abox_dma_acquire_irq_all(data);

	abox_info(dev, "%s:Total=%u PrdSz=%u(%u) #Prds=%u rate=%u, width=%d, channels=%u\n",
			snd_pcm_stream_str(substream),
			params_buffer_bytes(params), params_period_size(params),
			params_period_bytes(params), params_periods(params),
			params_rate(params), params_width(params),
			params_channels(params));

	return 0;
}

static int abox_dma_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = data->dev;

	abox_dbg(dev, "%s\n", __func__);

	if (rtd->dai_link->no_pcm)
		return 0;

	abox_dma_release_irq_all(data);

	return snd_pcm_lib_free_pages(substream);
}

static int abox_dma_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = data->dev;

	abox_dbg(dev, "%s\n", __func__);

	/* set auto fade in before dma enable */
	snd_soc_component_update_bits(data->cmpnt, DMA_REG_CTRL,
			ABOX_DMA_AUTO_FADE_IN_MASK,
			data->auto_fade_in ? ABOX_DMA_AUTO_FADE_IN_MASK : 0);

	return 0;
}

static int abox_dma_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct snd_soc_component *cmpnt = data->cmpnt;
	struct device *dev = data->dev;
	unsigned int reg, mask, val;
	int ret;

	abox_info(dev, "%s(%d)\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		abox_dma_barrier(dev, data, 0);
		reg = DMA_REG_CTRL;
		mask = ABOX_DMA_ENABLE_MASK;
		ret = snd_soc_component_read(cmpnt, reg, &val);
		if (ret < 0)
			break;

		val = (val & ~mask) | ((1 << ABOX_DMA_ENABLE_L) & mask);
		ret = snd_soc_component_write(cmpnt, reg, val);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* ToDo: wait for stable state.
		 * Func shouldn't be changed during transition.
		 */
		abox_dma_barrier(dev, data, 1);
		reg = DMA_REG_CTRL;
		mask = ABOX_DMA_FUNC_MASK;
		ret = snd_soc_component_read(cmpnt, reg, &val);
		if (ret < 0)
			break;

		/* normal mode */
		val = (val & ~mask) | ((0 << ABOX_DMA_FUNC_L) & mask);
		ret = snd_soc_component_write(cmpnt, reg, val);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		abox_dma_barrier(dev, data, 1);
		reg = DMA_REG_CTRL;
		mask = ABOX_DMA_ENABLE_MASK;
		ret = snd_soc_component_read(cmpnt, reg, &val);
		if (ret < 0)
			break;

		val = (val & ~mask) | ((0 << ABOX_DMA_ENABLE_L) & mask);
		ret = snd_soc_component_write(cmpnt, reg, val);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* ToDo: wait for stable state.
		 * Func shouldn't be changed during transition.
		 */
		abox_dma_barrier(dev, data, 1);
		reg = DMA_REG_CTRL;
		mask = ABOX_DMA_FUNC_MASK;
		ret = snd_soc_component_read(cmpnt, reg, &val);
		if (ret < 0)
			break;

		/* pending mode */
		val = (val & ~mask) | ((1 << ABOX_DMA_FUNC_L) & mask);
		ret = snd_soc_component_write(cmpnt, reg, val);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static size_t abox_dma_read_pointer(struct abox_dma_data *data)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int status = 0;
	unsigned int buf_str, buf_end, buf_offset;
	size_t offset, count, period_bytes;
	ssize_t buffer_bytes;

	snd_soc_component_read(cmpnt, DMA_REG_STATUS, &status);
	snd_soc_component_read(cmpnt, DMA_REG_BUF_STR, &buf_str);
	snd_soc_component_read(cmpnt, DMA_REG_BUF_END, &buf_end);
	snd_soc_component_read(cmpnt, DMA_REG_BUF_OFFSET, &buf_offset);

	buffer_bytes = buf_end - buf_str;
	period_bytes = buf_offset;

	if (hweight_long(ABOX_DMA_BUF_OFFSET_CNT_MASK) > 8)
		offset = ((status & ABOX_DMA_BUF_OFFSET_CNT_MASK) >>
				ABOX_DMA_BUF_OFFSET_CNT_L) << 4;
	else
		offset = ((status & ABOX_DMA_BUF_OFFSET_CNT_MASK) >>
				ABOX_DMA_BUF_OFFSET_CNT_L) * period_bytes;

	if (period_bytes > ABOX_DMA_BUF_CNT_MASK + 1)
		count = 0;
	else
		count = (status & ABOX_DMA_BUF_CNT_MASK);

	while ((offset % period_bytes) && (buffer_bytes >= 0)) {
		buffer_bytes -= period_bytes;
		if ((buffer_bytes & offset) == offset)
			offset = buffer_bytes;
	}

	return offset + count;
}

static snd_pcm_uframes_t abox_dma_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = data->dev;
	size_t pointer;

	if (abox_dma_progress(data->cmpnt))
		pointer = abox_dma_read_pointer(data);
	else
		pointer = 0;

	abox_dbg(dev, "%s: pointer=%08zx\n", __func__, pointer);

	return bytes_to_frames(runtime, (ssize_t)pointer);
}

static int abox_dma_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = data->dev;
	struct abox_data *abox_data = data->abox_data;

	abox_info(dev, "%s\n", __func__);

	abox_request_cpu_gear_dai(dev, abox_data, rtd->cpu_dai,
			abox_data->cpu_gear_min);
	if (substream->runtime)
		snd_soc_set_runtime_hwparams(substream, &abox_dma_hardware);
	data->substream = substream;

	return 0;
}

static int abox_dma_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = data->dev;

	abox_info(dev, "%s\n", __func__);

	data->substream = NULL;

	return 0;
}

static int abox_dma_mmap(struct snd_pcm_substream *substream,
		struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = data->dev;

	abox_dbg(dev, "%s\n", __func__);

	return dma_mmap_wc(dev, vma, runtime->dma_area, runtime->dma_addr,
			runtime->dma_bytes);
}

const struct snd_pcm_ops abox_dma_ops = {
	.open		= abox_dma_open,
	.close		= abox_dma_close,
	.hw_params	= abox_dma_hw_params,
	.hw_free	= abox_dma_hw_free,
	.prepare	= abox_dma_prepare,
	.trigger	= abox_dma_trigger,
	.pointer	= abox_dma_pointer,
	.mmap		= abox_dma_mmap,
};

static int abox_dma_pcm_new(struct snd_soc_pcm_runtime *runtime)
{
	struct snd_pcm *pcm = runtime->pcm;
	struct snd_pcm_substream *substream =
			pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream ?
			pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream :
			pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
	struct snd_dma_buffer *dmab = &substream->dma_buffer;
	struct snd_soc_dai *dai = runtime->cpu_dai;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(dai);
	struct device *dev = data->dev;
	struct device *dev_abox = data->dev_abox;
	int ret;

	abox_dbg(dev, "%s\n", __func__);

	if (dmab->bytes < BUFFER_BYTES_MIN)
			dmab->bytes = BUFFER_BYTES_MIN;

	snd_pcm_lib_preallocate_pages(substream, SNDRV_DMA_TYPE_DEV,
			dev, dmab->bytes, dmab->bytes);

	ret = abox_iommu_map(dev_abox, abox_dma_iova(data), dmab->addr,
			dmab->bytes, dmab->area);
	if (ret < 0)
		snd_pcm_lib_preallocate_free_for_all(pcm);

	data->dmab = *dmab;

	return ret;
}

static void abox_dma_pcm_free(struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *runtime = pcm->private_data;
	struct snd_soc_dai *dai = runtime->cpu_dai;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(dai);
	struct device *dev = data->dev;
	struct device *dev_abox = data->dev_abox;

	abox_dbg(dev, "%s\n", __func__);

	abox_iommu_unmap(dev_abox, abox_dma_iova(data));
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static int abox_dma_probe(struct snd_soc_component *cmpnt)
{
	struct device *dev = cmpnt->dev;
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);

	abox_dbg(dev, "%s\n", __func__);

	data->cmpnt = cmpnt;

	return 0;
}

static void abox_dma_remove(struct snd_soc_component *cmpnt)
{
	struct device *dev = cmpnt->dev;

	abox_dbg(dev, "%s\n", __func__);
}

static unsigned int abox_dma_read(struct snd_soc_component *cmpnt,
		unsigned int base, unsigned int reg)
{
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	struct abox_data *abox_data = data->abox_data;
	unsigned int val = 0;

	if (reg > DMA_REG_MAX) {
		abox_warn(cmpnt->dev, "invalid dma register:%#x\n", reg);
		dump_stack();
	}

	snd_soc_component_read(abox_data->cmpnt, base + reg, &val);

	return val;
}

static int abox_dma_write(struct snd_soc_component *cmpnt,
		unsigned int base, unsigned int reg, unsigned int val)
{
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	struct abox_data *abox_data = data->abox_data;
	int ret;

	if (reg > DMA_REG_MAX) {
		abox_warn(cmpnt->dev, "invalid dma register:%#x\n", reg);
		dump_stack();
	}

	ret = snd_soc_component_write(abox_data->cmpnt, base + reg, val);
	if (ret < 0)
		return ret;

	return 0;
}

const struct snd_soc_component_driver abox_dma = {
	.probe			= abox_dma_probe,
	.remove			= abox_dma_remove,
	.pcm_new		= abox_dma_pcm_new,
	.pcm_free		= abox_dma_pcm_free,
	.ops			= &abox_dma_ops,
};

int abox_dma_set_dst_bit_width(struct device *dev, int width)
{
	struct abox_dma_data *data = dev_get_drvdata(dev);
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int reg, mask, val;
	int ret;

	abox_dbg(dev, "%s(%d)\n", __func__, width);

	reg = DMA_REG_BIT_CTRL;
	mask = ABOX_DMA_DST_BIT_WIDTH_MASK;
	val = ((width / 8) - 1) << ABOX_DMA_DST_BIT_WIDTH_L;
	ret = snd_soc_component_update_bits(cmpnt, reg, mask, val);

	return ret;
}

int abox_dma_get_dst_bit_width(struct device *dev)
{
	struct abox_dma_data *data = dev_get_drvdata(dev);
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int reg, val;
	int width, ret;

	abox_dbg(dev, "%s\n", __func__);

	reg = DMA_REG_BIT_CTRL;
	ret = snd_soc_component_read(cmpnt, reg, &val);
	if (ret < 0)
		return ret;
	val &= ABOX_DMA_DST_BIT_WIDTH_MASK;
	val >>= ABOX_DMA_DST_BIT_WIDTH_L;
	width = (val + 1) * 8;

	return width;
}

int abox_dma_get_channels(struct device *dev)
{
	struct abox_dma_data *data = dev_get_drvdata(dev);

	abox_dbg(dev, "%s\n", __func__);

	return params_channels(&data->hw_params);
}

static void hw_param_mask_set(struct snd_pcm_hw_params *params,
		snd_pcm_hw_param_t param, unsigned int val)
{
	struct snd_mask *mask = hw_param_mask(params, param);

	snd_mask_none(mask);
	if ((int)val >= 0)
		snd_mask_set(mask, val);
}

static void hw_param_interval_set(struct snd_pcm_hw_params *params,
		snd_pcm_hw_param_t param, unsigned int val)
{
	struct snd_interval *interval = hw_param_interval(params, param);

	snd_interval_none(interval);
	if ((int)val >= 0) {
		interval->empty = 0;
		interval->min = interval->max = val;
		interval->openmin = interval->openmax = 0;
		interval->integer = 1;
	}
}

int abox_dma_hw_params_fixup(struct device *dev,
		struct snd_pcm_hw_params *params)
{
	struct abox_dma_data *data = dev_get_drvdata(dev);
	const struct snd_pcm_hw_params *fix = &data->hw_params;
	const struct snd_mask *mask;
	const struct snd_interval *interval;
	snd_pcm_hw_param_t param;

	abox_dbg(dev, "%s:Total=%u PrdSz=%u(%u) #Prds=%u rate=%u, width=%d, channels=%u\n",
			__func__, params_buffer_bytes(fix),
			params_period_size(fix), params_period_bytes(fix),
			params_periods(fix), params_rate(fix),
			params_width(fix), params_channels(fix));

	for (param = SNDRV_PCM_HW_PARAM_FIRST_MASK; param <
			SNDRV_PCM_HW_PARAM_LAST_MASK; param++) {
		mask = hw_param_mask_c(fix, param);
		if (!snd_mask_empty(mask))
			snd_mask_copy(hw_param_mask(params, param), mask);
	}

	for (param = SNDRV_PCM_HW_PARAM_FIRST_INTERVAL; param <
			SNDRV_PCM_HW_PARAM_LAST_INTERVAL; param++) {
		interval = hw_param_interval_c(fix, param);
		if (!snd_interval_empty(interval))
			snd_interval_copy(hw_param_interval(params, param),
					interval);
	}

	abox_dbg(dev, "%s:Total=%u PrdSz=%u(%u) #Prds=%u rate=%u, width=%d, channels=%u\n",
			__func__, params_buffer_bytes(params),
			params_period_size(params), params_period_bytes(params),
			params_periods(params), params_rate(params),
			params_width(params), params_channels(params));

	return 0;
}

void abox_dma_hw_params_set(struct device *dev, unsigned int rate,
		unsigned int width, unsigned int channels,
		unsigned int period_size, unsigned int periods, bool packed)
{
	struct abox_dma_data *data = dev_get_drvdata(dev);
	struct snd_pcm_hw_params *params = &data->hw_params;
	unsigned int buffer_size, format, pwidth;

	abox_dbg(dev, "%s(%u, %u, %u, %u, %u, %d)\n", __func__, rate, width,
			channels, period_size, periods, packed);

	if (!rate)
		rate = params_rate(params);

	if (!width)
		width = params_width(params);

	switch (width) {
	case 8:
		format = SNDRV_PCM_FORMAT_S8;
		break;
	case 16:
		format = SNDRV_PCM_FORMAT_S16;
		break;
	case 24:
		if (packed)
			format = SNDRV_PCM_FORMAT_S24_3LE;
		else
			format = SNDRV_PCM_FORMAT_S24;
		break;
	case 32:
		format = SNDRV_PCM_FORMAT_S32;
		break;
	default:
		format = params_format(params);
		break;
	}
	pwidth = snd_pcm_format_physical_width(format);

	if (!channels)
		channels = params_channels(params);

	if (!period_size)
		period_size = params_period_size(params);

	if (!periods)
		periods = params_periods(params);

	buffer_size = period_size * periods;

	hw_param_mask_set(params, SNDRV_PCM_HW_PARAM_FORMAT,
			format);
	hw_param_interval_set(params, SNDRV_PCM_HW_PARAM_SAMPLE_BITS,
			pwidth);
	hw_param_interval_set(params, SNDRV_PCM_HW_PARAM_FRAME_BITS,
			pwidth * channels);
	hw_param_interval_set(params, SNDRV_PCM_HW_PARAM_CHANNELS,
			channels);
	hw_param_interval_set(params, SNDRV_PCM_HW_PARAM_RATE,
			rate);
	hw_param_interval_set(params, SNDRV_PCM_HW_PARAM_PERIOD_TIME,
			USEC_PER_SEC * period_size / rate);
	hw_param_interval_set(params, SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
			period_size);
	hw_param_interval_set(params, SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
			period_size * (pwidth / 8) * channels);
	hw_param_interval_set(params, SNDRV_PCM_HW_PARAM_PERIODS,
			periods);
	hw_param_interval_set(params, SNDRV_PCM_HW_PARAM_BUFFER_TIME,
			USEC_PER_SEC * buffer_size / rate);
	hw_param_interval_set(params, SNDRV_PCM_HW_PARAM_BUFFER_SIZE,
			buffer_size);
	hw_param_interval_set(params, SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
			buffer_size * (pwidth / 8) * channels);
}

int abox_dma_hw_params_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_dma_data *data = dev_get_drvdata(dev);
	const struct snd_pcm_hw_params *hw_params = &data->hw_params;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	enum abox_dma_param param = mc->reg;
	long value;

	abox_dbg(dev, "%s(%d)\n", __func__, param);

	switch (param) {
	case DMA_RATE:
		value = params_rate(hw_params);
		break;
	case DMA_WIDTH:
		value = params_width(hw_params);
		break;
	case DMA_CHANNEL:
		value = params_channels(hw_params);
		break;
	case DMA_PERIOD:
		value = params_period_size(hw_params);
		break;
	case DMA_PERIODS:
		value = params_periods(hw_params);
		break;
	case DMA_PACKED:
		value = (params_format(hw_params) == SNDRV_PCM_FORMAT_S24_3LE);
		break;
	default:
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = value;

	return 0;
}

int abox_dma_hw_params_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	enum abox_dma_param param = mc->reg;
	long value = ucontrol->value.integer.value[0];

	abox_dbg(dev, "%s(%d, %ld)\n", __func__, param, value);

	if (value < mc->min || value > mc->max)
		return -EINVAL;

	switch (param) {
	case DMA_RATE:
		abox_dma_hw_params_set(dev, value, 0, 0, 0, 0, 0);
		break;
	case DMA_WIDTH:
		abox_dma_hw_params_set(dev, 0, value, 0, 0, 0, 0);
		break;
	case DMA_CHANNEL:
		abox_dma_hw_params_set(dev, 0, 0, value, 0, 0, 0);
		break;
	case DMA_PERIOD:
		abox_dma_hw_params_set(dev, 0, 0, 0, value, 0, 0);
		break;
	case DMA_PERIODS:
		abox_dma_hw_params_set(dev, 0, 0, 0, 0, value, 0);
		break;
	case DMA_PACKED:
		abox_dma_hw_params_set(dev, 0, 0, 0, 0, 0, !!value);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int abox_dma_auto_fade_in_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_dma_data *data = dev_get_drvdata(dev);

	abox_dbg(dev, "%s\n", __func__);

	ucontrol->value.integer.value[0] = data->auto_fade_in;

	return 0;
}

int abox_dma_auto_fade_in_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_dma_data *data = dev_get_drvdata(dev);
	bool value = !!ucontrol->value.integer.value[0];

	abox_dbg(dev, "%s(%d)\n", __func__, value);

	data->auto_fade_in = value;

	return 0;
}

static const char * const dma_func_texts[] = {
	"Normal", "Pending", "Mute",
};
SOC_ENUM_SINGLE_DECL(abox_dma_func_enum, DMA_REG_CTRL,
		ABOX_DMA_FUNC_L, dma_func_texts);

int abox_dma_func_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_dma_data *data = dev_get_drvdata(dev);
	unsigned int *item = ucontrol->value.enumerated.item;
	int ret;

	abox_dbg(dev, "%s(%d)\n", __func__, item[0]);

	if (abox_dma_progress(cmpnt)) {
		static const unsigned int VOL_MAX = 0x800000;
		unsigned int change = 1, rate;
		unsigned long wait_ms;

		snd_soc_component_read(cmpnt, DMA_REG_VOL_CHANGE, &change);
		rate = params_rate(&data->hw_params);
		wait_ms = DIV_ROUND_UP(MSEC_PER_SEC * VOL_MAX, change);
		wait_ms = DIV_ROUND_UP(wait_ms, rate);

		reinit_completion(&data->func_changed);
		ret = snd_soc_put_enum_double(kcontrol, ucontrol);
		if (ret > 0) {
			abox_info(dev, "func %d, wait %lu ms\n", item[0], wait_ms);
			wait_for_completion_timeout(&data->func_changed,
					msecs_to_jiffies((unsigned int)wait_ms));
		}
	} else {
		ret = snd_soc_put_enum_double(kcontrol, ucontrol);
	}

	return ret;
}

struct snd_soc_dai *abox_dma_get_dai(struct device *dev, enum abox_dma_dai type)
{
	struct abox_dma_data *data = dev_get_drvdata(dev);
	struct snd_soc_component *cmpnt = data->cmpnt;
	struct snd_soc_dai *dai;

	abox_dbg(dev, "%s(%d)\n", __func__, type);

	list_for_each_entry(dai, &cmpnt->dai_list, list) {
		if (type-- == 0)
			return dai;
	}

	return ERR_PTR(-EINVAL);
}

int abox_dma_can_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int rdir = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
			SNDRV_PCM_STREAM_CAPTURE : SNDRV_PCM_STREAM_PLAYBACK;

	switch (rtd->dpcm[rdir].state) {
	case SND_SOC_DPCM_STATE_OPEN:
	case SND_SOC_DPCM_STATE_HW_PARAMS:
	case SND_SOC_DPCM_STATE_PREPARE:
	case SND_SOC_DPCM_STATE_START:
	case SND_SOC_DPCM_STATE_STOP:
	case SND_SOC_DPCM_STATE_PAUSED:
	case SND_SOC_DPCM_STATE_SUSPEND:
	case SND_SOC_DPCM_STATE_HW_FREE:
		return 0;
	default:
		return 1;
	}
}

int abox_dma_can_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int rdir = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
			SNDRV_PCM_STREAM_CAPTURE : SNDRV_PCM_STREAM_PLAYBACK;

	switch (rtd->dpcm[rdir].state) {
	case SND_SOC_DPCM_STATE_HW_PARAMS:
	case SND_SOC_DPCM_STATE_PREPARE:
	case SND_SOC_DPCM_STATE_START:
	case SND_SOC_DPCM_STATE_STOP:
	case SND_SOC_DPCM_STATE_PAUSED:
	case SND_SOC_DPCM_STATE_SUSPEND:
		return 0;
	default:
		return 1;
	}
}

int abox_dma_can_stop(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int rdir = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
			SNDRV_PCM_STREAM_CAPTURE : SNDRV_PCM_STREAM_PLAYBACK;

	switch (rtd->dpcm[rdir].state) {
	case SND_SOC_DPCM_STATE_START:
	case SND_SOC_DPCM_STATE_PAUSED:
	case SND_SOC_DPCM_STATE_SUSPEND:
		return 0;
	default:
		return 1;
	}
}

int abox_dma_can_start(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int rdir = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
			SNDRV_PCM_STREAM_CAPTURE : SNDRV_PCM_STREAM_PLAYBACK;

	switch (rtd->dpcm[rdir].state) {
	case SND_SOC_DPCM_STATE_START:
		return 0;
	default:
		return 1;
	}
}

int abox_dma_can_prepare(struct snd_pcm_substream *substream)
{
	return abox_dma_can_start(substream);
}

int abox_dma_can_params(struct snd_pcm_substream *substream)
{
	return abox_dma_can_free(substream);
}

int abox_dma_can_open(struct snd_pcm_substream *substream)
{
	return abox_dma_can_close(substream);
}

static unsigned int abox_dma_get_dst_format(struct abox_dma_data *data)
{
	unsigned int width, channels;

	width = abox_dma_get_dst_bit_width(data->dev);
	channels = params_channels(&data->hw_params);

	return abox_get_format(width, channels);
}

static unsigned int abox_dma_get_format(struct abox_dma_data *data)
{
	unsigned int width, channels;

	width = params_width(&data->hw_params);
	channels = params_channels(&data->hw_params);

	return abox_get_format(width, channels);
}

static int abox_dma_dump_set_format(struct abox_dma_data *data)
{
	struct abox_data *abox_data = data->abox_data;
	enum abox_widget w;
	unsigned int format;
	int idx, ret;

	abox_dbg(data->dev, "%s\n", __func__);

	if (!data->of_data->get_src_widget)
		return 0;

	w = data->of_data->get_src_widget(data);
	if (w < 0)
		return w;

	switch (w) {
	case ABOX_WIDGET_SPUS_IN0 ... ABOX_WIDGET_SPUS_IN11:
		idx = w - ABOX_WIDGET_SPUS_IN0;
		format = abox_dma_get_dst_format(dev_get_drvdata(
				abox_data->dev_rdma[idx]));
		break;
	case ABOX_WIDGET_SPUS_ASRC0 ... ABOX_WIDGET_SPUS_ASRC7:
		idx = w - ABOX_WIDGET_SPUS_ASRC0;
		format = abox_cmpnt_asrc_get_dst_format(abox_data,
				SNDRV_PCM_STREAM_PLAYBACK, idx);
		break;
	case ABOX_WIDGET_SIFS0:
		format = abox_cmpnt_sif_get_dst_format(abox_data,
				SNDRV_PCM_STREAM_PLAYBACK, 0);
		break;
	case ABOX_WIDGET_NSRC0 ... ABOX_WIDGET_NSRC7:
		idx = w - ABOX_WIDGET_NSRC0;
		format = abox_cmpnt_sif_get_dst_format(abox_data,
				SNDRV_PCM_STREAM_CAPTURE, idx);
		break;
	case ABOX_WIDGET_SPUM_ASRC0 ... ABOX_WIDGET_SPUM_ASRC3:
		idx = w - ABOX_WIDGET_SPUM_ASRC0;
		format = abox_cmpnt_asrc_get_dst_format(abox_data,
				SNDRV_PCM_STREAM_CAPTURE, idx);
		break;
	case ABOX_WIDGET_UDMA_RD0 ... ABOX_WIDGET_UDMA_RD1:
		idx = w - ABOX_WIDGET_UDMA_RD0;
		format = abox_dma_get_format(dev_get_drvdata(
				abox_data->dev_udma_rd[idx]));
		break;
	default:
		return -EINVAL;
	}

	ret = snd_soc_component_update_bits(data->cmpnt, DMA_REG_CTRL,
			ABOX_DMA_FORMAT_MASK, format << ABOX_DMA_FORMAT_L);
	return ret;
}

static const unsigned int ABOX_DMA_DUMP_BUFFER_SIZE = SZ_512K;
static const unsigned int ABOX_DMA_DUMP_OFFSET = SZ_4K;

static int abox_dma_dump_set_buffer(struct abox_dma_data *data)
{
	struct abox_dma_dump *dump = data->dump;
	struct device *dev = data->dev;
	struct device *dev_abox = data->abox_data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int iova = abox_dma_iova(data);
	unsigned int reg, mask, val;
	int ret;

	abox_dbg(dev, "%s\n", __func__);

	if (dump->area)
		abox_err(dev, "memory leak suspected\n");

	dump->bytes = ABOX_DMA_DUMP_BUFFER_SIZE;
	dump->area = dma_alloc_coherent(dev, dump->bytes, &dump->addr,
			GFP_KERNEL);
	if (!dump->area)
		return -ENOMEM;
	abox_iommu_unmap(dev_abox, abox_dma_iova(data));
	ret = abox_iommu_map(dev_abox, abox_dma_iova(data), dump->addr,
			dump->bytes, dump->area);

	reg = DMA_REG_BUF_STR;
	mask = ABOX_DMA_BUF_STR_MASK;
	val = iova;
	ret = snd_soc_component_update_bits_async(cmpnt, reg, mask, val);
	if (ret < 0)
		return ret;

	reg = DMA_REG_BUF_END;
	mask = ABOX_DMA_BUF_END_MASK;
	val = iova + dump->bytes;
	ret = snd_soc_component_update_bits_async(cmpnt, reg, mask, val);
	if (ret < 0)
		return ret;

	reg = DMA_REG_BUF_OFFSET;
	mask = ABOX_DMA_BUF_OFFSET_MASK;
	val = ABOX_DMA_DUMP_OFFSET;
	ret = snd_soc_component_update_bits_async(cmpnt, reg, mask, val);
	if (ret < 0)
		return ret;

	reg = DMA_REG_STR_POINT;
	mask = ABOX_DMA_STR_POINT_MASK;
	val = iova;
	ret = snd_soc_component_update_bits_async(cmpnt, reg, mask, val);
	if (ret < 0)
		return ret;

	snd_soc_component_async_complete(cmpnt);

	return ret;
}

static void abox_dma_dump_free_buffer(struct abox_dma_data *data)
{
	struct abox_dma_dump *dump = data->dump;
	struct device *dev = data->dev;
	struct device *dev_abox = data->abox_data->dev;
	struct snd_dma_buffer *dmab = &data->dmab;
	unsigned int size = ABOX_DMA_DUMP_BUFFER_SIZE;
	unsigned int iova = abox_dma_iova(data);

	abox_dbg(dev, "%s\n", __func__);

	if (dump->area) {
		abox_iommu_unmap(dev_abox, iova);
		dma_free_coherent(dev, size, dump->area, dump->addr);
		abox_iommu_map(dev_abox, iova, dmab->addr, dmab->bytes,
				dmab->area);
		dump->area = NULL;
	}
}

static bool abox_dma_dump_started(struct abox_dma_data *data)
{
	unsigned int val = 0;

	snd_soc_component_read(data->cmpnt, DMA_REG_CTRL, &val);

	return (val & ABOX_DMA_ENABLE_MASK);
}

static int abox_dma_dump_stop(struct abox_dma_data *data)
{
	abox_dbg(data->dev, "%s\n", __func__);

	snd_soc_component_update_bits(data->cmpnt, DMA_REG_CTRL,
			ABOX_DMA_ENABLE_MASK, 0 << ABOX_DMA_ENABLE_L);
	abox_dma_barrier(data->dev, data, 0);

	data->dump->updated = true;
	wake_up_interruptible(&data->dump->waitqueue);

	return 0;
}

static int abox_dma_dump_start(struct abox_dma_data *data)
{
	int ret;

	abox_dbg(data->dev, "%s\n", __func__);

	/* restart if it has started already */
	if (abox_dma_dump_started(data))
		abox_dma_dump_stop(data);

	abox_dma_acquire_irq_all(data);
	ret = abox_dma_dump_set_format(data);
	if (ret < 0)
		return ret;
	data->dump->pointer = 0;
	ret = snd_soc_component_update_bits(data->cmpnt, DMA_REG_CTRL,
			ABOX_DMA_ENABLE_MASK, 1 << ABOX_DMA_ENABLE_L);
	if (ret < 0)
		return ret;

	return 0;
}

static int abox_dma_dump_file_notify(void *priv, bool enabled)
{
	struct abox_dma_data *data = priv;
	int ret;

	abox_dbg(data->dev, "%s(%d)\n", __func__, enabled);

	if (enabled)
		ret = abox_dma_dump_start(data);
	else
		ret = abox_dma_dump_stop(data);

	return ret;
}

static int abox_dma_dump_register_event_notifier(struct abox_dma_data *data)
{
	struct abox_data *abox_data = data->abox_data;
	enum abox_widget w;

	abox_dbg(data->dev, "%s\n", __func__);

	if (!data->of_data->get_src_widget)
		return 0;

	w = data->of_data->get_src_widget(data);
	if (w < 0)
		return w;

	abox_cmpnt_register_event_notifier(abox_data, w,
			abox_dma_dump_file_notify, data);

	return 0;
}

static int abox_dma_dump_unregister_event_notifier(struct abox_dma_data *data)
{
	struct abox_data *abox_data = data->abox_data;
	enum abox_widget w;

	abox_dbg(data->dev, "%s\n", __func__);

	if (!data->of_data->get_src_widget)
		return 0;

	w = data->of_data->get_src_widget(data);
	if (w < 0)
		return w;

	abox_cmpnt_unregister_event_notifier(abox_data, w);

	return 0;
}

static ssize_t abox_dma_dump_file_read(struct file *file, char __user *buffer,
		size_t count, loff_t *ppos)
{
	struct abox_dma_data *data = file->private_data;
	struct abox_dma_dump *dump = data->dump;
	struct device *dev = data->dev;
	size_t pointer, size, total, last;
	int ret;

	do {
		pointer = abox_dma_read_pointer(data);
		if (pointer < dump->pointer)
			size = pointer + dump->bytes - dump->pointer;
		else
			size = pointer - dump->pointer;

		if (size < count) {
			if (file->f_flags & O_NONBLOCK)
				return -EAGAIN;

			if (abox_dma_progress(data->cmpnt)) {
				msleep(1);
				continue;
			}

			if (size && !abox_dma_dump_started(data))
				break;

			dump->updated = false;
			ret = wait_event_interruptible(dump->waitqueue,
					dump->updated);
			if (ret < 0)
				return ret;
		}
	} while (size < count);

	total = size = min(size, count);
	abox_dbg(dev, "%s: %#zx, %#zx, %#zx, %#zx\n", __func__, count,
			dump->pointer, pointer, size);

	if (size > dump->bytes - dump->pointer) {
		last = dump->bytes - dump->pointer;
		if (copy_to_user(buffer, dump->area + dump->pointer, last))
			return -EFAULT;
		dump->pointer = 0;
		buffer += last;
		size -= last;
	}

	if (copy_to_user(buffer, dump->area + dump->pointer, size))
		return -EFAULT;
	dump->pointer += size;
	dump->pointer %= dump->bytes;

	return total;
}

static irqreturn_t abox_dma_dump_file_buf_done(int irq, void *dev_id)
{
	struct device *dev = dev_id;
	struct abox_dma_data *data = dev_get_drvdata(dev);

	abox_dbg(dev, "%s(%d)\n", __func__, irq);

	if (data->dump) {
		data->dump->updated = true;
		wake_up_interruptible(&data->dump->waitqueue);
	}

	return IRQ_HANDLED;
}

static int abox_dma_dump_file_open(struct inode *i, struct file *f)
{
	struct abox_dma_data *data = abox_dump_get_data(f);
	struct abox_data *abox_data = data->abox_data;
	struct device *dev = data->dev;
	int ret;

	abox_dbg(dev, "%s\n", __func__);

	if (atomic_cmpxchg(&data->dump->open_state, 0, 1)) {
		abox_err(dev, "already opened\n");

		return -EBUSY;
	}

	pm_runtime_get_sync(dev);
	abox_gic_register_irq_handler(abox_data->dev_gic,
			abox_dma_get_irq(data, DMA_IRQ_BUF_DONE),
			abox_dma_dump_file_buf_done, dev);
	f->private_data = data;
	ret = abox_dma_dump_register_event_notifier(data);
	if (ret < 0)
		return ret;
	ret = abox_dma_dump_set_buffer(data);
	if (ret < 0)
		return ret;
	ret = abox_dma_dump_start(data);

	return ret;
}

static int abox_dma_dump_file_release(struct inode *i, struct file *f)
{
	struct abox_dma_data *data = f->private_data;
	struct device *dev = data->dev;
	int ret;

	abox_dbg(dev, "%s\n", __func__);

	ret = abox_dma_dump_stop(data);
	abox_dma_dump_free_buffer(data);
	abox_dma_dump_unregister_event_notifier(data);
	abox_dma_release_irq_all(data);
	abox_gic_register_irq_handler(data->abox_data->dev_gic,
			abox_dma_get_irq(data, DMA_IRQ_BUF_DONE),
			abox_dma_irq_handlers[DMA_IRQ_BUF_DONE], dev);
	pm_runtime_put(dev);
	atomic_cmpxchg(&data->dump->open_state, 1, 0);

	return ret;
}

static unsigned int abox_dma_dump_file_poll(struct file *file, poll_table *wait)
{
	struct abox_dma_data *data = file->private_data;

	abox_dbg(data->dev, "%s\n", __func__);

	poll_wait(file, &data->dump->waitqueue, wait);
	return POLLIN | POLLRDNORM;
}

static const struct file_operations abox_dma_dump_fops = {
	.llseek = generic_file_llseek,
	.read = abox_dma_dump_file_read,
	.poll = abox_dma_dump_file_poll,
	.open = abox_dma_dump_file_open,
	.release = abox_dma_dump_file_release,
	.owner = THIS_MODULE,
};

static int abox_dma_dump_pcm_new(struct snd_soc_pcm_runtime *runtime)
{
	struct snd_soc_dai *dai = runtime->cpu_dai;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(dai);
	struct device *dev = data->dev;

	data->dump = devm_kzalloc(dev, sizeof(*data->dump), GFP_KERNEL);
	if (!data->dump)
		return -ENOMEM;
	atomic_set(&data->dump->open_state, 0);
	data->dump->file = abox_dump_register_file(data->dai_drv->name, data,
			&abox_dma_dump_fops);
	init_waitqueue_head(&data->dump->waitqueue);
	return abox_dma_pcm_new(runtime);
}

static void abox_dma_dump_pcm_free(struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *runtime = pcm->private_data;
	struct snd_soc_dai *dai = runtime->cpu_dai;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(dai);
	struct device *dev = data->dev;

	abox_dbg(dev, "%s\n", __func__);

	if (!IS_ERR_OR_NULL(data->dump->file))
		abox_dump_unregister_file(data->dump->file);
	devm_kfree(dev, data->dump);
	abox_dma_pcm_free(pcm);
}

static enum abox_irq abox_ddma_get_irq(struct abox_dma_data *data,
		enum abox_dma_irq irq)
{
	unsigned int id = data->id;
	enum abox_irq ret;

	switch (irq) {
	case DMA_IRQ_BUF_FULL:
		ret = IRQ_WDMA_DBG0_BUF_FULL + id;
		break;
	case DMA_IRQ_FADE_DONE:
		ret = IRQ_WDMA_DBG0_FADE_DONE + id;
		break;
	case DMA_IRQ_ERR:
		ret = IRQ_WDMA_DBG0_ERR + id;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum abox_dai abox_ddma_get_dai_id(enum abox_dma_dai dai, int id)
{
	return ABOX_DDMA0 + id;
}

static char *abox_ddma_get_dai_name(struct device *dev, enum abox_dma_dai dai,
		int id)
{
	return devm_kasprintf(dev, GFP_KERNEL, "DBG%d", id);
}

static enum abox_widget abox_ddma_get_src_widget(struct abox_dma_data *data)
{
	enum abox_widget w;
	unsigned int val;
	int ret;

	ret = snd_soc_component_read(data->cmpnt, DMA_REG_CTRL, &val);
	if (ret < 0)
		return ret;

	val = (val & ABOX_DMA_DEBUG_SRC_MASK) >> ABOX_DMA_DEBUG_SRC_L;
	switch (val) {
	case 0x0 ... 0xb:
		w = ABOX_WIDGET_SPUS_IN0 + val - 0x0;
		break;
	case 0x10 ... 0x17:
		w = ABOX_WIDGET_SPUS_ASRC0 + val - 0x10;
		break;
	case 0x18:
		w = ABOX_WIDGET_SIFS0 + val - 0x18;
		break;
	case 0x20 ... 0x27:
		w = ABOX_WIDGET_NSRC0 + val - 0x20;
		break;
	case 0x30 ... 0x33:
		w = ABOX_WIDGET_SPUM_ASRC0 + val - 0x30;
		break;
	default:
		w = -EINVAL;
		break;
	}

	return w;
}

const static struct snd_soc_dai_driver abox_ddma_dai_drv = {
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 8,
		.rates = ABOX_SAMPLING_RATES,
		.rate_min = 8000,
		.rate_max = 384000,
		.formats = ABOX_SAMPLE_FORMATS,
	},
};

static const char * const abox_ddma_src_enum_texts[] = {
	"RDMA0", "RDMA1", "RDMA2", "RDMA3", "RDMA4", "RDMA5",
	"RDMA6", "RDMA7", "RDMA8", "RDMA9", "RDMA10", "RDMA11",
	"SPUS_ASRC0", "SPUS_ASRC1", "SPUS_ASRC2", "SPUS_ASRC3", "SPUS_ASRC4",
	"SPUS_ASRC5", "SPUS_ASRC6", "SPUS_ASRC7", "SPUS_MIXER",
	"SPUM_SIFM0", "SPUM_SIFM1", "SPUM_SIFM2", "SPUM_SIFM3",
	"SPUM_SIFM4", "SPUM_SIFM5", "SPUM_SIFM6", "SPUM_SIFM7",
	"SPUM_ASRC0", "SPUM_ASRC1", "SPUM_ASRC2", "SPUM_ASRC3",
};

static const unsigned int abox_ddma_src_enum_values[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
	0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
	0x10, 0x11, 0x12, 0x13, 0x14,
	0x15, 0x16, 0x17, 0x18,
	0x20, 0x21, 0x22, 0x23,
	0x24, 0x25, 0x26, 0x27,
	0x30, 0x31, 0x32, 0x33,
};

static const struct soc_enum abox_ddma_src_enum[] = {
	SOC_VALUE_ENUM_SINGLE(DMA_REG_CTRL, ABOX_DMA_DEBUG_SRC_L,
			ABOX_DMA_DEBUG_SRC_MASK >> ABOX_DMA_DEBUG_SRC_L,
			ARRAY_SIZE(abox_ddma_src_enum_texts),
			abox_ddma_src_enum_texts, abox_ddma_src_enum_values),
};

static const struct snd_kcontrol_new abox_ddma_controls[] = {
	SOC_ENUM("SRC", abox_ddma_src_enum),
	SOC_ENUM_EXT("Func", abox_dma_func_enum,
			snd_soc_get_enum_double, abox_dma_func_put),
	SOC_SINGLE_EXT("Auto Fade In", DMA_REG_CTRL,
			ABOX_DMA_AUTO_FADE_IN_L, 1, 0,
			abox_dma_auto_fade_in_get, abox_dma_auto_fade_in_put),
	SOC_SINGLE("Vol Factor", DMA_REG_VOL_FACTOR,
			ABOX_DMA_VOL_FACTOR_L, 0xffffff, 0),
	SOC_SINGLE("Vol Change", DMA_REG_VOL_CHANGE,
			ABOX_DMA_VOL_FACTOR_L, 0xffffff, 0),
};

static unsigned int abox_ddma_read(struct snd_soc_component *cmpnt,
		unsigned int reg)
{
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	unsigned int base = ABOX_WDMA_DEBUG_CTRL(data->id);

	return abox_dma_read(cmpnt, base, reg);
}

static int abox_ddma_write(struct snd_soc_component *cmpnt,
		unsigned int reg, unsigned int val)
{
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	unsigned int base = ABOX_WDMA_DEBUG_CTRL(data->id);

	return abox_dma_write(cmpnt, base, reg, val);
}

const static struct snd_soc_component_driver abox_ddma = {
	.controls		= abox_ddma_controls,
	.num_controls		= ARRAY_SIZE(abox_ddma_controls),
	.probe			= abox_dma_probe,
	.remove			= abox_dma_remove,
	.read			= abox_ddma_read,
	.write			= abox_ddma_write,
	.pcm_new		= abox_dma_dump_pcm_new,
	.pcm_free		= abox_dma_dump_pcm_free,
	.ops			= &abox_dma_ops,
};

static enum abox_irq abox_udma_wr_dbg_get_irq(struct abox_dma_data *data,
		enum abox_dma_irq irq)
{
	unsigned int id = data->id;
	enum abox_irq ret;

	switch (irq) {
	case DMA_IRQ_BUF_EMPTY:
		ret = IRQ_UDMA_WR_DBG0_BUF_FULL + id;
		break;
	case DMA_IRQ_ERR:
		ret = IRQ_UDMA_WR_DBG0_ERR + id;
		break;
	case DMA_IRQ_FADE_DONE:
		ret = IRQ_UDMA_WR_DBG0_FADE_DONE + id;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum abox_dai abox_udma_wr_dbg_get_dai_id(enum abox_dma_dai dai, int id)
{
	return ABOX_UDMA_WR_DBG0 + id;
}

static char *abox_udma_wr_dbg_get_dai_name(struct device *dev,
		enum abox_dma_dai dai, int id)
{
	return devm_kasprintf(dev, GFP_KERNEL, "UDMA DBG%d", id);
}

static enum abox_widget abox_udma_wr_dbg_get_src_widget(
		struct abox_dma_data *data)
{
	enum abox_widget w;
	unsigned int val;
	int ret;

	ret = snd_soc_component_read(data->cmpnt, DMA_REG_CTRL, &val);
	if (ret < 0)
		return ret;

	val = (val & ABOX_DMA_DEBUG_SRC_MASK) >> ABOX_DMA_DEBUG_SRC_L;
	switch (val) {
	case 0x20 ... 0x21:
		w = ABOX_WIDGET_UDMA_RD0 + val - 0x20;
		break;
	default:
		w = -EINVAL;
		break;
	}

	return w;
}

static const char * const abox_udma_wr_dbg_src_enum_texts[] = {
	"UDMA_RD0", "UDMA_RD1",
};

static const unsigned int abox_udma_wr_dbg_src_enum_values[] = {
	0x20, 0x21,
};

static const struct soc_enum abox_udma_wr_dbg_src_enum[] = {
	SOC_VALUE_ENUM_SINGLE(DMA_REG_CTRL, ABOX_DMA_DEBUG_SRC_L,
			ABOX_DMA_DEBUG_SRC_MASK >> ABOX_DMA_DEBUG_SRC_L,
			ARRAY_SIZE(abox_udma_wr_dbg_src_enum_texts),
			abox_udma_wr_dbg_src_enum_texts,
			abox_udma_wr_dbg_src_enum_values),
};

static const struct snd_kcontrol_new abox_udma_wr_dbg_controls[] = {
	SOC_ENUM("SRC", abox_udma_wr_dbg_src_enum),
	SOC_ENUM_EXT("Func", abox_dma_func_enum,
			snd_soc_get_enum_double, abox_dma_func_put),
	SOC_SINGLE_EXT("Auto Fade In", DMA_REG_CTRL,
			ABOX_DMA_AUTO_FADE_IN_L, 1, 0,
			abox_dma_auto_fade_in_get, abox_dma_auto_fade_in_put),
	SOC_SINGLE("Vol Factor", DMA_REG_VOL_FACTOR,
			ABOX_DMA_VOL_FACTOR_L, 0xffffff, 0),
	SOC_SINGLE("Vol Change", DMA_REG_VOL_CHANGE,
			ABOX_DMA_VOL_FACTOR_L, 0xffffff, 0),
};

static unsigned int abox_udma_wr_dbg_read(struct snd_soc_component *cmpnt,
		unsigned int reg)
{
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	unsigned int base = ABOX_UDMA_WR_DEBUG_CTRL(data->id);

	return abox_dma_read(cmpnt, base, reg);
}

static int abox_udma_wr_dbg_write(struct snd_soc_component *cmpnt,
		unsigned int reg, unsigned int val)
{
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	unsigned int base = ABOX_UDMA_WR_DEBUG_CTRL(data->id);

	return abox_dma_write(cmpnt, base, reg, val);
}

const static struct snd_soc_component_driver abox_udma_wr_dbg = {
	.controls		= abox_udma_wr_dbg_controls,
	.num_controls		= ARRAY_SIZE(abox_udma_wr_dbg_controls),
	.probe			= abox_dma_probe,
	.remove			= abox_dma_remove,
	.read			= abox_udma_wr_dbg_read,
	.write			= abox_udma_wr_dbg_write,
	.pcm_new		= abox_dma_dump_pcm_new,
	.pcm_free		= abox_dma_dump_pcm_free,
	.ops			= &abox_dma_ops,
};

static enum abox_irq abox_dual_get_irq(struct abox_dma_data *data,
		enum abox_dma_irq irq)
{
	unsigned int id = data->id;
	enum abox_irq ret;

	switch (irq) {
	case DMA_IRQ_BUF_FULL:
		ret = IRQ_WDMA0_DUAL_BUF_FULL + id;
		break;
	case DMA_IRQ_ERR:
		ret = IRQ_WDMA0_DUAL_ERR + id;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum abox_dai abox_dual_get_dai_id(enum abox_dma_dai dai, int id)
{
	return ABOX_WDMA0_DUAL + id;
}

static char *abox_dual_get_dai_name(struct device *dev, enum abox_dma_dai dai,
		int id)
{
	return devm_kasprintf(dev, GFP_KERNEL, "WDMA%d DUAL", id);
}

const static struct snd_soc_dai_driver abox_dual_dai_drv = {
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 8,
		.rates = ABOX_SAMPLING_RATES,
		.rate_min = 8000,
		.rate_max = 384000,
		.formats = ABOX_SAMPLE_FORMATS,
	},
};

static unsigned int abox_dual_read(struct snd_soc_component *cmpnt,
		unsigned int reg)
{
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	unsigned int base = ABOX_WDMA_DUAL_CTRL(data->id);

	return abox_dma_read(cmpnt, base, reg);
}

static int abox_dual_write(struct snd_soc_component *cmpnt,
		unsigned int reg, unsigned int val)
{
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	unsigned int base = ABOX_WDMA_DUAL_CTRL(data->id);

	return abox_dma_write(cmpnt, base, reg, val);
}

const static struct snd_soc_component_driver abox_dual = {
	.probe			= abox_dma_probe,
	.remove			= abox_dma_remove,
	.read			= abox_dual_read,
	.write			= abox_dual_write,
	.pcm_new		= abox_dma_dump_pcm_new,
	.pcm_free		= abox_dma_dump_pcm_free,
	.ops			= &abox_dma_ops,
};

static enum abox_irq abox_udma_wr_dual_get_irq(struct abox_dma_data *data,
		enum abox_dma_irq irq)
{
	unsigned int id = data->id;
	enum abox_irq ret;

	switch (irq) {
	case DMA_IRQ_BUF_EMPTY:
		ret = IRQ_UDMA_WR0_DUAL_BUF_FULL + id;
		break;
	case DMA_IRQ_ERR:
		ret = IRQ_UDMA_WR0_DUAL_ERR + id;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum abox_dai abox_udma_wr_dual_get_dai_id(enum abox_dma_dai dai, int id)
{
	return ABOX_UDMA_WR0_DUAL + id;
}

static char *abox_udma_wr_dual_get_dai_name(struct device *dev,
		enum abox_dma_dai dai, int id)
{
	return devm_kasprintf(dev, GFP_KERNEL, "UDMA WR%d DUAL", id);
}

static unsigned int abox_udma_wr_dual_read(struct snd_soc_component *cmpnt,
		unsigned int reg)
{
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	unsigned int base = ABOX_UDMA_WR_DUAL_CTRL(data->id);

	return abox_dma_read(cmpnt, base, reg);
}

static int abox_udma_wr_dual_write(struct snd_soc_component *cmpnt,
		unsigned int reg, unsigned int val)
{
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	unsigned int base = ABOX_UDMA_WR_DUAL_CTRL(data->id);

	return abox_dma_write(cmpnt, base, reg, val);
}

const static struct snd_soc_component_driver abox_udma_wr_dual = {
	.probe			= abox_dma_probe,
	.remove			= abox_dma_remove,
	.read			= abox_udma_wr_dual_read,
	.write			= abox_udma_wr_dual_write,
	.pcm_new		= abox_dma_dump_pcm_new,
	.pcm_free		= abox_dma_dump_pcm_free,
	.ops			= &abox_dma_ops,
};

static const struct of_device_id samsung_abox_dma_match[] = {
	{
		.compatible = "samsung,abox-ddma",
		.data = (void *)&(struct abox_dma_of_data){
			.get_irq = abox_ddma_get_irq,
			.get_dai_id = abox_ddma_get_dai_id,
			.get_dai_name = abox_ddma_get_dai_name,
			.get_src_widget = abox_ddma_get_src_widget,
			.dai_drv = &abox_ddma_dai_drv,
			.num_dai = 1,
			.cmpnt_drv = &abox_ddma,
		},
	},
	{
		.compatible = "samsung,abox-dual",
		.data = (void *)&(struct abox_dma_of_data){
			.get_irq = abox_dual_get_irq,
			.get_dai_id = abox_dual_get_dai_id,
			.get_dai_name = abox_dual_get_dai_name,
			.dai_drv = &abox_dual_dai_drv,
			.num_dai = 1,
			.cmpnt_drv = &abox_dual,
		},
	},
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_ABOX_UDMA)
	{
		.compatible = "samsung,abox-udma-rd",
		.data = &abox_udma_rd_of_data,
	},
	{
		.compatible = "samsung,abox-udma-wr",
		.data = &abox_udma_wr_of_data,
	},
	{
		.compatible = "samsung,abox-udma-wr-dual",
		.data = (void *)&(struct abox_dma_of_data){
			.get_irq = abox_udma_wr_dual_get_irq,
			.get_dai_id = abox_udma_wr_dual_get_dai_id,
			.get_dai_name = abox_udma_wr_dual_get_dai_name,
			.dai_drv = &abox_dual_dai_drv,
			.num_dai = 1,
			.cmpnt_drv = &abox_udma_wr_dual,
		},
	},
	{
		.compatible = "samsung,abox-udma-wr-debug",
		.data = (void *)&(struct abox_dma_of_data){
			.get_irq = abox_udma_wr_dbg_get_irq,
			.get_dai_id = abox_udma_wr_dbg_get_dai_id,
			.get_dai_name = abox_udma_wr_dbg_get_dai_name,
			.get_src_widget = abox_udma_wr_dbg_get_src_widget,
			.dai_drv = &abox_ddma_dai_drv,
			.num_dai = 1,
			.cmpnt_drv = &abox_udma_wr_dbg,
		},
	},
#endif
	{},
};
MODULE_DEVICE_TABLE(of, samsung_abox_dma_match);

static int samsung_abox_dma_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device *dev_abox = dev->parent;
	struct abox_data *abox_data = dev_get_drvdata(dev_abox);
	struct device *dev_gic = abox_data->dev_gic;
	struct device_node *np = dev->of_node;
	struct abox_dma_data *data;
	const struct abox_dma_of_data *of_data;
	const char *type;
	u32 value;
	int i, ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	platform_set_drvdata(pdev, data);
	data->dev = dev;
	data->dev_abox = dev_abox;
	data->abox_data = abox_data;
	init_completion(&data->func_changed);
	dma_set_mask_and_coherent(dev, DMA_BIT_MASK(36));

	data->sfr_base = devm_get_ioremap(pdev, "sfr", &data->sfr_phys, NULL);
	if (IS_ERR(data->sfr_base))
		return PTR_ERR(data->sfr_base);

	ret = of_samsung_property_read_u32(dev, np, "id", &data->id);
	if (ret < 0)
		return ret;

	ret = of_samsung_property_read_u32(dev, np, "buffer_bytes", &value);
	if (ret < 0)
		value = 0;
	data->dmab.bytes = value;

	ret = of_samsung_property_read_string(dev, np, "buffer_type", &type);
	if (ret < 0)
		type = "";
	if (!strncmp(type, "ion", sizeof("ion")))
		data->buf_type = BUFFER_TYPE_ION;
	else if (!strncmp(type, "dma", sizeof("dma")))
		data->buf_type = BUFFER_TYPE_DMA;
	else if (!strncmp(type, "ram", sizeof("ram")))
		data->buf_type = BUFFER_TYPE_RAM;
	else
		data->buf_type = BUFFER_TYPE_DMA;

	of_data = data->of_data = of_device_get_match_data(dev);

	for (i = DMA_IRQ_BUF_DONE; i < DMA_IRQ_COUNT; i++) {
		enum abox_irq irq = abox_dma_get_irq(data, i);

		if (irq < 0 || irq > IRQ_COUNT)
			continue;

		ret = abox_gic_register_irq_handler(dev_gic, irq,
				abox_dma_irq_handlers[i], dev);
		if (ret < 0)
			return ret;
	}

	data->num_dai = of_data->num_dai;
	data->dai_drv = devm_kmemdup(dev, of_data->dai_drv,
			sizeof(*of_data->dai_drv) * data->num_dai,
			GFP_KERNEL);
	if (!data->dai_drv)
		return -ENOMEM;

	for (i = 0; i < data->num_dai; i++) {
		data->dai_drv[i].id = of_data->get_dai_id(i, data->id);
		data->dai_drv[i].name = of_data->get_dai_name(dev, i, data->id);
	}

	ret = devm_snd_soc_register_component(dev, of_data->cmpnt_drv,
			data->dai_drv, data->num_dai);
	if (ret < 0)
		return ret;

	pm_runtime_no_callbacks(dev);
	pm_runtime_enable(dev);

	return 0;
}

static int samsung_abox_dma_remove(struct platform_device *pdev)
{
	return 0;
}

static void samsung_abox_dma_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	abox_dbg(dev, "%s\n", __func__);
	pm_runtime_disable(dev);
}

struct platform_driver samsung_abox_dma_driver = {
	.probe  = samsung_abox_dma_probe,
	.remove = samsung_abox_dma_remove,
	.shutdown = samsung_abox_dma_shutdown,
	.driver = {
		.name = "abox-dma",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_abox_dma_match),
	},
};
