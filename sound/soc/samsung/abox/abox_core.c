/* sound/soc/samsung/abox/abox_core.c
 *
 * ALSA SoC Audio Layer - Samsung Abox Core driver
 *
 * Copyright (c) 2018 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched/clock.h>
#include <linux/pm_runtime.h>
#include <soc/samsung/exynos-pmu-if.h>
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
#include <soc/samsung/imgloader.h>
#include <soc/samsung/exynos-s2mpu.h>
#endif
#include "abox_util.h"
#include "abox_gic.h"
#include "abox.h"
#include "abox_dbg.h"
#include "abox_core.h"
#include "abox_memlog.h"

#define FIRMWARE_MAX SZ_4

enum abox_core_type { CA7, CA32, TYPE_COUNT };
enum abox_core_sfr_def { OFFSET, MASK, OFFSET_MASK };
enum abox_core_area { SRAM, DRAM };

struct abox_core_firmware {
	const struct firmware *firmware;
	const char *name;
	enum abox_core_area area;
	unsigned int offset;
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	/* Exynos Image Loader */
	bool code_signed;
	struct imgloader_desc   *fw_imgloader_desc;
	unsigned int fw_id;
#endif
};

struct abox_core {
	struct list_head list;
	struct device *dev;
	int id;
	enum abox_core_type type;
	u32 __iomem *gpr;
	u32 __iomem *status;
	int gpr_count;
	unsigned int pmu_power[OFFSET_MASK];
	unsigned int pmu_enable[OFFSET_MASK];
	unsigned int pmu_standby[OFFSET_MASK];
	struct abox_core_firmware fw[FIRMWARE_MAX];
};

static const char * const abox_core_type_name[] = {
	[CA7] = "CA7",
	[CA32] = "CA32",
	[TYPE_COUNT] = NULL,
};

static LIST_HEAD(cores);

static enum abox_core_type abox_core_name_to_type(const char *name)
{
	enum abox_core_type type;

	for (type = TYPE_COUNT - 1; type; type--) {
		if (!strcmp(abox_core_type_name[type], name))
			break;
	}

	return (type > 0) ? type : -EINVAL;
}

static struct abox_data *get_abox_data(void)
{	struct abox_core *core;

	if (list_empty(&cores))
		return NULL;

	core = list_first_entry(&cores, struct abox_core, list);
	if (!core || !core->dev || !core->dev->parent)
		return NULL;

	return dev_get_drvdata(core->dev->parent);
}

void abox_core_flush(void)
{
	struct abox_data *data = get_abox_data();
	struct device *dev_gic = data->dev_gic;

	abox_info(data->dev, "%s\n", __func__);

	abox_gic_generate_interrupt(dev_gic, SGI_FLUSH);
	usleep_range(300, 1000);
}

void abox_core_power(int on)
{
	struct abox_data *data = get_abox_data();
	struct device *dev = data->dev;
	struct abox_core *core;

	abox_info(dev, "%s(%d)\n", __func__, on);

	list_for_each_entry(core, &cores, list) {
		unsigned int offset = core->pmu_power[OFFSET];
		unsigned int mask = core->pmu_power[MASK];

		abox_dbg(dev, "core: %d\n", core->id);
		exynos_pmu_update(offset, mask, on ? mask : 0);
	}
}

void abox_core_enable(int enable)
{
	struct abox_data *data = get_abox_data();
	struct device *dev = data->dev;
	struct abox_core *core;

	abox_info(dev, "%s(%d)\n", __func__, enable);

	list_for_each_entry(core, &cores, list) {
		unsigned int offset = core->pmu_enable[OFFSET];
		unsigned int mask = core->pmu_enable[MASK];

		abox_dbg(dev, "core: %d\n", core->id);
		exynos_pmu_update(offset, mask, enable ? mask : 0);
	}
}

int abox_core_standby(void)
{
	struct abox_data *data = get_abox_data();
	struct device *dev = data->dev;
	struct abox_core *core;
	u64 limit;
	int ret = 0;

	abox_dbg(dev, "%s\n", __func__);

	limit = local_clock() + abox_get_waiting_ns(false);
	list_for_each_entry(core, &cores, list) {
		unsigned int offset = core->pmu_standby[OFFSET];
		unsigned int mask = core->pmu_standby[MASK];
		unsigned int value = 0;

		abox_dbg(dev, "core: %d\n", core->id);

		do {
			ret = exynos_pmu_read(offset, &value);
			if (ret < 0)
				dev_err_ratelimited(dev, "standby read error(%d): %d\n",
						core->id, ret);

			if ((value & mask) == mask)
				break;

			if (local_clock() > limit) {
				char *reason;

				reason = kasprintf(GFP_KERNEL,
						"standby timeout(%d)",
						core->id);
				abox_err(dev, "%s\n", reason);
				abox_dbg_dump_gpr_mem(dev, data,
						ABOX_DBG_DUMP_KERNEL, reason);
				kfree(reason);

				ret = -EBUSY;
				break;
			}
		} while (1);
	}

	return ret;
}

void abox_core_print_gpr(void)
{
	struct abox_data *data = get_abox_data();
	struct device *dev = data ? data->dev : NULL;
	struct abox_core *core;
	u32 __iomem *gpr;
	char *ver;
	int i;

	if (!data)
		return;

	ver = (char *)(&data->calliope_version);

	abox_info(dev, "========================================\n");
	abox_info(dev, "A-Box core register dump (%c%c%c%c)\n",
			ver[3], ver[2], ver[1], ver[0]);
	abox_info(dev, "----------------------------------------\n");
	list_for_each_entry(core, &cores, list) {
		abox_info(dev, "CORE: %d\n", core->id);
		gpr = core->gpr;
		for (i = 0; i < core->gpr_count - 2; i += 2) {
			abox_info(dev, "r%02d: %08x r%02d: %08x\n",
					i, readl(gpr++), i + 1, readl(gpr++));
		}
		abox_info(dev, "r%02d: %08x pc : %08x\n",
				i, readl(gpr++), readl(gpr++));
		if (core->status)
			abox_info(dev, "status : %08x\n", readl(core->status));
	}
	abox_info(dev, "========================================\n");
}

void abox_core_print_gpr_dump(unsigned int *dump)
{
	struct abox_data *data = get_abox_data();
	struct device *dev = data ? data->dev : NULL;
	struct abox_core *core;
	u32 a, b, *gpr = dump;
	char *ver;
	int i;

	if (!data)
		return;

	ver = (char *)(&data->calliope_version);

	abox_info(dev, "========================================\n");
	abox_info(dev, "A-Box core register dump (%c%c%c%c)\n",
			ver[3], ver[2], ver[1], ver[0]);
	abox_info(dev, "----------------------------------------\n");
	list_for_each_entry(core, &cores, list) {
		abox_info(dev, "CORE: %d\n", core->id);
		for (i = 0; i < core->gpr_count - 2; i += 2) {
			a = *gpr++;
			b = *gpr++;
			abox_info(dev, "r%02d: %08x r%02d: %08x\n",
					i, a, i + 1, b);
		}
		a = *gpr++;
		b = *gpr++;
		abox_info(dev, "r%02d: %08x pc : %08x\n", i, a, b);
		if (core->status)
			abox_info(dev, "status : %08x\n", *gpr);
	}
	abox_info(dev, "========================================\n");
}

void abox_core_dump_gpr(unsigned int *tgt)
{
	struct abox_data *data = get_abox_data();
	struct abox_core *core;
	u32 __iomem *gpr;
	int i;

	if (!data)
		return;

	list_for_each_entry(core, &cores, list) {
		gpr = core->gpr;
		for (i = 0; i < core->gpr_count; i++)
			*tgt++ = readl(gpr++);
		if (core->status)
			*tgt++ = readl(core->status);
	}
}

void abox_core_dump_gpr_dump(unsigned int *tgt, unsigned int *dump)
{
	struct abox_data *data = get_abox_data();
	struct abox_core *core;
	int i;

	if (!data)
		return;

	list_for_each_entry(core, &cores, list) {
		for (i = 0; i < core->gpr_count; i++)
			*tgt++ = *dump++;
		if (core->status)
			*tgt++ = *dump++;
	}
}

int abox_core_show_gpr(char *buf)
{
	struct abox_data *data = get_abox_data();
	struct abox_core *core;
	u32 __iomem *gpr;
	char *ver, *pbuf = buf;
	int i;

	if (!data)
		return 0;

	ver = (char *)(&data->calliope_version);

	pbuf += sprintf(pbuf, "========================================\n");
	pbuf += sprintf(pbuf, "A-Box core register dump (%c%c%c%c)\n",
			ver[3], ver[2], ver[1], ver[0]);
	pbuf += sprintf(pbuf, "----------------------------------------\n");
	list_for_each_entry(core, &cores, list) {
		pbuf += sprintf(pbuf, "CORE: %d\n", core->id);
		gpr = core->gpr;
		for (i = 0; i < core->gpr_count - 2; i += 2) {
			pbuf += sprintf(pbuf, "r%02d: %08x r%02d: %08x\n",
					i, readl(gpr++), i + 1, readl(gpr++));
		}
		pbuf += sprintf(pbuf, "r%02d: %08x pc : %08x\n",
				i, readl(gpr++), readl(gpr++));
		if (core->status)
			pbuf += sprintf(pbuf, "status : %08x\n",
					readl(core->status));
	}
	pbuf += sprintf(pbuf, "========================================\n");

	return pbuf - buf;
}

int abox_core_show_gpr_min(char *buf, int len)
{
	struct abox_data *data = get_abox_data();
	struct device *dev = data ? data->dev : NULL;
	struct abox_core *core;
	u32 __iomem *gpr;
	int i, total = 0;

	if (!data)
		return 0;

	if (pm_runtime_get_if_in_use(dev) > 0) {
		list_for_each_entry(core, &cores, list) {
			gpr = core->gpr;
			/* print x0~x14, pc(=x31) */
			for (i = 0; i < core->gpr_count && i < 15; i++)
				total += scnprintf(buf + total, len - total,
						"%08x", readl_relaxed(gpr++));
			total += scnprintf(buf + total, len - total,
					"%08x", readl_relaxed(core->gpr + 31));
		}
		pm_runtime_put(dev);
	}

	return total;
}

u32 abox_core_read_gpr(int core_id, int gpr_id)
{
	struct abox_data *data = get_abox_data();
	struct device *dev = data ? data->dev : NULL;
	struct abox_core *core;
	u32 ret = 0;

	if (!data)
		return 0;

	if (pm_runtime_get_if_in_use(dev) > 0) {
		list_for_each_entry(core, &cores, list) {
			if (core->id == core_id) {
				ret = readl_relaxed(core->gpr + gpr_id);
				break;
			}
		}
		pm_runtime_put_autosuspend(dev);
	}

	return ret;
}

u32 abox_core_read_gpr_dump(int core_id, int gpr_id, unsigned int *dump)
{
	struct abox_data *data = get_abox_data();
	struct device *dev = data ? data->dev : NULL;
	struct abox_core *core;

	list_for_each_entry(core, &cores, list) {
		if ((core_id == core->id) && (gpr_id < core->gpr_count))
			return *(dump + gpr_id);

		dump += core->gpr_count;
	}

	abox_err(dev, "%s(%d, %d, %p): invalid argument\n", __func__,
			core_id, gpr_id, dump);

	return 0;
}

#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
int abox_imgloader_mem_setup(struct imgloader_desc *desc, const u8 *metadata, size_t size,
		phys_addr_t *fw_phys_base, size_t *fw_bin_size, size_t *fw_mem_size)
{
	struct abox_data *data = get_abox_data();
	struct abox_core_firmware *fw = (struct abox_core_firmware *)desc->data;

	if (!fw)
		return -EINVAL;

	switch (fw->area) {
	default:
		abox_err(desc->dev, "%s: unknown area(%d)\n", __func__,
				fw->area);
		/* fall through */
	case SRAM:
		memcpy_toio(data->sram_base + fw->offset,
				fw->firmware->data,
				fw->firmware->size);
		break;
	case DRAM:
		memcpy(data->dram_base + fw->offset,
				fw->firmware->data,
				fw->firmware->size);
		break;
	}

	if (fw) {
		*fw_phys_base = data->sram_phys + fw->offset;
		*fw_bin_size = fw->firmware->size;
		*fw_mem_size = fw->firmware->size;
		abox_info(desc->dev, "Loaded ABOX Signed Firmware : %s\n", fw->name);
	}

	return 0;
}

int abox_imgloader_verify_fw(struct imgloader_desc *desc, phys_addr_t fw_phys_base,
		size_t fw_bin_size, size_t fw_mem_size)
{
	uint64_t ret64 = 0;

	ret64 = exynos_verify_subsystem_fw(desc->name, desc->fw_id,
			fw_phys_base, fw_bin_size,
			ALIGN(fw_mem_size, SZ_64K));
	if (ret64) {
		abox_warn(desc->dev, "Failed F/W verification, ret=%llu\n", ret64);
		return -EIO;
	}

	ret64 = exynos_request_fw_stage2_ap(desc->name);
	if (ret64) {
		abox_warn(desc->dev, "Failed F/W verification to S2MPU, ret=%llu\n", ret64);
		return -EIO;
	}

	return 0;
}

struct imgloader_ops abox_imgloader_ops = {
	.mem_setup = abox_imgloader_mem_setup,
	.verify_fw = abox_imgloader_verify_fw,
};

static int abox_core_imgloader_desc_init(struct abox_core *core, struct abox_core_firmware *fw)
{
	struct imgloader_desc *desc;

	desc = devm_kzalloc(core->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	abox_info(core->dev, "%s : use imgloader for %s\n", __func__, fw->name);

	desc->dev = core->dev;
	desc->owner = THIS_MODULE;
	desc->ops = &abox_imgloader_ops;
	desc->fw_name = fw->name;
	desc->name = "AUD";
	desc->data = (void *)fw;
	desc->s2mpu_support = false;
	desc->skip_request_firmware = true;
	desc->fw_id = fw->fw_id;
	fw->fw_imgloader_desc = desc;

	return imgloader_desc_init(desc);
}
#endif
static int abox_core_load_firmware(struct abox_core *core,
		struct abox_core_firmware *fw)
{
	struct device *dev = core->dev;
	int ret;

	ret = request_firmware_direct(&fw->firmware, fw->name, core->dev);
	if (ret >= 0)
		return 0;

	ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
			fw->name, dev, GFP_KERNEL, &fw->firmware,
			cache_firmware_simple);

	abox_info(dev, "%s isn't loaded yet\n", fw->name);
	return -EAGAIN;
}

int abox_core_download_firmware(void)
{
	struct abox_data *data = get_abox_data();
	struct device *dev = data ? data->dev : NULL;
	struct abox_core *core;
	struct abox_core_firmware *fw;
	size_t left;
	int ret = 0;

	if (!data)
		return -EAGAIN;

	abox_info(dev, "%s\n", __func__);

	memset(data->dram_base, 0, DRAM_FIRMWARE_SIZE);
	memset_io(data->sram_base, 0, data->sram_size);

	list_for_each_entry(core, &cores, list) {
		size_t len = ARRAY_SIZE(core->fw);

		for (fw = core->fw; (fw - core->fw < len) && fw->name; fw++) {
			if (!fw->firmware) {
				ret |= abox_core_load_firmware(core, fw);
				if (ret < 0)
					continue;
			}
			if (IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)) {
				if (fw->code_signed && fw->fw_imgloader_desc) {
					ret |= imgloader_boot(fw->fw_imgloader_desc);
					continue;
				}
			}
			abox_dbg(dev, "%s: download %s\n", __func__, fw->name);
			left = fw->offset + fw->firmware->size;
			switch (fw->area) {
			default:
				abox_err(dev, "%s: unknown area(%d)\n", __func__,
						fw->area);
				/* fall through */
			case SRAM:
				memcpy_toio(data->sram_base + fw->offset,
						fw->firmware->data,
						fw->firmware->size);
				break;
			case DRAM:
				memcpy(data->dram_base + fw->offset,
						fw->firmware->data,
						fw->firmware->size);
				break;
			}
		}
	}

	return ret;
}

static void abox_core_check_firmware(const struct firmware *fw, void *context)
{
	struct abox_data *data = get_abox_data();
	struct device *dev = data->dev;

	abox_dbg(dev, "%s\n", __func__);

	pm_runtime_resume(dev);
}

static int abox_core_wait_for_firmware(void *context,
		void (*cont)(const struct firmware *fw, void *context))
{
	struct abox_data *data = get_abox_data();
	struct device *dev = data ? data->dev : NULL;
	struct abox_core *core = context;
	struct abox_core_firmware *fw;
	int ret = 0;

	if (!data)
		return -EAGAIN;

	abox_dbg(dev, "%s\n", __func__);

	for (fw = core->fw; fw - core->fw < ARRAY_SIZE(core->fw); fw++) {
		if (!fw->name)
			break;
		ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				fw->name, dev, GFP_KERNEL, context, cont);
		if (ret < 0)
			break;
	}

	return ret;
}

static const struct of_device_id samsung_abox_core_match[] = {
	{
		.compatible = "samsung,abox-core",
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_abox_core_match);

static int abox_core_of_parse_firmware(struct device_node *np,
		struct abox_core_firmware *fw)
{
	int ret;

	ret = of_property_read_string(np, "samsung,name", &fw->name);
	if (ret < 0)
		return ret;

	ret = of_property_read_u32(np, "samsung,area", &fw->area);
	if (ret < 0)
		return ret;

	ret = of_property_read_u32(np, "samsung,offset", &fw->offset);
	if (ret < 0)
		return ret;

	if (IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)) {
		fw->code_signed = of_property_read_bool(np, "samsung,fw-signed");
		if (fw->code_signed) {
			ret = of_property_read_u32(np, "samsung,fw-id",
					&fw->fw_id);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int abox_core_of_parse(struct platform_device *pdev,
		struct device_node *np)
{
	struct device *dev = &pdev->dev;
	struct abox_core *core;
	struct abox_core_firmware *fw;
	struct device_node *child;
	const char *type;
	size_t gpr_count;
	int ret;

	core = devm_kzalloc(dev, sizeof(*core), GFP_KERNEL);
	if (!core)
		return -ENOMEM;

	core->dev = dev;

	core->gpr = devm_get_ioremap(pdev, "gpr", NULL, &gpr_count);
	if (IS_ERR(core->gpr))
		return PTR_ERR(core->gpr);
	core->gpr_count = gpr_count / sizeof(*core->gpr);

	core->status = devm_get_ioremap(pdev, "status", NULL, NULL);
	if (IS_ERR(core->status))
		core->status = NULL;

	ret = of_property_read_u32(np, "samsung,id", &core->id);
	if (ret < 0)
		return ret;

	ret = of_property_read_string(np, "samsung,type", &type);
	if (ret < 0)
		return ret;
	core->type = abox_core_name_to_type(type);

	ret = of_property_read_u32_array(np, "samsung,pmu_power",
			core->pmu_power, ARRAY_SIZE(core->pmu_power));
	if (ret < 0)
		return ret;

	ret = of_property_read_u32_array(np, "samsung,pmu_enable",
			core->pmu_enable, ARRAY_SIZE(core->pmu_enable));
	if (ret < 0)
		return ret;

	ret = of_property_read_u32_array(np, "samsung,pmu_standby",
			core->pmu_standby, ARRAY_SIZE(core->pmu_standby));
	if (ret < 0)
		return ret;

	fw = core->fw;
	for_each_child_of_node(np, child) {
		ret = abox_core_of_parse_firmware(child, fw);
		if (ret < 0)
			continue;
		if (IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)) {
			if (fw->code_signed) {
				ret = abox_core_imgloader_desc_init(core, fw);
				if (ret < 0)
					return ret;
			}
		}
		fw++;
	}

	list_add_tail(&core->list, &cores);

	abox_core_wait_for_firmware(core, abox_core_check_firmware);

	return 0;
}

static int samsung_abox_core_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int ret = 0;

	abox_info(dev, "%s\n", __func__);

	ret = abox_core_of_parse(pdev, np);
	if (ret < 0)
		abox_err(dev, "%s: parsing failed\n", __func__);

	return ret;
}

static int samsung_abox_core_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct abox_core *core = dev_get_drvdata(dev);

	abox_dbg(dev, "%s\n", __func__);
	list_del(&core->list);

	return 0;
}

struct platform_driver samsung_abox_core_driver = {
	.probe  = samsung_abox_core_probe,
	.remove = samsung_abox_core_remove,
	.driver = {
		.name = "abox-core",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_abox_core_match),
	},
};
