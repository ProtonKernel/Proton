/* sound/soc/samsung/abox/abox_tune.h
 *
 * ALSA SoC - Samsung Abox ASoC Audio Tuning Block driver
 *
 * Copyright (c) 2020 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_ABOX_ATUNE_H
#define __SND_SOC_ABOX_ATUNE_H


#define SPUS_BASE			0x0000
#define SPUM_BASE			0x1000

#define DSGAIN_BASE			0x0000
#define DSGAIN_ITV			0x40
#define DSGAIN_CTRL			0x0000
#define DSGAIN_VOL_CHANGE_FIN		0x0008
#define DSGAIN_VOL_CHANGE_FOUT		0x000c
#define DSGAIN_GAIN0			0x0010
#define DSGAIN_GAIN1			0x0014
#define DSGAIN_GAIN2			0x0018
#define DSGAIN_GAIN3			0x001c
#define DSGAIN_BIT_CTRL			0x0020

#define USGAIN_BASE			0x0080
#define USGAIN_ITV			0x40
#define USGAIN_CTRL			0x0000

/**
 * Probe atune widget and controls
 * @param[in]	data	pointer to abox_data structure
 * @return	0 or error code
 */
extern int abox_atune_probe(struct abox_data *data);

/**
 * Test whether the posttune is connected
 * @param[in]	data	pointer to abox_data structure
 * @param[in]	id	id of spus
 * @return	if it's connected true, otherwise false.
 */
extern bool abox_atune_spus_posttune_connected(struct abox_data *data, int id);

/**
 * Test whether the pretune is connected
 * @param[in]	data	pointer to abox_data structure
 * @param[in]	id	id of spum
 * @return	if it's connected true, otherwise false.
 */
extern bool abox_atune_spum_pretune_connected(struct abox_data *data, int id);

/**
 * Synchronize format and connect atune with spus or spum
 * @param[in]	data	pointer to abox_data structure
 * @param[in]	e	dapm event flag
 * @param[in]	stream	SNDRV_PCM_STREAM_PLAYBACK or SNDRV_PCM_STREAM_CAPTURE
 * @param[in]	sid	spus or spum id
 * @return	0 or error code
 */
extern int abox_atune_dapm_event(struct abox_data *data, int e, int stream,
		int sid);

#endif /* __SND_SOC_ABOX_ATUNE_H */
