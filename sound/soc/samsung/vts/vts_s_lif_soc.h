/* sound/soc/samsung/vts/vts_s_lif_soc.h
 *
 * ALSA SoC - Samsung VTS driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
  *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_VTS_S_LIF_SOC_H
#define __SND_SOC_VTS_S_LIF_SOC_H

#include <sound/memalloc.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/soc-dai.h>

#define VTS_S_LIF_PAD_ROUTE

int vts_s_lif_soc_set_fmt(struct vts_s_lif_data *dai, unsigned int fmt);

int vts_s_lif_soc_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params,
		struct vts_s_lif_data *data);

int vts_s_lif_soc_startup(struct snd_pcm_substream *substream,
		struct vts_s_lif_data *data);

void vts_s_lif_soc_shutdown(struct snd_pcm_substream *substream,
		struct vts_s_lif_data *data);

int vts_s_lif_soc_hw_free(struct snd_pcm_substream *substream,
		struct vts_s_lif_data *data);

int vts_s_lif_soc_dma_en(int cmd,
		struct vts_s_lif_data *data);

int vts_s_lif_soc_probe(struct vts_s_lif_data *data);

int vts_s_lif_soc_dmic_aud_gain_mode_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val);
int vts_s_lif_soc_dmic_aud_gain_mode_put(struct vts_s_lif_data *data,
		unsigned int id,unsigned int val);
int vts_s_lif_soc_dmic_aud_gain_mode_write(struct vts_s_lif_data *data,
		unsigned int id);

int vts_s_lif_soc_dmic_aud_max_scale_gain_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val);
int vts_s_lif_soc_dmic_aud_max_scale_gain_put(struct vts_s_lif_data *data,
		unsigned int id,unsigned int val);
int vts_s_lif_soc_dmic_aud_max_scale_gain_write(struct vts_s_lif_data *data,
		unsigned int id);

int vts_s_lif_soc_dmic_aud_control_gain_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val);
int vts_s_lif_soc_dmic_aud_control_gain_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val);
int vts_s_lif_soc_dmic_aud_control_gain_write(struct vts_s_lif_data *data,
		unsigned int id);

int vts_s_lif_soc_vol_set_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val);
int vts_s_lif_soc_vol_set_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val);

int vts_s_lif_soc_vol_change_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val);
int vts_s_lif_soc_vol_change_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val);

int vts_s_lif_soc_channel_map_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val);
int vts_s_lif_soc_channel_map_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val);

int vts_s_lif_soc_dmic_aud_control_hpf_sel_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val);
int vts_s_lif_soc_dmic_aud_control_hpf_sel_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val);

int vts_s_lif_soc_dmic_aud_control_hpf_en_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val);
int vts_s_lif_soc_dmic_aud_control_hpf_en_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val);

int vts_s_lif_soc_dmic_en_get(struct vts_s_lif_data *data,
		unsigned int id, unsigned int *val);
int vts_s_lif_soc_dmic_en_put(struct vts_s_lif_data *data,
		unsigned int id, unsigned int val, bool update);

void vts_s_lif_debug_pad_en(int en);

#endif /* __SND_SOC_VTS_S_LIF_SOC_H */
