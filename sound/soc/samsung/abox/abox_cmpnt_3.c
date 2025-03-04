/* sound/soc/samsung/abox/abox_cmpnt.c
 *
 * ALSA SoC Audio Layer - Samsung Abox Component driver
 *
 * Copyright (c) 2018 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/sched/clock.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include "abox.h"
#include "abox_dump.h"
#include "abox_vdma.h"
#include "abox_dma.h"
#include "abox_if.h"
#include "abox_cmpnt.h"
#include "abox_vss.h"
#include "abox_memlog.h"

enum asrc_tick {
	TICK_CP = 0x0,
	TICK_UAIF0 = 0x1,
	TICK_UAIF1 = 0x2,
	TICK_UAIF2 = 0x3,
	TICK_UAIF3 = 0x4,
	TICK_UAIF4 = 0x5,
	TICK_UAIF5 = 0x6,
	TICK_UAIF6 = 0x7,
	TICK_USB = 0x8,
	TICK_BCLK_CP = 0x9,
	TICK_BCLK_SPDY = 0xA,
	TICK_SYNC,
};

enum asrc_ratio {
	RATIO_1,
	RATIO_2,
	RATIO_4,
	RATIO_8,
	RATIO_16,
	RATIO_32,
	RATIO_64,
	RATIO_128,
	RATIO_256,
};

enum asrc_rate {
	RATE_8000,
	RATE_16000,
	RATE_32000,
	RATE_40000,
	RATE_44100,
	RATE_48000,
	RATE_96000,
	RATE_192000,
	RATE_384000,
	ASRC_RATE_COUNT,
};

static const unsigned int asrc_rates[] = {
	8000, 16000, 32000, 40000, 44100, 48000, 96000, 192000, 384000
};

static enum asrc_rate to_asrc_rate(unsigned int rate)
{
	enum asrc_rate arate;

	for (arate = RATE_8000; arate < ASRC_RATE_COUNT; arate++) {
		if (asrc_rates[arate] == rate)
			break;
	}

	return (arate < ASRC_RATE_COUNT) ? arate : RATE_48000;
}

struct asrc_ctrl {
	unsigned int isr;
	unsigned int osr;
	enum asrc_ratio ovsf;
	unsigned int ifactor;
	enum asrc_ratio dcmf;
};

static const struct asrc_ctrl asrc_table[ASRC_RATE_COUNT][ASRC_RATE_COUNT] = {
		/* isr, osr, ovsf, ifactor, dcmf */
	{
		{  8000,   8000, RATIO_8, 65536, RATIO_8},
		{  8000,  16000, RATIO_8, 65536, RATIO_8},
		{  8000,  32000, RATIO_8, 98304, RATIO_8},
		{  8000,  40000, RATIO_8, 76800, RATIO_8},
		{  8000,  44100, RATIO_8, 88200, RATIO_8},
		{  8000,  48000, RATIO_8, 98304, RATIO_8},
		{  8000,  96000, RATIO_8, 98304, RATIO_4},
		{  8000, 192000, RATIO_8, 98304, RATIO_2},
		{  8000, 384000, RATIO_8, 98304, RATIO_1},
	},
	{
		{ 16000,   8000, RATIO_8, 32768, RATIO_8},
		{ 16000,  16000, RATIO_8, 65536, RATIO_8},
		{ 16000,  32000, RATIO_8, 65536, RATIO_8},
		{ 16000,  40000, RATIO_8, 76800, RATIO_8},
		{ 16000,  44100, RATIO_8, 88200, RATIO_8},
		{ 16000,  48000, RATIO_8, 98304, RATIO_8},
		{ 16000,  96000, RATIO_8, 98304, RATIO_8},
		{ 16000, 192000, RATIO_8, 98304, RATIO_4},
		{ 16000, 384000, RATIO_8, 98304, RATIO_2},
	},
	{
		{ 32000,   8000, RATIO_8, 24576, RATIO_8},
		{ 32000,  16000, RATIO_8, 32768, RATIO_8},
		{ 32000,  32000, RATIO_8, 65536, RATIO_8},
		{ 32000,  40000, RATIO_8, 76800, RATIO_8},
		{ 32000,  44100, RATIO_8, 88200, RATIO_8},
		{ 32000,  48000, RATIO_8, 98304, RATIO_8},
		{ 32000,  96000, RATIO_8, 73728, RATIO_2},
		{ 32000, 192000, RATIO_8, 98304, RATIO_2},
		{ 32000, 384000, RATIO_8, 98304, RATIO_1},
	},
	{
		{ 40000,   8000, RATIO_8, 15360, RATIO_8},
		{ 40000,  16000, RATIO_8, 30720, RATIO_8},
		{ 40000,  32000, RATIO_8, 61440, RATIO_8},
		{ 40000,  40000, RATIO_8, 65536, RATIO_8},
		{ 40000,  44100, RATIO_8, 88200, RATIO_8},
		{ 40000,  48000, RATIO_8, 61440, RATIO_8},
		{ 40000,  96000, RATIO_8, 61440, RATIO_4},
		{ 40000, 192000, RATIO_8, 61440, RATIO_2},
		{ 40000, 384000, RATIO_8, 61440, RATIO_1},
	},
	{
		{ 44100,   8000, RATIO_8, 20480, RATIO_8},
		{ 44100,  16000, RATIO_8, 40960, RATIO_8},
		{ 44100,  32000, RATIO_8, 61440, RATIO_8},
		{ 44100,  40000, RATIO_8, 80000, RATIO_8},
		{ 44100,  44100, RATIO_8, 61440, RATIO_8},
		{ 44100,  48000, RATIO_8, 61440, RATIO_8},
		{ 44100,  96000, RATIO_8, 61440, RATIO_2},
		{ 44100, 192000, RATIO_8, 61440, RATIO_2},
		{ 44100, 384000, RATIO_8, 61440, RATIO_1},
	},
	{
		{ 48000,   8000, RATIO_8, 16384, RATIO_8},
		{ 48000,  16000, RATIO_8, 32768, RATIO_8},
		{ 48000,  32000, RATIO_8, 65536, RATIO_8},
		{ 48000,  40000, RATIO_8, 51200, RATIO_8},
		{ 48000,  44100, RATIO_8, 88200, RATIO_8},
		{ 48000,  48000, RATIO_8, 65536, RATIO_8},
		{ 48000,  96000, RATIO_8, 32768, RATIO_2},
		{ 48000, 192000, RATIO_8, 65536, RATIO_2},
		{ 48000, 384000, RATIO_8, 65536, RATIO_1},
	},
	{
		{ 96000,   8000, RATIO_2, 32768, RATIO_8},
		{ 96000,  16000, RATIO_2, 65536, RATIO_8},
		{ 96000,  32000, RATIO_2, 98304, RATIO_8},
		{ 96000,  40000, RATIO_4, 51200, RATIO_8},
		{ 96000,  44100, RATIO_4, 88200, RATIO_8},
		{ 96000,  48000, RATIO_4, 65536, RATIO_8},
		{ 96000,  96000, RATIO_4, 98304, RATIO_8},
		{ 96000, 192000, RATIO_4, 98304, RATIO_4},
		{ 96000, 384000, RATIO_4, 98304, RATIO_2},
	},
	{
		{192000,   8000, RATIO_2, 16384, RATIO_8},
		{192000,  16000, RATIO_2, 32768, RATIO_8},
		{192000,  32000, RATIO_2, 32768, RATIO_8},
		{192000,  40000, RATIO_2, 51200, RATIO_8},
		{192000,  44100, RATIO_4, 44100, RATIO_8},
		{192000,  48000, RATIO_2, 98304, RATIO_8},
		{192000,  96000, RATIO_1, 98304, RATIO_2},
		{192000, 192000, RATIO_1, 98304, RATIO_1},
		{192000, 384000, RATIO_2, 98304, RATIO_1},
	},
	{
		{384000,   8000, RATIO_1, 16384, RATIO_8},
		{384000,  16000, RATIO_1, 32768, RATIO_8},
		{384000,  32000, RATIO_1, 32768, RATIO_8},
		{384000,  40000, RATIO_1, 51200, RATIO_8},
		{384000,  44100, RATIO_1, 56448, RATIO_8},
		{384000,  48000, RATIO_1, 32768, RATIO_4},
		{384000,  96000, RATIO_1, 65536, RATIO_4},
		{384000, 192000, RATIO_1, 98304, RATIO_2},
		{384000, 384000, RATIO_1, 98304, RATIO_1},
	},
};

static unsigned int cal_ofactor(const struct asrc_ctrl *ctrl)
{
	unsigned int isr, osr, ofactor;

	isr = (ctrl->isr / 100) << ctrl->ovsf;
	osr = (ctrl->osr / 100) << ctrl->dcmf;
	ofactor = ctrl->ifactor * isr / osr;

	return ofactor;
}

static unsigned int is_limit(unsigned int is_default)
{
	return is_default / (100 / 5); /* 5% */
}

static unsigned int os_limit(unsigned int os_default)
{
	return os_default / (100 / 5); /* 5% */
}

static int sif_idx(enum ABOX_CONFIGMSG configmsg)
{
	return configmsg - ((configmsg < SET_SIFS0_FORMAT) ?
			SET_SIFS0_RATE : SET_SIFS0_FORMAT);
}

static unsigned int get_sif_rate(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg)
{
	return data->sif_rate[sif_idx(configmsg)];
}

static void set_sif_rate(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg, unsigned int val)
{
	data->sif_rate[sif_idx(configmsg)] = val;
}

static snd_pcm_format_t get_sif_format(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg)
{
	return data->sif_format[sif_idx(configmsg)];
}

static void set_sif_format(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg, snd_pcm_format_t val)
{
	data->sif_format[sif_idx(configmsg)] = val;
}

static int __maybe_unused get_sif_physical_width(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg)
{
	snd_pcm_format_t format = get_sif_format(data, configmsg);

	return snd_pcm_format_physical_width(format);
}

static int get_sif_width(struct abox_data *data, enum ABOX_CONFIGMSG configmsg)
{
	return snd_pcm_format_width(get_sif_format(data, configmsg));
}

static void __maybe_unused set_sif_width(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg, int width)
{
	struct device *dev = data->dev;
	snd_pcm_format_t format = SNDRV_PCM_FORMAT_S16;

	switch (width) {
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
		abox_err(dev, "%s(%d): invalid argument\n", __func__, width);
	}

	set_sif_format(data, configmsg, format);
}

static unsigned int get_sif_channels(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg)
{
	return data->sif_channels[sif_idx(configmsg)];
}

static void set_sif_channels(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg, unsigned int val)
{
	data->sif_channels[sif_idx(configmsg)] = val;
}

static int rate_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = get_sif_rate(data, reg);

	abox_dbg(dev, "%s(%#x): %u\n", __func__, reg, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int rate_put_ipc(struct device *adev, unsigned int val,
		enum ABOX_CONFIGMSG configmsg)
{
	static DEFINE_MUTEX(lock);
	struct abox_data *data = dev_get_drvdata(adev);
	ABOX_IPC_MSG msg;
	struct IPC_ABOX_CONFIG_MSG *abox_config_msg = &msg.msg.config;
	int ret;

	abox_dbg(adev, "%s(%u, %#x)\n", __func__, val, configmsg);

	mutex_lock(&lock);

	if (val > 0)
		set_sif_rate(data, configmsg, val);

	msg.ipcid = IPC_ABOX_CONFIG;
	abox_config_msg->param1 = get_sif_rate(data, configmsg);
	abox_config_msg->msgtype = configmsg;
	ret = abox_request_ipc(adev, msg.ipcid, &msg, sizeof(msg), 0, 0);
	if (ret < 0)
		abox_err(adev, "%s(%u, %#x) failed: %d\n", __func__, val,
				configmsg, ret);

	mutex_unlock(&lock);

	return ret;
}

static int rate_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	abox_info(dev, "%s(%#x, %u)\n", __func__, reg, val);

	return rate_put_ipc(dev, val, reg);
}

static int format_put_ipc(struct device *adev, snd_pcm_format_t format,
		unsigned int channels, enum ABOX_CONFIGMSG configmsg)
{
	static DEFINE_MUTEX(lock);
	struct abox_data *data = dev_get_drvdata(adev);
	ABOX_IPC_MSG msg;
	struct IPC_ABOX_CONFIG_MSG *abox_config_msg = &msg.msg.config;
	int width = snd_pcm_format_width(format);
	int ret;

	abox_dbg(adev, "%s(%d, %u, %#x)\n", __func__, width, channels,
			configmsg);

	mutex_lock(&lock);

	if (format > 0)
		set_sif_format(data, configmsg, format);
	if (channels > 0)
		set_sif_channels(data, configmsg, channels);

	width = get_sif_width(data, configmsg);
	channels = get_sif_channels(data, configmsg);

	/* update manually for regmap cache sync */
	switch (configmsg) {
	case SET_SIFS0_RATE:
	case SET_SIFS0_FORMAT:
		regmap_update_bits(data->regmap, ABOX_SPUS_CTRL1,
				ABOX_MIXP_FORMAT_MASK,
				abox_get_format(width, channels) <<
				ABOX_MIXP_FORMAT_L);
		break;
	default:
		/* Nothing to do */
		break;
	}

	msg.ipcid = IPC_ABOX_CONFIG;
	abox_config_msg->param1 = width;
	abox_config_msg->param2 = channels;
	abox_config_msg->msgtype = configmsg;
	ret = abox_request_ipc(adev, msg.ipcid, &msg, sizeof(msg), 0, 0);
	if (ret < 0)
		abox_err(adev, "%d(%d bit, %u channels) failed: %d\n", configmsg,
				width, channels, ret);

	mutex_unlock(&lock);

	return ret;
}

static int width_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = get_sif_width(data, reg);

	abox_dbg(dev, "%s(%#x): %u\n", __func__, reg, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int width_put_ipc(struct device *dev, unsigned int val,
		enum ABOX_CONFIGMSG configmsg)
{
	snd_pcm_format_t format = SNDRV_PCM_FORMAT_S16;

	abox_dbg(dev, "%s(%u, %#x)\n", __func__, val, configmsg);

	switch (val) {
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
		abox_warn(dev, "%s(%u, %#x) invalid argument\n", __func__,
				val, configmsg);
		break;
	}

	return format_put_ipc(dev, format, 0, configmsg);
}

static int width_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	abox_info(dev, "%s(%#x, %u)\n", __func__, reg, val);

	return width_put_ipc(dev, val, reg);
}

static int channels_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = get_sif_channels(data, reg);

	abox_dbg(dev, "%s(%#x): %u\n", __func__, reg, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int channels_put_ipc(struct device *dev, unsigned int val,
		enum ABOX_CONFIGMSG configmsg)
{
	unsigned int channels = val;

	abox_dbg(dev, "%s(%u, %#x)\n", __func__, val, configmsg);

	return format_put_ipc(dev, 0, channels, configmsg);
}

static int channels_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	abox_info(dev, "%s(%#x, %u)\n", __func__, reg, val);

	return channels_put_ipc(dev, val, reg);
}

static int audio_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int item;

	abox_dbg(dev, "%s: %u\n", __func__, data->audio_mode);

	item = snd_soc_enum_val_to_item(e, data->audio_mode);
	ucontrol->value.enumerated.item[0] = item;

	return 0;
}

static int audio_mode_put_ipc(struct device *dev, enum audio_mode mode)
{
	struct abox_data *data = dev_get_drvdata(dev);
	ABOX_IPC_MSG msg;
	struct IPC_SYSTEM_MSG *system_msg = &msg.msg.system;

	abox_dbg(dev, "%s(%d)\n", __func__, mode);

	data->audio_mode_time = local_clock();

	msg.ipcid = IPC_SYSTEM;
	system_msg->msgtype = ABOX_SET_MODE;
	system_msg->param1 = data->audio_mode = mode;
	return abox_request_ipc(dev, msg.ipcid, &msg, sizeof(msg), 0, 0);
}

static int audio_mode_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;
	struct abox_data *data = dev_get_drvdata(dev);
	enum audio_mode mode;

	if (item[0] >= e->items)
		return -EINVAL;

	mode = snd_soc_enum_item_to_val(e, item[0]);
	abox_info(dev, "%s(%u)\n", __func__, mode);

	if (IS_ENABLED(CONFIG_SOC_EXYNOS9830)) {
		if (mode == MODE_IN_CALL)
			abox_vss_notify_call(dev, data, 1);
		else if (mode == MODE_NORMAL)
			abox_vss_notify_call(dev, data, 0);
	}

	return audio_mode_put_ipc(dev, mode);
}

static const char * const audio_mode_enum_texts[] = {
	"NORMAL",
	"RINGTONE",
	"IN_CALL",
	"IN_COMMUNICATION",
	"IN_VIDEOCALL",
	"RESERVED0",
	"RESERVED1",
	"IN_LOOPBACK",
};
static const unsigned int audio_mode_enum_values[] = {
	MODE_NORMAL,
	MODE_RINGTONE,
	MODE_IN_CALL,
	MODE_IN_COMMUNICATION,
	MODE_IN_VIDEOCALL,
	MODE_RESERVED0,
	MODE_RESERVED1,
	MODE_IN_LOOPBACK,
};
SOC_VALUE_ENUM_SINGLE_DECL(audio_mode_enum, SND_SOC_NOPM, 0, 0,
		audio_mode_enum_texts, audio_mode_enum_values);

static int sound_type_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int item;

	abox_dbg(dev, "%s: %u\n", __func__, data->sound_type);

	item = snd_soc_enum_val_to_item(e, data->sound_type);
	ucontrol->value.enumerated.item[0] = item;

	return 0;
}

static int sound_type_put_ipc(struct device *dev, enum sound_type type)
{
	struct abox_data *data = dev_get_drvdata(dev);
	ABOX_IPC_MSG msg;
	struct IPC_SYSTEM_MSG *system_msg = &msg.msg.system;

	abox_dbg(dev, "%s(%d)\n", __func__, type);

	msg.ipcid = IPC_SYSTEM;
	system_msg->msgtype = ABOX_SET_TYPE;
	system_msg->param1 = data->sound_type = type;

	return abox_request_ipc(dev, msg.ipcid, &msg, sizeof(msg), 0, 0);
}

static int sound_type_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;
	enum sound_type type;

	if (item[0] >= e->items)
		return -EINVAL;

	type = snd_soc_enum_item_to_val(e, item[0]);
	abox_info(dev, "%s(%d)\n", __func__, type);

	return sound_type_put_ipc(dev, type);
}
static const char * const sound_type_enum_texts[] = {
	"VOICE",
	"SPEAKER",
	"HEADSET",
	"BTVOICE",
	"USB",
	"CALLFWD",
};
static const unsigned int sound_type_enum_values[] = {
	SOUND_TYPE_VOICE,
	SOUND_TYPE_SPEAKER,
	SOUND_TYPE_HEADSET,
	SOUND_TYPE_BTVOICE,
	SOUND_TYPE_USB,
	SOUND_TYPE_CALLFWD,
};
SOC_VALUE_ENUM_SINGLE_DECL(sound_type_enum, SND_SOC_NOPM, 0, 0,
		sound_type_enum_texts, sound_type_enum_values);

static int tickle_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	abox_dbg(dev, "%s\n", __func__);

	ucontrol->value.integer.value[0] = data->enabled;

	return 0;
}

static int tickle_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	long val = ucontrol->value.integer.value[0];

	abox_dbg(dev, "%s(%ld)\n", __func__, val);

	if (!!val)
		pm_request_resume(dev);

	return 0;
}

static unsigned int s_default = 36864;

static int asrc_factor_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = s_default;

	abox_dbg(dev, "%s(%#x): %u\n", __func__, reg, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int asrc_factor_put_ipc(struct device *adev, unsigned int val,
		enum ABOX_CONFIGMSG configmsg)
{
	ABOX_IPC_MSG msg;
	struct IPC_ABOX_CONFIG_MSG *abox_config_msg = &msg.msg.config;
	int ret;

	abox_dbg(adev, "%s(%u, %#x)\n", __func__, val, configmsg);

	s_default = val;

	msg.ipcid = IPC_ABOX_CONFIG;
	abox_config_msg->param1 = s_default;
	abox_config_msg->msgtype = configmsg;
	ret = abox_request_ipc(adev, msg.ipcid, &msg, sizeof(msg), 0, 0);
	if (ret < 0)
		abox_err(adev, "%s(%u, %#x) failed: %d\n", __func__, val,
				configmsg, ret);

	return ret;
}

static int asrc_factor_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	abox_info(dev, "%s(%#x, %u)\n", __func__, reg, val);

	return asrc_factor_put_ipc(dev, val, reg);
}

static bool spus_asrc_force_enable[] = {
	false, false, false, false,
	false, false, false, false,
	false, false, false, false
};

static bool spum_asrc_force_enable[] = {
	false, false, false, false,
	false, false, false, false,
};

static int spus_asrc_enable_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	long val = ucontrol->value.integer.value[0];
	int idx, ret;

	ret = sscanf(kcontrol->id.name, "%*s %*s ASRC%d", &idx);
	if (ret < 1) {
		abox_err(dev, "%s(%s): %d\n", __func__, kcontrol->id.name, ret);
		return ret;
	}
	if (idx < 0 || idx >= ARRAY_SIZE(spus_asrc_force_enable)) {
		abox_err(dev, "%s(%s): %d\n", __func__, kcontrol->id.name, idx);
		return -EINVAL;
	}

	abox_info(dev, "%s(%ld, %d)\n", __func__, val, idx);

	spus_asrc_force_enable[idx] = val;

	return snd_soc_put_volsw(kcontrol, ucontrol);
}

static int spum_asrc_enable_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	long val = ucontrol->value.integer.value[0];
	int idx, ret;

	ret = sscanf(kcontrol->id.name, "%*s %*s ASRC%d", &idx);
	if (ret < 1) {
		abox_err(dev, "%s(%s): %d\n", __func__, kcontrol->id.name, ret);
		return ret;
	}
	if (idx < 0 || idx >= ARRAY_SIZE(spum_asrc_force_enable)) {
		abox_err(dev, "%s(%s): %d\n", __func__, kcontrol->id.name, idx);
		return -EINVAL;
	}

	abox_info(dev, "%s(%ld, %d)\n", __func__, val, idx);

	spum_asrc_force_enable[idx] = val;

	return snd_soc_put_volsw(kcontrol, ucontrol);
}

static int get_apf_coef(struct abox_data *data, int stream, int idx)
{
	return data->apf_coef[stream ? 1 : 0][idx];
}

static void set_apf_coef(struct abox_data *data, int stream, int idx, int coef)
{
	data->apf_coef[stream ? 1 : 0][idx] = coef;
}

static int asrc_apf_coef_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int stream = mc->reg;
	int idx = mc->shift;

	abox_dbg(dev, "%s(%d, %d)\n", __func__, stream, idx);

	ucontrol->value.integer.value[0] = get_apf_coef(data, stream, idx);

	return 0;
}

static int asrc_apf_coef_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int stream = mc->reg;
	int idx = mc->shift;
	long val = ucontrol->value.integer.value[0];

	abox_dbg(dev, "%s(%d, %d, %ld)\n", __func__, stream, idx, val);

	set_apf_coef(data, stream, idx, val);

	return 0;
}

static int wake_lock_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	unsigned int val = data->ws.active;

	abox_dbg(dev, "%s: %u\n", __func__, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int wake_lock_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	abox_info(dev, "%s(%u)\n", __func__, val);

	if (val)
		__pm_stay_awake(&data->ws);
	else
		__pm_relax(&data->ws);

	return 0;
}

static int reset_log_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	unsigned long *addr;

	abox_dbg(dev, "%s: %u\n", __func__);

	for (addr = data->slog_base + ABOX_SLOG_OFFSET; (void *)addr <
			data->slog_base + data->slog_size; addr++) {
		if (*addr) {
			/* There is non-zero */
			ucontrol->value.integer.value[0] = 0;
			return 0;
		}
	}

	/* Area is all zero */
	ucontrol->value.integer.value[0] = 1;
	return 0;
}

static int reset_log_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	abox_dbg(dev, "%s(%u)\n", __func__, val);

	if (val) {
		abox_info(dev, "reset silent log area\n");
		memset(data->slog_base + ABOX_SLOG_OFFSET, 0,
				data->slog_size - ABOX_SLOG_OFFSET);
	}

	return 0;
}

static enum asrc_tick spus_asrc_os[] = {
	TICK_SYNC, TICK_SYNC, TICK_SYNC, TICK_SYNC,
	TICK_SYNC, TICK_SYNC, TICK_SYNC, TICK_SYNC,
	TICK_SYNC, TICK_SYNC, TICK_SYNC, TICK_SYNC,
};
static enum asrc_tick spus_asrc_is[] = {
	TICK_SYNC, TICK_SYNC, TICK_SYNC, TICK_SYNC,
	TICK_SYNC, TICK_SYNC, TICK_SYNC, TICK_SYNC,
	TICK_SYNC, TICK_SYNC, TICK_SYNC, TICK_SYNC
};

static enum asrc_tick spum_asrc_os[] = {
	TICK_SYNC, TICK_SYNC, TICK_SYNC, TICK_SYNC,
	TICK_SYNC, TICK_SYNC, TICK_SYNC, TICK_SYNC,
};
static enum asrc_tick spum_asrc_is[] = {
	TICK_SYNC, TICK_SYNC, TICK_SYNC, TICK_SYNC,
	TICK_SYNC, TICK_SYNC, TICK_SYNC, TICK_SYNC,
};

static int spus_asrc_os_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int idx = e->reg;
	unsigned int *item = ucontrol->value.enumerated.item;
	enum asrc_tick tick = spus_asrc_os[idx];

	abox_dbg(dev, "%s(%d): %d\n", __func__, idx, tick);

	item[0] = snd_soc_enum_val_to_item(e, tick);

	return 0;
}

static int spus_asrc_os_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int idx = e->reg;
	unsigned int *item = ucontrol->value.enumerated.item;
	enum asrc_tick tick;

	if (item[0] >= e->items)
		return -EINVAL;

	tick = snd_soc_enum_item_to_val(e, item[0]);
	abox_dbg(dev, "%s(%d, %d)\n", __func__, idx, tick);
	spus_asrc_os[idx] = tick;

	return 0;
}

static int spus_asrc_is_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int idx = e->reg;
	unsigned int *item = ucontrol->value.enumerated.item;
	enum asrc_tick tick = spus_asrc_is[idx];

	abox_dbg(dev, "%s(%d): %d\n", __func__, idx, tick);

	item[0] = snd_soc_enum_val_to_item(e, tick);

	return 0;
}

static int spus_asrc_is_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int idx = e->reg;
	unsigned int *item = ucontrol->value.enumerated.item;
	enum asrc_tick tick;

	if (item[0] >= e->items)
		return -EINVAL;

	tick = snd_soc_enum_item_to_val(e, item[0]);
	abox_dbg(dev, "%s(%d, %d)\n", __func__, idx, tick);
	spus_asrc_is[idx] = tick;

	return 0;
}

static int spum_asrc_os_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int idx = e->reg;
	unsigned int *item = ucontrol->value.enumerated.item;
	enum asrc_tick tick = spum_asrc_os[idx];

	abox_dbg(dev, "%s(%d): %d\n", __func__, idx, tick);

	item[0] = snd_soc_enum_val_to_item(e, tick);

	return 0;
}

static int spum_asrc_os_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int idx = e->reg;
	unsigned int *item = ucontrol->value.enumerated.item;
	enum asrc_tick tick;

	if (item[0] >= e->items)
		return -EINVAL;

	tick = snd_soc_enum_item_to_val(e, item[0]);
	abox_dbg(dev, "%s(%d, %d)\n", __func__, idx, tick);
	spum_asrc_os[idx] = tick;

	return 0;
}

static int spum_asrc_is_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int idx = e->reg;
	unsigned int *item = ucontrol->value.enumerated.item;
	enum asrc_tick tick = spum_asrc_is[idx];

	abox_dbg(dev, "%s(%d): %d\n", __func__, idx, tick);

	item[0] = snd_soc_enum_val_to_item(e, tick);

	return 0;
}

static int spum_asrc_is_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	int idx = e->reg;
	unsigned int *item = ucontrol->value.enumerated.item;
	enum asrc_tick tick;

	if (item[0] >= e->items)
		return -EINVAL;

	tick = snd_soc_enum_item_to_val(e, item[0]);
	abox_dbg(dev, "%s(%d, %d)\n", __func__, idx, tick);
	spum_asrc_is[idx] = tick;

	return 0;
}

static const struct snd_kcontrol_new cmpnt_controls[] = {
	SOC_SINGLE_EXT("SIFS0 Rate", SET_SIFS0_RATE, 8000, 384000, 0,
			rate_get, rate_put),
	SOC_SINGLE_EXT("SIFS1 Rate", SET_SIFS1_RATE, 8000, 384000, 0,
			rate_get, rate_put),
	SOC_SINGLE_EXT("SIFS2 Rate", SET_SIFS2_RATE, 8000, 384000, 0,
			rate_get, rate_put),
	SOC_SINGLE_EXT("SIFS3 Rate", SET_SIFS3_RATE, 8000, 384000, 0,
			rate_get, rate_put),
	SOC_SINGLE_EXT("SIFS4 Rate", SET_SIFS4_RATE, 8000, 384000, 0,
			rate_get, rate_put),
	SOC_SINGLE_EXT("SIFS5 Rate", SET_SIFS5_RATE, 8000, 384000, 0,
			rate_get, rate_put),
	SOC_SINGLE_EXT("SIFM0 Rate", SET_SIFM0_RATE, 8000, 384000, 0,
			rate_get, rate_put),
	SOC_SINGLE_EXT("SIFM1 Rate", SET_SIFM1_RATE, 8000, 384000, 0,
			rate_get, rate_put),
	SOC_SINGLE_EXT("SIFM2 Rate", SET_SIFM2_RATE, 8000, 384000, 0,
			rate_get, rate_put),
	SOC_SINGLE_EXT("SIFM3 Rate", SET_SIFM3_RATE, 8000, 384000, 0,
			rate_get, rate_put),
	SOC_SINGLE_EXT("SIFM4 Rate", SET_SIFM4_RATE, 8000, 384000, 0,
			rate_get, rate_put),
	SOC_SINGLE_EXT("SIFS0 Width", SET_SIFS0_FORMAT, 16, 32, 0,
			width_get, width_put),
	SOC_SINGLE_EXT("SIFS1 Width", SET_SIFS1_FORMAT, 16, 32, 0,
			width_get, width_put),
	SOC_SINGLE_EXT("SIFS2 Width", SET_SIFS2_FORMAT, 16, 32, 0,
			width_get, width_put),
	SOC_SINGLE_EXT("SIFS3 Width", SET_SIFS3_FORMAT, 16, 32, 0,
			width_get, width_put),
	SOC_SINGLE_EXT("SIFS4 Width", SET_SIFS4_FORMAT, 16, 32, 0,
			width_get, width_put),
	SOC_SINGLE_EXT("SIFS5 Width", SET_SIFS5_FORMAT, 16, 32, 0,
			width_get, width_put),
	SOC_SINGLE_EXT("SIFM0 Width", SET_SIFM0_FORMAT, 16, 32, 0,
			width_get, width_put),
	SOC_SINGLE_EXT("SIFM1 Width", SET_SIFM1_FORMAT, 16, 32, 0,
			width_get, width_put),
	SOC_SINGLE_EXT("SIFM2 Width", SET_SIFM2_FORMAT, 16, 32, 0,
			width_get, width_put),
	SOC_SINGLE_EXT("SIFM3 Width", SET_SIFM3_FORMAT, 16, 32, 0,
			width_get, width_put),
	SOC_SINGLE_EXT("SIFM4 Width", SET_SIFM4_FORMAT, 16, 32, 0,
			width_get, width_put),
	SOC_SINGLE_EXT("SIFS0 Channel", SET_SIFS0_FORMAT, 1, 8, 0,
			channels_get, channels_put),
	SOC_VALUE_ENUM_EXT("Audio Mode", audio_mode_enum,
			audio_mode_get, audio_mode_put),
	SOC_VALUE_ENUM_EXT("Sound Type", sound_type_enum,
			sound_type_get, sound_type_put),
	SOC_SINGLE_EXT("Tickle", 0, 0, 1, 0, tickle_get, tickle_put),
	SOC_SINGLE_EXT("Wakelock", 0, 0, 1, 0, wake_lock_get, wake_lock_put),
	SOC_SINGLE_EXT("Reset Log", 0, 0, 1, 0, reset_log_get, reset_log_put),
	SOC_SINGLE("NSRC0 Bridge", ABOX_ROUTE_CTRL2,
			ABOX_NSRC_CONNECTION_TYPE_L(0), 1, 0),
	SOC_SINGLE("NSRC1 Bridge", ABOX_ROUTE_CTRL2,
			ABOX_NSRC_CONNECTION_TYPE_L(1), 1, 0),
	SOC_SINGLE("NSRC2 Bridge", ABOX_ROUTE_CTRL2,
			ABOX_NSRC_CONNECTION_TYPE_L(2), 1, 0),
	SOC_SINGLE("NSRC3 Bridge", ABOX_ROUTE_CTRL2,
			ABOX_NSRC_CONNECTION_TYPE_L(3), 1, 0),
	SOC_SINGLE("NSRC4 Bridge", ABOX_ROUTE_CTRL2,
			ABOX_NSRC_CONNECTION_TYPE_L(4), 1, 0),
	SOC_SINGLE_EXT("ASRC Factor CP", SET_ASRC_FACTOR_CP, 0, 0x1ffff, 0,
			asrc_factor_get, asrc_factor_put),
	SOC_SINGLE("MIXP Dummy Start", ABOX_SPUS_LATENCY_CTRL0,
			ABOX_MIXP_DUMMY_START_L, 1, 0),
	SOC_SINGLE("SPUS ASRC0 Dummy Start", ABOX_SPUS_LATENCY_CTRL0,
			ABOX_RDMA_ASRC_DUMMY_START_L(0), 1, 0),
	SOC_SINGLE("SPUS ASRC1 Dummy Start", ABOX_SPUS_LATENCY_CTRL0,
			ABOX_RDMA_ASRC_DUMMY_START_L(1), 1, 0),
	SOC_SINGLE("SPUS ASRC2 Dummy Start", ABOX_SPUS_LATENCY_CTRL0,
			ABOX_RDMA_ASRC_DUMMY_START_L(2), 1, 0),
	SOC_SINGLE("SPUS ASRC3 Dummy Start", ABOX_SPUS_LATENCY_CTRL0,
			ABOX_RDMA_ASRC_DUMMY_START_L(3), 1, 0),
	SOC_SINGLE("SPUS ASRC4 Dummy Start", ABOX_SPUS_LATENCY_CTRL0,
			ABOX_RDMA_ASRC_DUMMY_START_L(4), 1, 0),
	SOC_SINGLE("SPUS ASRC5 Dummy Start", ABOX_SPUS_LATENCY_CTRL0,
			ABOX_RDMA_ASRC_DUMMY_START_L(5), 1, 0),
	SOC_SINGLE("SPUS ASRC6 Dummy Start", ABOX_SPUS_LATENCY_CTRL0,
			ABOX_RDMA_ASRC_DUMMY_START_L(6), 1, 0),
	SOC_SINGLE("SPUS ASRC7 Dummy Start", ABOX_SPUS_LATENCY_CTRL0,
			ABOX_RDMA_ASRC_DUMMY_START_L(7), 1, 0),
	SOC_SINGLE("SPUS ASRC8 Dummy Start", ABOX_SPUS_LATENCY_CTRL0,
			ABOX_RDMA_ASRC_DUMMY_START_L(8), 1, 0),
	SOC_SINGLE("SPUS ASRC9 Dummy Start", ABOX_SPUS_LATENCY_CTRL0,
			ABOX_RDMA_ASRC_DUMMY_START_L(9), 1, 0),
	SOC_SINGLE("SPUS ASRC10 Dummy Start", ABOX_SPUS_LATENCY_CTRL0,
			ABOX_RDMA_ASRC_DUMMY_START_L(10), 1, 0),
	SOC_SINGLE("SPUS ASRC11 Dummy Start", ABOX_SPUS_LATENCY_CTRL0,
			ABOX_RDMA_ASRC_DUMMY_START_L(11), 1, 0),
	SOC_SINGLE("SPUS ASRC0 Start Num", ABOX_SPUS_LATENCY_CTRL1,
			ABOX_RDMA_START_ASRC_NUM_L(0), 32, 0),
	SOC_SINGLE("SPUS ASRC1 Start Num", ABOX_SPUS_LATENCY_CTRL1,
			ABOX_RDMA_START_ASRC_NUM_L(1), 32, 0),
	SOC_SINGLE("SPUS ASRC2 Start Num", ABOX_SPUS_LATENCY_CTRL1,
			ABOX_RDMA_START_ASRC_NUM_L(2), 32, 0),
	SOC_SINGLE("SPUS ASRC3 Start Num", ABOX_SPUS_LATENCY_CTRL1,
			ABOX_RDMA_START_ASRC_NUM_L(3), 32, 0),
	SOC_SINGLE("SPUS ASRC4 Start Num", ABOX_SPUS_LATENCY_CTRL2,
			ABOX_RDMA_START_ASRC_NUM_L(4), 32, 0),
	SOC_SINGLE("SPUS ASRC5 Start Num", ABOX_SPUS_LATENCY_CTRL2,
			ABOX_RDMA_START_ASRC_NUM_L(5), 32, 0),
	SOC_SINGLE("SPUS ASRC6 Start Num", ABOX_SPUS_LATENCY_CTRL2,
			ABOX_RDMA_START_ASRC_NUM_L(6), 32, 0),
	SOC_SINGLE("SPUS ASRC7 Start Num", ABOX_SPUS_LATENCY_CTRL2,
			ABOX_RDMA_START_ASRC_NUM_L(7), 32, 0),
	SOC_SINGLE("SPUS ASRC8 Start Num", ABOX_SPUS_LATENCY_CTRL3,
			ABOX_RDMA_START_ASRC_NUM_L(8), 32, 0),
	SOC_SINGLE("SPUS ASRC9 Start Num", ABOX_SPUS_LATENCY_CTRL3,
			ABOX_RDMA_START_ASRC_NUM_L(9), 32, 0),
	SOC_SINGLE("SPUS ASRC10 Start Num", ABOX_SPUS_LATENCY_CTRL3,
			ABOX_RDMA_START_ASRC_NUM_L(10), 32, 0),
	SOC_SINGLE("SPUS ASRC11 Start Num", ABOX_SPUS_LATENCY_CTRL3,
			ABOX_RDMA_START_ASRC_NUM_L(11), 32, 0),
};

static const struct snd_kcontrol_new spus_asrc_controls[] = {
	SOC_SINGLE_EXT("SPUS ASRC0", ABOX_SPUS_CTRL_FC0,
			ABOX_FUNC_CHAIN_SRC_ASRC_L(0), 1, 0,
			snd_soc_get_volsw, spus_asrc_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC1", ABOX_SPUS_CTRL_FC0,
			ABOX_FUNC_CHAIN_SRC_ASRC_L(1), 1, 0,
			snd_soc_get_volsw, spus_asrc_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC2", ABOX_SPUS_CTRL_FC0,
			ABOX_FUNC_CHAIN_SRC_ASRC_L(2), 1, 0,
			snd_soc_get_volsw, spus_asrc_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC3", ABOX_SPUS_CTRL_FC0,
			ABOX_FUNC_CHAIN_SRC_ASRC_L(3), 1, 0,
			snd_soc_get_volsw, spus_asrc_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC4", ABOX_SPUS_CTRL_FC1,
			ABOX_FUNC_CHAIN_SRC_ASRC_L(4), 1, 0,
			snd_soc_get_volsw, spus_asrc_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC5", ABOX_SPUS_CTRL_FC1,
			ABOX_FUNC_CHAIN_SRC_ASRC_L(5), 1, 0,
			snd_soc_get_volsw, spus_asrc_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC6", ABOX_SPUS_CTRL_FC1,
			ABOX_FUNC_CHAIN_SRC_ASRC_L(6), 1, 0,
			snd_soc_get_volsw, spus_asrc_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC7", ABOX_SPUS_CTRL_FC1,
			ABOX_FUNC_CHAIN_SRC_ASRC_L(7), 1, 0,
			snd_soc_get_volsw, spus_asrc_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC8", ABOX_SPUS_CTRL_FC2,
			ABOX_FUNC_CHAIN_SRC_ASRC_L(8), 1, 0,
			snd_soc_get_volsw, spus_asrc_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC9", ABOX_SPUS_CTRL_FC2,
			ABOX_FUNC_CHAIN_SRC_ASRC_L(9), 1, 0,
			snd_soc_get_volsw, spus_asrc_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC10", ABOX_SPUS_CTRL_FC2,
			ABOX_FUNC_CHAIN_SRC_ASRC_L(10), 1, 0,
			snd_soc_get_volsw, spus_asrc_enable_put),
	SOC_SINGLE_EXT("SPUS ASRC11", ABOX_SPUS_CTRL_FC2,
			ABOX_FUNC_CHAIN_SRC_ASRC_L(11), 1, 0,
			snd_soc_get_volsw, spus_asrc_enable_put),
};

static const struct snd_kcontrol_new spum_asrc_controls[] = {
	SOC_SINGLE_EXT("SPUM ASRC0", ABOX_SPUM_CTRL0,
			ABOX_FUNC_CHAIN_NSRC_ASRC_L(0), 1, 0,
			snd_soc_get_volsw, spum_asrc_enable_put),
	SOC_SINGLE_EXT("SPUM ASRC1", ABOX_SPUM_CTRL0,
			ABOX_FUNC_CHAIN_NSRC_ASRC_L(1), 1, 0,
			snd_soc_get_volsw, spum_asrc_enable_put),
	SOC_SINGLE_EXT("SPUM ASRC2", ABOX_SPUM_CTRL0,
			ABOX_FUNC_CHAIN_NSRC_ASRC_L(2), 1, 0,
			snd_soc_get_volsw, spum_asrc_enable_put),
	SOC_SINGLE_EXT("SPUM ASRC3", ABOX_SPUM_CTRL0,
			ABOX_FUNC_CHAIN_NSRC_ASRC_L(3), 1, 0,
			snd_soc_get_volsw, spum_asrc_enable_put),
	SOC_SINGLE_EXT("SPUM ASRC4", ABOX_SPUM_CTRL0,
			ABOX_FUNC_CHAIN_NSRC_ASRC_L(4), 1, 0,
			snd_soc_get_volsw, spum_asrc_enable_put),
};

static const struct snd_kcontrol_new spus_asrc_id_controls[] = {
	SOC_SINGLE("SPUS ASRC0 ID", ABOX_SPUS_CTRL4,
			ABOX_SRC_ASRC_ID_L(0), 11, 0),
	SOC_SINGLE("SPUS ASRC1 ID", ABOX_SPUS_CTRL4,
			ABOX_SRC_ASRC_ID_L(1), 11, 0),
	SOC_SINGLE("SPUS ASRC2 ID", ABOX_SPUS_CTRL4,
			ABOX_SRC_ASRC_ID_L(2), 11, 0),
	SOC_SINGLE("SPUS ASRC3 ID", ABOX_SPUS_CTRL4,
			ABOX_SRC_ASRC_ID_L(3), 11, 0),
	SOC_SINGLE("SPUS ASRC4 ID", ABOX_SPUS_CTRL5,
			ABOX_SRC_ASRC_ID_L(4), 11, 0),
	SOC_SINGLE("SPUS ASRC5 ID", ABOX_SPUS_CTRL5,
			ABOX_SRC_ASRC_ID_L(5), 11, 0),
	SOC_SINGLE("SPUS ASRC6 ID", ABOX_SPUS_CTRL5,
			ABOX_SRC_ASRC_ID_L(6), 11, 0),
	SOC_SINGLE("SPUS ASRC7 ID", ABOX_SPUS_CTRL5,
			ABOX_SRC_ASRC_ID_L(7), 11, 0),
	SOC_SINGLE("SPUS ASRC8 ID", ABOX_SPUS_CTRL5,
			ABOX_SRC_ASRC_ID_L(8), 11, 0),
	SOC_SINGLE("SPUS ASRC9 ID", ABOX_SPUS_CTRL5,
			ABOX_SRC_ASRC_ID_L(9), 11, 0),
	SOC_SINGLE("SPUS ASRC10 ID", ABOX_SPUS_CTRL5,
			ABOX_SRC_ASRC_ID_L(10), 11, 0),
	SOC_SINGLE("SPUS ASRC11 ID", ABOX_SPUS_CTRL5,
			ABOX_SRC_ASRC_ID_L(11), 11, 0),
};

static const struct snd_kcontrol_new spum_asrc_id_controls[] = {
	SOC_SINGLE("SPUM ASRC0 ID", ABOX_SPUM_CTRL4,
			ABOX_NSRC_ASRC_ID_L(0), 7, 0),
	SOC_SINGLE("SPUM ASRC1 ID", ABOX_SPUM_CTRL4,
			ABOX_NSRC_ASRC_ID_L(1), 7, 0),
	SOC_SINGLE("SPUM ASRC2 ID", ABOX_SPUM_CTRL4,
			ABOX_NSRC_ASRC_ID_L(2), 7, 0),
	SOC_SINGLE("SPUM ASRC3 ID", ABOX_SPUM_CTRL4,
			ABOX_NSRC_ASRC_ID_L(3), 7, 0),
	SOC_SINGLE("SPUM ASRC4 ID", ABOX_SPUM_CTRL4,
			ABOX_NSRC_ASRC_ID_L(4), 7, 0),
};

static const struct snd_kcontrol_new spus_asrc_apf_coef_controls[] = {
	SOC_SINGLE_EXT("SPUS ASRC0 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			0, 1, 0, asrc_apf_coef_get, asrc_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC1 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			1, 1, 0, asrc_apf_coef_get, asrc_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC2 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			2, 1, 0, asrc_apf_coef_get, asrc_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC3 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			3, 1, 0, asrc_apf_coef_get, asrc_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC4 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			4, 1, 0, asrc_apf_coef_get, asrc_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC5 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			5, 1, 0, asrc_apf_coef_get, asrc_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC6 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			6, 1, 0, asrc_apf_coef_get, asrc_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC7 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			7, 1, 0, asrc_apf_coef_get, asrc_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC8 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			8, 1, 0, asrc_apf_coef_get, asrc_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC9 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			9, 1, 0, asrc_apf_coef_get, asrc_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC10 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			10, 1, 0, asrc_apf_coef_get, asrc_apf_coef_put),
	SOC_SINGLE_EXT("SPUS ASRC11 APF COEF", SNDRV_PCM_STREAM_PLAYBACK,
			11, 1, 0, asrc_apf_coef_get, asrc_apf_coef_put),
};

static const struct snd_kcontrol_new spum_asrc_apf_coef_controls[] = {
	SOC_SINGLE_EXT("SPUM ASRC0 APF COEF", SNDRV_PCM_STREAM_CAPTURE, 0, 1, 0,
			asrc_apf_coef_get, asrc_apf_coef_put),
	SOC_SINGLE_EXT("SPUM ASRC1 APF COEF", SNDRV_PCM_STREAM_CAPTURE, 1, 1, 0,
			asrc_apf_coef_get, asrc_apf_coef_put),
	SOC_SINGLE_EXT("SPUM ASRC2 APF COEF", SNDRV_PCM_STREAM_CAPTURE, 2, 1, 0,
			asrc_apf_coef_get, asrc_apf_coef_put),
	SOC_SINGLE_EXT("SPUM ASRC3 APF COEF", SNDRV_PCM_STREAM_CAPTURE, 3, 1, 0,
			asrc_apf_coef_get, asrc_apf_coef_put),
	SOC_SINGLE_EXT("SPUM ASRC4 APF COEF", SNDRV_PCM_STREAM_CAPTURE, 4, 1, 0,
			asrc_apf_coef_get, asrc_apf_coef_put),
};

static const char * const asrc_source_enum_texts[] = {
	"CP",
	"UAIF0",
	"UAIF1",
	"UAIF2",
	"UAIF3",
	"UAIF4",
	"UAIF5",
	"UAIF6",
	"USB",
	"BCLK_CP",
	"BCLK_SPDY",
	"ABOX",
};

static const unsigned int asrc_source_enum_values[] = {
	TICK_CP,
	TICK_UAIF0,
	TICK_UAIF1,
	TICK_UAIF2,
	TICK_UAIF3,
	TICK_UAIF4,
	TICK_UAIF5,
	TICK_UAIF6,
	TICK_USB,
	TICK_BCLK_CP,
	TICK_BCLK_SPDY,
	TICK_SYNC,
};

static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc0_os_enum, 0, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc1_os_enum, 1, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc2_os_enum, 2, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc3_os_enum, 3, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc4_os_enum, 4, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc5_os_enum, 5, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc6_os_enum, 6, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc7_os_enum, 7, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc8_os_enum, 8, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc9_os_enum, 9, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc10_os_enum, 10, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc11_os_enum, 11, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);

static const struct snd_kcontrol_new spus_asrc_os_controls[] = {
	SOC_VALUE_ENUM_EXT("SPUS ASRC0 OS", spus_asrc0_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC1 OS", spus_asrc1_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC2 OS", spus_asrc2_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC3 OS", spus_asrc3_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC4 OS", spus_asrc4_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC5 OS", spus_asrc5_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC6 OS", spus_asrc6_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC7 OS", spus_asrc7_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC8 OS", spus_asrc8_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC9 OS", spus_asrc9_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC10 OS", spus_asrc10_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC11 OS", spus_asrc11_os_enum,
			spus_asrc_os_get, spus_asrc_os_put),
};

static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc0_is_enum, 0, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc1_is_enum, 1, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc2_is_enum, 2, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc3_is_enum, 3, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc4_is_enum, 4, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc5_is_enum, 5, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc6_is_enum, 6, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc7_is_enum, 7, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc8_is_enum, 8, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc9_is_enum, 9, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc10_is_enum, 10, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spus_asrc11_is_enum, 11, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);

static const struct snd_kcontrol_new spus_asrc_is_controls[] = {
	SOC_VALUE_ENUM_EXT("SPUS ASRC0 IS", spus_asrc0_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC1 IS", spus_asrc1_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC2 IS", spus_asrc2_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC3 IS", spus_asrc3_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC4 IS", spus_asrc4_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC5 IS", spus_asrc5_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC6 IS", spus_asrc6_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC7 IS", spus_asrc7_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC8 IS", spus_asrc8_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC9 IS", spus_asrc9_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC10 IS", spus_asrc10_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUS ASRC11 IS", spus_asrc11_is_enum,
			spus_asrc_is_get, spus_asrc_is_put),
};

static SOC_VALUE_ENUM_SINGLE_DECL(spum_asrc0_os_enum, 0, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spum_asrc1_os_enum, 1, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spum_asrc2_os_enum, 2, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spum_asrc3_os_enum, 3, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spum_asrc4_os_enum, 4, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);

static const struct snd_kcontrol_new spum_asrc_os_controls[] = {
	SOC_VALUE_ENUM_EXT("SPUM ASRC0 OS", spum_asrc0_os_enum,
			spum_asrc_os_get, spum_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUM ASRC1 OS", spum_asrc1_os_enum,
			spum_asrc_os_get, spum_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUM ASRC2 OS", spum_asrc2_os_enum,
			spum_asrc_os_get, spum_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUM ASRC3 OS", spum_asrc3_os_enum,
			spum_asrc_os_get, spum_asrc_os_put),
	SOC_VALUE_ENUM_EXT("SPUM ASRC4 OS", spum_asrc4_os_enum,
			spum_asrc_os_get, spum_asrc_os_put),
};

static SOC_VALUE_ENUM_SINGLE_DECL(spum_asrc0_is_enum, 0, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spum_asrc1_is_enum, 1, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spum_asrc2_is_enum, 2, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spum_asrc3_is_enum, 3, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);
static SOC_VALUE_ENUM_SINGLE_DECL(spum_asrc4_is_enum, 4, 0, 0,
		asrc_source_enum_texts, asrc_source_enum_values);

static const struct snd_kcontrol_new spum_asrc_is_controls[] = {
	SOC_VALUE_ENUM_EXT("SPUM ASRC0 IS", spum_asrc0_is_enum,
			spum_asrc_is_get, spum_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUM ASRC1 IS", spum_asrc1_is_enum,
			spum_asrc_is_get, spum_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUM ASRC2 IS", spum_asrc2_is_enum,
			spum_asrc_is_get, spum_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUM ASRC3 IS", spum_asrc3_is_enum,
			spum_asrc_is_get, spum_asrc_is_put),
	SOC_VALUE_ENUM_EXT("SPUM ASRC4 IS", spum_asrc4_is_enum,
			spum_asrc_is_get, spum_asrc_is_put),
};

struct snd_soc_dapm_widget *spus_asrc_widgets[ARRAY_SIZE(spus_asrc_controls)];
struct snd_soc_dapm_widget *spum_asrc_widgets[ARRAY_SIZE(spum_asrc_controls)];

static void asrc_cache_widget(struct snd_soc_dapm_widget *w,
		int idx, int stream)
{
	struct device *dev = w->dapm->dev;

	abox_dbg(dev, "%s(%s, %d, %d)\n", __func__, w->name, idx, stream);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		spus_asrc_widgets[idx] = w;
	else
		spum_asrc_widgets[idx] = w;
}

static struct snd_soc_dapm_widget *asrc_find_widget(
		struct snd_soc_card *card, int idx, int stream)
{
	struct device *dev = card->dev;
	struct snd_soc_dapm_widget *w;
	const char *name;

	abox_dbg(dev, "%s(%s, %d, %d)\n", __func__, card->name, idx, stream);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		name = spus_asrc_controls[idx].name;
	else
		name = spum_asrc_controls[idx].name;

	list_for_each_entry(w, &card->widgets, list) {
		struct snd_soc_component *cmpnt = w->dapm->component;
		const char *name_prefix = cmpnt ? cmpnt->name_prefix : NULL;
		size_t prefix_len = name_prefix ? strlen(name_prefix) + 1 : 0;
		const char *w_name = w->name + prefix_len;

		if (!strcmp(name, w_name))
			return w;
	}

	return NULL;
}

static struct snd_soc_dapm_widget *asrc_get_widget(
		struct snd_soc_component *cmpnt, int idx, int stream)
{
	struct snd_soc_dapm_widget *w;
	struct device *dev;

	if (idx < 0)
		return NULL;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		w = spus_asrc_widgets[idx];
	else
		w = spum_asrc_widgets[idx];

	if (!w) {
		w = asrc_find_widget(cmpnt->card, idx, stream);
		asrc_cache_widget(w, idx, stream);
	}

	dev = w->dapm->dev;
	abox_dbg(dev, "%s(%d, %d): %s\n", __func__, idx, stream, w->name);

	return w;
}

static bool is_direct_connection(struct snd_soc_component *cmpnt,
		enum abox_dai dai)
{
	unsigned int val = 0;
	bool ret;

	switch (dai) {
	case ABOX_NSRC0:
	case ABOX_NSRC1:
	case ABOX_NSRC2:
	case ABOX_NSRC3:
	case ABOX_NSRC4:
	case ABOX_NSRC5:
	case ABOX_NSRC6:
	case ABOX_NSRC7:
		snd_soc_component_read(cmpnt, ABOX_ROUTE_CTRL2, &val);
		val &= ABOX_NSRC_CONNECTION_TYPE_MASK(dai - ABOX_NSRC0);
		val >>= ABOX_NSRC_CONNECTION_TYPE_L(dai - ABOX_NSRC0);
		ret = !val;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

static enum abox_dai get_sink_dai_id(struct abox_data *data, enum abox_dai id);

static enum abox_dai get_source_dai_id(struct abox_data *data, enum abox_dai id)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int val = 0;
	enum abox_dai _id, ret = ABOX_NONE;

	switch (id) {
	case ABOX_WDMA0:
	case ABOX_WDMA0_BE:
		ret = ABOX_NSRC0;
		break;
	case ABOX_WDMA1:
	case ABOX_WDMA1_BE:
		ret = ABOX_NSRC1;
		break;
	case ABOX_WDMA2:
	case ABOX_WDMA2_BE:
		ret = ABOX_NSRC2;
		break;
	case ABOX_WDMA3:
	case ABOX_WDMA3_BE:
		ret = ABOX_NSRC3;
		break;
	case ABOX_WDMA4:
	case ABOX_WDMA4_BE:
		ret = ABOX_NSRC4;
		break;
	case ABOX_WDMA5:
	case ABOX_WDMA5_BE:
		ret = ABOX_NSRC5;
		break;
	case ABOX_WDMA6:
	case ABOX_WDMA6_BE:
		ret = ABOX_NSRC6;
		break;
	case ABOX_WDMA7:
	case ABOX_WDMA7_BE:
		ret = ABOX_NSRC7;
		break;
	case ABOX_UAIF0:
	case ABOX_UAIF1:
	case ABOX_UAIF2:
	case ABOX_UAIF3:
	case ABOX_UAIF4:
	case ABOX_UAIF5:
	case ABOX_UAIF6:
		snd_soc_component_read(cmpnt, ABOX_ROUTE_CTRL0, &val);
		val &= ABOX_ROUTE_UAIF_SPK_MASK(id - ABOX_UAIF0);
		val >>= ABOX_ROUTE_UAIF_SPK_L(id - ABOX_UAIF0);
		switch (val) {
		case 0x1:
			ret = ABOX_SIFS0;
			break;
		case 0x2:
			ret = ABOX_SIFS1;
			break;
		case 0x3:
			ret = ABOX_SIFS2;
			break;
		case 0x4:
			ret = ABOX_SIFS3;
			break;
		case 0x5:
			ret = ABOX_SIFS4;
			break;
		case 0x6:
			ret = ABOX_SIFS5;
			break;
		default:
			ret = ABOX_NONE;
			break;
		}
		break;
	case ABOX_DSIF:
		snd_soc_component_read(cmpnt, ABOX_ROUTE_CTRL0, &val);
		val &= ABOX_ROUTE_DSIF_MASK;
		val >>= ABOX_ROUTE_DSIF_L;
		switch (val) {
		case 0x2:
			ret = ABOX_SIFS1;
			break;
		case 0x3:
			ret = ABOX_SIFS2;
			break;
		case 0x4:
			ret = ABOX_SIFS3;
			break;
		case 0x5:
			ret = ABOX_SIFS4;
			break;
		case 0x6:
			ret = ABOX_SIFS5;
			break;
		default:
			ret = ABOX_NONE;
			break;
		}
		break;
	case ABOX_SIFS0:
	case ABOX_SIFS1:
	case ABOX_SIFS2:
	case ABOX_SIFS3:
	case ABOX_SIFS4:
	case ABOX_SIFS5:
		for (_id = ABOX_RDMA0; _id <= ABOX_RDMA11; _id++) {
			if (get_sink_dai_id(data, _id) == id) {
				ret = _id;
				break;
			}
		}
		break;
	case ABOX_NSRC0:
	case ABOX_NSRC1:
	case ABOX_NSRC2:
	case ABOX_NSRC3:
	case ABOX_NSRC4:
	case ABOX_NSRC5:
	case ABOX_NSRC6:
	case ABOX_NSRC7:
		snd_soc_component_read(cmpnt, ABOX_ROUTE_CTRL1, &val);
		val &= ABOX_ROUTE_NSRC_MASK(id - ABOX_NSRC0);
		val >>= ABOX_ROUTE_NSRC_L(id - ABOX_NSRC0);
		switch (val) {
		case 0x1:
			ret = ABOX_SIFS0;
			break;
		case 0x2:
			ret = ABOX_SIFS1;
			break;
		case 0x3:
			ret = ABOX_SIFS2;
			break;
		case 0x4:
			ret = ABOX_SIFS3;
			break;
		case 0x5:
			ret = ABOX_SIFS4;
			break;
		case 0x6:
			ret = ABOX_SIFS5;
			break;
		case 0x8:
			ret = ABOX_UAIF0;
			break;
		case 0x9:
			ret = ABOX_UAIF1;
			break;
		case 0xa:
			ret = ABOX_UAIF2;
			break;
		case 0xb:
			ret = ABOX_UAIF3;
			break;
		case 0xc:
			ret = ABOX_UAIF4;
			break;
		case 0xd:
			ret = ABOX_UAIF5;
			break;
		case 0xe:
			ret = ABOX_UAIF6;
			break;
		case 0x10:
			ret = ABOX_BI_PDI0;
			break;
		case 0x11:
			ret = ABOX_BI_PDI1;
			break;
		case 0x12:
			ret = ABOX_BI_PDI2;
			break;
		case 0x13:
			ret = ABOX_BI_PDI3;
			break;
		case 0x14:
			ret = ABOX_BI_PDI4;
			break;
		case 0x15:
			ret = ABOX_BI_PDI5;
			break;
		case 0x16:
			ret = ABOX_BI_PDI6;
			break;
		case 0x17:
			ret = ABOX_BI_PDI7;
			break;
		case 0x18:
			ret = ABOX_RX_PDI0;
			break;
		case 0x19:
			ret = ABOX_RX_PDI1;
			break;
		case 0x1f:
			ret = ABOX_SPDY;
			break;
		default:
			ret = ABOX_NONE;
			break;
		}
		break;
	case ABOX_RDMA0:
	case ABOX_RDMA1:
	case ABOX_RDMA2:
	case ABOX_RDMA3:
	case ABOX_RDMA0_BE:
	case ABOX_RDMA1_BE:
	case ABOX_RDMA2_BE:
	case ABOX_RDMA3_BE:
		snd_soc_component_read(cmpnt, ABOX_SPUS_CTRL_FC0, &val);
		val &= ABOX_FUNC_CHAIN_SRC_IN_MASK(id - ABOX_RDMA0);
		val >>= ABOX_FUNC_CHAIN_SRC_IN_L(id - ABOX_RDMA0);
		switch (val) {
		case 0x1:
			ret = ABOX_SIFSM;
			break;
		case 0x3:
			ret = ABOX_SIFST;
			break;
		default:
			ret = ABOX_NONE;
			break;
		}
		break;
	case ABOX_RDMA4:
	case ABOX_RDMA5:
	case ABOX_RDMA6:
	case ABOX_RDMA7:
	case ABOX_RDMA4_BE:
	case ABOX_RDMA5_BE:
	case ABOX_RDMA6_BE:
	case ABOX_RDMA7_BE:
		snd_soc_component_read(cmpnt, ABOX_SPUS_CTRL_FC1, &val);
		val &= ABOX_FUNC_CHAIN_SRC_IN_MASK(id - ABOX_RDMA0);
		val >>= ABOX_FUNC_CHAIN_SRC_IN_L(id - ABOX_RDMA0);
		switch (val) {
		case 0x1:
			ret = ABOX_SIFSM;
			break;
		case 0x3:
			ret = ABOX_SIFST;
			break;
		default:
			ret = ABOX_NONE;
			break;
		}
		break;
	case ABOX_RDMA8:
	case ABOX_RDMA9:
	case ABOX_RDMA10:
	case ABOX_RDMA11:
	case ABOX_RDMA8_BE:
	case ABOX_RDMA9_BE:
	case ABOX_RDMA10_BE:
	case ABOX_RDMA11_BE:
		snd_soc_component_read(cmpnt, ABOX_SPUS_CTRL_FC2, &val);
		val &= ABOX_FUNC_CHAIN_SRC_IN_MASK(id - ABOX_RDMA0);
		val >>= ABOX_FUNC_CHAIN_SRC_IN_L(id - ABOX_RDMA0);
		switch (val) {
		case 0x1:
			ret = ABOX_SIFSM;
			break;
		case 0x3:
			ret = ABOX_SIFST;
			break;
		default:
			ret = ABOX_NONE;
			break;
		}
		break;
	case ABOX_SIFSM:
		snd_soc_component_read(cmpnt, ABOX_ROUTE_CTRL0, &val);
		val &= ABOX_ROUTE_SPUSM_MASK;
		val >>= ABOX_ROUTE_SPUSM_L;
		switch (val) {
		case 0x8:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
			ret = ABOX_UAIF0 + val - 0x8;
			break;
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
			ret = ABOX_BI_PDI0 + val - 0x10;
			break;
		case 0x18:
		case 0x19:
			ret = ABOX_RX_PDI0 + val - 0x18;
			break;
		default:
			ret = ABOX_NONE;
			break;
		}
		break;
	case ABOX_SIFST:
		snd_soc_component_read(cmpnt, ABOX_ROUTE_CTRL2, &val);
		val &= ABOX_ROUTE_SPUST_MASK;
		val >>= ABOX_ROUTE_SPUST_L;
		switch (val) {
		case 0x8:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd:
		case 0xe:
			ret = ABOX_UAIF0 + val - 0x8;
			break;
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
			ret = ABOX_BI_PDI0 + val - 0x10;
			break;
		case 0x18:
		case 0x19:
			ret = ABOX_RX_PDI0 + val - 0x18;
			break;
		default:
			ret = ABOX_NONE;
			break;
		}
		break;
	default:
		ret = ABOX_NONE;
		break;
	}

	return ret;
}

static enum abox_dai get_sink_dai_id(struct abox_data *data, enum abox_dai id)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int val = 0;
	enum abox_dai _id, ret = ABOX_NONE;

	switch (id) {
	case ABOX_RDMA0:
	case ABOX_RDMA1:
	case ABOX_RDMA2:
	case ABOX_RDMA3:
	case ABOX_RDMA0_BE:
	case ABOX_RDMA1_BE:
	case ABOX_RDMA2_BE:
	case ABOX_RDMA3_BE:
		snd_soc_component_read(cmpnt, ABOX_SPUS_CTRL_FC0, &val);
		val &= ABOX_FUNC_CHAIN_SRC_OUT_MASK(id - ABOX_RDMA0);
		val >>= ABOX_FUNC_CHAIN_SRC_OUT_L(id - ABOX_RDMA0);
		switch (val) {
		case 0x1:
			ret = ABOX_SIFS0;
			break;
		case 0x2:
			ret = ABOX_SIFS1;
			break;
		case 0x4:
			ret = ABOX_SIFS2;
			break;
		case 0x6:
			ret = ABOX_SIFS3;
			break;
		case 0x8:
			ret = ABOX_SIFS4;
			break;
		case 0xa:
			ret = ABOX_SIFS5;
			break;
		default:
			ret = ABOX_NONE;
			break;
		}
		break;
	case ABOX_RDMA4:
	case ABOX_RDMA5:
	case ABOX_RDMA6:
	case ABOX_RDMA7:
	case ABOX_RDMA4_BE:
	case ABOX_RDMA5_BE:
	case ABOX_RDMA6_BE:
	case ABOX_RDMA7_BE:
		snd_soc_component_read(cmpnt, ABOX_SPUS_CTRL_FC1, &val);
		val &= ABOX_FUNC_CHAIN_SRC_OUT_MASK(id - ABOX_RDMA0);
		val >>= ABOX_FUNC_CHAIN_SRC_OUT_L(id - ABOX_RDMA0);
		switch (val) {
		case 0x1:
			ret = ABOX_SIFS0;
			break;
		case 0x2:
			ret = ABOX_SIFS1;
			break;
		case 0x4:
			ret = ABOX_SIFS2;
			break;
		case 0x6:
			ret = ABOX_SIFS3;
			break;
		case 0x8:
			ret = ABOX_SIFS4;
			break;
		case 0xa:
			ret = ABOX_SIFS5;
			break;
		default:
			ret = ABOX_NONE;
			break;
		}
		break;
	case ABOX_RDMA8:
	case ABOX_RDMA9:
	case ABOX_RDMA10:
	case ABOX_RDMA11:
	case ABOX_RDMA8_BE:
	case ABOX_RDMA9_BE:
	case ABOX_RDMA10_BE:
	case ABOX_RDMA11_BE:
		snd_soc_component_read(cmpnt, ABOX_SPUS_CTRL_FC2, &val);
		val &= ABOX_FUNC_CHAIN_SRC_OUT_MASK(id - ABOX_RDMA0);
		val >>= ABOX_FUNC_CHAIN_SRC_OUT_L(id - ABOX_RDMA0);
		switch (val) {
		case 0x1:
			ret = ABOX_SIFS0;
			break;
		case 0x2:
			ret = ABOX_SIFS1;
			break;
		case 0x4:
			ret = ABOX_SIFS2;
			break;
		case 0x6:
			ret = ABOX_SIFS3;
			break;
		case 0x8:
			ret = ABOX_SIFS4;
			break;
		case 0xa:
			ret = ABOX_SIFS5;
			break;
		default:
			ret = ABOX_NONE;
			break;
		}
		break;
	case ABOX_UAIF0:
	case ABOX_UAIF1:
	case ABOX_UAIF2:
	case ABOX_UAIF3:
	case ABOX_UAIF4:
	case ABOX_UAIF5:
	case ABOX_UAIF6:
	case ABOX_SPDY:
		for (_id = ABOX_NSRC0; _id <= ABOX_NSRC5; _id++) {
			if (get_source_dai_id(data, _id) == id &&
					is_direct_connection(cmpnt, _id)) {
				ret = _id;
				break;
			}
		}
		break;
	case ABOX_SIFS0:
	case ABOX_SIFS1:
	case ABOX_SIFS2:
	case ABOX_SIFS3:
	case ABOX_SIFS4:
	case ABOX_SIFS5:
		for (_id = ABOX_UAIF0; _id <= ABOX_DSIF; _id++) {
			if (get_source_dai_id(data, _id) == id) {
				ret = _id;
				break;
			}
		}
		if (ret != ABOX_NONE)
			break;

		for (_id = ABOX_NSRC0; _id <= ABOX_NSRC5; _id++) {
			if (get_source_dai_id(data, _id) == id &&
					is_direct_connection(cmpnt, _id)) {
				ret = _id;
				break;
			}
		}
		break;
	case ABOX_NSRC0:
		ret = ABOX_WDMA0;
		break;
	case ABOX_NSRC1:
		ret = ABOX_WDMA1;
		break;
	case ABOX_NSRC2:
		ret = ABOX_WDMA2;
		break;
	case ABOX_NSRC3:
		ret = ABOX_WDMA3;
		break;
	case ABOX_NSRC4:
		ret = ABOX_WDMA4;
		break;
	case ABOX_NSRC5:
		ret = ABOX_WDMA5;
		break;
	case ABOX_NSRC6:
		ret = ABOX_WDMA6;
		break;
	case ABOX_NSRC7:
		ret = ABOX_WDMA7;
		break;
	default:
		ret = ABOX_NONE;
		break;
	}

	return ret;
}

static struct snd_soc_dai *find_dai(struct snd_soc_card *card, enum abox_dai id)
{
	struct snd_soc_pcm_runtime *rtd;

	list_for_each_entry(rtd, &card->rtd_list, list) {
		if (rtd->cpu_dai->id == id)
			return rtd->cpu_dai;
	}

	return NULL;
}

static int get_configmsg(enum abox_dai id, enum ABOX_CONFIGMSG *rate,
		enum ABOX_CONFIGMSG *format)
{
	int ret = 0;

	switch (id) {
	case ABOX_SIFS0:
		*rate = SET_SIFS0_RATE;
		*format = SET_SIFS0_FORMAT;
		break;
	case ABOX_SIFS1:
		*rate = SET_SIFS1_RATE;
		*format = SET_SIFS1_FORMAT;
		break;
	case ABOX_SIFS2:
		*rate = SET_SIFS2_RATE;
		*format = SET_SIFS2_FORMAT;
		break;
	case ABOX_SIFS3:
		*rate = SET_SIFS3_RATE;
		*format = SET_SIFS3_FORMAT;
		break;
	case ABOX_SIFS4:
		*rate = SET_SIFS4_RATE;
		*format = SET_SIFS4_FORMAT;
		break;
	case ABOX_SIFS5:
		*rate = SET_SIFS5_RATE;
		*format = SET_SIFS5_FORMAT;
		break;
	case ABOX_NSRC0:
		*rate = SET_SIFM0_RATE;
		*format = SET_SIFM0_FORMAT;
		break;
	case ABOX_NSRC1:
		*rate = SET_SIFM1_RATE;
		*format = SET_SIFM1_FORMAT;
		break;
	case ABOX_NSRC2:
		*rate = SET_SIFM2_RATE;
		*format = SET_SIFM2_FORMAT;
		break;
	case ABOX_NSRC3:
		*rate = SET_SIFM3_RATE;
		*format = SET_SIFM3_FORMAT;
		break;
	case ABOX_NSRC4:
		*rate = SET_SIFM4_RATE;
		*format = SET_SIFM4_FORMAT;
		break;
	case ABOX_NSRC5:
		*rate = SET_SIFM5_RATE;
		*format = SET_SIFM5_FORMAT;
		break;
	case ABOX_NSRC6:
		*rate = SET_SIFM6_RATE;
		*format = SET_SIFM6_FORMAT;
		break;
	case ABOX_NSRC7:
		*rate = SET_SIFM7_RATE;
		*format = SET_SIFM7_FORMAT;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int set_sif_params(struct abox_data *data, enum abox_dai id,
		const struct snd_pcm_hw_params *params)
{
	struct device *adev = data->dev;
	enum ABOX_CONFIGMSG msg_rate, msg_format;
	unsigned int rate, channels;
	snd_pcm_format_t format;
	int ret = 0;

	ret = get_configmsg(id, &msg_rate, &msg_format);
	if (ret < 0) {
		abox_err(adev, "can't set sif params: %d\n", ret);
		return ret;
	}

	rate = params_rate(params);
	format = params_format(params);
	channels = params_channels(params);

	if (get_sif_rate(data, msg_rate) != rate) {
		set_sif_rate(data, msg_rate, rate);
		rate_put_ipc(adev, rate, msg_rate);
	}

	if (get_sif_channels(data, msg_format) != channels ||
			get_sif_format(data, msg_format) != format) {
		set_sif_format(data, msg_format, format);
		set_sif_channels(data, msg_format, channels);
		format_put_ipc(adev, format, channels, msg_format);
	}

	return ret;
}

static unsigned int sifsx_cnt_val(unsigned long aclk, unsigned int rate,
		unsigned int physical_width, unsigned int channels)
{
	static const int correction;
	unsigned int n, d;

	/* k = n / d */
	d = channels * rate;
	n = 2 * (32 / physical_width);

	return DIV_ROUND_CLOSEST_ULL(aclk * n, d) - 1 + correction;
}

static int set_cnt_val(struct abox_data *data, struct snd_soc_dai *dai,
		struct snd_pcm_hw_params *params)
{
	struct device *dev = dai->dev;
	struct regmap *regmap = data->regmap;
	enum abox_dai id = dai->id;
	int idx = id - ABOX_SIFS0;
	unsigned int rate = params_rate(params);
	unsigned int width = params_width(params);
	unsigned int pwidth = params_physical_width(params);
	unsigned int channels = params_channels(params);
	unsigned long clk;
	unsigned int cnt_val;
	int ret = 0;

	ret = abox_register_bclk_usage(dev, data, id, rate, channels, width);
	if (ret < 0)
		abox_err(dev, "Unable to register bclk usage: %d\n", ret);

	clk = clk_get_rate(data->clk_cnt);
	cnt_val = sifsx_cnt_val(clk, rate, pwidth, channels);

	abox_info(dev, "%s[%#x](%ubit %uchannel %uHz at %luHz): %u\n",
			__func__, id, width, channels, rate, clk, cnt_val);

	ret = regmap_update_bits(regmap, ABOX_SPUS_CTRL_SIFS_CNT(idx),
			ABOX_SIFS_CNT_VAL_MASK(idx),
			cnt_val << ABOX_SIFS_CNT_VAL_L(idx));

	return ret;
}

static int hw_params_fixup(struct snd_soc_dai *dai,
		struct snd_pcm_hw_params *params)
{
	struct device *dev = dai->dev;
	struct abox_data *data = abox_get_data(dev);
	enum ABOX_CONFIGMSG msg_rate, msg_format;
	unsigned int rate, channels, width;
	snd_pcm_format_t format;
	int ret = 0;

	abox_dbg(dev, "%s(%s)\n", __func__, dai->name);

	ret = get_configmsg(dai->id, &msg_rate, &msg_format);
	if (ret < 0)
		return ret;

	rate = get_sif_rate(data, msg_rate);
	channels = get_sif_channels(data, msg_format);
	width = get_sif_width(data, msg_format);
	format = get_sif_format(data, msg_format);

	if (rate)
		hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE)->min = rate;

	if (channels)
		hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS)->min =
				channels;

	if (format) {
		struct snd_mask *mask;

		mask = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);
		snd_mask_none(mask);
		snd_mask_set(mask, format);
	}

	if (rate || channels || format)
		abox_dbg(dev, "%s: %d bit, %u ch, %uHz\n", dai->name,
				width, channels, rate);

	return ret;
}

static int sifs_hw_params_fixup(struct snd_soc_dai *dai,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_component *cmpnt = dai->component;
	struct device *dev = dai->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	enum abox_dai id = dai->id;
	enum abox_dai _id;
	struct snd_soc_dai *_dai;

	abox_dbg(dev, "%s(%s)\n", __func__, dai->name);

	_id = get_sink_dai_id(data, id);
	switch (_id) {
	case ABOX_UAIF0:
	case ABOX_UAIF1:
	case ABOX_UAIF2:
	case ABOX_UAIF3:
	case ABOX_UAIF4:
	case ABOX_UAIF5:
	case ABOX_UAIF6:
	case ABOX_DSIF:
		_dai = find_dai(cmpnt->card, _id);
		abox_if_hw_params_fixup(_dai, params);
		break;
	case ABOX_NSRC0:
	case ABOX_NSRC1:
	case ABOX_NSRC2:
	case ABOX_NSRC3:
	case ABOX_NSRC4:
	case ABOX_NSRC5:
	case ABOX_NSRC6:
	case ABOX_NSRC7:
		hw_params_fixup(dai, params);
		set_cnt_val(data, dai, params);
		break;
	default:
		abox_err(dev, "%s: invalid sink dai:%#x\n", dai->name, _id);
		break;
	}

	return set_sif_params(data, id, params);
}

static int sifm_hw_params_fixup(struct snd_soc_dai *dai,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_component *cmpnt = dai->component;
	struct device *dev = dai->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	enum abox_dai id = dai->id;
	enum abox_dai _id;
	struct snd_soc_dai *_dai;

	abox_dbg(dev, "%s(%s)\n", __func__, dai->name);

	_id = get_source_dai_id(data, id);
	switch (_id) {
	case ABOX_UAIF0:
	case ABOX_UAIF1:
	case ABOX_UAIF2:
	case ABOX_UAIF3:
	case ABOX_UAIF4:
	case ABOX_UAIF5:
	case ABOX_UAIF6:
	case ABOX_DSIF:
	case ABOX_SPDY:
		_dai = find_dai(cmpnt->card, _id);
		abox_if_hw_params_fixup(_dai, params);
		break;
	case ABOX_SIFS0:
	case ABOX_SIFS1:
	case ABOX_SIFS2:
	case ABOX_SIFS3:
	case ABOX_SIFS4:
	case ABOX_SIFS5:
		_dai = find_dai(cmpnt->card, _id);
		sifs_hw_params_fixup(_dai, params);
		break;
	default:
		abox_err(dev, "%s: invalid source dai:%#x\n", dai->name, _id);
		break;
	}

	return set_sif_params(data, id, params);
}

static int rdma_hw_params_fixup(struct snd_soc_dai *dai,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_component *cmpnt = dai->component;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(dai);
	struct device *dev = dai->dev;
	enum abox_dai id = dai->id;
	enum abox_dai _id;
	struct snd_soc_dai *_dai;
	int ret;

	abox_dbg(dev, "%s(%s)\n", __func__, dai->name);

	_id = get_sink_dai_id(data->abox_data, id);
	switch (_id) {
	case ABOX_SIFS0:
	case ABOX_SIFS1:
	case ABOX_SIFS2:
	case ABOX_SIFS3:
	case ABOX_SIFS4:
	case ABOX_SIFS5:
		_dai = find_dai(cmpnt->card, _id);
		ret = sifs_hw_params_fixup(_dai, params);
		abox_dma_set_dst_bit_width(dev, params_width(params));
		break;
	default:
		abox_err(dev, "%s: invalid sifs dai:%#x\n", dai->name, _id);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int wdma_hw_params_fixup(struct snd_soc_dai *dai,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_component *cmpnt = dai->component;
	struct abox_dma_data *data = snd_soc_dai_get_drvdata(dai);
	struct device *dev = dai->dev;
	enum abox_dai id = dai->id;
	enum abox_dai _id;
	struct snd_soc_dai *_dai;
	int ret;

	abox_dbg(dev, "%s(%s)\n", __func__, dai->name);

	_id = get_source_dai_id(data->abox_data, id);
	switch (_id) {
	case ABOX_NSRC0:
	case ABOX_NSRC1:
	case ABOX_NSRC2:
	case ABOX_NSRC3:
	case ABOX_NSRC4:
	case ABOX_NSRC5:
	case ABOX_NSRC6:
	case ABOX_NSRC7:
		_dai = find_dai(cmpnt->card, _id);
		ret = sifm_hw_params_fixup(_dai, params);
		abox_dma_set_dst_bit_width(dev, params_width(params));
		break;
	default:
		abox_err(dev, "%s: invalid sifs dai:%#x\n", dai->name, _id);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int rdma_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
		struct snd_pcm_hw_params *params)
{
	struct snd_pcm_hw_params _params = *params;

	rdma_hw_params_fixup(rtd->cpu_dai, &_params);
	return abox_dma_hw_params_fixup(rtd->cpu_dai->dev, params);
}

static int wdma_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
		struct snd_pcm_hw_params *params)
{
	struct snd_pcm_hw_params _params = *params;

	wdma_hw_params_fixup(rtd->cpu_dai, &_params);
	return abox_dma_hw_params_fixup(rtd->cpu_dai->dev, params);
}

unsigned int abox_cmpnt_sif_get_dst_format(struct abox_data *data,
		int stream, int id)
{
	static const enum ABOX_CONFIGMSG configmsg_p[] = {
		SET_SIFS0_FORMAT, SET_SIFS1_FORMAT, SET_SIFS2_FORMAT,
		SET_SIFS3_FORMAT, SET_SIFS4_FORMAT, SET_SIFS5_FORMAT,
	};
	static const enum ABOX_CONFIGMSG configmsg_c[] = {
		SET_SIFM0_FORMAT, SET_SIFM1_FORMAT, SET_SIFM2_FORMAT,
		SET_SIFM3_FORMAT, SET_SIFM4_FORMAT, SET_SIFM5_FORMAT,
		SET_SIFM6_FORMAT, SET_SIFM7_FORMAT,
	};
	const enum ABOX_CONFIGMSG *configmsg;
	unsigned int width, channels, format;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (id >= ARRAY_SIZE(configmsg_p)) {
			abox_err(data->dev, "%s(%d, %d): invalid argument\n",
					__func__, stream, id);
			return -EINVAL;
		}
		configmsg = configmsg_p;
	} else {
		if (id >= ARRAY_SIZE(configmsg_c)) {
			abox_err(data->dev, "%s(%d, %d): invalid argument\n",
					__func__, stream, id);
			return -EINVAL;
		}
		configmsg = configmsg_c;
	}

	width = get_sif_width(data, configmsg[id]);
	channels = get_sif_channels(data, configmsg[id]);
	format = abox_get_format(width, channels);

	return format;
}

void abox_cmpnt_register_event_notifier(struct abox_data *data,
		enum abox_widget w, int (*notify)(void *priv, bool en),
		void *priv)
{
	struct abox_event_notifier *event_notifier = &data->event_notifier[w];

	WRITE_ONCE(event_notifier->priv, priv);
	WRITE_ONCE(event_notifier->notify, notify);
}

void abox_cmpnt_unregister_event_notifier(struct abox_data *data,
		enum abox_widget w)
{
	struct abox_event_notifier *event_notifier = &data->event_notifier[w];

	WRITE_ONCE(event_notifier->notify, NULL);
	WRITE_ONCE(event_notifier->priv, NULL);
}

static int notify_event(struct abox_data *data, enum abox_widget w, int e)
{
	struct abox_event_notifier event_notifier;
	bool en;

	switch (e) {
	case SND_SOC_DAPM_POST_PMU:
	case SND_SOC_DAPM_PRE_PMU:
		en = true;
		break;
	case SND_SOC_DAPM_PRE_PMD:
	case SND_SOC_DAPM_POST_PMD:
		en = false;
		break;
	default:
		return 0;
	};

	event_notifier = data->event_notifier[w];

	if (event_notifier.notify)
		return event_notifier.notify(event_notifier.priv, en);
	else
		return 0;
}

static int spus_in0_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_SPUS_IN0, e);
}

static int spus_in1_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_SPUS_IN1, e);
}

static int spus_in2_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_SPUS_IN2, e);
}

static int spus_in3_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_SPUS_IN3, e);
}

static int spus_in4_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_SPUS_IN4, e);
}

static int spus_in5_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_SPUS_IN5, e);
}

static int spus_in6_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_SPUS_IN6, e);
}

static int spus_in7_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_SPUS_IN7, e);
}
static int spus_in8_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_SPUS_IN8, e);
}
static int spus_in9_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_SPUS_IN9, e);
}

static int spus_in10_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_SPUS_IN10, e);
}

static int spus_in11_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_SPUS_IN11, e);
}

static int sifs0_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_SIFS0, e);
}

static int sifs1_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_SIFS1, e);
}

static int sifs2_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_SIFS2, e);
}

static int sifs3_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_SIFS3, e);
}

static int sifs4_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_SIFS4, e);
}

static int sifs5_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_SIFS5, e);
}

static int nsrc0_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_NSRC0, e);
}

static int nsrc1_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_NSRC1, e);
}

static int nsrc2_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_NSRC2, e);
}

static int nsrc3_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_NSRC3, e);
}

static int nsrc4_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	return notify_event(data, ABOX_WIDGET_NSRC4, e);
}

static int asrc_get_idx(struct snd_soc_dapm_widget *w)
{
	return w->shift;
}

static const struct snd_kcontrol_new *asrc_get_kcontrol(int idx, int stream)
{
	if (idx < 0)
		return ERR_PTR(-EINVAL);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (idx < ARRAY_SIZE(spus_asrc_controls))
			return &spus_asrc_controls[idx];
		else
			return ERR_PTR(-EINVAL);
	} else {
		if (idx < ARRAY_SIZE(spum_asrc_controls))
			return &spum_asrc_controls[idx];
		else
			return ERR_PTR(-EINVAL);
	}
}

static const struct snd_kcontrol_new *asrc_get_id_kcontrol(int idx, int stream)
{
	if (idx < 0)
		return ERR_PTR(-EINVAL);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (idx < ARRAY_SIZE(spus_asrc_id_controls))
			return &spus_asrc_id_controls[idx];
		else
			return ERR_PTR(-EINVAL);
	} else {
		if (idx < ARRAY_SIZE(spum_asrc_id_controls))
			return &spum_asrc_id_controls[idx];
		else
			return ERR_PTR(-EINVAL);
	}
}

static int asrc_get_id(struct snd_soc_component *cmpnt, int idx, int stream)
{
	const struct snd_kcontrol_new *kcontrol;
	struct soc_mixer_control *mc;
	unsigned int reg, mask, val;
	int ret;

	kcontrol = asrc_get_id_kcontrol(idx, stream);
	if (IS_ERR(kcontrol))
		return PTR_ERR(kcontrol);

	mc = (struct soc_mixer_control *)kcontrol->private_value;
	reg = mc->reg;
	mask = ((1 << fls(mc->max)) - 1) << mc->shift;
	ret = snd_soc_component_read(cmpnt, reg, &val);
	if (ret < 0)
		return ret;

	return (val & mask) >> mc->shift;
}

static bool asrc_get_active(struct snd_soc_component *cmpnt, int idx,
		int stream)
{
	const struct snd_kcontrol_new *kcontrol;
	struct soc_mixer_control *mc;
	unsigned int reg, mask, val;
	int ret;

	kcontrol = asrc_get_kcontrol(idx, stream);
	if (IS_ERR(kcontrol))
		return false;

	mc = (struct soc_mixer_control *)kcontrol->private_value;
	reg = mc->reg;
	mask = 1 << mc->shift;
	ret = snd_soc_component_read(cmpnt, reg, &val);
	if (ret < 0)
		return false;

	return !!(val & mask);
}

static int asrc_get_idx_of_id(struct snd_soc_component *cmpnt, int id,
		int stream)
{
	int idx;
	size_t len;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		len = ARRAY_SIZE(spus_asrc_controls);
	else
		len = ARRAY_SIZE(spum_asrc_controls);

	for (idx = 0; idx < len; idx++) {
		if (id == asrc_get_id(cmpnt, idx, stream))
			return idx;
	}

	return -EINVAL;
}

static bool asrc_get_id_active(struct snd_soc_component *cmpnt, int id,
		int stream)
{
	int idx;

	idx = asrc_get_idx_of_id(cmpnt, id, stream);
	if (idx < 0)
		return false;

	return asrc_get_active(cmpnt, idx, stream);
}

static int asrc_put_id(struct snd_soc_component *cmpnt, int idx, int stream,
		int id)
{
	struct device *dev = cmpnt->dev;
	const struct snd_kcontrol_new *kcontrol;
	struct soc_mixer_control *mc;
	unsigned int reg, mask, val;

	abox_dbg(dev, "%s(%d, %d, %d)\n", __func__, idx, stream, id);

	kcontrol = asrc_get_id_kcontrol(idx, stream);
	if (IS_ERR(kcontrol))
		return PTR_ERR(kcontrol);

	mc = (struct soc_mixer_control *)kcontrol->private_value;
	reg = mc->reg;
	mask = ((1 << fls(mc->max)) - 1) << mc->shift;
	val = id << mc->shift;
	return snd_soc_component_update_bits(cmpnt, reg, mask, val);
}

static int asrc_put_active(struct snd_soc_dapm_widget *w, int stream, int on)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_component *cmpnt = dapm->component;
	struct device *dev = cmpnt->dev;
	const struct snd_kcontrol_new *kcontrol;
	struct soc_mixer_control *mc;
	unsigned int reg, mask, val;

	abox_dbg(dev, "%s(%s, %d, %d)\n", __func__, w->name, stream, on);

	kcontrol = asrc_get_kcontrol(asrc_get_idx(w), stream);
	if (IS_ERR(kcontrol))
		return PTR_ERR(kcontrol);

	abox_info(dev, "%s %s\n", w->name, on ? "on" : "off");
	mc = (struct soc_mixer_control *)kcontrol->private_value;
	reg = mc->reg;
	mask = 1 << mc->shift;
	val = !!on << mc->shift;
	return snd_soc_component_update_bits(cmpnt, reg, mask, val);
}

static int asrc_exchange_id(struct snd_soc_component *cmpnt, int stream,
		int idx1, int idx2)
{
	struct device *dev = cmpnt->dev;
	int id1 = asrc_get_id(cmpnt, idx1, stream);
	int id2 = asrc_get_id(cmpnt, idx2, stream);
	int ret;

	abox_dbg(dev, "%s(%d, %d, %d)\n", __func__, stream, idx1, idx2);

	if (idx1 == idx2)
		return 0;

	ret = asrc_put_id(cmpnt, idx1, stream, id2);
	if (ret < 0)
		return ret;
	ret = asrc_put_id(cmpnt, idx2, stream, id1);
	if (ret < 0)
		asrc_put_id(cmpnt, idx1, stream, id1);

	return ret;
}

static const int spus_asrc_max_channels[] = {
	8, 4, 4, 2, 8, 4, 4, 2,
};

static const int spum_asrc_max_channels[] = {
	8, 4, 4, 2,
};

static const int spus_asrc_sorted_id[] = {
	3, 7, 2, 6, 1, 5, 0, 4,
};

static const int spum_asrc_sorted_id[] = {
	3, 2, 1, 0,
};

static int spus_asrc_channels[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
};

static int spum_asrc_channels[] = {
	0, 0, 0, 0,
};

static int spus_asrc_lock[] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

static int spum_asrc_lock[] = {
	-1, -1, -1, -1, -1, -1, -1, -1,
};

int abox_cmpnt_asrc_lock(struct abox_data *data, int stream,
		int idx, int id)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	int ret = 0;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		spus_asrc_lock[idx] = id;
	else
		spum_asrc_lock[idx] = id;

	if (id != asrc_get_id(cmpnt, idx, stream)) {
		ret = asrc_exchange_id(cmpnt, stream, idx,
				asrc_get_idx_of_id(cmpnt, id, stream));
		if (ret < 0)
			return ret;
		ret = asrc_put_active(asrc_get_widget(cmpnt, idx, stream),
				stream, 1);
		if (ret < 0)
			return ret;
	}

	return ret;
}

static bool asrc_is_lock(int stream, int id)
{
	int i;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		for (i = 0; i < ARRAY_SIZE(spus_asrc_lock); i++) {
			if (spus_asrc_lock[i] == id)
				return true;
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(spum_asrc_lock); i++) {
			if (spum_asrc_lock[i] == id)
				return true;
		}
	}

	return false;
}

static int asrc_get_lock_id(int stream, int idx)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		return spus_asrc_lock[idx];
	else
		return spum_asrc_lock[idx];
}

static int asrc_get_num(int stream)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		return ARRAY_SIZE(spus_asrc_sorted_id);
	else
		return ARRAY_SIZE(spum_asrc_sorted_id);
}

static int asrc_get_max_channels(int id, int stream)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		return spus_asrc_max_channels[id];
	else
		return spum_asrc_max_channels[id];
}

static int asrc_get_sorted_id(int i, int stream)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		return spus_asrc_sorted_id[i];
	else
		return spum_asrc_sorted_id[i];
}

static int asrc_get_channels(int id, int stream)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		return spus_asrc_channels[id];
	else
		return spum_asrc_channels[id];
}

static void asrc_set_channels(int id, int stream, int channels)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		spus_asrc_channels[id] = channels;
	else
		spum_asrc_channels[id] = channels;
}

static int asrc_is_avail_id(struct snd_soc_dapm_widget *w, int id,
		int stream, int channels)
{
	struct snd_soc_component *cmpnt = w->dapm->component;
	struct device *dev = cmpnt->dev;
	int idx = asrc_get_idx_of_id(cmpnt, id, stream);
	struct snd_soc_dapm_widget *w_t = asrc_get_widget(cmpnt, idx, stream);
	int ret;

	if (asrc_get_max_channels(id, stream) < channels) {
		ret = false;
		goto out;
	}

	if (w_t != w && asrc_is_lock(stream, id)) {
		ret = false;
		goto out;
	}

	if (w_t != w && asrc_get_id_active(cmpnt, id, stream)) {
		ret = false;
		goto out;
	}

	if (id % 2) {
		if (asrc_get_id_active(cmpnt, id - 1, stream))
			ret = asrc_get_channels(id - 1, stream) <
					asrc_get_max_channels(id - 1, stream);
		else
			ret = true;
	} else {
		if (channels < asrc_get_max_channels(id, stream))
			ret = true;
		else
			ret = !asrc_get_id_active(cmpnt, id + 1, stream);
	}
out:
	abox_dbg(dev, "%s(%d, %d, %d): %d\n", __func__,
			id, stream, channels, ret);
	return ret;
}

static int asrc_assign_id(struct snd_soc_dapm_widget *w, int stream,
		unsigned int channels)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_component *cmpnt = dapm->component;
	int i, id, ret = -EINVAL;

	id = asrc_get_lock_id(stream, asrc_get_idx(w));
	if (id >= 0) {
		ret = asrc_exchange_id(cmpnt, stream, asrc_get_idx(w),
				asrc_get_idx_of_id(cmpnt, id, stream));
		if (ret >= 0)
			asrc_set_channels(id, stream, channels);
		return ret;
	}

	for (i = 0; i < asrc_get_num(stream); i++) {
		id = asrc_get_sorted_id(i, stream);

		if (asrc_is_avail_id(w, id, stream, channels)) {
			ret = asrc_exchange_id(cmpnt, stream, asrc_get_idx(w),
					asrc_get_idx_of_id(cmpnt, id, stream));
			if (ret >= 0)
				asrc_set_channels(id, stream, channels);
			break;
		}
	}

	return ret;
}

static void asrc_release_id(struct snd_soc_dapm_widget *w, int stream)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_component *cmpnt = dapm->component;
	int idx = asrc_get_idx(w);
	int id = asrc_get_id(cmpnt, idx, stream);

	if (id < 0 || id > asrc_get_num(stream))
		return;

	if (asrc_get_lock_id(stream, idx) >= 0)
		return;

	asrc_set_channels(id, stream, 0);
	asrc_put_active(w, stream, 0);
}

unsigned int abox_cmpnt_asrc_get_dst_format(struct abox_data *data,
		int stream, int id)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	int width, channels;
	unsigned int val;

	channels = asrc_get_channels(id, stream);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		snd_soc_component_read(cmpnt, ABOX_SPUS_ASRC_CTRL(id), &val);
	else
		snd_soc_component_read(cmpnt, ABOX_SPUM_ASRC_CTRL(id), &val);
	width = (val & ABOX_ASRC_BIT_WIDTH_MASK) >> ABOX_ASRC_BIT_WIDTH_L;

	return (width << 0x3) | (channels - 1);
}

void abox_cmpnt_asrc_release(struct abox_data *data, int stream, int idx)
{
	struct snd_soc_component *cmpnt = data->cmpnt;

	abox_dbg(cmpnt->dev, "%s(%d, %d)\n", __func__, stream, idx);

	asrc_release_id(asrc_get_widget(cmpnt, idx, stream), stream);
}

static int asrc_find_id(struct snd_soc_dapm_widget *w, int stream)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_component *cmpnt = dapm->component;

	return asrc_get_id(cmpnt, asrc_get_idx(w), stream);
}

static int update_bits_async(struct device *dev,
		struct snd_soc_component *cmpnt, const char *name,
		unsigned int reg, unsigned int mask, unsigned int val)
{
	int ret;

	abox_dbg(dev, "%s(%s, %#x, %#x, %#x)\n", __func__, name, reg, mask, val);

	ret = snd_soc_component_update_bits_async(cmpnt, reg, mask, val);
	if (ret < 0)
		abox_err(dev, "%s(%s, %#x, %#x, %#x): %d\n", __func__, name, reg,
				mask, val, ret);

	return ret;
}

static int asrc_update_tick(struct abox_data *data, int stream, int id)
{
	static const int tick_table[][3] = {
		/* aclk, ticknum, tickdiv */
		{600000000, 1, 1},
		{400000000, 3, 2},
		{300000000, 2, 1},
		{200000000, 3, 1},
		{150000000, 4, 1},
		{100000000, 6, 1},
	};
	struct snd_soc_component *cmpnt = data->cmpnt;
	struct device *dev = cmpnt->dev;
	unsigned int reg, mask, val;
	enum asrc_tick itick, otick;
	int idx = asrc_get_idx_of_id(cmpnt, id, stream);
	unsigned long aclk = clk_get_rate(data->clk_bus);
	int ticknum = 1, tickdiv = 1;
	int i, res, ret = 0;

	abox_dbg(dev, "%s(%d, %d, %luHz)\n", __func__, stream, id, aclk);

	if (idx < 0) {
		abox_err(dev, "%s(%d, %d): invalid idx: %d\n", __func__,
				stream, id, idx);
		return -EINVAL;
	}

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		reg = ABOX_SPUS_ASRC_CTRL(id);
		itick = spus_asrc_is[idx];
		otick = spus_asrc_os[idx];
	} else {
		reg = ABOX_SPUM_ASRC_CTRL(id);
		itick = spum_asrc_is[idx];
		otick = spum_asrc_os[idx];
	}

	if ((itick == TICK_SYNC) && (otick == TICK_SYNC))
		return 0;

	res = snd_soc_component_read(cmpnt, reg, &val);
	if (res < 0) {
		abox_err(dev, "%s: read fail(%#x): %d\n", __func__, reg, res);
		ret = res;
	}

	for (i = 0; i < ARRAY_SIZE(tick_table); i++) {
		if (aclk > tick_table[i][0])
			break;

		ticknum = tick_table[i][1];
		tickdiv = tick_table[i][2];
	}

	mask = ABOX_ASRC_TICKNUM_MASK;
	val = ticknum << ABOX_ASRC_TICKNUM_L;
	res = update_bits_async(dev, cmpnt, "ticknum", reg, mask, val);
	if (res < 0)
		ret = res;

	mask = ABOX_ASRC_TICKDIV_MASK;
	val = tickdiv << ABOX_ASRC_TICKDIV_L;
	res = update_bits_async(dev, cmpnt, "tickdiv", reg, mask, val);
	if (res < 0)
		ret = res;

	/* Todo: change it to abox_dbg() */
	abox_info(dev, "asrc tick(%d, %d) aclk=%luHz: %d, %d\n",
			stream, id, aclk, ticknum, tickdiv);

	return ret;
}

int abox_cmpnt_update_asrc_tick(struct device *adev)
{
	struct device *dev = adev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct snd_soc_component *cmpnt = data->cmpnt;
	int stream, id, res, ret = 0;

	if (!cmpnt)
		return 0;

	abox_dbg(dev, "%s\n", __func__);

	for (stream = 0; stream <= SNDRV_PCM_STREAM_LAST; stream++) {
		for (id = 0; id < asrc_get_num(stream); id++) {
			res = asrc_update_tick(data, stream, id);
			if (res < 0)
				ret = res;
		}
	}

	snd_soc_component_async_complete(cmpnt);

	return ret;
}

static int asrc_config_async(struct abox_data *data, int id, int stream,
		enum asrc_tick itick, unsigned int isr,
		enum asrc_tick otick, unsigned int osr,
		unsigned int s_default, unsigned int width, int apf_coef)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	bool spus = (stream == SNDRV_PCM_STREAM_PLAYBACK);
	unsigned int ofactor;
	unsigned int reg, mask, val;
	struct asrc_ctrl ctrl;
	int res, ret = 0;

	abox_dbg(dev, "%s(%d, %d, %d, %uHz, %d, %uHz, %u, %ubit, %d)\n",
			__func__, id, stream, itick, isr,
			otick, osr, s_default, width, apf_coef);

	if ((itick == TICK_SYNC) == (otick == TICK_SYNC)) {
		abox_err(dev, "%s: itick=%d otick=%d\n", __func__, itick, otick);
		return -EINVAL;
	}

	if (s_default == 0) {
		abox_err(dev, "invalid default\n");
		return -EINVAL;
	}

	/* set async side to input side */
	ctrl.isr = (itick != TICK_SYNC) ? isr : osr;
	ctrl.osr = (itick != TICK_SYNC) ? osr : isr;
	ctrl.ovsf = RATIO_8;
	ctrl.ifactor = s_default;
	ctrl.dcmf = RATIO_8;
	ofactor = cal_ofactor(&ctrl);
	while (ofactor > ABOX_ASRC_OS_DEFAULT_MASK) {
		ctrl.ovsf--;
		ofactor = cal_ofactor(&ctrl);
	}

	if (itick == TICK_SYNC) {
		swap(ctrl.isr, ctrl.osr);
		swap(ctrl.ovsf, ctrl.dcmf);
		swap(ctrl.ifactor, ofactor);
	}

	abox_dbg(dev, "asrc(%d, %d): %d, %uHz, %d, %uHz, %d, %d, %u, %u, %u, %u\n",
			stream, id, itick, ctrl.isr, otick, ctrl.osr,
			1 << ctrl.ovsf, 1 << ctrl.dcmf, ctrl.ifactor, ofactor,
			is_limit(ctrl.ifactor), os_limit(ofactor));

	reg = spus ? ABOX_SPUS_ASRC_CTRL(id) : ABOX_SPUM_ASRC_CTRL(id);

	mask = ABOX_ASRC_BIT_WIDTH_MASK;
	val = ((width / 8) - 1) << ABOX_ASRC_BIT_WIDTH_L;
	res = update_bits_async(dev, cmpnt, "width", reg, mask, val);
	if (res < 0)
		ret = res;

	mask = ABOX_ASRC_IS_SYNC_MODE_MASK;
	val = (itick != TICK_SYNC) << ABOX_ASRC_IS_SYNC_MODE_L;
	res = update_bits_async(dev, cmpnt, "is sync", reg, mask, val);
	if (res < 0)
		ret = res;

	mask = ABOX_ASRC_OS_SYNC_MODE_MASK;
	val = (otick != TICK_SYNC) << ABOX_ASRC_OS_SYNC_MODE_L;
	res = update_bits_async(dev, cmpnt, "os sync", reg, mask, val);
	if (res < 0)
		ret = res;

	mask = ABOX_ASRC_OVSF_RATIO_MASK;
	val = ctrl.ovsf << ABOX_ASRC_OVSF_RATIO_L;
	res = update_bits_async(dev, cmpnt, "ovsf ratio", reg, mask, val);
	if (res < 0)
		ret = res;

	mask = ABOX_ASRC_DCMF_RATIO_MASK;
	val = ctrl.dcmf << ABOX_ASRC_DCMF_RATIO_L;
	res = update_bits_async(dev, cmpnt, "dcmf ratio", reg, mask, val);
	if (res < 0)
		ret = res;

	mask = ABOX_ASRC_IS_SOURCE_SEL_MASK;
	val = itick << ABOX_ASRC_IS_SOURCE_SEL_L;
	res = update_bits_async(dev, cmpnt, "is source", reg, mask, val);
	if (res < 0)
		ret = res;

	mask = ABOX_ASRC_OS_SOURCE_SEL_MASK;
	val = otick << ABOX_ASRC_OS_SOURCE_SEL_L;
	res = update_bits_async(dev, cmpnt, "os source", reg, mask, val);
	if (res < 0)
		ret = res;

	reg = spus ? ABOX_SPUS_ASRC_IS_PARA0(id) : ABOX_SPUM_ASRC_IS_PARA0(id);
	mask = ABOX_ASRC_IS_DEFAULT_MASK;
	val = ctrl.ifactor;
	res = update_bits_async(dev, cmpnt, "is default", reg, mask, val);
	if (res < 0)
		ret = res;

	reg = spus ? ABOX_SPUS_ASRC_IS_PARA1(id) : ABOX_SPUM_ASRC_IS_PARA1(id);
	mask = ABOX_ASRC_IS_TPERIOD_LIMIT_MASK;
	val = is_limit(val);
	res = update_bits_async(dev, cmpnt, "is tperiod limit", reg, mask, val);
	if (res < 0)
		ret = res;

	reg = spus ? ABOX_SPUS_ASRC_OS_PARA0(id) : ABOX_SPUM_ASRC_OS_PARA0(id);
	mask = ABOX_ASRC_OS_DEFAULT_MASK;
	val = ofactor;
	res = update_bits_async(dev, cmpnt, "os default", reg, mask, val);
	if (res < 0)
		ret = res;

	reg = spus ? ABOX_SPUS_ASRC_OS_PARA1(id) : ABOX_SPUM_ASRC_OS_PARA1(id);
	mask = ABOX_ASRC_OS_TPERIOD_LIMIT_MASK;
	val = os_limit(val);
	res = update_bits_async(dev, cmpnt, "os tperiod limit", reg, mask, val);
	if (res < 0)
		ret = res;

	reg = spus ? ABOX_SPUS_ASRC_FILTER_CTRL(id) :
			ABOX_SPUM_ASRC_FILTER_CTRL(id);
	mask = ABOX_ASRC_APF_COEF_SEL_MASK;
	val = apf_coef << ABOX_ASRC_APF_COEF_SEL_L;
	res = update_bits_async(dev, cmpnt, "apf coef sel", reg, mask, val);
	if (res < 0)
		ret = res;

	res = asrc_update_tick(data, stream, id);
	if (res < 0)
		ret = res;

	snd_soc_component_async_complete(cmpnt);

	return ret;
}

static int asrc_config_sync(struct abox_data *data, int id, int stream,
		unsigned int isr, unsigned int osr, unsigned int width,
		int apf_coef)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	bool spus = (stream == SNDRV_PCM_STREAM_PLAYBACK);
	const struct asrc_ctrl *ctrl;
	unsigned int reg, mask, val;
	int res, ret = 0;

	abox_dbg(dev, "%s(%d, %d, %uHz, %uHz, %ubit, %d)\n", __func__,
			id, stream, isr, osr, width, apf_coef);

	ctrl = &asrc_table[to_asrc_rate(isr)][to_asrc_rate(osr)];

	reg = spus ? ABOX_SPUS_ASRC_CTRL(id) : ABOX_SPUM_ASRC_CTRL(id);
	mask = ABOX_ASRC_BIT_WIDTH_MASK;
	val = ((width / 8) - 1) << ABOX_ASRC_BIT_WIDTH_L;
	res = update_bits_async(dev, cmpnt, "width", reg, mask, val);
	if (res < 0)
		ret = res;

	mask = ABOX_ASRC_IS_SYNC_MODE_MASK;
	val = 0 << ABOX_ASRC_IS_SYNC_MODE_L;
	res = update_bits_async(dev, cmpnt, "is sync", reg, mask, val);
	if (res < 0)
		ret = res;

	mask = ABOX_ASRC_OS_SYNC_MODE_MASK;
	val = 0 << ABOX_ASRC_OS_SYNC_MODE_L;
	res = update_bits_async(dev, cmpnt, "os sync", reg, mask, val);
	if (res < 0)
		ret = res;

	mask = ABOX_ASRC_OVSF_RATIO_MASK;
	val = ctrl->ovsf << ABOX_ASRC_OVSF_RATIO_L;
	res = update_bits_async(dev, cmpnt, "ovsf ratio", reg, mask, val);
	if (res < 0)
		ret = res;

	mask = ABOX_ASRC_DCMF_RATIO_MASK;
	val = ctrl->dcmf << ABOX_ASRC_DCMF_RATIO_L;
	res = update_bits_async(dev, cmpnt, "dcmf ratio", reg, mask, val);
	if (res < 0)
		ret = res;

	mask = ABOX_ASRC_IS_SOURCE_SEL_MASK;
	val = TICK_SYNC << ABOX_ASRC_IS_SOURCE_SEL_L;
	res = update_bits_async(dev, cmpnt, "is source", reg, mask, val);
	if (res < 0)
		ret = res;

	mask = ABOX_ASRC_OS_SOURCE_SEL_MASK;
	val = TICK_SYNC << ABOX_ASRC_OS_SOURCE_SEL_L;
	res = update_bits_async(dev, cmpnt, "os source", reg, mask, val);
	if (res < 0)
		ret = res;

	reg = spus ? ABOX_SPUS_ASRC_IS_PARA0(id) : ABOX_SPUM_ASRC_IS_PARA0(id);
	mask = ABOX_ASRC_IS_DEFAULT_MASK;
	val = ctrl->ifactor;
	res = update_bits_async(dev, cmpnt, "is default", reg, mask, val);
	if (res < 0)
		ret = res;

	reg = spus ? ABOX_SPUS_ASRC_IS_PARA1(id) : ABOX_SPUM_ASRC_IS_PARA1(id);
	mask = ABOX_ASRC_IS_TPERIOD_LIMIT_MASK;
	val = is_limit(val);
	res = update_bits_async(dev, cmpnt, "is tperiod limit", reg, mask, val);
	if (res < 0)
		ret = res;

	reg = spus ? ABOX_SPUS_ASRC_OS_PARA0(id) : ABOX_SPUM_ASRC_OS_PARA0(id);
	mask = ABOX_ASRC_OS_DEFAULT_MASK;
	val = cal_ofactor(ctrl);
	res = update_bits_async(dev, cmpnt, "os default", reg, mask, val);
	if (res < 0)
		ret = res;

	reg = spus ? ABOX_SPUS_ASRC_OS_PARA1(id) : ABOX_SPUM_ASRC_OS_PARA1(id);
	mask = ABOX_ASRC_OS_TPERIOD_LIMIT_MASK;
	val = os_limit(val);
	res = update_bits_async(dev, cmpnt, "os tperiod limit", reg, mask, val);
	if (res < 0)
		ret = res;

	reg = spus ? ABOX_SPUS_ASRC_FILTER_CTRL(id) :
			ABOX_SPUM_ASRC_FILTER_CTRL(id);
	mask = ABOX_ASRC_APF_COEF_SEL_MASK;
	val = apf_coef << ABOX_ASRC_APF_COEF_SEL_L;
	res = update_bits_async(dev, cmpnt, "apf coef sel", reg, mask, val);
	if (res < 0)
		ret = res;

	snd_soc_component_async_complete(cmpnt);

	return ret;
}

static int asrc_config(struct abox_data *data, int id, int stream,
		enum asrc_tick itick, unsigned int isr,
		enum asrc_tick otick, unsigned int osr,
		unsigned int width, int apf_coef)
{
	struct device *dev = data->dev;
	int ret;

	abox_info(dev, "%s(%d, %d, %d, %uHz, %d, %uHz, %ubit, %d)\n",
			__func__, id, stream, itick, isr, otick, osr,
			width, apf_coef);

	if ((itick == TICK_SYNC) && (otick == TICK_SYNC)) {
		ret = asrc_config_sync(data, id, stream, isr, osr, width,
				apf_coef);
	} else {
		if (itick == TICK_CP) {
			ret = asrc_config_async(data, id, stream,
					itick, isr, otick, osr, s_default,
					width, apf_coef);
		} else if (otick == TICK_CP) {
			ret = asrc_config_async(data, id, stream,
					itick, isr, otick, osr, s_default,
					width, apf_coef);
		} else {
			abox_err(dev, "not supported\n");
			ret = -EINVAL;
		}
	}

	return ret;
}

static int asrc_set(struct abox_data *data, int stream, int idx,
		unsigned int channels, unsigned int rate, unsigned int tgt_rate,
		unsigned int tgt_width)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	struct snd_soc_dapm_widget *w = asrc_get_widget(cmpnt, idx, stream);
	enum asrc_tick itick, otick;
	int apf_coef = get_apf_coef(data, stream, idx);
	bool force_enable;
	int on, ret;

	abox_dbg(dev, "%s(%d, %d, %d, %uHz, %uHz, %ubit)\n", __func__,
			stream, idx, channels, rate, tgt_rate, tgt_width);

	ret = asrc_assign_id(w, stream, channels);
	if (ret < 0)
		abox_err(dev, "%s: assign failed: %d\n", __func__, ret);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		force_enable = spus_asrc_force_enable[idx];
		itick = spus_asrc_is[idx];
		otick = spus_asrc_os[idx];
		ret = asrc_config(data, asrc_find_id(w, stream), stream,
				itick, rate, otick, tgt_rate,
				tgt_width, apf_coef);
	} else {
		force_enable = spum_asrc_force_enable[idx];
		itick = spum_asrc_is[idx];
		otick = spum_asrc_os[idx];
		ret = asrc_config(data, asrc_find_id(w, stream), stream,
				itick, tgt_rate, otick, rate,
				tgt_width, apf_coef);
	}
	if (ret < 0)
		abox_err(dev, "%s: config failed: %d\n", __func__, ret);

	on = force_enable || (rate != tgt_rate) ||
			(itick != TICK_SYNC) || (otick != TICK_SYNC);
	ret = asrc_put_active(w, stream, on);

	return ret;
}

static int asrc_event(struct snd_soc_dapm_widget *w, int e, int stream)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct device *dev = dapm->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	int idx = asrc_get_idx(w);
	struct device *dev_dma;
	struct snd_soc_dai *dai_dma;
	struct snd_pcm_hw_params params, tgt_params;
	unsigned int tgt_rate = 0, tgt_width = 0;
	unsigned int rate = 0, width = 0, channels = 0;
	int id, ret = 0;

	abox_dbg(dev, "%s(%s, %d)\n", __func__, w->name, e);

	switch (e) {
	case SND_SOC_DAPM_PRE_PMU:
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			dev_dma = data->dev_rdma[idx];
		else
			dev_dma = data->dev_wdma[idx];

		ret = abox_dma_hw_params_fixup(dev_dma, NULL, &params);
		if (ret < 0) {
			abox_err(dev_dma, "hw params get failed: %d\n", ret);
			break;
		}

		rate = params_rate(&params);
		width = params_width(&params);
		channels = params_channels(&params);
		if (!rate || !width || !channels) {
			abox_err(dev_dma, "hw params invalid: %u %u %u\n",
					rate, width, channels);
			ret = -EINVAL;
			break;
		}

		dai_dma = abox_dma_get_dai(dev_dma, DMA_DAI_PCM);
		if (IS_ERR_OR_NULL(dai_dma)) {
			abox_err(dev_dma, "dai get failed: %ld\n",
					PTR_ERR(dai_dma));
			break;
		}

		tgt_params = params;
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			ret = rdma_hw_params_fixup(dai_dma, &tgt_params,
					stream);
		else
			ret = wdma_hw_params_fixup(dai_dma, &tgt_params,
					stream);
		if (ret < 0)
			abox_err(dev_dma, "hw params fixup failed: %d\n", ret);

		tgt_rate = params_rate(&tgt_params);
		tgt_width = params_width(&tgt_params);

		ret = asrc_set(data, stream, idx, channels, rate, tgt_rate,
				tgt_width);
		if (ret < 0)
			abox_err(dev, "%s: set failed: %d\n", __func__, ret);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* ASRC will be released in DMA stop. */
		break;
	}

	if (asrc_get_active(data->cmpnt, idx, stream)) {
		id = asrc_get_id(data->cmpnt, idx, stream);
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			notify_event(data, ABOX_WIDGET_SPUS_ASRC0 + id, e);
		else
			notify_event(data, ABOX_WIDGET_SPUM_ASRC0 + id, e);
	}

	return ret;
}

static int spus_asrc_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	return asrc_event(w, e, SNDRV_PCM_STREAM_PLAYBACK);
}

static int spum_asrc_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *k, int e)
{
	return asrc_event(w, e, SNDRV_PCM_STREAM_CAPTURE);
}

static const char * const spus_inx_texts[] = {
	"RDMA", "SIFSM", "RESERVED", "SIFST",
};
static SOC_ENUM_SINGLE_DECL(spus_in0_enum, ABOX_SPUS_CTRL_FC0,
		ABOX_FUNC_CHAIN_SRC_IN_L(0), spus_inx_texts);
static const struct snd_kcontrol_new spus_in0_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in0_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_in1_enum, ABOX_SPUS_CTRL_FC0,
		ABOX_FUNC_CHAIN_SRC_IN_L(1), spus_inx_texts);
static const struct snd_kcontrol_new spus_in1_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in1_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_in2_enum, ABOX_SPUS_CTRL_FC0,
		ABOX_FUNC_CHAIN_SRC_IN_L(2), spus_inx_texts);
static const struct snd_kcontrol_new spus_in2_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in2_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_in3_enum, ABOX_SPUS_CTRL_FC0,
		ABOX_FUNC_CHAIN_SRC_IN_L(3), spus_inx_texts);
static const struct snd_kcontrol_new spus_in3_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in3_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_in4_enum, ABOX_SPUS_CTRL_FC1,
		ABOX_FUNC_CHAIN_SRC_IN_L(4), spus_inx_texts);
static const struct snd_kcontrol_new spus_in4_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in4_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_in5_enum, ABOX_SPUS_CTRL_FC1,
		ABOX_FUNC_CHAIN_SRC_IN_L(5), spus_inx_texts);
static const struct snd_kcontrol_new spus_in5_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in5_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_in6_enum, ABOX_SPUS_CTRL_FC1,
		ABOX_FUNC_CHAIN_SRC_IN_L(6), spus_inx_texts);
static const struct snd_kcontrol_new spus_in6_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in6_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_in7_enum, ABOX_SPUS_CTRL_FC1,
		ABOX_FUNC_CHAIN_SRC_IN_L(7), spus_inx_texts);
static const struct snd_kcontrol_new spus_in7_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in7_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_in8_enum, ABOX_SPUS_CTRL_FC2,
		ABOX_FUNC_CHAIN_SRC_IN_L(8), spus_inx_texts);
static const struct snd_kcontrol_new spus_in8_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in8_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_in9_enum, ABOX_SPUS_CTRL_FC2,
		ABOX_FUNC_CHAIN_SRC_IN_L(9), spus_inx_texts);
static const struct snd_kcontrol_new spus_in9_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in9_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_in10_enum, ABOX_SPUS_CTRL_FC2,
		ABOX_FUNC_CHAIN_SRC_IN_L(10), spus_inx_texts);
static const struct snd_kcontrol_new spus_in10_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in10_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_in11_enum, ABOX_SPUS_CTRL_FC2,
		ABOX_FUNC_CHAIN_SRC_IN_L(11), spus_inx_texts);
static const struct snd_kcontrol_new spus_in11_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in11_enum),
};

static const char * const spus_outx_texts[] = {
	"RESERVED", "SIFS0", "SIFS1", "RESERVED", "SIFS2", "RESERVED",
	"SIFS3", "RESERVED", "SIFS4", "SIDETONE-SIFS0", "SIFS5",
};
static SOC_ENUM_SINGLE_DECL(spus_out0_enum, ABOX_SPUS_CTRL_FC0,
		ABOX_FUNC_CHAIN_SRC_OUT_L(0), spus_outx_texts);
static const struct snd_kcontrol_new spus_out0_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out0_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_out1_enum, ABOX_SPUS_CTRL_FC0,
		ABOX_FUNC_CHAIN_SRC_OUT_L(1), spus_outx_texts);
static const struct snd_kcontrol_new spus_out1_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out1_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_out2_enum, ABOX_SPUS_CTRL_FC0,
		ABOX_FUNC_CHAIN_SRC_OUT_L(2), spus_outx_texts);
static const struct snd_kcontrol_new spus_out2_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out2_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_out3_enum, ABOX_SPUS_CTRL_FC0,
		ABOX_FUNC_CHAIN_SRC_OUT_L(3), spus_outx_texts);
static const struct snd_kcontrol_new spus_out3_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out3_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_out4_enum, ABOX_SPUS_CTRL_FC1,
		ABOX_FUNC_CHAIN_SRC_OUT_L(4), spus_outx_texts);
static const struct snd_kcontrol_new spus_out4_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out4_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_out5_enum, ABOX_SPUS_CTRL_FC1,
		ABOX_FUNC_CHAIN_SRC_OUT_L(5), spus_outx_texts);
static const struct snd_kcontrol_new spus_out5_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out5_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_out6_enum, ABOX_SPUS_CTRL_FC1,
		ABOX_FUNC_CHAIN_SRC_OUT_L(6), spus_outx_texts);
static const struct snd_kcontrol_new spus_out6_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out6_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_out7_enum, ABOX_SPUS_CTRL_FC1,
		ABOX_FUNC_CHAIN_SRC_OUT_L(7), spus_outx_texts);
static const struct snd_kcontrol_new spus_out7_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out7_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_out8_enum, ABOX_SPUS_CTRL_FC2,
		ABOX_FUNC_CHAIN_SRC_OUT_L(8), spus_outx_texts);
static const struct snd_kcontrol_new spus_out8_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out8_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_out9_enum, ABOX_SPUS_CTRL_FC2,
		ABOX_FUNC_CHAIN_SRC_OUT_L(9), spus_outx_texts);
static const struct snd_kcontrol_new spus_out9_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out9_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_out10_enum, ABOX_SPUS_CTRL_FC2,
		ABOX_FUNC_CHAIN_SRC_OUT_L(10), spus_outx_texts);
static const struct snd_kcontrol_new spus_out10_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out10_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_out11_enum, ABOX_SPUS_CTRL_FC2,
		ABOX_FUNC_CHAIN_SRC_OUT_L(11), spus_outx_texts);
static const struct snd_kcontrol_new spus_out11_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out11_enum),
};

static const char * const spusm_texts[] = {
	"RESERVED", "RESERVED", "RESERVED", "RESERVED",
	"RESERVED", "RESERVED", "RESERVED", "RESERVED",
	"UAIF0", "UAIF1", "UAIF2", "UAIF3",
	"UAIF4", "UAIF5", "UAIF6", "RESERVED",
	"BI_PDI0", "BI_PDI1", "BI_PDI2", "BI_PDI3",
	"BI_PDI4", "BI_PDI5", "BI_PDI6", "BI_PDI7",
	"RX_PDI0", "RX_PDI1", "RESERVED", "RESERVED",
	"RESERVED", "RESERVED", "RESERVED", "SPDY",
};
static SOC_ENUM_SINGLE_DECL(spusm_enum, ABOX_ROUTE_CTRL0, ABOX_ROUTE_SPUSM_L,
		spusm_texts);
static const struct snd_kcontrol_new spusm_controls[] = {
	SOC_DAPM_ENUM("MUX", spusm_enum),
};

static SOC_ENUM_SINGLE_DECL(spust_enum, ABOX_ROUTE_CTRL2, ABOX_ROUTE_SPUST_L,
		spusm_texts);
static const struct snd_kcontrol_new spust_controls[] = {
	SOC_DAPM_ENUM("MUX", spust_enum),
};

int abox_cmpnt_sifsm_prepare(struct device *dev, struct abox_data *data,
		enum abox_dai dai)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	enum abox_dai src;
	unsigned int reg_val, val;
	int idx, ret = 0;

	abox_dbg(dev, "%s\n", __func__);

	switch (get_source_dai_id(data, dai)) {
	case ABOX_SIFSM:
		/* ToDo */
		break;
	case ABOX_SIFST:
		/* Needs force update. Flush is write only field. */
		ret = regmap_update_bits_base(cmpnt->regmap, ABOX_SIDETONE_CTRL,
			    ABOX_SDTN_FLUSH_MASK, ABOX_SDTN_FLUSH_MASK,
			    NULL, false, true);
		if (ret < 0)
			break;

		src = get_source_dai_id(data, ABOX_SIFST);
		switch (src) {
		case ABOX_UAIF0 ... ABOX_UAIF6:
			idx = src - ABOX_UAIF0;
			ret = snd_soc_component_read(cmpnt,
					ABOX_UAIF_CTRL1(idx), &reg_val);
			if (ret < 0)
				break;

			val = (reg_val & ABOX_FORMAT_MASK) >> ABOX_FORMAT_L;
			ret = snd_soc_component_update_bits(cmpnt,
					ABOX_SIDETONE_CTRL,
					ABOX_SDTN_FORMAT_MASK,
					val << ABOX_SDTN_FORMAT_L);
			break;
		case ABOX_BI_PDI0 ... ABOX_BI_PDI3:
			idx = src - ABOX_BI_PDI0;
			ret = snd_soc_component_read(cmpnt, ABOX_SW_PDI_CTRL0,
					&reg_val);
			if (ret < 0)
				break;

			val = (reg_val & ABOX_BI_PDI_FORMAT_MASK(idx)) >>
					ABOX_BI_PDI_FORMAT_L(idx);
			ret = snd_soc_component_update_bits(cmpnt,
					ABOX_SIDETONE_CTRL,
					ABOX_SDTN_FORMAT_MASK,
					val << ABOX_SDTN_FORMAT_L);
			break;
		case ABOX_BI_PDI4 ... ABOX_BI_PDI7:
			idx = src - ABOX_BI_PDI4;
			ret = snd_soc_component_read(cmpnt, ABOX_SW_PDI_CTRL1,
					&reg_val);
			if (ret < 0)
				break;

			val = (reg_val & ABOX_BI_PDI_FORMAT_MASK(idx)) >>
					ABOX_BI_PDI_FORMAT_L(idx);
			ret = snd_soc_component_update_bits(cmpnt,
					ABOX_SIDETONE_CTRL,
					ABOX_SDTN_FORMAT_MASK,
					val << ABOX_SDTN_FORMAT_L);
			break;
		case ABOX_TX_PDI0 ... ABOX_TX_PDI2:
			idx = src - ABOX_TX_PDI0;
			ret = snd_soc_component_read(cmpnt, ABOX_SW_PDI_CTRL2,
					&reg_val);
			if (ret < 0)
				break;

			val = (reg_val & ABOX_TX_PDI_FORMAT_MASK(idx)) >>
					ABOX_TX_PDI_FORMAT_L(idx);
			ret = snd_soc_component_update_bits(cmpnt,
					ABOX_SIDETONE_CTRL,
					ABOX_SDTN_FORMAT_MASK,
					val << ABOX_SDTN_FORMAT_L);
			break;
		case ABOX_RX_PDI0 ... ABOX_RX_PDI1:
			idx = src - ABOX_RX_PDI0;
			ret = snd_soc_component_read(cmpnt, ABOX_SW_PDI_CTRL3,
					&reg_val);
			if (ret < 0)
				break;

			val = (reg_val & ABOX_RX_PDI_FORMAT_MASK(idx)) >>
					ABOX_RX_PDI_FORMAT_L(idx);
			ret = snd_soc_component_update_bits(cmpnt,
					ABOX_SIDETONE_CTRL,
					ABOX_SDTN_FORMAT_MASK,
					val << ABOX_SDTN_FORMAT_L);
			break;
		default:
			ret = -EINVAL;
			break;
		}
		break;
	default:
		/* nothing to do */
		break;
	}

	return ret;
}

static const char * const sidetone_mode_texts[] = {
	"Normal", "Zero", "Bypass",
};
static SOC_ENUM_SINGLE_DECL(sidetone_mode, ABOX_SIDETONE_CTRL,
		ABOX_SDTN_ZERO_OUTPUT_L, sidetone_mode_texts);

static DECLARE_TLV_DB_LINEAR(sidetone_gain_tlv, 0, 3600);

static const char * const sidetone_headroom_hpf_texts[] = {
	"6dB", "12dB", "18dB", "24dB",
};
static const unsigned int sidetone_headroom_hpf_values[] = {
	2, 3, 4, 5,
};
static SOC_VALUE_ENUM_SINGLE_DECL(sidetone_headroom_hpf,
		ABOX_SIDETONE_FILTER_CTRL0, ABOX_SDTN_HEADROOM_HPF_L,
		ABOX_SDTN_HEADROOM_HPF_MASK, sidetone_headroom_hpf_texts,
		sidetone_headroom_hpf_values);

static const char * const sidetone_headroom_eq_texts[] = {
	"12dB", "18dB", "24dB",
};
static const unsigned int sidetone_headroom_eq_values[] = {
	3, 4, 5,
};
static SOC_VALUE_ENUM_SINGLE_DECL(sidetone_headroom_peak0,
		ABOX_SIDETONE_FILTER_CTRL0, ABOX_SDTN_HEADROOM_PEAK0_L,
		ABOX_SDTN_HEADROOM_PEAK0_MASK, sidetone_headroom_eq_texts,
		sidetone_headroom_eq_values);
static SOC_VALUE_ENUM_SINGLE_DECL(sidetone_headroom_peak1,
		ABOX_SIDETONE_FILTER_CTRL0, ABOX_SDTN_HEADROOM_PEAK1_L,
		ABOX_SDTN_HEADROOM_PEAK0_MASK, sidetone_headroom_eq_texts,
		sidetone_headroom_eq_values);
static SOC_VALUE_ENUM_SINGLE_DECL(sidetone_headroom_peak2,
		ABOX_SIDETONE_FILTER_CTRL0, ABOX_SDTN_HEADROOM_PEAK2_L,
		ABOX_SDTN_HEADROOM_PEAK0_MASK, sidetone_headroom_eq_texts,
		sidetone_headroom_eq_values);
static SOC_VALUE_ENUM_SINGLE_DECL(sidetone_headroom_lowsh,
		ABOX_SIDETONE_FILTER_CTRL0, ABOX_SDTN_HEADROOM_LOWSH_L,
		ABOX_SDTN_HEADROOM_PEAK0_MASK, sidetone_headroom_eq_texts,
		sidetone_headroom_eq_values);
static SOC_VALUE_ENUM_SINGLE_DECL(sidetone_headroom_highsh,
		ABOX_SIDETONE_FILTER_CTRL0, ABOX_SDTN_HEADROOM_HIGHSH_L,
		ABOX_SDTN_HEADROOM_PEAK0_MASK, sidetone_headroom_eq_texts,
		sidetone_headroom_eq_values);

static struct snd_kcontrol_new sidetone_controls[] = {
	SOC_SINGLE("SIDETONE IN CH", ABOX_SIDETONE_CTRL,
			ABOX_SDTN_CH_SEL_IN_L, 1, 0),
	SOC_SINGLE("SIDETONE OUT CH", ABOX_SIDETONE_CTRL,
			ABOX_SDTN_CH_SEL_OUT_L, 1, 0),
	SOC_SINGLE("SIDETONE OUT2 CH", ABOX_SIDETONE_CTRL,
			ABOX_SDTN_CH_SEL_OUT2_L, 1, 0),
	SOC_SINGLE("SIDETONE OUT2 EN", ABOX_SIDETONE_CTRL,
			ABOX_SDTN_OUT2_ENABLE_L, 1, 0),
	SOC_ENUM("SIDETONE MODE", sidetone_mode),
	SOC_SINGLE("SIDETONE HPF EN", ABOX_SIDETONE_CTRL,
			ABOX_SDTN_HPF_ENABLE_L, 1, 0),
	SOC_SINGLE("SIDETONE EQ EN", ABOX_SIDETONE_CTRL,
			ABOX_SDTN_EQ_ENABLE_L, 1, 0),
	SOC_SINGLE("SIDETONE GAIN IN EN", ABOX_SIDETONE_CTRL,
			ABOX_SDTN_GAIN_IN_ENABLE_L, 1, 0),
	SOC_SINGLE("SIDETONE GAIN OUT EN", ABOX_SIDETONE_CTRL,
			ABOX_SDTN_GAIN_OUT_ENABLE_L, 1, 0),
	SOC_SINGLE_TLV("SIDETONE GAIN IN", ABOX_SIDETONE_GAIN_CTRL,
			ABOX_SDTN_GAIN_IN_L, 133, 0, sidetone_gain_tlv),
	SOC_SINGLE_TLV("SIDETONE GAIN OUT", ABOX_SIDETONE_GAIN_CTRL,
			ABOX_SDTN_GAIN_OUT_L, 133, 0, sidetone_gain_tlv),
	SOC_ENUM("SIDETONE HEADROOM HPF", sidetone_headroom_hpf),
	SOC_ENUM("SIDETONE HEADROOM PEAK0", sidetone_headroom_peak0),
	SOC_ENUM("SIDETONE HEADROOM PEAK1", sidetone_headroom_peak1),
	SOC_ENUM("SIDETONE HEADROOM PEAK2", sidetone_headroom_peak2),
	SOC_ENUM("SIDETONE HEADROOM LOWSH", sidetone_headroom_lowsh),
	SOC_ENUM("SIDETONE HEADROOM HIGHSH", sidetone_headroom_highsh),
	SOC_SINGLE("SIDETONE POSTAMP HPF", ABOX_SIDETONE_FILTER_CTRL1,
			ABOX_SDTN_POSTAMP_HPF_L, 3, 0),
	SOC_SINGLE("SIDETONE POSTAMP PEAK0", ABOX_SIDETONE_FILTER_CTRL1,
			ABOX_SDTN_POSTAMP_PEAK0_L, 3, 0),
	SOC_SINGLE("SIDETONE POSTAMP PEAK1", ABOX_SIDETONE_FILTER_CTRL1,
			ABOX_SDTN_POSTAMP_PEAK1_L, 3, 0),
	SOC_SINGLE("SIDETONE POSTAMP PEAK2", ABOX_SIDETONE_FILTER_CTRL1,
			ABOX_SDTN_POSTAMP_PEAK2_L, 3, 0),
	SOC_SINGLE("SIDETONE POSTAMP LOWSH", ABOX_SIDETONE_FILTER_CTRL1,
			ABOX_SDTN_POSTAMP_LOWSH_L, 3, 0),
	SOC_SINGLE("SIDETONE POSTAMP HIGHSH", ABOX_SIDETONE_FILTER_CTRL1,
			ABOX_SDTN_POSTAMP_HIGHSH_L, 3, 0),
	SOC_SINGLE_XR_SX("SIDETONE HPF COEF0", ABOX_SIDETONE_HPF_COEF0,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE HPF COEF1", ABOX_SIDETONE_HPF_COEF1,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE HPF COEF2", ABOX_SIDETONE_HPF_COEF2,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE HPF COEF3", ABOX_SIDETONE_HPF_COEF3,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE HPF COEF4", ABOX_SIDETONE_HPF_COEF4,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE PEAK0 COEF0", ABOX_SIDETONE_PEAK0_COEF0,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE PEAK0 COEF1", ABOX_SIDETONE_PEAK0_COEF1,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE PEAK0 COEF2", ABOX_SIDETONE_PEAK0_COEF2,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE PEAK0 COEF3", ABOX_SIDETONE_PEAK0_COEF3,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE PEAK0 COEF4", ABOX_SIDETONE_PEAK0_COEF4,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE PEAK1 COEF0", ABOX_SIDETONE_PEAK1_COEF0,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE PEAK1 COEF1", ABOX_SIDETONE_PEAK1_COEF1,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE PEAK1 COEF2", ABOX_SIDETONE_PEAK1_COEF2,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE PEAK1 COEF3", ABOX_SIDETONE_PEAK1_COEF3,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE PEAK1 COEF4", ABOX_SIDETONE_PEAK1_COEF4,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE PEAK2 COEF0", ABOX_SIDETONE_PEAK2_COEF0,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE PEAK2 COEF1", ABOX_SIDETONE_PEAK2_COEF1,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE PEAK2 COEF2", ABOX_SIDETONE_PEAK2_COEF2,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE PEAK2 COEF3", ABOX_SIDETONE_PEAK2_COEF3,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE PEAK2 COEF4", ABOX_SIDETONE_PEAK2_COEF4,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE LOWSH COEF0", ABOX_SIDETONE_LOWSH_COEF0,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE LOWSH COEF1", ABOX_SIDETONE_LOWSH_COEF1,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE LOWSH COEF2", ABOX_SIDETONE_LOWSH_COEF2,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE LOWSH COEF3", ABOX_SIDETONE_LOWSH_COEF3,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE LOWSH COEF4", ABOX_SIDETONE_LOWSH_COEF4,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE HIGHSH COEF0", ABOX_SIDETONE_HIGHSH_COEF0,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE HIGHSH COEF1", ABOX_SIDETONE_HIGHSH_COEF1,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE HIGHSH COEF2", ABOX_SIDETONE_HIGHSH_COEF2,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE HIGHSH COEF3", ABOX_SIDETONE_HIGHSH_COEF3,
			1, 32, INT_MIN, INT_MAX, 0),
	SOC_SINGLE_XR_SX("SIDETONE HIGHSH COEF4", ABOX_SIDETONE_HIGHSH_COEF4,
			1, 32, INT_MIN, INT_MAX, 0),
};

static const struct snd_soc_dapm_route sidetone_routes[] = {
	{"SIDETONE", NULL, "SPUST"},
	{"SIFST", NULL, "SIDETONE"},
};

static const char * const sifsx_texts[] = {
	"SPUS OUT0", "SPUS OUT1", "SPUS OUT2", "SPUS OUT3",
	"SPUS OUT4", "SPUS OUT5", "SPUS OUT6", "SPUS OUT7",
	"SPUS OUT8", "SPUS OUT9", "SPUS OUT10", "SPUS OUT11",
	"RESERVED",
};
static SOC_ENUM_SINGLE_DECL(sifs1_enum, ABOX_SPUS_CTRL1, ABOX_SIFS_OUT_SEL_L(1),
		sifsx_texts);
static const struct snd_kcontrol_new sifs1_controls[] = {
	SOC_DAPM_ENUM("MUX", sifs1_enum),
};
static SOC_ENUM_SINGLE_DECL(sifs2_enum, ABOX_SPUS_CTRL1, ABOX_SIFS_OUT_SEL_L(2),
		sifsx_texts);
static const struct snd_kcontrol_new sifs2_controls[] = {
	SOC_DAPM_ENUM("MUX", sifs2_enum),
};
static SOC_ENUM_SINGLE_DECL(sifs3_enum, ABOX_SPUS_CTRL1, ABOX_SIFS_OUT_SEL_L(3),
		sifsx_texts);
static const struct snd_kcontrol_new sifs3_controls[] = {
	SOC_DAPM_ENUM("MUX", sifs3_enum),
};
static SOC_ENUM_SINGLE_DECL(sifs4_enum, ABOX_SPUS_CTRL1, ABOX_SIFS_OUT_SEL_L(4),
		sifsx_texts);
static const struct snd_kcontrol_new sifs4_controls[] = {
	SOC_DAPM_ENUM("MUX", sifs4_enum),
};
static SOC_ENUM_SINGLE_DECL(sifs5_enum, ABOX_SPUS_CTRL1, ABOX_SIFS_OUT_SEL_L(5),
		sifsx_texts);
static const struct snd_kcontrol_new sifs5_controls[] = {
	SOC_DAPM_ENUM("MUX", sifs5_enum),
};

static const struct snd_kcontrol_new sifs0_out_controls[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 1),
};
static const struct snd_kcontrol_new sifs1_out_controls[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 1),
};
static const struct snd_kcontrol_new sifs2_out_controls[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 1),
};
static const struct snd_kcontrol_new sifs3_out_controls[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 1),
};
static const struct snd_kcontrol_new sifs4_out_controls[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 1),
};
static const struct snd_kcontrol_new sifs5_out_controls[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 1),
};

static const char * const sifsm_texts[] = {
	"SPUS IN0", "SPUS IN1", "SPUS IN2", "SPUS IN3",
	"SPUS IN4", "SPUS IN5", "SPUS IN6", "SPUS IN7",
	"SPUS IN8", "SPUS IN9", "SPUS IN10", "SPUS IN11",
	"RESERVED",
};
static SOC_ENUM_SINGLE_DECL(sifsm_enum, ABOX_SPUS_CTRL1, ABOX_SIFSM_IN_SEL_L,
		sifsm_texts);
static const struct snd_kcontrol_new sifsm_controls[] = {
	SOC_DAPM_ENUM("DEMUX", sifsm_enum),
};

static SOC_ENUM_SINGLE_DECL(sifst_enum, ABOX_SPUS_CTRL2, ABOX_SIFST_IN_SEL_L,
		sifsm_texts);
static const struct snd_kcontrol_new sifst_controls[] = {
	SOC_DAPM_ENUM("DEMUX", sifst_enum),
};

static const char * const uaif_spkx_texts[] = {
	"RESERVED", "SIFS0", "SIFS1", "SIFS2",
	"SIFS3", "SIFS4", "SIFS5", "RESERVED",
	"RESERVED", "RESERVED", "RESERVED", "RESERVED",
	"SIFMS",
};
static SOC_ENUM_SINGLE_DECL(uaif0_spk_enum, ABOX_ROUTE_CTRL0,
		ABOX_ROUTE_UAIF_SPK_L(0), uaif_spkx_texts);
static const struct snd_kcontrol_new uaif0_spk_controls[] = {
	SOC_DAPM_ENUM("MUX", uaif0_spk_enum),
};
static SOC_ENUM_SINGLE_DECL(uaif1_spk_enum, ABOX_ROUTE_CTRL0,
		ABOX_ROUTE_UAIF_SPK_L(1), uaif_spkx_texts);
static const struct snd_kcontrol_new uaif1_spk_controls[] = {
	SOC_DAPM_ENUM("MUX", uaif1_spk_enum),
};
static SOC_ENUM_SINGLE_DECL(uaif2_spk_enum, ABOX_ROUTE_CTRL0,
		ABOX_ROUTE_UAIF_SPK_L(2), uaif_spkx_texts);
static const struct snd_kcontrol_new uaif2_spk_controls[] = {
	SOC_DAPM_ENUM("MUX", uaif2_spk_enum),
};
static SOC_ENUM_SINGLE_DECL(uaif3_spk_enum, ABOX_ROUTE_CTRL0,
		ABOX_ROUTE_UAIF_SPK_L(3), uaif_spkx_texts);
static const struct snd_kcontrol_new uaif3_spk_controls[] = {
	SOC_DAPM_ENUM("MUX", uaif3_spk_enum),
};
static SOC_ENUM_SINGLE_DECL(uaif4_spk_enum, ABOX_ROUTE_CTRL0,
		ABOX_ROUTE_UAIF_SPK_L(4), uaif_spkx_texts);
static const struct snd_kcontrol_new uaif4_spk_controls[] = {
	SOC_DAPM_ENUM("MUX", uaif4_spk_enum),
};
static SOC_ENUM_SINGLE_DECL(uaif5_spk_enum, ABOX_ROUTE_CTRL0,
		ABOX_ROUTE_UAIF_SPK_L(5), uaif_spkx_texts);
static const struct snd_kcontrol_new uaif5_spk_controls[] = {
	SOC_DAPM_ENUM("MUX", uaif5_spk_enum),
};
static SOC_ENUM_SINGLE_DECL(uaif6_spk_enum, ABOX_ROUTE_CTRL0,
		ABOX_ROUTE_UAIF_SPK_L(6), uaif_spkx_texts);
static const struct snd_kcontrol_new uaif6_spk_controls[] = {
	SOC_DAPM_ENUM("MUX", uaif6_spk_enum),
};

static const char * const dsif_spk_texts[] = {
	"RESERVED", "RESERVED", "SIFS1", "SIFS2", "SIFS3", "SIFS4", "SIFS5",
};
static SOC_ENUM_SINGLE_DECL(dsif_spk_enum, ABOX_ROUTE_CTRL0, ABOX_ROUTE_DSIF_L,
		dsif_spk_texts);
static const struct snd_kcontrol_new dsif_spk_controls[] = {
	SOC_DAPM_ENUM("MUX", dsif_spk_enum),
};

static const struct snd_kcontrol_new uaif0_controls[] = {
	SOC_DAPM_SINGLE("UAIF0 Switch", SND_SOC_NOPM, 0, 1, 1),
};
static const struct snd_kcontrol_new uaif1_controls[] = {
	SOC_DAPM_SINGLE("UAIF1 Switch", SND_SOC_NOPM, 0, 1, 1),
};
static const struct snd_kcontrol_new uaif2_controls[] = {
	SOC_DAPM_SINGLE("UAIF2 Switch", SND_SOC_NOPM, 0, 1, 1),
};
static const struct snd_kcontrol_new uaif3_controls[] = {
	SOC_DAPM_SINGLE("UAIF3 Switch", SND_SOC_NOPM, 0, 1, 1),
};
static const struct snd_kcontrol_new uaif4_controls[] = {
	SOC_DAPM_SINGLE("UAIF4 Switch", SND_SOC_NOPM, 0, 1, 1),
};
static const struct snd_kcontrol_new uaif5_controls[] = {
	SOC_DAPM_SINGLE("UAIF5 Switch", SND_SOC_NOPM, 0, 1, 1),
};
static const struct snd_kcontrol_new uaif6_controls[] = {
	SOC_DAPM_SINGLE("UAIF6 Switch", SND_SOC_NOPM, 0, 1, 1),
};
static const struct snd_kcontrol_new dsif_controls[] = {
	SOC_DAPM_SINGLE("DSIF Switch", SND_SOC_NOPM, 0, 1, 1),
};
static const struct snd_kcontrol_new spdy_controls[] = {
	SOC_DAPM_SINGLE("SPDY Switch", SND_SOC_NOPM, 0, 1, 1),
};

static const char * const rsrcx_texts[] = {
	"RESERVED", "SIFS0", "SIFS1", "SIFS2",
	"SIFS3", "SIFS4", "RESERVED", "RESERVED",
	"UAIF0", "UAIF1", "UAIF2", "UAIF3",
};
static SOC_ENUM_SINGLE_DECL(rsrc0_enum, ABOX_ROUTE_CTRL2, ABOX_ROUTE_RSRC_L(0),
		rsrcx_texts);
static const struct snd_kcontrol_new rsrc0_controls[] = {
	SOC_DAPM_ENUM("DEMUX", rsrc0_enum),
};
static SOC_ENUM_SINGLE_DECL(rsrc1_enum, ABOX_ROUTE_CTRL2, ABOX_ROUTE_RSRC_L(1),
		rsrcx_texts);
static const struct snd_kcontrol_new rsrc1_controls[] = {
	SOC_DAPM_ENUM("DEMUX", rsrc1_enum),
};

static const char * const nsrcx_texts[] = {
	"RESERVED", "SIFS0", "SIFS1", "SIFS2",
	"SIFS3", "SIFS4", "SIFS5", "RESERVED",
	"UAIF0", "UAIF1", "UAIF2", "UAIF3",
	"UAIF4", "UAIF5", "UAIF6", "RESERVED",
	"BI_PDI0", "BI_PDI1", "BI_PDI2", "BI_PDI3",
	"BI_PDI4", "BI_PDI5", "BI_PDI6", "BI_PDI7",
	"RX_PDI0", "RX_PDI1", "RESERVED", "RESERVED",
	"RESERVED", "RESERVED", "RESERVED", "SPDY",
};
static SOC_ENUM_SINGLE_DECL(nsrc0_enum, ABOX_ROUTE_CTRL1, ABOX_ROUTE_NSRC_L(0),
		nsrcx_texts);
static const struct snd_kcontrol_new nsrc0_controls[] = {
	SOC_DAPM_ENUM("DEMUX", nsrc0_enum),
};
static SOC_ENUM_SINGLE_DECL(nsrc1_enum, ABOX_ROUTE_CTRL1, ABOX_ROUTE_NSRC_L(1),
		nsrcx_texts);
static const struct snd_kcontrol_new nsrc1_controls[] = {
	SOC_DAPM_ENUM("DEMUX", nsrc1_enum),
};
static SOC_ENUM_SINGLE_DECL(nsrc2_enum, ABOX_ROUTE_CTRL1, ABOX_ROUTE_NSRC_L(2),
		nsrcx_texts);
static const struct snd_kcontrol_new nsrc2_controls[] = {
	SOC_DAPM_ENUM("DEMUX", nsrc2_enum),
};
static SOC_ENUM_SINGLE_DECL(nsrc3_enum, ABOX_ROUTE_CTRL1, ABOX_ROUTE_NSRC_L(3),
		nsrcx_texts);
static const struct snd_kcontrol_new nsrc3_controls[] = {
	SOC_DAPM_ENUM("DEMUX", nsrc3_enum),
};
static SOC_ENUM_SINGLE_DECL(nsrc4_enum, ABOX_ROUTE_CTRL1, ABOX_ROUTE_NSRC_L(4),
		nsrcx_texts);
static const struct snd_kcontrol_new nsrc4_controls[] = {
	SOC_DAPM_ENUM("DEMUX", nsrc4_enum),
};
static SOC_ENUM_SINGLE_DECL(nsrc5_enum, ABOX_ROUTE_CTRL1, ABOX_ROUTE_NSRC_L(5),
		nsrcx_texts);
static const struct snd_kcontrol_new nsrc5_controls[] = {
	SOC_DAPM_ENUM("DEMUX", nsrc5_enum),
};
static SOC_ENUM_SINGLE_DECL(nsrc6_enum, ABOX_ROUTE_CTRL1, ABOX_ROUTE_NSRC_L(6),
		nsrcx_texts);
static const struct snd_kcontrol_new nsrc6_controls[] = {
	SOC_DAPM_ENUM("DEMUX", nsrc6_enum),
};

static const struct snd_kcontrol_new recp_controls[] = {
	SOC_DAPM_SINGLE("PIFS0", ABOX_SPUM_CTRL1, ABOX_RECP_SRC_VALID_L, 1, 0),
	SOC_DAPM_SINGLE("PIFS1", ABOX_SPUM_CTRL1, ABOX_RECP_SRC_VALID_H, 1, 0),
};

static const char * const sifmx_texts[] = {
	"WDMA", "SIFMS",
};
static SOC_ENUM_SINGLE_DECL(sifm0_enum, ABOX_SPUM_CTRL0,
		ABOX_FUNC_CHAIN_NSRC_OUT_L(0), sifmx_texts);
static const struct snd_kcontrol_new sifm0_controls[] = {
	SOC_DAPM_ENUM("DEMUX", sifm0_enum),
};
static SOC_ENUM_SINGLE_DECL(sifm1_enum, ABOX_SPUM_CTRL0,
		ABOX_FUNC_CHAIN_NSRC_OUT_L(1), sifmx_texts);
static const struct snd_kcontrol_new sifm1_controls[] = {
	SOC_DAPM_ENUM("DEMUX", sifm1_enum),
};
static SOC_ENUM_SINGLE_DECL(sifm2_enum, ABOX_SPUM_CTRL0,
		ABOX_FUNC_CHAIN_NSRC_OUT_L(2), sifmx_texts);
static const struct snd_kcontrol_new sifm2_controls[] = {
	SOC_DAPM_ENUM("DEMUX", sifm2_enum),
};
static SOC_ENUM_SINGLE_DECL(sifm3_enum, ABOX_SPUM_CTRL0,
		ABOX_FUNC_CHAIN_NSRC_OUT_L(3), sifmx_texts);
static const struct snd_kcontrol_new sifm3_controls[] = {
	SOC_DAPM_ENUM("DEMUX", sifm3_enum),
};
static SOC_ENUM_SINGLE_DECL(sifm4_enum, ABOX_SPUM_CTRL0,
		ABOX_FUNC_CHAIN_NSRC_OUT_L(4), sifmx_texts);
static const struct snd_kcontrol_new sifm4_controls[] = {
	SOC_DAPM_ENUM("DEMUX", sifm4_enum),
};
static SOC_ENUM_SINGLE_DECL(sifm5_enum, ABOX_SPUM_CTRL0,
		ABOX_FUNC_CHAIN_NSRC_OUT_L(5), sifmx_texts);
static const struct snd_kcontrol_new sifm5_controls[] = {
	SOC_DAPM_ENUM("DEMUX", sifm5_enum),
};
static SOC_ENUM_SINGLE_DECL(sifm6_enum, ABOX_SPUM_CTRL0,
		ABOX_FUNC_CHAIN_NSRC_OUT_L(6), sifmx_texts);
static const struct snd_kcontrol_new sifm6_controls[] = {
	SOC_DAPM_ENUM("DEMUX", sifm6_enum),
};

static const char * const sifms_texts[] = {
	"RESERVED", "SIFM0", "SIFM1", "SIFM2",
	"SIFM3", "SIFM4", "SIFM5", "SIFM6",
};
static SOC_ENUM_SINGLE_DECL(sifms_enum, ABOX_SPUM_CTRL1, ABOX_SIFMS_OUT_SEL_L,
		sifms_texts);
static const struct snd_kcontrol_new sifms_controls[] = {
	SOC_DAPM_ENUM("MUX", sifms_enum),
};

static const struct snd_soc_dapm_widget cmpnt_widgets[] = {
	SND_SOC_DAPM_MUX("SPUSM", SND_SOC_NOPM, 0, 0, spusm_controls),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN6", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN7", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN8", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN9", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN10", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN11", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_DEMUX("SIFSM", SND_SOC_NOPM, 0, 0, sifsm_controls),

	SND_SOC_DAPM_MUX("SPUST", SND_SOC_NOPM, 0, 0, spust_controls),
	SND_SOC_DAPM_PGA("SIDETONE", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFST-SPUS IN0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFST-SPUS IN1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFST-SPUS IN2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFST-SPUS IN3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFST-SPUS IN4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFST-SPUS IN5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFST-SPUS IN6", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFST-SPUS IN7", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFST-SPUS IN8", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFST-SPUS IN9", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFST-SPUS IN10", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFST-SPUS IN11", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_DEMUX("SIFST", SND_SOC_NOPM, 0, 0, sifst_controls),

	SND_SOC_DAPM_MUX_E("SPUS IN0", SND_SOC_NOPM, 0, 0, spus_in0_controls,
			spus_in0_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN1", SND_SOC_NOPM, 0, 0, spus_in1_controls,
			spus_in1_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN2", SND_SOC_NOPM, 0, 0, spus_in2_controls,
			spus_in2_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN3", SND_SOC_NOPM, 0, 0, spus_in3_controls,
			spus_in3_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN4", SND_SOC_NOPM, 0, 0, spus_in4_controls,
			spus_in4_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN5", SND_SOC_NOPM, 0, 0, spus_in5_controls,
			spus_in5_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN6", SND_SOC_NOPM, 0, 0, spus_in6_controls,
			spus_in6_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN7", SND_SOC_NOPM, 0, 0, spus_in7_controls,
			spus_in7_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN8", SND_SOC_NOPM, 0, 0, spus_in8_controls,
			spus_in8_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN9", SND_SOC_NOPM, 0, 0, spus_in9_controls,
			spus_in9_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN10", SND_SOC_NOPM, 0, 0, spus_in10_controls,
			spus_in10_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SPUS IN11", SND_SOC_NOPM, 0, 0, spus_in11_controls,
			spus_in11_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC0", SND_SOC_NOPM, 0, 0, NULL, 0,
			spus_asrc_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC1", SND_SOC_NOPM, 1, 0, NULL, 0,
			spus_asrc_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC2", SND_SOC_NOPM, 2, 0, NULL, 0,
			spus_asrc_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC3", SND_SOC_NOPM, 3, 0, NULL, 0,
			spus_asrc_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC4", SND_SOC_NOPM, 4, 0, NULL, 0,
			spus_asrc_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC5", SND_SOC_NOPM, 5, 0, NULL, 0,
			spus_asrc_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC6", SND_SOC_NOPM, 6, 0, NULL, 0,
			spus_asrc_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC7", SND_SOC_NOPM, 7, 0, NULL, 0,
			spus_asrc_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC8", SND_SOC_NOPM, 8, 0, NULL, 0,
			spus_asrc_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC9", SND_SOC_NOPM, 9, 0, NULL, 0,
			spus_asrc_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC10", SND_SOC_NOPM, 10, 0, NULL, 0,
			spus_asrc_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUS ASRC11", SND_SOC_NOPM, 11, 0, NULL, 0,
			spus_asrc_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DEMUX("SPUS OUT0", SND_SOC_NOPM, 0, 0, spus_out0_controls),
	SND_SOC_DAPM_DEMUX("SPUS OUT1", SND_SOC_NOPM, 0, 0, spus_out1_controls),
	SND_SOC_DAPM_DEMUX("SPUS OUT2", SND_SOC_NOPM, 0, 0, spus_out2_controls),
	SND_SOC_DAPM_DEMUX("SPUS OUT3", SND_SOC_NOPM, 0, 0, spus_out3_controls),
	SND_SOC_DAPM_DEMUX("SPUS OUT4", SND_SOC_NOPM, 0, 0, spus_out4_controls),
	SND_SOC_DAPM_DEMUX("SPUS OUT5", SND_SOC_NOPM, 0, 0, spus_out5_controls),
	SND_SOC_DAPM_DEMUX("SPUS OUT6", SND_SOC_NOPM, 0, 0, spus_out6_controls),
	SND_SOC_DAPM_DEMUX("SPUS OUT7", SND_SOC_NOPM, 0, 0, spus_out7_controls),
	SND_SOC_DAPM_DEMUX("SPUS OUT8", SND_SOC_NOPM, 0, 0, spus_out8_controls),
	SND_SOC_DAPM_DEMUX("SPUS OUT9", SND_SOC_NOPM, 0, 0, spus_out9_controls),
	SND_SOC_DAPM_DEMUX("SPUS OUT10", SND_SOC_NOPM, 0, 0,
			spus_out10_controls),
	SND_SOC_DAPM_DEMUX("SPUS OUT11", SND_SOC_NOPM, 0, 0,
			spus_out11_controls),

	SND_SOC_DAPM_PGA("SPUS OUT0-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT1-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT2-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT3-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT4-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT5-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT6-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT7-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT8-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT9-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT10-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT11-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT0-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT1-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT2-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT3-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT4-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT5-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT6-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT7-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT8-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT9-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT10-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT11-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT0-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT1-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT2-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT3-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT4-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT5-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT6-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT7-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT8-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT9-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT10-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT11-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT0-SIFS3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT1-SIFS3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT2-SIFS3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT3-SIFS3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT4-SIFS3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT5-SIFS3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT6-SIFS3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT7-SIFS3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT8-SIFS3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT9-SIFS3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT10-SIFS3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT11-SIFS3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT0-SIFS4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT1-SIFS4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT2-SIFS4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT3-SIFS4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT4-SIFS4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT5-SIFS4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT6-SIFS4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT7-SIFS4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT8-SIFS4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT9-SIFS4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT10-SIFS4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT11-SIFS4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT0-SIFS5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT1-SIFS5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT2-SIFS5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT3-SIFS5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT4-SIFS5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT5-SIFS5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT6-SIFS5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT7-SIFS5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT8-SIFS5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT9-SIFS5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT10-SIFS5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT11-SIFS5", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER_E("SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0,
			sifs0_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SIFS1", SND_SOC_NOPM, 0, 0, sifs1_controls,
			sifs1_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SIFS2", SND_SOC_NOPM, 0, 0, sifs2_controls,
			sifs2_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SIFS3", SND_SOC_NOPM, 0, 0, sifs3_controls,
			sifs3_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SIFS4", SND_SOC_NOPM, 0, 0, sifs4_controls,
			sifs4_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SIFS5", SND_SOC_NOPM, 0, 0, sifs5_controls,
			sifs5_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SWITCH("SIFS0 OUT", SND_SOC_NOPM, 0, 0,
			sifs0_out_controls),
	SND_SOC_DAPM_SWITCH("SIFS1 OUT", SND_SOC_NOPM, 0, 0,
			sifs1_out_controls),
	SND_SOC_DAPM_SWITCH("SIFS2 OUT", SND_SOC_NOPM, 0, 0,
			sifs2_out_controls),
	SND_SOC_DAPM_SWITCH("SIFS3 OUT", SND_SOC_NOPM, 0, 0,
			sifs3_out_controls),
	SND_SOC_DAPM_SWITCH("SIFS4 OUT", SND_SOC_NOPM, 0, 0,
			sifs4_out_controls),
	SND_SOC_DAPM_SWITCH("SIFS5 OUT", SND_SOC_NOPM, 0, 0,
			sifs5_out_controls),

	SND_SOC_DAPM_MUX("UAIF0 SPK", SND_SOC_NOPM, 0, 0, uaif0_spk_controls),
	SND_SOC_DAPM_MUX("UAIF1 SPK", SND_SOC_NOPM, 0, 0, uaif1_spk_controls),
	SND_SOC_DAPM_MUX("UAIF2 SPK", SND_SOC_NOPM, 0, 0, uaif2_spk_controls),
	SND_SOC_DAPM_MUX("UAIF3 SPK", SND_SOC_NOPM, 0, 0, uaif3_spk_controls),
	SND_SOC_DAPM_MUX("UAIF4 SPK", SND_SOC_NOPM, 0, 0, uaif4_spk_controls),
	SND_SOC_DAPM_MUX("UAIF5 SPK", SND_SOC_NOPM, 0, 0, uaif5_spk_controls),
	SND_SOC_DAPM_MUX("UAIF6 SPK", SND_SOC_NOPM, 0, 0, uaif6_spk_controls),
	SND_SOC_DAPM_MUX("DSIF SPK", SND_SOC_NOPM, 0, 0, dsif_spk_controls),

	SND_SOC_DAPM_SWITCH("UAIF0 PLA", SND_SOC_NOPM, 0, 0, uaif0_controls),
	SND_SOC_DAPM_SWITCH("UAIF1 PLA", SND_SOC_NOPM, 0, 0, uaif1_controls),
	SND_SOC_DAPM_SWITCH("UAIF2 PLA", SND_SOC_NOPM, 0, 0, uaif2_controls),
	SND_SOC_DAPM_SWITCH("UAIF3 PLA", SND_SOC_NOPM, 0, 0, uaif3_controls),
	SND_SOC_DAPM_SWITCH("UAIF4 PLA", SND_SOC_NOPM, 0, 0, uaif4_controls),
	SND_SOC_DAPM_SWITCH("UAIF5 PLA", SND_SOC_NOPM, 0, 0, uaif5_controls),
	SND_SOC_DAPM_SWITCH("UAIF6 PLA", SND_SOC_NOPM, 0, 0, uaif6_controls),
	SND_SOC_DAPM_SWITCH("DSIF PLA", SND_SOC_NOPM, 0, 0, dsif_controls),

	SND_SOC_DAPM_SWITCH("UAIF0 CAP", SND_SOC_NOPM, 0, 0, uaif0_controls),
	SND_SOC_DAPM_SWITCH("UAIF1 CAP", SND_SOC_NOPM, 0, 0, uaif1_controls),
	SND_SOC_DAPM_SWITCH("UAIF2 CAP", SND_SOC_NOPM, 0, 0, uaif2_controls),
	SND_SOC_DAPM_SWITCH("UAIF3 CAP", SND_SOC_NOPM, 0, 0, uaif3_controls),
	SND_SOC_DAPM_SWITCH("UAIF4 CAP", SND_SOC_NOPM, 0, 0, uaif4_controls),
	SND_SOC_DAPM_SWITCH("UAIF5 CAP", SND_SOC_NOPM, 0, 0, uaif5_controls),
	SND_SOC_DAPM_SWITCH("UAIF6 CAP", SND_SOC_NOPM, 0, 0, uaif6_controls),
	SND_SOC_DAPM_SWITCH("SPDY CAP", SND_SOC_NOPM, 0, 0, spdy_controls),

	SND_SOC_DAPM_MUX_E("NSRC0", SND_SOC_NOPM, 0, 0, nsrc0_controls,
			nsrc0_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("NSRC1", SND_SOC_NOPM, 0, 0, nsrc1_controls,
			nsrc1_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("NSRC2", SND_SOC_NOPM, 0, 0, nsrc2_controls,
			nsrc2_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("NSRC3", SND_SOC_NOPM, 0, 0, nsrc3_controls,
			nsrc3_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("NSRC4", SND_SOC_NOPM, 0, 0, nsrc4_controls,
			nsrc4_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("SPUM ASRC0", SND_SOC_NOPM, 0, 0, NULL, 0,
			spum_asrc_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUM ASRC1", SND_SOC_NOPM, 1, 0, NULL, 0,
			spum_asrc_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUM ASRC2", SND_SOC_NOPM, 2, 0, NULL, 0,
			spum_asrc_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUM ASRC3", SND_SOC_NOPM, 3, 0, NULL, 0,
			spum_asrc_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPUM ASRC4", SND_SOC_NOPM, 4, 0, NULL, 0,
			spum_asrc_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DEMUX("SIFM0", SND_SOC_NOPM, 0, 0, sifm0_controls),
	SND_SOC_DAPM_DEMUX("SIFM1", SND_SOC_NOPM, 0, 0, sifm1_controls),
	SND_SOC_DAPM_DEMUX("SIFM2", SND_SOC_NOPM, 0, 0, sifm2_controls),
	SND_SOC_DAPM_DEMUX("SIFM3", SND_SOC_NOPM, 0, 0, sifm3_controls),
	SND_SOC_DAPM_DEMUX("SIFM4", SND_SOC_NOPM, 0, 0, sifm4_controls),

	SND_SOC_DAPM_PGA("SIFM0-SIFMS", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFM1-SIFMS", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFM2-SIFMS", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFM3-SIFMS", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFM4-SIFMS", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MUX("SIFMS", SND_SOC_NOPM, 0, 0, sifms_controls),
};

static const struct snd_soc_dapm_route cmpnt_routes[] = {
	/* sink, control, source */
	{"SIFSM", NULL, "SPUSM"},
	{"SIFSM-SPUS IN0", "SPUS IN0", "SIFSM"},
	{"SIFSM-SPUS IN1", "SPUS IN1", "SIFSM"},
	{"SIFSM-SPUS IN2", "SPUS IN2", "SIFSM"},
	{"SIFSM-SPUS IN3", "SPUS IN3", "SIFSM"},
	{"SIFSM-SPUS IN4", "SPUS IN4", "SIFSM"},
	{"SIFSM-SPUS IN5", "SPUS IN5", "SIFSM"},
	{"SIFSM-SPUS IN6", "SPUS IN6", "SIFSM"},
	{"SIFSM-SPUS IN7", "SPUS IN7", "SIFSM"},
	{"SIFSM-SPUS IN8", "SPUS IN8", "SIFSM"},
	{"SIFSM-SPUS IN9", "SPUS IN9", "SIFSM"},
	{"SIFSM-SPUS IN10", "SPUS IN10", "SIFSM"},
	{"SIFSM-SPUS IN11", "SPUS IN11", "SIFSM"},

	{"SPUS IN0", "SIFSM", "SIFSM-SPUS IN0"},
	{"SPUS IN1", "SIFSM", "SIFSM-SPUS IN1"},
	{"SPUS IN2", "SIFSM", "SIFSM-SPUS IN2"},
	{"SPUS IN3", "SIFSM", "SIFSM-SPUS IN3"},
	{"SPUS IN4", "SIFSM", "SIFSM-SPUS IN4"},
	{"SPUS IN5", "SIFSM", "SIFSM-SPUS IN5"},
	{"SPUS IN6", "SIFSM", "SIFSM-SPUS IN6"},
	{"SPUS IN7", "SIFSM", "SIFSM-SPUS IN7"},
	{"SPUS IN8", "SIFSM", "SIFSM-SPUS IN8"},
	{"SPUS IN9", "SIFSM", "SIFSM-SPUS IN9"},
	{"SPUS IN10", "SIFSM", "SIFSM-SPUS IN10"},
	{"SPUS IN11", "SIFSM", "SIFSM-SPUS IN11"},

	{"SIFST-SPUS IN0", "SPUS IN0", "SIFST"},
	{"SIFST-SPUS IN1", "SPUS IN1", "SIFST"},
	{"SIFST-SPUS IN2", "SPUS IN2", "SIFST"},
	{"SIFST-SPUS IN3", "SPUS IN3", "SIFST"},
	{"SIFST-SPUS IN4", "SPUS IN4", "SIFST"},
	{"SIFST-SPUS IN5", "SPUS IN5", "SIFST"},
	{"SIFST-SPUS IN6", "SPUS IN6", "SIFST"},
	{"SIFST-SPUS IN7", "SPUS IN7", "SIFST"},
	{"SIFST-SPUS IN8", "SPUS IN8", "SIFST"},
	{"SIFST-SPUS IN9", "SPUS IN9", "SIFST"},
	{"SIFST-SPUS IN10", "SPUS IN10", "SIFST"},
	{"SIFST-SPUS IN11", "SPUS IN11", "SIFST"},

	{"SPUS IN0", "SIFST", "SIFST-SPUS IN0"},
	{"SPUS IN1", "SIFST", "SIFST-SPUS IN1"},
	{"SPUS IN2", "SIFST", "SIFST-SPUS IN2"},
	{"SPUS IN3", "SIFST", "SIFST-SPUS IN3"},
	{"SPUS IN4", "SIFST", "SIFST-SPUS IN4"},
	{"SPUS IN5", "SIFST", "SIFST-SPUS IN5"},
	{"SPUS IN6", "SIFST", "SIFST-SPUS IN6"},
	{"SPUS IN7", "SIFST", "SIFST-SPUS IN7"},
	{"SPUS IN8", "SIFST", "SIFST-SPUS IN8"},
	{"SPUS IN9", "SIFST", "SIFST-SPUS IN9"},
	{"SPUS IN10", "SIFST", "SIFST-SPUS IN10"},
	{"SPUS IN11", "SIFST", "SIFST-SPUS IN11"},

	{"SPUS ASRC0", NULL, "SPUS IN0"},
	{"SPUS ASRC1", NULL, "SPUS IN1"},
	{"SPUS ASRC2", NULL, "SPUS IN2"},
	{"SPUS ASRC3", NULL, "SPUS IN3"},
	{"SPUS ASRC4", NULL, "SPUS IN4"},
	{"SPUS ASRC5", NULL, "SPUS IN5"},
	{"SPUS ASRC6", NULL, "SPUS IN6"},
	{"SPUS ASRC7", NULL, "SPUS IN7"},
	{"SPUS ASRC8", NULL, "SPUS IN8"},
	{"SPUS ASRC9", NULL, "SPUS IN9"},
	{"SPUS ASRC10", NULL, "SPUS IN10"},
	{"SPUS ASRC11", NULL, "SPUS IN11"},

	{"SPUS OUT0", NULL, "SPUS ASRC0"},
	{"SPUS OUT1", NULL, "SPUS ASRC1"},
	{"SPUS OUT2", NULL, "SPUS ASRC2"},
	{"SPUS OUT3", NULL, "SPUS ASRC3"},
	{"SPUS OUT4", NULL, "SPUS ASRC4"},
	{"SPUS OUT5", NULL, "SPUS ASRC5"},
	{"SPUS OUT6", NULL, "SPUS ASRC6"},
	{"SPUS OUT7", NULL, "SPUS ASRC7"},
	{"SPUS OUT8", NULL, "SPUS ASRC8"},
	{"SPUS OUT9", NULL, "SPUS ASRC9"},
	{"SPUS OUT10", NULL, "SPUS ASRC10"},
	{"SPUS OUT11", NULL, "SPUS ASRC11"},

	{"SPUS OUT0-SIFS0", "SIFS0", "SPUS OUT0"},
	{"SPUS OUT1-SIFS0", "SIFS0", "SPUS OUT1"},
	{"SPUS OUT2-SIFS0", "SIFS0", "SPUS OUT2"},
	{"SPUS OUT3-SIFS0", "SIFS0", "SPUS OUT3"},
	{"SPUS OUT4-SIFS0", "SIFS0", "SPUS OUT4"},
	{"SPUS OUT5-SIFS0", "SIFS0", "SPUS OUT5"},
	{"SPUS OUT6-SIFS0", "SIFS0", "SPUS OUT6"},
	{"SPUS OUT7-SIFS0", "SIFS0", "SPUS OUT7"},
	{"SPUS OUT8-SIFS0", "SIFS0", "SPUS OUT8"},
	{"SPUS OUT9-SIFS0", "SIFS0", "SPUS OUT9"},
	{"SPUS OUT10-SIFS0", "SIFS0", "SPUS OUT10"},
	{"SPUS OUT11-SIFS0", "SIFS0", "SPUS OUT11"},
	{"SPUS OUT0-SIFS1", "SIFS1", "SPUS OUT0"},
	{"SPUS OUT1-SIFS1", "SIFS1", "SPUS OUT1"},
	{"SPUS OUT2-SIFS1", "SIFS1", "SPUS OUT2"},
	{"SPUS OUT3-SIFS1", "SIFS1", "SPUS OUT3"},
	{"SPUS OUT4-SIFS1", "SIFS1", "SPUS OUT4"},
	{"SPUS OUT5-SIFS1", "SIFS1", "SPUS OUT5"},
	{"SPUS OUT6-SIFS1", "SIFS1", "SPUS OUT6"},
	{"SPUS OUT7-SIFS1", "SIFS1", "SPUS OUT7"},
	{"SPUS OUT8-SIFS1", "SIFS1", "SPUS OUT8"},
	{"SPUS OUT9-SIFS1", "SIFS1", "SPUS OUT9"},
	{"SPUS OUT10-SIFS1", "SIFS1", "SPUS OUT10"},
	{"SPUS OUT11-SIFS1", "SIFS1", "SPUS OUT11"},
	{"SPUS OUT0-SIFS2", "SIFS2", "SPUS OUT0"},
	{"SPUS OUT1-SIFS2", "SIFS2", "SPUS OUT1"},
	{"SPUS OUT2-SIFS2", "SIFS2", "SPUS OUT2"},
	{"SPUS OUT3-SIFS2", "SIFS2", "SPUS OUT3"},
	{"SPUS OUT4-SIFS2", "SIFS2", "SPUS OUT4"},
	{"SPUS OUT5-SIFS2", "SIFS2", "SPUS OUT5"},
	{"SPUS OUT6-SIFS2", "SIFS2", "SPUS OUT6"},
	{"SPUS OUT7-SIFS2", "SIFS2", "SPUS OUT7"},
	{"SPUS OUT8-SIFS2", "SIFS2", "SPUS OUT8"},
	{"SPUS OUT9-SIFS2", "SIFS2", "SPUS OUT9"},
	{"SPUS OUT10-SIFS2", "SIFS2", "SPUS OUT10"},
	{"SPUS OUT11-SIFS2", "SIFS2", "SPUS OUT11"},
	{"SPUS OUT0-SIFS3", "SIFS3", "SPUS OUT0"},
	{"SPUS OUT1-SIFS3", "SIFS3", "SPUS OUT1"},
	{"SPUS OUT2-SIFS3", "SIFS3", "SPUS OUT2"},
	{"SPUS OUT3-SIFS3", "SIFS3", "SPUS OUT3"},
	{"SPUS OUT4-SIFS3", "SIFS3", "SPUS OUT4"},
	{"SPUS OUT5-SIFS3", "SIFS3", "SPUS OUT5"},
	{"SPUS OUT6-SIFS3", "SIFS3", "SPUS OUT6"},
	{"SPUS OUT7-SIFS3", "SIFS3", "SPUS OUT7"},
	{"SPUS OUT8-SIFS3", "SIFS3", "SPUS OUT8"},
	{"SPUS OUT9-SIFS3", "SIFS3", "SPUS OUT9"},
	{"SPUS OUT10-SIFS3", "SIFS3", "SPUS OUT10"},
	{"SPUS OUT11-SIFS3", "SIFS3", "SPUS OUT11"},
	{"SPUS OUT0-SIFS4", "SIFS4", "SPUS OUT0"},
	{"SPUS OUT1-SIFS4", "SIFS4", "SPUS OUT1"},
	{"SPUS OUT2-SIFS4", "SIFS4", "SPUS OUT2"},
	{"SPUS OUT3-SIFS4", "SIFS4", "SPUS OUT3"},
	{"SPUS OUT4-SIFS4", "SIFS4", "SPUS OUT4"},
	{"SPUS OUT5-SIFS4", "SIFS4", "SPUS OUT5"},
	{"SPUS OUT6-SIFS4", "SIFS4", "SPUS OUT6"},
	{"SPUS OUT7-SIFS4", "SIFS4", "SPUS OUT7"},
	{"SPUS OUT8-SIFS4", "SIFS4", "SPUS OUT8"},
	{"SPUS OUT9-SIFS4", "SIFS4", "SPUS OUT9"},
	{"SPUS OUT10-SIFS4", "SIFS4", "SPUS OUT10"},
	{"SPUS OUT11-SIFS4", "SIFS4", "SPUS OUT11"},
	{"SPUS OUT0-SIFS5", "SIFS5", "SPUS OUT0"},
	{"SPUS OUT1-SIFS5", "SIFS5", "SPUS OUT1"},
	{"SPUS OUT2-SIFS5", "SIFS5", "SPUS OUT2"},
	{"SPUS OUT3-SIFS5", "SIFS5", "SPUS OUT3"},
	{"SPUS OUT4-SIFS5", "SIFS5", "SPUS OUT4"},
	{"SPUS OUT5-SIFS5", "SIFS5", "SPUS OUT5"},
	{"SPUS OUT6-SIFS5", "SIFS5", "SPUS OUT6"},
	{"SPUS OUT7-SIFS5", "SIFS5", "SPUS OUT7"},
	{"SPUS OUT8-SIFS5", "SIFS5", "SPUS OUT8"},
	{"SPUS OUT9-SIFS5", "SIFS5", "SPUS OUT9"},
	{"SPUS OUT10-SIFS5", "SIFS5", "SPUS OUT10"},
	{"SPUS OUT11-SIFS5", "SIFS5", "SPUS OUT11"},

	{"SPUS OUT0-SIFS0", "SIDETONE-SIFS0", "SPUS OUT0"},
	{"SPUS OUT1-SIFS0", "SIDETONE-SIFS0", "SPUS OUT1"},
	{"SPUS OUT2-SIFS0", "SIDETONE-SIFS0", "SPUS OUT2"},
	{"SPUS OUT3-SIFS0", "SIDETONE-SIFS0", "SPUS OUT3"},
	{"SPUS OUT4-SIFS0", "SIDETONE-SIFS0", "SPUS OUT4"},
	{"SPUS OUT5-SIFS0", "SIDETONE-SIFS0", "SPUS OUT5"},
	{"SPUS OUT6-SIFS0", "SIDETONE-SIFS0", "SPUS OUT6"},
	{"SPUS OUT7-SIFS0", "SIDETONE-SIFS0", "SPUS OUT7"},
	{"SPUS OUT8-SIFS0", "SIDETONE-SIFS0", "SPUS OUT8"},
	{"SPUS OUT9-SIFS0", "SIDETONE-SIFS0", "SPUS OUT9"},
	{"SPUS OUT10-SIFS0", "SIDETONE-SIFS0", "SPUS OUT10"},
	{"SPUS OUT11-SIFS0", "SIDETONE-SIFS0", "SPUS OUT11"},

	{"SIFS0", NULL, "SPUS OUT0-SIFS0"},
	{"SIFS0", NULL, "SPUS OUT1-SIFS0"},
	{"SIFS0", NULL, "SPUS OUT2-SIFS0"},
	{"SIFS0", NULL, "SPUS OUT3-SIFS0"},
	{"SIFS0", NULL, "SPUS OUT4-SIFS0"},
	{"SIFS0", NULL, "SPUS OUT5-SIFS0"},
	{"SIFS0", NULL, "SPUS OUT6-SIFS0"},
	{"SIFS0", NULL, "SPUS OUT7-SIFS0"},
	{"SIFS0", NULL, "SPUS OUT8-SIFS0"},
	{"SIFS0", NULL, "SPUS OUT9-SIFS0"},
	{"SIFS0", NULL, "SPUS OUT10-SIFS0"},
	{"SIFS0", NULL, "SPUS OUT11-SIFS0"},
	{"SIFS1", "SPUS OUT0", "SPUS OUT0-SIFS1"},
	{"SIFS1", "SPUS OUT1", "SPUS OUT1-SIFS1"},
	{"SIFS1", "SPUS OUT2", "SPUS OUT2-SIFS1"},
	{"SIFS1", "SPUS OUT3", "SPUS OUT3-SIFS1"},
	{"SIFS1", "SPUS OUT4", "SPUS OUT4-SIFS1"},
	{"SIFS1", "SPUS OUT5", "SPUS OUT5-SIFS1"},
	{"SIFS1", "SPUS OUT6", "SPUS OUT6-SIFS1"},
	{"SIFS1", "SPUS OUT7", "SPUS OUT7-SIFS1"},
	{"SIFS1", "SPUS OUT8", "SPUS OUT8-SIFS1"},
	{"SIFS1", "SPUS OUT9", "SPUS OUT9-SIFS1"},
	{"SIFS1", "SPUS OUT10", "SPUS OUT10-SIFS1"},
	{"SIFS1", "SPUS OUT11", "SPUS OUT11-SIFS1"},
	{"SIFS2", "SPUS OUT0", "SPUS OUT0-SIFS2"},
	{"SIFS2", "SPUS OUT1", "SPUS OUT1-SIFS2"},
	{"SIFS2", "SPUS OUT2", "SPUS OUT2-SIFS2"},
	{"SIFS2", "SPUS OUT3", "SPUS OUT3-SIFS2"},
	{"SIFS2", "SPUS OUT4", "SPUS OUT4-SIFS2"},
	{"SIFS2", "SPUS OUT5", "SPUS OUT5-SIFS2"},
	{"SIFS2", "SPUS OUT6", "SPUS OUT6-SIFS2"},
	{"SIFS2", "SPUS OUT7", "SPUS OUT7-SIFS2"},
	{"SIFS2", "SPUS OUT8", "SPUS OUT8-SIFS2"},
	{"SIFS2", "SPUS OUT9", "SPUS OUT9-SIFS2"},
	{"SIFS2", "SPUS OUT10", "SPUS OUT10-SIFS2"},
	{"SIFS2", "SPUS OUT11", "SPUS OUT11-SIFS2"},
	{"SIFS3", "SPUS OUT0", "SPUS OUT0-SIFS3"},
	{"SIFS3", "SPUS OUT1", "SPUS OUT1-SIFS3"},
	{"SIFS3", "SPUS OUT2", "SPUS OUT2-SIFS3"},
	{"SIFS3", "SPUS OUT3", "SPUS OUT3-SIFS3"},
	{"SIFS3", "SPUS OUT4", "SPUS OUT4-SIFS3"},
	{"SIFS3", "SPUS OUT5", "SPUS OUT5-SIFS3"},
	{"SIFS3", "SPUS OUT6", "SPUS OUT6-SIFS3"},
	{"SIFS3", "SPUS OUT7", "SPUS OUT7-SIFS3"},
	{"SIFS3", "SPUS OUT8", "SPUS OUT8-SIFS3"},
	{"SIFS3", "SPUS OUT9", "SPUS OUT9-SIFS3"},
	{"SIFS3", "SPUS OUT10", "SPUS OUT10-SIFS3"},
	{"SIFS3", "SPUS OUT11", "SPUS OUT11-SIFS3"},
	{"SIFS4", "SPUS OUT0", "SPUS OUT0-SIFS4"},
	{"SIFS4", "SPUS OUT1", "SPUS OUT1-SIFS4"},
	{"SIFS4", "SPUS OUT2", "SPUS OUT2-SIFS4"},
	{"SIFS4", "SPUS OUT3", "SPUS OUT3-SIFS4"},
	{"SIFS4", "SPUS OUT4", "SPUS OUT4-SIFS4"},
	{"SIFS4", "SPUS OUT5", "SPUS OUT5-SIFS4"},
	{"SIFS4", "SPUS OUT6", "SPUS OUT6-SIFS4"},
	{"SIFS4", "SPUS OUT7", "SPUS OUT7-SIFS4"},
	{"SIFS4", "SPUS OUT8", "SPUS OUT8-SIFS4"},
	{"SIFS4", "SPUS OUT9", "SPUS OUT9-SIFS4"},
	{"SIFS4", "SPUS OUT10", "SPUS OUT10-SIFS4"},
	{"SIFS4", "SPUS OUT11", "SPUS OUT11-SIFS4"},
	{"SIFS5", "SPUS OUT0", "SPUS OUT0-SIFS5"},
	{"SIFS5", "SPUS OUT1", "SPUS OUT1-SIFS5"},
	{"SIFS5", "SPUS OUT2", "SPUS OUT2-SIFS5"},
	{"SIFS5", "SPUS OUT3", "SPUS OUT3-SIFS5"},
	{"SIFS5", "SPUS OUT4", "SPUS OUT4-SIFS5"},
	{"SIFS5", "SPUS OUT5", "SPUS OUT5-SIFS5"},
	{"SIFS5", "SPUS OUT6", "SPUS OUT6-SIFS5"},
	{"SIFS5", "SPUS OUT7", "SPUS OUT7-SIFS5"},
	{"SIFS5", "SPUS OUT8", "SPUS OUT8-SIFS5"},
	{"SIFS5", "SPUS OUT9", "SPUS OUT9-SIFS5"},
	{"SIFS5", "SPUS OUT10", "SPUS OUT10-SIFS5"},
	{"SIFS5", "SPUS OUT11", "SPUS OUT11-SIFS5"},

	{"SIFS0 OUT", "Switch", "SIFS0"},
	{"SIFS1 OUT", "Switch", "SIFS1"},
	{"SIFS2 OUT", "Switch", "SIFS2"},
	{"SIFS3 OUT", "Switch", "SIFS3"},
	{"SIFS4 OUT", "Switch", "SIFS4"},
	{"SIFS5 OUT", "Switch", "SIFS5"},

	{"UAIF0 SPK", "SIFS0", "SIFS0 OUT"},
	{"UAIF0 SPK", "SIFS1", "SIFS1 OUT"},
	{"UAIF0 SPK", "SIFS2", "SIFS2 OUT"},
	{"UAIF0 SPK", "SIFS3", "SIFS3 OUT"},
	{"UAIF0 SPK", "SIFS4", "SIFS4 OUT"},
	{"UAIF0 SPK", "SIFS5", "SIFS5 OUT"},
	{"UAIF0 SPK", "SIFMS", "SIFMS"},
	{"UAIF1 SPK", "SIFS0", "SIFS0 OUT"},
	{"UAIF1 SPK", "SIFS1", "SIFS1 OUT"},
	{"UAIF1 SPK", "SIFS2", "SIFS2 OUT"},
	{"UAIF1 SPK", "SIFS3", "SIFS3 OUT"},
	{"UAIF1 SPK", "SIFS4", "SIFS4 OUT"},
	{"UAIF1 SPK", "SIFS5", "SIFS5 OUT"},
	{"UAIF1 SPK", "SIFMS", "SIFMS"},
	{"UAIF2 SPK", "SIFS0", "SIFS0 OUT"},
	{"UAIF2 SPK", "SIFS1", "SIFS1 OUT"},
	{"UAIF2 SPK", "SIFS2", "SIFS2 OUT"},
	{"UAIF2 SPK", "SIFS3", "SIFS3 OUT"},
	{"UAIF2 SPK", "SIFS4", "SIFS4 OUT"},
	{"UAIF2 SPK", "SIFS5", "SIFS5 OUT"},
	{"UAIF2 SPK", "SIFMS", "SIFMS"},
	{"UAIF3 SPK", "SIFS0", "SIFS0 OUT"},
	{"UAIF3 SPK", "SIFS1", "SIFS1 OUT"},
	{"UAIF3 SPK", "SIFS2", "SIFS2 OUT"},
	{"UAIF3 SPK", "SIFS3", "SIFS3 OUT"},
	{"UAIF3 SPK", "SIFS4", "SIFS4 OUT"},
	{"UAIF3 SPK", "SIFS5", "SIFS5 OUT"},
	{"UAIF3 SPK", "SIFMS", "SIFMS"},
	{"UAIF4 SPK", "SIFS0", "SIFS0 OUT"},
	{"UAIF4 SPK", "SIFS1", "SIFS1 OUT"},
	{"UAIF4 SPK", "SIFS2", "SIFS2 OUT"},
	{"UAIF4 SPK", "SIFS3", "SIFS3 OUT"},
	{"UAIF4 SPK", "SIFS4", "SIFS4 OUT"},
	{"UAIF4 SPK", "SIFS5", "SIFS5 OUT"},
	{"UAIF4 SPK", "SIFMS", "SIFMS"},
	{"UAIF5 SPK", "SIFS0", "SIFS0 OUT"},
	{"UAIF5 SPK", "SIFS1", "SIFS1 OUT"},
	{"UAIF5 SPK", "SIFS2", "SIFS2 OUT"},
	{"UAIF5 SPK", "SIFS3", "SIFS3 OUT"},
	{"UAIF5 SPK", "SIFS4", "SIFS4 OUT"},
	{"UAIF5 SPK", "SIFS5", "SIFS5 OUT"},
	{"UAIF5 SPK", "SIFMS", "SIFMS"},
	{"UAIF6 SPK", "SIFS0", "SIFS0 OUT"},
	{"UAIF6 SPK", "SIFS1", "SIFS1 OUT"},
	{"UAIF6 SPK", "SIFS2", "SIFS2 OUT"},
	{"UAIF6 SPK", "SIFS3", "SIFS3 OUT"},
	{"UAIF6 SPK", "SIFS4", "SIFS4 OUT"},
	{"UAIF6 SPK", "SIFS5", "SIFS5 OUT"},
	{"UAIF6 SPK", "SIFMS", "SIFMS"},
	{"DSIF SPK", "SIFS1", "SIFS1 OUT"},
	{"DSIF SPK", "SIFS2", "SIFS2 OUT"},
	{"DSIF SPK", "SIFS3", "SIFS3 OUT"},
	{"DSIF SPK", "SIFS4", "SIFS4 OUT"},
	{"DSIF SPK", "SIFS5", "SIFS5 OUT"},

	{"UAIF0 PLA", "UAIF0 Switch", "UAIF0 SPK"},
	{"UAIF1 PLA", "UAIF1 Switch", "UAIF1 SPK"},
	{"UAIF2 PLA", "UAIF2 Switch", "UAIF2 SPK"},
	{"UAIF3 PLA", "UAIF3 Switch", "UAIF3 SPK"},
	{"UAIF4 PLA", "UAIF4 Switch", "UAIF4 SPK"},
	{"UAIF5 PLA", "UAIF5 Switch", "UAIF5 SPK"},
	{"UAIF6 PLA", "UAIF6 Switch", "UAIF6 SPK"},
	{"DSIF PLA", "DSIF Switch", "DSIF SPK"},

	{"SIFS0", NULL, "SIFS0 Capture"},
	{"SIFS1", NULL, "SIFS1 Capture"},
	{"SIFS2", NULL, "SIFS2 Capture"},
	{"SIFS3", NULL, "SIFS3 Capture"},
	{"SIFS4", NULL, "SIFS4 Capture"},
	{"SIFS5", NULL, "SIFS5 Capture"},

	{"NSRC0 Playback", NULL, "NSRC0"},
	{"NSRC1 Playback", NULL, "NSRC1"},
	{"NSRC2 Playback", NULL, "NSRC2"},
	{"NSRC3 Playback", NULL, "NSRC3"},
	{"NSRC4 Playback", NULL, "NSRC4"},

	{"NSRC0", NULL, "NSRC0 Capture"},
	{"NSRC1", NULL, "NSRC1 Capture"},
	{"NSRC2", NULL, "NSRC2 Capture"},
	{"NSRC3", NULL, "NSRC3 Capture"},
	{"NSRC4", NULL, "NSRC4 Capture"},

	{"NSRC0", "SIFS0", "SIFS0 OUT"},
	{"NSRC0", "SIFS1", "SIFS1 OUT"},
	{"NSRC0", "SIFS2", "SIFS2 OUT"},
	{"NSRC0", "SIFS3", "SIFS3 OUT"},
	{"NSRC0", "SIFS4", "SIFS4 OUT"},
	{"NSRC0", "SIFS5", "SIFS5 OUT"},
	{"NSRC0", "UAIF0", "UAIF0 CAP"},
	{"NSRC0", "UAIF1", "UAIF1 CAP"},
	{"NSRC0", "UAIF2", "UAIF2 CAP"},
	{"NSRC0", "UAIF3", "UAIF3 CAP"},
	{"NSRC0", "UAIF4", "UAIF4 CAP"},
	{"NSRC0", "UAIF5", "UAIF5 CAP"},
	{"NSRC0", "UAIF6", "UAIF6 CAP"},
	{"NSRC0", "SPDY", "SPDY CAP"},

	{"NSRC1", "SIFS0", "SIFS0 OUT"},
	{"NSRC1", "SIFS1", "SIFS1 OUT"},
	{"NSRC1", "SIFS2", "SIFS2 OUT"},
	{"NSRC1", "SIFS3", "SIFS3 OUT"},
	{"NSRC1", "SIFS4", "SIFS4 OUT"},
	{"NSRC1", "SIFS5", "SIFS5 OUT"},
	{"NSRC1", "UAIF0", "UAIF0 CAP"},
	{"NSRC1", "UAIF1", "UAIF1 CAP"},
	{"NSRC1", "UAIF2", "UAIF2 CAP"},
	{"NSRC1", "UAIF3", "UAIF3 CAP"},
	{"NSRC1", "UAIF4", "UAIF4 CAP"},
	{"NSRC1", "UAIF5", "UAIF5 CAP"},
	{"NSRC1", "UAIF6", "UAIF6 CAP"},
	{"NSRC1", "SPDY", "SPDY CAP"},

	{"NSRC2", "SIFS0", "SIFS0 OUT"},
	{"NSRC2", "SIFS1", "SIFS1 OUT"},
	{"NSRC2", "SIFS2", "SIFS2 OUT"},
	{"NSRC2", "SIFS3", "SIFS3 OUT"},
	{"NSRC2", "SIFS4", "SIFS4 OUT"},
	{"NSRC2", "SIFS5", "SIFS5 OUT"},
	{"NSRC2", "UAIF0", "UAIF0 CAP"},
	{"NSRC2", "UAIF1", "UAIF1 CAP"},
	{"NSRC2", "UAIF2", "UAIF2 CAP"},
	{"NSRC2", "UAIF3", "UAIF3 CAP"},
	{"NSRC2", "UAIF4", "UAIF4 CAP"},
	{"NSRC2", "UAIF5", "UAIF5 CAP"},
	{"NSRC2", "UAIF6", "UAIF6 CAP"},
	{"NSRC2", "SPDY", "SPDY CAP"},

	{"NSRC3", "SIFS0", "SIFS0 OUT"},
	{"NSRC3", "SIFS1", "SIFS1 OUT"},
	{"NSRC3", "SIFS2", "SIFS2 OUT"},
	{"NSRC3", "SIFS3", "SIFS3 OUT"},
	{"NSRC3", "SIFS4", "SIFS4 OUT"},
	{"NSRC3", "SIFS5", "SIFS5 OUT"},
	{"NSRC3", "UAIF0", "UAIF0 CAP"},
	{"NSRC3", "UAIF1", "UAIF1 CAP"},
	{"NSRC3", "UAIF2", "UAIF2 CAP"},
	{"NSRC3", "UAIF3", "UAIF3 CAP"},
	{"NSRC3", "UAIF4", "UAIF4 CAP"},
	{"NSRC3", "UAIF5", "UAIF5 CAP"},
	{"NSRC3", "UAIF6", "UAIF6 CAP"},
	{"NSRC3", "SPDY", "SPDY CAP"},

	{"NSRC4", "SIFS0", "SIFS0 OUT"},
	{"NSRC4", "SIFS1", "SIFS1 OUT"},
	{"NSRC4", "SIFS2", "SIFS2 OUT"},
	{"NSRC4", "SIFS3", "SIFS3 OUT"},
	{"NSRC4", "SIFS4", "SIFS4 OUT"},
	{"NSRC4", "SIFS5", "SIFS5 OUT"},
	{"NSRC4", "UAIF0", "UAIF0 CAP"},
	{"NSRC4", "UAIF1", "UAIF1 CAP"},
	{"NSRC4", "UAIF2", "UAIF2 CAP"},
	{"NSRC4", "UAIF3", "UAIF3 CAP"},
	{"NSRC4", "UAIF4", "UAIF4 CAP"},
	{"NSRC4", "UAIF5", "UAIF5 CAP"},
	{"NSRC4", "UAIF6", "UAIF6 CAP"},
	{"NSRC4", "SPDY", "SPDY CAP"},

	{"SPUM ASRC0", NULL, "NSRC0"},
	{"SPUM ASRC1", NULL, "NSRC1"},
	{"SPUM ASRC2", NULL, "NSRC2"},
	{"SPUM ASRC3", NULL, "NSRC3"},
	{"SPUM ASRC4", NULL, "NSRC4"},

	{"SIFM0", NULL, "SPUM ASRC0"},
	{"SIFM1", NULL, "SPUM ASRC1"},
	{"SIFM2", NULL, "SPUM ASRC2"},
	{"SIFM3", NULL, "SPUM ASRC3"},
	{"SIFM4", NULL, "SPUM ASRC4"},

	{"SIFM0-SIFMS", "SIFMS", "SIFM0"},
	{"SIFM1-SIFMS", "SIFMS", "SIFM1"},
	{"SIFM2-SIFMS", "SIFMS", "SIFM2"},
	{"SIFM3-SIFMS", "SIFMS", "SIFM3"},
	{"SIFM4-SIFMS", "SIFMS", "SIFM4"},

	{"SIFMS", "SIFM0", "SIFM0-SIFMS"},
	{"SIFMS", "SIFM1", "SIFM1-SIFMS"},
	{"SIFMS", "SIFM2", "SIFM2-SIFMS"},
	{"SIFMS", "SIFM3", "SIFM3-SIFMS"},
	{"SIFMS", "SIFM4", "SIFM4-SIFMS"},
};

static int cmpnt_probe(struct snd_soc_component *cmpnt)
{
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(cmpnt);

	abox_dbg(dev, "%s\n", __func__);

	snd_soc_component_init_regmap(cmpnt, data->regmap);

	snd_soc_add_component_controls(cmpnt, spus_asrc_controls,
			ARRAY_SIZE(spus_asrc_controls));
	snd_soc_add_component_controls(cmpnt, spum_asrc_controls,
			ARRAY_SIZE(spum_asrc_controls));
	snd_soc_add_component_controls(cmpnt, spus_asrc_id_controls,
			ARRAY_SIZE(spus_asrc_id_controls));
	snd_soc_add_component_controls(cmpnt, spum_asrc_id_controls,
			ARRAY_SIZE(spum_asrc_id_controls));
	snd_soc_add_component_controls(cmpnt, spus_asrc_apf_coef_controls,
			ARRAY_SIZE(spus_asrc_apf_coef_controls));
	snd_soc_add_component_controls(cmpnt, spum_asrc_apf_coef_controls,
			ARRAY_SIZE(spum_asrc_apf_coef_controls));
	snd_soc_add_component_controls(cmpnt, spus_asrc_os_controls,
			ARRAY_SIZE(spus_asrc_os_controls));
	snd_soc_add_component_controls(cmpnt, spus_asrc_is_controls,
			ARRAY_SIZE(spus_asrc_is_controls));
	snd_soc_add_component_controls(cmpnt, spum_asrc_os_controls,
			ARRAY_SIZE(spum_asrc_os_controls));
	snd_soc_add_component_controls(cmpnt, spum_asrc_is_controls,
			ARRAY_SIZE(spum_asrc_is_controls));
	snd_soc_add_component_controls(cmpnt, sidetone_controls,
			ARRAY_SIZE(sidetone_controls));

	snd_soc_dapm_add_routes(dapm, sidetone_routes,
			ARRAY_SIZE(sidetone_routes));
	if (cmpnt->name_prefix) {
		struct snd_soc_dapm_route *routes;
		int i, num = ARRAY_SIZE(sidetone_routes);

		routes = kmemdup(sidetone_routes, sizeof(sidetone_routes),
				GFP_KERNEL);
		for (i = 0; i < num; i++) {
			routes[i].sink = kasprintf(GFP_KERNEL, "%s %s",
					cmpnt->name_prefix, routes[i].sink);
			routes[i].source = kasprintf(GFP_KERNEL, "%s %s",
					cmpnt->name_prefix, routes[i].source);
		}

		snd_soc_dapm_weak_routes(dapm, routes, num);

		for (i = 0; i < num; i++) {
			kfree(routes[i].sink);
			kfree(routes[i].source);
		}
		kfree(routes);
	} else {
		snd_soc_dapm_weak_routes(dapm, sidetone_routes,
				ARRAY_SIZE(sidetone_routes));
	}

	data->cmpnt = cmpnt;

	/* vdma and dump are initialized in abox component probe
	 * to set vdma to sound card 1 and dump to sound card 2.
	 */
	abox_vdma_register_card(dev);
	abox_dump_init(dev);

	wakeup_source_init(&data->ws, "abox");

	return 0;
}

static void cmpnt_remove(struct snd_soc_component *cmpnt)
{
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	abox_dbg(dev, "%s\n", __func__);

	wakeup_source_trash(&data->ws);
}

static const struct snd_soc_component_driver abox_cmpnt_drv = {
	.probe			= cmpnt_probe,
	.remove			= cmpnt_remove,
	.controls		= cmpnt_controls,
	.num_controls		= ARRAY_SIZE(cmpnt_controls),
	.dapm_widgets		= cmpnt_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(cmpnt_widgets),
	.dapm_routes		= cmpnt_routes,
	.num_dapm_routes	= ARRAY_SIZE(cmpnt_routes),
	.probe_order		= SND_SOC_COMP_ORDER_FIRST,
};

int abox_cmpnt_adjust_sbank(struct abox_data *data,
		enum abox_dai id, struct snd_pcm_hw_params *params)
{
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int time, size, reg, val;
	unsigned int mask = ABOX_SBANK_SIZE_MASK;
	int ret;

	switch (id) {
	case ABOX_RDMA0:
	case ABOX_RDMA1:
	case ABOX_RDMA2:
	case ABOX_RDMA3:
	case ABOX_RDMA4:
	case ABOX_RDMA5:
	case ABOX_RDMA6:
	case ABOX_RDMA7:
	case ABOX_RDMA8:
	case ABOX_RDMA9:
	case ABOX_RDMA10:
	case ABOX_RDMA11:
		reg = ABOX_SPUS_SBANK_RDMA(id - ABOX_RDMA0);
		break;
	case ABOX_RDMA0_BE:
	case ABOX_RDMA1_BE:
	case ABOX_RDMA2_BE:
	case ABOX_RDMA3_BE:
	case ABOX_RDMA4_BE:
	case ABOX_RDMA5_BE:
	case ABOX_RDMA6_BE:
	case ABOX_RDMA7_BE:
	case ABOX_RDMA8_BE:
	case ABOX_RDMA9_BE:
	case ABOX_RDMA10_BE:
	case ABOX_RDMA11_BE:
		reg = ABOX_SPUS_SBANK_RDMA(id - ABOX_RDMA0_BE);
		break;
	case ABOX_NSRC0:
	case ABOX_NSRC1:
	case ABOX_NSRC2:
	case ABOX_NSRC3:
	case ABOX_NSRC4:
	case ABOX_NSRC5:
	case ABOX_NSRC6:
	case ABOX_NSRC7:
		reg = ABOX_SPUM_SBANK_NSRC(id - ABOX_NSRC0);
		break;
	default:
		return -EINVAL;
	}

	time = hw_param_interval_c(params, SNDRV_PCM_HW_PARAM_PERIOD_TIME)->min;
	time /= 1000;
	if (time <= 10)
		size = SZ_128;
	else
		size = SZ_512;

	/* Sbank size unit is 16byte(= 4 shifts) */
	val = size << (ABOX_SBANK_SIZE_L - 4);

	abox_info(cmpnt->dev, "%s(%#x, %ums): %u\n", __func__, id, time, size);

	ret = snd_soc_component_update_bits(cmpnt, reg, mask, val);
	if (ret < 0) {
		abox_err(cmpnt->dev, "sbank write error: %d\n", ret);
		return ret;
	}

	return size;
}

int abox_cmpnt_reset_cnt_val(struct abox_data *data, enum abox_dai id)
{
	struct device *dev = data->dev;
	struct snd_soc_component *cmpnt = data->cmpnt;
	unsigned int idx, val, sifs_id;
	int ret = 0;

	abox_dbg(dev, "%s(%#x)\n", __func__, id);

	ret = snd_soc_component_read(cmpnt, ABOX_ROUTE_CTRL0, &val);
	if (ret < 0)
		return ret;

	switch (id) {
	case ABOX_UAIF0:
	case ABOX_UAIF1:
	case ABOX_UAIF2:
	case ABOX_UAIF3:
	case ABOX_UAIF4:
	case ABOX_UAIF5:
	case ABOX_UAIF6:
		idx = id - ABOX_UAIF0;
		sifs_id = val & ABOX_ROUTE_UAIF_SPK_MASK(idx);
		sifs_id = (sifs_id >> ABOX_ROUTE_UAIF_SPK_L(idx)) - 1;
		break;
	case ABOX_DSIF:
		sifs_id = val & ABOX_ROUTE_DSIF_MASK;
		sifs_id = (sifs_id >> ABOX_ROUTE_DSIF_L) - 1;
		break;
	default:
		return -EINVAL;
	}

	if (sifs_id < 12) {
		ret = snd_soc_component_write(cmpnt,
				ABOX_SPUS_CTRL_SIFS_CNT(sifs_id), 0);
		if (ret < 0)
			return ret;
		abox_info(dev, "reset sifs%d_cnt_val\n", sifs_id);
	}

	return ret;
}

static int sifs_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	int ret = 0;

	if (substream->stream != SNDRV_PCM_STREAM_CAPTURE)
		goto out;

	ret = set_cnt_val(data, dai, params);
out:
	return ret;
}

static const struct snd_soc_dai_ops sifs_dai_ops = {
	.hw_params	= sifs_hw_params,
};

static int src_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	enum abox_dai id = dai->id;
	struct abox_data *data = snd_soc_component_get_drvdata(dai->component);
	int ret = 0;

	if (substream->stream != SNDRV_PCM_STREAM_CAPTURE)
		return 0;

	abox_dbg(dev, "%s[%#x]\n", __func__, id);

	ret = abox_cmpnt_adjust_sbank(data, id, params);
	if (ret < 0)
		return ret;

	return 0;
}

static const struct snd_soc_dai_ops src_dai_ops = {
	.hw_params	= src_hw_params,
};

static struct snd_soc_dai_driver abox_cmpnt_dai_drv[] = {
	{
		.name = "SIFS0",
		.id = ABOX_SIFS0,
		.capture = {
			.stream_name = "SIFS0 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.ops = &sifs_dai_ops,
	},
	{
		.name = "SIFS1",
		.id = ABOX_SIFS1,
		.capture = {
			.stream_name = "SIFS1 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.ops = &sifs_dai_ops,
	},
	{
		.name = "SIFS2",
		.id = ABOX_SIFS2,
		.capture = {
			.stream_name = "SIFS2 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.ops = &sifs_dai_ops,
	},
	{
		.name = "SIFS3",
		.id = ABOX_SIFS3,
		.capture = {
			.stream_name = "SIFS3 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.ops = &sifs_dai_ops,
	},
	{
		.name = "SIFS4",
		.id = ABOX_SIFS4,
		.capture = {
			.stream_name = "SIFS4 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.ops = &sifs_dai_ops,
	},
	{
		.name = "SIFS5",
		.id = ABOX_SIFS5,
		.capture = {
			.stream_name = "SIFS5 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.ops = &sifs_dai_ops,
	},
	{
		.name = "NSRC0",
		.id = ABOX_NSRC0,
		.playback = {
			.stream_name = "NSRC0 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.capture = {
			.stream_name = "NSRC0 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.ops = &src_dai_ops,
	},
	{
		.name = "NSRC1",
		.id = ABOX_NSRC1,
		.playback = {
			.stream_name = "NSRC1 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.capture = {
			.stream_name = "NSRC1 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.ops = &src_dai_ops,
	},
	{
		.name = "NSRC2",
		.id = ABOX_NSRC2,
		.playback = {
			.stream_name = "NSRC2 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.capture = {
			.stream_name = "NSRC2 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.ops = &src_dai_ops,
	},
	{
		.name = "NSRC3",
		.id = ABOX_NSRC3,
		.playback = {
			.stream_name = "NSRC3 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.capture = {
			.stream_name = "NSRC3 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.ops = &src_dai_ops,
	},
	{
		.name = "NSRC4",
		.id = ABOX_NSRC4,
		.playback = {
			.stream_name = "NSRC4 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.capture = {
			.stream_name = "NSRC4 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.ops = &src_dai_ops,
	},
	{
		.name = "NSRC5",
		.id = ABOX_NSRC5,
		.playback = {
			.stream_name = "NSRC5 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.capture = {
			.stream_name = "NSRC5 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.ops = &src_dai_ops,
	},
	{
		.name = "NSRC6",
		.id = ABOX_NSRC6,
		.playback = {
			.stream_name = "NSRC6 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.capture = {
			.stream_name = "NSRC6 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.ops = &src_dai_ops,
	},
	{
		.name = "NSRC7",
		.id = ABOX_NSRC7,
		.playback = {
			.stream_name = "NSRC7 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.capture = {
			.stream_name = "NSRC7 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.ops = &src_dai_ops,
	},
	{
		.name = "USB",
		.id = ABOX_USB,
		.playback = {
			.stream_name = "USB Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.capture = {
			.stream_name = "USB Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
	},
	{
		.name = "FWD",
		.id = ABOX_FWD,
		.playback = {
			.stream_name = "FWD Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.capture = {
			.stream_name = "FWD Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
	},
};

int abox_cmpnt_update_cnt_val(struct device *adev)
{
	/* nothing to do anymore */
	return 0;
}

int abox_cmpnt_hw_params_fixup_helper(struct snd_soc_pcm_runtime *rtd,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct device *dev = dai->dev;
	int ret;

	abox_dbg(dev, "%s(%s)\n", __func__, dai->name);

	switch (dai->id) {
	case ABOX_RDMA0:
	case ABOX_RDMA1:
	case ABOX_RDMA2:
	case ABOX_RDMA3:
	case ABOX_RDMA4:
	case ABOX_RDMA5:
	case ABOX_RDMA6:
	case ABOX_RDMA7:
	case ABOX_RDMA8:
	case ABOX_RDMA9:
	case ABOX_RDMA10:
	case ABOX_RDMA11:
		ret = rdma_hw_params_fixup(dai, params);
		break;
	case ABOX_UAIF0:
	case ABOX_UAIF1:
	case ABOX_UAIF2:
	case ABOX_UAIF3:
	case ABOX_UAIF4:
	case ABOX_UAIF5:
	case ABOX_UAIF6:
	case ABOX_SPDY:
		ret = abox_if_hw_params_fixup(dai, params);
		break;
	case ABOX_SIFS0:
	case ABOX_SIFS1:
	case ABOX_SIFS2:
	case ABOX_SIFS3:
	case ABOX_SIFS4:
	case ABOX_SIFS5:
		ret = sifs_hw_params_fixup(dai, params);
		break;
	case ABOX_NSRC0:
	case ABOX_NSRC1:
	case ABOX_NSRC2:
	case ABOX_NSRC3:
	case ABOX_NSRC4:
	case ABOX_NSRC5:
	case ABOX_NSRC6:
	case ABOX_NSRC7:
		ret = sifm_hw_params_fixup(dai, params);
		break;
	case ABOX_RDMA0_BE:
	case ABOX_RDMA1_BE:
	case ABOX_RDMA2_BE:
	case ABOX_RDMA3_BE:
	case ABOX_RDMA4_BE:
	case ABOX_RDMA5_BE:
	case ABOX_RDMA6_BE:
	case ABOX_RDMA7_BE:
	case ABOX_RDMA8_BE:
	case ABOX_RDMA9_BE:
	case ABOX_RDMA10_BE:
	case ABOX_RDMA11_BE:
		ret = rdma_be_hw_params_fixup(rtd, params);
		break;
	case ABOX_WDMA0_BE:
	case ABOX_WDMA1_BE:
	case ABOX_WDMA2_BE:
	case ABOX_WDMA3_BE:
	case ABOX_WDMA4_BE:
	case ABOX_WDMA5_BE:
	case ABOX_WDMA6_BE:
	case ABOX_WDMA7_BE:
		ret = wdma_be_hw_params_fixup(rtd, params);
		break;
	default:
		abox_err(dev, "invalid hw_params fixup request\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int register_if_routes(struct device *dev,
		const struct snd_soc_dapm_route *route_base, int num,
		struct snd_soc_dapm_context *dapm, const char *name)
{
	struct snd_soc_dapm_route *route;
	int i;

	route = devm_kmemdup(dev, route_base, sizeof(*route_base) * num,
			GFP_KERNEL);
	if (!route)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		if (route[i].sink)
			route[i].sink = devm_kasprintf(dev, GFP_KERNEL,
					route[i].sink, name);
		if (route[i].control)
			route[i].control = devm_kasprintf(dev, GFP_KERNEL,
					route[i].control, name);
		if (route[i].source)
			route[i].source = devm_kasprintf(dev, GFP_KERNEL,
					route[i].source, name);
	}

	snd_soc_dapm_add_routes(dapm, route, num);
	devm_kfree(dev, route);

	return 0;
}

static const struct snd_soc_dapm_route route_base_if_pla[] = {
	/* sink, control, source */
	{"%s Playback", NULL, "%s PLA"},
};

static const struct snd_soc_dapm_route route_base_if_cap[] = {
	/* sink, control, source */
	{"SPUSM", "%s", "%s CAP"},
	{"SPUST", "%s", "%s CAP"},
	{"%s CAP", "%s Switch", "%s Capture"},
};

int abox_cmpnt_register_if(struct device *dev_abox,
		struct device *dev, unsigned int id, const char *name,
		bool playback, bool capture)
{
	struct abox_data *data = dev_get_drvdata(dev_abox);
	struct snd_soc_dapm_context *dapm;
	int ret;

	if (id >= ARRAY_SIZE(data->dev_if)) {
		abox_err(dev, "%s: invalid id(%u)\n", __func__, id);
		return -EINVAL;
	}

	dapm = snd_soc_component_get_dapm(data->cmpnt);

	data->dev_if[id] = dev;
	if (id > data->if_count)
		data->if_count = id + 1;

	if (playback) {
		ret = register_if_routes(dev, route_base_if_pla,
				ARRAY_SIZE(route_base_if_pla), dapm, name);
		if (ret < 0)
			return ret;
	}

	if (capture) {
		ret = register_if_routes(dev, route_base_if_cap,
				ARRAY_SIZE(route_base_if_cap), dapm, name);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int register_rdma_routes(struct device *dev,
		const struct snd_soc_dapm_route *route_base, int num,
		struct snd_soc_dapm_context *dapm, const char *name, int id)
{
	struct snd_soc_dapm_route *route;
	int i;

	route = kmemdup(route_base, sizeof(*route_base) * num,
			GFP_KERNEL);
	if (!route)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		if (route[i].sink)
			route[i].sink = devm_kasprintf(dev, GFP_KERNEL,
					route[i].sink, id);
		if (route[i].control)
			route[i].control = devm_kasprintf(dev, GFP_KERNEL,
					route[i].control);
		if (route[i].source)
			route[i].source = devm_kasprintf(dev, GFP_KERNEL,
					route[i].source, name);
	}

	snd_soc_dapm_add_routes(dapm, route, num);
	kfree(route);

	return 0;
}

static const struct snd_soc_dapm_route route_base_rdma[] = {
	/* sink, control, source */
	{"SPUS IN%d", NULL, "%s Playback"},
};

int abox_cmpnt_register_rdma(struct device *dev_abox,
		struct device *dev, unsigned int id, const char *name)
{
	struct abox_data *data = dev_get_drvdata(dev_abox);
	struct snd_soc_component *cmpnt = data->cmpnt;
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(cmpnt);

	if (id >= ARRAY_SIZE(data->dev_rdma)) {
		abox_err(dev, "%s: invalid id(%u)\n", __func__, id);
		return -EINVAL;
	}

	data->dev_rdma[id] = dev;
	if (id > data->rdma_count)
		data->rdma_count = id + 1;

	return register_rdma_routes(dev, route_base_rdma, 1, dapm, name, id);
}

static int register_wdma_routes(struct device *dev,
		const struct snd_soc_dapm_route *route_base, int num,
		struct snd_soc_dapm_context *dapm, const char *name, int id)
{
	struct snd_soc_dapm_route *route;
	int i;

	route = kmemdup(route_base, sizeof(*route_base) * num,
			GFP_KERNEL);
	if (!route)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		if (route[i].sink)
			route[i].sink = devm_kasprintf(dev, GFP_KERNEL,
					route[i].sink, name);
		if (route[i].control)
			route[i].control = devm_kasprintf(dev, GFP_KERNEL,
					route[i].control);
		if (route[i].source)
			route[i].source = devm_kasprintf(dev, GFP_KERNEL,
					route[i].source, id);
	}

	snd_soc_dapm_add_routes(dapm, route, num);
	kfree(route);

	return 0;
}

static const struct snd_soc_dapm_route route_base_wdma[] = {
	/* sink, control, source */
	{"%s Capture", "WDMA", "SIFM%d"},
};

int abox_cmpnt_register_wdma(struct device *dev_abox,
		struct device *dev, unsigned int id, const char *name)
{
	struct abox_data *data = dev_get_drvdata(dev_abox);
	struct snd_soc_component *cmpnt = data->cmpnt;
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(cmpnt);

	if (id >= ARRAY_SIZE(data->dev_wdma)) {
		abox_err(dev, "%s: invalid id(%u)\n", __func__, id);
		return -EINVAL;
	}

	data->dev_wdma[id] = dev;
	if (id > data->wdma_count)
		data->wdma_count = id + 1;

	return register_wdma_routes(dev, route_base_wdma, 1, dapm, name, id);
}

int abox_cmpnt_restore(struct device *adev)
{
	struct abox_data *data = dev_get_drvdata(adev);
	int i;

	abox_dbg(adev, "%s\n", __func__);

	for (i = SET_SIFS0_RATE; i <= SET_SIFM7_RATE; i++)
		rate_put_ipc(adev, 0, i);
	for (i = SET_SIFS0_FORMAT; i <= SET_SIFM7_FORMAT; i++)
		format_put_ipc(adev, 0, 0, i);
	if (s_default)
		asrc_factor_put_ipc(adev, s_default, SET_ASRC_FACTOR_CP);
	if (data->audio_mode)
		audio_mode_put_ipc(adev, data->audio_mode);
	if (data->sound_type)
		sound_type_put_ipc(adev, data->sound_type);

	return 0;
}

int abox_cmpnt_register(struct device *adev)
{
	const struct snd_soc_component_driver *cmpnt_drv;
	struct snd_soc_dai_driver *dai_drv;
	int num_dai;

	cmpnt_drv = &abox_cmpnt_drv;
	dai_drv = abox_cmpnt_dai_drv;
	num_dai = ARRAY_SIZE(abox_cmpnt_dai_drv);

	return snd_soc_register_component(adev, cmpnt_drv, dai_drv, num_dai);
}
