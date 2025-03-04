/*
 * core.h  --  MFD includes for Cirrus Logic CS35L41 codecs
 *
 * Copyright 2017 Cirrus Logic
 *
 * Author:	David Rhodes	<david.rhodes@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CS35L41_MFD_CORE_H
#define CS35L41_MFD_CORE_H

#include <linux/regmap.h>
#include <linux/regulator/consumer.h>

#define CIRRUS_MAX_AMPS			8

#define CS35L41_ALG_ID_HALO	0x400a4
#define CIRRUS_AMP_ALG_ID_HALO	0x4fa00
#define CIRRUS_AMP_ALG_ID_CSPL	0xcd

extern struct class *cirrus_amp_class;

struct cs35l41_data {
	struct cs35l41_platform_data *pdata;
	struct device *dev;
	struct regmap *regmap;
	struct class *mfd_class;
	struct regulator_bulk_data supplies[2];
	struct gpio_desc *reset_gpio;
	int num_supplies;
	int irq;
};

struct cirrus_amp_config {
	struct snd_soc_component *component;
	struct regmap *regmap;
	struct reg_sequence *pre_config;
	struct reg_sequence *post_config;
	const char *dsp_part_name;
	unsigned int num_pre_configs;
	unsigned int num_post_configs;
	unsigned int mbox_cmd;
	unsigned int mbox_sts;
	unsigned int global_en;
	unsigned int global_en_mask;
	unsigned int vimon_alg_id;
	unsigned int halo_alg_id;
	unsigned int bd_max_temp;
	unsigned int target_temp;
	unsigned int exit_temp;
	unsigned int default_redc;
	unsigned int cal_vpk_id;
	unsigned int cal_ipk_id;
	bool perform_vimon_cal;
	bool calibration_disable;
	bool pwr_enable;
	int (*amp_reinit)(struct snd_soc_component *component);
};

struct cirrus_bd {
	const char *bd_suffix;
	unsigned int max_exc;
	unsigned int over_exc_count;
	unsigned int max_temp;
	unsigned int max_temp_keep;
	unsigned int over_temp_count;
	unsigned int abnm_mute;
	int max_temp_limit;
};

struct cirrus_cal {
	unsigned int efs_cache_rdc;
	unsigned int efs_cache_vsc;
	unsigned int efs_cache_isc;
	unsigned int v_validation;
	unsigned int dsp_input1_cache;
	unsigned int dsp_input2_cache;
	int efs_cache_valid;
};

struct cirrus_pwr {
	unsigned int target_temp;
	unsigned int exit_temp;
	unsigned int amb_temp;
	unsigned int spk_temp;
	unsigned int passport_enable;
	bool amp_active;
};

struct cirrus_amp {
	struct regmap *regmap;
	struct snd_soc_component *component;
	struct cirrus_bd bd;
	struct cirrus_cal cal;
	struct cirrus_pwr pwr;
	struct reg_sequence *pre_config;
	struct reg_sequence *post_config;
	const char *dsp_part_name;
	const char *mfd_suffix;
	unsigned int num_pre_configs;
	unsigned int num_post_configs;
	unsigned int mbox_cmd;
	unsigned int mbox_sts;
	unsigned int global_en;
	unsigned int global_en_mask;
	unsigned int vimon_alg_id;
	unsigned int halo_alg_id;
	unsigned int default_redc;
	unsigned int cal_vpk_id;
	unsigned int cal_ipk_id;
	int index;
	bool perform_vimon_cal;
	bool calibration_disable;
	bool v_val_separate;
	int (*amp_reinit)(struct snd_soc_component *component);
};

struct cirrus_amp_group {
	struct device *bd_dev;
	struct device *cal_dev;
	struct device *pwr_dev;
	struct mutex cal_lock;
	struct mutex pwr_lock;
	struct delayed_work cal_complete_work;
	struct delayed_work pwr_work;
	struct workqueue_struct *pwr_workqueue;
	unsigned long long int last_bd_update;
	unsigned int efs_cache_temp;
	unsigned int uptime_ms;
	unsigned int interval;
	unsigned int status;
	unsigned int target_min_time_ms;
	unsigned int pwr_enable;
	bool cal_running;
	int cal_retry;
	unsigned int num_amps;
	struct cirrus_amp amps[];
};

struct cirrus_amp *cirrus_get_amp_from_suffix(const char *suffix);
int cirrus_amp_add(const char *mfd_suffix, struct cirrus_amp_config cfg);
int cirrus_amp_read_ctl(struct cirrus_amp *amp, const char *name,
			int type, unsigned int id, unsigned int *value);
int cirrus_amp_write_ctl(struct cirrus_amp *amp, const char *name,
			 int type, unsigned int id, unsigned int value);

int cs35l41_set_surface_temp(const char *suffix, int temperature);

extern struct cirrus_amp_group *amp_group;

#endif
