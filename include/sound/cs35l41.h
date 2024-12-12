/*
 * linux/sound/cs35l41.h -- Platform data for CS35L41
 *
 * Copyright (c) 2016 Cirrus Logic Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __CS35L41_H
#define __CS35L41_H

#include <sound/soc.h>

struct classh_cfg {
	bool classh_bst_override;
	bool classh_algo_enable;
	int classh_bst_max_limit;
	int classh_mem_depth;
	int classh_release_rate;
	int classh_headroom;
	int classh_wk_fet_delay;
	int classh_wk_fet_thld;
};

struct cs35l41_irq_cfg {
	bool is_present;
	bool irq_pol_inv;
	bool irq_out_en;
	int irq_src_sel;
};

struct cs35l41_platform_data {
	bool sclk_frc;
	bool lrclk_frc;
	bool right_channel;
	bool amp_gain_zc;
	bool dsp_ng_enable;
	bool inv_pcm;
	bool use_fsync_errata;
	int bst_ind;
	int bst_vctrl;
	int bst_ipk;
	int temp_warn_thld;
	int dsp_ng_pcm_thld;
	int dsp_ng_delay;
	int dout_hiz;
	unsigned int hw_ng_sel;
	unsigned int hw_ng_delay;
	unsigned int hw_ng_thld;
	unsigned int fixed_rate;
	unsigned int fixed_width;
	unsigned int fixed_wl;
	bool fixed_params;
	const char *dsp_part_name;
	const char *mfd_suffix;
	struct cs35l41_irq_cfg irq_config1;
	struct cs35l41_irq_cfg irq_config2;
	struct classh_cfg classh_config;
};

struct cs35l41_private {
	struct wm_adsp dsp; /* needs to be first member */
	struct snd_soc_codec *codec;
	struct cs35l41_platform_data pdata;
	struct device *dev;
	struct regmap *regmap;
	struct regulator_bulk_data supplies[2];
	int num_supplies;
	int irq;
	int clksrc;
	int extclk_freq;
	int extclk_cfg;
	int sclk;
	int amp_mute;
	int pcm_vol;
	int pcm_active;
	int dsprx2_src;
	int lrclk_fmt;
	int sclk_fmt;
	int pcm_source_last;
	int pll_freq_last;
	unsigned int spk_3_trim;
	unsigned int spk_4_trim;
	bool dspa_mode;
	bool i2s_mode;
	bool swire_mode;
	bool halo_booted;
	bool halo_routed;
	bool halo_played;
	/* GPIO for /RST */
	struct gpio_desc *reset_gpio;
	struct completion global_pup_done;
	struct completion global_pdn_done;
	struct completion mbox_cmd;
	struct mutex rate_lock;
};

int cs35l41_reinit(struct snd_soc_component *component);

int cs35l41_probe(struct cs35l41_private *cs35l41);
int cs35l41_remove(struct cs35l41_private *cs35l41);

#endif /* __CS35L41_H */
