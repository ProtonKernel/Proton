/*
 *  bigdata_cirrus_sysfs_cb.c
 *  Copyright (c) Samsung Electronics
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 */

#include <sound/soc.h>
#include <sound/cirrus/core.h>
#include <sound/cirrus/big_data.h>
#include <sound/cirrus/calibration.h>
#include <sound/samsung/sec_audio_sysfs.h>
#include "bigdata_cs35l41_sysfs_cb.h"

static struct snd_soc_component *cirrus_amp_component;
struct cirrus_amp *amp;

static const char *cirrus_amp_suffix[AMP_ID_MAX] = {
	[AMP_0] = "_0",
	[AMP_1] = "_1",
	[AMP_2] = "_2",
	[AMP_3] = "_3",
};

static int get_cirrus_amp_temperature_max(enum amp_id id)
{
	struct snd_soc_component *component = cirrus_amp_component;
	unsigned int value = 0;

	if (!component) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	dev_dbg(component->dev, "%s: %d\n", __func__, id);

	if (id >= AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id\n", __func__);
		return -EINVAL;
	}

	amp = cirrus_get_amp_from_suffix(cirrus_amp_suffix[id]);
	if (!amp) {
		dev_err(component->dev, "%s: invalid suffix\n", __func__);
		return -EINVAL;
	}

	value = amp->bd.max_temp >> CS35L41_BD_TEMP_RADIX;
	amp->bd.max_temp = 0;

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int get_cirrus_amp_temperature_keep_max(enum amp_id id)
{
	struct snd_soc_component *component = cirrus_amp_component;
	unsigned int value = 0;

	if (!component) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	dev_dbg(component->dev, "%s: %d\n", __func__, id);

	if (id >= AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id\n", __func__);
		return -EINVAL;
	}

	amp = cirrus_get_amp_from_suffix(cirrus_amp_suffix[id]);
	if (!amp) {
		dev_err(component->dev, "%s: invalid suffix\n", __func__);
		return -EINVAL;
	}

	value = amp->bd.max_temp_keep >> CS35L41_BD_TEMP_RADIX;

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int get_cirrus_amp_temperature_overcount(enum amp_id id)
{
	struct snd_soc_component *component = cirrus_amp_component;
	unsigned int value = 0;

	if (!component) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	dev_dbg(component->dev, "%s: %d\n", __func__, id);

	if (id >= AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id\n", __func__);
		return -EINVAL;
	}

	amp = cirrus_get_amp_from_suffix(cirrus_amp_suffix[id]);
	if (!amp) {
		dev_err(component->dev, "%s: invalid suffix\n", __func__);
		return -EINVAL;
	}

	value = amp->bd.over_temp_count;
	amp->bd.over_temp_count = 0;

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int get_cirrus_amp_excursion_max(enum amp_id id)
{
	struct snd_soc_component *component = cirrus_amp_component;
	unsigned int value = 0;

	if (!component) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	dev_dbg(component->dev, "%s: %d\n", __func__, id);

	if (id >= AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id\n", __func__);
		return -EINVAL;
	}

	amp = cirrus_get_amp_from_suffix(cirrus_amp_suffix[id]);
	if (!amp) {
		dev_err(component->dev, "%s: invalid suffix\n", __func__);
		return -EINVAL;
	}

	value = (amp->bd.max_exc &
				(((1 << CS35L41_BD_EXC_RADIX) - 1))) *
				10000 / (1 << CS35L41_BD_EXC_RADIX);
	amp->bd.max_exc = 0;

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int get_cirrus_amp_excursion_overcount(enum amp_id id)
{
	struct snd_soc_component *component = cirrus_amp_component;
	unsigned int value = 0;

	if (!component) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	dev_dbg(component->dev, "%s: %d\n", __func__, id);

	if (id >= AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id\n", __func__);
		return -EINVAL;
	}

	amp = cirrus_get_amp_from_suffix(cirrus_amp_suffix[id]);
	if (!amp) {
		dev_err(component->dev, "%s: invalid suffix\n", __func__);
		return -EINVAL;
	}

	value = amp->bd.over_exc_count;
	amp->bd.over_exc_count = 0;

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int get_cirrus_amp_curr_temperature(enum amp_id id)
{
	struct snd_soc_component *component = cirrus_amp_component;
	int value = 0;

	if (!component) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	dev_dbg(component->dev, "%s: %d\n", __func__, id);

	if (id >= AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id\n", __func__);
		return -EINVAL;
	}

	value = cirrus_cal_read_temp(cirrus_amp_suffix[id]);
	if (value < 0) {
		dev_err(component->dev, "%s: component is not enabled\n", __func__);
		return -EINVAL;
	}

	dev_info(component->dev, "%s: id %d value %d\n", __func__, id, value);

	return value;
}

static int set_cirrus_amp_surface_temperature(enum amp_id id, int temperature)
{
	struct snd_soc_component *component = cirrus_amp_component;
	int value = 0;

	if (!component) {
		pr_err("%s: component NULL\n", __func__);
		return -EPERM;
	}

	dev_dbg(component->dev, "%s: %d\n", __func__, id);

	if (id >= AMP_ID_MAX) {
		dev_err(component->dev, "%s: invalid id\n", __func__);
		return -EINVAL;
	}

	value = cs35l41_set_surface_temp(cirrus_amp_suffix[id], temperature);
	if (value < 0) {
		dev_err(component->dev, "%s: Amp%d is not enabled\n",
				__func__, id);
		return -EINVAL;
	}

	return value;
}

void register_cirrus_bigdata_cb(struct snd_soc_component *component)
{
	cirrus_amp_component = component;

	dev_info(component->dev, "%s\n", __func__);

	audio_register_temperature_max_cb(get_cirrus_amp_temperature_max);
	audio_register_temperature_keep_max_cb(get_cirrus_amp_temperature_keep_max);
	audio_register_temperature_overcount_cb(get_cirrus_amp_temperature_overcount);
	audio_register_excursion_max_cb(get_cirrus_amp_excursion_max);
	audio_register_excursion_overcount_cb(get_cirrus_amp_excursion_overcount);
	audio_register_curr_temperature_cb(get_cirrus_amp_curr_temperature);
	audio_register_surface_temperature_cb(set_cirrus_amp_surface_temperature);
}
EXPORT_SYMBOL_GPL(register_cirrus_bigdata_cb);
