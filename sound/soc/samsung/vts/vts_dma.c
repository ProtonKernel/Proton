// SPDX-License-Identifier: GPL-2.0-or-later
/* sound/soc/samsung/vts/vts-plat.c
 *
 * ALSA SoC - Samsung VTS platform driver
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
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/pm_runtime.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/pm_wakeup.h>

#include <asm-generic/delay.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>

#include <sound/samsung/mailbox.h>
#include <sound/samsung/vts.h>
#include <soc/samsung/exynos-pmu-if.h>

#include "vts.h"
#include "vts_res.h"
#include "vts_dbg.h"

static const struct snd_pcm_hardware vts_platform_hardware = {
	.info			= SNDRV_PCM_INFO_INTERLEAVED
				| SNDRV_PCM_INFO_BLOCK_TRANSFER
				| SNDRV_PCM_INFO_MMAP
				| SNDRV_PCM_INFO_MMAP_VALID,
	.formats		= SNDRV_PCM_FMTBIT_S16,
	.rates			= SNDRV_PCM_RATE_16000,
	.channels_min		= 1,
	.channels_max		= 2,
	.buffer_bytes_max	= BUFFER_BYTES_MAX,
	.period_bytes_min	= PERIOD_BYTES_MIN,
	.period_bytes_max	= PERIOD_BYTES_MAX,
	.periods_min		= BUFFER_BYTES_MAX / PERIOD_BYTES_MAX,
	.periods_max		= BUFFER_BYTES_MAX / PERIOD_BYTES_MIN,
};

static struct vts_platform_data *vts_get_drvdata(
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct snd_soc_dai_driver *dai_drv = dai->driver;
	struct vts_data *data =
		dev_get_drvdata(dai->dev);
	struct vts_platform_data *platform_data =
		platform_get_drvdata(data->pdev_vtsdma[dai_drv->id]);

	return platform_data;
}

static int vts_platform_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct device *dev = dai->dev;
	struct vts_platform_data *data = vts_get_drvdata(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	vts_dev_info(dev, "%s state %d %d\n", __func__,
			data->vts_data->vts_state,
			data->vts_data->clk_path);

	if (data->type == PLATFORM_VTS_TRIGGER_RECORD)
		snd_pcm_set_runtime_buffer(
			substream, &data->vts_data->dmab);
	else
		snd_pcm_set_runtime_buffer(
			substream, &data->vts_data->dmab_rec);

	vts_dev_info(dev, "%s:%s:DmaAddr=%pad Total=%zu"
		" PrdSz=%u(%u) #Prds=%u dma_area=%p\n",
		__func__, snd_pcm_stream_str(substream),
		&runtime->dma_addr,	runtime->dma_bytes,
		params_period_size(params),
		params_period_bytes(params),
		params_periods(params), runtime->dma_area);

	data->pointer = 0;

	return 0;
}

static int vts_platform_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct device *dev = dai->dev;

	vts_dev_dbg(dev, "%s\n", __func__);

	return 0;
}

static int vts_platform_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct device *dev = dai->dev;

	vts_dev_info(dev, "%s\n", __func__);

	return 0;
}

static void vts_platform_mif_control(struct device *dev, struct vts_data *vts_data, int on)
{
	int mif_status;
	int mif_req_out;
	int mif_req_out_status;
	int mif_ack_in;
	int mif_on_cnt = 0;

	if (on) {
		mif_req_out = 0x1;
		mif_ack_in = 0x0;
	} else {
		mif_req_out = 0x0;
		mif_ack_in = 0x10;
	}
	mif_status = readl(vts_data->sfr_base + VTS_MIF_REQ_ACK_IN);
	mif_req_out_status = readl(vts_data->sfr_base + VTS_MIF_REQ_OUT);
	vts_dev_info(dev, "%s MIF STATUS : 0x%x (0x%x)\n", __func__, mif_status, mif_req_out_status);
	if ((mif_status & (0x1 << 4)) == mif_ack_in) {
		if (on && (mif_req_out_status == 0x1)) {
			vts_dev_info(dev, "%s Wrong REQ OUT STATUS -> Toggle\n", __func__);
			writel(0x0, vts_data->sfr_base + VTS_MIF_REQ_OUT);
			mdelay(5);
		}
		writel(mif_req_out, vts_data->sfr_base + VTS_MIF_REQ_OUT);
		mif_req_out_status = readl(vts_data->sfr_base + VTS_MIF_REQ_OUT);
		vts_dev_info(dev, "%s MIF %d - 0x%x\n", __func__, on, mif_req_out_status);
		while (((mif_status & (0x1 << 4)) == mif_ack_in) && mif_on_cnt <= 480) {
			mif_on_cnt++;
			udelay(50);
			mif_status = readl(vts_data->sfr_base +
				VTS_MIF_REQ_ACK_IN);
		}
	} else {
		vts_dev_info(dev, "%s MIF already %d\n", __func__, on);
	}
	vts_dev_info(dev, "%s MIF STATUS OUT : 0x%x cnt : %d\n", __func__, mif_status, mif_on_cnt);
}

static int vts_platform_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct device *dev = dai->dev;
	struct vts_platform_data *data = vts_get_drvdata(substream);
	u32 values[3] = {0, 0, 0};
	int result = 0;

	vts_dev_info(dev, "%s ++ CMD: %d\n", __func__, cmd);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (IS_ENABLED(CONFIG_SOC_EXYNOS2100))
			vts_platform_mif_control(dev, data->vts_data, 1);

		if (data->type == PLATFORM_VTS_TRIGGER_RECORD) {
			vts_dev_dbg(dev, "%s VTS_IRQ_AP_START_COPY\n",
				__func__);
			result = vts_start_ipc_transaction(dev, data->vts_data,
				VTS_IRQ_AP_START_COPY, &values, 1, 1);
			data->vts_data->vts_tri_state = VTS_TRI_STATE_COPY_START;
		} else {
			vts_dev_dbg(dev, "%s VTS_IRQ_AP_START_REC\n", __func__);
			result = vts_start_ipc_transaction(dev, data->vts_data,
				VTS_IRQ_AP_START_REC, &values, 1, 1);
			data->vts_data->vts_rec_state = VTS_REC_STATE_START;

		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (data->type == PLATFORM_VTS_TRIGGER_RECORD) {
			vts_dev_dbg(dev, "%s VTS_IRQ_AP_STOP_COPY\n", __func__);
			result = vts_start_ipc_transaction(dev, data->vts_data,
				VTS_IRQ_AP_STOP_COPY, &values, 1, 1);
			data->vts_data->vts_tri_state = VTS_TRI_STATE_COPY_STOP;
		} else {
			vts_dev_dbg(dev, "%s VTS_IRQ_AP_STOP_REC\n", __func__);
			result = vts_start_ipc_transaction(dev, data->vts_data,
				VTS_IRQ_AP_STOP_REC, &values, 1, 1);
			data->vts_data->vts_rec_state = VTS_REC_STATE_STOP;
		}
		break;
	default:
		result = -EINVAL;
		break;
	}

	vts_dev_info(dev, "%s -- CMD: %d\n", __func__, cmd);
	return result;
}

static snd_pcm_uframes_t vts_platform_pointer(
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct device *dev = dai->dev;
	struct vts_platform_data *data = vts_get_drvdata(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	vts_dev_dbg(dev, "%s: pointer=%08x\n", __func__, data->pointer);

	return bytes_to_frames(runtime, data->pointer);
}

static int vts_platform_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct device *dev = dai->dev;
	struct vts_platform_data *data = vts_get_drvdata(substream);
	int result = 0;

	vts_dev_info(dev, "%s\n", __func__);

	if (data->vts_data->voicecall_enabled) {
		vts_dev_warn(dev, "%s VTS SRAM is Used for CP call\n",
					__func__);
		return -EBUSY;
	}

	/* vts_try_request_firmware_interface(data->vts_data); */
	pm_runtime_get_sync(dev);
	vts_start_runtime_resume(dev, 0);

	snd_soc_set_runtime_hwparams(substream, &vts_platform_hardware);

	/* Serial LIF Worked -> VTS (Trigger/Recoding) */
	if (data->vts_data->clk_path == VTS_CLK_SRC_AUD0) {
		data->vts_data->syssel_rate = 4;
		vts_clk_set_rate(&data->vts_data->pdev->dev,
			data->vts_data->syssel_rate);
		vts_dev_info(dev, "[Serial LIF Worked] Set VTS 3072000Hz\n");
	}

	if (data->type == PLATFORM_VTS_NORMAL_RECORD) {
		vts_dev_info(dev, "%s open --\n", __func__);
		vts_dev_info(dev, "SOC down : %d, MIF down : %d",
			readl(data->vts_data->sicd_base + SICD_SOC_DOWN_OFFSET),
			readl(data->vts_data->sicd_base + SICD_MIF_DOWN_OFFSET)
			);
		result = vts_set_dmicctrl(data->vts_data->pdev,
					VTS_MICCONF_FOR_RECORD, true);
		if (result < 0) {
			vts_dev_err(dev, "%s: MIC control configuration failed\n",
				__func__);
			pm_runtime_put_sync(dev);
		}
	}

	return result;
}

static int vts_platform_close(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct device *dev = dai->dev;
	struct vts_platform_data *data = vts_get_drvdata(substream);
	int result = 0;

	vts_dev_info(dev, "%s\n", __func__);

	if (data->vts_data->voicecall_enabled) {
		vts_dev_warn(dev, "%s VTS SRAM is Used for CP call\n",
					__func__);
		pm_runtime_get_sync(dev);
		vts_start_runtime_resume(dev, 0);
		/* vts_try_request_firmware_interface(data->vts_data); */
		return -EBUSY;
	}

	if (data->type == PLATFORM_VTS_NORMAL_RECORD) {
		vts_dev_info(dev, "%s close --\n", __func__);
		vts_dev_info(dev, "SOC down : %d, MIF down : %d",
			readl(data->vts_data->sicd_base + SICD_SOC_DOWN_OFFSET),
			readl(data->vts_data->sicd_base + SICD_MIF_DOWN_OFFSET)
			);
		result = vts_set_dmicctrl(data->vts_data->pdev,
					VTS_MICCONF_FOR_RECORD, false);
		if (result < 0)
			vts_dev_warn(dev, "%s: MIC control configuration failed\n",
				__func__);
	}

	pm_runtime_put_sync(dev);
	return result;
}

static int vts_platform_mmap(struct snd_pcm_substream *substream,
		struct vm_area_struct *vma)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct device *dev = dai->dev;
	struct snd_pcm_runtime *runtime = substream->runtime;

	vts_dev_info(dev, "%s\n", __func__);

	return dma_mmap_wc(dev, vma,
			runtime->dma_area,
			runtime->dma_addr,
			runtime->dma_bytes);
}

static int vts_platform_copy_user(struct snd_pcm_substream *substream,
	int channel, unsigned long pos, void __user *buf, unsigned long bytes)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	void *hwbuf;

	hwbuf = runtime->dma_area + pos +
		channel * (runtime->dma_bytes / runtime->channels);
	if (copy_to_user((void __user *)buf, hwbuf, bytes))
		return -EFAULT;

	return 0;
}

static struct snd_pcm_ops vts_platform_ops = {
	.open		= vts_platform_open,
	.close		= vts_platform_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= vts_platform_hw_params,
	.hw_free	= vts_platform_hw_free,
	.prepare	= vts_platform_prepare,
	.trigger	= vts_platform_trigger,
	.pointer	= vts_platform_pointer,
	.copy_user	= vts_platform_copy_user,
	.mmap		= vts_platform_mmap,
};

static int vts_platform_new(struct snd_soc_pcm_runtime *runtime)
{
	struct snd_soc_dai *dai = runtime->cpu_dai;
	struct snd_soc_dai_driver *dai_drv = dai->driver;
	struct device *dev = dai->dev;

	struct vts_data *vtsdata = dev_get_drvdata(dai->dev);
	struct vts_platform_data *data =
		platform_get_drvdata(vtsdata->pdev_vtsdma[dai_drv->id]);
	struct snd_pcm_substream *substream =
		runtime->pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;

	vts_dev_info(dev, "%s\n", __func__);
	data->substream = substream;
	vts_dev_info(dev, "%s Update Soc Card from runtime!!\n", __func__);
	data->vts_data->card = runtime->card;

	return 0;
}

static void vts_platform_free(struct snd_pcm *pcm)
{
}

static const struct snd_soc_component_driver vts_dma = {
	.ops		= &vts_platform_ops,
	.pcm_new	= vts_platform_new,
	.pcm_free	= vts_platform_free,
};

static struct snd_soc_dai_driver vts_dma_dai_drv = {
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16,
		.sig_bits = 16,
	},
};

static int samsung_vts_dma_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *np_vts;
	struct vts_platform_data *data;
	int result;
	int ret = 0;
	const char *type;

	dev_info(dev, "%s\n", __func__);
	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, data);
	data->dev = dev;

	np_vts = of_parse_phandle(np, "vts", 0);
	if (!np_vts) {
		dev_err(dev, "Failed to get vts device node\n");
		return -EPROBE_DEFER;
	}
	data->pdev_vts = of_find_device_by_node(np_vts);
	if (!data->pdev_vts) {
		dev_err(dev, "Failed to get vts platform device\n");
		return -EPROBE_DEFER;
	}
	data->vts_data = platform_get_drvdata(data->pdev_vts);

	result = of_property_read_u32_index(np, "id", 0, &data->id);
	if (result < 0) {
		dev_err(dev, "id property reading fail\n");
		return result;
	}

	result = of_property_read_string(np, "type", &type);
	if (result < 0) {
		dev_err(dev, "type property reading fail\n");
		return result;
	}

	if (!strncmp(type, "vts-record", sizeof("vts-record"))) {
		data->type = PLATFORM_VTS_NORMAL_RECORD;
		dev_info(dev, "%s - vts-record Probed\n", __func__);
	} else {
		data->type = PLATFORM_VTS_TRIGGER_RECORD;
		dev_info(dev, "%s - vts-trigger-record Probed\n", __func__);
	}

	vts_register_dma(data->vts_data->pdev, pdev, data->id);

	ret = devm_snd_soc_register_component(dev, &vts_dma,
						&vts_dma_dai_drv, 1);
	if (ret < 0) {
		dev_err(dev, "devm_snd_soc_register_component Fail");
		return ret;
	}
	dev_info(dev, "Probed successfully\n");
	return ret;
}

static int samsung_vts_dma_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id samsung_vts_dma_match[] = {
	{
		.compatible = "samsung,vts-dma",
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_vts_dma_match);

struct platform_driver samsung_vts_dma_driver = {
	.probe  = samsung_vts_dma_probe,
	.remove = samsung_vts_dma_remove,
	.driver = {
		.name = "samsung-vts-dma",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_vts_dma_match),
	},
};
