/* sound/soc/samsung/vts/vts_s_lif.c
 *
 * ALSA SoC - Samsung VTS Serial Local Interface driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* #define DEBUG */
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
#include <linux/sched/clock.h>
#include <linux/miscdevice.h>

#include <asm-generic/delay.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>

#include <sound/samsung/mailbox.h>
#include <sound/samsung/vts.h>
#include <soc/samsung/exynos-pmu-if.h>
#include <soc/samsung/exynos-el3_mon.h>

#include "vts.h"
#include "vts_util.h"
#include "vts_s_lif.h"
#include "vts_s_lif_soc.h"
#include "vts_s_lif_nm.h"
#include "vts_s_lif_dump.h"
#include "vts_s_lif_memlog.h"

/* for debug */
static struct vts_s_lif_data *p_vts_s_lif_data;

static int vts_s_lif_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct device *dev = dai->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	int ret = 0;

	slif_info(dev, "(0x%x)\n", fmt);

	ret =  vts_s_lif_soc_set_fmt(data, fmt);

	return ret;
}

static int vts_s_lif_dai_set_tristate(struct snd_soc_dai *dai, int tristate)
{
	struct device *dev = dai->dev;
	int ret = 0;

	slif_info(dev, "(%d)\n", tristate);

	return ret;
}

static int vts_s_lif_dai_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	int ret = 0;

	slif_info(dev, "\n");

	ret = vts_s_lif_soc_startup(substream, data);

	return ret;
}

static void vts_s_lif_dai_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);

	slif_info(dev, "\n");

	vts_s_lif_soc_shutdown(substream, data);

	return;
}

static int vts_s_lif_dai_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params, struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	int ret = 0;

	slif_info(dev, "\n");

	ret = vts_s_lif_soc_hw_params(substream, hw_params, data);

	return ret;
}

static int vts_s_lif_dai_hw_free(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	int ret = 0;

	slif_info(dev, "\n");

	ret = vts_s_lif_soc_hw_free(substream, data);

	return ret;
}

static int vts_s_lif_dai_mute_stream(struct snd_soc_dai *dai, int mute,
		int stream)
{
	struct device *dev = dai->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	int ret = 0;

	slif_info(dev, "(%d)\n", mute);
	ret =  vts_s_lif_soc_dma_en(!mute, data);

	return ret;
}

static const struct snd_soc_dai_ops vts_s_lif_dai_ops = {
	.set_fmt	= vts_s_lif_dai_set_fmt,
	.set_tristate	= vts_s_lif_dai_set_tristate,
	.startup	= vts_s_lif_dai_startup,
	.shutdown	= vts_s_lif_dai_shutdown,
	.hw_params	= vts_s_lif_dai_hw_params,
	.hw_free	= vts_s_lif_dai_hw_free,
	.mute_stream	= vts_s_lif_dai_mute_stream,
};

static struct snd_soc_dai_driver vts_s_lif_dai[] = {
	{
		.name = "vts-s-lif-rec",
		.id = 0,
		.capture = {
			.stream_name = "VTS Serial LIF Capture",
#if 1
			.channels_min = 2,
			.channels_max = 8,
#else
			.channels_min = 1,
			.channels_max = 8,
#endif
			.rates = SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16,
			.sig_bits = 16,
		 },
		.ops = &vts_s_lif_dai_ops,
	},
};

static const char *vts_s_lif_input_sel_texts[] = {"off", "on"};
static SOC_ENUM_SINGLE_DECL(vts_s_lif_input_sel0,
		VTS_S_INPUT_EN_BASE,
		VTS_S_INPUT_EN_EN0_L,
		vts_s_lif_input_sel_texts);
static SOC_ENUM_SINGLE_DECL(vts_s_lif_input_sel1,
		VTS_S_INPUT_EN_BASE,
		VTS_S_INPUT_EN_EN1_L,
		vts_s_lif_input_sel_texts);
static SOC_ENUM_SINGLE_DECL(vts_s_lif_input_sel2,
		VTS_S_INPUT_EN_BASE,
		VTS_S_INPUT_EN_EN1_L,
		vts_s_lif_input_sel_texts);
static SOC_ENUM_SINGLE_DECL(vts_s_lif_input_sel3,
		VTS_S_INPUT_EN_BASE,
		VTS_S_INPUT_EN_EN1_L,
		vts_s_lif_input_sel_texts);

static const char *vts_s_lif_hpf_en_texts[] = {"disable", "enable"};
enum hpf_en_val {
	HPF_EN_DISABLE = 0x0,
	HPF_EN_ENABLE = 0x1,
};
static const unsigned int vts_s_lif_hpf_en_enum_values[] = {
	HPF_EN_DISABLE,
	HPF_EN_ENABLE,
};

static SOC_VALUE_ENUM_SINGLE_DECL(vts_s_lif_hpf_en0, 0, 0, 0,
		vts_s_lif_hpf_en_texts, vts_s_lif_hpf_en_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(vts_s_lif_hpf_en1, 1, 0, 0,
		vts_s_lif_hpf_en_texts, vts_s_lif_hpf_en_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(vts_s_lif_hpf_en2, 2, 0, 0,
		vts_s_lif_hpf_en_texts, vts_s_lif_hpf_en_enum_values);

static const char *vts_s_lif_dmic_en_texts[] = {"disable", "enable"};
enum dmic_en_val {
	DMIC_EN_DISABLE = 0x0,
	DMIC_EN_ENABLE = 0x1,
};
static const unsigned int vts_s_lif_dmic_en_enum_values[] = {
	DMIC_EN_DISABLE,
	DMIC_EN_ENABLE,
};

static SOC_VALUE_ENUM_SINGLE_DECL(vts_s_lif_dmic_en0, 0, 0, 0,
		vts_s_lif_dmic_en_texts, vts_s_lif_dmic_en_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(vts_s_lif_dmic_en1, 1, 0, 0,
		vts_s_lif_dmic_en_texts, vts_s_lif_dmic_en_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(vts_s_lif_dmic_en2, 2, 0, 0,
		vts_s_lif_dmic_en_texts, vts_s_lif_dmic_en_enum_values);

static int vts_s_lif_gain_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = 0;
	int ret = 0;

	ret = vts_s_lif_soc_dmic_aud_gain_mode_get(data, reg, &val);
	if (ret < 0) {
		slif_err(dev, "failed: %d\n", ret);

		return ret;
	}

	slif_dbg(dev, "(%#x): %u\n", reg, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int vts_s_lif_gain_mode_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	slif_info(dev, "(%#x, %u)\n", reg, val);

	return vts_s_lif_soc_dmic_aud_gain_mode_put(data, reg, val);
}

static int vts_s_lif_max_scale_gain_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = 0;
	int ret = 0;

	ret = vts_s_lif_soc_dmic_aud_max_scale_gain_get(data, reg, &val);
	if (ret < 0) {
		slif_err(dev, "failed: %d\n", ret);

		return ret;
	}

	slif_dbg(dev, "(%#x): %u\n", reg, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int vts_s_lif_max_scale_gain_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	slif_info(dev, "(%#x, %u)\n", reg, val);

	return vts_s_lif_soc_dmic_aud_max_scale_gain_put(data, reg, val);
}

static int vts_s_lif_dmic_aud_control_gain_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = 0;
	int ret = 0;

	ret = vts_s_lif_soc_dmic_aud_control_gain_get(data, reg, &val);
	if (ret < 0) {
		slif_err(dev, "failed: %d\n", ret);

		return ret;
	}

	slif_dbg(dev, "(%#x): %u\n", reg, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int vts_s_lif_dmic_aud_control_gain_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	slif_info(dev, "(%#x, %u)\n", reg, val);

	return vts_s_lif_soc_dmic_aud_control_gain_put(data, reg, val);
}

static int vts_s_lif_vol_set_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = 0;
	int ret = 0;

	ret = vts_s_lif_soc_vol_set_get(data, reg, &val);
	if (ret < 0) {
		slif_err(dev, "failed: %d\n", ret);

		return ret;
	}

	slif_dbg(dev, "(%#x): %u\n", reg, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int vts_s_lif_vol_set_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	slif_info(dev, "(%#x, %u)\n", reg, val);

	return vts_s_lif_soc_vol_set_put(data, reg, val);
}

static int vts_s_lif_vol_change_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = 0;
	int ret = 0;

	ret = vts_s_lif_soc_vol_change_get(data, reg, &val);
	if (ret < 0) {
		slif_err(dev, "failed: %d\n", ret);

		return ret;
	}

	slif_dbg(dev, "(%#x): %u\n", reg, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int vts_s_lif_vol_change_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	slif_dbg(dev, "(%#x, %u)\n", reg, val);

	return vts_s_lif_soc_vol_change_put(data, reg, val);
}

static int vts_s_lif_channel_map_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = 0;
	int ret = 0;

	ret = vts_s_lif_soc_channel_map_get(data, reg, &val);
	if (ret < 0) {
		slif_err(dev, "failed: %d\n", ret);

		return ret;
	}

	slif_dbg(dev, "(%#x): %u\n", reg, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int vts_s_lif_channel_map_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	slif_dbg(dev, "(%#x, %u)\n", reg, val);

	return vts_s_lif_soc_channel_map_put(data, reg, val);
}

static int vts_s_lif_init_mute_ms_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);

	ucontrol->value.integer.value[0] = data->mute_ms;

	return 0;
}

static int vts_s_lif_init_mute_ms_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	slif_info(dev, "(%#x, %u)\n", reg, val);

	if ((val < 10) && (val != 0))
		val = 10;

	data->mute_ms = val;

	return 0;
}

static int vts_s_lif_dmic_aud_control_hpf_sel_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = 0;
	int ret = 0;

	ret = vts_s_lif_soc_dmic_aud_control_hpf_sel_get(data, reg, &val);
	if (ret < 0) {
		slif_err(dev, "failed: %d\n", ret);

		return ret;
	}

	slif_dbg(dev, "(%#x): %u\n", reg, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int vts_s_lif_dmic_aud_control_hpf_sel_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	slif_dbg(dev, "(%#x, %u)\n", reg, val);

	return vts_s_lif_soc_dmic_aud_control_hpf_sel_put(data, reg, val);
}

static int vts_s_lif_dmic_aud_control_hpf_en_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int item;
	unsigned int val = 0;
	int ret = 0;

	ret = vts_s_lif_soc_dmic_aud_control_hpf_en_get(data, reg, &val);
	if (ret < 0) {
		slif_err(dev, "failed: %d\n", ret);

		return ret;
	}

	slif_dbg(dev, "(%#x): %u\n", reg, val);
	item = snd_soc_enum_val_to_item(e, val);
	ucontrol->value.enumerated.item[0] = item;

	return 0;
}

static int vts_s_lif_dmic_aud_control_hpf_en_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int *item = ucontrol->value.enumerated.item;
	enum hpf_en_val type;
	unsigned int val;

	if (item[0] >= e->items)
		return -EINVAL;

	type = snd_soc_enum_item_to_val(e, item[0]);
	val = (unsigned int)type;

	slif_dbg(dev, "(%#x, %u)\n", reg, val);

	return vts_s_lif_soc_dmic_aud_control_hpf_en_put(data, reg, val);
}

static int vts_s_lif_dmic_en_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int item;
	unsigned int val = 0;

	vts_s_lif_soc_dmic_en_get(data, reg, &val);

	slif_dbg(dev, "(%#x): %u\n", reg, val);

	item = snd_soc_enum_val_to_item(e, val);
	ucontrol->value.enumerated.item[0] = item;

	return 0;
}

static int vts_s_lif_dmic_en_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg = e->reg;
	unsigned int *item = ucontrol->value.enumerated.item;
	enum hpf_en_val type;
	unsigned int val;

	if (item[0] >= e->items)
		return -EINVAL;

	type = snd_soc_enum_item_to_val(e, item[0]);
	val = (unsigned int)type;

	slif_dbg(dev, "(%#x, %u)\n", reg, val);

	return vts_s_lif_soc_dmic_en_put(data, reg, val, true);
}

#ifdef VTS_S_LIF_PAD_ROUTE
static int vts_s_lif_debug_pad_en_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;

	return 0;
}

static int vts_s_lif_debug_pad_en_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	slif_info(dev, "(%#x, %u)\n", reg, val);

	vts_s_lif_debug_pad_en(val);

	return 0;
}
#endif

static const struct snd_kcontrol_new vts_s_lif_controls[] = {
	SOC_ENUM("INPUT EN0", vts_s_lif_input_sel0),
	SOC_ENUM("INPUT EN1", vts_s_lif_input_sel1),
	SOC_ENUM("INPUT EN2", vts_s_lif_input_sel2),
	SOC_ENUM("INPUT EN3", vts_s_lif_input_sel3),
	/* gain controls */
	SOC_SINGLE_EXT("VTS DMIC GAIN MODE0", GAIN_MODE0, 0x0, 0xFFFF, 0,
			vts_s_lif_gain_mode_get, vts_s_lif_gain_mode_put),
	SOC_SINGLE_EXT("VTS DMIC GAIN MODE1", GAIN_MODE1, 0x0, 0xFFFF, 0,
			vts_s_lif_gain_mode_get, vts_s_lif_gain_mode_put),
	SOC_SINGLE_EXT("VTS DMIC GAIN MODE2", GAIN_MODE2, 0x0, 0xFFFF, 0,
			vts_s_lif_gain_mode_get, vts_s_lif_gain_mode_put),
	SOC_SINGLE_EXT("VTS MAX SCALE GAIN0", MAX_SCALE_GAIN0, 0x0, 0xFF, 0,
			vts_s_lif_max_scale_gain_get,
			vts_s_lif_max_scale_gain_put),
	SOC_SINGLE_EXT("VTS MAX SCALE GAIN1", MAX_SCALE_GAIN1, 0x0, 0xFF, 0,
			vts_s_lif_max_scale_gain_get,
			vts_s_lif_max_scale_gain_put),
	SOC_SINGLE_EXT("VTS MAX SCALE GAIN2", MAX_SCALE_GAIN2, 0x0, 0xFF, 0,
			vts_s_lif_max_scale_gain_get,
			vts_s_lif_max_scale_gain_put),
	SOC_SINGLE_EXT("VTS CONTROL DMIC AUD GAIN0", CONTROL_DMIC_AUD0,
			0x0, 0x3F, 0,
			vts_s_lif_dmic_aud_control_gain_get,
			vts_s_lif_dmic_aud_control_gain_put),
	SOC_SINGLE_EXT("VTS CONTROL DMIC AUD GAIN1", CONTROL_DMIC_AUD1,
			0x0, 0x3F, 0,
			vts_s_lif_dmic_aud_control_gain_get,
			vts_s_lif_dmic_aud_control_gain_put),
	SOC_SINGLE_EXT("VTS CONTROL DMIC AUD GAIN2", CONTROL_DMIC_AUD2,
			0x0, 0x3F, 0,
			vts_s_lif_dmic_aud_control_gain_get,
			vts_s_lif_dmic_aud_control_gain_put),

	SOC_SINGLE_EXT("VTS VOL SET0", VOL_SET0,
			0x0, 0xFFFFFF, 0,
			vts_s_lif_vol_set_get,
			vts_s_lif_vol_set_put),
	SOC_SINGLE_EXT("VTS VOL SET1", VOL_SET1,
			0x0, 0xFFFFFF, 0,
			vts_s_lif_vol_set_get,
			vts_s_lif_vol_set_put),
	SOC_SINGLE_EXT("VTS VOL SET2", VOL_SET2,
			0x0, 0xFFFFFF, 0,
			vts_s_lif_vol_set_get,
			vts_s_lif_vol_set_put),
	SOC_SINGLE_EXT("VTS VOL SET3", VOL_SET3,
			0x0, 0xFFFFFF, 0,
			vts_s_lif_vol_set_get,
			vts_s_lif_vol_set_put),

	SOC_SINGLE_EXT("VTS VOL CHANGE0", VOL_SET0,
			0x0, 0xFFFFFF, 0,
			vts_s_lif_vol_change_get,
			vts_s_lif_vol_change_put),
	SOC_SINGLE_EXT("VTS VOL CHANGE1", VOL_SET1,
			0x0, 0xFFFFFF, 0,
			vts_s_lif_vol_change_get,
			vts_s_lif_vol_change_put),
	SOC_SINGLE_EXT("VTS VOL CHANGE2", VOL_SET2,
			0x0, 0xFFFFFF, 0,
			vts_s_lif_vol_change_get,
			vts_s_lif_vol_change_put),
	SOC_SINGLE_EXT("VTS VOL CHANGE3", VOL_SET3,
			0x0, 0xFFFFFF, 0,
			vts_s_lif_vol_change_get,
			vts_s_lif_vol_change_put),

	SOC_SINGLE_EXT("VTS Channel Map0", MAP_CH0,
			0x0, 0x7, 0x0,
			vts_s_lif_channel_map_get,
			vts_s_lif_channel_map_put),
	SOC_SINGLE_EXT("VTS Channel Map1", MAP_CH1,
			0x0, 0x7, 0x0,
			vts_s_lif_channel_map_get,
			vts_s_lif_channel_map_put),
	SOC_SINGLE_EXT("VTS Channel Map2", MAP_CH2,
			0x0, 0x7, 0x0,
			vts_s_lif_channel_map_get,
			vts_s_lif_channel_map_put),
	SOC_SINGLE_EXT("VTS Channel Map3", MAP_CH3,
			0x0, 0x7, 0x0,
			vts_s_lif_channel_map_get,
			vts_s_lif_channel_map_put),
	SOC_SINGLE_EXT("VTS Channel Map4", MAP_CH4,
			0x0, 0x7, 0x0,
			vts_s_lif_channel_map_get,
			vts_s_lif_channel_map_put),
	SOC_SINGLE_EXT("VTS Channel Map5", MAP_CH5,
			0x0, 0x7, 0x0,
			vts_s_lif_channel_map_get,
			vts_s_lif_channel_map_put),
	SOC_SINGLE_EXT("VTS Channel Map6", MAP_CH6,
			0x0, 0x7, 0x0,
			vts_s_lif_channel_map_get,
			vts_s_lif_channel_map_put),
	SOC_SINGLE_EXT("VTS Channel Map7", MAP_CH7,
			0x0, 0x7, 0x0,
			vts_s_lif_channel_map_get,
			vts_s_lif_channel_map_put),
	SOC_SINGLE_EXT("VTS INIT MUTE MS", 0,
			0x0, 0xFF, 0,
			vts_s_lif_init_mute_ms_get,
			vts_s_lif_init_mute_ms_put),
	SOC_SINGLE_EXT("VTS HPF SEL0", 0,
			0x0, 0x3F, 0,
			vts_s_lif_dmic_aud_control_hpf_sel_get,
			vts_s_lif_dmic_aud_control_hpf_sel_put),
	SOC_SINGLE_EXT("VTS HPF SEL1", 1,
			0x0, 0x3F, 0,
			vts_s_lif_dmic_aud_control_hpf_sel_get,
			vts_s_lif_dmic_aud_control_hpf_sel_put),
	SOC_SINGLE_EXT("VTS HPF SEL2", 2,
			0x0, 0x3F, 0,
			vts_s_lif_dmic_aud_control_hpf_sel_get,
			vts_s_lif_dmic_aud_control_hpf_sel_put),
	SOC_VALUE_ENUM_EXT("VTS HPF EN0", vts_s_lif_hpf_en0,
			vts_s_lif_dmic_aud_control_hpf_en_get,
			vts_s_lif_dmic_aud_control_hpf_en_put),
	SOC_VALUE_ENUM_EXT("VTS HPF EN1", vts_s_lif_hpf_en1,
			vts_s_lif_dmic_aud_control_hpf_en_get,
			vts_s_lif_dmic_aud_control_hpf_en_put),
	SOC_VALUE_ENUM_EXT("VTS HPF EN2", vts_s_lif_hpf_en2,
			vts_s_lif_dmic_aud_control_hpf_en_get,
			vts_s_lif_dmic_aud_control_hpf_en_put),
	SOC_VALUE_ENUM_EXT("VTS DMIC EN0", vts_s_lif_dmic_en0,
			vts_s_lif_dmic_en_get,
			vts_s_lif_dmic_en_put),
	SOC_VALUE_ENUM_EXT("VTS DMIC EN1", vts_s_lif_dmic_en1,
			vts_s_lif_dmic_en_get,
			vts_s_lif_dmic_en_put),
	SOC_VALUE_ENUM_EXT("VTS DMIC EN2", vts_s_lif_dmic_en2,
			vts_s_lif_dmic_en_get,
			vts_s_lif_dmic_en_put),

#ifdef VTS_S_LIF_PAD_ROUTE
	SOC_SINGLE_EXT("VTS DEBUG PAD EN", 0,
			0x0, 0x1, 0,
			vts_s_lif_debug_pad_en_get,
			vts_s_lif_debug_pad_en_put),
#endif
};

static const struct snd_soc_dapm_widget vts_s_lif_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("PAD DPDM"),
};

static const struct snd_soc_dapm_route vts_s_lif_dapm_routes[] = {
	/* sink, control, source */
	{"VTS Serial LIF Capture", NULL, "PAD DPDM"},
/*
	{"DMIC SEL", "APDM", "PAD APDM"},
	{"DMIC SEL", "DPDM", "PAD DPDM"},
	{"DMIC IF", "RCH EN", "DMIC SEL"},
	{"DMIC IF", "LCH EN", "DMIC SEL"},
	{"VTS Capture", NULL, "DMIC IF"},
*/
};

static int vts_s_lif_component_probe(struct snd_soc_component *component)
{
	struct device *dev = component->dev;
	struct vts_s_lif_data *data = dev_get_drvdata(dev);

	slif_info(dev, "\n");

	data->cmpnt = component;
	snd_soc_component_init_regmap(data->cmpnt, data->regmap_sfr);

	return 0;
}

static const struct snd_soc_component_driver vts_s_lif_component = {
	.probe			= vts_s_lif_component_probe,
	.controls		= vts_s_lif_controls,
	.num_controls		= ARRAY_SIZE(vts_s_lif_controls),
	.dapm_widgets		= vts_s_lif_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(vts_s_lif_dapm_widgets),
	.dapm_routes		= vts_s_lif_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(vts_s_lif_dapm_routes),
};

static const struct reg_default vts_s_lif_reg_defaults[] = {
	{VTS_S_SFR_APB_BASE,			0x00000000},
	{VTS_S_INT_EN_SET_BASE,			0x00000000},
	{VTS_S_INT_EN_CLR_BASE,			0x00000000},
	{VTS_S_INT_PEND_BASE,			0x00000000},
	{VTS_S_INT_CLR_BASE,			0x00000000},

	{VTS_S_CTRL_BASE,			0x00000000},

	{VTS_S_CONFIG_MASTER_BASE,		0x00002010},
	{VTS_S_CONFIG_SLAVE_BASE,		0x00002010},
	{VTS_S_CONFIG_DONE_BASE,		0x00000000},
	{VTS_S_SAMPLE_PCNT_BASE,		0xFFFFFFFF},
	{VTS_S_INPUT_EN_BASE,			0x00000000},
	{VTS_S_VOL_SET(0),			0x00800000},
	{VTS_S_VOL_SET(1),			0x00800000},
	{VTS_S_VOL_SET(2),			0x00800000},
	{VTS_S_VOL_SET(3),			0x00800000},
	{VTS_S_VOL_CHANGE(0),			0x00000001},
	{VTS_S_VOL_CHANGE(1),			0x00000001},
	{VTS_S_VOL_CHANGE(2),			0x00000001},
	{VTS_S_VOL_CHANGE(3),			0x00000001},
#if (VTS_S_SOC_VERSION(1, 0, 0) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
#elif (VTS_S_SOC_VERSION(1, 1, 1) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	{VTS_S_CHANNEL_MAP_BASE,		0x76543210},
#elif (VTS_S_SOC_VERSION(1, 1, 2) >= CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	{VTS_S_CHANNEL_MAP_BASE,		0x76543210},
#else
#error "VTS_S_SOC_VERSION is not defined"
#endif
	{VTS_S_CONTROL_AHB_WMASTER_BASE,	0x00000000},

	{VTS_S_STARTADDR_SET0_BASE,		0x00000000},
	{VTS_S_ENDADDR_SET0_BASE,		0x00000000},
	{VTS_S_FILLED_SIZE_SET0_BASE,		0x00000000},
	{VTS_S_WRITE_POINTER_SET0_BASE,		0x00000000},
	{VTS_S_READ_POINTER_SET0_BASE,		0x00000000},

	{VTS_S_STARTADDR_SET1_BASE,		0x00000000},
	{VTS_S_ENDADDR_SET1_BASE,		0x00000000},
	{VTS_S_FILLED_SIZE_SET1_BASE,		0x00000000},
	{VTS_S_WRITE_POINTER_SET1_BASE,		0x00000000},
	{VTS_S_READ_POINTER_SET1_BASE,		0x00000000},
};

static bool readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case VTS_S_SFR_APB_BASE:
	case VTS_S_INT_EN_SET_BASE:
	case VTS_S_INT_EN_CLR_BASE:
	case VTS_S_INT_PEND_BASE:
	case VTS_S_CTRL_BASE:
	case VTS_S_CONFIG_MASTER_BASE:
	case VTS_S_CONFIG_SLAVE_BASE:
	case VTS_S_CONFIG_DONE_BASE:
	case VTS_S_SAMPLE_PCNT_BASE:
	case VTS_S_INPUT_EN_BASE:
	case VTS_S_VOL_SET(0):
	case VTS_S_VOL_SET(1):
	case VTS_S_VOL_SET(2):
	case VTS_S_VOL_SET(3):
	case VTS_S_VOL_CHANGE(0):
	case VTS_S_VOL_CHANGE(1):
	case VTS_S_VOL_CHANGE(2):
	case VTS_S_VOL_CHANGE(3):
#if (VTS_S_SOC_VERSION(1, 0, 0) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
#elif (VTS_S_SOC_VERSION(1, 1, 1) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	case VTS_S_CHANNEL_MAP_BASE:
#elif (VTS_S_SOC_VERSION(1, 1, 2) >= CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	case VTS_S_CHANNEL_MAP_BASE:
#else
#error "VTS_S_SOC_VERSION is not defined"
#endif
	case VTS_S_CONTROL_AHB_WMASTER_BASE:
	case VTS_S_STARTADDR_SET0_BASE:
	case VTS_S_ENDADDR_SET0_BASE:
	case VTS_S_FILLED_SIZE_SET0_BASE:
	case VTS_S_WRITE_POINTER_SET0_BASE:
	case VTS_S_READ_POINTER_SET0_BASE:
	case VTS_S_STARTADDR_SET1_BASE:
	case VTS_S_ENDADDR_SET1_BASE:
	case VTS_S_FILLED_SIZE_SET1_BASE:
	case VTS_S_WRITE_POINTER_SET1_BASE:
	case VTS_S_READ_POINTER_SET1_BASE:
		return true;
	default:
		return false;
	}
}

static bool writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case VTS_S_SFR_APB_BASE:
	case VTS_S_INT_EN_SET_BASE:
	case VTS_S_INT_EN_CLR_BASE:
	case VTS_S_INT_CLR_BASE:
	case VTS_S_CTRL_BASE:
	case VTS_S_CONFIG_MASTER_BASE:
	case VTS_S_CONFIG_SLAVE_BASE:
	case VTS_S_CONFIG_DONE_BASE:
	case VTS_S_SAMPLE_PCNT_BASE:
	case VTS_S_INPUT_EN_BASE:
	case VTS_S_VOL_SET(0):
	case VTS_S_VOL_SET(1):
	case VTS_S_VOL_SET(2):
	case VTS_S_VOL_SET(3):
	case VTS_S_VOL_CHANGE(0):
	case VTS_S_VOL_CHANGE(1):
	case VTS_S_VOL_CHANGE(2):
	case VTS_S_VOL_CHANGE(3):
#if (VTS_S_SOC_VERSION(1, 0, 0) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
#elif (VTS_S_SOC_VERSION(1, 1, 1) == CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	case VTS_S_CHANNEL_MAP_BASE:
#elif (VTS_S_SOC_VERSION(1, 1, 2) >= CONFIG_SND_SOC_SAMSUNG_VTS_S_LIF_VERSION)
	case VTS_S_CHANNEL_MAP_BASE:
#else
#error "VTS_S_SOC_VERSION is not defined"
#endif
	case VTS_S_CONTROL_AHB_WMASTER_BASE:
	case VTS_S_STARTADDR_SET0_BASE:
	case VTS_S_ENDADDR_SET0_BASE:
	case VTS_S_FILLED_SIZE_SET0_BASE:
	case VTS_S_WRITE_POINTER_SET0_BASE:
	case VTS_S_READ_POINTER_SET0_BASE:
	case VTS_S_STARTADDR_SET1_BASE:
	case VTS_S_ENDADDR_SET1_BASE:
	case VTS_S_FILLED_SIZE_SET1_BASE:
	case VTS_S_WRITE_POINTER_SET1_BASE:
	case VTS_S_READ_POINTER_SET1_BASE:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config vts_s_lif_component_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = VTS_S_LIF_MAX_REGISTER + 1,
	.num_reg_defaults_raw = VTS_S_LIF_MAX_REGISTER + 1,
	.readable_reg = readable_reg,
	.writeable_reg = writeable_reg,
	.cache_type = REGCACHE_RBTREE,
	.fast_io = true,
};

static const struct reg_default vts_s_lif_reg_dmic_aud_defaults[] = {
	{VTS_S_SFR_ENABLE_DMIC_AUD_BASE,		0x00000000},
	{VTS_S_SFR_CONTROL_DMIC_AUD_BASE,		0x84630000},
	{VTS_S_SFR_GAIN_MODE_BASE,			0x00001818},
	{VTS_S_SFR_MAX_SCALE_MODE_BASE,			0x00000000},
	{VTS_S_SFR_MAX_SCALE_GAIN_BASE,			0x00000028},
};

static bool readable_reg_dmic_aud(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case VTS_S_SFR_ENABLE_DMIC_AUD_BASE:
	case VTS_S_SFR_CONTROL_DMIC_AUD_BASE:
	case VTS_S_SFR_GAIN_MODE_BASE:
	case VTS_S_SFR_MAX_SCALE_MODE_BASE:
	case VTS_S_SFR_MAX_SCALE_GAIN_BASE:
		return true;
	default:
		return false;
	}
}

static bool writeable_reg_dmic_aud(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case VTS_S_SFR_ENABLE_DMIC_AUD_BASE:
	case VTS_S_SFR_CONTROL_DMIC_AUD_BASE:
	case VTS_S_SFR_GAIN_MODE_BASE:
	case VTS_S_SFR_MAX_SCALE_MODE_BASE:
	case VTS_S_SFR_MAX_SCALE_GAIN_BASE:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config vts_s_lif_regmap_dmic_aud_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = VTS_S_LIF_DMIC_AUD_MAX_REGISTER,
	.readable_reg = readable_reg_dmic_aud,
	.writeable_reg = writeable_reg_dmic_aud,
	.cache_type = REGCACHE_FLAT,
	.fast_io = true,
};

static int samsung_vts_s_lif_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct vts_s_lif_data *data;
	int ret = 0;

	slif_info(dev, "\n");
	data = devm_kzalloc(dev, sizeof(struct vts_s_lif_data), GFP_KERNEL);
	if (!data) {
		slif_err(dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto error;
	}

	platform_set_drvdata(pdev, data);
	data->dev = dev;
	data->pdev = pdev;
	dma_set_mask_and_coherent(dev, DMA_BIT_MASK(36));

	data->dev_vts = pdev->dev.parent;
	if (!data->dev_vts) {
		slif_err(dev, "Failed to get vts device\n");
		return -EPROBE_DEFER;
	}
	data->vts_data = dev_get_drvdata(data->dev_vts);

	data->sfr_base = vts_devm_get_request_ioremap(pdev, "sfr",
			NULL, NULL);
	if (IS_ERR(data->sfr_base)) {
		ret = PTR_ERR(data->sfr_base);
		goto error;
	}

	data->regmap_sfr = devm_regmap_init_mmio(dev,
			data->sfr_base,
			&vts_s_lif_component_regmap_config);

	data->sfr_sys_base = vts_devm_get_request_ioremap(pdev, "sys_sfr",
			NULL, NULL);
	if (IS_ERR(data->sfr_sys_base)) {
		ret = PTR_ERR(data->sfr_sys_base);
		goto error;
	}

	data->dmic_aud[0] = vts_devm_get_request_ioremap(pdev, "dmic_aud0",
			NULL, NULL);
	if (IS_ERR(data->dmic_aud[0])) {
		ret = PTR_ERR(data->dmic_aud[0]);
		goto error;
	}
	data->regmap_dmic_aud[0] = devm_regmap_init_mmio(dev,
			data->dmic_aud[0],
			&vts_s_lif_regmap_dmic_aud_config);

	data->dmic_aud[1] = vts_devm_get_request_ioremap(pdev, "dmic_aud1",
			NULL, NULL);
	if (IS_ERR(data->dmic_aud[1])) {
		ret = PTR_ERR(data->dmic_aud[1]);
		goto error;
	}
	data->regmap_dmic_aud[1] = devm_regmap_init_mmio(dev,
			data->dmic_aud[1],
			&vts_s_lif_regmap_dmic_aud_config);

	data->dmic_aud[2] = vts_devm_get_request_ioremap(pdev, "dmic_aud2",
			NULL, NULL);
	if (IS_ERR(data->dmic_aud[2])) {
		ret = PTR_ERR(data->dmic_aud[2]);
		goto error;
	}
	data->regmap_dmic_aud[2] = devm_regmap_init_mmio(dev,
			data->dmic_aud[2],
			&vts_s_lif_regmap_dmic_aud_config);

#ifdef VTS_S_LIF_REG_LOW_DUMP
	slif_info(dev, "CONFIG_MASTER: 0x%x \n", readl(data->sfr_base + 0x104));
	slif_info(dev, "CONFIG_SLAVE: 0x%x \n", readl(data->sfr_base + 0x108));
	slif_info(dev, "VOL_SET0: 0x%x \n", readl(data->sfr_base + 0x118));
	slif_info(dev, "VOL_CHANGE0: 0x%x \n", readl(data->sfr_base + 0x128));
#endif

	data->id = 0;
	snprintf(data->name, sizeof(data->name), "SERIAL_LIF%d", data->id);
	ret = vts_of_samsung_property_read_u32(dev, np, "max-div",
			&data->last_array[0]);
	if (ret < 0)
		data->last_array[0] = 32;

	ret = devm_snd_soc_register_component(dev, &vts_s_lif_component,
			vts_s_lif_dai, ARRAY_SIZE(vts_s_lif_dai));
	if (ret < 0) {
		slif_err(dev, "Failed to register ASoC component\n");
		goto error;
	}

	ret = vts_s_lif_soc_probe(data);
	if (ret < 0) {
		slif_err(dev, "vts_s_lif_soc_probe() Failed\n");
		goto error;
	}

	p_vts_s_lif_data = data;

	vts_s_lif_dump_init(dev);

	slif_info(dev, "Probed successfully\n");

	return ret;

error:
	return ret;
}

static int samsung_vts_s_lif_remove(struct platform_device *pdev)
{
	int ret = 0;

	return ret;
}

static int vts_s_lif_suspend(struct device *dev)
{
	int ret = 0;

	return ret;
}

static int vts_s_lif_resume(struct device *dev)
{
	int ret = 0;

	return ret;
}

static int vts_s_lif_runtime_suspend(struct device *dev)
{
	int ret = 0;

	return ret;
}

static int vts_s_lif_runtime_resume(struct device *dev)
{
	int ret = 0;

	return ret;
}

static const struct dev_pm_ops samsung_vts_s_lif_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(vts_s_lif_suspend, vts_s_lif_resume)
	SET_RUNTIME_PM_OPS(vts_s_lif_runtime_suspend,
			vts_s_lif_runtime_resume, NULL)
};

static const struct of_device_id samsung_vts_s_lif_of_match[] = {
	{
		.compatible = "samsung,vts-s-lif",
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_vts_s_lif_of_match);

struct platform_driver samsung_vts_s_lif_driver = {
	.probe  = samsung_vts_s_lif_probe,
	.remove = samsung_vts_s_lif_remove,
	.driver = {
		.name = "samsung-vts-s-lif",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_vts_s_lif_of_match),
		.pm = &samsung_vts_s_lif_pm,
	},
};
