/* sound/soc/samsung/abox/abox_uaif.c
 *
 * ALSA SoC Audio Layer - Samsung Abox UAIF driver
 *
 * Copyright (c) 2017 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/pinctrl/consumer.h>

#include <sound/pcm_params.h>
#include <sound/samsung/abox.h>

#include "abox_util.h"
#include "abox.h"
#include "abox_cmpnt.h"
#include "abox_if.h"
#include "abox_memlog.h"

/* Predefined rate of MUX_CLK_AUD_UAIF for slave mode */
#define RATE_MUX_SLAVE 100000000

enum abox_if_quirk {
	ABOX_IF_QUIRK_DELAYED_STOP,
	ABOX_IF_QUIRK_COUNT,
};

static const char * const abox_if_quirk_str[ABOX_IF_QUIRK_COUNT] = {
	"delayed stop",
};

static unsigned long abox_if_probe_quirks(struct device_node *np)
{
	int i, res;
	unsigned long ret = 0;

	for (i = 0; i < ABOX_IF_QUIRK_COUNT; i++) {
		res = of_property_match_string(np, "samsung,quirks",
				abox_if_quirk_str[i]);
		if (res >= 0)
			ret |= BIT(i);
	}

	return ret;
}

static bool abox_if_test_quirk(unsigned long quirks, enum abox_if_quirk quirk)
{
	return !!(quirks & BIT(quirk));
}

static int abox_if_disable_qchannel(struct abox_if_data *data, int disable)
{
	struct device *dev = data->cmpnt->dev;
	struct abox_data *abox_data = data->abox_data;
	int id = data->id;
	enum qchannel clk;

	abox_info(dev, "%s(%d)\n", __func__, disable);

	clk = ABOX_BCLK_UAIF0 + id;
	if (clk < ABOX_BCLK_UAIF0 || clk >= ABOX_BCLK_DSIF)
		return -EINVAL;

	return abox_disable_qchannel(dev, abox_data, clk, disable);
}

static int abox_if_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	struct device *dev = dai->dev;
	struct abox_if_data *data = snd_soc_dai_get_drvdata(dai);

	abox_dbg(dev, "%s(%d)\n", __func__, tristate);

	return abox_if_disable_qchannel(data, !tristate);
}

static int abox_if_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct abox_if_data *data = snd_soc_dai_get_drvdata(dai);
	struct abox_data *abox_data = data->abox_data;
	int ret;

	abox_dbg(dev, "%s[%c]\n", __func__,
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
			'C' : 'P');

	abox_request_cpu_gear_dai(dev, abox_data, dai, abox_data->cpu_gear_min);
	ret = clk_enable(data->clk_bclk);
	if (ret < 0) {
		abox_err(dev, "Failed to enable bclk: %d\n", ret);
		goto err;
	}
	ret = clk_enable(data->clk_bclk_gate);
	if (ret < 0) {
		abox_err(dev, "Failed to enable bclk_gate: %d\n", ret);
		goto err;
	}
err:
	return ret;
}

static void abox_if_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct abox_if_data *data = snd_soc_dai_get_drvdata(dai);
	struct abox_data *abox_data = data->abox_data;

	abox_dbg(dev, "%s[%c]\n", __func__,
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
			'C' : 'P');

	clk_disable(data->clk_bclk_gate);
	clk_disable(data->clk_bclk);
	abox_request_cpu_gear_dai(dev, abox_data, dai, 0);
}

static int abox_if_hw_free(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct abox_if_data *data = snd_soc_dai_get_drvdata(dai);
	struct abox_data *abox_data = data->abox_data;

	abox_dbg(dev, "%s[%c]\n", __func__,
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
			'C' : 'P');

	abox_register_bclk_usage(dev, abox_data, dai->id, 0, 0, 0);

	if (abox_if_test_quirk(data->quirks, ABOX_IF_QUIRK_DELAYED_STOP)) {
		unsigned long time;

		/* time of 1 frame in us */
		time = DIV_ROUND_UP(USEC_PER_SEC, data->config[ABOX_IF_RATE]);
		udelay(time);
		abox_info(dev, "delayed stop: %dus\n", time);
	}

	return 0;
}

static int abox_spdy_mute_stream(struct snd_soc_dai *dai, int mute, int stream)
{
	struct device *dev = dai->dev;
	struct abox_if_data *data = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_component *cmpnt = data->cmpnt;

	abox_info(dev, "%s(%d)\n", __func__, mute);

	return snd_soc_component_update_bits(cmpnt, SPDY_REG_CTRL,
				ABOX_ENABLE_MASK, !mute << ABOX_ENABLE_L);
}

static int abox_dsif_set_bclk_ratio(struct snd_soc_dai *dai, unsigned int ratio)
{
	struct device *dev = dai->dev;
	struct abox_if_data *data = snd_soc_dai_get_drvdata(dai);
	unsigned int rate = dai->rate;
	int ret;

	abox_dbg(dev, "%s(%u)\n", __func__, ratio);

	ret = clk_set_rate(data->clk_bclk, rate * ratio);
	if (ret < 0)
		abox_err(dev, "bclk set error=%d\n", ret);
	else
		abox_info(dev, "rate=%u, bclk=%lu\n", rate,
				clk_get_rate(data->clk_bclk));

	return ret;
}

static int abox_dsif_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct device *dev = dai->dev;
	struct abox_if_data *data = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl;
	int ret = 0;

	abox_info(dev, "%s(0x%08x)\n", __func__, fmt);

	pm_runtime_get_sync(dev);

	snd_soc_component_read(cmpnt, DSIF_REG_CTRL, &ctrl);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_PDM:
		break;
	default:
		ret = -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
	case SND_SOC_DAIFMT_NB_IF:
		set_value_by_name(ctrl, ABOX_DSIF_BCLK_POLARITY, 1);
		break;
	case SND_SOC_DAIFMT_IB_NF:
	case SND_SOC_DAIFMT_IB_IF:
		set_value_by_name(ctrl, ABOX_DSIF_BCLK_POLARITY, 0);
		break;
	default:
		ret = -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		ret = -EINVAL;
	}

	snd_soc_component_write(cmpnt, DSIF_REG_CTRL, ctrl);

	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return ret;
}

static int abox_dsif_set_channel_map(struct snd_soc_dai *dai,
		unsigned int tx_num, unsigned int *tx_slot,
		unsigned int rx_num, unsigned int *rx_slot)
{
	struct device *dev = dai->dev;
	struct abox_if_data *data = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_component *cmpnt = data->cmpnt;

	abox_info(dev, "%s\n", __func__);

	snd_soc_component_update_bits(cmpnt, DSIF_REG_CTRL, ABOX_ORDER_MASK,
			(tx_slot[0] ? 1 : 0) << ABOX_ORDER_L);

	return 0;
}

static int abox_dsif_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params, struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct abox_if_data *data = snd_soc_dai_get_drvdata(dai);
	unsigned int channels, rate, width;

	abox_info(dev, "%s[%c]\n", __func__,
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
			'C' : 'P');

	channels = params_channels(hw_params);
	rate = params_rate(hw_params);
	width = params_width(hw_params);

	abox_info(dev, "rate=%u, width=%d, channel=%u, bclk=%lu\n",
			rate,
			width,
			channels,
			clk_get_rate(data->clk_bclk));

	switch (params_format(hw_params)) {
	case SNDRV_PCM_FORMAT_S32:
		break;
	default:
		return -EINVAL;
	}

	switch (channels) {
	case 2:
		break;
	default:
		return -EINVAL;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		abox_cmpnt_reset_cnt_val(data->abox_data, dai->id);

	return 0;
}

static int abox_dsif_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct device *dev = dai->dev;
	struct abox_if_data *data = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_component *cmpnt = data->cmpnt;

	abox_info(dev, "%s(%d)\n", __func__, mute);

	return snd_soc_component_update_bits(cmpnt, DSIF_REG_CTRL,
				ABOX_ENABLE_MASK, !mute << ABOX_ENABLE_L);
}

static int abox_uaif_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct device *dev = dai->dev;
	struct abox_if_data *data = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl0, ctrl1;
	int ret = 0;

	abox_info(dev, "%s(0x%08x)\n", __func__, fmt);

	pm_runtime_get_sync(dev);

	snd_soc_component_read(cmpnt, UAIF_REG_CTRL0, &ctrl0);
	snd_soc_component_read(cmpnt, UAIF_REG_CTRL1, &ctrl1);

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		set_value_by_name(ctrl1, ABOX_WS_MODE, 0);
		break;
	case SND_SOC_DAIFMT_DSP_A:
		set_value_by_name(ctrl1, ABOX_WS_MODE, 1);
		break;
	default:
		ret = -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		set_value_by_name(ctrl1, ABOX_BCLK_POLARITY, 1);
		set_value_by_name(ctrl1, ABOX_WS_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_NB_IF:
		set_value_by_name(ctrl1, ABOX_BCLK_POLARITY, 1);
		set_value_by_name(ctrl1, ABOX_WS_POLAR, 1);
		break;
	case SND_SOC_DAIFMT_IB_NF:
		set_value_by_name(ctrl1, ABOX_BCLK_POLARITY, 0);
		set_value_by_name(ctrl1, ABOX_WS_POLAR, 0);
		break;
	case SND_SOC_DAIFMT_IB_IF:
		set_value_by_name(ctrl1, ABOX_BCLK_POLARITY, 0);
		set_value_by_name(ctrl1, ABOX_WS_POLAR, 1);
		break;
	default:
		ret = -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		data->slave = true;
		set_value_by_name(ctrl0, ABOX_MODE, 0);
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		set_value_by_name(ctrl0, ABOX_MODE, 1);
		break;
	default:
		ret = -EINVAL;
	}

	snd_soc_component_write(cmpnt, UAIF_REG_CTRL0, ctrl0);
	snd_soc_component_write(cmpnt, UAIF_REG_CTRL1, ctrl1);

	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);

	return ret;
}

static int abox_uaif_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct abox_if_data *data = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_component *cmpnt = data->cmpnt;

	abox_dbg(dev, "%s[%c]\n", __func__,
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
			'C' : 'P');

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		snd_soc_component_update_bits(cmpnt, UAIF_REG_CTRL0,
				ABOX_DATA_MODE_MASK | ABOX_IRQ_MODE_MASK,
				(1 << ABOX_DATA_MODE_L) |
				(0 << ABOX_IRQ_MODE_L));

	return abox_if_startup(substream, dai);
}

static void abox_uaif_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct abox_if_data *data = snd_soc_dai_get_drvdata(dai);
	struct abox_data *abox_data = data->abox_data;
	int ret;

	abox_dbg(dev, "%s[%c]\n", __func__,
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
			'C' : 'P');

	abox_if_shutdown(substream, dai);

	/* Reset clock source to AUDIF for suspend */
	if (data->slave) {
		ret = clk_set_rate(data->clk_mux,
				clk_get_rate(abox_data->clk_audif));
		if (ret < 0)
			abox_err(dev, "failed to set clk_mux\n");

	}
}

static int abox_uaif_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = dai->dev;
	struct abox_if_data *data = snd_soc_dai_get_drvdata(dai);
	struct abox_data *abox_data = data->abox_data;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int ctrl1;
	unsigned int channels, rate, width;
	int ret;

	abox_info(dev, "%s[%c]\n", __func__,
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
			'C' : 'P');

	channels = params_channels(hw_params);
	rate = params_rate(hw_params);
	width = params_width(hw_params);

	ret = abox_register_bclk_usage(dev, abox_data, dai->id, rate, channels,
			width);
	if (ret < 0)
		abox_err(dev, "Unable to register bclk usage: %d\n", ret);

	if (data->slave) {
		ret = clk_set_rate(data->clk_mux, RATE_MUX_SLAVE);
		if (ret < 0) {
			abox_err(dev, "mux set error: %d\n", ret);
			return ret;
		}

		ret = clk_set_rate(data->clk_bclk, RATE_MUX_SLAVE);
		if (ret < 0) {
			abox_err(dev, "bclk set error: %d\n", ret);
			return ret;
		}
	} else {
		ret = clk_set_rate(data->clk_bclk, rate * channels * width);
		if (ret < 0) {
			abox_err(dev, "bclk set error: %d\n", ret);
			return ret;
		}
	}

	abox_info(dev, "rate=%u, width=%u, channel=%u, bclk=%lu\n",
			rate, width, channels, clk_get_rate(data->clk_bclk));

	ret = snd_soc_component_read(cmpnt, UAIF_REG_CTRL1, &ctrl1);
	if (ret < 0)
		abox_err(dev, "sfr access failed: %d\n", ret);

	switch (params_format(hw_params)) {
	case SNDRV_PCM_FORMAT_S16:
	case SNDRV_PCM_FORMAT_S24:
	case SNDRV_PCM_FORMAT_S24_3LE:
	case SNDRV_PCM_FORMAT_S32:
		set_value_by_name(ctrl1, ABOX_SBIT_MAX, (width - 1));
		break;
	default:
		return -EINVAL;
	}

	switch (rtd->dai_link->dai_fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		set_value_by_name(ctrl1, ABOX_VALID_STR, 0);
		set_value_by_name(ctrl1, ABOX_VALID_END, 0);
		break;
	default:
		set_value_by_name(ctrl1, ABOX_VALID_STR, (width - 1));
		set_value_by_name(ctrl1, ABOX_VALID_END, (width - 1));
		break;
	}

	set_value_by_name(ctrl1, ABOX_SLOT_MAX, (channels - 1));
	set_value_by_name(ctrl1, ABOX_FORMAT, abox_get_format(width, channels));

	ret = snd_soc_component_write(cmpnt, UAIF_REG_CTRL1, ctrl1);
	if (ret < 0)
		abox_err(dev, "sfr access failed: %d\n", ret);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		abox_cmpnt_reset_cnt_val(abox_data, dai->id);

	return 0;
}

static int abox_uaif_mute_stream(struct snd_soc_dai *dai, int mute, int stream)
{
	struct device *dev = dai->dev;
	struct abox_if_data *data = snd_soc_dai_get_drvdata(dai);
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned long mask, shift;

	abox_info(dev, "%s[%c](%d)\n", __func__,
			(stream == SNDRV_PCM_STREAM_CAPTURE) ? 'C' : 'P', mute);

	if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		mask = ABOX_MIC_ENABLE_MASK;
		shift = ABOX_MIC_ENABLE_L;
	} else {
		mask = ABOX_SPK_ENABLE_MASK;
		shift = ABOX_SPK_ENABLE_L;
	}

	/* Delay to flush FIFO in UAIF */
	if (mute)
		usleep_range(600, 1000);

	return snd_soc_component_update_bits(cmpnt, UAIF_REG_CTRL0,
				mask, !mute << shift);
}

static const struct snd_soc_dai_ops abox_spdy_dai_ops = {
	.startup	= abox_if_startup,
	.shutdown	= abox_if_shutdown,
	.hw_free	= abox_if_hw_free,
	.mute_stream	= abox_spdy_mute_stream,
};

static struct snd_soc_dai_driver abox_spdy_dai_drv = {
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = ABOX_SAMPLING_RATES,
		.rate_min = 40000,
		.rate_max = 40000,
		.formats = SNDRV_PCM_FMTBIT_S16,
	},
	.ops = &abox_spdy_dai_ops,
};

static const struct snd_soc_dai_ops abox_dsif_dai_ops = {
	.set_bclk_ratio	= abox_dsif_set_bclk_ratio,
	.set_fmt	= abox_dsif_set_fmt,
	.set_channel_map	= abox_dsif_set_channel_map,
	.set_tristate	= abox_if_set_tristate,
	.startup	= abox_if_startup,
	.shutdown	= abox_if_shutdown,
	.hw_params	= abox_dsif_hw_params,
	.hw_free	= abox_if_hw_free,
	.digital_mute	= abox_dsif_digital_mute,
};

static struct snd_soc_dai_driver abox_dsif_dai_drv = {
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = ABOX_SAMPLING_RATES,
		.rate_min = 8000,
		.rate_max = 384000,
		.formats = SNDRV_PCM_FMTBIT_S32,
	},
	.ops = &abox_dsif_dai_ops,
};

static const struct snd_soc_dai_ops abox_uaif_dai_ops = {
	.set_fmt	= abox_uaif_set_fmt,
	.set_tristate	= abox_if_set_tristate,
	.startup	= abox_uaif_startup,
	.shutdown	= abox_uaif_shutdown,
	.hw_params	= abox_uaif_hw_params,
	.hw_free	= abox_if_hw_free,
	.mute_stream	= abox_uaif_mute_stream,
};

static struct snd_soc_dai_driver abox_uaif_dai_drv = {
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 8,
		.rates = ABOX_SAMPLING_RATES,
		.rate_min = 8000,
		.rate_max = 384000,
		.formats = ABOX_SAMPLE_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 8,
		.rates = ABOX_SAMPLING_RATES,
		.rate_min = 8000,
		.rate_max = 384000,
		.formats = ABOX_SAMPLE_FORMATS,
	},
	.ops = &abox_uaif_dai_ops,
	.symmetric_rates = 1,
	.symmetric_channels = 1,
	.symmetric_samplebits = 1,
};

static unsigned int abox_uaif_read_format(struct abox_if_data *data)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	struct device *dev = cmpnt->dev;
	unsigned int val = 0;
	int ret;

	ret = snd_soc_component_read(cmpnt, UAIF_REG_CTRL1, &val);
	if (ret < 0)
		abox_err(dev, "sfr access failed: %d\n", ret);

	val = (val & ABOX_FORMAT_MASK) >> ABOX_FORMAT_L;

	return val;
}

static unsigned int abox_if_read_width(struct abox_if_data *data)
{
	unsigned int ret;

	switch (data->dai_drv->id) {
	case ABOX_UAIF0 ... ABOX_UAIF6:
		ret = abox_get_width(abox_uaif_read_format(data));
		break;
	case ABOX_DSIF:
		ret = 32;
		break;
	case ABOX_SPDY:
		ret = 16;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

static int abox_if_config_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_if_data *data = snd_soc_component_get_drvdata(cmpnt);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = data->config[reg];

	abox_dbg(dev, "%s(0x%08x): %u\n", __func__, reg, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int abox_if_config_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_if_data *data = snd_soc_component_get_drvdata(cmpnt);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];
	unsigned int rate, width, channels, bclk;
	int ret;

	abox_info(dev, "%s(0x%08x, %u)\n", __func__, reg, val);

	if (val < mc->min || val > mc->max)
		return -EINVAL;

	data->config[reg] = val;

	rate = data->config[ABOX_IF_RATE];
	width = data->config[ABOX_IF_WIDTH];
	channels = data->config[ABOX_IF_CHANNEL];
	bclk = rate * width * channels;
	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		abox_info(dev, "unable to get pm runtime: %d\n", ret);
	} else if (bclk && (bclk != clk_get_rate(data->clk_bclk)) &&
			!data->cmpnt->active) {
		struct abox_data *abox_data = data->abox_data;
		enum abox_dai dai_id = data->dai_drv->id;

		abox_register_bclk_usage(dev, abox_data, dai_id,
				rate, channels, width);
		ret = clk_set_rate(data->clk_bclk, bclk);
		if (ret < 0)
			abox_err(dev, "failed to set clk_bclk\n");
		abox_dbg(dev, "rate=%u, width=%u, channel=%u, bclk=%lu\n",
			rate, width, channels, clk_get_rate(data->clk_bclk));
		abox_register_bclk_usage(dev, abox_data, dai_id, 0, 0, 0);
	}
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put(dev);

	return 0;
}

static int abox_if_extend_bclk_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_if_data *data = snd_soc_component_get_drvdata(cmpnt);
	bool val = data->extend_bclk;

	abox_dbg(dev, "%s: %d\n", __func__, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int abox_if_extend_bclk_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_if_data *data = snd_soc_component_get_drvdata(cmpnt);
	bool val = !!ucontrol->value.integer.value[0];

	abox_info(dev, "%s(%d)\n", __func__, val);

	data->extend_bclk = val;

	return 0;
}

static int abox_if_enable_bclk_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_if_data *data = snd_soc_component_get_drvdata(cmpnt);
	bool val = data->enable_bclk;

	abox_dbg(dev, "%s: %d\n", __func__, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int abox_if_enable_bclk_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_if_data *data = snd_soc_component_get_drvdata(cmpnt);
	bool val = !!ucontrol->value.integer.value[0];

	abox_info(dev, "%s(%d)\n", __func__, val);

	data->enable_bclk = val;

	return abox_if_disable_qchannel(data, val);
}

static const char * const uaif_start_fifo_size_enum_texts[] = {
	"16Byte", "24Byte", "32Byte", "40Byte", "48Byte", "56Byte", "64Byte",
};
static const unsigned int uaif_start_fifo_size_enum_values[] = {
	2, 3, 4, 5, 6, 7, 8,
};
static SOC_VALUE_ENUM_SINGLE_DECL(uaif_start_fifo_size_spk_enum,
		UAIF_REG_CTRL0, ABOX_START_FIFO_SIZE_SPK_L,
		ABOX_START_FIFO_SIZE_SPK_MASK,
		uaif_start_fifo_size_enum_texts,
		uaif_start_fifo_size_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(uaif_start_fifo_size_mic_enum,
		UAIF_REG_CTRL0, ABOX_START_FIFO_SIZE_MIC_L,
		ABOX_START_FIFO_SIZE_SPK_MASK,
		uaif_start_fifo_size_enum_texts,
		uaif_start_fifo_size_enum_values);
static const struct snd_kcontrol_new abox_uaif_controls[] = {
	SOC_ENUM("Start FIFO Size Spk", uaif_start_fifo_size_spk_enum),
	SOC_ENUM("Start FIFO Size Mic", uaif_start_fifo_size_mic_enum),
	SOC_SINGLE("Start FIFO Diff Spk", UAIF_REG_CTRL0,
			ABOX_START_FIFO_DIFF_SPK_L, 0xf, 0),
	SOC_SINGLE("Start FIFO Diff Mic", UAIF_REG_CTRL0,
			ABOX_START_FIFO_DIFF_MIC_L, 0xf, 0),
};
static const struct snd_kcontrol_new abox_if_controls[] = {
	SOC_SINGLE_EXT("Width", ABOX_IF_WIDTH, 0, 32, 0,
			abox_if_config_get, abox_if_config_put),
	SOC_SINGLE_EXT("Channel", ABOX_IF_CHANNEL, 0, 8, 0,
			abox_if_config_get, abox_if_config_put),
	SOC_SINGLE_EXT("Rate", ABOX_IF_RATE, 0, 384000, 0,
			abox_if_config_get, abox_if_config_put),
	SOC_SINGLE_EXT("Extend BCLK", 0, 0, 1, 0,
			abox_if_extend_bclk_get, abox_if_extend_bclk_put),
	SOC_SINGLE_EXT("Enable BCLK", 0, 0, 1, 0,
			abox_if_enable_bclk_get, abox_if_enable_bclk_put),
};

static int abox_if_bclk_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_if_data *data = dev_get_drvdata(dev);
	int disable = SND_SOC_DAPM_EVENT_ON(e);

	abox_dbg(dev, "%s(%d)\n", __func__, e);

	return abox_if_disable_qchannel(data, disable);
}

static int abox_if_bclk_connected(struct snd_soc_dapm_widget *source,
		struct snd_soc_dapm_widget *sink)
{
	struct snd_soc_dapm_context *dapm = source->dapm;
	struct abox_if_data *data = dev_get_drvdata(dapm->dev);

	return data->extend_bclk;
}

static const struct snd_soc_dapm_widget abox_if_pinctrl_widgets[] = {
	SND_SOC_DAPM_PINCTRL("PINCTRL", "active", "idle"),
};

static const struct snd_soc_dapm_route abox_if_pinctrl_routes[] = {
	/* sink, control, source */
	{"Playback", NULL, "PINCTRL"},
	{"Capture", NULL, "PINCTRL"},
};

static const struct snd_soc_dapm_widget abox_if_widgets[] = {
	SND_SOC_DAPM_SUPPLY("BCLK", SND_SOC_NOPM, 0, 0, abox_if_bclk_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_route abox_if_routes[] = {
	/* sink, control, source */
	{"Playback", NULL, "BCLK", abox_if_bclk_connected},
	{"Capture", NULL, "BCLK", abox_if_bclk_connected},
};

static int abox_if_cmpnt_probe(struct snd_soc_component *cmpnt)
{
	struct device *dev = cmpnt->dev;
	struct abox_if_data *data = snd_soc_component_get_drvdata(cmpnt);
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(cmpnt);
	struct pinctrl *p;

	abox_dbg(dev, "%s\n", __func__);

	data->cmpnt = cmpnt;
	abox_cmpnt_register_if(data->abox_data->dev, dev, data->id,
			data->dai_drv->name,
			!!data->dai_drv->playback.formats,
			!!data->dai_drv->capture.formats);

	switch (data->dai_drv->id) {
	case ABOX_UAIF0 ... ABOX_UAIF6:
		snd_soc_add_component_controls(cmpnt, abox_uaif_controls,
				ARRAY_SIZE(abox_uaif_controls));
		snd_soc_dapm_add_routes(dapm, abox_if_routes,
				ARRAY_SIZE(abox_if_routes));
		break;
	case ABOX_DSIF:
		/* nothing to add now */
		break;
	case ABOX_SPDY:
		/* nothing to add now */
		break;
	default:
		abox_err(dev, "invalid dai id: %d\n", data->dai_drv->id);
		break;
	}

	p = pinctrl_get(dev);
	if (!IS_ERR(p)) {
		/* ALSA returns error if pinctrl widget is registered on
		 * the device without pinctrl attribute.
		 */
		snd_soc_dapm_new_controls(dapm, abox_if_pinctrl_widgets,
				ARRAY_SIZE(abox_if_pinctrl_widgets));
		snd_soc_dapm_add_routes(dapm, abox_if_pinctrl_routes,
				ARRAY_SIZE(abox_if_pinctrl_routes));
		pinctrl_put(p);
	}

	return 0;
}

static void abox_if_cmpnt_remove(struct snd_soc_component *cmpnt)
{
	struct device *dev = cmpnt->dev;

	abox_dbg(dev, "%s\n", __func__);
}

static unsigned int abox_if_read(struct snd_soc_component *cmpnt,
		unsigned int reg)
{
	struct abox_if_data *data = snd_soc_component_get_drvdata(cmpnt);
	struct abox_data *abox_data = data->abox_data;
	unsigned int val;
	int ret;

	ret = snd_soc_component_read(abox_data->cmpnt, data->base + reg, &val);
	if (ret < 0) {
		abox_err(cmpnt->dev, "invalid register:%#x\n", reg);
		dump_stack();
		return ret;
	}

	return val;
}

static int abox_if_write(struct snd_soc_component *cmpnt,
		unsigned int reg, unsigned int val)
{
	struct abox_if_data *data = snd_soc_component_get_drvdata(cmpnt);
	struct abox_data *abox_data = data->abox_data;
	int ret;

	ret = snd_soc_component_write(abox_data->cmpnt, data->base + reg, val);
	if (ret < 0) {
		abox_err(cmpnt->dev, "invalid register:%#x\n", reg);
		dump_stack();
		return ret;
	}

	return 0;
}

static const struct snd_soc_component_driver abox_if_cmpnt = {
	.controls	= abox_if_controls,
	.num_controls	= ARRAY_SIZE(abox_if_controls),
	.dapm_widgets	= abox_if_widgets,
	.num_dapm_widgets = ARRAY_SIZE(abox_if_widgets),
	.probe		= abox_if_cmpnt_probe,
	.remove		= abox_if_cmpnt_remove,
	.read		= abox_if_read,
	.write		= abox_if_write,
};

static enum abox_dai abox_spdy_get_dai_id(int id)
{
	return ABOX_SPDY;
}

static const char *abox_spdy_get_dai_name(int id)
{
	return "SPDY";
}

static unsigned int abox_spdy_get_reg_base(int id)
{
	return ABOX_SPDYIF_CTRL;
}

static enum abox_dai abox_dsif_get_dai_id(int id)
{
	return ABOX_DSIF;
}

static const char *abox_dsif_get_dai_name(int id)
{
	return "DSIF";
}

static unsigned int abox_dsif_get_reg_base(int id)
{
	return ABOX_DSIF_CTRL;
}

static enum abox_dai abox_uaif_get_dai_id(int id)
{
	return ABOX_UAIF0 + id;
}

static const char *abox_uaif_get_dai_name(int id)
{
	static const char * const names[] = {
		"UAIF0", "UAIF1", "UAIF2", "UAIF3", "UAIF4",
		"UAIF5", "UAIF6", "UAIF7", "UAIF8", "UAIF9",
	};

	return (id < ARRAY_SIZE(names)) ? names[id] : ERR_PTR(-EINVAL);
}

static unsigned int abox_uaif_get_reg_base(int id)
{
	return ABOX_UAIF_CTRL0(id);
}

static const struct of_device_id samsung_abox_if_match[] = {
	{
		.compatible = "samsung,abox-uaif",
		.data = (void *)&(struct abox_if_of_data){
			.get_dai_id = abox_uaif_get_dai_id,
			.get_dai_name = abox_uaif_get_dai_name,
			.get_reg_base = abox_uaif_get_reg_base,
			.base_dai_drv = &abox_uaif_dai_drv,
		},
	},
	{
		.compatible = "samsung,abox-dsif",
		.data = (void *)&(struct abox_if_of_data){
			.get_dai_id = abox_dsif_get_dai_id,
			.get_dai_name = abox_dsif_get_dai_name,
			.get_reg_base = abox_dsif_get_reg_base,
			.base_dai_drv = &abox_dsif_dai_drv,
		},
	},
	{
		.compatible = "samsung,abox-spdy",
		.data = (void *)&(struct abox_if_of_data){
			.get_dai_id = abox_spdy_get_dai_id,
			.get_dai_name = abox_spdy_get_dai_name,
			.get_reg_base = abox_spdy_get_reg_base,
			.base_dai_drv = &abox_spdy_dai_drv,
		},
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_abox_if_match);

static int samsung_abox_if_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device *dev_abox = dev->parent;
	struct device_node *np = dev->of_node;
	struct abox_if_data *data;
	int ret;

	abox_dbg(dev, "%s\n", __func__);

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	platform_set_drvdata(pdev, data);

	data->sfr_base = devm_get_ioremap(pdev, "sfr", NULL, NULL);
	if (IS_ERR(data->sfr_base))
		return PTR_ERR(data->sfr_base);

	ret = of_samsung_property_read_u32(dev, np, "id", &data->id);
	if (ret < 0) {
		abox_err(dev, "id property reading fail\n");
		return ret;
	}

	data->clk_mux = devm_clk_get_and_prepare(pdev, "mux");
	if (IS_ERR(data->clk_mux))
		data->clk_mux = NULL;

	data->clk_bclk = devm_clk_get_and_prepare(pdev, "bclk");
	if (IS_ERR(data->clk_bclk))
		return PTR_ERR(data->clk_bclk);

	data->clk_bclk_gate = devm_clk_get_and_prepare(pdev, "bclk_gate");
	if (IS_ERR(data->clk_bclk_gate))
		return PTR_ERR(data->clk_bclk_gate);

	data->quirks = abox_if_probe_quirks(np);

	data->of_data = of_device_get_match_data(dev);
	data->abox_data = dev_get_drvdata(dev_abox);
	data->base = data->of_data->get_reg_base(data->id);
	data->dai_drv = devm_kmemdup(dev, data->of_data->base_dai_drv,
			sizeof(*data->of_data->base_dai_drv), GFP_KERNEL);
	if (!data->dai_drv)
		return -ENOMEM;

	data->dai_drv->id = data->of_data->get_dai_id(data->id);
	data->dai_drv->name = data->of_data->get_dai_name(data->id);
	ret = devm_snd_soc_register_component(dev, &abox_if_cmpnt,
			data->dai_drv, 1);
	if (ret < 0)
		return ret;

	pm_runtime_enable(dev);
	pm_runtime_no_callbacks(dev);

	return ret;
}

static int samsung_abox_if_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	abox_dbg(dev, "%s\n", __func__);

	return 0;
}

static void samsung_abox_if_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	abox_dbg(dev, "%s\n", __func__);
	pm_runtime_disable(dev);
}

struct platform_driver samsung_abox_if_driver = {
	.probe  = samsung_abox_if_probe,
	.remove = samsung_abox_if_remove,
	.shutdown = samsung_abox_if_shutdown,
	.driver = {
		.name = "abox-if",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_abox_if_match),
	},
};

int abox_if_hw_params_fixup(struct snd_soc_dai *dai,
		struct snd_pcm_hw_params *params)
{
	struct device *dev = dai->dev;
	struct abox_if_data *data = dev_get_drvdata(dev);
	unsigned int rate, channels, width;
	int ret = 0;

	if (dev->driver != &samsung_abox_if_driver.driver)
		return -EINVAL;

	abox_dbg(dev, "%s[%s]\n", __func__, dai->name);

	rate = data->config[ABOX_IF_RATE];
	channels = data->config[ABOX_IF_CHANNEL];
	width = data->config[ABOX_IF_WIDTH];

	/* don't break symmetric limitation */
	if (dai->driver->symmetric_rates && dai->rate && dai->rate != rate)
		rate = dai->rate;
	if (dai->driver->symmetric_channels && dai->channels &&
			dai->channels != channels)
		channels = dai->channels;
	if (dai->driver->symmetric_samplebits && dai->sample_bits) {
		unsigned int r_width;

		r_width = abox_if_read_width(data);
		if (r_width && r_width != width)
			width = r_width;
	}

	if (rate)
		hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE)->min = rate;

	if (channels)
		hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS)->min =
				channels;

	if (width) {
		unsigned int format = 0;

		switch (width) {
		case 8:
			format = SNDRV_PCM_FORMAT_S8;
			break;
		case 16:
			format = SNDRV_PCM_FORMAT_S16;
			break;
		case 24:
			format = SNDRV_PCM_FORMAT_S24;
			break;
		case 32:
			format = SNDRV_PCM_FORMAT_S32;
			break;
		default:
			width = format = 0;
			break;
		}

		if (format) {
			struct snd_mask *mask;

			mask = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);
			snd_mask_none(mask);
			snd_mask_set(mask, format);
		}
	}

	if (rate || channels || width)
		abox_dbg(dev, "%s: %d bit, %u ch, %uHz\n", dai->name,
				width, channels, rate);

	return ret;
}

int abox_if_hw_params_fixup_helper(struct snd_soc_pcm_runtime *rtd,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_dai *dai = rtd->cpu_dai;

	return abox_if_hw_params_fixup(dai, params);
}
