/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/sched.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#include <exynos-is-module.h>
#include <exynos-is-sensor.h>
#include <exynos-is.h>
#include "is-config.h"
#include "is-dt.h"
#include "is-core.h"
#include "is-dvfs.h"
#include "is-interface-sensor.h"
#include "is-device-camif-dma.h"

#ifdef CONFIG_OF
static int get_pin_lookup_state(struct pinctrl *pinctrl,
	struct exynos_sensor_pin (*pin_ctrls)[GPIO_SCENARIO_MAX][GPIO_CTRL_MAX])
{
	int ret = 0;
	u32 i, j, k;
	char pin_name[30];
	struct pinctrl_state *s;

	for (i = 0; i < SENSOR_SCENARIO_MAX; ++i) {
		for (j = 0; j < GPIO_SCENARIO_MAX; ++j) {
			for (k = 0; k < GPIO_CTRL_MAX; ++k) {
				if (pin_ctrls[i][j][k].act == PIN_FUNCTION) {
					snprintf(pin_name, sizeof(pin_name), "%s%d",
						pin_ctrls[i][j][k].name,
						pin_ctrls[i][j][k].value);
					s = pinctrl_lookup_state(pinctrl, pin_name);
					if (IS_ERR_OR_NULL(s)) {
						err("pinctrl_lookup_state(%s) is failed", pin_name);
						ret = -EINVAL;
						goto p_err;
					}

					pin_ctrls[i][j][k].pin = (ulong)s;
				}
			}
		}
	}

p_err:
	return ret;
}

static int parse_gate_info(struct exynos_platform_is *pdata, struct device_node *np)
{
	int ret = 0;
	struct device_node *group_np = NULL;
	struct device_node *gate_info_np;
	struct property *prop;
	struct property *prop2;
	const __be32 *p;
	const char *s;
	u32 i = 0, u = 0;
	struct exynos_is_clk_gate_info *gate_info;

	/* get subip of is info */
	gate_info = kzalloc(sizeof(struct exynos_is_clk_gate_info), GFP_KERNEL);
	if (!gate_info) {
		printk(KERN_ERR "%s: no memory for is gate_info\n", __func__);
		return -EINVAL;
	}

	s = NULL;
	/* get gate register info */
	prop2 = of_find_property(np, "clk_gate_strs", NULL);
	of_property_for_each_u32(np, "clk_gate_enums", prop, p, u) {
		printk(KERN_INFO "int value: %d\n", u);
		s = of_prop_next_string(prop2, s);
		if (s != NULL) {
			printk(KERN_INFO "String value: %d-%s\n", u, s);
			gate_info->gate_str[u] = s;
		}
	}

	/* gate info */
	gate_info_np = of_get_child_by_name(np, "clk_gate_ctrl");
	if (!gate_info_np) {
		ret = -ENOENT;
		goto p_err;
	}
	i = 0;
	while ((group_np = of_get_next_child(gate_info_np, group_np))) {
		struct exynos_is_clk_gate_group *group =
				&gate_info->groups[i];
		of_property_for_each_u32(group_np, "mask_clk_on_org", prop, p, u) {
			printk(KERN_INFO "(%d) int1 value: %d\n", i, u);
			group->mask_clk_on_org |= (1 << u);
		}
		of_property_for_each_u32(group_np, "mask_clk_off_self_org", prop, p, u) {
			printk(KERN_INFO "(%d) int2 value: %d\n", i, u);
			group->mask_clk_off_self_org |= (1 << u);
		}
		of_property_for_each_u32(group_np, "mask_clk_off_depend", prop, p, u) {
			printk(KERN_INFO "(%d) int3 value: %d\n", i, u);
			group->mask_clk_off_depend |= (1 << u);
		}
		of_property_for_each_u32(group_np, "mask_cond_for_depend", prop, p, u) {
			printk(KERN_INFO "(%d) int4 value: %d\n", i, u);
			group->mask_cond_for_depend |= (1 << u);
		}
		i++;
		printk(KERN_INFO "(%d) [0x%x , 0x%x, 0x%x, 0x%x\n", i,
			group->mask_clk_on_org,
			group->mask_clk_off_self_org,
			group->mask_clk_off_depend,
			group->mask_cond_for_depend
		);
	}

	pdata->gate_info = gate_info;
	pdata->gate_info->clk_on_off = exynos_is_clk_gate;

	return 0;
p_err:
	kfree(gate_info);
	return ret;
}

#if defined(CONFIG_PM_DEVFREQ)
DECLARE_EXTERN_DVFS_DT(IS_SN_END);
static int parse_dvfs_data(struct exynos_platform_is *pdata, struct device_node *np, int index)
{
	int i;
	u32 temp;
	char *pprop;
	const char *name;

	pprop = __getname();
	if (unlikely(!pprop))
		return -ENOMEM;

	for (i = 0; i < IS_SN_END; i++) {
#if defined(QOS_TNR)
		snprintf(pprop, PATH_MAX, "%s%s", is_dvfs_dt_arr[i].parse_scenario_nm, "tnr");
		DT_READ_U32(np, pprop, pdata->dvfs_data[index][is_dvfs_dt_arr[i].scenario_id][IS_DVFS_TNR]);
#endif
#if defined(QOS_CSIS)
		snprintf(pprop, PATH_MAX, "%s%s", is_dvfs_dt_arr[i].parse_scenario_nm, "csis");
		DT_READ_U32(np, pprop, pdata->dvfs_data[index][is_dvfs_dt_arr[i].scenario_id][IS_DVFS_CSIS]);
#endif
#if defined(QOS_ISP)
		snprintf(pprop, PATH_MAX, "%s%s", is_dvfs_dt_arr[i].parse_scenario_nm, "isp");
		DT_READ_U32(np, pprop, pdata->dvfs_data[index][is_dvfs_dt_arr[i].scenario_id][IS_DVFS_ISP]);
#endif
#if defined(QOS_INTCAM)
		snprintf(pprop, PATH_MAX, "%s%s", is_dvfs_dt_arr[i].parse_scenario_nm, "int_cam");
		DT_READ_U32(np, pprop, pdata->dvfs_data[index][is_dvfs_dt_arr[i].scenario_id][IS_DVFS_INT_CAM]);
#endif
		snprintf(pprop, PATH_MAX, "%s%s", is_dvfs_dt_arr[i].parse_scenario_nm, "int");
		DT_READ_U32(np, pprop, pdata->dvfs_data[index][is_dvfs_dt_arr[i].scenario_id][IS_DVFS_INT]);

		snprintf(pprop, PATH_MAX, "%s%s", is_dvfs_dt_arr[i].parse_scenario_nm, "cam");
		DT_READ_U32(np, pprop, pdata->dvfs_data[index][is_dvfs_dt_arr[i].scenario_id][IS_DVFS_CAM]);

		snprintf(pprop, PATH_MAX, "%s%s", is_dvfs_dt_arr[i].parse_scenario_nm, "mif");
		DT_READ_U32(np, pprop, pdata->dvfs_data[index][is_dvfs_dt_arr[i].scenario_id][IS_DVFS_MIF]);

		snprintf(pprop, PATH_MAX, "%s%s", is_dvfs_dt_arr[i].parse_scenario_nm, "i2c");
		DT_READ_U32(np, pprop, pdata->dvfs_data[index][is_dvfs_dt_arr[i].scenario_id][IS_DVFS_I2C]);

		snprintf(pprop, PATH_MAX, "%s%s", is_dvfs_dt_arr[i].parse_scenario_nm, "hpg");
		DT_READ_U32(np, pprop, pdata->dvfs_data[index][is_dvfs_dt_arr[i].scenario_id][IS_DVFS_HPG]);

		snprintf(pprop, PATH_MAX, "%s%s", is_dvfs_dt_arr[i].parse_scenario_nm, "cpu");
		DT_READ_STR(np, pprop, pdata->dvfs_cpu[index][is_dvfs_dt_arr[i].scenario_id]);
	}
	__putname(pprop);

#ifdef DBG_DUMP_DVFS_DT
	for (i = 0; i < IS_SN_END; i++) {
		probe_info("---- %s ----\n", is_dvfs_dt_arr[i].parse_scenario_nm);
#if defined(QOS_TNR)
		probe_info("[%d][%d][TNR] = %d\n", index, i, pdata->dvfs_data[index][i][IS_DVFS_TNR]);
#endif
#if defined(QOS_CSIS)
		probe_info("[%d][%d][CSIS] = %d\n", index, i, pdata->dvfs_data[index][i][IS_DVFS_CSIS]);
#endif
#if defined(QOS_ISP)
		probe_info("[%d][%d][ISP] = %d\n", index, i, pdata->dvfs_data[index][i][IS_DVFS_ISP]);
#endif
#if defined(QOS_INTCAM)
		probe_info("[%d][%d][INT_CAM] = %d\n", index, i, pdata->dvfs_data[index][i][IS_DVFS_INT_CAM]);
#endif
		probe_info("[%d][%d][INT] = %d\n", index, i, pdata->dvfs_data[index][i][IS_DVFS_INT]);
		probe_info("[%d][%d][CAM] = %d\n", index, i, pdata->dvfs_data[index][i][IS_DVFS_CAM]);
		probe_info("[%d][%d][MIF] = %d\n", index, i, pdata->dvfs_data[index][i][IS_DVFS_MIF]);
		probe_info("[%d][%d][I2C] = %d\n", index, i, pdata->dvfs_data[index][i][IS_DVFS_I2C]);
		probe_info("[%d][%d][HPG] = %d\n", index, i, pdata->dvfs_data[index][i][IS_DVFS_HPG]);
		probe_info("[%d][%d][CPU] = %s\n", index, i, pdata->dvfs_cpu[index][i]);
	}
#endif
	return 0;
}
#else
static int parse_dvfs_data(struct exynos_platform_is *pdata, struct device_node *np, int index)
{
	return 0;
}
#endif

static int parse_dvfs_table(struct is_dvfs_ctrl *dvfs,
	struct exynos_platform_is *pdata, struct device_node *np)
{
	int ret = 0;
	u32 table_cnt;
	struct device_node *table_np;
	const char *dvfs_table_desc;

	table_np = NULL;

	table_cnt = 0;
	while ((table_np = of_get_next_child(np, table_np)) &&
		(table_cnt < IS_DVFS_TABLE_IDX_MAX)) {
		ret = of_property_read_string(table_np, "desc", &dvfs_table_desc);
		if (ret)
			dvfs_table_desc = "NOT defined";

		probe_info("dvfs table[%d] is %s", table_cnt, dvfs_table_desc);
		parse_dvfs_data(pdata, table_np, table_cnt);
		table_cnt++;
	}

	dvfs->dvfs_table_max = table_cnt;

	return ret;
}

static int bts_parse_data(struct device_node *np, struct is_bts_scen **data, int *num_bts_scen)
{
	int i;
	int ret = 0;
	int num_scen;
	struct is_bts_scen *bts_scen;

	if (of_have_populated_dt()) {
		num_scen = (unsigned int)of_property_count_strings(np, "list-scen-bts");
		if (num_scen <= 0) {
			probe_warn("There should be at least one scenario\n");
			goto err_of_property;
		}

		bts_scen = kcalloc(num_scen, sizeof(struct is_bts_scen), GFP_KERNEL);
		if (bts_scen == NULL) {
			ret = -ENOMEM;
			probe_err("no memory for bts_scen");
			goto err_alloc;
		}

		for (i = 0; i < num_scen; i++) {
			bts_scen[i].index = i;
			ret = of_property_read_string_index(np, "list-scen-bts", i, &(bts_scen[i].name));
			if (ret < 0) {
				probe_err("Unable to get name of bts scenarios\n");
				goto err_read_string;
			}
		}

		*data = bts_scen;
		*num_bts_scen = num_scen;
	} else {
		probe_warn("Invalid device tree node!\n");
	}

	return 0;

err_read_string:
	kfree(bts_scen);
err_alloc:
err_of_property:

	return ret;
}

static int parse_lic_offset_data(struct is_hardware *is_hw, struct device_node *dnode)
{
	int ret = 0;
	char *str_lic_ip;
	int elems;
	u32 offsets = LIC_CHAIN_OFFSET_NUM / 2 - 1;
	u32 set_idx = offsets + 1;
	int i, index_a, index_b;
	char *str_a, *str_b;

	str_lic_ip = __getname();
	if (unlikely(!str_lic_ip)) {
		err("[@] out of memory for str_lic_ip!");
		ret = -ENOMEM;
		goto err_alloc_str_lic_ip;
	}

	snprintf(str_lic_ip, PATH_MAX, "3AA");
	if (of_property_read_bool(dnode, str_lic_ip)) {
		elems = of_property_count_u32_elems(dnode, str_lic_ip);
		if (elems != LIC_CHAIN_OFFSET_NUM) {
			err("wrong LIC_CHAIN_OFFSET_NUM(%d!=%d)", elems, LIC_CHAIN_OFFSET_NUM);
			ret = -EINVAL;
			goto err_get_elems;
		}

		of_property_read_u32_array(dnode, str_lic_ip,
				&is_hw->lic_offset_def[0], elems);
	} else {
		err("[@] Can't fine %s node", str_lic_ip);
	}

	probe_info("[@] Parse_lic_offset_data\n");

	index_a = COREX_SETA * set_idx; /* setA */
	index_b = COREX_SETB * set_idx; /* setB */

	str_a = __getname();
	if (unlikely(!str_a)) {
		err("[@] out of memory for str_a!");
		ret = -ENOMEM;
		goto err_alloc_str_a;
	}

	str_b = __getname();
	if (unlikely(!str_b)) {
		err("[@] out of memory for str_b!");
		ret = -ENOMEM;
		goto err_alloc_str_b;
	}

	snprintf(str_a, PATH_MAX, "%d", is_hw->lic_offset_def[index_a + 0]);
	snprintf(str_b, PATH_MAX, "%d", is_hw->lic_offset_def[index_b + 0]);
	for (i = 1; i < offsets; i++) {
		snprintf(str_a, PATH_MAX, "%s, %d", str_a, is_hw->lic_offset_def[index_a + i]);
		snprintf(str_b, PATH_MAX, "%s, %d", str_b, is_hw->lic_offset_def[index_b + i]);
	}
	probe_info("[@] 3AA lic_offset_def: setA<%s>(%d), setB<%s>(%d)",
		str_a, is_hw->lic_offset_def[index_a + offsets],
		str_b, is_hw->lic_offset_def[index_b + offsets]);

	__putname(str_b);
err_alloc_str_b:
	__putname(str_a);
err_alloc_str_a:
err_get_elems:
	__putname(str_lic_ip);
err_alloc_str_lic_ip:

	return ret;
}

int is_parse_dt(struct platform_device *pdev)
{
	int ret = 0;
	struct is_core *core;
	struct is_dvfs_ctrl *dvfs;
	struct is_bts_scen **bts_scen;
	struct exynos_platform_is *pdata;
	struct device *dev;
	struct device_node *dvfs_np = NULL;
	struct device_node *vender_np = NULL;
	struct device_node *lic_offset_np = NULL;
	struct device_node *buffer_np = NULL;
	struct device_node *np;
	u32 mem_info[2];

	FIMC_BUG(!pdev);

	dev = &pdev->dev;
	np = dev->of_node;

	core = dev_get_drvdata(&pdev->dev);
	if (!core) {
		probe_err("core is NULL");
		return -ENOMEM;
	}

	pdata = kzalloc(sizeof(struct exynos_platform_is), GFP_KERNEL);
	if (!pdata) {
		probe_err("no memory for platform data");
		return -ENOMEM;
	}

	dvfs = &core->resourcemgr.dvfs_ctrl;
	bts_scen = &core->resourcemgr.bts_scen;
	pdata->clk_get = exynos_is_clk_get;
	pdata->clk_cfg = exynos_is_clk_cfg;
	pdata->clk_on = exynos_is_clk_on;
	pdata->clk_off = exynos_is_clk_off;
	pdata->print_clk = exynos_is_print_clk;

	of_property_read_u32(np, "num_of_3aa", &pdata->num_of_ip.taa);
	of_property_read_u32(np, "num_of_lme", &pdata->num_of_ip.lme);
	of_property_read_u32(np, "num_of_isp", &pdata->num_of_ip.isp);
	of_property_read_u32(np, "num_of_mcsc", &pdata->num_of_ip.mcsc);
	of_property_read_u32(np, "num_of_vra", &pdata->num_of_ip.vra);
	of_property_read_u32(np, "num_of_clh", &pdata->num_of_ip.clh);
	of_property_read_u32(np, "num_of_ypp", &pdata->num_of_ip.ypp);

	if (parse_gate_info(pdata, np) < 0)
		probe_info("can't parse clock gate info node");

	ret = of_property_read_u32(np, "chain_config", &core->chain_config);
	if (ret)
		probe_warn("chain configuration read is fail(%d)", ret);

	probe_info("FIMC-IS chain configuration: %d\n", core->chain_config);

	ret = of_property_read_u32_array(np, "secure_mem_info", mem_info, 2);
	if (ret) {
		core->secure_mem_info[0] = 0;
		core->secure_mem_info[1] = 0;
	} else {
		core->secure_mem_info[0] = mem_info[0];
		core->secure_mem_info[1] = mem_info[1];
	}
	probe_info("ret(%d) secure_mem_info(%#08lx, %#08lx)", ret,
		core->secure_mem_info[0], core->secure_mem_info[1]);

	ret = of_property_read_u32_array(np, "non_secure_mem_info", mem_info, 2);
	if (ret) {
		core->non_secure_mem_info[0] = 0;
		core->non_secure_mem_info[1] = 0;
	} else {
		core->non_secure_mem_info[0] = mem_info[0];
		core->non_secure_mem_info[1] = mem_info[1];
	}
	probe_info("ret(%d) non_secure_mem_info(%#08lx, %#08lx)", ret,
		core->non_secure_mem_info[0], core->non_secure_mem_info[1]);

	ret = of_property_read_u32_array(np, "memlog_size", pdata->is_memlog_size, 2);
	if (ret) {
		pdata->is_memlog_size[0] = IS_MEMLOG_PRINTF_DRV_SIZE;
		pdata->is_memlog_size[1] = IS_MEMLOG_PRINTF_DDK_SIZE;
	}
	probe_info("ret(%d) is_memlog_size(0x%#08lx, 0x%#08lx)", ret,
		pdata->is_memlog_size[0], pdata->is_memlog_size[1]);

	vender_np = of_get_child_by_name(np, "vender");
	if (vender_np) {
		ret = is_vender_dt(vender_np);
		if (ret)
			probe_err("is_vender_dt is fail(%d)", ret);
	}

	pdata->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pdata->pinctrl)) {
		probe_err("devm_pinctrl_get is fail");
		goto p_err;
	}

	dvfs_np = of_get_child_by_name(np, "is_dvfs");
	if (dvfs_np) {
		ret = parse_dvfs_table(dvfs, pdata, dvfs_np);
		if (ret)
			probe_err("parse_dvfs_table is fail(%d)", ret);
	}

	bts_parse_data(np, bts_scen, &core->resourcemgr.num_bts_scen);

	lic_offset_np = of_get_child_by_name(np, "lic_offsets");
	if (lic_offset_np) {
		ret = parse_lic_offset_data(&core->hardware, lic_offset_np);
		if (ret)
			probe_err("parse_lic_offset_data is fail(%d)", ret);
	}

	buffer_np = of_get_child_by_name(np, "buffer_info");
	if (buffer_np) {
		of_property_read_u32(buffer_np, "ypp_buf_max_width",
				&core->hardware.ypp_internal_buf_max_width);
	}

	dev->platform_data = pdata;

	return 0;

p_err:
	kfree(pdata);
	return ret;
}

int is_sensor_parse_dt(struct platform_device *pdev)
{
	int ret;
	struct exynos_platform_is_sensor *pdata;
	struct device_node *dnode;
	struct device *dev;

	FIMC_BUG(!pdev);
	FIMC_BUG(!pdev->dev.of_node);

	dev = &pdev->dev;
	dnode = dev->of_node;

	pdata = kzalloc(sizeof(struct exynos_platform_is_sensor), GFP_KERNEL);
	if (!pdata) {
		err("%s: no memory for platform data", __func__);
		return -ENOMEM;
	}

	pdata->iclk_cfg = exynos_is_sensor_iclk_cfg;
	pdata->iclk_on = exynos_is_sensor_iclk_on;
	pdata->iclk_off = exynos_is_sensor_iclk_off;
	pdata->mclk_on = exynos_is_sensor_mclk_on;
	pdata->mclk_off = exynos_is_sensor_mclk_off;
	pdata->mclk_force_off = is_sensor_mclk_force_off;

	ret = of_property_read_u32(dnode, "id", &pdata->id);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto err_read_id;
	}

	ret = of_property_read_u32(dnode, "scenario", &pdata->scenario);
	if (ret) {
		err("scenario read is fail(%d)", ret);
		goto err_read_scenario;
	}

	ret = of_property_read_u32(dnode, "csi_ch", &pdata->csi_ch);
	if (ret) {
		err("csi_ch read is fail(%d)", ret);
		goto err_read_csi_ch;
	}

	ret = of_property_read_u32(dnode, "wdma_ch_hint", &pdata->wdma_ch_hint);
	if (ret) {
		probe_info("skip wdma_ch_hint data read (%d)", ret);
		pdata->wdma_ch_hint = 0xFFFF;
	}

	ret = of_property_read_u32(dnode, "csi_mux", &pdata->csi_mux);
	if (ret) {
		probe_info("skip phy-csi mux data read (%d)", ret);
	}

	ret = of_property_read_u32(dnode, "multi_ch", &pdata->multi_ch);
	if (ret) {
		probe_info("skip multi_ch bool data read (%d)", ret);
	}

	ret = of_property_read_u32(dnode, "use_cphy", &pdata->use_cphy);
	if (ret)
		probe_info("use_cphy read is fail(%d)", ret);

	ret = of_property_read_u32(dnode, "scramble", &pdata->scramble);
	if (ret)
		probe_info("scramble read is fail(%d)", ret);

	if (of_property_read_bool(dnode, "use_ssvc0_internal"))
		set_bit(SUBDEV_SSVC0_INTERNAL_USE, &pdata->internal_state);

	if (of_property_read_bool(dnode, "use_ssvc1_internal"))
		set_bit(SUBDEV_SSVC1_INTERNAL_USE, &pdata->internal_state);

	if (of_property_read_bool(dnode, "use_ssvc2_internal"))
		set_bit(SUBDEV_SSVC2_INTERNAL_USE, &pdata->internal_state);

	if (of_property_read_bool(dnode, "use_ssvc3_internal"))
		set_bit(SUBDEV_SSVC3_INTERNAL_USE, &pdata->internal_state);

	ret = of_property_read_u32(dnode, "use_module_sel", &pdata->use_module_sel);
	if (ret)
		pdata->use_module_sel = MODULE_SEL_NONE;

	info("use_module_sel = %d\n", pdata->use_module_sel);

	ret = of_property_read_u32(dnode, "i2c_dummy_enable", &pdata->i2c_dummy_enable);
	if (ret)
		probe_info("i2c_dummy_enable is false(%d)", ret);

	/* used to Runtime module selection */
	if (pdata->use_module_sel) {
		int i;
		int identifier_cnt =
			of_property_count_elems_of_size(dnode, "module_sel_identifier", sizeof(u32));

		pdata->module_sel_cnt =
			of_property_count_elems_of_size(dnode, "module_sel_sensor_id", sizeof(u32));
		if (pdata->module_sel_cnt != identifier_cnt) {
			err("not matched cnt (module:%d, identifier:%d\n",
					pdata->module_sel_cnt, identifier_cnt - 1);
			goto err_read_is_bns;
		}

		ret = of_property_read_u32(dnode, "module_sel_idx", &pdata->module_sel_idx);
		if (ret)
			err("skip read module_sel_idx, read by EEPROM should be read");

		pdata->module_sel_sensor_id = devm_kzalloc(dev, sizeof(u32) * pdata->module_sel_cnt, GFP_KERNEL);
		if (!pdata->module_sel_sensor_id) {
			err("pdata->module_sel_sensor_id is NULL");
			ret = -ENOMEM;
			goto err_read_is_bns;
		}

		pdata->module_sel_val = devm_kzalloc(dev, sizeof(u32) * pdata->module_sel_cnt, GFP_KERNEL);
		if (!pdata->module_sel_sensor_id) {
			err("pdata->module_sel_val is NULL");
			ret = -ENOMEM;
			goto err_read_is_bns;
		}

		for (i = 0; i < pdata->module_sel_cnt; i++) {
			ret = of_property_read_u32_index(dnode, "module_sel_sensor_id",
								i, &pdata->module_sel_sensor_id[i]);
			if (ret) {
				err("read module_sel_sensor_id failed(%d)", ret);
				goto err_read_is_bns;
			}

			ret = of_property_read_u32_index(dnode, "module_sel_identifier",
								i, &pdata->module_sel_val[i]);
			if (ret) {
				err("read module_sel_val failed(%d)", ret);
				goto err_read_is_bns;
			}

			info("Runtime module sel(%d): sensor_id:%d, identifier:%d\n",
					i, pdata->module_sel_sensor_id[i], pdata->module_sel_val[i]);
		}
	}

	pdev->id = pdata->id;
	dev->platform_data = pdata;

	return 0;

err_read_is_bns:
err_read_csi_ch:
err_read_scenario:
err_read_id:
	kfree(pdata);

	return ret;
}

static int parse_af_data(struct exynos_platform_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->af_product_name);
	DT_READ_U32(dnode, "i2c_addr", pdata->af_i2c_addr);
	DT_READ_U32(dnode, "i2c_ch", pdata->af_i2c_ch);

	return 0;
}

static int parse_flash_data(struct exynos_platform_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->flash_product_name);
	DT_READ_U32(dnode, "flash_first_gpio", pdata->flash_first_gpio);
	DT_READ_U32(dnode, "flash_second_gpio", pdata->flash_second_gpio);

	return 0;
}

static int parse_ois_data(struct exynos_platform_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->ois_product_name);
	DT_READ_U32(dnode, "i2c_addr", pdata->ois_i2c_addr);
	DT_READ_U32(dnode, "i2c_ch", pdata->ois_i2c_ch);

	return 0;
}

static int parse_mcu_data(struct exynos_platform_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->mcu_product_name);
	DT_READ_U32(dnode, "i2c_addr", pdata->mcu_i2c_addr);
	DT_READ_U32(dnode, "i2c_ch", pdata->mcu_i2c_ch);

	return 0;
}

static int parse_aperture_data(struct exynos_platform_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->aperture_product_name);
	DT_READ_U32(dnode, "i2c_addr", pdata->aperture_i2c_addr);
	DT_READ_U32(dnode, "i2c_ch", pdata->aperture_i2c_ch);

	return 0;
}

static int parse_vc_extra_data(struct exynos_platform_is_module *pdata, struct device_node *dnode)
{
	int idx_stat;
	int idx_dt;
	char *str_stat;

	str_stat = __getname();
	if (unlikely(!str_stat)) {
		err("out of memory for str_stat.");
		return -ENOMEM;
	}

	for (idx_stat = 0; idx_stat < VC_BUF_DATA_TYPE_MAX; idx_stat++) {
		idx_dt = 0;
		snprintf(str_stat, PATH_MAX, "%s%d", "stat", idx_stat);
		if (!of_find_property(dnode, str_stat, NULL)) {
			pdata->vc_extra_info[idx_stat].stat_type = VC_STAT_TYPE_INVALID;
			pdata->vc_extra_info[idx_stat].sensor_mode = VC_SENSOR_MODE_INVALID;
			pdata->vc_extra_info[idx_stat].max_width = 0;
			pdata->vc_extra_info[idx_stat].max_height = 0;
			pdata->vc_extra_info[idx_stat].max_element = 0;
			continue;
		}

		of_property_read_u32_index(dnode, str_stat, idx_dt++, &pdata->vc_extra_info[idx_stat].stat_type);
		of_property_read_u32_index(dnode, str_stat, idx_dt++, &pdata->vc_extra_info[idx_stat].sensor_mode);
		of_property_read_u32_index(dnode, str_stat, idx_dt++, &pdata->vc_extra_info[idx_stat].max_width);
		of_property_read_u32_index(dnode, str_stat, idx_dt++, &pdata->vc_extra_info[idx_stat].max_height);
		of_property_read_u32_index(dnode, str_stat, idx_dt++, &pdata->vc_extra_info[idx_stat].max_element);
	}

	__putname(str_stat);

	return 0;
}

static int parse_modes_data(struct exynos_platform_is_module *pdata, struct device_node *dnode)
{
	int ret = 0;
	struct device_node *next, *opt_np;
	int idx_mode;
	int idx_vc;
	int idx_dt;
	char *str_vc;
	struct is_sensor_cfg *cfg;
	u32 format;
	u32 dummy_idx;
	u32 vc_num;
	u32 elems;

	str_vc = __getname();
	if (unlikely(!str_vc)) {
		err("out of memory for str_vc.");
		ret = -ENOMEM;
		goto err_alloc_str_vc;
	}

	pdata->cfgs = of_get_child_count(dnode);
	pdata->cfg = kcalloc(pdata->cfgs, sizeof(struct is_sensor_cfg), GFP_KERNEL);
	if (!pdata->cfg) {
		err("out of memory for sensor modes.");
		ret = -ENOMEM;
		goto err_alloc_sensor_mode;

	}

	idx_mode = 0;
	for_each_child_of_node(dnode, next) {
		idx_dt = 0;
		cfg = &pdata->cfg[idx_mode];
		idx_mode++;

		of_property_read_u32_index(next, "common", idx_dt++, &cfg->width);
		of_property_read_u32_index(next, "common", idx_dt++, &cfg->height);
		of_property_read_u32_index(next, "common", idx_dt++, &cfg->framerate);
		of_property_read_u32_index(next, "common", idx_dt++, &cfg->settle);
		of_property_read_u32_index(next, "common", idx_dt++, &cfg->mode);
		of_property_read_u32_index(next, "common", idx_dt++, &cfg->lanes);
		of_property_read_u32_index(next, "common", idx_dt++, &cfg->mipi_speed);
		of_property_read_u32_index(next, "common", idx_dt++, &cfg->interleave_mode);
		of_property_read_u32_index(next, "common", idx_dt++, &cfg->lrte);
		of_property_read_u32_index(next, "common", idx_dt++, &cfg->pd_mode);
		of_property_read_u32_index(next, "common", idx_dt++, (u32 *)&cfg->ex_mode);

		opt_np = of_get_child_by_name(next, "option");
		if (opt_np && of_find_property(opt_np, "votf", NULL))
			of_property_read_u32(opt_np, "votf", &cfg->votf);
		else
			cfg->votf = 0;

		if (opt_np && of_find_property(opt_np, "wdma_ch_hint", NULL)) {
			of_property_read_u32(opt_np, "wdma_ch_hint", &cfg->wdma_ch_hint);
		} else {
			cfg->wdma_ch_hint = 0xFFFF;
		}

		if (opt_np && of_find_property(opt_np, "max_fps", NULL))
			of_property_read_u32(opt_np, "max_fps", &cfg->max_fps);
		else
			cfg->max_fps = cfg->framerate;

		if (opt_np && of_find_property(opt_np, "binning", NULL))
			of_property_read_u32(opt_np, "binning", &cfg->binning);
		else
			cfg->binning = min(BINNING(pdata->active_width, cfg->width),
					BINNING(pdata->active_height, cfg->height));

		if (opt_np && of_find_property(opt_np, "vvalid_time", NULL))
			of_property_read_u32(opt_np, "vvalid_time", &cfg->vvalid_time);
		else
			cfg->vvalid_time = 0;

		if (opt_np && of_find_property(opt_np, "req_vvalid_time", NULL))
			of_property_read_u32(opt_np, "req_vvalid_time", &cfg->req_vvalid_time);
		else
			cfg->req_vvalid_time = 0;

		if (opt_np && of_find_property(opt_np, "special_mode", NULL))
			of_property_read_u32(opt_np, "special_mode", &cfg->special_mode);
		else
			cfg->special_mode = -1;

		for (idx_vc = 0; idx_vc < CSI_VIRTUAL_CH_MAX; idx_vc++)
			cfg->dummy_pixel[idx_vc] = 0;

		if (opt_np && of_find_property(opt_np, "dummy_pixel", NULL)) {
			elems = of_property_count_u32_elems(opt_np, "dummy_pixel");
			if (elems % SENSOR_DUMMY_ELEMS) {
				err("the length of dummy info. is not a multiple of SENSOR_DUMMY_ELEMS");
			} else {
				dummy_idx = 0;
				while (elems > 0) {
					of_property_read_u32_index(opt_np, "dummy_pixel", dummy_idx++, &vc_num);
					of_property_read_u32_index(opt_np, "dummy_pixel", dummy_idx++, &cfg->dummy_pixel[vc_num]);
					elems = elems - 2;
				}
			}
		}

		if (opt_np && of_find_property(opt_np, "fid_loc", NULL)) {
			of_property_read_u32_index(opt_np, "fid_loc", 0, &cfg->fid_loc.line);
			of_property_read_u32_index(opt_np, "fid_loc", 1, &cfg->fid_loc.byte);
			cfg->fid_loc.valid = 1;
		} else
			cfg->fid_loc.valid = 0;

		for (idx_vc = 0; idx_vc < CSI_VIRTUAL_CH_MAX; idx_vc++) {
			idx_dt = 0;
			snprintf(str_vc, PATH_MAX,  "%s%d", "vc", idx_vc);
			of_property_read_u32_index(next, str_vc, idx_dt++, &cfg->input[idx_vc].map);

			/* input format pasing */
			of_property_read_u32_index(next, str_vc, idx_dt++, &format);
			cfg->input[idx_vc].hwformat = format & HW_FORMAT_MASK;
			cfg->input[idx_vc].extformat = format & HW_EXT_FORMAT_MASK;

			of_property_read_u32_index(next, str_vc, idx_dt++, &cfg->input[idx_vc].width);
			of_property_read_u32_index(next, str_vc, idx_dt++, &cfg->input[idx_vc].height);

			/* output format pasing */
			of_property_read_u32_index(next, str_vc, idx_dt++, &format);
			cfg->output[idx_vc].hwformat = format & HW_FORMAT_MASK;
			cfg->output[idx_vc].extformat = format & HW_EXT_FORMAT_MASK;

			of_property_read_u32_index(next, str_vc, idx_dt++, &cfg->output[idx_vc].type);
			of_property_read_u32_index(next, str_vc, idx_dt++, &cfg->output[idx_vc].width);
			of_property_read_u32_index(next, str_vc, idx_dt++, &cfg->output[idx_vc].height);
		}
	}

err_alloc_sensor_mode:
	__putname(str_vc);
err_alloc_str_vc:
	return ret;
}
static int parse_power_seq_data(struct exynos_platform_is_module *pdata, struct device_node *dnode)
{
	int ret = 0;
	u32 temp;
	char *pprop;
	struct device_node *sn_np, *seq_np;
	struct is_core *core;
	struct device_node **node_table;
	struct device_node *temp_node;
	int num;
	long *node_num;
	long temp_num;
	int i, j;
	int gpio_mclk;

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		probe_err("core is NULL");
		return -EINVAL;
	}

	gpio_mclk = of_get_named_gpio(dnode, "gpio_mclk", 0);
	if (gpio_is_valid(gpio_mclk)) {
		if (gpio_request_one(gpio_mclk, GPIOF_OUT_INIT_LOW, "CAM_MCLK_OUTPUT_LOW")) {
			probe_err("%s: failed to gpio request mclk\n", dnode->name);
			return -ENODEV;
		}
		gpio_free(gpio_mclk);
	} else {
		probe_err("%s: failed to get mclk\n", dnode->name);
		return -EINVAL;
	}

	for_each_child_of_node(dnode, sn_np) {
		u32 sensor_scenario, gpio_scenario;

		DT_READ_U32(sn_np, "sensor_scenario", sensor_scenario);
		DT_READ_U32(sn_np, "gpio_scenario",gpio_scenario);

		probe_info("power_seq[%s] : sensor_scenario=%d, gpio_scenario=%d\n",
			sn_np->name, sensor_scenario, gpio_scenario);

		SET_PIN_INIT(pdata, sensor_scenario, gpio_scenario);

		num = of_get_child_count(sn_np);
		node_table = kcalloc(num, sizeof(*node_table), GFP_KERNEL);
		if (!node_table) {
			err("out of memory for node_table[%s].", sn_np->name);
			return -ENOMEM;
		}

		node_num = kcalloc(num, sizeof(*node_num), GFP_KERNEL);
		if (!node_num) {
			err("out of memory for node_num[%s].", sn_np->name);
			kfree(node_table);
			return -ENOMEM;
		}

		i = 0;
		for_each_child_of_node(sn_np, seq_np) {
			node_table[i] = seq_np;
			ret = kstrtol(seq_np->name, 10, &node_num[i]);
			if (ret) {
				kfree(node_table);
				kfree(node_num);
				err("fail to kstrtol [%d]%s:%s.", i, sn_np->name, seq_np->name);
				return ret;
			}
			i++;
		}

		/* sorting */
		for (i = 0; i < num; i++) {
			for (j = i + 1; j < num; j++) {
				if (node_num[i] > node_num[j]) {
					temp_node = node_table[i];
					temp_num = node_num[i];

					node_table[i] = node_table[j];
					node_num[i] = node_num[j];

					node_table[j] = temp_node;
					node_num[j] = temp_num;
				}
			}
		}

		for (i = 0; i < num; i++) {
			int gpio;
			int idx_share;
			int idx_seq;
			int idx_prop;
			struct exynos_sensor_pin *pin;
			struct property *prop;
			int length;

			prop = of_find_property(node_table[i], "pname", &length);
			if (!prop || length <= 1)
				continue;

			idx_seq = pdata->pinctrl_index[sensor_scenario][gpio_scenario];
			pin = &pdata->pin_ctrls[sensor_scenario][gpio_scenario][idx_seq];
			pdata->pinctrl_index[sensor_scenario][gpio_scenario]++;

			/* of_get_named_gpio does not guarantee about device tree
			 * format for gpio.
			 *
			 * ex) { pname = "pin"; pin = <PIN_OUTPUT 0 0>; gpio = <&gpp 1 0x1>; };
			 *
			 * If do not describe property of gpio correctly,
			 * of_get_named_gpio is returned zero value.
			 * therefore, this need to prevent.
			 * length is depends on cell(size is 4) count.
			 */
			prop = of_find_property(node_table[i], "gpio", &length);
			if (!prop || length < 12) {
				pin->pin = -1;
			} else {
				gpio = of_get_named_gpio(node_table[i], "gpio", 0);
				if (gpio_is_valid(gpio)) {
					pin->pin = gpio;
					gpio_request_one(gpio, GPIOF_OUT_INIT_LOW,
							"CAM_GPIO_OUTPUT_LOW");
					gpio_free(gpio);
				}
			}

			of_property_read_string(node_table[i], "pname", (const char **)&pin->name);

			idx_prop = 0;
			of_property_read_u32_index(node_table[i], "pin", idx_prop++, &pin->act);
			of_property_read_u32_index(node_table[i], "pin", idx_prop++, &pin->value);
			of_property_read_u32_index(node_table[i], "pin", idx_prop++, &pin->delay);
			of_property_read_u32_index(node_table[i], "pin", idx_prop++, &pin->voltage);

			idx_prop = 0;
			if (of_find_property(node_table[i], "share", NULL)) {
				of_property_read_u32_index(node_table[i], "share", idx_prop++, &pin->shared_rsc_type);
				of_property_read_u32_index(node_table[i], "share", idx_prop++, &idx_share);
				of_property_read_u32_index(node_table[i], "share", idx_prop++, &pin->shared_rsc_active);
				pin->shared_rsc_slock = &core->shared_rsc_slock[idx_share];
				pin->shared_rsc_count = &core->shared_rsc_count[idx_share];
			} else {
				pin->shared_rsc_type = 0;
				pin->shared_rsc_slock = NULL;
				pin->shared_rsc_count = NULL;
				pin->shared_rsc_active = 0;
			}

			if (of_find_property(node_table[i], "actuator_i2c_delay", NULL)) {
				of_property_read_u32(node_table[i], "actuator_i2c_delay", &pin->actuator_i2c_delay);
			}

			if (pin->act == PIN_REGULATOR) {
				bool reg_cap = true;
				struct is_module_regulator *is_module_regulator;
				struct device *dev = &core->pdev->dev;
				struct regulator *regulator;

				list_for_each_entry(is_module_regulator, &core->resourcemgr.regulator_list, list) {
					if (!strcmp(is_module_regulator->name, pin->name)) {
						reg_cap = false;
						break;
					}
				}

				if (reg_cap) {
					is_module_regulator = kzalloc(sizeof(struct is_module_regulator), GFP_KERNEL);
					if (!is_module_regulator) {
						err("%s: failed to alloc is_regulator", __func__);
						return -ENOMEM;
					}

					is_module_regulator->name = pin->name;
					list_add_tail(&is_module_regulator->list, &core->resourcemgr.regulator_list);
					dbg("%s is registered in regulator_list", is_module_regulator->name);

					if (sensor_scenario == SENSOR_SCENARIO_NORMAL) {
						/* If sensor voltage source is already enabled due to specific reason.
						 * Set regulator_force_disable, then maintain initial state
						 */
						regulator = regulator_get_optional(dev, pin->name);
						if (IS_ERR_OR_NULL(regulator)) {
							err("%s: failed to get regulator", __func__);
							return PTR_ERR(regulator);
						}

						if (regulator_is_enabled(regulator)) {
							warn("regulator(%s) is already enabled, forcely disable\n", pin->name);
							ret = regulator_force_disable(regulator);
							if (ret)
								err("regulator_force_disable fail(%d)\n", ret);
						}
						regulator_put(regulator);
					}
				}
			}

			dbg("%s: gpio=%d, name=%s, act=%d, val=%d, delay=%d, volt=%d, share=<%d %d %d>\n",
				node_table[i]->name,
				pin->pin,
				pin->name,
				pin->act,
				pin->value,
				pin->delay,
				pin->voltage,
				pin->shared_rsc_type,
				share_index,
				pin->shared_rsc_active);
		}

		kfree(node_table);
		kfree(node_num);
	}

	return 0;
}

static int parse_eeprom_data(struct exynos_platform_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->eeprom_product_name);
	DT_READ_U32(dnode, "i2c_addr", pdata->eeprom_i2c_addr);
	DT_READ_U32(dnode, "i2c_ch", pdata->eeprom_i2c_ch);

	return 0;
}

static int parse_laser_af_data(struct exynos_platform_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->laser_af_product_name);

	return 0;
}

static int parse_match_seq_data(struct exynos_platform_is_module *pdata, struct device_node *dnode)
{
	int ret = 0;
	struct device_node *group_np;
	struct is_core *core;
	int num_entry;
	int i, j;
	int idx_prop;
	struct exynos_sensor_module_match *entry;

	core = (struct is_core *)dev_get_drvdata(is_dev);
	if (!core) {
		probe_err("core is NULL");
		return -EINVAL;
	}

	/*
	 * A match seq node is divided into group and entry.
	 * Group can be configured as many as MATCH_GROUP_MAX,
	 * and each group can have as many entries as MATCH_ENTRY_MAX.
	 * Each entry consists of a slave_addr, reg offset, number of byte, and expected value
	 * to compare the result with i2c transfer.
	 */
	i = 0;
	for_each_child_of_node(dnode, group_np) {
		j = 0;
		num_entry = of_property_count_elems_of_size((group_np),
				"entry", sizeof(u32)) / 5;
		for (j = 0; j < num_entry; j++) {
			entry = &pdata->match_entry[i][j];
			idx_prop = j * 5;
			of_property_read_u32_index(group_np, "entry", idx_prop++, &entry->slave_addr);
			of_property_read_u32_index(group_np, "entry", idx_prop++, &entry->reg);
			of_property_read_u32_index(group_np, "entry", idx_prop++, &entry->reg_type);
			of_property_read_u32_index(group_np, "entry", idx_prop++, &entry->expected_data);
			of_property_read_u32_index(group_np, "entry", idx_prop++, &entry->data_type);
			probe_info("%s: slave_addr(0x%04x), reg(0x%04x), reg_type(%d), expected_data(0x%04x), data_type(%d)\n",
					__func__, entry->slave_addr, entry->reg, entry->reg_type, entry->expected_data, entry->data_type);
		}
		pdata->num_of_match_entry[i] = num_entry;
		i++;
	}
	pdata->num_of_match_groups = i;

	return ret;
}

int is_module_parse_dt(struct device *dev,
	is_moudle_callback module_callback)
{
	int ret = 0;
	struct exynos_platform_is_module *pdata;
	struct device_node *dnode;
	struct device_node *af_np;
	struct device_node *flash_np;
	struct device_node *ois_np;
	struct device_node *mcu_np;
	struct device_node *aperture_np;
	struct device_node *vc_extra_np;
	struct device_node *modes_np;
	struct device_node *power_np;
	struct device_node *eeprom_np;
	struct device_node *laser_af_np;
	struct device_node *match_np;
	u32 use = 0;

	FIMC_BUG(!dev);
	FIMC_BUG(!dev->of_node);

	dnode = dev->of_node;
	pdata = kzalloc(sizeof(struct exynos_platform_is_module), GFP_KERNEL);
	if (!pdata) {
		probe_err("%s: no memory for platform data", __func__);
		return -ENOMEM;
	}

	pdata->gpio_cfg = exynos_is_module_pins_cfg;
	pdata->gpio_dbg = exynos_is_module_pins_dbg;

	/* common data */
	ret = of_property_read_u32(dnode, "id", &pdata->id);
	if (ret) {
		probe_err("id read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "mclk_ch", &pdata->mclk_ch);
	if (ret) {
		probe_err("mclk_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "sensor_i2c_ch", &pdata->sensor_i2c_ch);
	if (ret) {
		probe_info("i2c_ch dt parsing skipped\n");
	}

	ret = of_property_read_u32(dnode, "sensor_i2c_addr", &pdata->sensor_i2c_addr);
	if (ret) {
		probe_info("i2c_addr dt parsing skipped\n");
	}

	ret = of_property_read_u32(dnode, "position", &pdata->position);
	if (ret) {
		probe_err("position read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "sensor_id", &pdata->sensor_id);
	if (ret)
		probe_warn("sensor_id read is skipped(%d)", ret);

	ret = of_property_read_u32(dnode, "active_width", &pdata->active_width);
	if (ret)
		probe_warn("active_width read is skipped(%d)", ret);

	ret = of_property_read_u32(dnode, "active_height", &pdata->active_height);
	if (ret)
		probe_warn("active_height read is skipped(%d)", ret);

	ret = of_property_read_u32(dnode, "margin_left", &pdata->margin_left);
	if (ret)
		probe_warn("margin_left read is skipped(%d)", ret);

	ret = of_property_read_u32(dnode, "margin_right", &pdata->margin_right);
	if (ret)
		probe_warn("margin_right read is skipped(%d)", ret);

	ret = of_property_read_u32(dnode, "margin_top", &pdata->margin_top);
	if (ret)
		probe_warn("margin_top read is skipped(%d)", ret);

	ret = of_property_read_u32(dnode, "margin_bottom", &pdata->margin_bottom);
	if (ret)
		probe_warn("margin_bottom read is skipped(%d)", ret);

	ret = of_property_read_u32(dnode, "max_framerate", &pdata->max_framerate);
	if (ret)
		probe_warn("max_framerate read is skipped(%d)", ret);

	ret = of_property_read_u32(dnode, "bitwidth", &pdata->bitwidth);
	if (ret)
		probe_warn("bitwidth read is skipped(%d)", ret);

	ret = of_property_read_u32(dnode, "use_retention_mode", &pdata->use_retention_mode);
	if (ret)
		probe_warn("use_retention_mode read is skipped(%d)", ret);

	ret = of_property_read_u32(dnode, "use_binning_ratio_table", &pdata->use_binning_ratio_table);
	if (ret)
		probe_warn("use_binning_ratio_table read is skipped(%d)", ret);

	ret = of_property_read_string(dnode, "sensor_maker", (const char **)&pdata->sensor_maker);
	if (ret)
		probe_warn("sensor_maker read is skipped(%d)", ret);

	ret = of_property_read_string(dnode, "sensor_name", (const char **)&pdata->sensor_name);
	if (ret)
		probe_warn("sensor_name read is skipped(%d)", ret);

	ret = of_property_read_string(dnode, "setfile_name", (const char **)&pdata->setfile_name);
	if (ret)
		probe_warn("setfile_name read is skipped(%d)", ret);

	ret = of_property_read_u32(dnode, "sensor_module_type", &pdata->sensor_module_type);
	if (ret) {
		probe_warn("sensor_module_type read is skipped(%d)", ret);
		pdata->sensor_module_type = SENSOR_TYPE_RGB;
	}

	/* vendor data */
	ret = of_property_read_u32(dnode, "rom_id", &pdata->rom_id);
	if (ret) {
		probe_info("rom_id dt parsing skipped\n");
		pdata->rom_id = ROM_ID_NOTHING;
	}

	ret = of_property_read_u32(dnode, "rom_type", &pdata->rom_type);
	if (ret) {
		probe_info("rom_type dt parsing skipped\n");
		pdata->rom_type = ROM_TYPE_NONE;
	}

	ret = of_property_read_u32(dnode, "rom_cal_index", &pdata->rom_cal_index);
	if (ret) {
		probe_info("rom_cal_index dt parsing skipped\n");
		pdata->rom_cal_index = ROM_CAL_NOTHING;
	}

	ret = of_property_read_u32(dnode, "rom_dualcal_id", &pdata->rom_dualcal_id);
	if (ret) {
		probe_info("rom_dualcal_id dt parsing skipped\n");
		pdata->rom_dualcal_id = ROM_ID_NOTHING;
	}


	ret = of_property_read_u32(dnode, "rom_dualcal_index", &pdata->rom_dualcal_index);
	if (ret) {
		probe_info("rom_dualcal_index dt parsing skipped\n");
		pdata->rom_dualcal_index = ROM_DUALCAL_NOTHING;
	}

	/* peri */
	af_np = of_get_child_by_name(dnode, "af");
	if (!af_np) {
		pdata->af_product_name = ACTUATOR_NAME_NOTHING;
	} else {
		parse_af_data(pdata, af_np);
	}

	flash_np = of_get_child_by_name(dnode, "flash");
	if (!flash_np) {
		pdata->flash_product_name = FLADRV_NAME_NOTHING;
	} else {
		parse_flash_data(pdata, flash_np);
	}

	ois_np = of_get_child_by_name(dnode, "ois");
	if (!ois_np) {
		pdata->ois_product_name = OIS_NAME_NOTHING;
	} else {
		parse_ois_data(pdata, ois_np);
	}

	mcu_np = of_get_child_by_name(dnode, "mcu");
	if (!mcu_np) {
		pdata->mcu_product_name = MCU_NAME_NOTHING;
	} else {
		parse_mcu_data(pdata, mcu_np);
	}

	aperture_np = of_get_child_by_name(dnode, "aperture");
	if (!aperture_np) {
		pdata->aperture_product_name = APERTURE_NAME_NOTHING;
	} else {
		parse_aperture_data(pdata, aperture_np);
	}

	/* vc_extra data */
	vc_extra_np = of_get_child_by_name(dnode, "vc_extra");
	if (!vc_extra_np) {
		probe_warn("sensor vc_extra are not declared to DT");
	} else {
		ret = parse_vc_extra_data(pdata, vc_extra_np);
		if (ret) {
			probe_err("parse_vc_extra_data is fail(%d)", ret);
			goto p_err;
		}
	}

	/* mode data */
	modes_np = of_get_child_by_name(dnode, "modes");
	if (!modes_np) {
		probe_warn("sensor modes are not declared to DT");
	} else {
		ret = parse_modes_data(pdata, modes_np);
		if (ret) {
			probe_err("parse_modes_data is fail(%d)", ret);
			goto p_err;
		}
	}

	/* power sequence data */
	power_np = of_get_child_by_name(dnode, "power_seq");
	if (power_np) {
		ret = of_property_read_u32(power_np, "use", &use);
		if (ret)
			probe_warn("use read is skipped");
	}

	if (power_np && use) {
		ret = parse_power_seq_data(pdata, power_np);
		if (ret) {
			probe_err("%s: parse_power_seq_data(%d)", power_np->full_name, ret);
			goto p_err;
		}
	} else {
		if (module_callback) {
			ret = module_callback(dev, pdata);
			if (ret) {
				probe_err("sensor dt callback is fail(%d)", ret);
				goto p_err;
			}
		} else {
			probe_err("sensor dt callback is NULL");
			goto p_err;
		}
	}

	eeprom_np = of_find_node_by_name(dnode, "eeprom");
	if (!eeprom_np)
		pdata->eeprom_product_name = EEPROM_NAME_NOTHING;
	else
		parse_eeprom_data(pdata, eeprom_np);

	laser_af_np = of_get_child_by_name(dnode, "laser_af");
	if (!laser_af_np)
		pdata->laser_af_product_name = LASER_AF_NAME_NOTHING;
	else
		parse_laser_af_data(pdata, laser_af_np);

	match_np = of_get_child_by_name(dnode, "match_seq");
	if (match_np) {
		ret = parse_match_seq_data(pdata, match_np);
		if (ret) {
			probe_err("%s: parse_match_seq_data(%d)", match_np->full_name, ret);
			goto p_err;
		}
	}

	pdata->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pdata->pinctrl)) {
		probe_err("devm_pinctrl_get is fail");
		goto p_err;
	}

	ret = get_pin_lookup_state(pdata->pinctrl, pdata->pin_ctrls);
	if (ret) {
		probe_err("get_pin_lookup_state is fail(%d)", ret);
		goto p_err;
	}

	dev->platform_data = pdata;

	return ret;

p_err:
	kfree(pdata);
	return ret;
}

#ifdef CONFIG_SPI
int is_spi_parse_dt(struct is_spi *spi)
{
	int ret = 0;
	struct device_node *np;
	struct device *dev;
	struct pinctrl_state *s;

	FIMC_BUG(!spi);

	dev = &spi->device->dev;

	np = of_find_compatible_node(NULL,NULL, spi->node);
	if(np == NULL) {
		probe_err("compatible: fail to read, spi_parse_dt");
		ret = -ENODEV;
		goto p_err;
	}

	spi->use_spi_pinctrl = of_property_read_bool(np, "use_spi_pinctrl");
	if (!spi->use_spi_pinctrl) {
		probe_info("%s: spi dt parsing skipped\n", __func__);
		goto p_err;
	}

	spi->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(spi->pinctrl)) {
		probe_err("devm_pinctrl_get is fail");
		goto p_err;
	}

	s = pinctrl_lookup_state(spi->pinctrl, "ssn_out");
	if (IS_ERR_OR_NULL(s)) {
		probe_info("pinctrl_lookup_state(%s) is not found", "ssn_out");
		spi->pin_ssn_out = NULL;
	} else {
		spi->pin_ssn_out = s;
	}

	s = pinctrl_lookup_state(spi->pinctrl, "ssn_fn");
	if (IS_ERR_OR_NULL(s)) {
		probe_info("pinctrl_lookup_state(%s) is not found", "ssn_fn");
		spi->pin_ssn_fn = NULL;
	} else {
		spi->pin_ssn_fn = s;
	}

	s = pinctrl_lookup_state(spi->pinctrl, "ssn_inpd");
	if (IS_ERR_OR_NULL(s)) {
		probe_info("pinctrl_lookup_state(%s) is not found", "ssn_inpd");
		spi->pin_ssn_inpd = NULL;
	} else {
		spi->pin_ssn_inpd = s;
	}

	s = pinctrl_lookup_state(spi->pinctrl, "ssn_inpu");
	if (IS_ERR_OR_NULL(s)) {
		probe_info("pinctrl_lookup_state(%s) is not found", "ssn_inpu");
		spi->pin_ssn_inpu = NULL;
	} else {
		spi->pin_ssn_inpu = s;
	}

	spi->parent_pinctrl = devm_pinctrl_get(spi->device->dev.parent->parent);

	s = pinctrl_lookup_state(spi->parent_pinctrl, "spi_out");
	if (IS_ERR_OR_NULL(s)) {
		err("pinctrl_lookup_state(%s) is failed", "spi_out");
		ret = -EINVAL;
		goto p_err;
	}

	spi->parent_pin_out = s;

	s = pinctrl_lookup_state(spi->parent_pinctrl, "spi_fn");
	if (IS_ERR_OR_NULL(s)) {
		err("pinctrl_lookup_state(%s) is failed", "spi_fn");
		ret = -EINVAL;
		goto p_err;
	}

	spi->parent_pin_fn = s;

p_err:
	return ret;
}
#endif
#else
struct exynos_platform_is *is_parse_dt(struct device *dev)
{
	return ERR_PTR(-EINVAL);
}
#endif
