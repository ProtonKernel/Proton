/* sound/soc/samsung/abox/abox_tune.c
 *
 * ALSA SoC Audio Layer - Samsung Abox Audio Tuning Block driver
 *
 * Copyright (c) 2020 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <sound/tlv.h>

#include "abox.h"
#include "abox_cmpnt.h"
#include "abox_dma.h"
#include "abox_atune.h"
#include "abox_memlog.h"

#define VERBOSE 1
#undef GAIN_CONTROL
#define COUNT_ATUNE 2
#define COUNT_CH 8
#define COUNT_EQ 5

#ifdef GAIN_CONTROL
struct atune_gain_control {
	unsigned int base;
	unsigned int count;
	unsigned int min;
	unsigned int max;
};

static int atune_gain_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	struct snd_soc_component *cmpnt = snd_kcontrol_chip(kcontrol);
	struct device *dev = cmpnt->dev;
	struct atune_gain_control *params = (void *)kcontrol->private_value;

	abox_dbg(dev, "%s: %s\n", __func__, kcontrol->id.name);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = params->count;
	uinfo->value.integer.min = params->min;
	uinfo->value.integer.max = params->max;
	return 0;
}

static int atune_gain_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct atune_gain_control *params = (void *)kcontrol->private_value;
	unsigned int i;
	int ret = 0;

	abox_dbg(dev, "%s: %s\n", __func__, kcontrol->id.name);

	for (i = 0; i < params->count; i++) {
		unsigned int reg, val;
		int index, shift, db;

		reg = params->base + (i / ATUNE_FLD_PER_SFR(CH_GAIN));
		ret = snd_soc_component_read(cmpnt, reg, &val);
		if (ret < 0) {
			abox_err(dev, "reading fail at %#x\n", reg);
			break;
		}
		shift = (int)(val & ATUNE_CH_GAIN_SHIFT_MASK(i)) >>
				ATUNE_CH_GAIN_SHIFT_L(i);
		index = (int)(val & ATUNE_CH_GAIN_INDEX_MASK(i)) >>
				ATUNE_CH_GAIN_INDEX_L(i);
		db = shift * 6 + (index ? (index - 6) : 0);
		if (VERBOSE)
			abox_dbg(dev, "%s: shift:%d, index:%d, db:%d\n",
					__func__, shift, index, db);
		ucontrol->value.integer.value[i] = db;
		abox_dbg(dev, "%s: %#x => %#x\n", kcontrol->id.name, reg, val);
	}

	return ret;
}

static int atune_gain_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct atune_gain_control *params = (void *)kcontrol->private_value;
	unsigned int i;

	abox_dbg(dev, "%s: %s\n", __func__, kcontrol->id.name);

	for (i = 0; i < params->count; i++) {
		unsigned int reg, val;
		int index, shift, db;

		db = (int)ucontrol->value.integer.value[i];
		shift = db / 6;
		index = db - (shift * 6);
		index = index ? (index + 6) : 0;
		if (VERBOSE)
			abox_dbg(dev, "%s: shift:%d, index:%d, db:%d\n",
					__func__, shift, index, db);
		reg = params->base + (i / ATUNE_FLD_PER_SFR(CH_GAIN));
		val = (shift << ATUNE_CH_GAIN_SHIFT_L(i)) |
				(index << ATUNE_CH_GAIN_INDEX_L(i));
		snd_soc_component_write(cmpnt, reg, val);
		abox_dbg(dev, "%s: %#x <= %#x\n", kcontrol->id.name, reg, val);
	}

	return 0;
}

#define ATUNE_GAIN_CONTROL(xname, xbase, xcount, xmin, xmax)	\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname),	\
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,		\
	.info = atune_gain_info, .get = atune_gain_get,		\
	.put = atune_gain_put, .private_value =			\
		((unsigned long)&(struct atune_gain_control)	\
		{.base = xbase, .count = xcount,		\
		.min = xmin, .max = xmax}) }
#endif

#define SOC_SINGLE_S_TLV(xname, reg, xshift, xmin, xmax, xsign_bit, \
	xinvert, tlv_array) \
	SOC_DOUBLE_R_S_TLV(xname, reg, reg, xshift, xmin, xmax, xsign_bit, \
		xinvert, tlv_array)

static const char * const gain_func_texts[] = {
	"MUTE", "NORMAL", "FADE IN", "FADE OUT",
};

static SOC_ENUM_SINGLE_DECL(spus0_pregain_func_enum, ATUNE_SPUS_USGAIN_CTRL(0),
		ATUNE_FUNC_L, gain_func_texts);
static SOC_ENUM_SINGLE_DECL(spus1_pregain_func_enum, ATUNE_SPUS_USGAIN_CTRL(1),
		ATUNE_FUNC_L, gain_func_texts);
static SOC_ENUM_SINGLE_DECL(spus0_postgain_func_enum, ATUNE_SPUS_DSGAIN_CTRL(0),
		ATUNE_FUNC_L, gain_func_texts);
static SOC_ENUM_SINGLE_DECL(spus1_postgain_func_enum, ATUNE_SPUS_DSGAIN_CTRL(1),
		ATUNE_FUNC_L, gain_func_texts);
static SOC_ENUM_SINGLE_DECL(spum0_pregain_func_enum, ATUNE_SPUM_USGAIN_CTRL(0),
		ATUNE_FUNC_L, gain_func_texts);
static SOC_ENUM_SINGLE_DECL(spum1_pregain_func_enum, ATUNE_SPUM_USGAIN_CTRL(1),
		ATUNE_FUNC_L, gain_func_texts);
static SOC_ENUM_SINGLE_DECL(spum0_postgain_func_enum, ATUNE_SPUM_DSGAIN_CTRL(0),
		ATUNE_FUNC_L, gain_func_texts);
static SOC_ENUM_SINGLE_DECL(spum1_postgain_func_enum, ATUNE_SPUM_DSGAIN_CTRL(1),
		ATUNE_FUNC_L, gain_func_texts);

static const DECLARE_TLV_DB_SCALE(atune_gain_shift_tlv, -19200, 600, 0);
static const DECLARE_TLV_DB_RANGE(atune_gain_index_tlv,
	0x0, 0x0, TLV_DB_SCALE_ITEM(0, 0, 0),
	0x1, 0x5, TLV_DB_SCALE_ITEM(-500, 100, 0),
);

#define ATUNE_SPUS_PREGAIN_CH_CONTROLS(x, ch)				\
	SOC_SINGLE_S_TLV("SPUS"#x" PREGAIN CH"#ch" Main Volume",	\
			ATUNE_SPUS_USGAIN_GAIN_CH(x, ch),		\
			ATUNE_CH_GAIN_SHIFT_L(ch),			\
			-32, 20, 5, 0, atune_gain_shift_tlv),		\
	SOC_SINGLE_S_TLV("SPUS"#x" PREGAIN CH"#ch" Fine Volume",	\
			ATUNE_SPUS_USGAIN_GAIN_CH(x, ch),		\
			ATUNE_CH_GAIN_INDEX_L(ch),			\
			0, 5, 2, 0, atune_gain_index_tlv)

#define ATUNE_SPUS_PREGAIN_CONTROLS(x)		\
	ATUNE_SPUS_PREGAIN_CH_CONTROLS(x, 0),	\
	ATUNE_SPUS_PREGAIN_CH_CONTROLS(x, 1),	\
	ATUNE_SPUS_PREGAIN_CH_CONTROLS(x, 2),	\
	ATUNE_SPUS_PREGAIN_CH_CONTROLS(x, 3),	\
	ATUNE_SPUS_PREGAIN_CH_CONTROLS(x, 4),	\
	ATUNE_SPUS_PREGAIN_CH_CONTROLS(x, 5),	\
	ATUNE_SPUS_PREGAIN_CH_CONTROLS(x, 6),	\
	ATUNE_SPUS_PREGAIN_CH_CONTROLS(x, 7)

#define ATUNE_SPUS_POSTGAIN_CH_CONTROLS(x, ch)				\
	SOC_SINGLE_S_TLV("SPUS"#x" POSTGAIN CH"#ch" Main Volume",	\
			ATUNE_SPUS_DSGAIN_GAIN_CH(x, ch),		\
			ATUNE_CH_GAIN_SHIFT_L(ch),			\
			-32, 20, 5, 0, atune_gain_shift_tlv),		\
	SOC_SINGLE_S_TLV("SPUS"#x" POSTGAIN CH"#ch" Fine Volume",	\
			ATUNE_SPUS_DSGAIN_GAIN_CH(x, ch),		\
			ATUNE_CH_GAIN_INDEX_L(ch),			\
			0, 5, 2, 0, atune_gain_index_tlv)

#define ATUNE_SPUS_POSTGAIN_CONTROLS(x)		\
	ATUNE_SPUS_POSTGAIN_CH_CONTROLS(x, 0),	\
	ATUNE_SPUS_POSTGAIN_CH_CONTROLS(x, 1),	\
	ATUNE_SPUS_POSTGAIN_CH_CONTROLS(x, 2),	\
	ATUNE_SPUS_POSTGAIN_CH_CONTROLS(x, 3),	\
	ATUNE_SPUS_POSTGAIN_CH_CONTROLS(x, 4),	\
	ATUNE_SPUS_POSTGAIN_CH_CONTROLS(x, 5),	\
	ATUNE_SPUS_POSTGAIN_CH_CONTROLS(x, 6),	\
	ATUNE_SPUS_POSTGAIN_CH_CONTROLS(x, 7)

static const char * const dither_type_texts[] = {
	"OFF", "RPDF", "TPDF",
};

static SOC_ENUM_SINGLE_DECL(spus0_postgain_dither_type_enum,
		ATUNE_SPUS_DSGAIN_BIT_CTRL(0), ATUNE_DITHER_TYPE_L,
		dither_type_texts);
static SOC_ENUM_SINGLE_DECL(spus1_postgain_dither_type_enum,
		ATUNE_SPUS_DSGAIN_BIT_CTRL(1), ATUNE_DITHER_TYPE_L,
		dither_type_texts);
static SOC_ENUM_SINGLE_DECL(spum0_postgain_dither_type_enum,
		ATUNE_SPUM_DSGAIN_BIT_CTRL(0), ATUNE_DITHER_TYPE_L,
		dither_type_texts);
static SOC_ENUM_SINGLE_DECL(spum1_postgain_dither_type_enum,
		ATUNE_SPUM_DSGAIN_BIT_CTRL(1), ATUNE_DITHER_TYPE_L,
		dither_type_texts);

#define ATUNE_SPUM_PREGAIN_CH_CONTROLS(x, ch)				\
	SOC_SINGLE_S_TLV("SPUM"#x" PREGAIN CH"#ch" Main Volume",	\
			ATUNE_SPUM_USGAIN_GAIN_CH(x, ch),		\
			ATUNE_CH_GAIN_SHIFT_L(ch),			\
			-32, 20, 5, 0, atune_gain_shift_tlv),		\
	SOC_SINGLE_S_TLV("SPUM"#x" PREGAIN CH"#ch" Fine Volume",	\
			ATUNE_SPUM_USGAIN_GAIN_CH(x, ch),		\
			ATUNE_CH_GAIN_INDEX_L(ch),			\
			0, 5, 2, 0, atune_gain_index_tlv)

#define ATUNE_SPUM_PREGAIN_CONTROLS(x)		\
	ATUNE_SPUM_PREGAIN_CH_CONTROLS(x, 0),	\
	ATUNE_SPUM_PREGAIN_CH_CONTROLS(x, 1),	\
	ATUNE_SPUM_PREGAIN_CH_CONTROLS(x, 2),	\
	ATUNE_SPUM_PREGAIN_CH_CONTROLS(x, 3),	\
	ATUNE_SPUM_PREGAIN_CH_CONTROLS(x, 4),	\
	ATUNE_SPUM_PREGAIN_CH_CONTROLS(x, 5),	\
	ATUNE_SPUM_PREGAIN_CH_CONTROLS(x, 6),	\
	ATUNE_SPUM_PREGAIN_CH_CONTROLS(x, 7)

#define ATUNE_SPUM_POSTGAIN_CH_CONTROLS(x, ch)				\
	SOC_SINGLE_S_TLV("SPUM"#x" POSTGAIN CH"#ch" Main Volume",	\
			ATUNE_SPUM_DSGAIN_GAIN_CH(x, ch),		\
			ATUNE_CH_GAIN_SHIFT_L(ch),			\
			-32, 20, 5, 0, atune_gain_shift_tlv),		\
	SOC_SINGLE_S_TLV("SPUM"#x" POSTGAIN CH"#ch" Fine Volume",	\
			ATUNE_SPUM_DSGAIN_GAIN_CH(x, ch),		\
			ATUNE_CH_GAIN_INDEX_L(ch),			\
			0, 5, 2, 0, atune_gain_index_tlv)

#define ATUNE_SPUM_POSTGAIN_CONTROLS(x)		\
	ATUNE_SPUM_POSTGAIN_CH_CONTROLS(x, 0),	\
	ATUNE_SPUM_POSTGAIN_CH_CONTROLS(x, 1),	\
	ATUNE_SPUM_POSTGAIN_CH_CONTROLS(x, 2),	\
	ATUNE_SPUM_POSTGAIN_CH_CONTROLS(x, 3),	\
	ATUNE_SPUM_POSTGAIN_CH_CONTROLS(x, 4),	\
	ATUNE_SPUM_POSTGAIN_CH_CONTROLS(x, 5),	\
	ATUNE_SPUM_POSTGAIN_CH_CONTROLS(x, 6),	\
	ATUNE_SPUM_POSTGAIN_CH_CONTROLS(x, 7)

static const unsigned int headroom_mask = ATUNE_HEADROOM_HPF_MASK >>
		ATUNE_HEADROOM_HPF_L;

static const char * const headroom_texts[] = {
	"6dB", "12dB", "18dB", "24dB",
};

static const unsigned int headroom_values[] = {
	2, 3, 4, 5,
};

static const unsigned int headroom_items = ARRAY_SIZE(headroom_texts);

#define ATUNE_SPUS_BQF_CH_HPF_HEADROOM_ENUM(x, ch)	\
	SOC_VALUE_ENUM_SINGLE(ATUNE_SPUS_BQF_CH##ch##_HEADROOM(x),	\
		ATUNE_HEADROOM_HPF_L, headroom_mask,			\
		headroom_items, headroom_texts, headroom_values)

static const struct soc_enum spus_bqf_ch_hpf_headroom[][COUNT_CH] = {
	/* SPUS BQF0 */
	{
		ATUNE_SPUS_BQF_CH_HPF_HEADROOM_ENUM(0, 0),
		ATUNE_SPUS_BQF_CH_HPF_HEADROOM_ENUM(0, 1),
		ATUNE_SPUS_BQF_CH_HPF_HEADROOM_ENUM(0, 2),
		ATUNE_SPUS_BQF_CH_HPF_HEADROOM_ENUM(0, 3),
		ATUNE_SPUS_BQF_CH_HPF_HEADROOM_ENUM(0, 4),
		ATUNE_SPUS_BQF_CH_HPF_HEADROOM_ENUM(0, 5),
		ATUNE_SPUS_BQF_CH_HPF_HEADROOM_ENUM(0, 6),
		ATUNE_SPUS_BQF_CH_HPF_HEADROOM_ENUM(0, 7),
	},
	/* SPUS BQF1 */
	{
		ATUNE_SPUS_BQF_CH_HPF_HEADROOM_ENUM(0, 0),
		ATUNE_SPUS_BQF_CH_HPF_HEADROOM_ENUM(0, 1),
		ATUNE_SPUS_BQF_CH_HPF_HEADROOM_ENUM(0, 2),
		ATUNE_SPUS_BQF_CH_HPF_HEADROOM_ENUM(0, 3),
		ATUNE_SPUS_BQF_CH_HPF_HEADROOM_ENUM(0, 4),
		ATUNE_SPUS_BQF_CH_HPF_HEADROOM_ENUM(0, 5),
		ATUNE_SPUS_BQF_CH_HPF_HEADROOM_ENUM(0, 6),
		ATUNE_SPUS_BQF_CH_HPF_HEADROOM_ENUM(0, 7),
	},
};

#define ATUNE_SPUS_BQF_CH_EQ_HEADROOM_ENUM(x, ch, eq)	\
	SOC_VALUE_ENUM_SINGLE(ATUNE_SPUS_BQF_CH##ch##_HEADROOM(x),	\
		ATUNE_HEADROOM_EQ_L(eq), headroom_mask,			\
		headroom_items, headroom_texts, headroom_values)

#define ATUNE_SPUS_BQF_CH_HEADROOM_ENUM(x, ch)	\
	ATUNE_SPUS_BQF_CH_EQ_HEADROOM_ENUM(x, ch, 0),	\
	ATUNE_SPUS_BQF_CH_EQ_HEADROOM_ENUM(x, ch, 1),	\
	ATUNE_SPUS_BQF_CH_EQ_HEADROOM_ENUM(x, ch, 2),	\
	ATUNE_SPUS_BQF_CH_EQ_HEADROOM_ENUM(x, ch, 3),	\
	ATUNE_SPUS_BQF_CH_EQ_HEADROOM_ENUM(x, ch, 4)

static const struct soc_enum spus_bqf_ch_eq_headroom[][COUNT_CH][COUNT_EQ] = {
	/* SPUS0 */
	{
		{ ATUNE_SPUS_BQF_CH_HEADROOM_ENUM(0, 0) },
		{ ATUNE_SPUS_BQF_CH_HEADROOM_ENUM(0, 1) },
		{ ATUNE_SPUS_BQF_CH_HEADROOM_ENUM(0, 2) },
		{ ATUNE_SPUS_BQF_CH_HEADROOM_ENUM(0, 3) },
		{ ATUNE_SPUS_BQF_CH_HEADROOM_ENUM(0, 4) },
		{ ATUNE_SPUS_BQF_CH_HEADROOM_ENUM(0, 5) },
		{ ATUNE_SPUS_BQF_CH_HEADROOM_ENUM(0, 6) },
		{ ATUNE_SPUS_BQF_CH_HEADROOM_ENUM(0, 7) },
	},
	/* SPUS1 */
	{
		{ ATUNE_SPUS_BQF_CH_HEADROOM_ENUM(1, 0) },
		{ ATUNE_SPUS_BQF_CH_HEADROOM_ENUM(1, 1) },
		{ ATUNE_SPUS_BQF_CH_HEADROOM_ENUM(1, 2) },
		{ ATUNE_SPUS_BQF_CH_HEADROOM_ENUM(1, 3) },
		{ ATUNE_SPUS_BQF_CH_HEADROOM_ENUM(1, 4) },
		{ ATUNE_SPUS_BQF_CH_HEADROOM_ENUM(1, 5) },
		{ ATUNE_SPUS_BQF_CH_HEADROOM_ENUM(1, 6) },
		{ ATUNE_SPUS_BQF_CH_HEADROOM_ENUM(1, 7) },
	},
};

#define ATUNE_SPUM_BQF_CH_HPF_HEADROOM_ENUM(x, ch)	\
	SOC_VALUE_ENUM_SINGLE(ATUNE_SPUM_BQF_CH##ch##_HEADROOM(x),	\
		ATUNE_HEADROOM_HPF_L, headroom_mask,			\
		headroom_items, headroom_texts, headroom_values)

static const struct soc_enum spum_bqf_ch_hpf_headroom[][COUNT_CH] = {
	/* SPUM0 */
	{
		ATUNE_SPUM_BQF_CH_HPF_HEADROOM_ENUM(0, 0),
		ATUNE_SPUM_BQF_CH_HPF_HEADROOM_ENUM(0, 1),
		ATUNE_SPUM_BQF_CH_HPF_HEADROOM_ENUM(0, 2),
		ATUNE_SPUM_BQF_CH_HPF_HEADROOM_ENUM(0, 3),
		ATUNE_SPUM_BQF_CH_HPF_HEADROOM_ENUM(0, 4),
		ATUNE_SPUM_BQF_CH_HPF_HEADROOM_ENUM(0, 5),
		ATUNE_SPUM_BQF_CH_HPF_HEADROOM_ENUM(0, 6),
		ATUNE_SPUM_BQF_CH_HPF_HEADROOM_ENUM(0, 7),
	},
	/* SPUM1 */
	{
		ATUNE_SPUM_BQF_CH_HPF_HEADROOM_ENUM(0, 0),
		ATUNE_SPUM_BQF_CH_HPF_HEADROOM_ENUM(0, 1),
		ATUNE_SPUM_BQF_CH_HPF_HEADROOM_ENUM(0, 2),
		ATUNE_SPUM_BQF_CH_HPF_HEADROOM_ENUM(0, 3),
		ATUNE_SPUM_BQF_CH_HPF_HEADROOM_ENUM(0, 4),
		ATUNE_SPUM_BQF_CH_HPF_HEADROOM_ENUM(0, 5),
		ATUNE_SPUM_BQF_CH_HPF_HEADROOM_ENUM(0, 6),
		ATUNE_SPUM_BQF_CH_HPF_HEADROOM_ENUM(0, 7),
	},
};

#define ATUNE_SPUM_BQF_CH_EQ_HEADROOM_ENUM(x, ch, eq)	\
	SOC_VALUE_ENUM_SINGLE(ATUNE_SPUM_BQF_CH##ch##_HEADROOM(x),	\
		ATUNE_HEADROOM_EQ_L(eq), headroom_mask,			\
		headroom_items, headroom_texts, headroom_values)

#define ATUNE_SPUM_BQF_CH_HEADROOM_ENUM(x, ch)	\
	ATUNE_SPUM_BQF_CH_EQ_HEADROOM_ENUM(x, ch, 0),	\
	ATUNE_SPUM_BQF_CH_EQ_HEADROOM_ENUM(x, ch, 1),	\
	ATUNE_SPUM_BQF_CH_EQ_HEADROOM_ENUM(x, ch, 2),	\
	ATUNE_SPUM_BQF_CH_EQ_HEADROOM_ENUM(x, ch, 3),	\
	ATUNE_SPUM_BQF_CH_EQ_HEADROOM_ENUM(x, ch, 4)

static const struct soc_enum spum_bqf_ch_eq_headroom[][COUNT_CH][COUNT_EQ] = {
	/* SPUM0 */
	{
		{ ATUNE_SPUM_BQF_CH_HEADROOM_ENUM(0, 0) },
		{ ATUNE_SPUM_BQF_CH_HEADROOM_ENUM(0, 1) },
		{ ATUNE_SPUM_BQF_CH_HEADROOM_ENUM(0, 2) },
		{ ATUNE_SPUM_BQF_CH_HEADROOM_ENUM(0, 3) },
		{ ATUNE_SPUM_BQF_CH_HEADROOM_ENUM(0, 4) },
		{ ATUNE_SPUM_BQF_CH_HEADROOM_ENUM(0, 5) },
		{ ATUNE_SPUM_BQF_CH_HEADROOM_ENUM(0, 6) },
		{ ATUNE_SPUM_BQF_CH_HEADROOM_ENUM(0, 7) },
	},
	/* SPUM1 */
	{
		{ ATUNE_SPUM_BQF_CH_HEADROOM_ENUM(1, 0) },
		{ ATUNE_SPUM_BQF_CH_HEADROOM_ENUM(1, 1) },
		{ ATUNE_SPUM_BQF_CH_HEADROOM_ENUM(1, 2) },
		{ ATUNE_SPUM_BQF_CH_HEADROOM_ENUM(1, 3) },
		{ ATUNE_SPUM_BQF_CH_HEADROOM_ENUM(1, 4) },
		{ ATUNE_SPUM_BQF_CH_HEADROOM_ENUM(1, 5) },
		{ ATUNE_SPUM_BQF_CH_HEADROOM_ENUM(1, 6) },
		{ ATUNE_SPUM_BQF_CH_HEADROOM_ENUM(1, 7) },
	},
};

#define ATUNE_SPUS_BQF_CH_HEADROOM_CONTROLS(x, ch)		\
	SOC_ENUM("SPUS"#x" CH"#ch" HEADROOM HPF",		\
			spus_bqf_ch_hpf_headroom[x][ch]),	\
	SOC_ENUM("SPUS"#x" CH"#ch" HEADROOM EQ0",		\
			spus_bqf_ch_eq_headroom[x][ch][0]),	\
	SOC_ENUM("SPUS"#x" CH"#ch" HEADROOM EQ1",		\
			spus_bqf_ch_eq_headroom[x][ch][1]),	\
	SOC_ENUM("SPUS"#x" CH"#ch" HEADROOM EQ2",		\
			spus_bqf_ch_eq_headroom[x][ch][2]),	\
	SOC_ENUM("SPUS"#x" CH"#ch" HEADROOM EQ3",		\
			spus_bqf_ch_eq_headroom[x][ch][3]),	\
	SOC_ENUM("SPUS"#x" CH"#ch" HEADROOM EQ4",		\
			spus_bqf_ch_eq_headroom[x][ch][4])

#define ATUNE_SPUS_BQF_HEADROOM_CONTROLS(x)		\
	ATUNE_SPUS_BQF_CH_HEADROOM_CONTROLS(x, 0),	\
	ATUNE_SPUS_BQF_CH_HEADROOM_CONTROLS(x, 1),	\
	ATUNE_SPUS_BQF_CH_HEADROOM_CONTROLS(x, 2),	\
	ATUNE_SPUS_BQF_CH_HEADROOM_CONTROLS(x, 3),	\
	ATUNE_SPUS_BQF_CH_HEADROOM_CONTROLS(x, 4),	\
	ATUNE_SPUS_BQF_CH_HEADROOM_CONTROLS(x, 5),	\
	ATUNE_SPUS_BQF_CH_HEADROOM_CONTROLS(x, 6),	\
	ATUNE_SPUS_BQF_CH_HEADROOM_CONTROLS(x, 7)

#define ATUNE_SPUM_BQF_CH_HEADROOM_CONTROLS(x, ch)		\
	SOC_ENUM("SPUM"#x" CH"#ch" HEADROOM HPF",		\
			spum_bqf_ch_hpf_headroom[x][ch]),	\
	SOC_ENUM("SPUM"#x" CH"#ch" HEADROOM EQ0",		\
			spum_bqf_ch_eq_headroom[x][ch][0]),	\
	SOC_ENUM("SPUM"#x" CH"#ch" HEADROOM EQ1",		\
			spum_bqf_ch_eq_headroom[x][ch][1]),	\
	SOC_ENUM("SPUM"#x" CH"#ch" HEADROOM EQ2",		\
			spum_bqf_ch_eq_headroom[x][ch][2]),	\
	SOC_ENUM("SPUM"#x" CH"#ch" HEADROOM EQ3",		\
			spum_bqf_ch_eq_headroom[x][ch][3]),	\
	SOC_ENUM("SPUM"#x" CH"#ch" HEADROOM EQ4",		\
			spum_bqf_ch_eq_headroom[x][ch][4])

#define ATUNE_SPUM_BQF_HEADROOM_CONTROLS(x)		\
	ATUNE_SPUM_BQF_CH_HEADROOM_CONTROLS(x, 0),	\
	ATUNE_SPUM_BQF_CH_HEADROOM_CONTROLS(x, 1),	\
	ATUNE_SPUM_BQF_CH_HEADROOM_CONTROLS(x, 2),	\
	ATUNE_SPUM_BQF_CH_HEADROOM_CONTROLS(x, 3),	\
	ATUNE_SPUM_BQF_CH_HEADROOM_CONTROLS(x, 4),	\
	ATUNE_SPUM_BQF_CH_HEADROOM_CONTROLS(x, 5),	\
	ATUNE_SPUM_BQF_CH_HEADROOM_CONTROLS(x, 6),	\
	ATUNE_SPUM_BQF_CH_HEADROOM_CONTROLS(x, 7)

#define ATUNE_SPUS_BQF_CH_POSTAMP_CONTROLS(x, ch)		\
	SOC_SINGLE("SPUS"#x" CH"#ch" POSTAMP HPF",		\
			ATUNE_SPUS_BQF_CH##ch##_POSTAMP(x),	\
			ATUNE_POSTAMP_HPF_L, 3, 0),		\
	SOC_SINGLE("SPUS"#x" CH"#ch" POSTAMP EQ0",		\
			ATUNE_SPUS_BQF_CH##ch##_POSTAMP(x),	\
			ATUNE_POSTAMP_EQ_L(0), 3, 0),		\
	SOC_SINGLE("SPUS"#x" CH"#ch" POSTAMP EQ1",		\
			ATUNE_SPUS_BQF_CH##ch##_POSTAMP(x),	\
			ATUNE_POSTAMP_EQ_L(1), 3, 0),		\
	SOC_SINGLE("SPUS"#x" CH"#ch" POSTAMP EQ2",		\
			ATUNE_SPUS_BQF_CH##ch##_POSTAMP(x),	\
			ATUNE_POSTAMP_EQ_L(2), 3, 0),		\
	SOC_SINGLE("SPUS"#x" CH"#ch" POSTAMP EQ3",		\
			ATUNE_SPUS_BQF_CH##ch##_POSTAMP(x),	\
			ATUNE_POSTAMP_EQ_L(3), 3, 0),		\
	SOC_SINGLE("SPUS"#x" CH"#ch" POSTAMP EQ4",		\
			ATUNE_SPUS_BQF_CH##ch##_POSTAMP(x),	\
			ATUNE_POSTAMP_EQ_L(4), 3, 0)

#define ATUNE_SPUS_BQF_POSTAMP_CONTROLS(x)		\
	ATUNE_SPUS_BQF_CH_POSTAMP_CONTROLS(x, 0),	\
	ATUNE_SPUS_BQF_CH_POSTAMP_CONTROLS(x, 1),	\
	ATUNE_SPUS_BQF_CH_POSTAMP_CONTROLS(x, 2),	\
	ATUNE_SPUS_BQF_CH_POSTAMP_CONTROLS(x, 3),	\
	ATUNE_SPUS_BQF_CH_POSTAMP_CONTROLS(x, 4),	\
	ATUNE_SPUS_BQF_CH_POSTAMP_CONTROLS(x, 5),	\
	ATUNE_SPUS_BQF_CH_POSTAMP_CONTROLS(x, 6),	\
	ATUNE_SPUS_BQF_CH_POSTAMP_CONTROLS(x, 7)

#define ATUNE_SPUM_BQF_CH_POSTAMP_CONTROLS(x, ch)		\
	SOC_SINGLE("SPUM"#x" CH"#ch" POSTAMP HPF",		\
			ATUNE_SPUM_BQF_CH##ch##_POSTAMP(x),	\
			ATUNE_POSTAMP_HPF_L, 3, 0),		\
	SOC_SINGLE("SPUM"#x" CH"#ch" POSTAMP EQ0",		\
			ATUNE_SPUM_BQF_CH##ch##_POSTAMP(x),	\
			ATUNE_POSTAMP_EQ_L(0), 3, 0),		\
	SOC_SINGLE("SPUM"#x" CH"#ch" POSTAMP EQ1",		\
			ATUNE_SPUM_BQF_CH##ch##_POSTAMP(x),	\
			ATUNE_POSTAMP_EQ_L(1), 3, 0),		\
	SOC_SINGLE("SPUM"#x" CH"#ch" POSTAMP EQ2",		\
			ATUNE_SPUM_BQF_CH##ch##_POSTAMP(x),	\
			ATUNE_POSTAMP_EQ_L(2), 3, 0),		\
	SOC_SINGLE("SPUM"#x" CH"#ch" POSTAMP EQ3",		\
			ATUNE_SPUM_BQF_CH##ch##_POSTAMP(x),	\
			ATUNE_POSTAMP_EQ_L(3), 3, 0),		\
	SOC_SINGLE("SPUM"#x" CH"#ch" POSTAMP EQ4",		\
			ATUNE_SPUM_BQF_CH##ch##_POSTAMP(x),	\
			ATUNE_POSTAMP_EQ_L(4), 3, 0)

#define ATUNE_SPUM_BQF_POSTAMP_CONTROLS(x)		\
	ATUNE_SPUM_BQF_CH_POSTAMP_CONTROLS(x, 0),	\
	ATUNE_SPUM_BQF_CH_POSTAMP_CONTROLS(x, 1),	\
	ATUNE_SPUM_BQF_CH_POSTAMP_CONTROLS(x, 2),	\
	ATUNE_SPUM_BQF_CH_POSTAMP_CONTROLS(x, 3),	\
	ATUNE_SPUM_BQF_CH_POSTAMP_CONTROLS(x, 4),	\
	ATUNE_SPUM_BQF_CH_POSTAMP_CONTROLS(x, 5),	\
	ATUNE_SPUM_BQF_CH_POSTAMP_CONTROLS(x, 6),	\
	ATUNE_SPUM_BQF_CH_POSTAMP_CONTROLS(x, 7)

struct atune_multi_control {
	int min, max;
	int regs[8];
	unsigned int shifts[8];
	unsigned int count;
	unsigned int sign_bit;
	unsigned int invert:1;
	unsigned int autodisable:1;
	struct snd_soc_dobj dobj;
};

/* revisit snd_soc_read_signed() in ASoC */
static int cmpnt_read_signed(struct snd_soc_component *component,
	unsigned int reg, unsigned int mask, unsigned int shift,
	unsigned int sign_bit, int *signed_val)
{
	int ret;
	unsigned int val;

	ret = snd_soc_component_read(component, reg, &val);
	if (ret < 0)
		return ret;

	val = (val >> shift) & mask;

	if (!sign_bit || !(val & BIT(sign_bit)))
		*signed_val = val;
	else
		*signed_val = (int)val | ~((int)(BIT(sign_bit) - 1));

	return 0;
}

static int atune_info_multi(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	struct atune_multi_control *amc =
		(struct atune_multi_control *)kcontrol->private_value;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = amc->count;
	uinfo->value.integer.min = amc->min;
	uinfo->value.integer.max = amc->max;
	return 0;
}

static int __maybe_unused atune_get_multi(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct atune_multi_control *amc =
		(struct atune_multi_control *)kcontrol->private_value;
	unsigned int *reg = amc->regs;
	unsigned int *shift = amc->shifts;
	unsigned int i, count = amc->count;
	int max = amc->max;
	int sign_bit = amc->sign_bit;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = amc->invert;
	int val;
	int ret;

	if (sign_bit)
		mask = BIT(sign_bit + 1) - 1;

	for (i = count - 1; i; i--) {
		ret = cmpnt_read_signed(component, reg[i], mask, shift[i],
			sign_bit, &val);
		if (ret)
			return ret;

		ucontrol->value.integer.value[i] = invert ? (max - val): val;
	}

	return 0;
}

static int __maybe_unused atune_put_multi(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct atune_multi_control *amc =
		(struct atune_multi_control *)kcontrol->private_value;
	unsigned int *reg = amc->regs;
	unsigned int *shift = amc->shifts;
	unsigned int i, count = amc->count;
	int max = amc->max;
	unsigned int sign_bit = amc->sign_bit;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = amc->invert;
	int err;
	unsigned int val, val_mask;

	if (sign_bit)
		mask = BIT(sign_bit + 1) - 1;

	for (i = count - 1; i; i--) {
		val = ((ucontrol->value.integer.value[i]) & mask);
		if (invert)
			val = max - val;
		val_mask = mask << shift[i];
		val = val << shift[i];
		err = snd_soc_component_update_bits(component, reg[i],
			val_mask, val);
		if (err < 0)
			break;
	}

	return err;
}

static int atune_get_multi_linear(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct atune_multi_control *amc =
		(struct atune_multi_control *)kcontrol->private_value;
	unsigned int reg = amc->regs[0];
	unsigned int shift = amc->shifts[0];
	unsigned int offset;
	unsigned int i, count = amc->count;
	int max = amc->max;
	int sign_bit = amc->sign_bit;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = amc->invert;
	int val;
	int ret;

	if (sign_bit)
		mask = BIT(sign_bit + 1) - 1;

	for (i = 0, offset = 0; i < count; i++, offset += SFR_STRIDE) {
		ret = cmpnt_read_signed(component, reg + offset, mask, shift,
			sign_bit, &val);
		if (ret)
			return ret;

		ucontrol->value.integer.value[i] = invert ? (max - val): val;
	}

	return 0;
}

static int atune_put_multi_linear(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct atune_multi_control *amc =
		(struct atune_multi_control *)kcontrol->private_value;
	unsigned int reg = amc->regs[0];
	unsigned int shift = amc->shifts[0];
	unsigned int offset;
	unsigned int i, count = amc->count;
	int max = amc->max;
	unsigned int sign_bit = amc->sign_bit;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = amc->invert;
	int err;
	unsigned int val, val_mask;

	if (sign_bit)
		mask = BIT(sign_bit + 1) - 1;

	for (i = 0, offset = 0; i < count; i++, offset += SFR_STRIDE) {
		val = ((ucontrol->value.integer.value[i]) & mask);
		if (invert)
			val = max - val;
		val_mask = mask << shift;
		val = val << shift;
		err = snd_soc_component_update_bits(component, reg + offset,
			val_mask, val);
		if (err < 0)
			break;
	}

	return err;
}

#define ATUNE_MULTI_VALUE(xregs, xshifts, xcount, xmin, xmax, \
		xsign_bit, xinvert) \
	((unsigned long)&(struct atune_multi_control) \
	{.regs = xregs, .shifts = xshifts, .count = xcount, .min = xmin, \
	.max = xmax, .sign_bit = xsign_bit, .invert = xinvert,})

#define ATUNE_MULTI(xname, regs, shifts, min, max, sign_bit, invert) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = atune_info_multi, .get = atune_get_multi,\
	.put = atune_put_multi, \
	.private_value = ATUNE_MULTI_VALUE(regs, shifts, \
	ARRAY_SIZE(regs), min, max, sign_bit, invert) }

#define ATUNE_MULTI_LINEAR(xname, regbase, shift, count, min, max, \
		sign_bit, invert) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = atune_info_multi, .get = atune_get_multi_linear,\
	.put = atune_put_multi_linear, \
	.private_value = ATUNE_MULTI_VALUE({ regbase }, { shift }, \
	count, min, max, sign_bit, invert) }

#define ATUNE_SPUS_BQF_CH_COEF_CONTROLS(x, ch)				\
	ATUNE_MULTI_LINEAR("SPUS"#x" CH"#ch" HPF COEF",			\
			ATUNE_SPUS_BQF_CH##ch##_HPF_COEF0(x),		\
			ATUNE_COEF_L, 5, INT_MIN, INT_MAX, 31, 0),	\
	ATUNE_MULTI_LINEAR("SPUS"#x" CH"#ch" EQ0 COEF",			\
			ATUNE_SPUS_BQF_CH##ch##_EQ0_COEF0(x),		\
			ATUNE_COEF_L, 5, INT_MIN, INT_MAX, 31, 0),	\
	ATUNE_MULTI_LINEAR("SPUS"#x" CH"#ch" EQ1 COEF",			\
			ATUNE_SPUS_BQF_CH##ch##_EQ1_COEF0(x),		\
			ATUNE_COEF_L, 5, INT_MIN, INT_MAX, 31, 0),	\
	ATUNE_MULTI_LINEAR("SPUS"#x" CH"#ch" EQ2 COEF",			\
			ATUNE_SPUS_BQF_CH##ch##_EQ2_COEF0(x),		\
			ATUNE_COEF_L, 5, INT_MIN, INT_MAX, 31, 0),	\
	ATUNE_MULTI_LINEAR("SPUS"#x" CH"#ch" EQ3 COEF",			\
			ATUNE_SPUS_BQF_CH##ch##_EQ3_COEF0(x),		\
			ATUNE_COEF_L, 5, INT_MIN, INT_MAX, 31, 0),	\
	ATUNE_MULTI_LINEAR("SPUS"#x" CH"#ch" EQ4 COEF",			\
			ATUNE_SPUS_BQF_CH##ch##_EQ4_COEF0(x),		\
			ATUNE_COEF_L, 5, INT_MIN, INT_MAX, 31, 0)

#define ATUNE_SPUS_BQF_COEF_CONTROLS(x)		\
	ATUNE_SPUS_BQF_CH_COEF_CONTROLS(x, 0),	\
	ATUNE_SPUS_BQF_CH_COEF_CONTROLS(x, 1),	\
	ATUNE_SPUS_BQF_CH_COEF_CONTROLS(x, 2),	\
	ATUNE_SPUS_BQF_CH_COEF_CONTROLS(x, 3),	\
	ATUNE_SPUS_BQF_CH_COEF_CONTROLS(x, 4),	\
	ATUNE_SPUS_BQF_CH_COEF_CONTROLS(x, 5),	\
	ATUNE_SPUS_BQF_CH_COEF_CONTROLS(x, 6),	\
	ATUNE_SPUS_BQF_CH_COEF_CONTROLS(x, 7)

#define ATUNE_SPUM_BQF_CH_COEF_CONTROLS(x, ch)				\
	ATUNE_MULTI_LINEAR("SPUM"#x" CH"#ch" HPF COEF",			\
			ATUNE_SPUM_BQF_CH##ch##_HPF_COEF0(x),		\
			ATUNE_COEF_L, 5, INT_MIN, INT_MAX, 31, 0),	\
	ATUNE_MULTI_LINEAR("SPUM"#x" CH"#ch" EQ0 COEF",			\
			ATUNE_SPUM_BQF_CH##ch##_EQ0_COEF0(x),		\
			ATUNE_COEF_L, 5, INT_MIN, INT_MAX, 31, 0),	\
	ATUNE_MULTI_LINEAR("SPUM"#x" CH"#ch" EQ1 COEF",			\
			ATUNE_SPUM_BQF_CH##ch##_EQ1_COEF0(x),		\
			ATUNE_COEF_L, 5, INT_MIN, INT_MAX, 31, 0),	\
	ATUNE_MULTI_LINEAR("SPUM"#x" CH"#ch" EQ2 COEF",			\
			ATUNE_SPUM_BQF_CH##ch##_EQ2_COEF0(x),		\
			ATUNE_COEF_L, 5, INT_MIN, INT_MAX, 31, 0),	\
	ATUNE_MULTI_LINEAR("SPUM"#x" CH"#ch" EQ3 COEF",			\
			ATUNE_SPUM_BQF_CH##ch##_EQ3_COEF0(x),		\
			ATUNE_COEF_L, 5, INT_MIN, INT_MAX, 31, 0),	\
	ATUNE_MULTI_LINEAR("SPUM"#x" CH"#ch" EQ4 COEF",			\
			ATUNE_SPUM_BQF_CH##ch##_EQ4_COEF0(x),		\
			ATUNE_COEF_L, 5, INT_MIN, INT_MAX, 31, 0)

#define ATUNE_SPUM_BQF_COEF_CONTROLS(x)		\
	ATUNE_SPUM_BQF_CH_COEF_CONTROLS(x, 0),	\
	ATUNE_SPUM_BQF_CH_COEF_CONTROLS(x, 1),	\
	ATUNE_SPUM_BQF_CH_COEF_CONTROLS(x, 2),	\
	ATUNE_SPUM_BQF_CH_COEF_CONTROLS(x, 3),	\
	ATUNE_SPUM_BQF_CH_COEF_CONTROLS(x, 4),	\
	ATUNE_SPUM_BQF_CH_COEF_CONTROLS(x, 5),	\
	ATUNE_SPUM_BQF_CH_COEF_CONTROLS(x, 6),	\
	ATUNE_SPUM_BQF_CH_COEF_CONTROLS(x, 7)

static const DECLARE_TLV_DB_SCALE(atune_ng_threshold_tlv, -9600, 100, 0);
static const DECLARE_TLV_DB_MINMAX(atune_drc_makeup_gain_tlv, 0, 3000);
static const DECLARE_TLV_DB_SCALE(atune_drc_threshold_tlv, -8000, 100, 0);

#define ATUNE_SPUS_DRC_BAND_CONTROLS(x, band)			\
	SOC_SINGLE_S_TLV("SPUS"#x" DRC "#band" MAKEUP GAIN",	\
			ATUNE_SPUS_DRC_COMP_##band##0(x),	\
			ATUNE_MAKEUP_GAIN_L, 0x0400, 0x7FFF,	\
			15, 0, 	atune_drc_makeup_gain_tlv),	\
	SOC_SINGLE_S_TLV("SPUS"#x" DRC "#band" THRESHOLD",	\
			ATUNE_SPUS_DRC_COMP_##band##0(x),	\
			ATUNE_THRS_L, -80, 0, 7, 0,		\
			atune_drc_threshold_tlv),		\
	SOC_SINGLE("SPUS"#x" DRC "#band" RATIO",		\
			ATUNE_SPUS_DRC_COMP_##band##0(x),	\
			ATUNE_RATIO_L, 0xf, 0),			\
	SOC_SINGLE("SPUS"#x" DRC "#band" RMS",			\
			ATUNE_SPUS_DRC_COMP_##band##0(x),	\
			ATUNE_RMS_ENABLE_L, 1, 0),		\
	SOC_SINGLE("SPUS"#x" DRC "#band" MUTE",			\
			ATUNE_SPUS_DRC_COMP_##band##0(x),	\
			ATUNE_MUTE_ENABLE_L, 1, 0),		\
	SOC_SINGLE("SPUS"#x" DRC "#band" ATTACK",		\
			ATUNE_SPUS_DRC_COMP_##band##1(x),	\
			ATUNE_ATTACK_TIME_L, INT_MAX, 0),	\
	SOC_SINGLE("SPUS"#x" DRC "#band" RELEASE",		\
			ATUNE_SPUS_DRC_COMP_##band##2(x),	\
			ATUNE_RELEASE_TIME_L, INT_MAX, 0)

#define ATUNE_SPUS_DRC_CONTROLS(x)		\
	ATUNE_SPUS_DRC_BAND_CONTROLS(x, LB),	\
	ATUNE_SPUS_DRC_BAND_CONTROLS(x, MB),	\
	ATUNE_SPUS_DRC_BAND_CONTROLS(x, HB)

#define ATUNE_SPUM_DRC_BAND_CONTROLS(x, band)			\
	SOC_SINGLE_S_TLV("SPUM"#x" DRC "#band" MAKEUP GAIN",	\
			ATUNE_SPUM_DRC_COMP_##band##0(x),	\
			ATUNE_MAKEUP_GAIN_L, 0x0400, 0x7FFF,	\
			15, 0, atune_drc_makeup_gain_tlv),	\
	SOC_SINGLE_S_TLV("SPUM"#x" DRC "#band" THRESHOLD",	\
			ATUNE_SPUM_DRC_COMP_##band##0(x),	\
			ATUNE_THRS_L, -80, 0, 7, 0,		\
			atune_drc_threshold_tlv),		\
	SOC_SINGLE("SPUM"#x" DRC "#band" RATIO",		\
			ATUNE_SPUM_DRC_COMP_##band##0(x),	\
			ATUNE_RATIO_L, 0xf, 0),			\
	SOC_SINGLE("SPUM"#x" DRC "#band" RMS",			\
			ATUNE_SPUM_DRC_COMP_##band##0(x),	\
			ATUNE_RMS_ENABLE_L, 1, 0),		\
	SOC_SINGLE("SPUM"#x" DRC "#band" MUTE",			\
			ATUNE_SPUM_DRC_COMP_##band##0(x),	\
			ATUNE_MUTE_ENABLE_L, 1, 0),		\
	SOC_SINGLE("SPUM"#x" DRC "#band" ATTACK",		\
			ATUNE_SPUM_DRC_COMP_##band##1(x),	\
			ATUNE_ATTACK_TIME_L, INT_MAX, 0),	\
	SOC_SINGLE("SPUM"#x" DRC "#band" RELEASE",		\
			ATUNE_SPUM_DRC_COMP_##band##2(x),	\
			ATUNE_RELEASE_TIME_L, INT_MAX, 0)

#define ATUNE_SPUM_DRC_CONTROLS(x)		\
	ATUNE_SPUM_DRC_BAND_CONTROLS(x, LB),	\
	ATUNE_SPUM_DRC_BAND_CONTROLS(x, MB),	\
	ATUNE_SPUM_DRC_BAND_CONTROLS(x, HB)

static struct snd_kcontrol_new atune_controls[] = {
	SOC_ENUM("SPUS0 PREGAIN FUNC", spus0_pregain_func_enum),
	SOC_SINGLE("SPUS0 PREGAIN EN", ATUNE_SPUS_USGAIN_CTRL(0),
			ATUNE_ENABLE_L, 1, 0),
	SOC_SINGLE_XR_SX("SPUS0 PREGAIN VOL CHANGE FADE IN",
			ATUNE_SPUS_USGAIN_VOL_CHANGE_FIN(0),
			1, 32, 0, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SPUS0 PREGAIN VOL CHANGE FADE OUT",
			ATUNE_SPUS_USGAIN_VOL_CHANGE_FOUT(0),
			1, 32, INT_MIN, 0, 0),
	ATUNE_SPUS_PREGAIN_CONTROLS(0),
	SOC_SINGLE("SPUS0 EQ EN", ATUNE_SPUS_BQF_CTRL(0),
			ATUNE_EQ_ENABLE_L, 1, 0),
	SOC_SINGLE("SPUS0 HPF EN", ATUNE_SPUS_BQF_CTRL(0),
			ATUNE_HPF_ENABLE_L, 1, 0),
	ATUNE_SPUS_BQF_HEADROOM_CONTROLS(0),
	ATUNE_SPUS_BQF_POSTAMP_CONTROLS(0),
	ATUNE_SPUS_BQF_COEF_CONTROLS(0),
	SOC_SINGLE("SPUS0 LIMITER WINDOW", ATUNE_SPUS_DRC_CTRL(0),
			ATUNE_LIMIT_WINDOW_SIZE_L, 0xff, 0),
	SOC_SINGLE_S_TLV("SPUS0 DRC NG THRESHOLD", ATUNE_SPUS_DRC_CTRL(0),
			ATUNE_NOISE_THRS_L, -108, -12, 7, 0,
			atune_ng_threshold_tlv),
	SOC_SINGLE("SPUS0 DRC NG HB EN", ATUNE_SPUS_DRC_CTRL(0),
			ATUNE_DRC_NG_ENABLE_HB_L, 1, 0),
	SOC_SINGLE("SPUS0 DRC NG MB EN", ATUNE_SPUS_DRC_CTRL(0),
			ATUNE_DRC_NG_ENABLE_MB_L, 1, 0),
	SOC_SINGLE("SPUS0 DRC NG LB EN", ATUNE_SPUS_DRC_CTRL(0),
			ATUNE_DRC_NG_ENABLE_LB_L, 1, 0),
	SOC_SINGLE("SPUS0 DRC HB EN", ATUNE_SPUS_DRC_CTRL(0),
			ATUNE_DRC_ENABLE_HB_L, 1, 0),
	SOC_SINGLE("SPUS0 DRC MB EN", ATUNE_SPUS_DRC_CTRL(0),
			ATUNE_DRC_ENABLE_MB_L, 1, 0),
	SOC_SINGLE("SPUS0 DRC LB EN", ATUNE_SPUS_DRC_CTRL(0),
			ATUNE_DRC_ENABLE_LB_L, 1, 0),
	SOC_SINGLE("SPUS0 LIMITER EN", ATUNE_SPUS_DRC_CTRL(0),
			ATUNE_LIMIT_ENABLE_L, 1, 0),
	SOC_SINGLE("SPUS0 DRC EN", ATUNE_SPUS_DRC_CTRL(0),
			ATUNE_DRC_ENABLE_L, 1, 0),
	ATUNE_SPUS_DRC_CONTROLS(0),
	SOC_SINGLE("SPUS0 LIMITER THRESHOLD", ATUNE_SPUS_DRC_LMT_CTRL0(0),
			ATUNE_THRESHOLD_L, INT_MAX, 0),
	SOC_SINGLE("SPUS0 LIMITER ATTACK", ATUNE_SPUS_DRC_LMT_CTRL1(0),
			ATUNE_ATTACK_TIME_L, INT_MAX, 0),
	ATUNE_MULTI_LINEAR("SPUS0 DRC XPF0 COEF", ATUNE_SPUS_DRC_xPF0_COEF0(0),
			ATUNE_COEF_L, 4, INT_MIN, INT_MAX, 31, 0),
	ATUNE_MULTI_LINEAR("SPUS0 DRC XPF1 COEF", ATUNE_SPUS_DRC_xPF1_COEF0(0),
			ATUNE_COEF_L, 4, INT_MIN, INT_MAX, 31, 0),
	SOC_ENUM("SPUS0 POSTGAIN FUNC", spus0_postgain_func_enum),
	SOC_SINGLE("SPUS0 POSTGAIN EN", ATUNE_SPUS_DSGAIN_CTRL(0),
			ATUNE_ENABLE_L, 1, 0),
	SOC_SINGLE_XR_SX("SPUS0 POSTGAIN VOL CHANGE FADE IN",
			ATUNE_SPUS_DSGAIN_VOL_CHANGE_FIN(0),
			1, 32, 0, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SPUS0 POSTGAIN VOL CHANGE FADE OUT",
			ATUNE_SPUS_DSGAIN_VOL_CHANGE_FOUT(0),
			1, 32, INT_MIN, 0, 0),
	ATUNE_SPUS_POSTGAIN_CONTROLS(0),
	SOC_ENUM("SPUS0 POSTGAIN Dither Type", spus0_postgain_dither_type_enum),

	SOC_ENUM("SPUS1 PREGAIN FUNC", spus1_pregain_func_enum),
	SOC_SINGLE("SPUS1 PREGAIN EN", ATUNE_SPUS_USGAIN_CTRL(1),
			ATUNE_ENABLE_L, 1, 0),
	SOC_SINGLE_XR_SX("SPUS1 PREGAIN VOL CHANGE FADE IN",
			ATUNE_SPUS_USGAIN_VOL_CHANGE_FIN(1),
			1, 32, 0, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SPUS1 PREGAIN VOL CHANGE FADE OUT",
			ATUNE_SPUS_USGAIN_VOL_CHANGE_FOUT(1),
			1, 32, INT_MIN, 0, 0),
	ATUNE_SPUS_PREGAIN_CONTROLS(1),
	SOC_SINGLE("SPUS1 EQ EN", ATUNE_SPUS_BQF_CTRL(1),
			ATUNE_EQ_ENABLE_L, 1, 0),
	SOC_SINGLE("SPUS1 HPF EN", ATUNE_SPUS_BQF_CTRL(1),
			ATUNE_HPF_ENABLE_L, 1, 0),
	ATUNE_SPUS_BQF_HEADROOM_CONTROLS(1),
	ATUNE_SPUS_BQF_POSTAMP_CONTROLS(1),
	ATUNE_SPUS_BQF_COEF_CONTROLS(1),
	SOC_SINGLE("SPUS1 LIMITER WINDOW", ATUNE_SPUS_DRC_CTRL(1),
			ATUNE_LIMIT_WINDOW_SIZE_L, 0xff, 0),
	SOC_SINGLE_S_TLV("SPUS1 DRC NG THRESHOLD", ATUNE_SPUS_DRC_CTRL(1),
			ATUNE_NOISE_THRS_L, -108, -12, 7, 0,
			atune_ng_threshold_tlv),
	SOC_SINGLE("SPUS1 DRC NG HB EN", ATUNE_SPUS_DRC_CTRL(1),
			ATUNE_DRC_NG_ENABLE_HB_L, 1, 0),
	SOC_SINGLE("SPUS1 DRC NG MB EN", ATUNE_SPUS_DRC_CTRL(1),
			ATUNE_DRC_NG_ENABLE_MB_L, 1, 0),
	SOC_SINGLE("SPUS1 DRC NG LB EN", ATUNE_SPUS_DRC_CTRL(1),
			ATUNE_DRC_NG_ENABLE_LB_L, 1, 0),
	SOC_SINGLE("SPUS1 DRC HB EN", ATUNE_SPUS_DRC_CTRL(1),
			ATUNE_DRC_ENABLE_HB_L, 1, 0),
	SOC_SINGLE("SPUS1 DRC MB EN", ATUNE_SPUS_DRC_CTRL(1),
			ATUNE_DRC_ENABLE_MB_L, 1, 0),
	SOC_SINGLE("SPUS1 DRC LB EN", ATUNE_SPUS_DRC_CTRL(1),
			ATUNE_DRC_ENABLE_LB_L, 1, 0),
	SOC_SINGLE("SPUS1 LIMITER EN", ATUNE_SPUS_DRC_CTRL(1),
			ATUNE_LIMIT_ENABLE_L, 1, 0),
	SOC_SINGLE("SPUS1 DRC EN", ATUNE_SPUS_DRC_CTRL(1),
			ATUNE_DRC_ENABLE_L, 1, 0),
	ATUNE_SPUS_DRC_CONTROLS(1),
	SOC_SINGLE("SPUS1 LIMITER THRESHOLD", ATUNE_SPUS_DRC_LMT_CTRL0(1),
			ATUNE_THRESHOLD_L, INT_MAX, 0),
	SOC_SINGLE("SPUS1 LIMITER ATTACK", ATUNE_SPUS_DRC_LMT_CTRL1(1),
			ATUNE_ATTACK_TIME_L, INT_MAX, 0),
	ATUNE_MULTI_LINEAR("SPUS1 DRC XPF0 COEF", ATUNE_SPUS_DRC_xPF0_COEF0(1),
			ATUNE_COEF_L, 4, INT_MIN, INT_MAX, 31, 0),
	ATUNE_MULTI_LINEAR("SPUS1 DRC XPF1 COEF", ATUNE_SPUS_DRC_xPF1_COEF0(1),
			ATUNE_COEF_L, 4, INT_MIN, INT_MAX, 31, 0),
	SOC_ENUM("SPUS1 POSTGAIN FUNC", spus1_postgain_func_enum),
	SOC_SINGLE("SPUS1 POSTGAIN EN", ATUNE_SPUS_DSGAIN_CTRL(1),
			ATUNE_ENABLE_L, 1, 0),
	SOC_SINGLE_XR_SX("SPUS1 POSTGAIN VOL CHANGE FADE IN",
			ATUNE_SPUS_DSGAIN_VOL_CHANGE_FIN(1),
			1, 32, 0, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SPUS1 POSTGAIN VOL CHANGE FADE OUT",
			ATUNE_SPUS_DSGAIN_VOL_CHANGE_FOUT(1),
			1, 32, INT_MIN, 0, 0),
	ATUNE_SPUS_POSTGAIN_CONTROLS(1),
	SOC_ENUM("SPUS1 POSTGAIN Dither Type", spus1_postgain_dither_type_enum),

	SOC_ENUM("SPUM0 PREGAIN FUNC", spum0_pregain_func_enum),
	SOC_SINGLE("SPUM0 PREGAIN EN", ATUNE_SPUM_USGAIN_CTRL(0),
			ATUNE_ENABLE_L, 1, 0),
	SOC_SINGLE_XR_SX("SPUM0 PREGAIN VOL CHANGE FADE IN",
			ATUNE_SPUM_USGAIN_VOL_CHANGE_FIN(0),
			1, 32, 0, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SPUM0 PREGAIN VOL CHANGE FADE OUT",
			ATUNE_SPUM_USGAIN_VOL_CHANGE_FOUT(0),
			1, 32, INT_MIN, 0, 0),
	ATUNE_SPUM_PREGAIN_CONTROLS(0),
	SOC_SINGLE("SPUM0 EQ EN", ATUNE_SPUM_BQF_CTRL(0),
			ATUNE_EQ_ENABLE_L, 1, 0),
	SOC_SINGLE("SPUM0 HPF EN", ATUNE_SPUM_BQF_CTRL(0),
			ATUNE_HPF_ENABLE_L, 1, 0),
	ATUNE_SPUM_BQF_HEADROOM_CONTROLS(0),
	ATUNE_SPUM_BQF_POSTAMP_CONTROLS(0),
	ATUNE_SPUM_BQF_COEF_CONTROLS(0),
	SOC_SINGLE("SPUM0 LIMITER WINDOW", ATUNE_SPUM_DRC_CTRL(0),
			ATUNE_LIMIT_WINDOW_SIZE_L, 0xff, 0),
	SOC_SINGLE_S_TLV("SPUM0 DRC NG THRESHOLD", ATUNE_SPUM_DRC_CTRL(0),
			ATUNE_NOISE_THRS_L, -108, -12, 7, 0,
			atune_ng_threshold_tlv),
	SOC_SINGLE("SPUM0 DRC NG HB EN", ATUNE_SPUM_DRC_CTRL(0),
			ATUNE_DRC_NG_ENABLE_HB_L, 1, 0),
	SOC_SINGLE("SPUM0 DRC NG MB EN", ATUNE_SPUM_DRC_CTRL(0),
			ATUNE_DRC_NG_ENABLE_MB_L, 1, 0),
	SOC_SINGLE("SPUM0 DRC NG LB EN", ATUNE_SPUM_DRC_CTRL(0),
			ATUNE_DRC_NG_ENABLE_LB_L, 1, 0),
	SOC_SINGLE("SPUM0 DRC HB EN", ATUNE_SPUM_DRC_CTRL(0),
			ATUNE_DRC_ENABLE_HB_L, 1, 0),
	SOC_SINGLE("SPUM0 DRC MB EN", ATUNE_SPUM_DRC_CTRL(0),
			ATUNE_DRC_ENABLE_MB_L, 1, 0),
	SOC_SINGLE("SPUM0 DRC LB EN", ATUNE_SPUM_DRC_CTRL(0),
			ATUNE_DRC_ENABLE_LB_L, 1, 0),
	SOC_SINGLE("SPUM0 LIMITER EN", ATUNE_SPUM_DRC_CTRL(0),
			ATUNE_LIMIT_ENABLE_L, 1, 0),
	SOC_SINGLE("SPUM0 DRC EN", ATUNE_SPUM_DRC_CTRL(0),
			ATUNE_DRC_ENABLE_L, 1, 0),
	ATUNE_SPUM_DRC_CONTROLS(0),
	SOC_SINGLE("SPUM0 LIMITER THRESHOLD", ATUNE_SPUM_DRC_LMT_CTRL0(0),
			ATUNE_THRESHOLD_L, INT_MAX, 0),
	SOC_SINGLE("SPUM0 LIMITER ATTACK", ATUNE_SPUM_DRC_LMT_CTRL1(0),
			ATUNE_ATTACK_TIME_L, INT_MAX, 0),
	ATUNE_MULTI_LINEAR("SPUM0 DRC XPF0 COEF", ATUNE_SPUM_DRC_xPF0_COEF0(0),
			ATUNE_COEF_L, 4, INT_MIN, INT_MAX, 31, 0),
	ATUNE_MULTI_LINEAR("SPUM0 DRC XPF1 COEF", ATUNE_SPUM_DRC_xPF1_COEF0(0),
			ATUNE_COEF_L, 4, INT_MIN, INT_MAX, 31, 0),
	SOC_ENUM("SPUM0 POSTGAIN FUNC", spum0_postgain_func_enum),
	SOC_SINGLE("SPUM0 POSTGAIN EN", ATUNE_SPUM_DSGAIN_CTRL(0),
			ATUNE_ENABLE_L, 1, 0),
	SOC_SINGLE_XR_SX("SPUM0 POSTGAIN VOL CHANGE FADE IN",
			ATUNE_SPUM_DSGAIN_VOL_CHANGE_FIN(0),
			1, 32, 0, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SPUM0 POSTGAIN VOL CHANGE FADE OUT",
			ATUNE_SPUM_DSGAIN_VOL_CHANGE_FOUT(0),
			1, 32, INT_MIN, 0, 0),
	ATUNE_SPUM_POSTGAIN_CONTROLS(0),
	SOC_ENUM("SPUM0 POSTGAIN Dither Type", spum0_postgain_dither_type_enum),

	SOC_ENUM("SPUM1 PREGAIN FUNC", spum1_pregain_func_enum),
	SOC_SINGLE("SPUM1 PREGAIN EN", ATUNE_SPUM_USGAIN_CTRL(1),
			ATUNE_ENABLE_L, 1, 0),
	SOC_SINGLE_XR_SX("SPUM1 PREGAIN VOL CHANGE FADE IN",
			ATUNE_SPUM_USGAIN_VOL_CHANGE_FIN(1),
			1, 32, 0, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SPUM1 PREGAIN VOL CHANGE FADE OUT",
			ATUNE_SPUM_USGAIN_VOL_CHANGE_FOUT(1),
			1, 32, INT_MIN, 0, 0),
	ATUNE_SPUM_PREGAIN_CONTROLS(1),
	SOC_SINGLE("SPUM1 EQ EN", ATUNE_SPUM_BQF_CTRL(1),
			ATUNE_EQ_ENABLE_L, 1, 0),
	SOC_SINGLE("SPUM1 HPF EN", ATUNE_SPUM_BQF_CTRL(1),
			ATUNE_HPF_ENABLE_L, 1, 0),
	ATUNE_SPUM_BQF_HEADROOM_CONTROLS(1),
	ATUNE_SPUM_BQF_POSTAMP_CONTROLS(1),
	ATUNE_SPUM_BQF_COEF_CONTROLS(1),
	SOC_SINGLE("SPUM1 LIMITER WINDOW", ATUNE_SPUM_DRC_CTRL(1),
			ATUNE_LIMIT_WINDOW_SIZE_L, 0xff, 0),
	SOC_SINGLE_S_TLV("SPUM1 DRC NG THRESHOLD", ATUNE_SPUM_DRC_CTRL(1),
			ATUNE_NOISE_THRS_L, -108, -12, 7, 0,
			atune_ng_threshold_tlv),
	SOC_SINGLE("SPUM1 DRC NG HB EN", ATUNE_SPUM_DRC_CTRL(1),
			ATUNE_DRC_NG_ENABLE_HB_L, 1, 0),
	SOC_SINGLE("SPUM1 DRC NG MB EN", ATUNE_SPUM_DRC_CTRL(1),
			ATUNE_DRC_NG_ENABLE_MB_L, 1, 0),
	SOC_SINGLE("SPUM1 DRC NG LB EN", ATUNE_SPUM_DRC_CTRL(1),
			ATUNE_DRC_NG_ENABLE_LB_L, 1, 0),
	SOC_SINGLE("SPUM1 DRC HB EN", ATUNE_SPUM_DRC_CTRL(1),
			ATUNE_DRC_ENABLE_HB_L, 1, 0),
	SOC_SINGLE("SPUM1 DRC MB EN", ATUNE_SPUM_DRC_CTRL(1),
			ATUNE_DRC_ENABLE_MB_L, 1, 0),
	SOC_SINGLE("SPUM1 DRC LB EN", ATUNE_SPUM_DRC_CTRL(1),
			ATUNE_DRC_ENABLE_LB_L, 1, 0),
	SOC_SINGLE("SPUM1 LIMITER EN", ATUNE_SPUM_DRC_CTRL(1),
			ATUNE_LIMIT_ENABLE_L, 1, 0),
	SOC_SINGLE("SPUM1 DRC EN", ATUNE_SPUM_DRC_CTRL(1),
			ATUNE_DRC_ENABLE_L, 1, 0),
	ATUNE_SPUM_DRC_CONTROLS(1),
	SOC_SINGLE("SPUM1 LIMITER THRESHOLD", ATUNE_SPUM_DRC_LMT_CTRL0(1),
			ATUNE_THRESHOLD_L, INT_MAX, 0),
	SOC_SINGLE("SPUM1 LIMITER ATTACK", ATUNE_SPUM_DRC_LMT_CTRL1(1),
			ATUNE_ATTACK_TIME_L, INT_MAX, 0),
	ATUNE_MULTI_LINEAR("SPUM1 DRC XPF0 COEF", ATUNE_SPUM_DRC_xPF0_COEF0(1),
			ATUNE_COEF_L, 4, INT_MIN, INT_MAX, 31, 0),
	ATUNE_MULTI_LINEAR("SPUM1 DRC XPF1 COEF", ATUNE_SPUM_DRC_xPF1_COEF0(1),
			ATUNE_COEF_L, 4, INT_MIN, INT_MAX, 31, 0),
	SOC_ENUM("SPUM1 POSTGAIN FUNC", spum1_postgain_func_enum),
	SOC_SINGLE("SPUM1 POSTGAIN EN", ATUNE_SPUM_DSGAIN_CTRL(1),
			ATUNE_ENABLE_L, 1, 0),
	SOC_SINGLE_XR_SX("SPUM1 POSTGAIN VOL CHANGE FADE IN",
			ATUNE_SPUM_DSGAIN_VOL_CHANGE_FIN(1),
			1, 32, 0, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SPUM1 POSTGAIN VOL CHANGE FADE OUT",
			ATUNE_SPUM_DSGAIN_VOL_CHANGE_FOUT(1),
			1, 32, INT_MIN, 0, 0),
	ATUNE_SPUM_POSTGAIN_CONTROLS(1),
	SOC_ENUM("SPUM1 POSTGAIN Dither Type", spum1_postgain_dither_type_enum),
};

static const unsigned int spus_pre_tune_mask =
		ABOX_PRE_TUNE_SEL_MASK(0) >> ABOX_PRE_TUNE_SEL_L(0);
static const char * const spus_pre_tune_texts[] = {
	"SPUS0", "SPUS1", "SPUS2", "SPUS3",
	"SPUS4", "SPUS5", "SPUS6", "SPUS7",
	"SPUS8", "SPUS9", "SPUS10", "SPUS11",
	"RESERVED",
};
static const unsigned int spus_pre_tune_values[] = {
	0x0, 0x1, 0x2, 0x3,
	0x4, 0x5, 0x6, 0x7,
	0x8, 0x9, 0xa, 0xb,
	0xf,
};
static SOC_VALUE_ENUM_SINGLE_DECL(spus_pre_tune0_enum,
		ABOX_SPUS_CTRL_TUNE_SEL, ABOX_PRE_TUNE_SEL_L(0),
		spus_pre_tune_mask, spus_pre_tune_texts, spus_pre_tune_values);
static const struct snd_kcontrol_new spus_pre_tune0_controls[] = {
	SOC_DAPM_ENUM("SPUS PRETUNE0 SEL", spus_pre_tune0_enum),
};
static SOC_VALUE_ENUM_SINGLE_DECL(spus_pre_tune1_enum,
		ABOX_SPUS_CTRL_TUNE_SEL, ABOX_PRE_TUNE_SEL_L(1),
		spus_pre_tune_mask, spus_pre_tune_texts, spus_pre_tune_values);
static const struct snd_kcontrol_new spus_pre_tune1_controls[] = {
	SOC_DAPM_ENUM("SPUS PRETUNE1 SEL", spus_pre_tune1_enum),
};

static int atune_find_spus(struct snd_soc_component *cmpnt, int aid)
{
	unsigned int val;
	int ret;

	ret = snd_soc_component_read(cmpnt, ABOX_SPUS_CTRL_TUNE_SEL, &val);
	if (ret < 0)
		return ret;

	ret = (val & ABOX_PRE_TUNE_SEL_MASK(aid)) >> ABOX_PRE_TUNE_SEL_L(aid);
	if (ret > 0xb)
		ret = -EPIPE;

	return ret;
}

static const unsigned int spus_post_tune_mask =
		ABOX_POST_TUNE_SEL_MASK(0) >> ABOX_POST_TUNE_SEL_L(0);
static const char * const spus_post_tune_texts[] = {
	"SIFS0", "SIFS1", "SIFS2", "SIFS3",
	"SIFS4", "SIFS5", "SIFS6", "RESERVED",
};
static const unsigned int spus_post_tune_values[] = {
	0x0, 0x1, 0x2, 0x3,
	0x4, 0x5, 0x6, 0xf,
};
static SOC_VALUE_ENUM_SINGLE_DECL(spus_post_tune0_enum,
		ABOX_SPUS_CTRL_TUNE_SEL, ABOX_POST_TUNE_SEL_L(0),
		spus_post_tune_mask, spus_post_tune_texts,
		spus_post_tune_values);
static const struct snd_kcontrol_new spus_post_tune0_controls[] = {
	SOC_DAPM_ENUM("SPUS POSTTUNE0 SEL", spus_post_tune0_enum),
};
static SOC_VALUE_ENUM_SINGLE_DECL(spus_post_tune1_enum,
		ABOX_SPUS_CTRL_TUNE_SEL, ABOX_POST_TUNE_SEL_L(1),
		spus_post_tune_mask, spus_post_tune_texts,
		spus_post_tune_values);
static const struct snd_kcontrol_new spus_post_tune1_controls[] = {
	SOC_DAPM_ENUM("SPUS POSTTUNE1 SEL", spus_post_tune1_enum),
};

static int atune_find_sifs(struct snd_soc_component *cmpnt, int aid)
{
	unsigned int val;
	int ret;

	ret = snd_soc_component_read(cmpnt, ABOX_SPUS_CTRL_TUNE_SEL, &val);
	if (ret < 0)
		return ret;

	ret = (val & ABOX_POST_TUNE_SEL_MASK(aid)) >> ABOX_POST_TUNE_SEL_L(aid);
	if (ret > 0x6)
		ret = -EPIPE;

	return ret;
}

static const unsigned int spum_tune_mask =
		ABOX_PRE_TUNE_SEL_MASK(0) >> ABOX_PRE_TUNE_SEL_L(0);
static const char * const spum_tune_texts[] = {
	"SIFM0", "SIFM1", "SIFM2", "SIFM3",
	"SIFM4", "SIFM5", "SIFM6", "SIFM7",
	"RESERVED",
};
static const unsigned int spum_tune_values[] = {
	0x0, 0x1, 0x2, 0x3,
	0x4, 0x5, 0x6, 0x7,
	0xf,
};
static SOC_VALUE_ENUM_SINGLE_DECL(spum_pre_tune0_enum,
		ABOX_SPUM_CTRL_TUNE_SEL, ABOX_PRE_TUNE_SEL_L(0),
		spum_tune_mask, spum_tune_texts, spum_tune_values);
static const struct snd_kcontrol_new spum_pre_tune0_controls[] = {
	SOC_DAPM_ENUM("SPUM PRETUNE0 SEL", spum_pre_tune0_enum),
};
static SOC_VALUE_ENUM_SINGLE_DECL(spum_pre_tune1_enum,
		ABOX_SPUM_CTRL_TUNE_SEL, ABOX_PRE_TUNE_SEL_L(1),
		spum_tune_mask, spum_tune_texts, spum_tune_values);
static const struct snd_kcontrol_new spum_pre_tune1_controls[] = {
	SOC_DAPM_ENUM("SPUM PRETUNE1 SEL", spum_pre_tune1_enum),
};
static SOC_VALUE_ENUM_SINGLE_DECL(spum_post_tune0_enum,
		ABOX_SPUM_CTRL_TUNE_SEL, ABOX_POST_TUNE_SEL_L(0),
		spum_tune_mask, spum_tune_texts, spum_tune_values);
static const struct snd_kcontrol_new spum_post_tune0_controls[] = {
	SOC_DAPM_ENUM("SPUM POSTTUNE0 SEL", spum_post_tune0_enum),
};
static SOC_VALUE_ENUM_SINGLE_DECL(spum_post_tune1_enum,
		ABOX_SPUM_CTRL_TUNE_SEL, ABOX_POST_TUNE_SEL_L(1),
		spum_tune_mask, spum_tune_texts, spum_tune_values);
static const struct snd_kcontrol_new spum_post_tune1_controls[] = {
	SOC_DAPM_ENUM("SPUM POSTTUNE1 SEL", spum_post_tune1_enum),
};

static int atune_find_nsrc(struct snd_soc_component *cmpnt, int aid)
{
	unsigned int val;
	int ret;

	ret = snd_soc_component_read(cmpnt, ABOX_SPUM_CTRL_TUNE_SEL, &val);
	if (ret < 0)
		return ret;

	ret = (val & ABOX_PRE_TUNE_SEL_MASK(aid)) >> ABOX_PRE_TUNE_SEL_L(aid);
	if (ret > 0x7)
		ret = -EPIPE;

	return ret;
}

static int atune_find_sifm(struct snd_soc_component *cmpnt, int aid)
{
	unsigned int val;
	int ret;

	ret = snd_soc_component_read(cmpnt, ABOX_SPUM_CTRL_TUNE_SEL, &val);
	if (ret < 0)
		return ret;

	ret = (val & ABOX_POST_TUNE_SEL_MASK(aid)) >> ABOX_POST_TUNE_SEL_L(aid);
	if (ret > 0x7)
		ret = -EPIPE;

	return ret;
}

static int sifs_from_spus(struct abox_data *data, int sid)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int val;
	int ret;

	ret = snd_soc_component_read(cmpnt, ABOX_SPUS_CTRL_FC_SRC(sid), &val);
	if (ret < 0)
		return ret;

	val &= ABOX_FUNC_CHAIN_SRC_SIFS_MASK(sid);
	val >>= ABOX_FUNC_CHAIN_SRC_SIFS_L(sid);
	return (int)val;
}

static int spus_posttune_enabled(struct abox_data *data, int sid)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int val = 0;
	unsigned int mask;

	snd_soc_component_read(cmpnt, ABOX_SPUS_CTRL_FC_SRC(sid), &val);
	mask = ABOX_FUNC_CHAIN_SRC_BQF_MASK(sid) |
			ABOX_FUNC_CHAIN_SRC_DRC_MASK(sid) |
			ABOX_FUNC_CHAIN_SRC_DSG_MASK(sid);

	return !!(val & mask);
}

static int spum_pretune_enabled(struct abox_data *data, int sid)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int val = 0;
	unsigned int mask;

	snd_soc_component_read(cmpnt, ABOX_SPUM_CTRL_FC_NSRC(sid), &val);
	mask = ABOX_FUNC_CHAIN_NSRC_USG_MASK(sid) |
			ABOX_FUNC_CHAIN_NSRC_BQF_MASK(sid);

	return !!(val & mask);
}

static int spus_pregain_event(struct abox_data *data, int e, int aid, int sid)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	struct device *dev_dma;
	unsigned int shift, mask, val, width, channels;
	int ret = 0;
	struct snd_pcm_hw_params params;

	if (aid < 0 || sid < 0)
		return 0;

	abox_dbg(data->dev, "%s(%d, %d, %d)\n", __func__, e, aid, sid);

	if (SND_SOC_DAPM_EVENT_ON(e)) {
		/* sync function chain */
		ret = snd_soc_component_read(cmpnt, ATUNE_SPUS_USGAIN_CTRL(aid),
				&val);
		if (ret < 0)
			goto out;

		val = !!(val & ATUNE_ENABLE_MASK);
		shift = ABOX_FUNC_CHAIN_SRC_USG_L(sid);
		mask = ABOX_FUNC_CHAIN_SRC_USG_MASK(sid);
		ret = snd_soc_component_update_bits(cmpnt,
				ABOX_SPUS_CTRL_FC_SRC(sid),
				mask, val << shift);
		if (ret < 0) {
			abox_err(data->dev, "%s: function chain set fail: %d\n",
					__func__, ret);
			goto out;
		}

		abox_dbg(data->dev, "%s(%d, %d, %d): connected\n",
				__func__, e, aid, sid);

		if (!spus_posttune_enabled(data, sid)) {
			/* if pretune only is enabled,
			 * enable ASRC for bit conversion
			 */
			abox_cmpnt_asrc_enable(data, SNDRV_PCM_STREAM_PLAYBACK,
					sid);
		}

		/* sync format */
		dev_dma = data->dev_rdma[sid];
		ret = abox_dma_hw_params_fixup(dev_dma, &params);
		if (ret < 0) {
			abox_err(data->dev, "%s: hw params get failed: %d\n",
					__func__, ret);
			goto out;
		}
		width = abox_dma_get_dst_bit_width(dev_dma);
		channels = params_channels(&params);
		val = abox_get_format(width, channels);
		shift = ATUNE_FORMAT_L;
		mask = ATUNE_FORMAT_MASK;
		ret = snd_soc_component_update_bits(cmpnt,
				ATUNE_SPUS_USGAIN_CTRL(aid),
				mask, val << shift);
		abox_dbg(data->dev, "%s(%d, %d): input format=%#x\n", __func__,
				aid, sid, val);
	}
out:
	return ret;
}

static int spus_pregain0_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	return spus_pregain_event(NULL, e, 0, 0);
}

static int spus_pregain1_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	return spus_pregain_event(NULL, e, 1, 0);
}

static int spus_bqf_event(struct abox_data *data, int e, int aid, int sid)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int shift, mask, val;
	int ret = 0;

	if (aid < 0 || sid < 0)
		return 0;

	abox_dbg(data->dev, "%s(%d, %d, %d)\n", __func__, e, aid, sid);

	if (SND_SOC_DAPM_EVENT_ON(e)) {
		/* sync function chain */
		ret = snd_soc_component_read(cmpnt, ATUNE_SPUS_BQF_CTRL(aid),
				&val);
		if (ret < 0)
			goto out;

		val = !!(val & (ATUNE_EQ_ENABLE_MASK | ATUNE_HPF_ENABLE_MASK));
		shift = ABOX_FUNC_CHAIN_SRC_BQF_L(sid);
		mask = ABOX_FUNC_CHAIN_SRC_BQF_MASK(sid);
		ret = snd_soc_component_update_bits(cmpnt,
				ABOX_SPUS_CTRL_FC_SRC(sid),
				mask, val << shift);
		if (ret < 0) {
			abox_err(data->dev, "%s: function chain set fail: %d\n",
					__func__, ret);
			goto out;
		}

		abox_dbg(data->dev, "%s(%d, %d, %d): connected\n",
				__func__, e, aid, sid);

		/* atune can process 32bit only */
		abox_dma_set_dst_bit_width(data->dev_rdma[sid], 32);
	}
out:
	return ret;
}

static int spus_bqf0_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	/* later use */
	return 0;
}

static int spus_bqf1_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	/* later use */
	return 0;
}

static int spus_drc_event(struct abox_data *data, int e, int aid, int sid)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int shift, mask, val;
	int ret = 0;

	if (aid < 0 || sid < 0)
		return 0;

	abox_dbg(data->dev, "%s(%d, %d, %d)\n", __func__, e, aid, sid);

	if (SND_SOC_DAPM_EVENT_ON(e)) {
		/* sync function chain */
		ret = snd_soc_component_read(cmpnt, ATUNE_SPUS_DRC_CTRL(aid),
				&val);
		if (ret < 0)
			goto out;

		val = !!(val & (ATUNE_DRC_ENABLE_MASK |
				ATUNE_LIMIT_ENABLE_MASK));
		shift = ABOX_FUNC_CHAIN_SRC_DRC_L(sid);
		mask = ABOX_FUNC_CHAIN_SRC_DRC_MASK(sid);
		ret = snd_soc_component_update_bits(cmpnt,
				ABOX_SPUS_CTRL_FC_SRC(sid),
				mask, val << shift);
		if (ret < 0) {
			abox_err(data->dev, "%s: function chain set fail: %d\n",
					__func__, ret);
			goto out;
		}

		abox_dbg(data->dev, "%s(%d, %d, %d): connected\n",
				__func__, e, aid, sid);

		/* atune can process 32bit only */
		abox_dma_set_dst_bit_width(data->dev_rdma[sid], 32);
	}
out:
	return ret;
}

static int spus_drc0_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	/* later use */
	return 0;
}

static int spus_drc1_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	/* later use */
	return 0;
}

static int spus_postgain_event(struct abox_data *data, int e, int aid, int sid)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int shift, mask, val, rate, format;
	int sifs_id, ret = 0;

	if (aid < 0 || sid < 0)
		return 0;

	abox_dbg(data->dev, "%s(%d, %d, %d)\n", __func__, e, aid, sid);

	if (SND_SOC_DAPM_EVENT_ON(e)) {
		/* sync function chain */
		ret = snd_soc_component_read(cmpnt, ATUNE_SPUS_DSGAIN_CTRL(aid),
				&val);
		if (ret < 0)
			goto out;

		val = !!(val & ATUNE_ENABLE_MASK);
		shift = ABOX_FUNC_CHAIN_SRC_DSG_L(sid);
		mask = ABOX_FUNC_CHAIN_SRC_DSG_MASK(sid);
		ret = snd_soc_component_update_bits(cmpnt,
				ABOX_SPUS_CTRL_FC_SRC(sid),
				mask, val << shift);
		if (ret < 0) {
			abox_err(data->dev, "%s: function chain set fail: %d\n",
					__func__, ret);
			goto out;
		}

		abox_dbg(data->dev, "%s(%d, %d, %d): connected\n",
				__func__, e, aid, sid);

		/* atune can process 32bit only */
		abox_dma_set_dst_bit_width(data->dev_rdma[sid], 32);

		/* get sifs id */
		ret = snd_soc_component_read(cmpnt, ABOX_SPUS_CTRL_FC_SRC(sid),
				&val);
		if (ret < 0)
			goto out;
		sifs_id = sifs_from_spus(data, sid);

		/* get rate */
		rate = abox_cmpnt_sif_get_dst_rate(data,
				SNDRV_PCM_STREAM_PLAYBACK, sifs_id);

		/* sync rate */
		val = ilog2((rate / 48000));
		shift = ATUNE_SAMPLE_RATE_L;
		mask = ATUNE_SAMPLE_RATE_MASK;
		ret = snd_soc_component_update_bits(cmpnt,
				ATUNE_SPUS_DSGAIN_CTRL(aid),
				mask, val << shift);
		abox_dbg(data->dev, "%s(%d, %d): rate=%#x\n", __func__,
				aid, sid, val);

		/* get format */
		format = abox_cmpnt_sif_get_dst_format(data,
				SNDRV_PCM_STREAM_PLAYBACK, sifs_id);

		/* sync input format */
		val = format | (0x3 << 3); /* 32bit fixed */
		shift = ATUNE_FORMAT_L;
		mask = ATUNE_FORMAT_MASK;
		ret = snd_soc_component_update_bits(cmpnt,
				ATUNE_SPUS_DSGAIN_CTRL(aid),
				mask, val << shift);
		abox_dbg(data->dev, "%s(%d, %d): input format=%#x\n", __func__,
				aid, sid, val);

		/* sync output format */
		val = format >> 0x3;
		shift = ATUNE_DITHER_OUTPUT_BIT_L;
		mask = ATUNE_DITHER_OUTPUT_BIT_MASK;
		ret = snd_soc_component_update_bits(cmpnt,
				ATUNE_SPUS_DSGAIN_BIT_CTRL(aid),
				mask, val << shift);
		abox_dbg(data->dev, "%s(%d, %d): output format=%#x\n", __func__,
				aid, sid, val);
	}
out:
	return ret;
}

static int spus_postgain0_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	/* later use */
	return 0;
}

static int spus_postgain1_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	/* later use */
	return 0;
}

static int spum_pregain_event(struct abox_data *data, int e, int aid, int sid)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int shift, mask, val;
	int ret = 0;

	if (aid < 0 || sid < 0)
		return 0;

	abox_dbg(data->dev, "%s(%d, %d, %d)\n", __func__, e, aid, sid);

	if (SND_SOC_DAPM_EVENT_ON(e)) {
		/* sync function chain */
		ret = snd_soc_component_read(cmpnt, ATUNE_SPUM_USGAIN_CTRL(aid),
				&val);
		if (ret < 0)
			goto out;

		val = !!(val & ATUNE_ENABLE_MASK);
		shift = ABOX_FUNC_CHAIN_NSRC_USG_L(sid);
		mask = ABOX_FUNC_CHAIN_NSRC_USG_MASK(sid);
		ret = snd_soc_component_update_bits(cmpnt,
				ABOX_SPUM_CTRL_FC_NSRC(sid),
				mask, val << shift);
		if (ret < 0) {
			abox_err(data->dev, "%s: function chain set fail: %d\n",
					__func__, ret);
			goto out;
		}

		abox_dbg(data->dev, "%s(%d, %d, %d): connected\n",
				__func__, e, aid, sid);

		/* atune can process 32bit only */
		abox_dma_set_dst_bit_width(data->dev_wdma[sid], 32);

		/* sync format */
		val = abox_cmpnt_sif_get_dst_format(data,
				SNDRV_PCM_STREAM_CAPTURE, sid);
		shift = ATUNE_FORMAT_L;
		mask = ATUNE_FORMAT_MASK;
		ret = snd_soc_component_update_bits(cmpnt,
				ATUNE_SPUM_USGAIN_CTRL(aid),
				mask, val << shift);
		abox_dbg(data->dev, "%s(%d, %d): input format=%#x\n", __func__,
				aid, sid, val);
	}
out:
	return ret;
}

static int spum_pregain0_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	/* later use */
	return 0;
}

static int spum_pregain1_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	/* later use */
	return 0;
}

static int spum_bqf_event(struct abox_data *data, int e, int aid, int sid)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int shift, mask, val;
	int ret = 0;

	if (aid < 0 || sid < 0)
		return 0;

	abox_dbg(data->dev, "%s(%d, %d, %d)\n", __func__, e, aid, sid);

	if (SND_SOC_DAPM_EVENT_ON(e)) {
		/* sync function chain */
		ret = snd_soc_component_read(cmpnt, ATUNE_SPUM_BQF_CTRL(aid),
				&val);
		if (ret < 0)
			goto out;

		val = !!(val & (ATUNE_EQ_ENABLE_MASK | ATUNE_HPF_ENABLE_MASK));
		shift = ABOX_FUNC_CHAIN_NSRC_BQF_L(sid);
		mask = ABOX_FUNC_CHAIN_NSRC_BQF_MASK(sid);
		ret = snd_soc_component_update_bits(cmpnt,
				ABOX_SPUM_CTRL_FC_NSRC(sid),
				mask, val << shift);
		if (ret < 0) {
			abox_err(data->dev, "%s: function chain set fail: %d\n",
					__func__, ret);
			goto out;
		}

		abox_dbg(data->dev, "%s(%d, %d, %d): connected\n",
				__func__, e, aid, sid);

		/* atune can process 32bit only */
		abox_dma_set_dst_bit_width(data->dev_wdma[sid], 32);
	}
out:
	return ret;
}

static int spum_bqf0_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	/* later use */
	return 0;
}

static int spum_bqf1_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	/* later use */
	return 0;
}

static int spum_drc_event(struct abox_data *data, int e, int aid, int sid)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int shift, mask, val;
	int ret = 0;

	if (aid < 0 || sid < 0)
		return 0;

	abox_dbg(data->dev, "%s(%d, %d, %d)\n", __func__, e, aid, sid);

	if (SND_SOC_DAPM_EVENT_ON(e)) {
		/* sync function chain */
		ret = snd_soc_component_read(cmpnt, ATUNE_SPUM_DRC_CTRL(aid),
				&val);
		if (ret < 0)
			goto out;

		val = !!(val & (ATUNE_DRC_ENABLE_MASK |
				ATUNE_LIMIT_ENABLE_MASK));
		shift = ABOX_FUNC_CHAIN_NSRC_DRC_L(sid);
		mask = ABOX_FUNC_CHAIN_NSRC_DRC_MASK(sid);
		ret = snd_soc_component_update_bits(cmpnt,
				ABOX_SPUM_CTRL_FC_NSRC(sid),
				mask, val << shift);
		if (ret < 0) {
			abox_err(data->dev, "%s: function chain set fail: %d\n",
					__func__, ret);
			goto out;
		}

		abox_dbg(data->dev, "%s(%d, %d, %d): connected\n",
				__func__, e, aid, sid);

		if (!spum_pretune_enabled(data, sid)) {
			/* if posttune only is enabled,
			 * enable ASRC for bit conversion
			 */
			abox_cmpnt_asrc_enable(data, SNDRV_PCM_STREAM_CAPTURE,
					sid);
		}
	}
out:
	return ret;
}

static int spum_drc0_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	/* later use */
	return 0;
}

static int spum_drc1_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	/* later use */
	return 0;
}

static int spum_postgain_event(struct abox_data *data, int e, int aid, int sid)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	struct device *dev_dma;
	unsigned int shift, mask, val, width, channels, rate;
	int ret = 0;
	struct snd_pcm_hw_params params;

	if (aid < 0 || sid < 0)
		return 0;

	abox_dbg(data->dev, "%s(%d, %d, %d)\n", __func__, e, aid, sid);

	if (SND_SOC_DAPM_EVENT_ON(e)) {
		/* sync function chain */
		ret = snd_soc_component_read(cmpnt, ATUNE_SPUM_DSGAIN_CTRL(aid),
				&val);
		if (ret < 0)
			goto out;

		val = !!(val & ATUNE_ENABLE_MASK);
		shift = ABOX_FUNC_CHAIN_NSRC_DSG_L(sid);
		mask = ABOX_FUNC_CHAIN_NSRC_DSG_MASK(sid);
		ret = snd_soc_component_update_bits(cmpnt,
				ABOX_SPUM_CTRL_FC_NSRC(sid),
				mask, val << shift);
		if (ret < 0) {
			abox_err(data->dev, "%s: function chain set fail: %d\n",
					__func__, ret);
			goto out;
		}

		abox_dbg(data->dev, "%s(%d, %d, %d): connected\n",
				__func__, e, aid, sid);

		if (!spum_pretune_enabled(data, sid)) {
			/* if posttune only is enabled,
			 * enable ASRC for bit conversion
			 */
			abox_cmpnt_asrc_enable(data, SNDRV_PCM_STREAM_CAPTURE,
					sid);
		}

		/* get format */
		dev_dma = data->dev_wdma[sid];
		ret = abox_dma_hw_params_fixup(dev_dma, &params);
		if (ret < 0) {
			abox_err(dev_dma, "%s: hw params get failed: %d\n",
					__func__, ret);
			goto out;
		}
		rate = params_rate(&params);
		channels = params_channels(&params);
		width = abox_dma_get_dst_bit_width(dev_dma);

		/* sync rate */
		val = ilog2((rate / 48000));
		shift = ATUNE_SAMPLE_RATE_L;
		mask = ATUNE_SAMPLE_RATE_MASK;
		ret = snd_soc_component_update_bits(cmpnt,
				ATUNE_SPUM_DSGAIN_CTRL(aid),
				mask, val << shift);
		abox_dbg(data->dev, "%s(%d, %d): rate=%#x\n", __func__,
				aid, sid, val);

		/* sync input format */
		val = abox_get_format(32, channels); /* 32bit fixed */
		shift = ATUNE_FORMAT_L;
		mask = ATUNE_FORMAT_MASK;
		ret = snd_soc_component_update_bits(cmpnt,
				ATUNE_SPUM_DSGAIN_CTRL(aid),
				mask, val << shift);
		abox_dbg(data->dev, "%s(%d, %d): input format=%#x\n", __func__,
				aid, sid, val);

		/* sync output format */
		val = (width / 8) - 1;
		shift = ATUNE_DITHER_OUTPUT_BIT_L;
		mask = ATUNE_DITHER_OUTPUT_BIT_MASK;
		ret = snd_soc_component_update_bits(cmpnt,
				ATUNE_SPUM_DSGAIN_BIT_CTRL(aid),
				mask, val << shift);
		abox_dbg(data->dev, "%s(%d, %d): output format=%#x\n", __func__,
				aid, sid, val);
	}
out:
	return ret;
}

static int spum_postgain0_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	/* later use */
	return 0;
}

static int spum_postgain1_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	/* later use */
	return 0;
}

/* later use */
#define ATUNE_EVENT_FLAGS 0

static const struct snd_soc_dapm_widget atune_widgets[] = {
	SND_SOC_DAPM_MUX("SPUS PRETUNE0 IN", SND_SOC_NOPM, 0, 0,
			spus_pre_tune0_controls),
	SND_SOC_DAPM_DEMUX("SPUS PRETUNE0 OUT", SND_SOC_NOPM, 0, 0,
			spus_pre_tune0_controls),
	SND_SOC_DAPM_MUX("SPUS POSTTUNE0 IN", SND_SOC_NOPM, 0, 0,
			spus_post_tune0_controls),
	SND_SOC_DAPM_DEMUX("SPUS POSTTUNE0 OUT", SND_SOC_NOPM, 0, 0,
			spus_post_tune0_controls),
	SND_SOC_DAPM_PGA_E("SPUS PREGAIN0", SND_SOC_NOPM, 0, 0, NULL, 0,
			spus_pregain0_event, ATUNE_EVENT_FLAGS),
	SND_SOC_DAPM_PGA_E("SPUS BQF0", SND_SOC_NOPM, 0, 0, NULL, 0,
			spus_bqf0_event, ATUNE_EVENT_FLAGS),
	SND_SOC_DAPM_PGA_E("SPUS DRC0", SND_SOC_NOPM, 0, 0, NULL, 0,
			spus_drc0_event, ATUNE_EVENT_FLAGS),
	SND_SOC_DAPM_PGA_E("SPUS POSTGAIN0", SND_SOC_NOPM, 0, 0, NULL, 0,
			spus_postgain0_event, ATUNE_EVENT_FLAGS),

	SND_SOC_DAPM_MUX("SPUS PRETUNE1 IN", SND_SOC_NOPM, 0, 0,
			spus_pre_tune1_controls),
	SND_SOC_DAPM_DEMUX("SPUS PRETUNE1 OUT", SND_SOC_NOPM, 0, 0,
			spus_pre_tune1_controls),
	SND_SOC_DAPM_MUX("SPUS POSTTUNE1 IN", SND_SOC_NOPM, 0, 0,
			spus_post_tune1_controls),
	SND_SOC_DAPM_DEMUX("SPUS POSTTUNE1 OUT", SND_SOC_NOPM, 0, 0,
			spus_post_tune1_controls),
	SND_SOC_DAPM_PGA_E("SPUS PREGAIN1", SND_SOC_NOPM, 0, 0, NULL, 0,
			spus_pregain1_event, ATUNE_EVENT_FLAGS),
	SND_SOC_DAPM_PGA_E("SPUS BQF1", SND_SOC_NOPM, 0, 0, NULL, 0,
			spus_bqf1_event, ATUNE_EVENT_FLAGS),
	SND_SOC_DAPM_PGA_E("SPUS DRC1", SND_SOC_NOPM, 0, 0, NULL, 0,
			spus_drc1_event, ATUNE_EVENT_FLAGS),
	SND_SOC_DAPM_PGA_E("SPUS POSTGAIN1", SND_SOC_NOPM, 0, 0, NULL, 0,
			spus_postgain1_event, ATUNE_EVENT_FLAGS),

	SND_SOC_DAPM_MUX("SPUM PRETUNE0 IN", SND_SOC_NOPM, 0, 0,
			spum_pre_tune0_controls),
	SND_SOC_DAPM_DEMUX("SPUM PRETUNE0 OUT", SND_SOC_NOPM, 0, 0,
			spum_pre_tune0_controls),
	SND_SOC_DAPM_MUX("SPUM POSTTUNE0 IN", SND_SOC_NOPM, 0, 0,
			spum_post_tune0_controls),
	SND_SOC_DAPM_DEMUX("SPUM POSTTUNE0 OUT", SND_SOC_NOPM, 0, 0,
			spum_post_tune0_controls),
	SND_SOC_DAPM_PGA_E("SPUM PREGAIN0", SND_SOC_NOPM, 0, 0, NULL, 0,
			spum_pregain0_event, ATUNE_EVENT_FLAGS),
	SND_SOC_DAPM_PGA_E("SPUM BQF0", SND_SOC_NOPM, 0, 0, NULL, 0,
			spum_bqf0_event, ATUNE_EVENT_FLAGS),
	SND_SOC_DAPM_PGA_E("SPUM DRC0", SND_SOC_NOPM, 0, 0, NULL, 0,
			spum_drc0_event, ATUNE_EVENT_FLAGS),
	SND_SOC_DAPM_PGA_E("SPUM POSTGAIN0", SND_SOC_NOPM, 0, 0, NULL, 0,
			spum_postgain0_event, ATUNE_EVENT_FLAGS),

	SND_SOC_DAPM_MUX("SPUM PRETUNE1 IN", SND_SOC_NOPM, 0, 0,
			spum_pre_tune1_controls),
	SND_SOC_DAPM_DEMUX("SPUM PRETUNE1 OUT", SND_SOC_NOPM, 0, 0,
			spum_pre_tune1_controls),
	SND_SOC_DAPM_MUX("SPUM POSTTUNE1 IN", SND_SOC_NOPM, 0, 0,
			spum_post_tune1_controls),
	SND_SOC_DAPM_DEMUX("SPUM POSTTUNE1 OUT", SND_SOC_NOPM, 0, 0,
			spum_post_tune1_controls),
	SND_SOC_DAPM_PGA_E("SPUM PREGAIN1", SND_SOC_NOPM, 0, 0, NULL, 0,
			spum_pregain1_event, ATUNE_EVENT_FLAGS),
	SND_SOC_DAPM_PGA_E("SPUM BQF1", SND_SOC_NOPM, 0, 0, NULL, 0,
			spum_bqf1_event, ATUNE_EVENT_FLAGS),
	SND_SOC_DAPM_PGA_E("SPUM DRC1", SND_SOC_NOPM, 0, 0, NULL, 0,
			spum_drc1_event, ATUNE_EVENT_FLAGS),
	SND_SOC_DAPM_PGA_E("SPUM POSTGAIN1", SND_SOC_NOPM, 0, 0, NULL, 0,
			spum_postgain1_event, ATUNE_EVENT_FLAGS),
};

static const struct snd_soc_dapm_route atune_routes[] = {
	/* sink, control, source */
	{"SPUS PRETUNE0 IN", "SPUS0", "SPUS IN0"},
	{"SPUS PRETUNE0 IN", "SPUS1", "SPUS IN1"},
	{"SPUS PRETUNE0 IN", "SPUS2", "SPUS IN2"},
	{"SPUS PRETUNE0 IN", "SPUS3", "SPUS IN3"},
	{"SPUS PRETUNE0 IN", "SPUS4", "SPUS IN4"},
	{"SPUS PRETUNE0 IN", "SPUS5", "SPUS IN5"},
	{"SPUS PRETUNE0 IN", "SPUS6", "SPUS IN6"},
	{"SPUS PRETUNE0 IN", "SPUS7", "SPUS IN7"},
	{"SPUS PRETUNE0 IN", "SPUS8", "SPUS IN8"},
	{"SPUS PRETUNE0 IN", "SPUS9", "SPUS IN9"},
	{"SPUS PRETUNE0 IN", "SPUS10", "SPUS IN10"},
	{"SPUS PRETUNE0 IN", "SPUS11", "SPUS IN11"},
	{"SPUS PREGAIN0", NULL, "SPUS PRETUNE0 IN"},
	{"SPUS PRETUNE0 OUT", NULL, "SPUS PREGAIN0"},
	{"SPUS PGA0",  "SPUS0", "SPUS PRETUNE0 OUT"},
	{"SPUS PGA1",  "SPUS1", "SPUS PRETUNE0 OUT"},
	{"SPUS PGA2",  "SPUS2", "SPUS PRETUNE0 OUT"},
	{"SPUS PGA3",  "SPUS3", "SPUS PRETUNE0 OUT"},
	{"SPUS PGA4",  "SPUS4", "SPUS PRETUNE0 OUT"},
	{"SPUS PGA5",  "SPUS5", "SPUS PRETUNE0 OUT"},
	{"SPUS PGA6",  "SPUS6", "SPUS PRETUNE0 OUT"},
	{"SPUS PGA7",  "SPUS7", "SPUS PRETUNE0 OUT"},
	{"SPUS PGA8",  "SPUS8", "SPUS PRETUNE0 OUT"},
	{"SPUS PGA9",  "SPUS9", "SPUS PRETUNE0 OUT"},
	{"SPUS PGA10", "SPUS10", "SPUS PRETUNE0 OUT"},
	{"SPUS PGA11", "SPUS11", "SPUS PRETUNE0 OUT"},

	{"SPUS POSTTUNE0 IN", "SIFS0", "SIFS0"},
	{"SPUS POSTTUNE0 IN", "SIFS1", "SIFS1"},
	{"SPUS POSTTUNE0 IN", "SIFS2", "SIFS2"},
	{"SPUS POSTTUNE0 IN", "SIFS3", "SIFS3"},
	{"SPUS POSTTUNE0 IN", "SIFS4", "SIFS4"},
	{"SPUS POSTTUNE0 IN", "SIFS5", "SIFS5"},
	{"SPUS POSTTUNE0 IN", "SIFS6", "SIFS6"},
	{"SPUS BQF0", NULL, "SPUS POSTTUNE0 IN"},
	{"SPUS DRC0", NULL, "SPUS BQF0"},
	{"SPUS POSTGAIN0", NULL, "SPUS DRC0"},
	{"SPUS POSTTUNE0 OUT", NULL, "SPUS POSTGAIN0"},
	{"SIFS0 PGA", "SIFS0", "SPUS POSTTUNE0 OUT"},
	{"SIFS1 PGA", "SIFS1", "SPUS POSTTUNE0 OUT"},
	{"SIFS2 PGA", "SIFS2", "SPUS POSTTUNE0 OUT"},
	{"SIFS3 PGA", "SIFS3", "SPUS POSTTUNE0 OUT"},
	{"SIFS4 PGA", "SIFS4", "SPUS POSTTUNE0 OUT"},
	{"SIFS5 PGA", "SIFS5", "SPUS POSTTUNE0 OUT"},
	{"SIFS6 PGA", "SIFS6", "SPUS POSTTUNE0 OUT"},

	{"SPUS PRETUNE1 IN", "SPUS0", "SPUS IN0"},
	{"SPUS PRETUNE1 IN", "SPUS1", "SPUS IN1"},
	{"SPUS PRETUNE1 IN", "SPUS2", "SPUS IN2"},
	{"SPUS PRETUNE1 IN", "SPUS3", "SPUS IN3"},
	{"SPUS PRETUNE1 IN", "SPUS4", "SPUS IN4"},
	{"SPUS PRETUNE1 IN", "SPUS5", "SPUS IN5"},
	{"SPUS PRETUNE1 IN", "SPUS6", "SPUS IN6"},
	{"SPUS PRETUNE1 IN", "SPUS7", "SPUS IN7"},
	{"SPUS PRETUNE1 IN", "SPUS8", "SPUS IN8"},
	{"SPUS PRETUNE1 IN", "SPUS9", "SPUS IN9"},
	{"SPUS PRETUNE1 IN", "SPUS10", "SPUS IN10"},
	{"SPUS PRETUNE1 IN", "SPUS11", "SPUS IN11"},
	{"SPUS PREGAIN1", NULL, "SPUS PRETUNE1 IN"},
	{"SPUS PRETUNE1 OUT", NULL, "SPUS PREGAIN1"},
	{"SPUS PGA0",  "SPUS0", "SPUS PRETUNE1 OUT"},
	{"SPUS PGA1",  "SPUS1", "SPUS PRETUNE1 OUT"},
	{"SPUS PGA2",  "SPUS2", "SPUS PRETUNE1 OUT"},
	{"SPUS PGA3",  "SPUS3", "SPUS PRETUNE1 OUT"},
	{"SPUS PGA4",  "SPUS4", "SPUS PRETUNE1 OUT"},
	{"SPUS PGA5",  "SPUS5", "SPUS PRETUNE1 OUT"},
	{"SPUS PGA6",  "SPUS6", "SPUS PRETUNE1 OUT"},
	{"SPUS PGA7",  "SPUS7", "SPUS PRETUNE1 OUT"},
	{"SPUS PGA8",  "SPUS8", "SPUS PRETUNE1 OUT"},
	{"SPUS PGA9",  "SPUS9", "SPUS PRETUNE1 OUT"},
	{"SPUS PGA10", "SPUS10", "SPUS PRETUNE1 OUT"},
	{"SPUS PGA11", "SPUS11", "SPUS PRETUNE1 OUT"},

	{"SPUS POSTTUNE1 IN", "SIFS0", "SIFS0"},
	{"SPUS POSTTUNE1 IN", "SIFS1", "SIFS1"},
	{"SPUS POSTTUNE1 IN", "SIFS2", "SIFS2"},
	{"SPUS POSTTUNE1 IN", "SIFS3", "SIFS3"},
	{"SPUS POSTTUNE1 IN", "SIFS4", "SIFS4"},
	{"SPUS POSTTUNE1 IN", "SIFS5", "SIFS5"},
	{"SPUS POSTTUNE1 IN", "SIFS6", "SIFS6"},
	{"SPUS BQF1", NULL, "SPUS POSTTUNE1 IN"},
	{"SPUS DRC1", NULL, "SPUS BQF1"},
	{"SPUS POSTGAIN1", NULL, "SPUS DRC1"},
	{"SPUS POSTTUNE1 OUT", NULL, "SPUS POSTGAIN1"},
	{"SIFS0 PGA", "SIFS0", "SPUS POSTTUNE1 OUT"},
	{"SIFS1 PGA", "SIFS1", "SPUS POSTTUNE1 OUT"},
	{"SIFS2 PGA", "SIFS2", "SPUS POSTTUNE1 OUT"},
	{"SIFS3 PGA", "SIFS3", "SPUS POSTTUNE1 OUT"},
	{"SIFS4 PGA", "SIFS4", "SPUS POSTTUNE1 OUT"},
	{"SIFS5 PGA", "SIFS5", "SPUS POSTTUNE1 OUT"},
	{"SIFS6 PGA", "SIFS6", "SPUS POSTTUNE1 OUT"},

	{"SPUM PRETUNE0 IN", "SIFM0", "NSRC0"},
	{"SPUM PRETUNE0 IN", "SIFM1", "NSRC1"},
	{"SPUM PRETUNE0 IN", "SIFM2", "NSRC2"},
	{"SPUM PRETUNE0 IN", "SIFM3", "NSRC3"},
	{"SPUM PRETUNE0 IN", "SIFM4", "NSRC4"},
	{"SPUM PRETUNE0 IN", "SIFM5", "NSRC5"},
	{"SPUM PRETUNE0 IN", "SIFM6", "NSRC6"},
	{"SPUM PRETUNE0 IN", "SIFM7", "NSRC7"},
	{"SPUM PREGAIN0", NULL, "SPUM PRETUNE0 IN"},
	{"SPUM BQF0", NULL, "SPUM PREGAIN0"},
	{"SPUM PRETUNE0 OUT", NULL, "SPUM BQF0"},
	{"NSRC0 PGA", "SIFM0", "SPUM PRETUNE0 OUT"},
	{"NSRC1 PGA", "SIFM1", "SPUM PRETUNE0 OUT"},
	{"NSRC2 PGA", "SIFM2", "SPUM PRETUNE0 OUT"},
	{"NSRC3 PGA", "SIFM3", "SPUM PRETUNE0 OUT"},
	{"NSRC4 PGA", "SIFM4", "SPUM PRETUNE0 OUT"},
	{"NSRC5 PGA", "SIFM5", "SPUM PRETUNE0 OUT"},
	{"NSRC6 PGA", "SIFM6", "SPUM PRETUNE0 OUT"},
	{"NSRC7 PGA", "SIFM7", "SPUM PRETUNE0 OUT"},

	{"SPUM POSTTUNE0 IN", "SIFM0", "SPUM ASRC0"},
	{"SPUM POSTTUNE0 IN", "SIFM1", "SPUM ASRC1"},
	{"SPUM POSTTUNE0 IN", "SIFM2", "SPUM ASRC2"},
	{"SPUM POSTTUNE0 IN", "SIFM3", "SPUM ASRC3"},
	{"SPUM POSTTUNE0 IN", "SIFM4", "SPUM ASRC4"},
	{"SPUM POSTTUNE0 IN", "SIFM5", "SPUM ASRC5"},
	{"SPUM POSTTUNE0 IN", "SIFM6", "SPUM ASRC6"},
	{"SPUM POSTTUNE0 IN", "SIFM7", "SPUM ASRC7"},
	{"SPUM DRC0", NULL, "SPUM POSTTUNE0 IN"},
	{"SPUM POSTGAIN0", NULL, "SPUM DRC0"},
	{"SPUM POSTTUNE0 OUT", NULL, "SPUM POSTGAIN0"},
	{"SPUM PGA0", "SIFM0", "SPUM POSTTUNE0 OUT"},
	{"SPUM PGA1", "SIFM1", "SPUM POSTTUNE0 OUT"},
	{"SPUM PGA2", "SIFM2", "SPUM POSTTUNE0 OUT"},
	{"SPUM PGA3", "SIFM3", "SPUM POSTTUNE0 OUT"},
	{"SPUM PGA4", "SIFM4", "SPUM POSTTUNE0 OUT"},
	{"SPUM PGA5", "SIFM5", "SPUM POSTTUNE0 OUT"},
	{"SPUM PGA6", "SIFM6", "SPUM POSTTUNE0 OUT"},
	{"SPUM PGA7", "SIFM7", "SPUM POSTTUNE0 OUT"},

	{"SPUM PRETUNE1 IN", "SIFM0", "NSRC0"},
	{"SPUM PRETUNE1 IN", "SIFM1", "NSRC1"},
	{"SPUM PRETUNE1 IN", "SIFM2", "NSRC2"},
	{"SPUM PRETUNE1 IN", "SIFM3", "NSRC3"},
	{"SPUM PRETUNE1 IN", "SIFM4", "NSRC4"},
	{"SPUM PRETUNE1 IN", "SIFM5", "NSRC5"},
	{"SPUM PRETUNE1 IN", "SIFM6", "NSRC6"},
	{"SPUM PRETUNE1 IN", "SIFM7", "NSRC7"},
	{"SPUM PREGAIN1", NULL, "SPUM PRETUNE1 IN"},
	{"SPUM BQF1", NULL, "SPUM PREGAIN1"},
	{"SPUM PRETUNE1 OUT", NULL, "SPUM BQF1"},
	{"NSRC0 PGA", "SIFM0", "SPUM PRETUNE1 OUT"},
	{"NSRC1 PGA", "SIFM1", "SPUM PRETUNE1 OUT"},
	{"NSRC2 PGA", "SIFM2", "SPUM PRETUNE1 OUT"},
	{"NSRC3 PGA", "SIFM3", "SPUM PRETUNE1 OUT"},
	{"NSRC4 PGA", "SIFM4", "SPUM PRETUNE1 OUT"},
	{"NSRC5 PGA", "SIFM5", "SPUM PRETUNE1 OUT"},
	{"NSRC6 PGA", "SIFM6", "SPUM PRETUNE1 OUT"},
	{"NSRC7 PGA", "SIFM7", "SPUM PRETUNE1 OUT"},

	{"SPUM POSTTUNE1 IN", "SIFM0", "SPUM ASRC0"},
	{"SPUM POSTTUNE1 IN", "SIFM1", "SPUM ASRC1"},
	{"SPUM POSTTUNE1 IN", "SIFM2", "SPUM ASRC2"},
	{"SPUM POSTTUNE1 IN", "SIFM3", "SPUM ASRC3"},
	{"SPUM POSTTUNE1 IN", "SIFM4", "SPUM ASRC4"},
	{"SPUM POSTTUNE1 IN", "SIFM5", "SPUM ASRC5"},
	{"SPUM POSTTUNE1 IN", "SIFM6", "SPUM ASRC6"},
	{"SPUM POSTTUNE1 IN", "SIFM7", "SPUM ASRC7"},
	{"SPUM DRC1", NULL, "SPUM POSTTUNE1 IN"},
	{"SPUM POSTGAIN1", NULL, "SPUM DRC1"},
	{"SPUM POSTTUNE1 OUT", NULL, "SPUM POSTGAIN1"},
	{"SPUM PGA0", "SIFM0", "SPUM POSTTUNE1 OUT"},
	{"SPUM PGA1", "SIFM1", "SPUM POSTTUNE1 OUT"},
	{"SPUM PGA2", "SIFM2", "SPUM POSTTUNE1 OUT"},
	{"SPUM PGA3", "SIFM3", "SPUM POSTTUNE1 OUT"},
	{"SPUM PGA4", "SIFM4", "SPUM POSTTUNE1 OUT"},
	{"SPUM PGA5", "SIFM5", "SPUM POSTTUNE1 OUT"},
	{"SPUM PGA6", "SIFM6", "SPUM POSTTUNE1 OUT"},
	{"SPUM PGA7", "SIFM7", "SPUM POSTTUNE1 OUT"},
};

int abox_atune_probe(struct abox_data *data)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(cmpnt);
	int ret;

	if (!cmpnt)
		return -EAGAIN;

	ret = snd_soc_add_component_controls(cmpnt, atune_controls,
			ARRAY_SIZE(atune_controls));
	if (ret < 0)
		return ret;

	ret = snd_soc_dapm_new_controls(dapm, atune_widgets,
			ARRAY_SIZE(atune_widgets));
	if (ret < 0)
		return ret;

	ret = snd_soc_dapm_add_routes(dapm, atune_routes,
			ARRAY_SIZE(atune_routes));
	if (ret < 0)
		return ret;

	abox_info(cmpnt->dev, "atune probed\n");

	return 0;
}

int abox_atune_remove(struct abox_data *data)
{
	/* widgets and controls would be removed with component */
	return 0;
}

static int pretune_id_from_spus(struct abox_data *data, int sid)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	int aid, ret = -EPIPE;

	for (aid = 0; aid < COUNT_ATUNE; aid++) {
		ret = atune_find_spus(cmpnt, aid);
		if (ret == sid)
			return aid;
	}

	return -EPIPE;
}

static int posttune_id_from_sifs(struct abox_data *data, int sid)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	int aid, ret = -EPIPE;

	for (aid = 0; aid < COUNT_ATUNE; aid++) {
		ret = atune_find_sifs(cmpnt, aid);
		if (ret == sid)
			return aid;
	}

	return -EPIPE;
}

static int posttune_id_from_spus(struct abox_data *data, int sid)
{
	int sifs = sifs_from_spus(data, sid);

	if (sifs < 0)
		return sifs;
	else
		return posttune_id_from_sifs(data, sifs);
}

static int pretune_id_from_nsrc(struct abox_data *data, int sid)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	int aid, ret = -EPIPE;

	for (aid = 0; aid < COUNT_ATUNE; aid++) {
		ret = atune_find_nsrc(cmpnt, aid);
		if (ret == sid)
			return aid;
	}

	return -EPIPE;
}

static int posttune_id_from_sifm(struct abox_data *data, int sid)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	int aid, ret = -EPIPE;

	for (aid = 0; aid < COUNT_ATUNE; aid++) {
		ret = atune_find_sifm(cmpnt, aid);
		if (ret == sid)
			return aid;
	}

	return -EPIPE;
}

static int posttune_id_from_nsrc(struct abox_data *data, int sid)
{
	/* nsrc and sifm is matched by one to one */
	return posttune_id_from_sifm(data, sid);
}

bool abox_atune_spus_posttune_connected(struct abox_data *data, int id)
{
	if (spus_posttune_enabled(data, id)) {
		if (posttune_id_from_spus(data, id) >= 0)
			return true;
	}
	return false;
}

bool abox_atune_spum_pretune_connected(struct abox_data *data, int id)
{
	if (spum_pretune_enabled(data, id)) {
		if (pretune_id_from_nsrc(data, id) >= 0)
			return true;
	}
	return false;
}

int abox_atune_dapm_event(struct abox_data *data, int e, int stream, int sid)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int mask;
	int aid, ret = 0;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* clear atune connection to function chain */
		mask = ABOX_FUNC_CHAIN_SRC_USG_MASK(sid) |
				ABOX_FUNC_CHAIN_SRC_BQF_MASK(sid) |
				ABOX_FUNC_CHAIN_SRC_DRC_MASK(sid) |
				ABOX_FUNC_CHAIN_SRC_DSG_MASK(sid);
		snd_soc_component_update_bits(cmpnt,
				ABOX_SPUS_CTRL_FC_SRC(sid), mask, 0x0);

		aid = posttune_id_from_spus(data, sid);
		if (aid >= 0) {
			ret = spus_bqf_event(data, e, aid, sid);
			if (ret < 0)
				goto err;
			ret = spus_drc_event(data, e, aid, sid);
			if (ret < 0)
				goto err;
			ret = spus_postgain_event(data, e, aid, sid);
			if (ret < 0)
				goto err;
		}

		aid = pretune_id_from_spus(data, sid);
		if (aid >= 0) {
			ret = spus_pregain_event(data, e, aid, sid);
			if (ret < 0)
				goto err;
		}
	} else {
		/* clear atune connection to function chain */
		mask = ABOX_FUNC_CHAIN_NSRC_USG_MASK(sid) |
				ABOX_FUNC_CHAIN_NSRC_BQF_MASK(sid) |
				ABOX_FUNC_CHAIN_NSRC_DRC_MASK(sid) |
				ABOX_FUNC_CHAIN_NSRC_DSG_MASK(sid);
		snd_soc_component_update_bits(cmpnt,
				ABOX_SPUM_CTRL_FC_NSRC(sid), mask, 0x0);

		aid = pretune_id_from_nsrc(data, sid);
		if (aid >= 0) {
			ret = spum_pregain_event(data, e, aid, sid);
			if (ret < 0)
				goto err;
			ret = spum_bqf_event(data, e, aid, sid);
			if (ret < 0)
				goto err;
		}

		aid = posttune_id_from_nsrc(data, sid);
		if (aid >= 0) {
			ret = spum_drc_event(data, e, aid, sid);
			if (ret < 0)
				goto err;
			ret = spum_postgain_event(data, e, aid, sid);
			if (ret < 0)
				goto err;
		}
	}
err:
	return ret;
}
