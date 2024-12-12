/*
 * Calibration support for Cirrus Logic CS35L41 codec
 *
 * Copyright 2017 Cirrus Logic
 *
 * Author:	David Rhodes	<david.rhodes@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <linux/firmware.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/fs.h>

#include <sound/cirrus/core.h>
#include <sound/cirrus/calibration.h>

#include "wmfw.h"
#include "wm_adsp.h"

enum cirrus_cspl_mboxstate {
	CSPL_MBOX_STS_RUNNING = 0,
	CSPL_MBOX_STS_PAUSED = 1,
	CSPL_MBOX_STS_RDY_FOR_REINIT = 2,
};

enum cirrus_cspl_mboxcmd {
	CSPL_MBOX_CMD_NONE = 0,
	CSPL_MBOX_CMD_PAUSE = 1,
	CSPL_MBOX_CMD_RESUME = 2,
	CSPL_MBOX_CMD_REINIT = 3,
	CSPL_MBOX_CMD_STOP_PRE_REINIT = 4,
	CSPL_MBOX_CMD_UNKNOWN_CMD = -1,
	CSPL_MBOX_CMD_INVALID_SEQUENCE = -2,
};

#define CIRRUS_CAL_VERSION "5.01.18"

#define CIRRUS_CAL_DIR_NAME			"cirrus_cal"
#define CIRRUS_CAL_CONFIG_FILENAME_SUFFIX	"-dsp1-spk-prot-calib.bin"
#define CIRRUS_CAL_PLAYBACK_FILENAME_SUFFIX	"-dsp1-spk-prot.bin"
#define CIRRUS_CAL_RDC_SAVE_LOCATION		"/efs/cirrus/rdc_cal"
#define CIRRUS_CAL_TEMP_SAVE_LOCATION		"/efs/cirrus/temp_cal"
#define CIRRUS_CAL_VSC_SAVE_LOCATION		"/efs/cirrus/vsc_cal"
#define CIRRUS_CAL_ISC_SAVE_LOCATION		"/efs/cirrus/isc_cal"

#define CS35L41_CAL_COMPLETE_DELAY_MS	1250
#define CIRRUS_CAL_RETRIES		2
#define CS35L40_CAL_AMBIENT_DEFAULT	23

int cirrus_cal_logger_get_variable(struct cirrus_amp *amp, unsigned int id,
				   unsigned int *result)
{
	unsigned int state;
	int retries = 100;

	cirrus_amp_write_ctl(amp, "RTLOG_COUNT", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, 0);
	cirrus_amp_write_ctl(amp, "RTLOG_VARIABLE", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, id);
	cirrus_amp_write_ctl(amp, "RTLOG_COUNT", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, 1);
	cirrus_amp_write_ctl(amp, "RTLOG_STATE", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, 0);
	cirrus_amp_write_ctl(amp, "RTLOG_ENABLE", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL,1);

	do {
		usleep_range(20, 50);
		cirrus_amp_read_ctl(amp, "RTLOG_STATE", WMFW_ADSP2_XM,
				    CIRRUS_AMP_ALG_ID_CSPL, &state);
	} while (state == 0 && --retries > 0);

	if (retries == 0) {
		dev_err(amp_group->cal_dev, "variable read failed\n");
		return -1;
	}

	cirrus_amp_read_ctl(amp, "RTLOG_DATA", WMFW_ADSP2_XM,
			    CIRRUS_AMP_ALG_ID_CSPL, result);
	cirrus_amp_write_ctl(amp, "RTLOG_ENABLE", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, 0);

	return 0;
}

static unsigned long long int cs35l41_rdc_to_ohms(unsigned long int rdc)
{
	return ((rdc * CS35L41_CAL_AMP_CONSTANT_NUM) /
		CS35L41_CAL_AMP_CONSTANT_DENOM);
}

static unsigned int cirrus_cal_vpk_to_mv(unsigned int vpk)
{
	return (vpk * CIRRUS_CAL_VFS_MV) >> 19;
}

static bool cirrus_cal_vsc_in_range(unsigned int vsc)
{
	return ((vsc <= CS35L41_VIMON_CAL_VSC_UB) ||
		(vsc >= CS35L41_VIMON_CAL_VSC_LB && vsc <= 0x00FFFFFF));
}

static bool cirrus_cal_isc_in_range(unsigned int isc)
{
	return ((isc <= CS35L41_VIMON_CAL_ISC_UB) ||
		(isc >= CS35L41_VIMON_CAL_ISC_LB && isc <= 0x00FFFFFF));
}

static int cirrus_cal_load_config(const char *file, struct cirrus_amp *amp)
{
	struct wm_adsp *dsp = snd_soc_component_get_drvdata(amp->component);
	int ret;

	dsp->firmwares[dsp->fw].fullname  = true;
	dsp->firmwares[dsp->fw].binfile = file;

	ret = wm_adsp_load_coeff(dsp);

	dsp->firmwares[dsp->fw].fullname  = false;
	dsp->firmwares[dsp->fw].binfile = NULL;

	return ret;
}

static int cirrus_cal_start(void);

static void cirrus_cal_complete_work(struct work_struct *work)
{
	struct cirrus_amp *amp;
	struct reg_sequence *post_config;
	struct regmap *regmap;
	const char *dsp_part_name;
	char *playback_config_filename;
	unsigned long long int ohms;
	unsigned int cal_state, mbox_cmd, mbox_sts;
	int rdc, status, checksum, temp, vsc, isc, timeout = 100, i;
	int delay = msecs_to_jiffies(CS35L41_CAL_COMPLETE_DELAY_MS);
	bool vsc_in_range, isc_in_range;
	bool cal_retry = false;

	mutex_lock(&amp_group->cal_lock);

	for (i = 0; i < amp_group->num_amps; i++) {
		amp = &amp_group->amps[i];
		if (amp->calibration_disable)
			continue;

		regmap = amp->regmap;
		dsp_part_name = amp->dsp_part_name;
		post_config = amp->post_config;
		mbox_cmd = amp->mbox_cmd;
		mbox_sts = amp->mbox_sts;

		playback_config_filename = kzalloc(PAGE_SIZE, GFP_KERNEL);
		snprintf(playback_config_filename, PAGE_SIZE, "%s%s",
			 dsp_part_name, CIRRUS_CAL_PLAYBACK_FILENAME_SUFFIX);

		cirrus_amp_read_ctl(amp, "CAL_STATUS", WMFW_ADSP2_XM,
				    CIRRUS_AMP_ALG_ID_CSPL, &status);
		cirrus_amp_read_ctl(amp, "CAL_R", WMFW_ADSP2_XM,
				    CIRRUS_AMP_ALG_ID_CSPL, &rdc);
		cirrus_amp_read_ctl(amp, "CAL_AMBIENT", WMFW_ADSP2_XM,
				    CIRRUS_AMP_ALG_ID_CSPL, &temp);
		cirrus_amp_read_ctl(amp, "CAL_CHECKSUM", WMFW_ADSP2_XM,
				    CIRRUS_AMP_ALG_ID_CSPL, &checksum);

		ohms = cs35l41_rdc_to_ohms((unsigned long int)rdc);

		cirrus_amp_read_ctl(amp, "CSPL_STATE", WMFW_ADSP2_XM,
				    CIRRUS_AMP_ALG_ID_CSPL, &cal_state);
		if (cal_state == CS35L41_CSPL_STATE_ERROR) {
			dev_err(amp_group->cal_dev,
			      "Error during ReDC cal, invalidating results\n");
			rdc = status = checksum = 0;
		}

		if (amp->perform_vimon_cal) {
			cirrus_amp_read_ctl(amp, "VSC", WMFW_ADSP2_XM,
					    amp->vimon_alg_id, &vsc);
			cirrus_amp_read_ctl(amp, "ISC", WMFW_ADSP2_XM,
					    amp->vimon_alg_id, &isc);
			cirrus_amp_read_ctl(amp, "VIMON_CAL_STATE",
					    WMFW_ADSP2_XM, amp->vimon_alg_id,
					    &cal_state);
			if (cal_state == CS35L41_CAL_VIMON_STATUS_INVALID ||
			    cal_state == 0) {
				dev_err(amp_group->cal_dev,
				      "Error during VIMON cal, invalidating results\n");
				rdc = status = checksum = 0;
			}

			vsc_in_range = cirrus_cal_vsc_in_range(vsc);
			isc_in_range = cirrus_cal_isc_in_range(isc);

			if (!vsc_in_range)
				dev_err(amp_group->cal_dev, "VIMON Cal cs35l41%s: VSC out of range (%x)\n",
					amp->mfd_suffix, vsc);
			if (!isc_in_range)
				dev_err(amp_group->cal_dev, "VIMON Cal cs35l41%s: ISC out of range (%x)\n",
					amp->mfd_suffix, isc);
			if (!vsc_in_range || !isc_in_range) {
				dev_err(amp_group->cal_dev, "VIMON cal out of range, invalidating results\n");
				rdc = status = checksum = 0;

				cirrus_amp_write_ctl(amp, "VIMON_CAL_STATE",
					WMFW_ADSP2_XM, amp->vimon_alg_id,
					CS35L41_CAL_VIMON_STATUS_INVALID);

				if (amp_group->cal_retry < CIRRUS_CAL_RETRIES) {
					dev_info(amp_group->cal_dev, "Retry Calibration\n");
					cal_retry = true;
				}
			}
		} else {
			vsc = 0;
			isc = 0;
			cirrus_amp_write_ctl(amp, "VIMON_CAL_STATE",
				WMFW_ADSP2_XM, amp->vimon_alg_id,
				CS35L41_CAL_VIMON_STATUS_INVALID);
		}

		dev_info(amp_group->cal_dev,
			 "Calibration finished: amp%s\n", amp->mfd_suffix);
		dev_info(amp_group->cal_dev, "Duration:\t%d ms\n",
			 CS35L41_CAL_COMPLETE_DELAY_MS);
		dev_info(amp_group->cal_dev, "Status:\t%d\n", status);
		if (status == CS35L41_CSPL_STATUS_OUT_OF_RANGE)
			dev_err(amp_group->cal_dev,
				"Calibration out of range\n");
		if (status == CS35L41_CSPL_STATUS_INCOMPLETE)
			dev_err(amp_group->cal_dev, "Calibration incomplete\n");
		dev_info(amp_group->cal_dev, "R :\t\t%d (%llu.%04llu Ohms)\n",
			 rdc, ohms >> CS35L41_CAL_RDC_RADIX,
			 (ohms & (((1 << CS35L41_CAL_RDC_RADIX) - 1))) *
			 10000 / (1 << CS35L41_CAL_RDC_RADIX));
		dev_info(amp_group->cal_dev, "Checksum:\t%d\n", checksum);
		dev_info(amp_group->cal_dev, "Ambient:\t%d\n", temp);

		usleep_range(5000, 5500);

		/* Send STOP_PRE_REINIT command and poll for response */
		regmap_write(regmap, mbox_cmd, CSPL_MBOX_CMD_STOP_PRE_REINIT);
		timeout = 100;
		do {
			dev_info(amp_group->cal_dev,
				 "waiting for REINIT ready...\n");
			usleep_range(1000, 1500);
			regmap_read(regmap, mbox_sts, &cal_state);
		} while ((cal_state != CSPL_MBOX_STS_RDY_FOR_REINIT) &&
				--timeout > 0);

		usleep_range(5000, 5500);

		cirrus_cal_load_config(playback_config_filename, amp);

		cirrus_amp_write_ctl(amp, "CAL_STATUS", WMFW_ADSP2_XM,
				     CIRRUS_AMP_ALG_ID_CSPL, status);
		cirrus_amp_write_ctl(amp, "CAL_R", WMFW_ADSP2_XM,
				     CIRRUS_AMP_ALG_ID_CSPL, rdc);
		cirrus_amp_write_ctl(amp, "CAL_AMBIENT", WMFW_ADSP2_XM,
				     CIRRUS_AMP_ALG_ID_CSPL, temp);
		cirrus_amp_write_ctl(amp, "CAL_CHECKSUM", WMFW_ADSP2_XM,
				     CIRRUS_AMP_ALG_ID_CSPL, checksum);

		/* Send REINIT command and poll for response */
		regmap_write(regmap, mbox_cmd, CSPL_MBOX_CMD_REINIT);
		timeout = 100;
		do {
			dev_info(amp_group->cal_dev,
				 "waiting for REINIT done...\n");
			usleep_range(1000, 1500);
			regmap_read(regmap, mbox_sts, &cal_state);

		} while ((cal_state != CSPL_MBOX_STS_RUNNING) &&
				--timeout > 0);

		cirrus_amp_read_ctl(amp, "CSPL_STATE", WMFW_ADSP2_XM,
				    CIRRUS_AMP_ALG_ID_CSPL, &cal_state);
		if (cal_state == CS35L41_CSPL_STATE_ERROR)
			dev_err(amp_group->cal_dev,
				"Playback config load error\n");

		regmap_multi_reg_write(regmap, post_config,
				       amp->num_post_configs);

		amp->cal.efs_cache_rdc = rdc;
		amp->cal.efs_cache_vsc = vsc;
		amp->cal.efs_cache_isc = isc;
		amp_group->efs_cache_temp = temp;
		amp->cal.efs_cache_valid = 1;

		kfree(playback_config_filename);
	}

	if (cal_retry == true) {
		cirrus_cal_start();
		queue_delayed_work(system_unbound_wq,
			&amp_group->cal_complete_work,
			delay);
		amp_group->cal_retry++;
	} else {
		amp_group->cal_running = 0;
	}

	dev_dbg(amp_group->cal_dev, "Calibration complete\n");
	mutex_unlock(&amp_group->cal_lock);
}

static void cirrus_cal_v_val_complete(struct cirrus_amp *amps, int num_amps,
					bool separate)
{
	struct regmap *regmap;
	struct reg_sequence *post_config;
	const char *dsp_part_name;
	char *playback_config_filename;
	unsigned int mbox_cmd, mbox_sts, cal_state;
	int timeout = 100, amp;

	for (amp = 0; amp < num_amps; amp++) {
		if (amps[amp].v_val_separate && !separate) continue;
		regmap = amps[amp].regmap;
		dsp_part_name = amps[amp].dsp_part_name;
		post_config = amps[amp].post_config;
		mbox_cmd = amps[amp].mbox_cmd;
		mbox_sts = amps[amp].mbox_sts;

		playback_config_filename = kzalloc(PAGE_SIZE, GFP_KERNEL);
		snprintf(playback_config_filename, PAGE_SIZE, "%s%s",
			 dsp_part_name, CIRRUS_CAL_PLAYBACK_FILENAME_SUFFIX);

		/* Send STOP_PRE_REINIT command and poll for response */
		regmap_write(regmap, mbox_cmd, CSPL_MBOX_CMD_STOP_PRE_REINIT);
		timeout = 100;
		do {
			dev_info(amp_group->cal_dev,
				 "waiting for REINIT ready...\n");
			usleep_range(1000, 1500);
			regmap_read(regmap, mbox_sts, &cal_state);

		} while ((cal_state != CSPL_MBOX_STS_RDY_FOR_REINIT) &&
				--timeout > 0);

		usleep_range(5000, 5500);

		cirrus_cal_load_config(playback_config_filename,
				       &amp_group->amps[amp]);

		/* Send REINIT command and poll for response */
		regmap_write(regmap, mbox_cmd, CSPL_MBOX_CMD_REINIT);

		timeout = 100;
		do {
			dev_info(amp_group->cal_dev,
				 "waiting for REINIT done...\n");
			usleep_range(1000, 1500);
			regmap_read(regmap, mbox_sts, &cal_state);

		} while ((cal_state != CSPL_MBOX_STS_RUNNING) &&
				--timeout > 0);

		cirrus_amp_read_ctl(&amp_group->amps[amp], "CSPL_STATE",
				    WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL,
				    &cal_state);
		if (cal_state == CS35L41_CSPL_STATE_ERROR)
			dev_err(amp_group->cal_dev,
				"Playback config load error\n");

		regmap_multi_reg_write(regmap, post_config,
				       amps[amp].num_post_configs);

		kfree(playback_config_filename);
	}

	dev_info(amp_group->cal_dev, "V validation complete\n");
}

static int cirrus_cal_get_power_temp(void)
{
	union power_supply_propval value = {0};
	struct power_supply *psy;

	psy = power_supply_get_by_name("battery");
	if (!psy) {
		dev_warn(amp_group->cal_dev,
			 "failed to get battery, assuming %d\n",
			 CS35L40_CAL_AMBIENT_DEFAULT);
		return CS35L40_CAL_AMBIENT_DEFAULT;
	}

	power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &value);

	return DIV_ROUND_CLOSEST(value.intval, 10);
}

static void cirrus_cal_vimon_cal_start(struct cirrus_amp *amp)
{

	cirrus_amp_write_ctl(amp, "VIMON_CLASS_H_CAL_DELAY", WMFW_ADSP2_XM,
			     amp->vimon_alg_id, CS35L41_CLASSH_DELAY_50MS);
	cirrus_amp_write_ctl(amp, "VIMON_CLASS_D_CAL_DELAY", WMFW_ADSP2_XM,
			     amp->vimon_alg_id, CS35L41_CLASSD_DELAY_50MS);

	cirrus_amp_write_ctl(amp, "VIMON_CAL_STATE", WMFW_ADSP2_XM,
			     amp->vimon_alg_id, 0);
	cirrus_amp_write_ctl(amp, "HALO_HEARTBEAT", WMFW_ADSP2_XM,
			     amp->halo_alg_id, 0);
}

static int cirrus_cal_vimon_cal_complete(struct cirrus_amp *amp)
{
	unsigned int vimon_cal, vsc, isc;
	bool vsc_in_range, isc_in_range;

	cirrus_amp_read_ctl(amp, "VIMON_CAL_STATE", WMFW_ADSP2_XM,
			    amp->vimon_alg_id, &vimon_cal);
	cirrus_amp_read_ctl(amp, "VSC", WMFW_ADSP2_XM, amp->vimon_alg_id, &vsc);
	cirrus_amp_read_ctl(amp, "ISC", WMFW_ADSP2_XM, amp->vimon_alg_id, &isc);

	dev_info(amp_group->cal_dev,
		 "VIMON Cal results cs35l41%s, status=%d vsc=%x isc=%x\n",
		 amp->mfd_suffix, vimon_cal, vsc, isc);

	vsc_in_range = cirrus_cal_vsc_in_range(vsc);
	isc_in_range = cirrus_cal_isc_in_range(isc);

	if (!vsc_in_range || !isc_in_range)
		vimon_cal = CS35L41_CAL_VIMON_STATUS_INVALID;

	return vimon_cal;
}

static int cirrus_cal_wait_for_active(struct cirrus_amp *amp)
{
	struct regmap *regmap = amp->regmap;
	unsigned int global_en;
	unsigned int halo_state;
	int timeout = 50;

	regmap_read(regmap, amp->global_en, &global_en);

	while ((global_en & amp->global_en_mask) == 0) {
		usleep_range(1000, 1500);
		regmap_read(regmap, amp->global_en, &global_en);
	}

	do {
		dev_info(amp_group->cal_dev, "waiting for HALO start...\n");

		msleep(16);

		cirrus_amp_read_ctl(amp, "HALO_STATE", WMFW_ADSP2_XM,
				    amp->halo_alg_id, &halo_state);
		timeout--;
	} while ((halo_state == 0) && timeout > 0);

	if (timeout == 0) {
		dev_err(amp_group->cal_dev, "Failed to setup calibration\n");
		return -EINVAL;
	}

	return 0;
}

static void cirrus_cal_redc_start(struct cirrus_amp *amp)
{
	struct regmap *regmap = amp->regmap;
	const char *dsp_part_name = amp->dsp_part_name;
	char *cal_config_filename;
	unsigned int halo_state;
	int timeout = 50;
	int ambient;

	cal_config_filename = kzalloc(PAGE_SIZE, GFP_KERNEL);
	snprintf(cal_config_filename, PAGE_SIZE, "%s%s", dsp_part_name,
		 CIRRUS_CAL_CONFIG_FILENAME_SUFFIX);

	dev_info(amp_group->cal_dev, "ReDC Calibration load start\n");

	/* Send STOP_PRE_REINIT command and poll for response */
	regmap_write(regmap, amp->mbox_cmd, CSPL_MBOX_CMD_STOP_PRE_REINIT);
	timeout = 100;
	do {
		dev_info(amp_group->cal_dev, "waiting for REINIT ready...\n");
		usleep_range(1000, 1500);
		regmap_read(regmap, amp->mbox_sts, &halo_state);
	} while ((halo_state != CSPL_MBOX_STS_RDY_FOR_REINIT) &&
			--timeout > 0);

	dev_dbg(amp_group->cal_dev, "load %s\n", dsp_part_name);
	cirrus_cal_load_config(cal_config_filename, amp);

	ambient = cirrus_cal_get_power_temp();
	cirrus_amp_write_ctl(amp, "CAL_AMBIENT", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, ambient);

	/* Send REINIT command and poll for response */
	regmap_write(regmap, amp->mbox_cmd, CSPL_MBOX_CMD_REINIT);
	timeout = 100;
	do {
		dev_info(amp_group->cal_dev, "waiting for REINIT done...\n");
		usleep_range(1000, 1500);
		regmap_read(regmap, amp->mbox_sts, &halo_state);
	} while ((halo_state != CSPL_MBOX_STS_RUNNING) &&
			--timeout > 0);

	kfree(cal_config_filename);
}

int cirrus_cal_apply(const char *mfd_suffix)
{
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(mfd_suffix);
	unsigned int temp, rdc, status, checksum, vsc, isc;
	unsigned int vimon_cal_status = CS35L41_CAL_VIMON_STATUS_SUCCESS;
	int ret = 0;

	if (!amp)
		return 0;

	if (amp->cal.efs_cache_valid == 1) {
		rdc = amp->cal.efs_cache_rdc;
		vsc = amp->cal.efs_cache_vsc;
		isc = amp->cal.efs_cache_isc;
		vimon_cal_status = CS35L41_CAL_VIMON_STATUS_SUCCESS;
		temp = amp_group->efs_cache_temp;
	} else {

		dev_info(amp_group->cal_dev,
				"No saved EFS, writing defaults\n");
		rdc = amp->default_redc;
		temp = CS35L40_CAL_AMBIENT_DEFAULT;
		vimon_cal_status = CS35L41_CAL_VIMON_STATUS_INVALID;
		amp->cal.efs_cache_rdc = rdc;
		amp_group->efs_cache_temp = temp;
	}

	status = 1;
	checksum = status + rdc;

	dev_info(amp_group->cal_dev, "Writing calibration to cs35l41%s\n",
		 mfd_suffix);

	dev_info(amp_group->cal_dev,
		 "RDC = %d, Temp = %d, Status = %d Checksum = %d\n",
		 rdc, temp, status, checksum);

	cirrus_amp_write_ctl(amp, "CAL_STATUS", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, status);
	cirrus_amp_write_ctl(amp, "CAL_R", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, rdc);
	cirrus_amp_write_ctl(amp, "CAL_AMBIENT", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, temp);
	cirrus_amp_write_ctl(amp, "CAL_CHECKSUM", WMFW_ADSP2_XM,
			     CIRRUS_AMP_ALG_ID_CSPL, checksum);

	if (!amp->perform_vimon_cal) {
		cirrus_amp_write_ctl(amp, "VIMON_CAL_STATE", WMFW_ADSP2_XM,
				     amp->vimon_alg_id,
				     CS35L41_CAL_VIMON_STATUS_INVALID);
		goto skip_vimon_cal;
	}

	cirrus_amp_write_ctl(amp, "VIMON_CAL_STATE", WMFW_ADSP2_XM,
			     amp->vimon_alg_id, vimon_cal_status);

	if (vimon_cal_status != CS35L41_CAL_VIMON_STATUS_INVALID) {
		dev_info(amp_group->cal_dev,
			 "VIMON Cal status=%d vsc=%x isc=%x\n",
			 vimon_cal_status, vsc, isc);
		cirrus_amp_write_ctl(amp, "VSC", WMFW_ADSP2_XM,
				     amp->vimon_alg_id, vsc);
		cirrus_amp_write_ctl(amp, "ISC", WMFW_ADSP2_XM,
				     amp->vimon_alg_id, isc);
	} else {
		dev_info(amp_group->cal_dev, "VIMON Cal status invalid\n");
	}

skip_vimon_cal:
	return ret;
}
EXPORT_SYMBOL_GPL(cirrus_cal_apply);

int cirrus_cal_read_temp(const char *mfd_suffix)
{
	struct cirrus_amp *amp;
	int reg = 0, ret;
	unsigned int halo_state;
	unsigned int global_en;

	amp = cirrus_get_amp_from_suffix(mfd_suffix);

	if (!amp)
		goto err;

	regmap_read(amp->regmap, amp->global_en, &global_en);

	if ((global_en & amp->global_en_mask) == 0)
		goto err;

	regmap_read(amp->regmap, amp->mbox_sts, &halo_state);

	if (halo_state != CSPL_MBOX_STS_RUNNING)
		goto err;

	if (amp_group->cal_running)
		goto err;

	ret = cirrus_cal_logger_get_variable(amp,
			CIRRUS_CAL_RTLOG_ID_TEMP,
			&reg);
	if (ret == 0) {
		if (reg == 0)
			cirrus_cal_logger_get_variable(amp,
				CIRRUS_CAL_RTLOG_ID_TEMP,
				&reg);
		dev_info(amp_group->cal_dev,
			"Read temp: %d.%04d degrees C\n",
			reg >> CIRRUS_CAL_RTLOG_RADIX_TEMP,
			(reg & (((1 << CIRRUS_CAL_RTLOG_RADIX_TEMP) - 1))) *
			10000 / (1 << CIRRUS_CAL_RTLOG_RADIX_TEMP));
		return (reg >> CIRRUS_CAL_RTLOG_RADIX_TEMP);
	}
err:
	return -1;
}
EXPORT_SYMBOL_GPL(cirrus_cal_read_temp);

static int cirrus_cal_start(void)
{
	int redc_cal_start_retries, vimon_cal_retries = 0;
	bool vimon_calibration_failed = false;
	unsigned int cal_state;
	int amp;
	struct reg_sequence *config;
	struct regmap *regmap;
	int ret;

	for (amp = 0; amp < amp_group->num_amps; amp++) {
		if (amp_group->amps[amp].calibration_disable)
			continue;

		regmap = amp_group->amps[amp].regmap;

		cirrus_amp_write_ctl(&amp_group->amps[amp], "CAL_STATUS",
			WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL, 0);
		cirrus_amp_write_ctl(&amp_group->amps[amp], "CAL_R",
			WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL, 0);
		cirrus_amp_write_ctl(&amp_group->amps[amp], "CAL_AMBIENT",
			WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL, 0);
		cirrus_amp_write_ctl(&amp_group->amps[amp], "CAL_CHECKSUM",
			WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL, 0);
		if (amp_group->amps[amp].perform_vimon_cal) {
			cirrus_amp_write_ctl(&amp_group->amps[amp], "VSC",
				WMFW_ADSP2_XM,
				amp_group->amps[amp].vimon_alg_id, 0);
			cirrus_amp_write_ctl(&amp_group->amps[amp], "ISC",
				WMFW_ADSP2_XM,
				amp_group->amps[amp].vimon_alg_id, 0);
		}

		ret = cirrus_cal_wait_for_active(&amp_group->amps[amp]);
		if (ret < 0) {
			dev_err(amp_group->cal_dev,
				"Could not start amp%s (%d)\n",
				amp_group->amps[amp].mfd_suffix, ret);
			return -ETIMEDOUT;
		}
	}

	do {
		vimon_calibration_failed = false;

		for (amp = 0; amp < amp_group->num_amps; amp++) {
			if (amp_group->amps[amp].calibration_disable)
				continue;

			regmap = amp_group->amps[amp].regmap;
			config = amp_group->amps[amp].pre_config;

			regmap_multi_reg_write(regmap, config,
					       amp_group->amps[amp].num_pre_configs);
			if (amp_group->amps[amp].perform_vimon_cal)
				cirrus_cal_vimon_cal_start(&amp_group->amps[amp]);
		}


		msleep(112);

		for (amp = 0; amp < amp_group->num_amps; amp++) {
			if (amp_group->amps[amp].calibration_disable)
				continue;

			if (amp_group->amps[amp].perform_vimon_cal) {
				ret = cirrus_cal_vimon_cal_complete(
							&amp_group->amps[amp]);

				if (ret != CS35L41_CAL_VIMON_STATUS_SUCCESS) {
					vimon_calibration_failed = true;
					dev_info(amp_group->cal_dev,
					  "VIMON Calibration Error cs35l41%s\n",
					  amp_group->amps[amp].mfd_suffix);
				}
			}
		}

		vimon_cal_retries--;
	} while (vimon_cal_retries >= 0 && vimon_calibration_failed);

	for (amp = 0; amp < amp_group->num_amps; amp++) {
		if (amp_group->amps[amp].calibration_disable)
			continue;

		regmap = amp_group->amps[amp].regmap;

		cirrus_cal_redc_start(&amp_group->amps[amp]);
		msleep(90);

		cirrus_amp_read_ctl(&amp_group->amps[amp], "CSPL_STATE",
			WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL, &cal_state);

		redc_cal_start_retries = 5;
		while (cal_state == CS35L41_CSPL_STATE_ERROR &&
						redc_cal_start_retries > 0) {
			if (cal_state == CS35L41_CSPL_STATE_ERROR)
				dev_err(amp_group->cal_dev,
					"Calibration load error\n");

			cirrus_cal_redc_start(&amp_group->amps[amp]);
			msleep(90);
			cirrus_amp_read_ctl(&amp_group->amps[amp], "CSPL_STATE",
				WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL, &cal_state);
			redc_cal_start_retries--;
		}

		if (redc_cal_start_retries == 0) {
			config = amp_group->amps[amp].post_config;

			dev_err(amp_group->cal_dev,
				"Calibration setup fail amp%s (%d)\n",
				amp_group->amps[amp].mfd_suffix, ret);

			regmap_multi_reg_write(regmap, config,
					amp_group->amps[amp].num_post_configs);
			return -ETIMEDOUT;
		}
	}

	return 0;
}

/***** SYSFS Interfaces *****/

static ssize_t cirrus_cal_version_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, CIRRUS_CAL_VERSION "\n");
}

static ssize_t cirrus_cal_version_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return 0;
}

static ssize_t cirrus_cal_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%s\n", amp_group->cal_running ?
			       "Enabled" : "Disabled");
}

static ssize_t cirrus_cal_status_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int ret = 0, prepare;

	if (amp_group->cal_running) {
		dev_err(amp_group->cal_dev,
			"cirrus_cal measurement in progress\n");
		return size;
	}

	mutex_lock(&amp_group->cal_lock);

	ret = kstrtos32(buf, 10, &prepare);
	if (ret != 0 || prepare != 1)
		goto err;

	amp_group->cal_running = true;
	amp_group->cal_retry = 0;

	cirrus_cal_start();

	dev_dbg(amp_group->cal_dev, "Calibration prepare complete\n");

	queue_delayed_work(system_unbound_wq, &amp_group->cal_complete_work,
			   msecs_to_jiffies(CS35L41_CAL_COMPLETE_DELAY_MS));

err:
	mutex_unlock(&amp_group->cal_lock);
	if (ret < 0)
		amp_group->cal_running = false;

	return size;
}

static ssize_t cirrus_cal_v_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%s\n", amp_group->cal_running ?
			       "Enabled" : "Disabled");
}

static ssize_t cirrus_cal_v_status_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct regmap *regmap;
	struct reg_sequence *config;
	unsigned int vmax[CIRRUS_MAX_AMPS];
	unsigned int vmin[CIRRUS_MAX_AMPS];
	unsigned int cal_state;
	int ret = 0, i, j, reg, prepare, retries, num_amps;
	const char *suffix;
	struct cirrus_amp *amps;
	bool separate = false;

	if (amp_group->cal_running) {
		dev_err(amp_group->cal_dev,
			"cirrus_cal measurement in progress\n");
		return size;
	}

	mutex_lock(&amp_group->cal_lock);

	ret = kstrtos32(buf, 10, &prepare);
	if (ret != 0 || prepare != 1)
		goto err;

	amp_group->cal_running = true;

	if (strlen(attr->attr.name) > strlen("v_status")) {
		suffix = &(attr->attr.name[strlen("v_status")]);
		amps = cirrus_get_amp_from_suffix(suffix);
		if (amps) {
			dev_info(dev, "V-validation for amp: cs35l41%s\n",
					suffix);
			num_amps = 1;
			separate = true;
		} else {
			mutex_unlock(&amp_group->cal_lock);
			return size;
		}
	} else {
		num_amps = amp_group->num_amps;
		amps = amp_group->amps;
		separate = false;
	}

	for (i = 0; i < amp_group->num_amps; i++) {
		if (amps[i].v_val_separate && !separate) continue;

		regmap = amps[i].regmap;
		config = amps[i].pre_config;

		vmax[i] = 0;
		vmin[i] = INT_MAX;

		ret = cirrus_cal_wait_for_active(&amps[i]);
		if (ret < 0) {
			dev_err(amp_group->cal_dev,
				"Could not start amp%s\n",
				amps[i].mfd_suffix);
			goto err;
		}

		regmap_multi_reg_write(regmap, config,
				       amps[i].num_pre_configs);

		cirrus_cal_redc_start(&amps[i]);
		msleep(90);

		cirrus_amp_read_ctl(&amps[i], "CSPL_STATE",
			WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL, &cal_state);

		retries = 5;
		while (cal_state == CS35L41_CSPL_STATE_ERROR && retries > 0) {
			if (cal_state == CS35L41_CSPL_STATE_ERROR)
				dev_err(amp_group->cal_dev,
					"Calibration load error\n");

			cirrus_cal_redc_start(&amps[i]);
			msleep(90);
			cirrus_amp_read_ctl(&amps[i], "CSPL_STATE",
				WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL, &cal_state);
			retries--;
		}

		if (retries == 0) {
			config = amps[i].post_config;
			dev_err(amp_group->cal_dev,
				"Calibration setup fail @ %d\n", i);
			regmap_multi_reg_write(regmap, config,
					amps[i].num_post_configs);
			goto err;
		}
	}

	dev_info(amp_group->cal_dev, "V validation prepare complete\n");

	for (i = 0; i < 100; i++) {
		for (j = 0; j < num_amps; j++) {
			if (amps[j].v_val_separate && !separate)
				continue;
			regmap = amps[j].regmap;
			cirrus_cal_logger_get_variable(&amps[j],
						amps[j].cal_vpk_id,
						&reg);
			if (reg > vmax[j])
				vmax[j] = reg;
			if (reg < vmin[j])
				vmin[j] = reg;
		}

		cirrus_amp_read_ctl(&amp_group->amps[0], "CAL_STATUS",
					WMFW_ADSP2_XM,
					CIRRUS_AMP_ALG_ID_CSPL, &reg);
		if (reg != 0 && reg != CS35L41_CSPL_STATUS_INCOMPLETE)
			break;
	}

	for (i = 0; i < num_amps; i++) {
		if (amps[i].v_val_separate && !separate) continue;
		dev_info(amp_group->cal_dev,
			"V Validation results for amp%s\n",
			amps[i].mfd_suffix);

		dev_dbg(amp_group->cal_dev, "V Max: 0x%x\n", vmax[i]);
		vmax[i] = cirrus_cal_vpk_to_mv(vmax[i]);
		dev_info(amp_group->cal_dev, "V Max: %d mV\n", vmax[i]);

		dev_dbg(amp_group->cal_dev, "V Min: 0x%x\n", vmin[i]);
		vmin[i] = cirrus_cal_vpk_to_mv(vmin[i]);
		dev_info(amp_group->cal_dev, "V Min: %d mV\n", vmin[i]);

		if (vmax[i] < CIRRUS_CAL_V_VAL_UB_MV &&
		    vmax[i] > CIRRUS_CAL_V_VAL_LB_MV) {
			amps[i].cal.v_validation = 1;
			dev_info(amp_group->cal_dev,
				 "V validation success\n");
		} else {
			amps[i].cal.v_validation = 0xCC;
			dev_err(amp_group->cal_dev,
				"V validation failed\n");
		}

	}

	cirrus_cal_v_val_complete(amps, num_amps, separate);

err:
	amp_group->cal_running = false;
	mutex_unlock(&amp_group->cal_lock);

	return size;
}

#ifdef CONFIG_SND_SOC_CS35L41_REINIT_SYSFS
static ssize_t cirrus_cal_reinit_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "\n");
}

static ssize_t cirrus_cal_reinit_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int reinit, i;
	int ret = kstrtos32(buf, 10, &reinit);

	if (amp_group->cal_running) {
		dev_err(amp_group->cal_dev,
			"cirrus_cal measurement in progress\n");
		return size;
	}

	if (ret == 0 && reinit == 1) {
		mutex_lock(&amp_group->cal_lock);

		for (i = 0; i < amp_group->num_amps; i++) {
			if (amp_group->amps[i].amp_reinit != NULL)
				amp_group->amps[i].amp_reinit(
					amp_group->amps[i].component);
		}

		mutex_unlock(&amp_group->cal_lock);
	}

	return size;
}
#endif /* CONFIG_SND_SOC_CS35L41_REINIT_SYSFS*/

static ssize_t cirrus_cal_vval_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	const char *suffix = &(attr->attr.name[strlen("v_validation")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);
	dev_info(dev, "%s\n", __func__);

	return sprintf(buf, "%d", amp->cal.v_validation);
}

static ssize_t cirrus_cal_vval_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	dev_info(dev, "%s\n", __func__);
	return 0;
}

static ssize_t cirrus_cal_rdc_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned int rdc;
	const char *suffix = &(attr->attr.name[strlen("rdc")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (amp) {
		rdc = amp->cal.efs_cache_rdc;
		return sprintf(buf, "%d", rdc);
	} else
		return 0;
}

static ssize_t cirrus_cal_rdc_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int rdc, ret;
	const char *suffix = &(attr->attr.name[strlen("rdc")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	ret = kstrtos32(buf, 10, &rdc);
	if (ret == 0 && amp) {
		if (rdc < 0) {
			amp->cal.efs_cache_vsc = 0;
			amp->cal.efs_cache_isc = 0;
			amp->cal.efs_cache_rdc = 0;
			amp->cal.efs_cache_valid = 0;
			return size;
		}

		amp->cal.efs_cache_rdc = rdc;

		dev_info(dev, "EFS Cache RDC set: 0x%x\n", rdc);
		if (amp->cal.efs_cache_rdc && amp_group->efs_cache_temp &&
				amp->cal.efs_cache_vsc &&
				amp->cal.efs_cache_isc)
			amp->cal.efs_cache_valid = 1;
	}
	return size;
}

static ssize_t cirrus_cal_vsc_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned int vsc;
	const char *suffix = &(attr->attr.name[strlen("vsc")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (amp) {
		vsc = amp->cal.efs_cache_vsc;
		return sprintf(buf, "%d", vsc);
	} else
		return 0;
}

static ssize_t cirrus_cal_vsc_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int vsc, ret;
	const char *suffix = &(attr->attr.name[strlen("vsc")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	ret = kstrtos32(buf, 10, &vsc);
	if (ret == 0 && amp) {
		if (vsc < 0) {
			amp->cal.efs_cache_vsc = 0;
			amp->cal.efs_cache_isc = 0;
			amp->cal.efs_cache_rdc = 0;
			amp->cal.efs_cache_valid = 0;
			return size;
		}

		amp->cal.efs_cache_vsc = vsc;

		dev_info(dev, "EFS Cache VSC set: 0x%x\n", vsc);
		if (amp->cal.efs_cache_rdc && amp_group->efs_cache_temp &&
				amp->cal.efs_cache_vsc &&
				amp->cal.efs_cache_isc)
			amp->cal.efs_cache_valid = 1;
	}
	return size;
}

static ssize_t cirrus_cal_isc_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned int isc;
	const char *suffix = &(attr->attr.name[strlen("isc")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (amp) {
		isc = amp->cal.efs_cache_isc;
		return sprintf(buf, "%d", isc);
	} else
		return 0;
}

static ssize_t cirrus_cal_isc_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int isc, ret;
	const char *suffix = &(attr->attr.name[strlen("isc")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	ret = kstrtos32(buf, 10, &isc);
	if (ret == 0 && amp) {
		if (isc < 0) {
			amp->cal.efs_cache_vsc = 0;
			amp->cal.efs_cache_isc = 0;
			amp->cal.efs_cache_rdc = 0;
			amp->cal.efs_cache_valid = 0;
			return size;
		}

		amp->cal.efs_cache_isc = isc;

		dev_info(dev, "EFS Cache ISC set: 0x%x\n", isc);
		if (amp->cal.efs_cache_rdc && amp_group->efs_cache_temp &&
				amp->cal.efs_cache_vsc &&
				amp->cal.efs_cache_isc)
			amp->cal.efs_cache_valid = 1;
	}
	return size;
}

static ssize_t cirrus_cal_temp_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned int temp;
	const char *suffix = &(attr->attr.name[strlen("temp")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (amp) {
		temp = amp_group->efs_cache_temp;
		return sprintf(buf, "%d", temp);
	} else
		return 0;
}

static ssize_t cirrus_cal_temp_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int temp, ret;
	const char *suffix = &(attr->attr.name[strlen("temp")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	ret = kstrtos32(buf, 10, &temp);
	if (ret == 0 && amp) {
		amp_group->efs_cache_temp = temp;

		dev_info(dev, "EFS Cache temp set: %d\n", temp);
		if (amp->cal.efs_cache_rdc && amp_group->efs_cache_temp &&
				amp->cal.efs_cache_vsc &&
				amp->cal.efs_cache_isc)
			amp->cal.efs_cache_valid = 1;
	}
	return size;
}

static ssize_t cirrus_cal_checksum_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned int checksum;
	const char *suffix = &(attr->attr.name[strlen("checksum")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (amp) {
		cirrus_amp_read_ctl(amp, "CAL_CHECKSUM",
				WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL, &checksum);
		return sprintf(buf, "%d", checksum);
	} else
		return 0;
}

static ssize_t cirrus_cal_checksum_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int checksum, ret;
	const char *suffix = &(attr->attr.name[strlen("checksum")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	ret = kstrtos32(buf, 10, &checksum);
	if (ret == 0 && amp)
		cirrus_amp_write_ctl(amp, "CAL_CHECKSUM",
				WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL, checksum);
	return size;
}

static ssize_t cirrus_cal_set_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned int set_status;
	const char *suffix = &(attr->attr.name[strlen("set_status")]);
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(suffix);

	if (amp) {
		cirrus_amp_read_ctl(amp, "CAL_SET_STATUS",
				WMFW_ADSP2_XM, CIRRUS_AMP_ALG_ID_CSPL, &set_status);
		return sprintf(buf, "%d", set_status);
	} else
		return 0;
}

static ssize_t cirrus_cal_set_status_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return 0;
}

static DEVICE_ATTR(version, 0444, cirrus_cal_version_show,
				cirrus_cal_version_store);
static DEVICE_ATTR(status, 0664, cirrus_cal_status_show,
				cirrus_cal_status_store);
static DEVICE_ATTR(v_status, 0664, cirrus_cal_v_status_show,
				cirrus_cal_v_status_store);
#ifdef CONFIG_SND_SOC_CS35L41_REINIT_SYSFS
static DEVICE_ATTR(reinit, 0664, cirrus_cal_reinit_show,
				cirrus_cal_reinit_store);
#endif /* CONFIG_SND_SOC_CS35L41_REINIT_SYSFS */

static struct device_attribute v_val_attribute = {
	.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0664)},
	.show = cirrus_cal_v_status_show,
	.store = cirrus_cal_v_status_store,
};

static struct device_attribute generic_amp_attrs[CIRRUS_CAL_NUM_ATTRS_AMP] = {
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0444)},
		.show = cirrus_cal_vval_show,
		.store = cirrus_cal_vval_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0664)},
		.show = cirrus_cal_rdc_show,
		.store = cirrus_cal_rdc_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0664)},
		.show = cirrus_cal_vsc_show,
		.store = cirrus_cal_vsc_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0664)},
		.show = cirrus_cal_isc_show,
		.store = cirrus_cal_isc_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0664)},
		.show = cirrus_cal_temp_show,
		.store = cirrus_cal_temp_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0664)},
		.show = cirrus_cal_checksum_show,
		.store = cirrus_cal_checksum_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0444)},
		.show = cirrus_cal_set_status_show,
		.store = cirrus_cal_set_status_store,
	},
};

static const char *generic_amp_attr_names[CIRRUS_CAL_NUM_ATTRS_AMP] = {
	"v_validation",
	"rdc",
	"vsc",
	"isc",
	"temp",
	"checksum",
	"set_status"
};

static struct attribute *cirrus_cal_attr_base[] = {
	&dev_attr_version.attr,
	&dev_attr_status.attr,
	&dev_attr_v_status.attr,
#ifdef CONFIG_SND_SOC_CS35L41_REINIT_SYSFS
	&dev_attr_reinit.attr,
#endif /* CONFIG_SND_SOC_CS35L41_REINIT_SYSFS */
	NULL,
};

/* Kernel does not allow attributes to be dynamically allocated */
static struct attribute_group cirrus_cal_attr_grp;
static struct device_attribute
		amp_attrs_prealloc[CIRRUS_MAX_AMPS][CIRRUS_CAL_NUM_ATTRS_AMP];
static char attr_names_prealloc[CIRRUS_MAX_AMPS][CIRRUS_CAL_NUM_ATTRS_AMP][20];
static char v_val_attr_names_prealloc[CIRRUS_MAX_AMPS][20];
static struct device_attribute v_val_attrs_prealloc[CIRRUS_MAX_AMPS];

struct device_attribute *cirrus_cal_create_amp_attrs(const char *mfd_suffix,
							int index)
{
	struct device_attribute *amp_attrs_new;
	int i, suffix_len = strlen(mfd_suffix);

	if (index >= CIRRUS_MAX_AMPS)
		return NULL;

	amp_attrs_new = &(amp_attrs_prealloc[index][0]);

	memcpy(amp_attrs_new, &generic_amp_attrs,
		sizeof(struct device_attribute) *
		CIRRUS_CAL_NUM_ATTRS_AMP);

	for (i = 0; i < CIRRUS_CAL_NUM_ATTRS_AMP; i++) {
		amp_attrs_new[i].attr.name = attr_names_prealloc[index][i];
		snprintf((char *)amp_attrs_new[i].attr.name,
			strlen(generic_amp_attr_names[i]) + suffix_len + 1,
			"%s%s", generic_amp_attr_names[i], mfd_suffix);
	}

	return amp_attrs_new;
}

int cirrus_cal_init(void)
{
	struct device_attribute *new_attrs;
	int ret = 0, i, j, num_amps, v_val_num_attrs = 0;

	if (!amp_group) {
		pr_err("%s: Empty amp group\n", __func__);
		return -ENODATA;
	}

	amp_group->cal_dev = device_create(cirrus_amp_class, NULL, 1, NULL,
					   CIRRUS_CAL_DIR_NAME);
	if (IS_ERR(amp_group->cal_dev)) {
		ret = PTR_ERR(amp_group->cal_dev);
		pr_err("%s: Failed to create CAL device (%d)\n", __func__, ret);
		return ret;
	}

	dev_set_drvdata(amp_group->cal_dev, amp_group);

	num_amps = amp_group->num_amps;

	for (i = 0; i < num_amps; i++) {
		if (amp_group->amps[i].v_val_separate)
			v_val_num_attrs++;
	}

	cirrus_cal_attr_grp.attrs = kzalloc(sizeof(struct attribute *) *
					(CIRRUS_CAL_NUM_ATTRS_AMP * num_amps +
					v_val_num_attrs +
					CIRRUS_CAL_NUM_ATTRS_BASE + 1),
								GFP_KERNEL);
	for (i = 0; i < num_amps; i++) {
		new_attrs = cirrus_cal_create_amp_attrs(
				    amp_group->amps[i].mfd_suffix, i);
		for (j = 0; j < CIRRUS_CAL_NUM_ATTRS_AMP; j++) {
			dev_dbg(amp_group->cal_dev, "New attribute: %s\n",
				new_attrs[j].attr.name);
			cirrus_cal_attr_grp.attrs[i * CIRRUS_CAL_NUM_ATTRS_AMP
						  + j] = &new_attrs[j].attr;
		}
	}

	for (i = j = 0; i < num_amps; i++) {
		if (amp_group->amps[i].v_val_separate){
			memcpy(&v_val_attrs_prealloc[j],
				&v_val_attribute, sizeof(struct device_attribute));

			v_val_attrs_prealloc[j].attr.name =
				v_val_attr_names_prealloc[j];
			snprintf((char *)v_val_attrs_prealloc[j].attr.name,
				strlen("v_status") +
				strlen(amp_group->amps[i].mfd_suffix) + 1,
				"v_status%s", amp_group->amps[i].mfd_suffix);
			dev_info(amp_group->cal_dev, "New attribute: %s\n",
				v_val_attrs_prealloc[j].attr.name);
			cirrus_cal_attr_grp.attrs[num_amps * CIRRUS_CAL_NUM_ATTRS_AMP
						  + j] = &v_val_attrs_prealloc[j].attr;
			j++;
		}
	}

	memcpy(&cirrus_cal_attr_grp.attrs[num_amps * CIRRUS_CAL_NUM_ATTRS_AMP +
					v_val_num_attrs],
		cirrus_cal_attr_base, sizeof(struct attribute *) *
					CIRRUS_CAL_NUM_ATTRS_BASE);
	cirrus_cal_attr_grp.attrs[num_amps * CIRRUS_CAL_NUM_ATTRS_AMP +
			CIRRUS_CAL_NUM_ATTRS_BASE + v_val_num_attrs] = NULL;

	ret = sysfs_create_group(&amp_group->cal_dev->kobj,
				 &cirrus_cal_attr_grp);
	if (ret) {
		dev_err(amp_group->cal_dev, "Failed to create sysfs group\n");
		device_del(amp_group->bd_dev);
		return ret;
	}

	mutex_init(&amp_group->cal_lock);
	INIT_DELAYED_WORK(&amp_group->cal_complete_work,
			  cirrus_cal_complete_work);

	return ret;
}

void cirrus_cal_exit(void)
{
	flush_work(&amp_group->cal_complete_work.work);
	mutex_destroy(&amp_group->cal_lock);
	kfree(cirrus_cal_attr_grp.attrs);
	device_del(amp_group->bd_dev);
}


