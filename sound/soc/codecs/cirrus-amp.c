/*
 * Extended support for CS35L41 Amp
 *
 * Copyright 2017 Cirrus Logic
 *
 * Author:      David Rhodes    <david.rhodes@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/cirrus/core.h>
#include <sound/cirrus/big_data.h>
#include <sound/cirrus/calibration.h>
#include <sound/cirrus/power.h>

#include "wmfw.h"
#include "wm_adsp.h"

struct class *cirrus_amp_class;
EXPORT_SYMBOL_GPL(cirrus_amp_class);

struct cirrus_amp_group *amp_group;

struct cirrus_amp *cirrus_get_amp_from_suffix(const char *suffix)
{
	int i;

	if (amp_group == NULL || (amp_group->num_amps == 0))
		return NULL;

	pr_debug("%s: suffix = %s\n", __func__, suffix);

	for (i = 0; i < amp_group->num_amps; i++) {
		if (amp_group->amps[i].bd.bd_suffix) {
			pr_debug("%s: comparing %s & %s\n", __func__,
				 amp_group->amps[i].bd.bd_suffix, suffix);
			if (!strcmp(amp_group->amps[i].bd.bd_suffix, suffix))
				return &amp_group->amps[i];
		}
	}

	for (i = 0; i < amp_group->num_amps; i++) {
		pr_debug("%s: comparing %s & %s\n", __func__,
			 amp_group->amps[i].mfd_suffix, suffix);
		if (!strcmp(amp_group->amps[i].mfd_suffix, suffix))
			return &amp_group->amps[i];
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(cirrus_get_amp_from_suffix);

int cirrus_amp_add(const char *mfd_suffix, struct cirrus_amp_config cfg)
{
	struct cirrus_amp *amp = cirrus_get_amp_from_suffix(mfd_suffix);

	if (amp) {
		dev_info(amp_group->cal_dev,
			 "Amp added, suffix: %s dsp_part_name: %s\n",
			 mfd_suffix, cfg.dsp_part_name);

		amp->component = cfg.component;
		amp->regmap = cfg.regmap;
		amp->dsp_part_name = cfg.dsp_part_name;
		amp->num_pre_configs = cfg.num_pre_configs;
		amp->num_post_configs = cfg.num_post_configs;
		amp->mbox_cmd = cfg.mbox_cmd;
		amp->mbox_sts = cfg.mbox_sts;
		amp->global_en = cfg.global_en;
		amp->global_en_mask = cfg.global_en_mask;
		amp->vimon_alg_id = cfg.vimon_alg_id;
		amp->pwr.target_temp = cfg.target_temp;
		amp->pwr.exit_temp = cfg.exit_temp;
		amp->perform_vimon_cal = cfg.perform_vimon_cal;
		amp->calibration_disable = cfg.calibration_disable;
		amp->cal_vpk_id = cfg.cal_vpk_id ? cfg.cal_vpk_id :
					CIRRUS_CAL_RTLOG_ID_V_PEAK;
		amp->cal_ipk_id = cfg.cal_vpk_id ? cfg.cal_ipk_id :
					CIRRUS_CAL_RTLOG_ID_I_PEAK;
		amp->halo_alg_id = cfg.halo_alg_id ? cfg.halo_alg_id :
					CIRRUS_AMP_ALG_ID_HALO;
		amp->bd.max_temp_limit = cfg.bd_max_temp ?
						 cfg.bd_max_temp - 1 : 99;
		amp->default_redc = cfg.default_redc ? cfg.default_redc :
						CIRRUS_CAL_RDC_DEFAULT;

		amp->pre_config = kcalloc(cfg.num_pre_configs,
					  sizeof(struct reg_sequence),
					  GFP_KERNEL);

		amp->post_config = kcalloc(cfg.num_post_configs,
					   sizeof(struct reg_sequence),
					   GFP_KERNEL);

		amp->amp_reinit = cfg.amp_reinit;

		amp_group->pwr_enable |= cfg.pwr_enable;

		memcpy(amp->pre_config, cfg.pre_config,
		       sizeof(struct reg_sequence) * cfg.num_pre_configs);

		memcpy(amp->post_config, cfg.post_config,
		       sizeof(struct reg_sequence) * cfg.num_post_configs);
	} else {
		dev_err(amp_group->cal_dev,
			"No amp with suffix %s registered\n", mfd_suffix);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(cirrus_amp_add);

int cirrus_amp_read_ctl(struct cirrus_amp *amp, const char *name,
			int type, unsigned int id, unsigned int *value)
{
	struct wm_adsp *dsp = snd_soc_component_get_drvdata(amp->component);
	unsigned int tmp;
	int ret = 0;

	if (amp->component) {
		ret = wm_adsp_read_ctl(dsp, name, type, id, (void *)&tmp, 4);
		*value = (tmp & 0xff0000) >> 8 |
			(tmp & 0xff00) << 8 |
			(tmp & 0xff000000) >> 24;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(cirrus_amp_read_ctl);

int cirrus_amp_write_ctl(struct cirrus_amp *amp, const char *name,
			 int type, unsigned int id, unsigned int value)
{
	struct wm_adsp *dsp = snd_soc_component_get_drvdata(amp->component);
	unsigned int tmp;

	tmp = (value & 0xff0000) >> 8 |
			(value & 0xff00) << 8 |
			(value & 0xff000000) >> 24 |
			(value & 0xff) << 24;

	if (amp->component)
		return wm_adsp_write_ctl(dsp, name, type, id, (void *)&tmp, 4);

	return 0;
}
EXPORT_SYMBOL_GPL(cirrus_amp_write_ctl);

static const struct of_device_id cirrus_amp_of_match[] = {
	{ .compatible = "cirrus-amp", },
	{},
};
MODULE_DEVICE_TABLE(of, cirrus_amp_of_match);

static int cirrus_amp_probe(struct platform_device *pdev)
{
	struct device_node *np;
	struct device_node *amp_node;
	const char **mfd_suffixes;
	const char **bd_suffixes;
	bool v_val_separate[CIRRUS_MAX_AMPS];
	int ret = 0, num = 0, num_amps, i, j;

	cirrus_amp_class = class_create(THIS_MODULE, "cirrus");
	if (IS_ERR(cirrus_amp_class)) {
		ret = PTR_ERR(cirrus_amp_class);
		pr_err("%s: Unable to register cirrus_amp class (%d)",
		       __func__, ret);
		return ret;
	}

	for_each_matching_node(np, cirrus_amp_of_match)
		num++;

	if (num != 1) {
		pr_info("%s: Exactly 1 OF entry is allowed (%d detected)\n",
			__func__, num);
		ret = -EINVAL;
		goto class_err;
	}

	np = of_find_matching_node(NULL, cirrus_amp_of_match);
	if (!np) {
		pr_err("%s: Device node required\n", __func__);
		ret = -ENODEV;
		goto class_err;
	}

	num_amps = of_count_phandle_with_args(np, "cirrus,amps", NULL);
	if (num_amps <= 0) {
		pr_err("%s: Failed to parse 'cirrus,amps'\n", __func__);
		ret = -ENODEV;
		goto class_err;
	}

	amp_group = kzalloc(sizeof(struct cirrus_amp_group) +
			    sizeof(struct cirrus_amp) * num_amps,
			    GFP_KERNEL);
	mfd_suffixes = kcalloc(num_amps, sizeof(char *), GFP_KERNEL);
	bd_suffixes = kcalloc(num_amps, sizeof(char *), GFP_KERNEL);

	amp_group->num_amps = num_amps;

	for (i = 0; i < num_amps; i++) {
		amp_node = of_parse_phandle(np, "cirrus,amps", i);
		if (IS_ERR(amp_node)) {
			pr_err("%s: Failed to parse 'cirrus,amps' (%d)\n",
			       __func__, i);
			ret = PTR_ERR(amp_node);
			goto suffix_free;
		}

		pr_debug("%s: Found linked amp: %s\n", __func__,
			 amp_node->full_name);

		ret = of_property_read_string(amp_node, "cirrus,mfd-suffix",
					      &mfd_suffixes[i]);
		if (ret < 0) {
			pr_err("%s: No MFD suffix found for amp: %s\n",
			       __func__, amp_node->full_name);

			of_node_put(amp_node);
			goto suffix_free;
		}

		ret = of_property_read_string(amp_node, "cirrus,bd-suffix",
					      &bd_suffixes[i]);
		if (ret < 0)
			pr_debug("%s: No BD suffix found for amp: %s\n",
				 __func__, amp_node->full_name);

		v_val_separate[i] = of_property_read_bool(amp_node,
							"cirrus,v-val_separate");

		of_node_put(amp_node);
	}

	for (i = 0; i < num_amps; i++) {
		for (j = 0; j < num_amps; j++) {
			if (i == j)
				continue;

			if (strcmp(mfd_suffixes[i], mfd_suffixes[j]) == 0) {
				pr_err("%s: MFD suffixes must be unique\n",
				       __func__);
				pr_err("%s: Found duplicate suffix: %s\n",
				       __func__, mfd_suffixes[i]);

				ret = -EINVAL;
				goto suffix_err;
			}

			/* bd_suffixes can be empty but must be unique */
			if (bd_suffixes[i] && bd_suffixes[j] &&
			    (strcmp(bd_suffixes[i], bd_suffixes[j]) == 0)) {
				pr_err("%s: BD suffixes must be unique\n",
				       __func__);
				pr_err("%s: Found duplicate suffix: %s\n",
				       __func__, bd_suffixes[i]);

				ret = -EINVAL;
				goto suffix_err;
			}
		}

		pr_info("%s: Found MFD suffix: %s\n", __func__,
			mfd_suffixes[i]);

		amp_group->amps[i].mfd_suffix =
				kstrdup(mfd_suffixes[i], GFP_KERNEL);

		if (bd_suffixes[i]) {
			pr_info("%s: Found BD suffix: %s\n", __func__,
				bd_suffixes[i]);

			amp_group->amps[i].bd.bd_suffix =
					kstrdup(bd_suffixes[i], GFP_KERNEL);
		}

		if (v_val_separate[i])
			amp_group->amps[i].v_val_separate = true;
	}

	ret = cirrus_bd_init();
	if (ret < 0) {
		pr_err("%s: Error in BD init (%d)\n", __func__, ret);
		goto suffix_err;
	}

	ret = cirrus_cal_init();
	if (ret < 0) {
		pr_err("%s: Error in CAL init (%d)\n", __func__, ret);
		cirrus_bd_exit();
		goto suffix_err;
	}

	ret = cirrus_pwr_init();
	if (ret < 0) {
		pr_err("%s: Error in PWR init (%d)\n", __func__, ret);
		cirrus_bd_exit();
		cirrus_cal_exit();
		goto suffix_err;
	}

	ret = 0;

	goto suffix_free;

suffix_err:
	for (i = 0; i < num_amps; i++) {
		kfree(amp_group->amps[i].mfd_suffix);
		kfree(amp_group->amps[i].bd.bd_suffix);
	}

suffix_free:
	kfree(mfd_suffixes);
	kfree(bd_suffixes);

	if (ret < 0)
		kfree(amp_group);

class_err:
	if (ret < 0)
		class_destroy(cirrus_amp_class);

	return ret;
}

static int cirrus_amp_remove(struct platform_device *pdev)
{
	int i;

	cirrus_cal_exit();
	cirrus_bd_exit();
	cirrus_pwr_exit();

	for (i = 0; i < amp_group->num_amps; i++){
		kfree(amp_group->amps[i].pre_config);
		kfree(amp_group->amps[i].post_config);
	}

	kfree(amp_group);

	class_destroy(cirrus_amp_class);

	return 0;
}

static struct platform_driver cirrus_amp_driver = {
	.driver = {
		.name		= "cirrus-amp",
		.of_match_table = cirrus_amp_of_match,
	},
	.probe		= cirrus_amp_probe,
	.remove		= cirrus_amp_remove,
};
module_platform_driver(cirrus_amp_driver);

MODULE_AUTHOR("David Rhodes <David.Rhodes@cirrus.com>");
MODULE_DESCRIPTION("Cirrus Amp driver");
MODULE_LICENSE("GPL");

