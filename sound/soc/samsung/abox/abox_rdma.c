/* sound/soc/samsung/abox/abox_rdma.c
 *
 * ALSA SoC Audio Layer - Samsung Abox RDMA driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/regmap.h>
#include <linux/iommu.h>
#include <linux/delay.h>
#include <linux/memblock.h>
#include <linux/sched/clock.h>
#include <sound/hwdep.h>
#include <linux/miscdevice.h>
#include <linux/dma-buf.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>

#include <sound/samsung/abox.h>
#include "abox_util.h"
#include "abox_gic.h"
#include "abox_dbg.h"
#include "abox_vss.h"
#include "abox_cmpnt.h"
#include "abox.h"
#include "abox_dma.h"
#include "abox_memlog.h"

#define COMPR_USE_COPY
#define COMPR_USE_FIXED_MEMORY

/* Mailbox between driver and firmware for offload */
#define COMPR_CMD_CODE		(0x0004)
#define COMPR_HANDLE_ID		(0x0008)
#define COMPR_IP_TYPE		(0x000C)
#define COMPR_SIZE_OF_FRAGMENT	(0x0010)
#define COMPR_PHY_ADDR_INBUF	(0x0014)
#define COMPR_SIZE_OF_INBUF	(0x0018)
#define COMPR_LEFT_VOL		(0x001C)
#define COMPR_RIGHT_VOL		(0x0020)
#define EFFECT_EXT_ON		(0x0024)
#define COMPR_ALPA_NOTI		(0x0028)
#define COMPR_PARAM_RATE	(0x0034)
#define COMPR_PARAM_SAMPLE	(0x0038)
#define COMPR_PARAM_CH		(0x003C)
#define COMPR_RENDERED_PCM_SIZE	(0x004C)
#define COMPR_RETURN_CMD	(0x0040)
#define COMPR_IP_ID		(0x0044)
#define COMPR_SIZE_OUT_DATA	(0x0048)
#define COMPR_UPSCALE		(0x0050)
#define COMPR_CPU_LOCK_LV	(0x0054)
#define COMPR_CHECK_CMD		(0x0058)
#define COMPR_CHECK_RUNNING	(0x005C)
#define COMPR_ACK		(0x0060)
#define COMPR_INTR_ACK		(0x0064)
#define COMPR_INTR_DMA_ACK	(0x0068)
#define COMPR_MAX		COMPR_INTR_DMA_ACK

/* COMPR_UPSCALE */
#define COMPR_BIT_SHIFT		(0)
#define COMPR_BIT_MASK		(0xFF)
#define COMPR_CH_SHIFT		(8)
#define COMPR_CH_MASK		(0xF)
#define COMPR_RATE_SHIFT	(12)
#define COMPR_RATE_MASK		(0xFFFFF)

/* Interrupt type */
#define INTR_WAKEUP		(0x0)
#define INTR_READY		(0x1000)
#define INTR_DMA		(0x2000)
#define INTR_CREATED		(0x3000)
#define INTR_DECODED		(0x4000)
#define INTR_RENDERED		(0x5000)
#define INTR_FLUSH		(0x6000)
#define INTR_PAUSED		(0x6001)
#define INTR_EOS		(0x7000)
#define INTR_DESTROY		(0x8000)
#define INTR_FX_EXT		(0x9000)
#define INTR_EFF_REQUEST	(0xA000)
#define INTR_SET_CPU_LOCK	(0xC000)
#define INTR_FW_LOG		(0xFFFF)

#define COMPRESSED_LR_VOL_MAX_STEPS     0x2000

enum OFFLOAD_CMDTYPE {
	/* OFFLOAD */
	CMD_COMPR_CREATE = 0x50,
	CMD_COMPR_DESTROY,
	CMD_COMPR_SET_PARAM,
	CMD_COMPR_WRITE,
	CMD_COMPR_READ,
	CMD_COMPR_START,
	CMD_COMPR_STOP,
	CMD_COMPR_PAUSE,
	CMD_COMPR_EOS,
	CMD_COMPR_GET_VOLUME,
	CMD_COMPR_SET_VOLUME,
	CMD_COMPR_CA5_WAKEUP,
	CMD_COMPR_HPDET_NOTIFY,
};

enum OFFLOAD_IPTYPE {
	COMPR_MP3 = 0x0,
	COMPR_AAC = 0x1,
	COMPR_FLAC = 0x2,
};

static const struct snd_compr_caps abox_rdma_compr_caps = {
	.direction		= SND_COMPRESS_PLAYBACK,
	.min_fragment_size	= SZ_4K,
	.max_fragment_size	= SZ_64K,
	.min_fragments		= 1,
	.max_fragments		= 5,
	.num_codecs		= 3,
	.codecs			= {
		SND_AUDIOCODEC_MP3,
		SND_AUDIOCODEC_AAC,
		SND_AUDIOCODEC_FLAC
	},
};

static struct reserved_mem *abox_rdma_compr_buffer;

static int __init abox_rdma_compr_buffer_setup(struct reserved_mem *rmem)
{
	pr_info("%s: base=%pa, size=%pa\n", __func__, &rmem->base, &rmem->size);
	abox_rdma_compr_buffer = rmem;
	return 0;
}

RESERVEDMEM_OF_DECLARE(abox_rdma_compr_buffer, "exynos,abox_rdma_compr_buffer",
		abox_rdma_compr_buffer_setup);

static void abox_rdma_mailbox_write(struct device *dev, u32 index, u32 value)
{
	struct regmap *regmap = dev_get_regmap(dev, NULL);
	int ret;

	abox_dbg(dev, "%s(%#x, %#x)\n", __func__, index, value);

	if (!regmap) {
		abox_err(dev, "%s: regmap is null\n", __func__);
		return;
	}

	pm_runtime_get(dev);
	ret = regmap_write(regmap, index, value);
	if (ret < 0)
		abox_warn(dev, "%s(%#x) failed: %d\n", __func__, index, ret);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);
}

static u32 abox_rdma_mailbox_read(struct device *dev, u32 index)
{
	struct regmap *regmap = dev_get_regmap(dev, NULL);
	int ret;
	u32 val = 0;

	abox_dbg(dev, "%s(%#x)\n", __func__, index);

	if (!regmap) {
		abox_err(dev, "%s: regmap is null\n", __func__);
		return 0;
	}

	pm_runtime_get(dev);
	ret = regmap_read(regmap, index, &val);
	if (ret < 0)
		abox_warn(dev, "%s(%#x) failed: %d\n", __func__, index, ret);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return val;
}

static void abox_rdma_mailbox_update_bits(struct device *dev, u32 index,
		u32 mask, u32 value)
{
	struct regmap *regmap = dev_get_regmap(dev, NULL);
	int ret;

	abox_dbg(dev, "%s(%#x, %#x, %#x)\n", __func__, index, mask, value);

	if (!regmap) {
		abox_err(dev, "%s: regmap is null\n", __func__);
		return;
	}

	pm_runtime_get(dev);
	ret = regmap_update_bits(regmap, index, mask, value);
	if (ret < 0)
		abox_warn(dev, "%s(%#x) failed: %d\n", __func__, index, ret);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);
}

static void abox_mailbox_save(struct device *dev)
{
	struct regmap *regmap = dev_get_regmap(dev, NULL);

	if (regmap) {
		regcache_cache_only(regmap, true);
		regcache_mark_dirty(regmap);
	}
}

static void abox_mailbox_restore(struct device *dev)
{
	struct regmap *regmap = dev_get_regmap(dev, NULL);

	if (regmap) {
		regcache_cache_only(regmap, false);
		regcache_sync(regmap);
	}
}

static bool abox_mailbox_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case COMPR_ACK:
	case COMPR_INTR_ACK:
	case COMPR_INTR_DMA_ACK:
	case COMPR_RETURN_CMD:
	case COMPR_SIZE_OUT_DATA:
	case COMPR_IP_ID:
	case COMPR_RENDERED_PCM_SIZE:
		return true;
	default:
		return false;
	}
}

static bool abox_mailbox_rw_reg(struct device *dev, unsigned int reg)
{
	return true;
}

static const struct regmap_config abox_mailbox_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = COMPR_MAX,
	.volatile_reg = abox_mailbox_volatile_reg,
	.readable_reg = abox_mailbox_rw_reg,
	.writeable_reg = abox_mailbox_rw_reg,
	.cache_type = REGCACHE_FLAT,
	.fast_io = true,
};

static int abox_rdma_request_ipc(struct abox_dma_data *data,
		ABOX_IPC_MSG *msg, int atomic, int sync)
{
	return abox_request_ipc(data->dev_abox, msg->ipcid, msg, sizeof(*msg),
			atomic, sync);
}

static int abox_rdma_mailbox_send_cmd(struct device *dev, unsigned int cmd)
{
	struct abox_dma_data *dma_data = dev_get_drvdata(dev);
	struct device *dev_abox = dma_data->dev_abox;
	struct abox_compr_data *data = &dma_data->compr_data;
	ABOX_IPC_MSG msg;
	u64 timeout;
	int ret;

	abox_dbg(dev, "%s(%#x)\n", __func__, cmd);

	mutex_lock(&data->cmd_lock);

	abox_rdma_mailbox_write(dev, COMPR_HANDLE_ID, data->handle_id);
	abox_rdma_mailbox_write(dev, COMPR_CMD_CODE, cmd);
	msg.ipcid = IPC_OFFLOAD;
	ret = abox_request_ipc(dev_abox, msg.ipcid, &msg, sizeof(msg), 0, 0);

	timeout = local_clock() + abox_get_waiting_ns(true);
	while (!abox_rdma_mailbox_read(dev, COMPR_ACK)) {
		if (local_clock() <= timeout) {
			cond_resched();
			continue;
		}
		abox_err(dev, "%s(%#x): No ack error!", __func__, cmd);
		ret = -EFAULT;
		break;
	}
	/* clear ACK */
	abox_rdma_mailbox_write(dev, COMPR_ACK, 0);

	mutex_unlock(&data->cmd_lock);

	return ret;
}

static void abox_rdma_compr_clear_intr_ack(struct device *dev)
{
	abox_rdma_mailbox_write(dev, COMPR_INTR_ACK, 0);
}

static int abox_rdma_compr_isr_handler(void *priv)
{
	struct device *dev = priv;
	struct abox_dma_data *dma_data = dev_get_drvdata(dev);
	struct abox_compr_data *data = &dma_data->compr_data;
	struct snd_compr_stream *cstream;
	struct snd_compr_runtime *runtime;
	u32 val, fw_stat, size;
	u64 avail;
	unsigned long flags;

	val = abox_rdma_mailbox_read(dev, COMPR_RETURN_CMD);
	fw_stat = val >> 16;
	val &= 0xff;
	abox_dbg(dev, "%s: %#x, %#x\n", __func__, fw_stat, val);

	switch (fw_stat) {
	case INTR_CREATED:
		abox_info(dev, "INTR_CREATED\n");
		complete(&data->created);
		break;
	case INTR_DECODED:
		if (val) {
			abox_err(dev, "INTR_DECODED: err(%#x)\n", val);
			break;
		}

		if (!data->cstream || !data->cstream->runtime) {
			abox_err(dev, "INTR_DECODED: no runtime\n");
			break;
		}

		cstream = data->cstream;
		runtime = cstream->runtime;

		size = abox_rdma_mailbox_read(dev, COMPR_SIZE_OUT_DATA);
		abox_dbg(dev, "INTR_DECODED: %u\n", size);

		spin_lock_irqsave(&data->lock, flags);
		/* update copied total bytes */
		data->copied_total += size;
		data->byte_offset += size;
		if (data->byte_offset >= runtime->buffer_size)
			data->byte_offset -= runtime->buffer_size;
		spin_unlock_irqrestore(&data->lock, flags);

		snd_compr_fragment_elapsed(cstream);

		if (!data->start && runtime->state != SNDRV_PCM_STATE_PAUSED) {
			/* writes must be restarted */
			abox_err(dev, "INTR_DECODED: invalid state: %d\n",
					runtime->state);
			break;
		}

		avail = data->received_total - data->copied_total;
		abox_dbg(dev, "INTR_DECODED: free buffer: %llu\n",
				runtime->buffer_size - avail);
		if (avail < runtime->fragment_size)
			abox_dbg(dev, "INTR_DECODED: insufficient data: %llu\n",
					avail);
		break;
	case INTR_FLUSH:
		if (val) {
			abox_err(dev, "INTR_FLUSH: err(%#x)\n", val);
			break;
		}

		/* flushed */
		complete(&data->flushed);
		break;
	case INTR_PAUSED:
		if (val) {
			abox_err(dev, "INTR_PAUSED: err(%#x)\n", val);
			break;
		}
		break;
	case INTR_EOS:
		if (atomic_cmpxchg(&data->draining, 1, 0)) {
			if (data->copied_total != data->received_total)
				abox_warn(dev, "INTR_EOS: not sync(%llu/%llu)\n",
						data->copied_total,
						data->received_total);

			/* ALSA Framework callback to notify drain complete */
			snd_compr_drain_notify(data->cstream);
			abox_dbg(dev, "%s: drain notify\n", __func__);
		}
		break;
	case INTR_DESTROY:
		if (val) {
			abox_err(dev, "INTR_DESTROY: err(%#x)\n", val);
			break;
		}

		/* destroyed */
		complete(&data->destroyed);
		break;
	default:
		/* ignore */
		break;
	}

	abox_rdma_compr_clear_intr_ack(dev);

	return IRQ_HANDLED;
}

static void abox_rdma_compr_recover(struct device *dev_abox)
{
	ABOX_IPC_MSG msg;

	msg.ipcid = IPC_SYSTEM;
	msg.msg.system.msgtype = ABOX_RECOVER_OFFLOAD;
	abox_request_ipc(dev_abox, msg.ipcid, &msg, sizeof(msg), 0, 1);
}

static int abox_rdma_compr_set_param(struct device *dev,
		struct snd_compr_runtime *runtime)
{
	struct abox_dma_data *dma_data = dev_get_drvdata(dev);
	struct abox_compr_data *data = &dma_data->compr_data;
	int id = dma_data->id;
	int ret;

	abox_info(dev, "%s buffer: %llu\n", __func__, runtime->buffer_size);

#ifdef COMPR_USE_FIXED_MEMORY
	/* free memory allocated by ALSA */
	if (runtime->buffer != data->dma_area)
		kfree(runtime->buffer);

	runtime->buffer = data->dma_area;
	if (runtime->buffer_size > data->dma_size) {
		abox_err(dev, "allocated buffer size is smaller than requested(%llu > %zu)\n",
				runtime->buffer_size, data->dma_size);
		ret = -ENOMEM;
		goto error;
	}
#else
#ifdef COMPR_USE_COPY
	runtime->buffer = dma_alloc_coherent(dev, runtime->buffer_size,
			&data->dma_addr, GFP_KERNEL);
	if (!runtime->buffer) {
		abox_err(dev, "dma memory allocation failed (size=%llu)\n",
				runtime->buffer_size);
		ret = -ENOMEM;
		goto error;
	}
#else
	data->dma_addr = dma_map_single(dev, runtime->buffer,
			runtime->buffer_size, DMA_TO_DEVICE);
	ret = dma_mapping_error(dev, data->dma_addr);
	if (ret) {
		abox_err(dev, "dma memory mapping failed(%d)\n", ret);
		goto error;
	}
#endif
	ret = abox_iommu_map(dma_data->abox_data->dev,
			IOVA_COMPR_BUFFER(id), virt_to_phys(runtime->buffer),
			round_up(runtime->buffer_size, PAGE_SIZE), 0);
	if (ret < 0) {
		abox_err(dev, "iommu mapping failed(%d)\n", ret);
		goto error;
	}
#endif
	/* set buffer information at mailbox */
	abox_rdma_mailbox_write(dev, COMPR_SIZE_OF_INBUF, runtime->buffer_size);
	abox_rdma_mailbox_write(dev, COMPR_PHY_ADDR_INBUF,
			IOVA_COMPR_BUFFER(id));
	abox_rdma_mailbox_write(dev, COMPR_PARAM_SAMPLE, data->sample_rate);
	abox_rdma_mailbox_write(dev, COMPR_PARAM_CH, data->channels);
	abox_rdma_mailbox_write(dev, COMPR_IP_TYPE, data->codec_id << 16);
	ret = abox_rdma_mailbox_send_cmd(dev, CMD_COMPR_SET_PARAM);
	if (ret < 0)
		goto error;

	/* wait until the parameter is set up */
	ret = wait_for_completion_timeout(&data->created,
			abox_get_waiting_jiffies(true));
	if (!ret) {
		abox_err(dev, "CMD_COMPR_SET_PARAM time out\n");
		abox_rdma_mailbox_write(dev, COMPR_INTR_ACK, 0);
		ret = -EBUSY;
		goto error;
	}

	/* created instance */
	data->handle_id = abox_rdma_mailbox_read(dev, COMPR_IP_ID);
	abox_info(dev, "codec id=%#x, handle_id=%#x\n",
			data->codec_id, data->handle_id);
	return 0;
error:
	return ret;
}

static void __abox_rdma_compr_get_hw_params_legacy(struct device *dev,
		struct snd_pcm_hw_params *params, unsigned int upscale)
{
	struct snd_mask *format_mask;
	struct snd_interval *rate_interval;

	rate_interval = hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);
	format_mask = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);
	switch (upscale) {
	default:
		/* fallback */
	case 0:
		/* 48kHz 16bit */
		rate_interval->min = 48000;
		snd_mask_set(format_mask, SNDRV_PCM_FORMAT_S16);
		abox_info(dev, "%s: 48kHz 16bit\n", __func__);
		break;
	case 1:
		/* 192kHz 24bit */
		rate_interval->min = 192000;
		snd_mask_set(format_mask, SNDRV_PCM_FORMAT_S24);
		abox_info(dev, "%s: 192kHz 24bit\n", __func__);
		break;
	case 2:
		/* 48kHz 24bit */
		rate_interval->min = 48000;
		snd_mask_set(format_mask, SNDRV_PCM_FORMAT_S24);
		abox_info(dev, "%s: 48kHz 24bit\n", __func__);
		break;
	case 3:
		/* 384kHz 32bit */
		rate_interval->min = 384000;
		snd_mask_set(format_mask, SNDRV_PCM_FORMAT_S32);
		abox_info(dev, "%s: 384kHz 32bit\n", __func__);
		break;
	}
}

static void __abox_rdma_compr_get_hw_params(struct device *dev,
		struct snd_pcm_hw_params *params, unsigned int upscale)
{
	unsigned int bit, ch, rate;
	struct snd_mask *format_mask;
	struct snd_interval *rate_interval, *ch_interval;

	format_mask = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);
	ch_interval = hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);
	rate_interval = hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);
	bit = (upscale >> COMPR_BIT_SHIFT) & COMPR_BIT_MASK;
	ch = (upscale >> COMPR_CH_SHIFT) & COMPR_CH_MASK;
	rate = (upscale >> COMPR_RATE_SHIFT) & COMPR_RATE_MASK;

	switch (bit) {
	default:
		/* fallback */
	case 16:
		snd_mask_set(format_mask, SNDRV_PCM_FORMAT_S16);
		break;
	case 24:
		snd_mask_set(format_mask, SNDRV_PCM_FORMAT_S24);
		break;
	case 32:
		snd_mask_set(format_mask, SNDRV_PCM_FORMAT_S32);
		break;
	}
	ch_interval->min = ch ? ch : 2;
	rate_interval->min = rate ? rate : 48000;

	abox_info(dev, "%s: %ubit %uch %uHz\n", __func__, bit, ch, rate);
}

static void __abox_rdma_compr_set_hw_params_legacy(struct device *dev,
		struct snd_pcm_hw_params *params)
{
	struct abox_dma_data *pdata = dev_get_drvdata(dev);
	struct abox_compr_data *cdata = &pdata->compr_data;
	unsigned int width, rate;
	unsigned int upscale;

	rate = params_rate(params);
	width = params_width(params);

	if (rate <= 48000) {
		if (rate != 48000)
			abox_warn(dev, "unsupported offload rate: %u\n", rate);

		if (width >= 24) {
			upscale = 2;
			abox_info(dev, "%s: 48kHz 24bit\n", __func__);
			rate = 48000;
			width = 24;
		} else {
			upscale = 0;
			abox_info(dev, "%s: 48kHz 16bit\n", __func__);
			rate = 48000;
			width = 16;
		}
	} else {
		if (rate != 192000)
			abox_warn(dev, "unsupported offload rate: %u\n", rate);

		upscale = 1;
		abox_info(dev, "%s: 192kHz 24bit\n", __func__);
		rate = 192000;
		width = 24;
	}

	if (abox_rdma_mailbox_read(dev, COMPR_UPSCALE) != upscale) {
		abox_rdma_mailbox_write(dev, COMPR_UPSCALE, upscale);
		cdata->dirty = true;
		abox_dma_hw_params_set(dev, rate, width, 2, 0, 0, 0);
	}
}

static void __abox_rdma_compr_set_hw_params(struct device *dev,
		struct snd_pcm_hw_params *params)
{
	struct abox_dma_data *pdata = dev_get_drvdata(dev);
	struct abox_compr_data *cdata = &pdata->compr_data;
	unsigned int bit, ch, rate, upscale;

	bit = params_width(params);
	ch = params_channels(params);
	rate = params_rate(params);

	upscale = (bit >> COMPR_BIT_SHIFT) & COMPR_BIT_MASK;
	upscale |= (ch >> COMPR_CH_SHIFT) & COMPR_CH_MASK;
	upscale |= (rate >> COMPR_RATE_SHIFT) & COMPR_RATE_MASK;

	if (abox_rdma_mailbox_read(dev, COMPR_UPSCALE) != upscale) {
		abox_rdma_mailbox_write(dev, COMPR_UPSCALE, upscale);
		cdata->dirty = true;
		abox_dma_hw_params_set(dev, rate, bit, ch, 0, 0, 0);
	}

	abox_info(dev, "%s: %ubit %uch %uHz\n", __func__, bit, ch, rate);
}

static int abox_rdma_compr_set_hw_params(struct snd_compr_stream *stream)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct abox_dma_data *dma_data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = dma_data->dev;
	struct snd_pcm_hw_params params = {0, };
	unsigned int upscale;

	abox_dbg(dev, "%s\n", __func__);

	abox_hw_params_fixup_helper(rtd, &params);

	upscale = abox_rdma_mailbox_read(dev, COMPR_UPSCALE);
	if (upscale <= 0xF) {
		__abox_rdma_compr_set_hw_params_legacy(dev, &params);
		upscale = abox_rdma_mailbox_read(dev, COMPR_UPSCALE);
		__abox_rdma_compr_get_hw_params_legacy(dev, &params, upscale);
	} else {
		__abox_rdma_compr_set_hw_params(dev, &params);
		upscale = abox_rdma_mailbox_read(dev, COMPR_UPSCALE);
		__abox_rdma_compr_get_hw_params(dev, &params, upscale);
	}

	return 0;
}

static int abox_rdma_compr_open(struct snd_compr_stream *stream)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct abox_dma_data *dma_data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = dma_data->dev;
	struct abox_compr_data *data = &dma_data->compr_data;
	struct abox_data *abox_data = dma_data->abox_data;

	abox_info(dev, "%s\n", __func__);

	abox_wait_restored(abox_data);

	/* init runtime data */
	data->cstream = stream;
	data->byte_offset = 0;
	data->copied_total = 0;
	data->received_total = 0;
	data->sample_rate = 44100;
	data->channels = 0x3; /* stereo channel mask */
	data->start = false;
	data->bespoke_start = false;
	atomic_set(&data->draining, 0);
	reinit_completion(&data->flushed);
	reinit_completion(&data->destroyed);
	reinit_completion(&data->created);

	pm_runtime_get_sync(dev);
	abox_request_cpu_gear_dai(dev, abox_data, rtd->cpu_dai,
			abox_data->cpu_gear_min);
	abox_set_system_state(abox_data, SYSTEM_OFFLOAD, true);

	abox_rdma_compr_set_hw_params(stream);

	return 0;
}

static int abox_rdma_compr_free(struct snd_compr_stream *stream)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct abox_dma_data *dma_data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = dma_data->dev;
	struct abox_compr_data *data = &dma_data->compr_data;
	struct abox_data *abox_data = dma_data->abox_data;
	int ret = 0;

	abox_info(dev, "%s\n", __func__);

	if (atomic_cmpxchg(&data->draining, 1, 0)) {
		/* ALSA Framework callback to notify drain complete */
		snd_compr_drain_notify(stream);
		abox_dbg(dev, "%s: drain notify\n", __func__);
	}

	if (!completion_done(&data->created)) {
		ret = abox_rdma_mailbox_send_cmd(dev, CMD_COMPR_DESTROY);
		if (ret >= 0) {
			ret = wait_for_completion_timeout(&data->destroyed,
					abox_get_waiting_jiffies(true));
			if (!ret) {
				abox_err(dev, "CMD_COMPR_DESTROY time out\n");
				ret = -EBUSY;
			} else {
				ret = 0;
			}
		}
	}

#ifdef COMPR_USE_FIXED_MEMORY
	/* prevent kfree in ALSA */
	stream->runtime->buffer = NULL;
#else
{
	struct snd_compr_runtime *runtime = stream->runtime;

	abox_iommu_unmap(&abox_data->pdev->dev, IOVA_COMPR_BUFFER(id));

#ifdef COMPR_USE_COPY
	dma_free_coherent(dev, runtime->buffer_size, runtime->buffer,
			data->dma_addr);
	runtime->buffer = NULL;
#else
	dma_unmap_single(dev, data->dma_addr, runtime->buffer_size,
			DMA_TO_DEVICE);
#endif
}
#endif
	abox_set_system_state(abox_data, SYSTEM_OFFLOAD, false);
	abox_request_cpu_gear_dai(dev, abox_data, rtd->cpu_dai, 0);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put(dev);

	return ret;
}

static int abox_rdma_compr_set_params(struct snd_compr_stream *stream,
			    struct snd_compr_params *params)
{
	struct snd_compr_runtime *runtime = stream->runtime;
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct abox_dma_data *dma_data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = dma_data->dev;
	struct abox_compr_data *data = &dma_data->compr_data;
	int ret = 0;

	abox_dbg(dev, "%s\n", __func__);

	/* COMPR set_params */
	memcpy(&data->codec_param, params, sizeof(data->codec_param));

	data->byte_offset = 0;
	data->copied_total = 0;
	data->channels = data->codec_param.codec.ch_in;
	data->sample_rate = data->codec_param.codec.sample_rate;

	if (data->sample_rate == 0 ||
		data->channels == 0) {
		abox_err(dev, "%s: invalid parameters: sample(%u), ch(%u)\n",
				__func__, data->sample_rate, data->channels);
		return -EINVAL;
	}

	switch (params->codec.id) {
	case SND_AUDIOCODEC_MP3:
		data->codec_id = COMPR_MP3;
		break;
	case SND_AUDIOCODEC_AAC:
		data->codec_id = COMPR_AAC;
		break;
	case SND_AUDIOCODEC_FLAC:
		data->codec_id = COMPR_FLAC;
		break;
	default:
		abox_err(dev, "%s: unknown codec id %d\n", __func__,
				params->codec.id);
		break;
	}

	ret = abox_rdma_compr_set_param(dev, runtime);
	if (ret) {
		abox_err(dev, "%s: esa_compr_set_param fail(%d)\n", __func__,
				ret);
		abox_rdma_compr_recover(dma_data->dev_abox);
		return ret;
	}

	abox_info(dev, "%s: sample rate:%u, channels:%u\n", __func__,
			data->sample_rate, data->channels);
	return 0;
}

static int abox_rdma_compr_set_metadata(struct snd_compr_stream *stream,
			      struct snd_compr_metadata *metadata)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct abox_dma_data *dma_data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = dma_data->dev;

	abox_info(dev, "%s(%u)\n", __func__, metadata->key);

	if (metadata->key == SNDRV_COMPRESS_ENCODER_PADDING)
		abox_dbg(dev, "%s: got encoder padding %u", __func__,
				metadata->value[0]);
	else if (metadata->key == SNDRV_COMPRESS_ENCODER_DELAY)
		abox_dbg(dev, "%s: got encoder delay %u", __func__,
				metadata->value[0]);

	return 0;
}

static int abox_rdma_compr_trigger(struct snd_compr_stream *stream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct abox_dma_data *dma_data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = dma_data->dev;
	struct abox_compr_data *data = &dma_data->compr_data;
	int ret = 0;

	abox_info(dev, "%s(%d)\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		abox_dbg(dev, "SNDRV_PCM_TRIGGER_PAUSE_PUSH\n");
		ret = abox_rdma_mailbox_send_cmd(dev, CMD_COMPR_PAUSE);
		if (ret < 0)
			abox_err(dev, "%s: pause cmd failed(%d)\n", __func__,
					ret);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		abox_dbg(dev, "SNDRV_PCM_TRIGGER_STOP\n");

		if (atomic_cmpxchg(&data->draining, 1, 0)) {
			/* ALSA Framework callback to notify drain complete */
			snd_compr_drain_notify(stream);
			abox_dbg(dev, "%s: drain notify\n", __func__);
		}

		ret = abox_rdma_mailbox_send_cmd(dev, CMD_COMPR_STOP);
		if (ret < 0)
			abox_err(dev, "%s: stop cmd failed (%d)\n",
				__func__, ret);

		ret = wait_for_completion_timeout(&data->flushed,
				abox_get_waiting_jiffies(true));
		if (!ret) {
			abox_err(dev, "CMD_COMPR_STOP time out\n");
			/* Disable DMA by force */
			regmap_update_bits_base(dma_data->abox_data->regmap,
					ABOX_RDMA_CTRL(dma_data->id),
					ABOX_RDMA_ENABLE_MASK, 0, NULL,
					false, true);
			ret = -EBUSY;
		} else {
			ret = 0;
		}

		data->start = false;
		data->bespoke_start = false;

		/* reset */
		data->byte_offset = 0;
		data->copied_total = 0;
		data->received_total = 0;
		break;
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		abox_dbg(dev, "%s: %s", __func__,
				(cmd == SNDRV_PCM_TRIGGER_START) ?
				"SNDRV_PCM_TRIGGER_START" :
				"SNDRV_PCM_TRIGGER_PAUSE_RELEASE");

		data->start = true;
		data->bespoke_start = true;
		data->dirty = false;
		ret = abox_rdma_mailbox_send_cmd(dev, CMD_COMPR_START);
		if (ret < 0)
			abox_err(dev, "%s: start cmd failed\n", __func__);

		break;
	case SND_COMPR_TRIGGER_NEXT_TRACK:
		abox_dbg(dev, "%s: SND_COMPR_TRIGGER_NEXT_TRACK\n", __func__);
		break;
	case SND_COMPR_TRIGGER_PARTIAL_DRAIN:
	case SND_COMPR_TRIGGER_DRAIN:
		abox_dbg(dev, "%s: %s", __func__,
				(cmd == SND_COMPR_TRIGGER_DRAIN) ?
				"SND_COMPR_TRIGGER_DRAIN" :
				"SND_COMPR_TRIGGER_PARTIAL_DRAIN");

		if (!data->start) {
			abox_err(dev, "%s: stream wasn't started\n", __func__);
			ret = -EPERM;
			break;
		}

		atomic_set(&data->draining, 1);
		abox_dbg(dev, "%s: CMD_COMPR_EOS\n", __func__);
		ret = abox_rdma_mailbox_send_cmd(dev, CMD_COMPR_EOS);
		if (ret < 0)
			abox_err(dev, "%s: can't send eos (%d)\n", __func__,
					ret);
		break;
	default:
		break;
	}

	return 0;
}

static int abox_rdma_compr_pointer(struct snd_compr_stream *stream,
			 struct snd_compr_tstamp *tstamp)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct abox_dma_data *dma_data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = dma_data->dev;
	struct abox_compr_data *data = &dma_data->compr_data;
	unsigned int num_channel;
	u32 pcm_size;
	unsigned long flags;

	abox_dbg(dev, "%s\n", __func__);

	spin_lock_irqsave(&data->lock, flags);
	tstamp->sampling_rate = data->sample_rate;
	tstamp->byte_offset = data->byte_offset;
	tstamp->copied_total = data->copied_total;
	spin_unlock_irqrestore(&data->lock, flags);

	pcm_size = abox_rdma_mailbox_read(dev, COMPR_RENDERED_PCM_SIZE);

	/* set the number of channels */
	num_channel = hweight32(data->channels);

	if (pcm_size) {
		tstamp->pcm_io_frames = pcm_size / (2 * num_channel);
		abox_dbg(dev, "%s: pcm_size(%u), frame_count(%u), copied_total(%u)\n",
				__func__, pcm_size, tstamp->pcm_io_frames,
				tstamp->copied_total);

	}

	return 0;
}

static int abox_rdma_compr_mmap(struct snd_compr_stream *stream,
		struct vm_area_struct *vma)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct abox_dma_data *dma_data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = dma_data->dev;
	struct snd_compr_runtime *runtime = stream->runtime;

	abox_info(dev, "%s\n", __func__);

	return dma_mmap_wc(dev, vma,
			runtime->buffer,
			virt_to_phys(runtime->buffer),
			runtime->buffer_size);
}

static int abox_rdma_compr_ack(struct snd_compr_stream *stream, size_t bytes)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct abox_dma_data *dma_data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = dma_data->dev;
	struct abox_compr_data *data = &dma_data->compr_data;
	int ret;

	abox_dbg(dev, "%s\n", __func__);

	/* write mp3 data to firmware */
	data->received_total += bytes;
	abox_rdma_mailbox_write(dev, COMPR_SIZE_OF_FRAGMENT, bytes);
	ret = abox_rdma_mailbox_send_cmd(dev, CMD_COMPR_WRITE);

	return ret;
}

#ifdef COMPR_USE_COPY
static int abox_compr_write_data(struct snd_compr_stream *stream,
	       const char __user *buf, size_t count)
{
	void *dstn;
	size_t copy;
	struct snd_compr_runtime *runtime = stream->runtime;
	/* 64-bit Modulus */
	u64 app_pointer = div64_u64(runtime->total_bytes_available,
				    runtime->buffer_size);
	app_pointer = runtime->total_bytes_available -
		      (app_pointer * runtime->buffer_size);
	dstn = runtime->buffer + app_pointer;

	pr_debug("copying %ld at %lld\n",
			(unsigned long)count, app_pointer);

	if (count < runtime->buffer_size - app_pointer) {
		if (copy_from_user(dstn, buf, count))
			return -EFAULT;
	} else {
		copy = runtime->buffer_size - app_pointer;
		if (copy_from_user(dstn, buf, copy))
			return -EFAULT;
		if (copy_from_user(runtime->buffer, buf + copy, count - copy))
			return -EFAULT;
	}
	abox_rdma_compr_ack(stream, count);

	return count;
}

static int abox_rdma_compr_copy(struct snd_compr_stream *stream,
		char __user *buf, size_t count)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct abox_dma_data *dma_data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = dma_data->dev;

	abox_dbg(dev, "%s\n", __func__);

	return abox_compr_write_data(stream, buf, count);
}
#endif

static int abox_rdma_compr_get_caps(struct snd_compr_stream *stream,
		struct snd_compr_caps *caps)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct abox_dma_data *dma_data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = dma_data->dev;

	abox_info(dev, "%s\n", __func__);

	memcpy(caps, &abox_rdma_compr_caps, sizeof(*caps));

	return 0;
}

static int abox_rdma_compr_get_codec_caps(struct snd_compr_stream *stream,
		struct snd_compr_codec_caps *codec)
{
	struct snd_soc_pcm_runtime *rtd = stream->private_data;
	struct abox_dma_data *dma_data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = dma_data->dev;

	abox_info(dev, "%s\n", __func__);

	return 0;
}

static struct snd_compr_ops abox_rdma_compr_ops = {
	.open		= abox_rdma_compr_open,
	.free		= abox_rdma_compr_free,
	.set_params	= abox_rdma_compr_set_params,
	.set_metadata	= abox_rdma_compr_set_metadata,
	.trigger	= abox_rdma_compr_trigger,
	.pointer	= abox_rdma_compr_pointer,
#ifdef COMPR_USE_COPY
	.copy		= abox_rdma_compr_copy,
#endif
	.mmap		= abox_rdma_compr_mmap,
	.ack		= abox_rdma_compr_ack,
	.get_caps	= abox_rdma_compr_get_caps,
	.get_codec_caps	= abox_rdma_compr_get_codec_caps,
};

static int abox_rdma_compr_vol_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int volumes[2];

	volumes[0] = (unsigned int)ucontrol->value.integer.value[0];
	volumes[1] = (unsigned int)ucontrol->value.integer.value[1];
	abox_dbg(dev, "%s: left_vol=%d right_vol=%d\n",
			__func__, volumes[0], volumes[1]);

	abox_rdma_mailbox_write(dev, mc->reg, volumes[0]);
	abox_rdma_mailbox_write(dev, mc->rreg, volumes[1]);

	return 0;
}

static int abox_rdma_compr_vol_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int volumes[2];

	volumes[0] = abox_rdma_mailbox_read(dev, mc->reg);
	volumes[1] = abox_rdma_mailbox_read(dev, mc->rreg);
	abox_dbg(dev, "%s: left_vol=%d right_vol=%d\n",
			__func__, volumes[0], volumes[1]);

	ucontrol->value.integer.value[0] = volumes[0];
	ucontrol->value.integer.value[1] = volumes[1];

	return 0;
}

static const DECLARE_TLV_DB_LINEAR(abox_rdma_compr_vol_gain, 0,
		COMPRESSED_LR_VOL_MAX_STEPS);

static int abox_rdma_compr_format_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int upscale;

	abox_warn(dev, "%s is deprecated\n", kcontrol->id.name);

	upscale = ucontrol->value.enumerated.item[0];
	abox_dbg(dev, "%s: scale=%u\n", __func__, upscale);

	abox_rdma_mailbox_write(dev, e->reg, upscale);

	return 0;
}

static int abox_rdma_compr_format_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int upscale;

	abox_warn(dev, "%s is deprecated\n", kcontrol->id.name);

	upscale = abox_rdma_mailbox_read(dev, e->reg);
	abox_dbg(dev, "%s: upscale=%u\n", __func__, upscale);

	if (upscale >= e->items) {
		unsigned int bit, rate;

		bit = (upscale >> COMPR_BIT_SHIFT) & COMPR_BIT_MASK;
		rate = (upscale >> COMPR_RATE_SHIFT) & COMPR_RATE_MASK;

		if (rate == 384000) {
			upscale = 3;
		} else if (rate == 192000) {
			upscale = 1;
		} else if (rate == 48000) {
			if (bit == 24)
				upscale = 2;
			else
				upscale = 0;
		} else {
			upscale = 0;
		}
	}

	ucontrol->value.enumerated.item[0] = upscale;

	return 0;
}

static int abox_rdma_compr_mailbox_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	abox_dbg(dev, "%s: %u\n", __func__, val);

	abox_rdma_mailbox_update_bits(dev, mc->reg, mask << shift,
			val << shift);

	return 0;
}

static int abox_rdma_compr_mailbox_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int val = abox_rdma_mailbox_read(dev, mc->reg);

	abox_dbg(dev, "%s: %u\n", __func__, val);

	ucontrol->value.integer.value[0] = (val >> shift) & mask;

	return 0;
}

static const char * const abox_rdma_compr_format_text[] = {
	"48kHz 16bit",
	"192kHz 24bit",
	"48kHz 24bit",
	"384kHz 32bit",
};

static SOC_ENUM_SINGLE_DECL(abox_rdma_compr_format,
		COMPR_UPSCALE, 0,
		abox_rdma_compr_format_text);

static const struct snd_kcontrol_new abox_rdma_compr_controls[] = {
	SOC_DOUBLE_R_EXT_TLV("Volume", COMPR_LEFT_VOL, COMPR_RIGHT_VOL,
			0, COMPRESSED_LR_VOL_MAX_STEPS, 0,
			abox_rdma_compr_vol_get, abox_rdma_compr_vol_put,
			abox_rdma_compr_vol_gain),
	SOC_ENUM_EXT("Format", abox_rdma_compr_format,
			abox_rdma_compr_format_get, abox_rdma_compr_format_put),
	SOC_SINGLE_EXT("Bit", COMPR_UPSCALE, COMPR_BIT_SHIFT,
			COMPR_BIT_MASK, 0, abox_rdma_compr_mailbox_get,
			abox_rdma_compr_mailbox_put),
	SOC_SINGLE_EXT("Ch", COMPR_UPSCALE, COMPR_CH_SHIFT,
			COMPR_CH_MASK, 0, abox_rdma_compr_mailbox_get,
			abox_rdma_compr_mailbox_put),
	SOC_SINGLE_EXT("Rate", COMPR_UPSCALE, COMPR_RATE_SHIFT,
			COMPR_RATE_MASK, 0, abox_rdma_compr_mailbox_get,
			abox_rdma_compr_mailbox_put),
};

static const struct snd_pcm_hardware abox_rdma_hardware = {
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

static irqreturn_t abox_rdma_fade_done(int irq, void *dev_id)
{
	struct device *dev = dev_id;
	struct abox_dma_data *data = dev_get_drvdata(dev);

	abox_info(dev, "%s(%d)\n", __func__, irq);

	complete(&data->func_changed);

	return IRQ_HANDLED;
}

static irqreturn_t abox_rdma_ipc_handler(int ipc, void *dev_id,
		ABOX_IPC_MSG *msg)
{
	struct abox_data *abox_data = dev_id;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg->msg.pcmtask;
	int id = pcmtask_msg->channel_id;
	struct abox_dma_data *data;
	struct device *dev;

	if (id >= ARRAY_SIZE(abox_data->dev_rdma) || !abox_data->dev_rdma[id])
		return IRQ_NONE;

	dev = abox_data->dev_rdma[id];
	data = dev_get_drvdata(dev);

	abox_dbg(dev, "%s(%d)\n", __func__, pcmtask_msg->msgtype);

	switch (pcmtask_msg->msgtype) {
	case PCM_PLTDAI_POINTER:
		if (data->backend) {
			dev_warn_ratelimited(dev, "pointer ipc to backend\n");
			break;
		}

		data->pointer = pcmtask_msg->param.pointer;
		snd_pcm_period_elapsed(data->substream);
		break;
	case PCM_PLTDAI_ACK:
		data->ack_enabled = !!pcmtask_msg->param.trigger;
		break;
	case PCM_PLTDAI_CLOSED:
		complete(&data->closed);
		break;
	default:
		abox_warn(dev, "unknown message: %d\n", pcmtask_msg->msgtype);
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

static int abox_rdma_enabled(struct abox_dma_data *data)
{
	unsigned int val = 0;

	regmap_read(data->abox_data->regmap, ABOX_RDMA_CTRL(data->id), &val);

	return !!(val & ABOX_RDMA_ENABLE_MASK);
}

static int abox_rdma_progress(struct abox_dma_data *data)
{
	unsigned int val = 0;

	regmap_read(data->abox_data->regmap, ABOX_RDMA_STATUS(data->id), &val);

	return !!(val & ABOX_RDMA_PROGRESS_MASK);
}

static int abox_rdma_discrete(struct abox_dma_data *data)
{
	unsigned int val = 0;

	regmap_read(data->abox_data->regmap, ABOX_RDMA_CTRL(data->id), &val);

	return !!(val & ABOX_DMA_BUF_TYPE_MASK);
}

static void abox_rdma_disable_barrier(struct device *dev,
		struct abox_dma_data *data)
{
	struct abox_data *abox_data = data->abox_data;
	u64 timeout = local_clock() + abox_get_waiting_ns(true);

	while (abox_rdma_progress(data) ||
		(abox_rdma_enabled(data) && !abox_rdma_discrete(data))) {
		if (local_clock() <= timeout) {
			cond_resched();
			continue;
		}
		dev_warn_ratelimited(dev, "RDMA disable timeout\n");
		abox_dbg_dump_simple(dev, abox_data, "RDMA disable timeout");
		/* Disable DMA by force */
		regmap_update_bits_base(abox_data->regmap,
				ABOX_RDMA_CTRL(data->id),
				ABOX_RDMA_ENABLE_MASK, 0, NULL, false, true);
		break;
	}
}

static int abox_rdma_backend(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	return (rtd->cpu_dai->id >= ABOX_RDMA0_BE);
}

static int abox_rdma_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(cpu_dai);
	struct device *dev = data->dev;
	struct abox_data *abox_data = data->abox_data;
	struct device *dev_abox = abox_data->dev;
	struct snd_dma_buffer *dmab;
	int id = data->id;
	size_t buffer_bytes = params_buffer_bytes(params);
	unsigned int freq, iova;
	int sbank_size, burst_len;
	int ret;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	if (abox_rdma_backend(substream) && !abox_dma_can_params(substream)) {
		abox_info(dev, "%s skip\n", __func__);
		return 0;
	}
	abox_dbg(dev, "%s\n", __func__);

	data->hw_params = *params;

	switch (data->buf_type) {
	case BUFFER_TYPE_RAM:
		if (data->ramb.bytes >= buffer_bytes) {
			iova = data->ramb.addr;
			dmab = &data->ramb;
			break;
		}
		/* fallback to DMA mode */
		abox_info(dev, "fallback to dma buffer\n");
	case BUFFER_TYPE_DMA:
		if (data->dmab.bytes < buffer_bytes) {
			abox_iommu_unmap(dev_abox, IOVA_RDMA_BUFFER(id));
			snd_dma_free_pages(&data->dmab);
			ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV,
					dev, PAGE_ALIGN(buffer_bytes),
					&data->dmab);
			if (ret < 0)
				return ret;
			ret = abox_iommu_map(dev_abox, IOVA_RDMA_BUFFER(id),
					data->dmab.addr, data->dmab.bytes,
					data->dmab.area);
			if (ret < 0)
				return ret;
			abox_info(dev, "dma buffer changed\n");
		}
		iova = IOVA_RDMA_BUFFER(id);
		dmab = &data->dmab;
		break;
	case BUFFER_TYPE_ION:
		if (data->dmab.bytes < buffer_bytes)
			return -ENOMEM;

		abox_info(dev, "ion_buffer %s bytes(%zu) size(%zu)\n",
				__func__, buffer_bytes, data->ion_buf->size);
		iova = IOVA_RDMA_BUFFER(id);
		dmab = &data->dmab;
		break;
	default:
		abox_err(dev, "buf_type is not defined\n");
		break;
	}

	if (!abox_rdma_backend(substream)) {
		snd_pcm_set_runtime_buffer(substream, dmab);
		runtime->dma_bytes = buffer_bytes;
		abox_dma_acquire_irq(data, DMA_IRQ_FADE_DONE);
	} else {
		abox_dbg(dev, "backend dai mode\n");
	}
	data->backend = abox_rdma_backend(substream);

	sbank_size = abox_cmpnt_adjust_sbank(data->abox_data,
			cpu_dai->id, params, data->sbank_size);
	if (sbank_size > 0 && sbank_size < SZ_128)
		burst_len = 0x1;
	else
		burst_len = 0x4;

	if (abox_dma_is_sync_mode(data)) {
		burst_len = 0x1;
		iova = IOVA_WDMA_BUFFER(id);
	}

	ret = snd_soc_component_update_bits(data->cmpnt,
			DMA_REG_CTRL0, ABOX_DMA_BURST_LEN_MASK,
			burst_len << ABOX_DMA_BURST_LEN_L);
	if (ret < 0)
		abox_err(dev, "burst length write error: %d\n", ret);

	pcmtask_msg->channel_id = id;
	msg.ipcid = IPC_PCMPLAYBACK;
	msg.task_id = pcmtask_msg->channel_id = id;

	pcmtask_msg->msgtype = PCM_SET_BUFFER;
	pcmtask_msg->param.setbuff.addr = iova;
	pcmtask_msg->param.setbuff.size = params_period_bytes(params);
	pcmtask_msg->param.setbuff.count = params_periods(params);
	ret = abox_rdma_request_ipc(data, &msg, 0, 0);
	if (ret < 0)
		return ret;

	pcmtask_msg->msgtype = PCM_PLTDAI_HW_PARAMS;
	pcmtask_msg->param.hw_params.sample_rate = params_rate(params);
	pcmtask_msg->param.hw_params.bit_depth = params_width(params);
	pcmtask_msg->param.hw_params.channels = params_channels(params);
	if (params_format(params) == SNDRV_PCM_FORMAT_S24_3LE)
		pcmtask_msg->param.hw_params.packed = 1;
	else
		pcmtask_msg->param.hw_params.packed = 0;
	ret = abox_rdma_request_ipc(data, &msg, 0, 0);
	if (ret < 0)
		return ret;

	if (params_rate(params) > 48000)
		abox_request_cpu_gear_dai(dev, abox_data, cpu_dai,
				abox_data->cpu_gear_min - 1);

	freq = data->pm_qos_cl0[abox_get_rate_type(params_rate(params))];
	abox_request_cl0_freq_dai(dev, cpu_dai, freq);
	freq = data->pm_qos_cl1[abox_get_rate_type(params_rate(params))];
	abox_request_cl1_freq_dai(dev, cpu_dai, freq);
	freq = data->pm_qos_cl2[abox_get_rate_type(params_rate(params))];
	abox_request_cl2_freq_dai(dev, cpu_dai, freq);

	abox_info(dev, "%s:Total=%u PrdSz=%u(%u) #Prds=%u rate=%u, width=%d, channels=%u\n",
			snd_pcm_stream_str(substream),
			params_buffer_bytes(params), params_period_size(params),
			params_period_bytes(params), params_periods(params),
			params_rate(params), params_width(params),
			params_channels(params));

	return 0;
}

static int abox_rdma_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(cpu_dai);
	struct device *dev = data->dev;
	int id = data->id;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	if (abox_rdma_backend(substream) && !abox_dma_can_free(substream)) {
		abox_dbg(dev, "%s skip\n", __func__);
		return 0;
	}
	abox_dbg(dev, "%s\n", __func__);

	msg.ipcid = IPC_PCMPLAYBACK;
	pcmtask_msg->msgtype = PCM_PLTDAI_HW_FREE;
	msg.task_id = pcmtask_msg->channel_id = id;
	abox_rdma_request_ipc(data, &msg, 0, 0);
	abox_request_cl0_freq_dai(dev, cpu_dai, 0);
	abox_request_cl1_freq_dai(dev, cpu_dai, 0);
	abox_request_cl2_freq_dai(dev, cpu_dai, 0);

	if (!abox_rdma_backend(substream)) {
		abox_dma_release_irq(data, DMA_IRQ_FADE_DONE);
		snd_pcm_set_runtime_buffer(substream, NULL);
	}

	return 0;
}

static int abox_rdma_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = data->dev;
	int id = data->id;
	int ret;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	if (abox_rdma_backend(substream) && !abox_dma_can_prepare(substream)) {
		abox_dbg(dev, "%s skip\n", __func__);
		return 0;
	}
	abox_dbg(dev, "%s\n", __func__);

	ret = abox_cmpnt_sifsm_prepare(dev, data->abox_data, data->dai_drv->id);
	if (ret < 0)
		return ret;

	data->pointer = IOVA_RDMA_BUFFER(id);

	/* set auto fade in before dma enable */
	snd_soc_component_update_bits(data->cmpnt, DMA_REG_CTRL,
			ABOX_DMA_AUTO_FADE_IN_MASK,
			data->auto_fade_in ? ABOX_DMA_AUTO_FADE_IN_MASK : 0);

	msg.ipcid = IPC_PCMPLAYBACK;
	pcmtask_msg->msgtype = PCM_PLTDAI_PREPARE;
	msg.task_id = pcmtask_msg->channel_id = id;
	ret = abox_rdma_request_ipc(data, &msg, 0, 0);

	return ret;
}

static int abox_rdma_trigger_ipc(struct abox_dma_data *data, bool atomic,
		bool start)
{
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	if (data->enabled == start)
		return 0;

	data->enabled = start;

	msg.ipcid = IPC_PCMPLAYBACK;
	pcmtask_msg->msgtype = PCM_PLTDAI_TRIGGER;
	msg.task_id = pcmtask_msg->channel_id = data->id;
	pcmtask_msg->param.trigger = start;
	return abox_rdma_request_ipc(data, &msg, atomic, 0);
}

static int abox_rdma_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = data->dev;
	int ret;

	/* use mute_stream callback instead, if the stream is backend */
	if (abox_rdma_backend(substream))
		return 0;

	abox_info(dev, "%s(%d)\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		ret = abox_rdma_trigger_ipc(data, true, true);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		ret = abox_rdma_trigger_ipc(data, true, false);
		if (!completion_done(&data->func_changed))
			complete(&data->func_changed);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static snd_pcm_uframes_t abox_rdma_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = data->dev;
	int id = data->id;
	ssize_t pointer;
	unsigned int status = 0;
	bool progress;

	regmap_read(data->abox_data->regmap, ABOX_RDMA_STATUS(id), &status);
	progress = !!(status & ABOX_RDMA_PROGRESS_MASK);

	if (data->pointer >= IOVA_RDMA_BUFFER(id)) {
		pointer = data->pointer - IOVA_RDMA_BUFFER(id);
	} else if (((data->type == PLATFORM_NORMAL) ||
			(data->type == PLATFORM_SYNC)) && progress) {
		ssize_t offset, count;
		ssize_t buffer_bytes, period_bytes;

		buffer_bytes = snd_pcm_lib_buffer_bytes(substream);
		period_bytes = snd_pcm_lib_period_bytes(substream);

		if (hweight_long(ABOX_RDMA_RBUF_OFFSET_MASK) > 8)
			offset = ((status & ABOX_RDMA_RBUF_OFFSET_MASK) >>
					ABOX_RDMA_RBUF_OFFSET_L) << 4;
		else
			offset = ((status & ABOX_RDMA_RBUF_OFFSET_MASK) >>
					ABOX_RDMA_RBUF_OFFSET_L) * period_bytes;

		if (period_bytes > ABOX_RDMA_RBUF_CNT_MASK + 1)
			count = 0;
		else
			count = (status & ABOX_RDMA_RBUF_CNT_MASK);

		while ((offset % period_bytes) && (buffer_bytes >= 0)) {
			buffer_bytes -= period_bytes;
			if ((buffer_bytes & offset) == offset)
				offset = buffer_bytes;
		}

		pointer = offset + count;
	} else {
		pointer = 0;
	}

	abox_dbg(dev, "%s: pointer=%08zx\n", __func__, pointer);

	return bytes_to_frames(runtime, pointer);
}

static int abox_rdma_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = data->dev;
	struct abox_data *abox_data = data->abox_data;
	int id = data->id;
	int ret;
	long time;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	if (abox_rdma_backend(substream) && !abox_dma_can_open(substream)) {
		abox_info(dev, "%s skip\n", __func__);
		return 0;
	}
	abox_info(dev, "%s\n", __func__);

	abox_wait_restored(abox_data);

	if (data->closing) {
		data->closing = false;
		/* complete close before new open */
		time = wait_for_completion_timeout(&data->closed,
				abox_get_waiting_jiffies(true));
		if (time == 0)
			abox_warn(dev, "close timeout\n");
	}

	if (data->type == PLATFORM_CALL) {
		if (abox_cpu_gear_idle(dev, ABOX_CPU_GEAR_CALL_VSS))
			abox_request_cpu_gear_sync(dev, abox_data,
					ABOX_CPU_GEAR_CALL_KERNEL,
					ABOX_CPU_GEAR_MAX, rtd->cpu_dai->name);
		ret = abox_vss_notify_call(dev, abox_data, 1);
		if (ret < 0)
			abox_warn(dev, "call notify failed: %d\n", ret);
	}
	abox_request_cpu_gear_dai(dev, abox_data, rtd->cpu_dai,
			abox_data->cpu_gear_min);

	if (substream->runtime)
		snd_soc_set_runtime_hwparams(substream, &abox_rdma_hardware);

	data->substream = substream;

	msg.ipcid = IPC_PCMPLAYBACK;
	pcmtask_msg->msgtype = PCM_PLTDAI_OPEN;
	msg.task_id = pcmtask_msg->channel_id = id;
	ret = abox_rdma_request_ipc(data, &msg, 0, 0);

	return ret;
}

static int abox_rdma_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = data->dev;
	struct abox_data *abox_data = data->abox_data;
	int id = data->id;
	int ret;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;

	if (abox_rdma_backend(substream) && !abox_dma_can_close(substream)) {
		abox_info(dev, "%s skip\n", __func__);
		return 0;
	}
	abox_info(dev, "%s\n", __func__);

	abox_rdma_disable_barrier(dev, data);

	data->substream = NULL;
	data->closing = true;

	msg.ipcid = IPC_PCMPLAYBACK;
	pcmtask_msg->msgtype = PCM_PLTDAI_CLOSE;
	msg.task_id = pcmtask_msg->channel_id = id;
	ret = abox_rdma_request_ipc(data, &msg, 0, 0);

	abox_request_cpu_gear_dai(dev, abox_data, rtd->cpu_dai, 0);
	if (data->type == PLATFORM_CALL) {
		abox_request_cpu_gear(dev, abox_data, ABOX_CPU_GEAR_CALL_KERNEL,
				ABOX_CPU_GEAR_MIN, rtd->cpu_dai->name);
		ret = abox_vss_notify_call(dev, abox_data, 0);
		if (ret < 0)
			abox_warn(dev, "call notify failed: %d\n", ret);
	}

	/* Release ASRC to reuse it in other DMA */
	abox_cmpnt_asrc_release(abox_data, SNDRV_PCM_STREAM_PLAYBACK, id);

	return ret;
}

static int abox_rdma_mmap(struct snd_pcm_substream *substream,
		struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = data->dev;
	struct abox_data *abox_data = data->abox_data;

	abox_info(dev, "%s\n", __func__);

	/* Increased cpu gear for sound camp.
	 * Only sound camp uses mmap now.
	 */
	abox_request_cpu_gear_dai(dev, abox_data, rtd->cpu_dai,
			abox_data->cpu_gear_min - 1);

	if (data->buf_type == BUFFER_TYPE_ION)
		return dma_buf_mmap(data->ion_buf->dma_buf, vma, 0);
	else
		return dma_mmap_wc(dev, vma,
				runtime->dma_area,
				runtime->dma_addr,
				runtime->dma_bytes);
}

static int abox_rdma_copy_user(struct snd_pcm_substream *substream, int channel,
		unsigned long pos, void __user *buf, unsigned long bytes)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(rtd->cpu_dai);
	struct device *dev = data->dev;
	int id = data->id;
	ABOX_IPC_MSG msg;
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg.msg.pcmtask;
	unsigned long appl_bytes = (pos + bytes) % runtime->dma_bytes;
	unsigned long start;
	void *ptr;
	int ret = 0;

	start = pos + channel * (runtime->dma_bytes / runtime->channels);

	ptr = runtime->dma_area + start;
	if (copy_from_user(ptr, buf, bytes))
		ret = -EFAULT;

	if (!data->ack_enabled)
		return ret;

	abox_dbg(dev, "%s: %ld\n", __func__, appl_bytes);

	msg.ipcid = IPC_PCMPLAYBACK;
	pcmtask_msg->msgtype = PCM_PLTDAI_ACK;
	pcmtask_msg->param.pointer = (unsigned int)appl_bytes;
	msg.task_id = pcmtask_msg->channel_id = id;

	return abox_rdma_request_ipc(data, &msg, 1, 0);
}

static struct snd_pcm_ops abox_rdma_ops = {
	.open		= abox_rdma_open,
	.close		= abox_rdma_close,
	.hw_params	= abox_rdma_hw_params,
	.hw_free	= abox_rdma_hw_free,
	.prepare	= abox_rdma_prepare,
	.trigger	= abox_rdma_trigger,
	.pointer	= abox_rdma_pointer,
	.copy_user	= abox_rdma_copy_user,
	.mmap		= abox_rdma_mmap,
};

static int abox_rdma_fio_ioctl(struct snd_hwdep *hw, struct file *file,
		unsigned int cmd, unsigned long _arg);

#ifdef CONFIG_COMPAT
static int abox_rdma_fio_compat_ioctl(struct snd_hwdep *hw,
		struct file *file,
		unsigned int cmd, unsigned long _arg);
#endif

static int abox_pcm_add_hwdep_dev(struct snd_soc_pcm_runtime *runtime,
		struct abox_dma_data *data)
{
	struct snd_hwdep *hwdep;
	int rc;
	char id[] = "ABOX_MMAP_FD_NN";

	snprintf(id, sizeof(id), "ABOX_MMAP_FD_%d", SNDRV_PCM_STREAM_PLAYBACK);
	pr_debug("%s: pcm dev %d\n", __func__, runtime->pcm->device);
	rc = snd_hwdep_new(runtime->card->snd_card,
			   &id[0],
			   0 + runtime->pcm->device,
			   &hwdep);
	if (!hwdep || rc < 0) {
		pr_err("%s: hwdep intf failed to create %s - hwdep\n", __func__,
		       id);
		return rc;
	}

	hwdep->iface = 0;
	hwdep->private_data = data;
	hwdep->ops.ioctl = abox_rdma_fio_ioctl;
	hwdep->ops.ioctl_compat = abox_rdma_fio_compat_ioctl;
	data->hwdep = hwdep;

	return 0;
}

static int abox_rdma_pcm_new(struct snd_soc_pcm_runtime *runtime)
{
	struct snd_soc_dai *dai = runtime->cpu_dai;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(dai);
	struct device *dev = data->dev;
	struct device *dev_abox = data->abox_data->dev;
	int id = data->id;
	size_t buffer_bytes = data->dmab.bytes;
	int ret;

	switch (data->buf_type) {
	case BUFFER_TYPE_ION:
		buffer_bytes = BUFFER_ION_BYTES_MAX;
		data->ion_buf = abox_ion_alloc(dev, data->abox_data,
				IOVA_RDMA_BUFFER(id), buffer_bytes, true);
		if (!IS_ERR(data->ion_buf)) {
			/* update buffer infomation using ion allocated buffer  */
			data->dmab.area = data->ion_buf->kva;
			data->dmab.addr = data->ion_buf->iova;
			data->dmab.bytes  = data->ion_buf->size;

			ret = abox_pcm_add_hwdep_dev(runtime, data);
			if (ret < 0) {
				abox_err(dev, "failed to add hwdep: %d\n", ret);
				return ret;
			}
			break;
		}
		abox_warn(dev, "fallback to dma alloc\n");
		data->buf_type = BUFFER_TYPE_DMA;
	case BUFFER_TYPE_DMA:
		if (buffer_bytes < BUFFER_BYTES_MIN)
			buffer_bytes = BUFFER_BYTES_MIN;

		ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, dev,
				buffer_bytes, &data->dmab);
		if (ret < 0)
			return ret;

		ret = abox_iommu_map(dev_abox, IOVA_RDMA_BUFFER(id),
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

static void abox_rdma_pcm_free(struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *runtime = pcm->private_data;
	struct snd_soc_dai *dai = runtime->cpu_dai;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(dai);
	struct device *dev = data->dev;
	struct device *dev_abox = data->abox_data->dev;
	int id = data->id;
	int ret = 0;

	switch (data->buf_type) {
	case BUFFER_TYPE_ION:
		ret = abox_ion_free(dev, data->abox_data, data->ion_buf);
		if (ret < 0)
			abox_err(dev, "abox_ion_free() failed %d\n", ret);

		if (data->hwdep) {
			snd_device_free(runtime->card->snd_card, data->hwdep);
			data->hwdep = NULL;
		}
		break;
	case BUFFER_TYPE_DMA:
		abox_iommu_unmap(dev_abox, IOVA_RDMA_BUFFER(id));
		snd_dma_free_pages(&data->dmab);
		break;
	default:
		/* nothing to do */
		break;
	}
}

static int abox_rdma_compr_probe(struct snd_soc_component *cmpnt)
{
	struct device *dev = cmpnt->dev;
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	struct abox_compr_data *compr_data = &data->compr_data;
	struct device *dev_abox = data->abox_data->dev;
	int ret;

#ifdef COMPR_USE_FIXED_MEMORY
	if (!abox_rdma_compr_buffer) {
		struct device_node *np_tmp;

		np_tmp = of_parse_phandle(dev->of_node, "memory-region", 0);
		if (np_tmp)
			abox_rdma_compr_buffer = of_reserved_mem_lookup(np_tmp);
	}
	if (abox_rdma_compr_buffer) {
		compr_data->dma_size = abox_rdma_compr_buffer->size;
		compr_data->dma_addr = abox_rdma_compr_buffer->base;
		compr_data->dma_area = rmem_vmap(abox_rdma_compr_buffer);
	} else {
		compr_data->dma_size = abox_rdma_compr_caps.max_fragments *
				abox_rdma_compr_caps.max_fragment_size;
		compr_data->dma_area = dmam_alloc_coherent(dev,
				compr_data->dma_size, &compr_data->dma_addr,
				GFP_KERNEL);
	}
	if (compr_data->dma_area == NULL) {
		abox_err(dev, "dma memory allocation failed: %lu\n",
				PTR_ERR(compr_data->dma_area));
		return -ENOMEM;
	}
	ret = abox_iommu_map(dev_abox, IOVA_COMPR_BUFFER(data->id),
			compr_data->dma_addr,
			PAGE_ALIGN(compr_data->dma_size),
			compr_data->dma_area);
	if (ret < 0) {
		abox_err(dev, "dma memory iommu map failed: %d\n", ret);
		return ret;
	}
#endif
	return 0;
}

static int abox_rdma_compr_remove(struct snd_soc_component *cmpnt)
{
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	struct device *dev_abox = data->abox_data->dev;

	abox_iommu_unmap(dev_abox, IOVA_COMPR_BUFFER(data->id));

	return 0;
}

static const char * const dither_width_texts[] = {
	"32bit", "64bit", "128bit", "256bit",
};
static SOC_ENUM_SINGLE_DECL(dither_width_enum, DMA_REG_BIT_CTRL,
		ABOX_DMA_DITHER_WIDTH_L, dither_width_texts);
static const struct snd_kcontrol_new abox_rdma_bit_controls_3_1_0[] = {
	SOC_SINGLE("Dither On", DMA_REG_BIT_CTRL,
			ABOX_DMA_DITHER_ON_L, 1, 0),
	ABOX_DMA_SINGLE_S("Dither Strength", DMA_REG_BIT_CTRL,
			ABOX_DMA_DITHER_STRENGTH_L, 16, 6, 0),
	SOC_ENUM("Dither Width", dither_width_enum),
};

static const char * const dither_type_texts[] = {
	"OFF", "RPDF", "TPDF",
};
static SOC_ENUM_SINGLE_DECL(dither_type_enum, DMA_REG_BIT_CTRL,
		ABOX_DMA_DITHER_TYPE_L, dither_type_texts);
static const struct snd_kcontrol_new abox_rdma_bit_controls_3_1_1[] = {
	SOC_ENUM("Dither Type", dither_type_enum),
};

static int abox_rdma_probe(struct snd_soc_component *cmpnt)
{
	struct device *dev = cmpnt->dev;
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	u32 id;
	int ret;

	abox_dbg(dev, "%s\n", __func__);

	if (ABOX_SOC_VERSION(3, 1, 0) < CONFIG_SND_SOC_SAMSUNG_ABOX_VERSION)
		snd_soc_add_component_controls(cmpnt,
				abox_rdma_bit_controls_3_1_1,
				ARRAY_SIZE(abox_rdma_bit_controls_3_1_1));
	else
		snd_soc_add_component_controls(cmpnt,
				abox_rdma_bit_controls_3_1_0,
				ARRAY_SIZE(abox_rdma_bit_controls_3_1_0));

	data->cmpnt = cmpnt;
	abox_cmpnt_register_rdma(data->abox_data->dev, dev, data->id,
			data->dai_drv[DMA_DAI_PCM].name);

	ret = of_samsung_property_read_u32(dev, dev->of_node, "asrc-id", &id);
	if (ret >= 0) {
		ret = abox_cmpnt_asrc_lock(data->abox_data,
				SNDRV_PCM_STREAM_PLAYBACK, data->id, id);
		if (ret < 0)
			abox_err(dev, "asrc id lock failed\n");
		else
			abox_info(dev, "asrc id locked: %u\n", id);
	}

	if (data->type == PLATFORM_COMPRESS)
		abox_rdma_compr_probe(cmpnt);

	return 0;
}

static void abox_rdma_remove(struct snd_soc_component *cmpnt)
{
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	struct device *dev = cmpnt->dev;

	abox_info(dev, "%s\n", __func__);

	if (data->type == PLATFORM_COMPRESS)
		abox_rdma_compr_remove(cmpnt);
}

static unsigned int abox_rdma_read(struct snd_soc_component *cmpnt,
		unsigned int reg)
{
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	struct abox_data *abox_data = data->abox_data;
	unsigned int id = data->id;
	unsigned int base = ABOX_RDMA_CTRL0(id);
	unsigned int val = 0;

	if (reg > DMA_REG_STATUS) {
		abox_warn(cmpnt->dev, "invalid dma register:%#x\n", reg);
		dump_stack();
	}

	/* CTRL register is shared with firmware */
	if (reg == DMA_REG_CTRL) {
		if (pm_runtime_get_if_in_use(cmpnt->dev) > 0) {
			regmap_read(abox_data->regmap, base + reg, &val);
			pm_runtime_put(cmpnt->dev);
		} else {
			val = data->c_reg_ctrl;
		}
	} else {
		regmap_read(abox_data->regmap, base + reg, &val);
	}

	return val;
}

static int abox_rdma_write(struct snd_soc_component *cmpnt,
		unsigned int reg, unsigned int val)
{
	struct abox_dma_data *data = snd_soc_component_get_drvdata(cmpnt);
	struct abox_data *abox_data = data->abox_data;
	unsigned int id = data->id;
	unsigned int base = ABOX_RDMA_CTRL0(id);
	int ret = 0;

	if (reg > DMA_REG_STATUS) {
		abox_warn(cmpnt->dev, "invalid dma register:%#x\n", reg);
		dump_stack();
	}

	/* CTRL register is shared with firmware */
	if (reg == DMA_REG_CTRL) {
		data->c_reg_ctrl &= ~REG_CTRL_KERNEL_MASK;
		data->c_reg_ctrl |= val & REG_CTRL_KERNEL_MASK;
		if (pm_runtime_get_if_in_use(cmpnt->dev) > 0) {
			ret = regmap_update_bits(abox_data->regmap, base + reg,
					REG_CTRL_KERNEL_MASK, val);
			pm_runtime_put(cmpnt->dev);
		}
	} else {
		ret = regmap_write(abox_data->regmap, base + reg, val);
	}

	return ret;
}

static int abox_rdma_sbank_size_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_dma_data *data = dev_get_drvdata(dev);

	abox_dbg(dev, "%s\n", __func__);

	ucontrol->value.integer.value[0] = data->sbank_size;

	return 0;
}

static int abox_rdma_sbank_size_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_dma_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	long value = ucontrol->value.integer.value[0];

	abox_dbg(dev, "%s(%ld)\n", __func__, value);

	if (value < mc->min || value > mc->max)
		return -EINVAL;

	data->sbank_size = (unsigned int)value;

	return 0;
}

static const struct snd_kcontrol_new abox_rdma_controls[] = {
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
	SOC_SINGLE("Dummy Start", DMA_REG_CTRL0, ABOX_DMA_DUMMY_START_L, 1, 0),
	SOC_ENUM_EXT("Func", abox_dma_func_enum,
			snd_soc_get_enum_double, abox_dma_func_put),
	SOC_SINGLE_EXT("Auto Fade In", DMA_REG_CTRL0,
			ABOX_DMA_AUTO_FADE_IN_L, 1, 0,
			abox_dma_auto_fade_in_get, abox_dma_auto_fade_in_put),
	SOC_SINGLE("Vol Factor", DMA_REG_VOL_FACTOR,
			ABOX_DMA_VOL_FACTOR_L, 0xffffff, 0),
	SOC_SINGLE("Vol Change", DMA_REG_VOL_CHANGE,
			ABOX_DMA_VOL_FACTOR_L, 0xffffff, 0),
	ABOX_DMA_SINGLE_S("Dither Seed", DMA_REG_DITHER_SEED,
			ABOX_DMA_DITHER_SEED_L, INT_MAX, 31, 0),
	SOC_SINGLE("Sync Mode", DMA_REG_CTRL0, ABOX_DMA_SYNC_MODE_L, 1, 0),
	SOC_SINGLE_EXT("Sbank Size", 0, 0, SZ_512, 0, abox_rdma_sbank_size_get,
			abox_rdma_sbank_size_put),
};

static const struct snd_soc_component_driver abox_rdma_compr = {
	.controls	= abox_rdma_compr_controls,
	.num_controls	= ARRAY_SIZE(abox_rdma_compr_controls),
	.probe		= abox_rdma_probe,
	.remove		= abox_rdma_remove,
	.read		= abox_rdma_read,
	.write		= abox_rdma_write,
	.pcm_new	= abox_rdma_pcm_new,
	.pcm_free	= abox_rdma_pcm_free,
	.ops		= &abox_rdma_ops,
	.compr_ops	= &abox_rdma_compr_ops,
};

static const struct snd_soc_component_driver abox_rdma = {
	.controls	= abox_rdma_controls,
	.num_controls	= ARRAY_SIZE(abox_rdma_controls),
	.probe		= abox_rdma_probe,
	.remove		= abox_rdma_remove,
	.read		= abox_rdma_read,
	.write		= abox_rdma_write,
	.pcm_new	= abox_rdma_pcm_new,
	.pcm_free	= abox_rdma_pcm_free,
	.ops		= &abox_rdma_ops,
	.compr_ops	= &abox_rdma_compr_ops,
};

static int abox_rdma_mute_stream(struct snd_soc_dai *dai, int mute, int stream)
{
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(dai);
	struct device *dev = dai->dev;

	if (mute) {
		if (!abox_dma_can_stop(data->substream))
			return 0;
	} else {
		if (!abox_dma_can_start(data->substream))
			return 0;
	}

	abox_info(dev, "%s(%d)\n", __func__, mute);

	return abox_rdma_trigger_ipc(data, false, !mute);
}

static const struct snd_soc_dai_ops abox_rdma_be_dai_ops = {
	.mute_stream	= abox_rdma_mute_stream,
};

static const struct snd_soc_dai_driver abox_rdma_dai_drv[] = {
	{
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
	{
		.ops = &abox_rdma_be_dai_ops,
		.playback = {
			.stream_name = "BE Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.capture = {
			.stream_name = "BE Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.symmetric_samplebits = 1,

	},
};

static enum abox_irq abox_rdma_get_irq(struct abox_dma_data *data,
		enum abox_dma_irq irq)
{
	unsigned int id = data->id;
	enum abox_irq ret;

	switch (irq) {
	case DMA_IRQ_BUF_EMPTY:
		ret = IRQ_RDMA0_BUF_EMPTY + id;
		break;
	case DMA_IRQ_FADE_DONE:
		ret = IRQ_RDMA0_FADE_DONE + id;
		break;
	case DMA_IRQ_ERR:
		ret = IRQ_RDMA0_ERR + id;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum abox_dai abox_rdma_get_dai_id(enum abox_dma_dai dai, int id)
{
	enum abox_dai ret;

	switch (dai) {
	case DMA_DAI_PCM:
		ret = ABOX_RDMA0 + id;
		ret = (ret <= ABOX_RDMA11) ? ret : -EINVAL;
		break;
	case DMA_DAI_BE:
		ret = ABOX_RDMA0_BE + id;
		ret = (ret <= ABOX_RDMA11_BE) ? ret : -EINVAL;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static char *abox_rdma_get_dai_name(struct device *dev, enum abox_dma_dai dai,
		int id)
{
	char *ret;

	switch (dai) {
	case DMA_DAI_PCM:
		ret = devm_kasprintf(dev, GFP_KERNEL, "RDMA%d", id);
		break;
	case DMA_DAI_BE:
		ret = devm_kasprintf(dev, GFP_KERNEL, "RDMA%d BE", id);
		break;
	default:
		ret = ERR_PTR(-EINVAL);
		break;
	}

	return ret;
}

static char *abox_rdma_compr_get_dai_name(struct device *dev,
		enum abox_dma_dai dai, int id)
{
	char *ret;

	switch (dai) {
	case DMA_DAI_PCM:
		ret = devm_kasprintf(dev, GFP_KERNEL, "ComprTx0");
		break;
	case DMA_DAI_BE:
		ret = devm_kasprintf(dev, GFP_KERNEL, "ComprTx0 BE");
		break;
	default:
		ret = ERR_PTR(-EINVAL);
		break;
	}

	return ret;
}

static const struct of_device_id samsung_abox_rdma_match[] = {
	{
		.compatible = "samsung,abox-rdma",
		.data = (void *)&(struct abox_dma_of_data){
			.get_irq = abox_rdma_get_irq,
			.get_dai_id = abox_rdma_get_dai_id,
			.get_dai_name = abox_rdma_get_dai_name,
			.dai_drv = abox_rdma_dai_drv,
			.num_dai = ARRAY_SIZE(abox_rdma_dai_drv),
			.cmpnt_drv = &abox_rdma,
		},
	},
	{
		.compatible = "samsung,abox-rdma-compr",
		.data = (void *)&(struct abox_dma_of_data){
			.get_dai_id = abox_rdma_get_dai_id,
			.get_dai_name = abox_rdma_compr_get_dai_name,
			.dai_drv = abox_rdma_dai_drv,
			.num_dai = ARRAY_SIZE(abox_rdma_dai_drv),
			.cmpnt_drv = &abox_rdma_compr,
		},
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_abox_rdma_match);

static int abox_rdma_fio_common_ioctl(struct snd_hwdep *hw, struct file *filp,
		unsigned int cmd, unsigned long __user *_arg)
{
	struct abox_dma_data *data = hw->private_data;
	struct device *dev = data ? data->dev : NULL;
	struct snd_pcm_mmap_fd mmap_fd;
	int ret = 0;
	unsigned long arg;

	if (!data || (((cmd >> 8) & 0xff) != 'U'))
		return -ENOTTY;

	if (get_user(arg, _arg))
		return -EFAULT;

	abox_dbg(dev, "%s: ioctl(%#x)\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_IOCTL_MMAP_DATA_FD:
		ret = abox_ion_get_mmap_fd(dev, data->ion_buf, &mmap_fd);
		if (ret < 0) {
			abox_err(dev, "%s MMAP_FD failed: %d\n", __func__, ret);
			return ret;
		}

		if (copy_to_user(_arg, &mmap_fd, sizeof(mmap_fd)))
			return -EFAULT;
		break;
	default:
		abox_err(dev, "unknown ioctl = %#x\n", cmd);
		return -EINVAL;
	}

	return 0;
}

static int abox_rdma_fio_ioctl(struct snd_hwdep *hw, struct file *file,
		unsigned int cmd, unsigned long _arg)
{
	return abox_rdma_fio_common_ioctl(hw, file,
			cmd, (unsigned long __user *)_arg);
}

#ifdef CONFIG_COMPAT
static int abox_rdma_fio_compat_ioctl(struct snd_hwdep *hw,
		struct file *file,
		unsigned int cmd, unsigned long _arg)
{
	return abox_rdma_fio_common_ioctl(hw, file, cmd, compat_ptr(_arg));
}
#endif /* CONFIG_COMPAT */

static int abox_rdma_runtime_suspend(struct device *dev)
{
	struct abox_dma_data *data = dev_get_drvdata(dev);

	abox_dbg(dev, "%s\n", __func__);

	if (data->mailbox)
		abox_mailbox_save(dev);

	return 0;
}

static int abox_rdma_runtime_resume(struct device *dev)
{
	struct abox_dma_data *data = dev_get_drvdata(dev);

	abox_dbg(dev, "%s\n", __func__);

	regmap_update_bits(data->abox_data->regmap, ABOX_RDMA_CTRL0(data->id),
			REG_CTRL_KERNEL_MASK, data->c_reg_ctrl);

	if (data->mailbox)
		abox_mailbox_restore(dev);

	return 0;
}

static int samsung_abox_rdma_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct abox_dma_data *data;
	const struct abox_dma_of_data *of_data;
	int i, ret;
	u32 value;
	const char *type;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	platform_set_drvdata(pdev, data);
	data->dev = dev;
	dma_set_mask_and_coherent(dev, DMA_BIT_MASK(36));

	data->sfr_base = devm_get_ioremap(pdev, "sfr", NULL, NULL);
	if (IS_ERR(data->sfr_base))
		return PTR_ERR(data->sfr_base);

	data->dev_abox = pdev->dev.parent;
	if (!data->dev_abox) {
		abox_err(dev, "Failed to get abox device\n");
		return -EPROBE_DEFER;
	}
	data->abox_data = dev_get_drvdata(data->dev_abox);

	spin_lock_init(&data->compr_data.lock);
	mutex_init(&data->compr_data.cmd_lock);
	init_completion(&data->compr_data.flushed);
	init_completion(&data->compr_data.destroyed);
	init_completion(&data->compr_data.created);
	init_completion(&data->closed);
	init_completion(&data->func_changed);
	data->compr_data.isr_handler = abox_rdma_compr_isr_handler;

	abox_register_ipc_handler(data->dev_abox, IPC_PCMPLAYBACK,
			abox_rdma_ipc_handler, data->abox_data);

	ret = of_samsung_property_read_u32(dev, np, "id", &data->id);
	if (ret < 0)
		return ret;

	ret = of_samsung_property_read_string(dev, np, "type", &type);
	if (ret < 0)
		type = "";
	if (!strncmp(type, "call", sizeof("call")))
		data->type = PLATFORM_CALL;
	else if (!strncmp(type, "compress", sizeof("compress")))
		data->type = PLATFORM_COMPRESS;
	else if (!strncmp(type, "realtime", sizeof("realtime")))
		data->type = PLATFORM_REALTIME;
	else if (!strncmp(type, "vi-sensing", sizeof("vi-sensing")))
		data->type = PLATFORM_VI_SENSING;
	else if (!strncmp(type, "sync", sizeof("sync")))
		data->type = PLATFORM_SYNC;
	else
		data->type = PLATFORM_NORMAL;

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

	of_samsung_property_read_u32_array(dev, np, "pm-qos-lit",
			data->pm_qos_cl0, ARRAY_SIZE(data->pm_qos_cl0));
	ret = of_samsung_property_read_u32_array(dev, np, "pm-qos-mid",
			data->pm_qos_cl1, ARRAY_SIZE(data->pm_qos_cl1));
	if (ret < 0)
		of_samsung_property_read_u32_array(dev, np, "pm-qos-big",
			data->pm_qos_cl1, ARRAY_SIZE(data->pm_qos_cl1));
	else
		of_samsung_property_read_u32_array(dev, np, "pm-qos-big",
			data->pm_qos_cl2, ARRAY_SIZE(data->pm_qos_cl2));

	of_data = data->of_data = of_device_get_match_data(dev);
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

	if (data->type == PLATFORM_COMPRESS) {
		data->dai_drv->compress_new = snd_soc_new_compress;
		data->mailbox_base = devm_get_ioremap(pdev, "mailbox",
				NULL, NULL);
		if (IS_ERR(data->mailbox_base))
			return PTR_ERR(data->mailbox_base);

		data->mailbox = devm_regmap_init_mmio(dev,
				data->mailbox_base,
				&abox_mailbox_config);
		if (IS_ERR(data->mailbox))
			return PTR_ERR(data->mailbox);

		pm_runtime_set_autosuspend_delay(dev, 1);
		pm_runtime_use_autosuspend(dev);
	}
	pm_runtime_enable(dev);

	ret = devm_snd_soc_register_component(dev, of_data->cmpnt_drv,
			data->dai_drv, data->num_dai);
	if (ret < 0)
		return ret;

	abox_dma_register_irq(data, DMA_IRQ_FADE_DONE,
			abox_rdma_fade_done, dev);

	data->hwdep = NULL;

	return 0;
}

static int samsung_abox_rdma_remove(struct platform_device *pdev)
{
	return 0;
}

static void samsung_abox_rdma_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	abox_dbg(dev, "%s\n", __func__);
	pm_runtime_disable(dev);
}

static const struct dev_pm_ops samsung_abox_rdma_pm = {
	SET_RUNTIME_PM_OPS(abox_rdma_runtime_suspend,
			abox_rdma_runtime_resume, NULL)
};

struct platform_driver samsung_abox_rdma_driver = {
	.probe  = samsung_abox_rdma_probe,
	.remove = samsung_abox_rdma_remove,
	.shutdown = samsung_abox_rdma_shutdown,
	.driver = {
		.name = "abox-rdma",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_abox_rdma_match),
		.pm = &samsung_abox_rdma_pm,
	},
};
