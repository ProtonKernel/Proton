/*
 * Exynos regulator support.
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/slab.h>

#define EXYNOS_FLEXPMU_DBG_PREFIX	"EXYNOS-FLEXPMU-DBG: "

#define DATA_LINE	(16)
#define DATA_IDX	(8)

#define FLEXPMU_DBG_FUNC_READ(__name)		\
	exynos_flexpmu_dbg_ ## __name ## _read

#define BUF_MAX_LINE	10
#define BUF_LINE_SIZE	255
#define BUF_SIZE	(BUF_MAX_LINE * BUF_LINE_SIZE)

#define DEC_PRINT	1
#define HEX_PRINT	2

/* enum for debugfs files */
enum flexpmu_debugfs_id {
	FID_CPU_STATUS,
	FID_SEQ_STATUS,
	FID_CUR_SEQ,
	FID_SW_FLAG,
	FID_SEQ_COUNT,
	FID_MIF_ALWAYS_ON,
	FID_LPM_COUNT,
	FID_APM_REQ_INFO,
	FID_MID_DVS_EN,
	FID_MAX
};

char *flexpmu_debugfs_name[FID_MAX] = {
	"cpu_status",
	"seq_status",
	"cur_sequence",
	"sw_flag",
	"seq_count",
	"mif_always_on",
	"lpm_count",
	"apm_req_info",
	"mid_dvs_en",
};

/* enum for data lines */
enum data_id {
	DID_CPU_STATUS,		/* 0 */
	DID_SEQ_STATUS,
	DID_CUR_SEQ0,
	DID_CUR_SEQ1,
	DID_PWR_MODE0,		/* 4 */
	DID_PWR_MODE1,
	DID_PWR_MODE2,
	DID_PWR_MODE3,
	DID_PWR_MODE4,
	DID_PWR_MODE5,
	DID_SW_FLAG,
	DID_IRQ_STATUS,		/* 11 */
	DID_IRQ_DATA,
	DID_IPC_AP_STATUS,
	DID_IPC_AP_RXDATA,
	DID_IPC_AP_TXDATA,
	DID_SOC_COUNT,
	DID_MIF_COUNT,
	DID_IPC_VTS0,
	DID_IPC_VTS1,
	DID_LOCAL_PWR,
	DID_MIF_ALWAYS_ON,		/* 21 */
	DID_AP_COUNT_SLEEP,
	DID_MIF_COUNT_SLEEP,
	DID_AP_COUNT_SICD,
	DID_MIF_COUNT_SICD,
	DID_CUR_PMD,
	DID_CPU_INFORM01,
	DID_CPU_INFORM23,
	DID_CPU_INFORM45,
	DID_CPU_INFORM67,
	DID_INT_REG01,
	DID_INT_REG02,
	DID_INT_REG03,
	DID_INT_REG04,
	DID_INT_REG05,
	DID_INT_REG06,
	DID_INT_REG07,
	DID_INT_REG08,
	DID_INT_REG09,
	DID_INT_REG10,
	DID_INT_REG11,
	DID_MIFAP0,
	DID_MIFAP1,
	DID_MIFAUD0,
	DID_MIFAUD1,
	DID_MIFVTS0,
	DID_MIFVTS1,
	DID_MIFCP0,
	DID_MIFCP1,
	DID_MID_DVS_EN,
	DID_MAX
};

struct flexpmu_dbg_print_arg {
	char prefix[BUF_LINE_SIZE];
	int print_type;
};

struct dbgfs_info {
	int fid;
	struct dentry *den;
	struct file_operations fops;
};

struct dbgfs_info *flexpmu_dbg_info;
void __iomem *flexpmu_dbg_base;
static struct dentry *flexpmu_dbg_root;

struct flexpmu_apm_req_info {
	unsigned int active_req_tick;
	unsigned int last_rel_tick;
	unsigned int total_count;
	unsigned int total_time_tick;
	unsigned long long int active_since_us;
	unsigned long long int last_rel_us;
	unsigned long long int total_time_us;
	bool active_flag;
};

void __iomem *rtc_base;

#define MIF_MASTER_MAX		4
char *flexpmu_master_name[MIF_MASTER_MAX] = {
	"MIF_AP",
	"MIF_AUD",
	"MIF_VTS",
	"MIF_CP",
};

struct flexpmu_apm_req_info apm_req[MIF_MASTER_MAX];

u32 acpm_get_mifdn_count(void)
{
	return __raw_readl(flexpmu_dbg_base + (DATA_LINE * DID_MIF_COUNT_SLEEP) + DATA_IDX + 4);
}
EXPORT_SYMBOL_GPL(acpm_get_mifdn_count);

u32 acpm_get_apsocdn_count(void)
{
	return __raw_readl(flexpmu_dbg_base + (DATA_LINE * DID_AP_COUNT_SLEEP) + DATA_IDX + 4);
}
EXPORT_SYMBOL_GPL(acpm_get_apsocdn_count);

u32 acpm_get_early_wakeup_count(void)
{
	return __raw_readl(flexpmu_dbg_base + (DATA_LINE * DID_AP_COUNT_SLEEP) + DATA_IDX);
}
EXPORT_SYMBOL_GPL(acpm_get_early_wakeup_count);

/*notify to flexpmu, it is SICD w MIF(is_dsu_cpd=0) or SICD wo MIF(is_dsu_cpd=1).*/
u32 acpm_noti_dsu_cpd(bool is_dsu_cpd)
{
	__raw_writel(is_dsu_cpd, flexpmu_dbg_base + (DATA_LINE * DID_MIF_ALWAYS_ON) + DATA_IDX);
	return 0;
}
EXPORT_SYMBOL_GPL(acpm_noti_dsu_cpd);

u32 acpm_get_dsu_cpd(void)
{
	return __raw_readl(flexpmu_dbg_base + (DATA_LINE * DID_MIF_ALWAYS_ON) + DATA_IDX);
}
EXPORT_SYMBOL_GPL(acpm_get_dsu_cpd);

#define MIF_REQ_MASK		(0x00FF0000)
#define MIF_REQ_SHIFT		(16)
u32 acpm_get_mif_request(void)
{
	u32 reg = __raw_readl(flexpmu_dbg_base + (DATA_LINE * DID_SW_FLAG) + DATA_IDX + 4);

	return ((reg & MIF_REQ_MASK) >> MIF_REQ_SHIFT);

}
EXPORT_SYMBOL_GPL(acpm_get_mif_request);

static ssize_t print_dataline_2(int did, struct flexpmu_dbg_print_arg *print_arg,
		ssize_t len, char *buf, int *data_count)
{
	int data[2];
	ssize_t line_len;
	int i;

	for (i = 0; i < 2; i++) {
		if (print_arg[*data_count].print_type == DEC_PRINT) {
			data[i] = __raw_readl(flexpmu_dbg_base +
					(DATA_LINE * did) + DATA_IDX + i * 4);

			line_len = snprintf(buf + len, BUF_SIZE - len, "%s : %d\n",
					print_arg[*data_count].prefix, data[i]);
			if (line_len > 0)
				len += line_len;
		} else if (print_arg[*data_count].print_type == HEX_PRINT) {
			data[i] = __raw_readl(flexpmu_dbg_base +
					(DATA_LINE * did) + DATA_IDX + i * 4);

			line_len = snprintf(buf + len, BUF_SIZE - len, "%s : 0x%x\n",
					print_arg[*data_count].prefix, data[i]);
			if (line_len > 0)
				len += line_len;
		}
		*data_count += 1;
	}

	return len;
}

static ssize_t print_dataline_8(int did, struct flexpmu_dbg_print_arg *print_arg,
		ssize_t len, char *buf, int *data_count)
{
	int data[8];
	ssize_t line_len;
	int i;

	for (i = 0; i < 8; i++) {
		if (print_arg[*data_count].print_type == DEC_PRINT) {
			data[i] = __raw_readb(flexpmu_dbg_base +
					(DATA_LINE * did) + DATA_IDX + i);

			line_len = snprintf(buf + len, BUF_SIZE - len, "%s : %d\n",
					print_arg[*data_count].prefix, data[i]);
			if (line_len > 0)
				len += line_len;
		} else if (print_arg[*data_count].print_type == HEX_PRINT) {
			data[i] = __raw_readb(flexpmu_dbg_base +
					(DATA_LINE * did) + DATA_IDX + i);

			line_len = snprintf(buf + len, BUF_SIZE - len, "%s : 0x%x\n",
					print_arg[*data_count].prefix, data[i]);
			if (line_len > 0)
				len += line_len;
		}
		*data_count += 1;
	}

	return len;
}

static ssize_t exynos_flexpmu_dbg_cpu_status_read(int fid, char *buf)
{
	ssize_t ret = 0;
	int data_count = 0;

	struct flexpmu_dbg_print_arg print_arg[BUF_MAX_LINE] = {
		{"CL0_CPU0", DEC_PRINT},
		{"CL0_CPU1", DEC_PRINT},
		{"CL0_CPU2", DEC_PRINT},
		{"CL0_CPU3", DEC_PRINT},
		{"CL1_CPU0", DEC_PRINT},
		{"CL1_CPU1", DEC_PRINT},
		{"CL1_CPU2", DEC_PRINT},
		{"CL1_CPU3", DEC_PRINT},
	};

	ret = print_dataline_8(DID_CPU_STATUS, print_arg, ret, buf, &data_count);

	return ret;
}

static ssize_t exynos_flexpmu_dbg_seq_status_read(int fid, char *buf)
{
	ssize_t ret = 0;
	int data_count = 0;

	struct flexpmu_dbg_print_arg print_arg[BUF_MAX_LINE] = {
		{"SOC seq", DEC_PRINT},
		{"MIF seq", DEC_PRINT},
		{},
		{},
		{"nonCPU CL0", DEC_PRINT},
		{"nonCPU CL1", DEC_PRINT},
		{},
	};

	ret = print_dataline_8(DID_SEQ_STATUS, print_arg, ret, buf, &data_count);

	return ret;
}

static ssize_t exynos_flexpmu_dbg_cur_sequence_read(int fid, char *buf)
{
	ssize_t ret = 0;
	int data_count = 0;

	struct flexpmu_dbg_print_arg print_arg[BUF_MAX_LINE] = {
		{"UP Sequence", DEC_PRINT},
		{"DOWN Sequence", DEC_PRINT},
		{"Access Type", DEC_PRINT},
		{"Seq Index", DEC_PRINT},
	};

	ret = print_dataline_2(DID_CUR_SEQ0, print_arg, ret, buf, &data_count);
	ret = print_dataline_2(DID_CUR_SEQ1, print_arg, ret, buf, &data_count);

	return ret;
}

static ssize_t exynos_flexpmu_dbg_sw_flag_read(int fid, char *buf)
{
	ssize_t ret = 0;
	int data_count = 0;

	struct flexpmu_dbg_print_arg print_arg[BUF_MAX_LINE] = {
		{"Hotplug out flag", HEX_PRINT},
		{"Reset-Release flag", HEX_PRINT},
		{},
		{},
		{"CHUB ref_count", DEC_PRINT},
		{},
		{"MIF req_Master", HEX_PRINT},
		{"MIF req_count", DEC_PRINT},
	};

	ret = print_dataline_8(DID_SW_FLAG, print_arg, ret, buf, &data_count);

	return ret;
}

static ssize_t exynos_flexpmu_dbg_seq_count_read(int fid, char *buf)
{
	ssize_t ret = 0;
	int data_count = 0;

	struct flexpmu_dbg_print_arg print_arg[BUF_MAX_LINE] = {
		{"Early Wakeup", DEC_PRINT},
		{"SOC sequence", DEC_PRINT},
		{},
		{"MIF sequence", DEC_PRINT},
	};

	ret = print_dataline_2(DID_SOC_COUNT, print_arg, ret, buf, &data_count);
	ret = print_dataline_2(DID_MIF_COUNT, print_arg, ret, buf, &data_count);

	return ret;
}

static ssize_t exynos_flexpmu_dbg_mif_always_on_read(int fid, char *buf)
{
	ssize_t ret = 0;
	int data_count = 0;

	struct flexpmu_dbg_print_arg print_arg[BUF_MAX_LINE] = {
		{},
		{"MIF always on", DEC_PRINT},
	};

	ret = print_dataline_2(DID_MIF_ALWAYS_ON, print_arg, ret, buf, &data_count);

	return ret;
}

static ssize_t exynos_flexpmu_dbg_mid_dvs_en_read(int fid, char *buf)
{
	ssize_t ret = 0;
	int data_count = 0;

	struct flexpmu_dbg_print_arg print_arg[BUF_MAX_LINE] = {
		{"MID DVS states", HEX_PRINT},
		{"MID DVS enable", DEC_PRINT},
	};

	ret = print_dataline_2(DID_MID_DVS_EN, print_arg, ret, buf, &data_count);

	return ret;
}

static ssize_t exynos_flexpmu_dbg_lpm_count_read(int fid, char *buf)
{
	ssize_t ret = 0;
	int data_count = 0;

	struct flexpmu_dbg_print_arg print_arg[BUF_MAX_LINE] = {
		{"[SLEEP] Early wakeup", DEC_PRINT},
		{"[SLEEP] SOC seq down", DEC_PRINT},
		{},
		{"[SLEEP] MIF seq down", DEC_PRINT},
		{"[SICD] Early wakeup", DEC_PRINT},
		{"[SICD] SOC seq down", DEC_PRINT},
		{},
		{"[SICD] MIF seq down", DEC_PRINT},
	};

	ret = print_dataline_2(DID_AP_COUNT_SLEEP, print_arg, ret, buf, &data_count);
	ret = print_dataline_2(DID_MIF_COUNT_SLEEP, print_arg, ret, buf, &data_count);
	ret = print_dataline_2(DID_AP_COUNT_SICD, print_arg, ret, buf, &data_count);
	ret = print_dataline_2(DID_MIF_COUNT_SICD, print_arg, ret, buf, &data_count);

	return ret;
}

#define RTC_TICK_TO_US		976	/* 1024 Hz : 1tick = 976.5625us */
#define CURTICCNT_0		0x90

static ssize_t exynos_flexpmu_dbg_apm_req_info_read(int fid, char *buf)
{
	size_t ret = 0;
	unsigned long long int curr_tick = 0;
	int i = 0;

	if (!rtc_base) {
		ret = snprintf(buf + ret, BUF_SIZE - ret,
				"%s\n", "This node is not supported.\n");
		return ret;
	}

	curr_tick = __raw_readl(rtc_base + CURTICCNT_0);
	ret += snprintf(buf + ret, BUF_SIZE - ret,
			"%s: %lld\n", "curr_time", curr_tick * RTC_TICK_TO_US);
	ret += snprintf(buf + ret, BUF_SIZE - ret,
			"%8s   %32s %32s %32s %32s\n", "Master", "active_since(us ago)",
			"last_rel_time(us ago)", "total_req_time(us)", "req_count");

	for (i = 0; i < MIF_MASTER_MAX; i++) {
		apm_req[i].active_req_tick = __raw_readl(flexpmu_dbg_base
				+ (DATA_LINE * (DID_MIFAP0 + i * 2)) + DATA_IDX);
		apm_req[i].last_rel_tick = __raw_readl(flexpmu_dbg_base
				+ (DATA_LINE * (DID_MIFAP0 + i * 2)) + DATA_IDX  + 4);
		apm_req[i].total_count = __raw_readl(flexpmu_dbg_base
				+ (DATA_LINE * (DID_MIFAP1 + i * 2)) + DATA_IDX);
		apm_req[i].total_time_tick = __raw_readl(flexpmu_dbg_base
				+ (DATA_LINE * (DID_MIFAP1 + i * 2)) + DATA_IDX  + 4);

		if (apm_req[i].last_rel_tick > 0) {
			apm_req[i].last_rel_us =
				(curr_tick - apm_req[i].last_rel_tick) * RTC_TICK_TO_US;
		}

		apm_req[i].total_time_us =
			apm_req[i].total_time_tick * RTC_TICK_TO_US;

		if (apm_req[i].active_req_tick == 0) {
			apm_req[i].active_flag = false;
			apm_req[i].active_since_us = 0;
		} else {
			apm_req[i].active_flag = true;
			apm_req[i].active_since_us =
				(curr_tick - apm_req[i].active_req_tick) * RTC_TICK_TO_US;
			apm_req[i].total_time_us += apm_req[i].active_since_us;
		}

		if (BUF_SIZE - (BUF_MAX_LINE * 3) > ret) {
			ret += snprintf(buf + ret, BUF_SIZE - ret,
					"%8s : %32lld %32lld %32lld %32d\n",
					flexpmu_master_name[i],
					apm_req[i].active_since_us,
					apm_req[i].last_rel_us,
					apm_req[i].total_time_us,
					apm_req[i].total_count);
		}
	}

		return ret;
}

static ssize_t (*flexpmu_debugfs_read_fptr[FID_MAX])(int, char *) = {
	FLEXPMU_DBG_FUNC_READ(cpu_status),
	FLEXPMU_DBG_FUNC_READ(seq_status),
	FLEXPMU_DBG_FUNC_READ(cur_sequence),
	FLEXPMU_DBG_FUNC_READ(sw_flag),
	FLEXPMU_DBG_FUNC_READ(seq_count),
	FLEXPMU_DBG_FUNC_READ(mif_always_on),
	FLEXPMU_DBG_FUNC_READ(lpm_count),
	FLEXPMU_DBG_FUNC_READ(apm_req_info),
	FLEXPMU_DBG_FUNC_READ(mid_dvs_en),
};

static ssize_t exynos_flexpmu_dbg_read(struct file *file, char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct dbgfs_info *d2f = file->private_data;
	ssize_t ret;
	char buf[BUF_SIZE] = {0,};

	ret = flexpmu_debugfs_read_fptr[d2f->fid](d2f->fid, buf);
	if (ret > sizeof(buf))
		return ret;

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

static ssize_t exynos_flexpmu_dbg_write(struct file *file, const char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct dbgfs_info *f2d = file->private_data;
	ssize_t ret;
	char buf[32];

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret < 0)
		return ret;

	switch (f2d->fid) {
	case FID_MIF_ALWAYS_ON:
		if (buf[0] == '0') {
			__raw_writel(0, flexpmu_dbg_base +
					(DATA_LINE * DID_MIF_ALWAYS_ON) + 0xC);
		}
		if (buf[0] == '1') {
			__raw_writel(1, flexpmu_dbg_base +
					(DATA_LINE * DID_MIF_ALWAYS_ON) + 0xC);
		}
		break;
	case FID_MID_DVS_EN:
		if (buf[0] == '0') {
			__raw_writel(0, flexpmu_dbg_base +
					(DATA_LINE * DID_MID_DVS_EN) + 0xC);
		}
		if (buf[0] == '1') {
			__raw_writel(1, flexpmu_dbg_base +
					(DATA_LINE * DID_MID_DVS_EN) + 0xC);
		}
		break;
	default:
		break;
	}

	return ret;
}

static int exynos_flexpmu_dbg_probe(struct platform_device *pdev)
{
	int ret = 0;
	const __be32 *prop;
	unsigned int len = 0;
	unsigned int data_base = 0;
	unsigned int data_size = 0;
	unsigned int mid_dvs_en = 0;
	int i;

	flexpmu_dbg_info = kzalloc(sizeof(struct dbgfs_info) * FID_MAX, GFP_KERNEL);
	if (!flexpmu_dbg_info) {
		pr_err("%s %s: can not allocate mem for flexpmu_dbg_info\n",
				EXYNOS_FLEXPMU_DBG_PREFIX, __func__);
		ret = -ENOMEM;
		goto err_flexpmu_info;
	}

	prop = of_get_property(pdev->dev.of_node, "data-base", &len);
	if (!prop) {
		pr_err("%s %s: can not read data-base in DT\n",
				EXYNOS_FLEXPMU_DBG_PREFIX, __func__);
		ret = -EINVAL;
		goto err_dbgfs_probe;
	}
	data_base = be32_to_cpup(prop);

	prop = of_get_property(pdev->dev.of_node, "data-size", &len);
	if (!prop) {
		pr_err("%s %s: can not read data-base in DT\n",
				EXYNOS_FLEXPMU_DBG_PREFIX, __func__);
		ret = -EINVAL;
		goto err_dbgfs_probe;
	}
	data_size = be32_to_cpup(prop);

	prop = of_get_property(pdev->dev.of_node, "mid-dvs-en", &len);
	if (!prop) {
		pr_info("%s %s: can not read mid-dvs-en in DT\n",
				EXYNOS_FLEXPMU_DBG_PREFIX, __func__);
		mid_dvs_en = 0;
	} else {
		mid_dvs_en = be32_to_cpup(prop);
	}

	if (data_base && data_size)
		flexpmu_dbg_base = ioremap(data_base, data_size);

	flexpmu_dbg_root = debugfs_create_dir("flexpmu-dbg", NULL);
	if (!flexpmu_dbg_root) {
		pr_err("%s %s: could not create debugfs root dir\n",
				EXYNOS_FLEXPMU_DBG_PREFIX, __func__);
		ret = -ENOMEM;
		goto err_dbgfs_probe;
	}

	for (i = 0; i < FID_MAX; i++) {
		flexpmu_dbg_info[i].fid = i;
		flexpmu_dbg_info[i].fops.open = simple_open;
		flexpmu_dbg_info[i].fops.read = exynos_flexpmu_dbg_read;
		flexpmu_dbg_info[i].fops.write = exynos_flexpmu_dbg_write;
		flexpmu_dbg_info[i].fops.llseek = default_llseek;
		flexpmu_dbg_info[i].den = debugfs_create_file(flexpmu_debugfs_name[i],
				0644, flexpmu_dbg_root, &flexpmu_dbg_info[i],
				&flexpmu_dbg_info[i].fops);
	}

	rtc_base = of_iomap(pdev->dev.of_node, 0);
	if (!rtc_base) {
		dev_info(&pdev->dev,
				"apm_req_info node is not available!\n");
	}

	if (mid_dvs_en == 1) {
		__raw_writel(1, flexpmu_dbg_base + (DATA_LINE * DID_MID_DVS_EN) + 0xC);
		pr_info("%s: MID DVS enabled\n", EXYNOS_FLEXPMU_DBG_PREFIX);
	}

	platform_set_drvdata(pdev, flexpmu_dbg_info);

	return 0;

err_dbgfs_probe:
	kfree(flexpmu_dbg_info);
err_flexpmu_info:
	return ret;
}

static int exynos_flexpmu_dbg_remove(struct platform_device *pdev)
{
	struct dbgfs_info *flexpmu_dbg_info = platform_get_drvdata(pdev);

	debugfs_remove_recursive(flexpmu_dbg_root);
	kfree(flexpmu_dbg_info);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id exynos_flexpmu_dbg_match[] = {
	{
		.compatible = "samsung,exynos-flexpmu-dbg",
	},
	{},
};

static struct platform_driver exynos_flexpmu_dbg_drv = {
	.probe		= exynos_flexpmu_dbg_probe,
	.remove		= exynos_flexpmu_dbg_remove,
	.driver		= {
		.name	= "exynos_flexpmu_dbg",
		.owner	= THIS_MODULE,
		.of_match_table = exynos_flexpmu_dbg_match,
	},
};

static int exynos_flexpmu_dbg_init(void)
{
	return platform_driver_register(&exynos_flexpmu_dbg_drv);
}
late_initcall(exynos_flexpmu_dbg_init);

MODULE_LICENSE("GPL");
