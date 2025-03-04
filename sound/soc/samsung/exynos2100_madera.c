/*
 *  Driver for Madera CODECs on Exynos2100
 *
 *  Copyright 2013 Wolfson Microelectronics
 *  Copyright 2016 Cirrus Logic
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/debugfs.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/slab.h>

#include <sound/samsung/abox.h>

#if IS_ENABLED(CONFIG_SND_SOC_MADERA)
#include <linux/mfd/madera/core.h>
#include <linux/extcon/extcon-madera.h>
#include "../codecs/madera.h"
#endif

#define MADERA_BASECLK_48K	49152000
#define MADERA_BASECLK_44K1	45158400

#define MADERA_AMP_RATE	48000
#define MADERA_AMP_BCLK	(MADERA_AMP_RATE * 16 * 4)

#define CLK_SRC_SCLK 0
#define CLK_SRC_LRCLK 1
#define CLK_SRC_PDM 2
#define CLK_SRC_SELF 3
#define CLK_SRC_MCLK 4
#define CLK_SRC_SWIRE 5

#define CLK_SRC_DAI 0
#define CLK_SRC_CODEC 1

#define MADERA_DAI_ID			0x4793
#define CS35L41_DAI_ID			0x3541
#define ABOX_BE_DAI_ID(c, i)		(0xbe00 | (c) << 4 | (i))
#define MADERA_CODEC_MAX		32
#define MADERA_AUX_MAX			2
#define RDMA_COUNT			12
#define WDMA_COUNT			8
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_VTS)
#define VTS_COUNT			2
#else
#define VTS_COUNT			0
#endif
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_DISPLAYPORT)
#define DP_COUNT			2
#else
#define DP_COUNT			0
#endif
#define DDMA_COUNT			6
#define DUAL_COUNT			WDMA_COUNT
#define UAIF_START			(RDMA_COUNT + WDMA_COUNT + VTS_COUNT\
					+ DP_COUNT + DDMA_COUNT + DUAL_COUNT)
#define UAIF_COUNT			7

#define for_each_link_cpus(link, i, cpu)	\
	for ((i) = 0;				\
	     ((i) < link->num_cpus) &&		\
	     ((cpu) = &link->cpus[i]);		\
	     (i)++)

static unsigned int baserate = MADERA_BASECLK_48K;

enum FLL_ID { FLL1, FLL2, FLL3, FLLAO };
enum CLK_ID { SYSCLK, ASYNCCLK, DSPCLK, OPCLK, OUTCLK };

/* Debugfs value overrides, default to 0 */
static unsigned int forced_mclk1;
static unsigned int forced_sysclk;
static unsigned int forced_dspclk;

struct clk_conf {
	int id;
	const char *name;
	int source;
	int rate;
	int fout;

	bool valid;
};

#define MADERA_MAX_CLOCKS 10

struct madera_drvdata {
	struct device *dev;

	struct clk_conf fll1_refclk;
	struct clk_conf fll2_refclk;
	struct clk_conf fllao_refclk;
	struct clk_conf sysclk;
	struct clk_conf asyncclk;
	struct clk_conf dspclk;
	struct clk_conf opclk;
	struct clk_conf outclk;

	struct notifier_block nb;

	int left_amp_dai;
	int right_amp_dai;
	struct clk *clk[MADERA_MAX_CLOCKS];
};

static struct madera_drvdata exynos9820_drvdata;

static int map_fllid_with_name(const char *name)
{
	if (!strcmp(name, "fll1-refclk"))
		return FLL1;
	else if (!strcmp(name, "fll2-refclk"))
		return FLL2;
	else if (!strcmp(name, "fll3-refclk"))
		return FLL3;
	else if (!strcmp(name, "fllao-refclk"))
		return FLLAO;
	else
		return -1;
}

static int map_clkid_with_name(const char *name)
{
	if (!strcmp(name, "sysclk"))
		return SYSCLK;
	else if (!strcmp(name, "asyncclk"))
		return ASYNCCLK;
	else if (!strcmp(name, "dspclk"))
		return DSPCLK;
	else if (!strcmp(name, "opclk"))
		return OPCLK;
	else if (!strcmp(name, "outclk"))
		return OUTCLK;
	else
		return -1;
}

static struct snd_soc_pcm_runtime *get_rtd(struct snd_soc_card *card, int id)
{
	struct snd_soc_dai_link *dai_link;
	struct snd_soc_pcm_runtime *rtd = NULL;

	for (dai_link = card->dai_link;
			dai_link - card->dai_link < card->num_links;
			dai_link++) {
		if (id == dai_link->id) {
			rtd = snd_soc_get_pcm_runtime(card, dai_link->name);
			break;
		}
	}

	return rtd;
}

static int madera_start_fll(struct snd_soc_card *card,
				struct clk_conf *config)
{
	struct snd_soc_dai *codec_dai;
	struct snd_soc_component *codec;
	unsigned int fsrc = 0, fin = 0, fout = 0, pll_id;
	int ret;

	if (!config->valid)
		return 0;

	codec_dai = get_rtd(card, MADERA_DAI_ID)->codec_dai;
	codec = codec_dai->component;

	pll_id = map_fllid_with_name(config->name);
	switch (pll_id) {
	case FLL1:
		if (forced_mclk1) {
			/* use 32kHz input to avoid overclocking the FLL when
			 * forcing a specific MCLK frequency into the codec
			 * FLL calculations
			 */
#if IS_ENABLED(CONFIG_SND_SOC_MADERA)
			fsrc = MADERA_FLL_SRC_MCLK2;
#endif
			fin = forced_mclk1;
		} else {
			fsrc = config->source;
			fin = config->rate;
		}

		if (forced_sysclk)
			fout = forced_sysclk;
		else
			fout = config->fout;
		break;
	case FLL2:
	case FLLAO:
		fsrc = config->source;
		fin = config->rate;
		fout = config->fout;
		break;
	default:
		dev_err(card->dev, "Unknown FLLID for %s\n", config->name);
	}

	dev_dbg(card->dev, "Setting %s fsrc=%d fin=%uHz fout=%uHz\n",
		config->name, fsrc, fin, fout);

	ret = snd_soc_component_set_pll(codec, config->id, fsrc, fin, fout);
	if (ret)
		dev_err(card->dev, "Failed to start %s\n", config->name);

	return ret;
}

static int madera_stop_fll(struct snd_soc_card *card,
				struct clk_conf *config)
{
	struct snd_soc_dai *codec_dai;
	struct snd_soc_component *codec;
	int ret;

	if (!config->valid)
		return 0;

	codec_dai = get_rtd(card, MADERA_DAI_ID)->codec_dai;
	codec = codec_dai->component;

	ret = snd_soc_component_set_pll(codec, config->id, 0, 0, 0);
	if (ret)
		dev_err(card->dev, "Failed to stop %s\n", config->name);

	return ret;
}

static int madera_set_clock(struct snd_soc_card *card,
				struct clk_conf *config)
{
	struct snd_soc_dai *aif_dai;
	struct snd_soc_component *codec;
	unsigned int freq = 0, clk_id;
	int ret;
	int dir = SND_SOC_CLOCK_IN;

	if (!config->valid)
		return 0;

	aif_dai = get_rtd(card, MADERA_DAI_ID)->codec_dai;
	codec = aif_dai->component;

	clk_id = map_clkid_with_name(config->name);
	switch (clk_id) {
	case  SYSCLK:
		if (forced_sysclk)
			freq = forced_sysclk;
		else
			if (config->rate)
				freq = config->rate;
			else
				freq = baserate * 2;
		break;
	case ASYNCCLK:
		freq = config->rate;
		break;
	case DSPCLK:
		if (forced_dspclk)
			freq = forced_dspclk;
		else
			if (config->rate)
				freq = config->rate;
			else
				freq = baserate * 3;
		break;
	case OPCLK:
		freq = config->rate;
		dir = SND_SOC_CLOCK_OUT;
		break;
	case OUTCLK:
		freq = config->rate;
		break;
	default:
		dev_err(card->dev, "Unknown Clock ID for %s\n", config->name);
	}

	dev_dbg(card->dev, "Setting %s freq to %u Hz\n", config->name, freq);

	ret = snd_soc_component_set_sysclk(codec, config->id,
				       config->source, freq, dir);
	if (ret)
		dev_err(card->dev, "Failed to set %s to %u Hz\n",
			config->name, freq);

	return ret;
}

static int madera_stop_clock(struct snd_soc_card *card,
				struct clk_conf *config)
{
	struct snd_soc_dai *aif_dai;
	struct snd_soc_component *codec;
	int ret;

	if (!config->valid)
		return 0;

	aif_dai = get_rtd(card, MADERA_DAI_ID)->codec_dai;
	codec = aif_dai->component;

	ret = snd_soc_component_set_sysclk(codec, config->id, 0, 0, 0);
	if (ret)
		dev_err(card->dev, "Failed to stop %s\n", config->name);

	return ret;
}

static int madera_set_clocking(struct snd_soc_card *card,
				  struct madera_drvdata *drvdata)
{
	int ret;

	ret = madera_start_fll(card, &drvdata->fll1_refclk);
	if (ret)
		return ret;

	if (!drvdata->sysclk.rate) {
		ret = madera_set_clock(card, &drvdata->sysclk);
		if (ret)
			return ret;
	}

	if (!drvdata->dspclk.rate) {
		ret = madera_set_clock(card, &drvdata->dspclk);
		if (ret)
			return ret;
	}

	ret = madera_set_clock(card, &drvdata->opclk);
	if (ret)
		return ret;

	return ret;
}

static const struct snd_soc_ops rdma_ops = {
};

static const struct snd_soc_ops wdma_ops = {
};

static int madera_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct madera_drvdata *drvdata = card->drvdata;
	unsigned int rate = params_rate(params);
	int ret;

	/* Treat sysclk rate zero as automatic mode */
	if (!drvdata->sysclk.rate) {
		if (rate % 4000)
			baserate = MADERA_BASECLK_44K1;
		else
			baserate = MADERA_BASECLK_48K;
	}

	dev_dbg(card->dev, "Requesting Rate: %dHz, FLL: %dHz\n", rate,
		drvdata->sysclk.rate ? drvdata->sysclk.rate : baserate * 2);

	/* Ensure we can't race against set_bias_level */
	mutex_lock_nested(&card->dapm_mutex, SND_SOC_DAPM_CLASS_RUNTIME);
	ret = madera_set_clocking(card, drvdata);
	mutex_unlock(&card->dapm_mutex);

	return ret;
}

static const struct snd_soc_ops uaif0_ops = {
	.hw_params = madera_hw_params,
};

static int cs35l41_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai **codec_dais = rtd->codec_dais;
	unsigned int clk;
	unsigned int num_codecs = rtd->num_codecs;
	int ret = 0, i;

	/* using bclk for sysclk */
	clk = snd_soc_params_to_bclk(params);
	for (i = 0; i < num_codecs; i++) {
		ret = snd_soc_component_set_sysclk(codec_dais[i]->component,
					CLK_SRC_SCLK, 0, clk,
					SND_SOC_CLOCK_IN);
		if (ret < 0)
			dev_err(card->dev, "%s: set codec sysclk failed: %d\n",
					codec_dais[i]->name, ret);
	}

	return ret;
}

static const struct snd_soc_ops cs35l41_ops = {
	.hw_params = cs35l41_hw_params,
};

static const struct snd_soc_ops uaif_ops = {
};

static int dsif_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int tx_slot[] = {0, 1};

	/* bclk ratio 64 for DSD64, 128 for DSD128 */
	snd_soc_dai_set_bclk_ratio(cpu_dai, 64);

	/* channel map 0 1 if left is first, 1 0 if right is first */
	snd_soc_dai_set_channel_map(cpu_dai, 2, tx_slot, 0, NULL);
	return 0;
}

static const struct snd_soc_ops dsif_ops = {
	.hw_params = dsif_hw_params,
};

static const struct snd_soc_ops udma_ops = {
};

static int madera_set_bias_level(struct snd_soc_card *card,
				  struct snd_soc_dapm_context *dapm,
				  enum snd_soc_bias_level level)
{
	struct snd_soc_dai *codec_dai;
	struct madera_drvdata *drvdata = card->drvdata;
	int ret;

	codec_dai = get_rtd(card, MADERA_DAI_ID)->codec_dai;

	if (dapm->dev != codec_dai->dev)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_STANDBY:
		if (dapm->bias_level != SND_SOC_BIAS_OFF)
			break;

		ret = madera_set_clocking(card, drvdata);
		if (ret)
			return ret;

		ret = madera_start_fll(card, &drvdata->fll2_refclk);
		if (ret)
			return ret;

		ret = madera_start_fll(card, &drvdata->fllao_refclk);
		if (ret)
			return ret;
		break;
	default:
		break;
	}

	return 0;
}

static int madera_set_bias_level_post(struct snd_soc_card *card,
					 struct snd_soc_dapm_context *dapm,
					 enum snd_soc_bias_level level)
{
	struct snd_soc_dai *codec_dai;
	struct madera_drvdata *drvdata = card->drvdata;
	int ret;

	codec_dai = get_rtd(card, MADERA_DAI_ID)->codec_dai;

	if (dapm->dev != codec_dai->dev)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_OFF:
		ret = madera_stop_fll(card, &drvdata->fll1_refclk);
		if (ret)
			return ret;

		ret = madera_stop_fll(card, &drvdata->fll2_refclk);
		if (ret)
			return ret;

		ret = madera_stop_fll(card, &drvdata->fllao_refclk);
		if (ret)
			return ret;

		if (!drvdata->sysclk.rate) {
			ret = madera_stop_clock(card, &drvdata->sysclk);
			if (ret)
				return ret;
		}

		if (!drvdata->dspclk.rate) {
			ret = madera_stop_clock(card, &drvdata->dspclk);
			if (ret)
				return ret;
		}
		break;
	default:
		break;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_SND_SOC_MADERA)
/* Used for debugging and test automation */
static u32 voice_trigger_count;

static int madera_notify(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	const struct madera_hpdet_notify_data *hp_inf;
	const struct madera_micdet_notify_data *md_inf;
	const struct madera_voice_trigger_info *vt_inf;
	const struct madera_drvdata *drvdata =
		container_of(nb, struct madera_drvdata, nb);

	switch (event) {
	case MADERA_NOTIFY_VOICE_TRIGGER:
		vt_inf = data;
		dev_info(drvdata->dev, "Voice Triggered (core_num=%d)\n",
			 vt_inf->core_num);
		++voice_trigger_count;
		break;
	case MADERA_NOTIFY_HPDET:
		hp_inf = data;
		dev_info(drvdata->dev, "HPDET val=%d.%02d ohms\n",
			 hp_inf->impedance_x100 / 100,
			 hp_inf->impedance_x100 % 100);
		break;
	case MADERA_NOTIFY_MICDET:
		md_inf = data;
		dev_info(drvdata->dev, "MICDET present=%c val=%d.%02d ohms\n",
			 md_inf->present ? 'Y' : 'N',
			 md_inf->impedance_x100 / 100,
			 md_inf->impedance_x100 % 100);
		break;
	default:
		dev_info(drvdata->dev, "notifier event=0x%lx data=0x%p\n",
			 event, data);
		break;
	}

	return NOTIFY_DONE;
}
#else
static int madera_notify(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	return 0;
}

static int madera_register_notifier(struct snd_soc_component *component,
					struct notifier_block *nb)
{
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_DEBUG_FS)
static int madera_force_fll1_enable_write(void *data, u64 val)
{
	struct snd_soc_card *card = data;
	struct madera_drvdata *drvdata = card->drvdata;
	int ret;

	if (val == 0)
		ret = madera_stop_fll(card, &drvdata->fll1_refclk);
	else
		ret = madera_start_fll(card, &drvdata->fll1_refclk);

	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(madera_force_fll1_enable_fops, NULL,
			madera_force_fll1_enable_write, "%llu\n");

static void madera_init_debugfs(struct snd_soc_card *card)
{
	struct dentry *root;

	if (!card->debugfs_card_root) {
		dev_warn(card->dev, "No card debugfs root\n");
		return;
	}

	root = debugfs_create_dir("test-automation", card->debugfs_card_root);
	if (!root) {
		dev_warn(card->dev, "Failed to create debugfs dir\n");
		return;
	}
#if IS_ENABLED(CONFIG_SND_SOC_MADERA)
	debugfs_create_u32("voice_trigger_count", 0444, root,
			   &voice_trigger_count);
#endif
	debugfs_create_u32("forced_mclk1", 0664, root, &forced_mclk1);
	debugfs_create_u32("forced_sysclk", 0664, root, &forced_sysclk);
	debugfs_create_u32("forced_dspclk", 0664, root, &forced_dspclk);

	debugfs_create_file("force_fll1_enable", 0664, root, card,
			    &madera_force_fll1_enable_fops);
}
#else
static void madera_init_debugfs(struct snd_soc_card *card)
{
}
#endif

static int madera_amp_late_probe(struct snd_soc_card *card, int dai)
{
	struct madera_drvdata *drvdata = card->drvdata;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai *amp_dai;
	struct snd_soc_component *amp;
	int ret;

	if (!dai || !card->dai_link[dai].name)
		return 0;

	if (!drvdata->opclk.valid) {
		dev_err(card->dev, "OPCLK required to use speaker amp\n");
		return -ENOENT;
	}

	rtd = snd_soc_get_pcm_runtime(card, card->dai_link[dai].name);

	amp_dai = rtd->codec_dai;
	amp = amp_dai->component;

	ret = snd_soc_dai_set_tdm_slot(rtd->cpu_dai, 0x3, 0x3, 4, 16);
	if (ret)
		dev_err(card->dev, "Failed to set TDM: %d\n", ret);

	ret = snd_soc_component_set_sysclk(amp, 0, 0, drvdata->opclk.rate,
				       SND_SOC_CLOCK_IN);
	if (ret != 0) {
		dev_err(card->dev, "Failed to set amp SYSCLK: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(amp_dai, 0, MADERA_AMP_BCLK,
				     SND_SOC_CLOCK_IN);
	if (ret != 0) {
		dev_err(card->dev, "Failed to set amp DAI clock: %d\n", ret);
		return ret;
	}

	return 0;
}

static int exynos2100_late_probe(struct snd_soc_card *card)
{
	struct madera_drvdata *drvdata = card->drvdata;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai *aif_dai;
	struct snd_soc_dapm_context *dapm;
	struct snd_soc_dai_link *link;
	const char *name;
	int ret, i;

	aif_dai = get_rtd(card, MADERA_DAI_ID)->codec_dai;

	if (drvdata->sysclk.valid) {
		ret = snd_soc_dai_set_sysclk(aif_dai, drvdata->sysclk.id, 0, 0);
		if (ret != 0) {
			dev_err(drvdata->dev, "Failed to set AIF1 clock: %d\n",
				ret);
			return ret;
		}
	}

	if (drvdata->sysclk.rate) {
		ret = madera_set_clock(card, &drvdata->sysclk);
		if (ret)
			return ret;
	}

	if (drvdata->dspclk.rate) {
		ret = madera_set_clock(card, &drvdata->dspclk);
		if (ret)
			return ret;
	}

	ret = madera_set_clock(card, &drvdata->asyncclk);
	if (ret)
		return ret;

	ret = madera_set_clock(card, &drvdata->outclk);
	if (ret)
		return ret;

	ret = madera_amp_late_probe(card, drvdata->left_amp_dai);
	if (ret)
		return ret;

	ret = madera_amp_late_probe(card, drvdata->right_amp_dai);
	if (ret)
		return ret;

	dapm = &card->dapm;
	snd_soc_dapm_ignore_suspend(dapm, "VOUTPUT");
	snd_soc_dapm_ignore_suspend(dapm, "VINPUT1");
	snd_soc_dapm_ignore_suspend(dapm, "VINPUT2");
	snd_soc_dapm_ignore_suspend(dapm, "VOUTPUTCALL");
	snd_soc_dapm_ignore_suspend(dapm, "VINPUTCALL");
	snd_soc_dapm_ignore_suspend(dapm, "HEADSETMIC");
	snd_soc_dapm_ignore_suspend(dapm, "RECEIVER");
	snd_soc_dapm_ignore_suspend(dapm, "HEADPHONE");
	snd_soc_dapm_ignore_suspend(dapm, "SPEAKER");
	snd_soc_dapm_ignore_suspend(dapm, "BLUETOOTH MIC");
	snd_soc_dapm_ignore_suspend(dapm, "BLUETOOTH SPK");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC4");
	snd_soc_dapm_ignore_suspend(dapm, "VTS Virtual Output");
	snd_soc_dapm_sync(dapm);

	if (IS_ENABLED(CONFIG_SND_SOC_MADERA)) {
		struct snd_soc_component *codec;

		codec = aif_dai->component;

		dapm = snd_soc_component_get_dapm(codec);
		snd_soc_dapm_ignore_suspend(dapm, "AIF1 Playback");
		snd_soc_dapm_ignore_suspend(dapm, "AIF1 Capture");
		snd_soc_dapm_ignore_suspend(dapm, "AUXPDM1");
		snd_soc_dapm_sync(dapm);

		madera_init_debugfs(card);

		drvdata->nb.notifier_call = madera_notify;
		madera_register_notifier(codec, &drvdata->nb);
	}

	list_for_each_entry(link, &card->dai_link_list, list) {
		rtd = snd_soc_get_pcm_runtime(card, link->name);
		if (!rtd)
			continue;

		for (i = 0; i < rtd->num_codecs; i++) {
			aif_dai = rtd->cpu_dai;
			dapm = snd_soc_component_get_dapm(aif_dai->component);
			if (aif_dai->playback_widget) {
				name = aif_dai->playback_widget->name;
				dev_dbg(card->dev, "ignore suspend: %s\n",
						name);
				snd_soc_dapm_ignore_suspend(dapm, name);
				snd_soc_dapm_sync(dapm);
			}
			if (aif_dai->capture_widget) {
				name = aif_dai->capture_widget->name;
				dev_dbg(card->dev, "ignore suspend: %s\n",
						name);
				snd_soc_dapm_ignore_suspend(dapm, name);
				snd_soc_dapm_sync(dapm);
			}

			aif_dai = rtd->codec_dais[i];
			dapm = snd_soc_component_get_dapm(aif_dai->component);
			if (aif_dai->playback_widget) {
				name = aif_dai->playback_widget->name;
				dev_dbg(card->dev, "ignore suspend: %s\n",
						name);
				snd_soc_dapm_ignore_suspend(dapm, name);
				snd_soc_dapm_sync(dapm);
			}
			if (aif_dai->capture_widget) {
				name = aif_dai->capture_widget->name;
				dev_dbg(card->dev, "ignore suspend: %s\n",
						name);
				snd_soc_dapm_ignore_suspend(dapm, name);
				snd_soc_dapm_sync(dapm);
			}
		}
	}

	return 0;
}

static const struct snd_soc_dapm_widget exynos2100_supply_widgets[] = {
	SND_SOC_DAPM_REGULATOR_SUPPLY("MICBIAS1", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("MICBIAS2", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("MICBIAS3", 0, 0),
	SND_SOC_DAPM_REGULATOR_SUPPLY("MICBIAS4", 0, 0),
};

static int exynos2100_probe(struct snd_soc_card *card)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(exynos2100_supply_widgets); i++) {
		const struct snd_soc_dapm_widget *w;
		struct regulator *r;

		w = &exynos2100_supply_widgets[i];
		switch (w->id) {
		case snd_soc_dapm_regulator_supply:
			r = regulator_get(card->dev, w->name);
			if (!IS_ERR_OR_NULL(r)) {
				regulator_put(r);
				snd_soc_dapm_new_control(&card->dapm, w);
			}
			break;
		default:
			dev_warn(card->dev, "ignored: %s\n", w->name);
			break;
		}
	}

	return 0;
}

static struct snd_soc_pcm_stream madera_amp_params[] = {
	{
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.rate_min = MADERA_AMP_RATE,
		.rate_max = MADERA_AMP_RATE,
		.channels_min = 1,
		.channels_max = 1,
	},
};

#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_VTS)
SND_SOC_DAILINK_DEFS(dailink_vts_dma0,
	DAILINK_COMP_ARRAY(COMP_CPU("vts-tri")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("15510000.vts:vts_dma@0")));

SND_SOC_DAILINK_DEFS(dailink_vts_dma1,
	DAILINK_COMP_ARRAY(COMP_CPU("vts-rec")),
	DAILINK_COMP_ARRAY(COMP_CODEC("snd-soc-dummy", "snd-soc-dummy-dai")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("15510000.vts:vts_dma@1")));
#endif

static struct snd_soc_dai_link exynos2100_dai[100] = {
	{
		.name = "RDMA0",
		.stream_name = "RDMA0",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA1",
		.stream_name = "RDMA1",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA2",
		.stream_name = "RDMA2",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA3",
		.stream_name = "RDMA3",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA4",
		.stream_name = "RDMA4",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA5",
		.stream_name = "RDMA5",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA6",
		.stream_name = "RDMA6",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA7",
		.stream_name = "RDMA7",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA8",
		.stream_name = "RDMA8",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA9",
		.stream_name = "RDMA9",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA10",
		.stream_name = "RDMA10",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "RDMA11",
		.stream_name = "RDMA11",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "WDMA0",
		.stream_name = "WDMA0",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA1",
		.stream_name = "WDMA1",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA2",
		.stream_name = "WDMA2",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA3",
		.stream_name = "WDMA3",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA4",
		.stream_name = "WDMA4",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA5",
		.stream_name = "WDMA5",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA6",
		.stream_name = "WDMA6",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA7",
		.stream_name = "WDMA7",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_PRE
		},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
	},
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_VTS)
	{
		.name = "VTS-Trigger",
		.stream_name = "VTS-Trigger",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.capture_only = true,
		SND_SOC_DAILINK_REG(dailink_vts_dma0),
	},
	{
		.name = "VTS-Record",
		.stream_name = "VTS-Record",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.capture_only = true,
		SND_SOC_DAILINK_REG(dailink_vts_dma1),
	},
#endif
#if IS_ENABLED(CONFIG_SND_SOC_SAMSUNG_DISPLAYPORT)
	{
		.name = "DP0 Audio",
		.stream_name = "DP0 Audio",
		.ignore_suspend = 1,
	},
	{
		.name = "DP1 Audio",
		.stream_name = "DP1 Audio",
		.ignore_suspend = 1,
	},
#endif
	{
		.name = "WDMA0 DUAL",
		.stream_name = "WDMA0 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA1 DUAL",
		.stream_name = "WDMA1 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA2 DUAL",
		.stream_name = "WDMA2 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA3 DUAL",
		.stream_name = "WDMA3 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA4 DUAL",
		.stream_name = "WDMA4 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA5 DUAL",
		.stream_name = "WDMA5 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA6 DUAL",
		.stream_name = "WDMA6 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "WDMA7 DUAL",
		.stream_name = "WDMA7 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "DEBUG0",
		.stream_name = "DEBUG0",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "DEBUG1",
		.stream_name = "DEBUG1",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "DEBUG2",
		.stream_name = "DEBUG2",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "DEBUG3",
		.stream_name = "DEBUG3",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "DEBUG4",
		.stream_name = "DEBUG4",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "DEBUG5",
		.stream_name = "DEBUG5",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "UAIF0",
		.stream_name = "UAIF0",
		.id = MADERA_DAI_ID,
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif0_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "UAIF1",
		.stream_name = "UAIF1",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "UAIF2",
		.stream_name = "UAIF2",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "UAIF3",
		.stream_name = "UAIF3",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "UAIF4",
		.stream_name = "UAIF4",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "UAIF5",
		.stream_name = "UAIF5",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "UAIF6",
		.stream_name = "UAIF6",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "DSIF",
		.stream_name = "DSIF",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &dsif_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "UDMA RD0",
		.stream_name = "UDMA RD0",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &udma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "UDMA RD1",
		.stream_name = "UDMA RD1",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &udma_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "UDMA WR0",
		.stream_name = "UDMA WR0",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &udma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "UDMA WR1",
		.stream_name = "UDMA WR1",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &udma_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "UDMA WR0 DUAL",
		.stream_name = "UDMA WR0 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "UDMA WR1 DUAL",
		.stream_name = "UDMA WR1 DUAL",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "UDMA DEBUG0",
		.stream_name = "UDMA DEBUG0",
		.capture_only = 1,
		.ignore_suspend = 1,
	},
	{
		.name = "SIFS0",
		.stream_name = "SIFS0",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_capture = 1,
	},
	{
		.name = "SIFS1",
		.stream_name = "SIFS1",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_capture = 1,
	},
	{
		.name = "SIFS2",
		.stream_name = "SIFS2",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_capture = 1,
	},
	{
		.name = "SIFS3",
		.stream_name = "SIFS3",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_capture = 1,
	},
	{
		.name = "SIFS4",
		.stream_name = "SIFS4",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_capture = 1,
	},
	{
		.name = "SIFS5",
		.stream_name = "SIFS5",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_capture = 1,
	},
	{
		.name = "SIFS6",
		.stream_name = "SIFS6",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_capture = 1,
	},
	{
		.name = "NSRC0",
		.stream_name = "NSRC0",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "NSRC1",
		.stream_name = "NSRC1",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "NSRC2",
		.stream_name = "NSRC2",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "NSRC3",
		.stream_name = "NSRC3",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "NSRC4",
		.stream_name = "NSRC4",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "NSRC5",
		.stream_name = "NSRC5",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "NSRC6",
		.stream_name = "NSRC6",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "NSRC7",
		.stream_name = "NSRC7",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA0 BE",
		.stream_name = "RDMA0 BE",
		.id = ABOX_BE_DAI_ID(0, 0),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA1 BE",
		.stream_name = "RDMA1 BE",
		.id = ABOX_BE_DAI_ID(0, 1),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA2 BE",
		.stream_name = "RDMA2 BE",
		.id = ABOX_BE_DAI_ID(0, 2),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA3 BE",
		.stream_name = "RDMA3 BE",
		.id = ABOX_BE_DAI_ID(0, 3),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA4 BE",
		.stream_name = "RDMA4 BE",
		.id = ABOX_BE_DAI_ID(0, 4),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA5 BE",
		.stream_name = "RDMA5 BE",
		.id = ABOX_BE_DAI_ID(0, 5),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA6 BE",
		.stream_name = "RDMA6 BE",
		.id = ABOX_BE_DAI_ID(0, 6),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA7 BE",
		.stream_name = "RDMA7 BE",
		.id = ABOX_BE_DAI_ID(0, 7),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA8 BE",
		.stream_name = "RDMA8 BE",
		.id = ABOX_BE_DAI_ID(0, 8),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA9 BE",
		.stream_name = "RDMA9 BE",
		.id = ABOX_BE_DAI_ID(0, 9),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA10 BE",
		.stream_name = "RDMA10 BE",
		.id = ABOX_BE_DAI_ID(0, 10),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "RDMA11 BE",
		.stream_name = "RDMA11 BE",
		.id = ABOX_BE_DAI_ID(0, 11),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA0 BE",
		.stream_name = "WDMA0 BE",
		.id = ABOX_BE_DAI_ID(1, 0),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA1 BE",
		.stream_name = "WDMA1 BE",
		.id = ABOX_BE_DAI_ID(1, 1),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA2 BE",
		.stream_name = "WDMA2 BE",
		.id = ABOX_BE_DAI_ID(1, 2),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA3 BE",
		.stream_name = "WDMA3 BE",
		.id = ABOX_BE_DAI_ID(1, 3),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA4 BE",
		.stream_name = "WDMA4 BE",
		.id = ABOX_BE_DAI_ID(1, 4),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA5 BE",
		.stream_name = "WDMA5 BE",
		.id = ABOX_BE_DAI_ID(1, 5),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA6 BE",
		.stream_name = "WDMA6 BE",
		.id = ABOX_BE_DAI_ID(1, 6),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "WDMA7 BE",
		.stream_name = "WDMA7_BE",
		.id = ABOX_BE_DAI_ID(1, 7),
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "USB",
		.stream_name = "USB",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "FWD",
		.stream_name = "FWD",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
	},
};

static const char * const vts_output_texts[] = {
	"None",
	"DMIC1",
};

static const struct soc_enum vts_output_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(vts_output_texts),
			vts_output_texts);


static const struct snd_kcontrol_new vts_output_mux[] = {
	SOC_DAPM_ENUM("VTS Virtual Output Mux", vts_output_enum),
};

static const struct snd_kcontrol_new exynos2100_controls[] = {
	SOC_DAPM_PIN_SWITCH("DMIC1"),
	SOC_DAPM_PIN_SWITCH("DMIC2"),
	SOC_DAPM_PIN_SWITCH("DMIC3"),
	SOC_DAPM_PIN_SWITCH("DMIC4"),
	SOC_DAPM_PIN_SWITCH("SPEAKER"),
	SOC_DAPM_PIN_SWITCH("RECEIVER"),
};

static const struct snd_soc_dapm_widget exynos2100_widgets[] = {
	SND_SOC_DAPM_OUTPUT("VOUTPUT"),
	SND_SOC_DAPM_INPUT("VINPUT1"),
	SND_SOC_DAPM_INPUT("VINPUT2"),
	SND_SOC_DAPM_OUTPUT("VOUTPUTCALL"),
	SND_SOC_DAPM_INPUT("VINPUTCALL"),
	SND_SOC_DAPM_MIC("DMIC1", NULL),
	SND_SOC_DAPM_MIC("DMIC2", NULL),
	SND_SOC_DAPM_MIC("DMIC3", NULL),
	SND_SOC_DAPM_MIC("DMIC4", NULL),
	SND_SOC_DAPM_MIC("HEADSETMIC", NULL),
	SND_SOC_DAPM_SPK("RECEIVER", NULL),
	SND_SOC_DAPM_HP("HEADPHONE", NULL),
	SND_SOC_DAPM_SPK("SPEAKER", NULL),
	SND_SOC_DAPM_MIC("SPEAKER FB", NULL),
	SND_SOC_DAPM_MIC("BLUETOOTH MIC", NULL),
	SND_SOC_DAPM_SPK("BLUETOOTH SPK", NULL),
	SND_SOC_DAPM_MIC("USB MIC", NULL),
	SND_SOC_DAPM_SPK("USB SPK", NULL),
	SND_SOC_DAPM_MIC("FWD MIC", NULL),
	SND_SOC_DAPM_SPK("FWD SPK", NULL),
	SND_SOC_DAPM_OUTPUT("VTS Virtual Output"),
	SND_SOC_DAPM_MUX("VTS Virtual Output Mux", SND_SOC_NOPM, 0, 0,
			&vts_output_mux[0]),
};

static const struct snd_soc_dapm_route exynos2100_routes[] = {
	{"VTS Virtual Output Mux", "DMIC1", "DMIC1"},
};

static struct snd_soc_codec_conf codec_conf[MADERA_CODEC_MAX];

static struct snd_soc_aux_dev aux_dev[MADERA_AUX_MAX];

static struct snd_soc_card exynos2100_madera = {
	.name = "Exynos2100-Madera",
	.owner = THIS_MODULE,
	.dai_link = exynos2100_dai,
	.num_links = ARRAY_SIZE(exynos2100_dai),

	.probe = exynos2100_probe,
	.late_probe = exynos2100_late_probe,

	.controls = exynos2100_controls,
	.num_controls = ARRAY_SIZE(exynos2100_controls),
	.dapm_widgets = exynos2100_widgets,
	.num_dapm_widgets = ARRAY_SIZE(exynos2100_widgets),
	.dapm_routes = exynos2100_routes,
	.num_dapm_routes = ARRAY_SIZE(exynos2100_routes),

	.set_bias_level = madera_set_bias_level,
	.set_bias_level_post = madera_set_bias_level_post,

	.drvdata = (void *)&exynos9820_drvdata,

	.codec_conf = codec_conf,
	.num_configs = ARRAY_SIZE(codec_conf),

	.aux_dev = aux_dev,
	.num_aux_devs = ARRAY_SIZE(aux_dev),
};

static int read_clk_conf(struct device_node *np,
				   const char * const prop,
				   struct clk_conf *conf,
				   bool is_fll)
{
	u32 tmp;
	int ret;

	/*Truncate "cirrus," from prop_name to fetch clk_name*/
	conf->name = &prop[7];

	ret = of_property_read_u32_index(np, prop, 0, &tmp);
	if (ret)
		return ret;

	conf->id = tmp;

	ret = of_property_read_u32_index(np, prop, 1, &tmp);
	if (ret)
		return ret;

	if (tmp < 0xffff)
		conf->source = tmp;
	else
		conf->source = -1;

	ret = of_property_read_u32_index(np, prop, 2, &tmp);
	if (ret)
		return ret;

	conf->rate = tmp;

	if (is_fll) {
		ret = of_property_read_u32_index(np, prop, 3, &tmp);
		if (ret)
			return ret;
		conf->fout = tmp;
	}

	conf->valid = true;

	return 0;
}

static int read_platform(struct device_node *np, struct device *dev,
		struct snd_soc_dai_link *dai_link)
{
	int ret = 0;
	struct snd_soc_dai_link_component *platform;

	np = of_get_child_by_name(np, "platform");
	if (!np)
		return 0;

	platform = devm_kcalloc(dev, 1, sizeof(*platform), GFP_KERNEL);
	if (!platform) {
		ret = -ENOMEM;
		goto out;
	}

	platform->of_node = of_parse_phandle(np, "sound-dai", 0);
	if (!platform->of_node) {
		ret = -ENODEV;
		goto out;
	}
out:
	if (ret < 0) {
		if (platform)
			devm_kfree(dev, platform);
	} else {
		dai_link->platforms = platform;
		dai_link->num_platforms = 1;
	}
	of_node_put(np);

	return ret;
}

static int read_cpu(struct device_node *np, struct device *dev,
		struct snd_soc_dai_link *dai_link)
{
	int ret = 0;
	struct snd_soc_dai_link_component *cpu;

	np = of_get_child_by_name(np, "cpu");
	if (!np)
		return 0;

	cpu = devm_kcalloc(dev, 1, sizeof(*cpu), GFP_KERNEL);
	if (!cpu) {
		ret = -ENOMEM;
		goto out;
	}

	cpu->of_node = of_parse_phandle(np, "sound-dai", 0);
	if (!cpu->of_node) {
		ret = -ENODEV;
		goto out;
	}

	ret = snd_soc_of_get_dai_name(np, &cpu->dai_name);
out:
	if (ret < 0) {
		if (cpu)
			devm_kfree(dev, cpu);
	} else {
		dai_link->cpus = cpu;
		dai_link->num_cpus = 1;
	}
	of_node_put(np);

	return ret;
}

SND_SOC_DAILINK_DEF(dailink_comp_dummy, DAILINK_COMP_ARRAY(COMP_DUMMY()));

static int read_codec(struct device_node *np, struct device *dev,
		struct snd_soc_dai_link *dai_link)
{
	int ret;

	np = of_get_child_by_name(np, "codec");
	if (!np) {
		dai_link->codecs = dailink_comp_dummy;
		dai_link->num_codecs = ARRAY_SIZE(dailink_comp_dummy);
		return 0;
	}

	ret = snd_soc_of_get_dai_link_codecs(dev, np, dai_link);
	of_node_put(np);

	return ret;
}

static void exynos2100_register_card_work_func(struct work_struct *work)
{
	struct snd_soc_card *card = &exynos2100_madera;
	int ret;

	ret = devm_snd_soc_register_card(card->dev, card);
	if (ret)
		dev_err(card->dev, "sound card register failed: %d\n", ret);
}
DECLARE_WORK(exynos2100_register_card_work, exynos2100_register_card_work_func);

static int exynos2100_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &exynos2100_madera;
	struct madera_drvdata *drvdata = card->drvdata;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *dai;
	struct snd_soc_dai_link *link;
	int nlink = 0;
	int i, ret;
	const char *cur = NULL;
	struct property *p;

	card->dev = &pdev->dev;
	drvdata->dev = card->dev;

	snd_soc_card_set_drvdata(card, drvdata);

	i = 0;
	p = of_find_property(np, "clock-names", NULL);
	if (p) {
		while ((cur = of_prop_next_string(p, cur)) != NULL) {
			drvdata->clk[i] = devm_clk_get(drvdata->dev, cur);
			if (IS_ERR(drvdata->clk[i])) {
				dev_info(drvdata->dev, "Failed to get %s: %ld\n",
					 cur, PTR_ERR(drvdata->clk[i]));
				drvdata->clk[i] = NULL;
				break;
			}

			clk_prepare_enable(drvdata->clk[i]);

			if (++i == MADERA_MAX_CLOCKS)
				break;
		}
	}

	ret = read_clk_conf(np, "cirrus,sysclk",
				      &drvdata->sysclk, false);
	if (ret)
		dev_dbg(card->dev, "Failed to parse sysclk: %d\n", ret);
	ret = read_clk_conf(np, "cirrus,asyncclk",
				      &drvdata->asyncclk, false);
	if (ret)
		dev_dbg(card->dev, "Failed to parse asyncclk: %d\n", ret);

	ret = read_clk_conf(np, "cirrus,dspclk",
				      &drvdata->dspclk, false);
	if (ret)
		dev_dbg(card->dev, "Failed to parse dspclk: %d\n", ret);

	ret = read_clk_conf(np, "cirrus,opclk",
				      &drvdata->opclk, false);
	if (ret)
		dev_dbg(card->dev, "Failed to parse opclk: %d\n", ret);

	ret = read_clk_conf(np, "cirrus,fll1-refclk",
				      &drvdata->fll1_refclk, true);
	if (ret)
		dev_dbg(card->dev, "Failed to parse fll1-refclk: %d\n", ret);

	ret = read_clk_conf(np, "cirrus,fll2-refclk",
				      &drvdata->fll2_refclk, true);
	if (ret)
		dev_dbg(card->dev, "Failed to parse fll2-refclk: %d\n", ret);

	ret = read_clk_conf(np, "cirrus,fllao-refclk",
				      &drvdata->fllao_refclk, true);
	if (ret)
		dev_dbg(card->dev, "Failed to parse fllao-refclk: %d\n", ret);

	ret = read_clk_conf(np, "cirrus,outclk",
				      &drvdata->outclk, false);
	if (ret)
		dev_dbg(card->dev, "Failed to parse outclk: %d\n", ret);

	for_each_child_of_node(np, dai) {
		link = &exynos2100_dai[nlink];

		if (!link->name)
			link->name = dai->name;
		if (!link->stream_name)
			link->stream_name = dai->name;

		if (!link->cpus) {
			ret = read_cpu(dai, card->dev, link);
			if (ret < 0) {
				dev_err(card->dev, "Failed to parse cpu DAI for %s: %d\n",
						dai->name, ret);
				return ret;
			}
		}

		if (!link->platforms) {
			ret = read_platform(dai, card->dev, link);
			if (ret < 0) {
				dev_warn(card->dev, "Failed to parse platform for %s: %d\n",
						dai->name, ret);
				ret = 0;
			}
		}

		if (!link->codecs) {
			ret = read_codec(dai, card->dev, link);
			if (ret < 0) {
				dev_warn(card->dev, "Failed to parse codec DAI for %s: %d\n",
						dai->name, ret);
				ret = 0;
			}

			if (link->codecs && strstr(link->codecs[0].dai_name,
					"cs35l41"))
				link->ops = &cs35l41_ops;
		}

		if (strstr(dai->name, "left-amp")) {
			link->params = madera_amp_params;
			drvdata->left_amp_dai = nlink;
		} else if (strstr(dai->name, "right-amp")) {
			link->params = madera_amp_params;
			drvdata->right_amp_dai = nlink;
		}

		link->dai_fmt = snd_soc_of_parse_daifmt(dai, NULL, NULL, NULL);

		if (++nlink == card->num_links)
			break;
	}

	if (!nlink) {
		dev_err(card->dev, "No DAIs specified\n");
		return -EINVAL;
	}

	/* Dummy pcm to adjust ID of PCM added by topology */
	for (; nlink < card->num_links; nlink++) {
		link = &exynos2100_dai[nlink];

		if (!link->name)
			link->name = devm_kasprintf(card->dev, GFP_KERNEL,
					"dummy%d", nlink);
		if (!link->stream_name)
			link->stream_name = link->name;

		if (!link->cpus) {
			link->cpus = dailink_comp_dummy;
			link->num_cpus = ARRAY_SIZE(dailink_comp_dummy);
		}

		if (!link->codecs) {
			link->codecs = dailink_comp_dummy;
			link->num_codecs = ARRAY_SIZE(dailink_comp_dummy);
		}

		link->no_pcm = 1;
		link->ignore_suspend = 1;
	}

	if (of_property_read_bool(np, "samsung,routing")) {
		ret = snd_soc_of_parse_audio_routing(card, "samsung,routing");
		if (ret)
			return ret;
	}

	for (i = 0; i < ARRAY_SIZE(codec_conf); i++) {
		codec_conf[i].of_node = of_parse_phandle(np, "samsung,codec",
				i);
		if (!codec_conf[i].of_node)
			break;

		ret = of_property_read_string_index(np, "samsung,prefix", i,
				&codec_conf[i].name_prefix);
		if (ret < 0)
			codec_conf[i].name_prefix = "";
	}
	card->num_configs = i;

	for (i = 0; i < ARRAY_SIZE(aux_dev); i++) {
		aux_dev[i].dlc.of_node = of_parse_phandle(np, "samsung,aux", i);
		if (!aux_dev[i].dlc.of_node)
			break;
	}
	card->num_aux_devs = i;

	schedule_work(&exynos2100_register_card_work);

	return ret;
}

static int exynos2100_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct madera_drvdata *drvdata;
	struct snd_soc_dai_link *dai_link;
	struct snd_soc_dai_link_component *cpu, *platform;
	int i;

	card = platform_get_drvdata(pdev);
	if (!card)
		return 0;

	drvdata = snd_soc_card_get_drvdata(card);
	if (!drvdata)
		return 0;

	for (dai_link = exynos2100_dai; dai_link - exynos2100_dai <
			ARRAY_SIZE(exynos2100_dai); dai_link++) {
		for_each_link_cpus(dai_link, i, cpu) {
			if (cpu->of_node)
				of_node_put(cpu->of_node);
		}

		for_each_link_platforms(dai_link, i, platform) {
			if (platform->of_node)
				of_node_put(platform->of_node);
		}

		snd_soc_of_put_dai_link_codecs(dai_link);
	}

	for (i = 0; i < MADERA_MAX_CLOCKS; ++i)
		if (drvdata->clk[i])
			clk_disable_unprepare(drvdata->clk[i]);

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id exynos2100_audio_of_match[] = {
	{ .compatible = "samsung,exynos2100-madera", },
	{},
};
MODULE_DEVICE_TABLE(of, exynos2100_audio_of_match);
#endif /* CONFIG_OF */

static struct platform_driver exynos2100_audio_driver = {
	.driver		= {
		.name	= "exynos2100-audio",
		.owner	= THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(exynos2100_audio_of_match),
	},

	.probe		= exynos2100_audio_probe,
	.remove		= exynos2100_audio_remove,
};

module_platform_driver(exynos2100_audio_driver);

MODULE_DESCRIPTION("ALSA SoC Exynos2100 Audio Driver");
MODULE_AUTHOR("Charles Keepax <ckeepax@opensource.wolfsonmicro.com>");
MODULE_AUTHOR("Gyeongtaek Lee <gt82.lee@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:exynos2100-madera");
