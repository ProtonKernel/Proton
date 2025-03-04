/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * IPs Traffic Monitor(ITMON) Driver for Samsung Exynos2100 SOC
 * By Hosung Kim (hosung0.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/pm_domain.h>
#include <soc/samsung/exynos-itmon.h>
#include <soc/samsung/debug-snapshot.h>
#include <linux/sec_debug.h>

/* #define MULTI_IRQ_SUPPORT_ITMON */

#define OFFSET_TMOUT_REG		(0x2000)
#define OFFSET_REQ_R			(0x0)
#define OFFSET_REQ_W			(0x20)
#define OFFSET_RESP_R			(0x40)
#define OFFSET_RESP_W			(0x60)
#define OFFSET_ERR_REPT			(0x20)
#define OFFSET_PROT_CHK			(0x100)
#define OFFSET_NUM			(0x4)

#define REG_INT_MASK			(0x0)
#define REG_INT_CLR			(0x4)
#define REG_INT_INFO			(0x8)
#define REG_EXT_INFO_0			(0x10)
#define REG_EXT_INFO_1			(0x14)
#define REG_EXT_INFO_2			(0x18)
#define REG_EXT_USER			(0x80)

#define REG_DBG_CTL			(0x10)
#define REG_TMOUT_INIT_VAL		(0x14)
#define REG_TMOUT_FRZ_EN		(0x18)
#define REG_TMOUT_FRZ_STATUS		(0x1C)
#define REG_TMOUT_BUF_WR_OFFSET		(0x20)

#define REG_TMOUT_BUF_POINT_ADDR	(0x20)
#define REG_TMOUT_BUF_ID		(0x24)

#define REG_TMOUT_BUF_PAYLOAD_0		(0x28)
#define REG_TMOUT_BUF_PAYLOAD_1		(0x30)
#define REG_TMOUT_BUF_PAYLOAD_2		(0x34)
#define REG_TMOUT_BUF_PAYLOAD_3		(0x38)
#define REG_TMOUT_BUF_PAYLOAD_4		(0x3C)

#define REG_PROT_CHK_CTL		(0x4)
#define REG_PROT_CHK_INT		(0x8)
#define REG_PROT_CHK_INT_ID		(0xC)
#define REG_PROT_CHK_START_ADDR_LOW	(0x10)
#define REG_PROT_CHK_END_ADDR_LOW	(0x14)
#define REG_PROT_CHK_START_END_ADDR_UPPER	(0x18)

#define RD_RESP_INT_ENABLE		(1 << 0)
#define WR_RESP_INT_ENABLE		(1 << 1)
#define ARLEN_RLAST_INT_ENABLE		(1 << 2)
#define AWLEN_WLAST_INT_ENABLE		(1 << 3)
#define INTEND_ACCESS_INT_ENABLE	(1 << 4)

#define BIT_PROT_CHK_ERR_OCCURRED(x)	(((x) & (0x1 << 0)) >> 0)
#define BIT_PROT_CHK_ERR_CODE(x)	(((x) & (0x7 << 1)) >> 1)

#define BIT_ERR_CODE(x)			(((x) & (0xF << 28)) >> 28)
#define BIT_ERR_OCCURRED(x)		(((x) & (0x1 << 27)) >> 27)
#define BIT_ERR_VALID(x)		(((x) & (0x1 << 26)) >> 26)
#define BIT_AXID(x)			(((x) & (0xFFFF)))
#define BIT_AXUSER(x)			(((x) & (0xFFFFFFFF)))
#define BIT_AXUSER_PERI(x)		(((x) & (0xFFFF << 16)) >> 16)
#define BIT_AXBURST(x)			(((x) & (0x3)))
#define BIT_AXPROT(x)			(((x) & (0x3 << 2)) >> 2)
#define BIT_AXLEN(x)			(((x) & (0xF << 16)) >> 16)
#define BIT_AXSIZE(x)			(((x) & (0x7 << 28)) >> 28)

#define ERRCODE_SLVERR			(0)
#define ERRCODE_DECERR			(1)
#define ERRCODE_UNSUPPORTED		(2)
#define ERRCODE_POWER_DOWN		(3)
#define ERRCODE_UNKNOWN_4		(4)
#define ERRCODE_UNKNOWN_5		(5)
#define ERRCODE_TMOUT			(6)

#define DATA				(0)
#define PERI				(1)
#define BUS_PATH_TYPE			(2)

#define TRANS_TYPE_WRITE		(0)
#define TRANS_TYPE_READ			(1)
#define TRANS_TYPE_NUM			(2)

#define FROM_CP				(0)
#define FROM_CPU			(2)

#define CP_COMMON_STR			"CP_"
#define NOT_AVAILABLE_STR		"N/A"

#define TMOUT				(0xFFFFF)
#define TMOUT_TEST			(0x1)

#define PANIC_THRESHOLD			(10)

/* This value will be fixed */
#define INTEND_ADDR_START		(0)
#define INTEND_ADDR_END			(0)

enum err_type {
	FATAL = 0,
	DREX_TMOUT,
	CPU,
	CP,
	CHUB,
	IP,
	TYPE_MAX,
};

struct itmon_policy {
	char *name;
	int policy;
	bool error;
};

struct itmon_rpathinfo {
	unsigned int id;
	char *port_name;
	char *dest_name;
	unsigned int bits;
	unsigned int shift_bits;
};

struct itmon_masterinfo {
	char *port_name;
	unsigned int user;
	char *master_name;
	unsigned int bits;
};

struct itmon_nodegroup;

struct itmon_traceinfo {
	char *port;
	char *master;
	char *dest;
	unsigned long target_addr;
	unsigned int errcode;
	bool read;
	bool onoff;
	bool path_dirty;
	bool dirty;
	unsigned long from;
	int path_type;
	char buf[SZ_32];
	unsigned int axsize;
	unsigned int axlen;
	unsigned int axburst;
	unsigned int axprot;
	bool baaw_prot;
	struct list_head list;
};

struct itmon_tracedata {
	unsigned int int_info;
	unsigned int ext_info_0;
	unsigned int ext_info_1;
	unsigned int ext_info_2;
	unsigned int ext_user;
	unsigned int dbg_mo_cnt;
	unsigned int prot_chk_ctl;
	unsigned int prot_chk_info;
	unsigned int prot_chk_int_id;
	unsigned int offset;
	struct itmon_traceinfo *ref_info;
	bool logging;
	bool read;
};

struct itmon_nodeinfo {
	unsigned int type;
	char *name;
	unsigned int phy_regs;
	bool err_enabled;
	bool prot_chk_enabled;
	bool addr_detect_enabled;
	bool retention;
	unsigned int time_val;
	bool tmout_enabled;
	bool tmout_frz_enabled;
	void __iomem *regs;
	struct itmon_tracedata tracedata;
	struct itmon_nodegroup *group;
	struct list_head list;
};

struct itmon_nodegroup {
	char *name;
	unsigned int phy_regs;
	bool ex_table;
	struct itmon_nodeinfo *nodeinfo;
	unsigned int nodesize;
	unsigned int path_type;
	void __iomem *regs;
	int irq;
};

struct itmon_platdata {
	const struct itmon_rpathinfo *rpathinfo;
	const struct itmon_masterinfo *masterinfo;
	struct itmon_nodegroup *nodegroup;
	struct list_head infolist[TRANS_TYPE_NUM];
	struct list_head datalist[TRANS_TYPE_NUM];
	ktime_t last_time;
	bool cp_crash_in_progress;
	unsigned int sysfs_tmout_val;

	struct itmon_policy *policy;
	unsigned int err_cnt;
	unsigned int err_cnt_by_cpu;
	unsigned int panic_threshold;
	bool probed;
};

struct itmon_dev {
	struct device *dev;
	struct itmon_platdata *pdata;
	struct of_device_id *match;
	int irq;
	int id;
	void __iomem *regs;
	spinlock_t ctrl_lock;
	struct itmon_notifier notifier_info;
};

struct itmon_panic_block {
	struct notifier_block nb_panic_block;
	struct itmon_dev *pdev;
};

static struct itmon_policy err_policy[] = {
	[FATAL]		= {"err_fatal",		0, false},
	[DREX_TMOUT]	= {"err_drex_tmout",	0, false},
	[CPU]		= {"err_cpu",		0, false},
	[CP]		= {"err_cp",		0, false},
	[CHUB]		= {"err_chub",		0, false},
	[IP]		= {"err_ip",		0, false},
};

const static struct itmon_rpathinfo rpathinfo[] = {
	/* 0x8000_0000 - 0xf_ffff_ffff */
	{0,	"VPC0",		"DREX_IRPS",	0x3F, 0},
	{1,	"NPUS0",	"DREX_IRPS",	0x3F, 0},
	{2,	"VPC2",		"DREX_IRPS",	0x3F, 0},
	{3,	"VPC1",		"DREX_IRPS",	0x3F, 0},
	{4,	"NPUS1",	"DREX_IRPS",	0x3F, 0},
	{5,	"NPUS2",	"DREX_IRPS",	0x3F, 0},
	{6,	"DPU00",	"DREX_IRPS",	0x3F, 0},
	{7,	"DPU01",	"DREX_IRPS",	0x3F, 0},
	{8,	"DPU10",	"DREX_IRPS",	0x3F, 0},
	{9,	"DPU11",	"DREX_IRPS",	0x3F, 0},
	{10,	"DIT",		"DREX_IRPS",	0x3F, 0},
	{11,	"SBIC",		"DREX_IRPS",	0x3F, 0},
	{12,	"ALIVE",	"DREX_IRPS",	0x3F, 0},
	{13,	"HSI0",		"DREX_IRPS",	0x3F, 0},
	{14,	"CSIS0",	"DREX_IRPS",	0x3F, 0},
	{15,	"CSIS1",	"DREX_IRPS",	0x3F, 0},
	{16,	"CSIS2",	"DREX_IRPS",	0x3F, 0},
	{17,	"CSIS3",	"DREX_IRPS",	0x3F, 0},
	{18,	"TAA",		"DREX_IRPS",	0x3F, 0},
	{19,	"MCSC0",	"DREX_IRPS",	0x3F, 0},
	{20,	"DNS0",		"DREX_IRPS",	0x3F, 0},
	{21,	"DNS1",		"DREX_IRPS",	0x3F, 0},
	{22,	"YUVPP",	"DREX_IRPS",	0x3F, 0},
	{23,	"MCFP00",	"DREX_IRPS",	0x3F, 0},
	{24,	"MCFP01",	"DREX_IRPS",	0x3F, 0},
	{25,	"MCFP10",	"DREX_IRPS",	0x3F, 0},
	{26,	"MCSC1",	"DREX_IRPS",	0x3F, 0},
	{27,	"MFC00",	"DREX_IRPS",	0x3F, 0},
	{28,	"MFC01",	"DREX_IRPS",	0x3F, 0},
	{29,	"LME",		"DREX_IRPS",	0x3F, 0},
	{30,	"MCFP02",	"DREX_IRPS",	0x3F, 0},
	{31,	"MCFP03",	"DREX_IRPS",	0x3F, 0},
	{32,	"SSP",		"DREX_IRPS",	0x3F, 0},
	{33,	"MFC10",	"DREX_IRPS",	0x3F, 0},
	{34,	"MFC11",	"DREX_IRPS",	0x3F, 0},
	{35,	"HSI1",		"DREX_IRPS",	0x3F, 0},
	{36,	"M2M",		"DREX_IRPS",	0x3F, 0},
	{37,	"MCSC2",	"DREX_IRPS",	0x3F, 0},
	{38,	"CP_0",		"DREX_IRPS",	0x3F, 0},
	{39,	"CP_1",		"DREX_IRPS",	0x3F, 0},
	{40,	"CP_2",		"DREX_IRPS",	0x3F, 0},
	{41,	"AUD",		"DREX_IRPS",	0x3F, 0},
	{42,	"CORESIGHT",	"DREX_IRPS",	0x3F, 0},
	{43,	"G3D",		"DREX_IRPS",	0x3F, 0},

	/* 0x0000_0000 - 0x7fff_ffff */

	{0,	"VPC0",		"BUS0_DP",	0x3F, 0},
	{1,	"NPUS0",	"BUS0_DP",	0x3F, 0},
	{2,	"VPC2",		"BUS0_DP",	0x3F, 0},
	{3,	"VPC1",		"BUS0_DP",	0x3F, 0},
	{4,	"NPUS1",	"BUS0_DP",	0x3F, 0},
	{5,	"NPUS2",	"BUS0_DP",	0x3F, 0},
	{6,	"DPU00",	"BUS0_DP",	0x3F, 0},
	{7,	"DPU01",	"BUS0_DP",	0x3F, 0},
	{8,	"DPU10",	"BUS0_DP",	0x3F, 0},
	{9,	"DPU11",	"BUS0_DP",	0x3F, 0},
	{10,	"DIT",		"BUS0_DP",	0x3F, 0},
	{11,	"SBIC",		"BUS0_DP",	0x3F, 0},
	{12,	"ALIVE",	"BUS0_DP",	0x3F, 0},
	{13,	"HSI0",		"BUS0_DP",	0x3F, 0},
	{14,	"CSIS0",	"BUS0_DP",	0x3F, 0},
	{15,	"CSIS1",	"BUS0_DP",	0x3F, 0},
	{16,	"CSIS2",	"BUS0_DP",	0x3F, 0},
	{17,	"CSIS3",	"BUS0_DP",	0x3F, 0},
	{18,	"TAA",		"BUS0_DP",	0x3F, 0},
	{19,	"MCSC0",	"BUS0_DP",	0x3F, 0},
	{20,	"DNS0",		"BUS0_DP",	0x3F, 0},
	{21,	"DNS1",		"BUS0_DP",	0x3F, 0},
	{22,	"YUVPP",	"BUS0_DP",	0x3F, 0},
	{23,	"MCFP00",	"BUS0_DP",	0x3F, 0},
	{24,	"MCFP01",	"BUS0_DP",	0x3F, 0},
	{25,	"MCFP10",	"BUS0_DP",	0x3F, 0},
	{26,	"MCSC1",	"BUS0_DP",	0x3F, 0},
	{27,	"MFC00",	"BUS0_DP",	0x3F, 0},
	{28,	"MFC01",	"BUS0_DP",	0x3F, 0},
	{29,	"LME",		"BUS0_DP",	0x3F, 0},
	{30,	"MCFP02",	"BUS0_DP",	0x3F, 0},
	{31,	"MCFP03",	"BUS0_DP",	0x3F, 0},
	{32,	"SSP",		"BUS0_DP",	0x3F, 0},
	{33,	"MFC10",	"BUS0_DP",	0x3F, 0},
	{34,	"MFC11",	"BUS0_DP",	0x3F, 0},
	{35,	"HSI1",		"BUS0_DP",	0x3F, 0},
	{36,	"M2M",		"BUS0_DP",	0x3F, 0},
	{37,	"MCSC2",	"BUS0_DP",	0x3F, 0},

	/* 0x0000_0000 - 0x7fff_ffff */

	{0,	"CP_0",		"CORE_DP",	0x7, 0},
	{1,	"CP_1",		"CORE_DP",	0x7, 0},
	{2,	"CP_2",		"CORE_DP",	0x7, 0},
	{3,	"AUD",		"CORE_DP",	0x7, 0},
	{4,	"CORESIGHT",	"CORE_DP",	0x7, 0},
	{5,	"G3D",		"CORE_DP",	0x7, 0},
};

const static struct itmon_masterinfo masterinfo[] = {
	{"AUD",		0x0, /*X00000*/	"SPUS/SPUM",		0x1F},
	{"AUD",		0x4, /*X00100*/	"Cortex-A7",		0x1F},
	{"AUD",		0xC, /*X01100*/	"AUDEN",		0x1F},
	{"AUD",		0x10,/*X10000*/	"UDMA",			0x1F},
	{"AUD",		0x14,/*X10100*/	"XDMA",			0x1F},
	{"AUD",		0x18,/*011000*/	"C2A0_AUD",		0x3F},
	{"AUD",		0x38,/*111000*/	"C2A1_AUD",		0x3F},
	{"AUD",		0x2, /*XXXX10*/	"SYSMMU_S1_ABOX",	0x3},
	{"AUD",		0x1, /*XXXXX1*/	"SYSMMU_S2_ABOX",	0x1},

	{"ALIVE",	0x0, /*X00000*/	"ALIVE(APM)",		0x1F},
	{"ALIVE",	0x8, /*X01000*/	"ALIVE(DBGC)",		0x1F},
	{"ALIVE",	0x10,/*X10000*/	"ALIVE(PEM)",		0x1F},
	{"ALIVE",	0x2, /*XXX010*/	"VTS(CM4F System)",	0x7},
	{"ALIVE",	0x4, /*XXX100*/	"PDMA",			0x7},
	{"ALIVE",	0x6, /*XXX110*/	"SDMA",			0x7},
	{"ALIVE",	0x1, /*XXXXX1*/	"SYSMMU_S2_ALIVE",	0x1},

	{"DIT",		0x0, /*XXXXX0*/	"DIT",			0x1},
	{"DIT",		0x1, /*XXXXX1*/	"SYSMMU_S2_DIT",	0x1},

	{"SBIC",	0x0, /*XXXXX0*/	"SBIC",			0x1},
	{"SBIC",	0x1, /*XXXXX1*/	"SYSMMU_S2_SBIC",	0x1},

	{"CORESIGHT",	0x0, /*XXXX00*/	"CSSYS",		0x3},
	{"CORESIGHT",	0x1, /*XXXX01*/	"ETR",			0x3},
	{"CORESIGHT",	0x2, /*XXXX10*/	"DBGC",			0x3},

	{"CSIS0",	0x0, /*XXX000*/	"CSIS_DMA0",		0x7},
	{"CSIS0",	0x4, /*XXX100*/	"CSIS_DMA1",		0x7},
	{"CSIS0",	0x2, /*XXXX10*/	"SYSMMU_S1_D0_CSIS",	0x3},
	{"CSIS0",	0x1, /*XXXXX1*/	"SYSMMU_S2_D0_CSIS",	0x1},

	{"CSIS1",	0x0, /*XXX000*/	"CSIS_DMA2",		0x7},
	{"CSIS1",	0x4, /*XXX100*/	"CSIS_DMA3",		0x7},
	{"CSIS1",	0x2, /*XXXX10*/	"SYSMMU_S1_D1_CSIS",	0x3},
	{"CSIS1",	0x1, /*XXXXX1*/	"SYSMMU_S2_D1_CSIS",	0x3},

	{"CSIS2",	0x0, /*XX0000*/	"ZSL0",			0xF},
	{"CSIS2",	0x4, /*XX0100*/	"ZSL1",			0xF},
	{"CSIS2",	0x8, /*XX1000*/	"ZSL2",			0xF},
	{"CSIS2",	0xC, /*XX1100*/	"ZSL3",			0xF},
	{"CSIS2",	0x2, /*XXXX10*/	"SYSMMU_S1_D2_CSIS",	0x3},
	{"CSIS2",	0x1, /*XXXXX1*/	"SYSMMU_S2_D2_CSIS",	0x1},

	{"CSIS3",	0x0, /*000000*/	"STRP0/2/ZDLSTRP_W_VOTF", 0x3F},
	{"CSIS3",	0x20,/*100000*/	"STRP1/3,CSIS_W_VOTF",	0x3F},
	{"CSIS3",	0x4, /*X00100*/	"PDP_STAT_IMG0",	0x1F},
	{"CSIS3",	0x8, /*X01000*/	"PDP_STAT_IMG1",	0x1F},
	{"CSIS3",	0xC, /*X01100*/	"PDP_STAT_IMG2",	0x1F},
	{"CSIS3",	0x10,/*X10000*/	"PDP_AF0",		0x1F},
	{"CSIS3",	0x14,/*X10100*/	"PDP_AF1",		0x1F},
	{"CSIS3",	0x18,/*X11000*/	"PDP_AF2",		0x1F},
	{"CSIS3",	0x1C,/*X11100*/	"PDP_R_VOTF",		0x1F},
	{"CSIS3",	0x2, /*XXXX10*/	"SYSMMU_S1_D3_CSIS",	0x3},
	{"CSIS3",	0x1, /*XXXXX1*/	"SYSMMU_S2_D3_CSIS",	0x1},

	{"DNS0",	0x0, /*X00000*/	"DNS_S0",		0x1F},
	{"DNS0",	0x4, /*X00100*/	"DNS_S1",		0x1F},
	{"DNS0",	0x8, /*X01000*/	"DNS_S2",		0x1F},
	{"DNS0",	0xC, /*X01100*/	"DNS_S3",		0x1F},
	{"DNS0",	0x10,/*X10000*/	"DNS_S4",		0x1F},
	{"DNS0",	0x14,/*X10100*/	"DNS_S5",		0x1F},
	{"DNS0",	0x18,/*X11000*/	"DNS_S6",		0x1F},
	{"DNS0",	0x1C,/*X11100*/	"DNS_S7",		0x1F},
	{"DNS0",	0x1C,/*X11100*/	"SYSMMU_S1_D0_DNS",	0x3},
	{"DNS0",	0x1, /*XXXXX1*/	"SYSMMU_S2_D0_DNS",	0x1},

	{"DNS1",	0x0, /*X00000*/	"DNS_S8",		0x1F},
	{"DNS1",	0x4, /*X00100*/	"DNS_S9",		0x1F},
	{"DNS1",	0x8, /*X01000*/	"DNS_S10",		0x1F},
	{"DNS1",	0xC, /*X01100*/	"DNS_W_VOTF",		0x1F},
	{"DNS1",	0x10,/*X10000*/	"REP_R_VOTF",		0x1F},
	{"DNS1",	0x14,/*X10100*/	"REP_W_VOTF",		0x1F},
	{"DNS1",	0x2, /*XXXX10*/	"SYSMMU_S1_D1_DNS",	0x3},
	{"DNS1",	0x1, /*XXXXX1*/	"SYSMMU_S2_D1_DNS",	0x1},

	{"DPU00",	0x0, /*XXXX00*/	"DPUF0",		0x3},
	{"DPU00",	0x2, /*XXXX10*/	"SYSMMU_S1_D0_DPUF00",	0x3},
	{"DPU00",	0x1, /*XXXXX1*/	"SYSMMU_S2_D0_DPUF00",	0x1},

	{"DPU01",	0x0, /*XXXX00*/	"DPUF0",		0x3},
	{"DPU01",	0x2, /*XXXX10*/	"SYSMMU_S1_D1_DPUF0",	0x3},
	{"DPU01",	0x1, /*XXXXX1*/	"SYSMMU_S2_D1_DPUF0",	0x1},

	{"DPU10",	0x0, /*XXXX00*/	"DPUF1",		0x3},
	{"DPU10",	0x2, /*XXXX10*/	"SYSMMU_S1_D0_DPUF1",	0x3},
	{"DPU10",	0x1, /*XXXXX1*/	"SYSMMU_S2_D0_DPUF1",	0x1},

	{"DPU11",	0x0, /*XXXX00*/	"DPUF1",		0x3},
	{"DPU11",	0x2, /*XXXX10*/	"SYSMMU_S1_D1_DPUF1",	0x3},
	{"DPU11",	0x1, /*XXXXX1*/	"SYSMMU_S2_D1_DPUF1",	0x1},

	{"HSI0",	0x0, /*XXXXX0*/	"USB31DRD",		0x3},
	{"HSI0",	0x2, /*XXXXX0*/	"USB31_Debug",		0x3},
	{"HSI0",	0x1, /*XXXXX1*/	"SYSMMU_S2_USB",	0x1},

	{"HSI1",	0x0, /*XX0000*/	"UFS_EMBD",		0xF},
	{"HSI1",	0x2, /*XXX010*/	"MMC_CARD",		0x7},
	{"HSI1",	0x4, /*XXX100*/	"PCIE_GEN2",		0x7},
	{"HSI1",	0x6, /*XXX110*/	"PCIE_GEN4",		0x7},
	{"HSI1",	0x1, /*XXXXX1*/	"SYSMMU_S2_HSI1",	0x1},

	{"LME",		0x0, /*XXX000*/	"LME",			0x7},
	{"LME",		0x4, /*XXX100*/	"LME_VOTF",		0x7},
	{"LME",		0x2, /*XXXX10*/	"SYSMMU_S1_LME",	0x3},
	{"LME",		0x1, /*XXXXX1*/	"SYSMMU_S2_LME",	0x1},

	{"M2M",		0x0, /*X00000*/	"JPEG0",		0x1F},
	{"M2M",		0x4, /*X00100*/	"JPEG1",		0x1F},
	{"M2M",		0x8, /*X01000*/	"MSCL",			0x1F},
	{"M2M",		0xC, /*X01100*/	"ASTC",			0x1F},
	{"M2M",		0x10,/*X10000*/	"JSQZ",			0x1F},
	{"M2M",		0x2, /*XXXX10*/	"SYSMMU_S1_M2M",	0x3},
	{"M2M",		0x1, /*XXXX10*/	"SYSMMU_S2_M2M",	0x1},

	{"MCFP00",	0x0, /*XX0000*/	"MCFP0_0",		0x1F},
	{"MCFP00",	0x4, /*XX0100*/	"MCFP0_1",		0x1F},
	{"MCFP00",	0x8, /*XX1000*/	"MCFP0_2",		0x1F},
	{"MCFP00",	0xC, /*XX1100*/	"MCFP0_3",		0x1F},
	{"MCFP00",	0x2, /*XXXX10*/	"SYSMMU_S1_D0_MCFP0",	0x3},
	{"MCFP00",	0x1, /*XXXXX1*/	"SYSMMU_S2_D0_MCFP0",	0x1},

	{"MCFP01",	0x0, /*XXXX00*/	"MCFP0_4",		0x3},
	{"MCFP01",	0x2, /*XXXX10*/	"SYSMMU_S1_D1_MCFP0",	0x3},
	{"MCFP01",	0x1, /*XXXXX1*/	"SYSMMU_S2_D1_MCFP0",	0x1},

	{"MCFP02",	0x0, /*XXXX00*/	"MCFP0_5",		0x3},
	{"MCFP02",	0x2, /*XXXX10*/	"SYSMMU_S1_D2_MCFP0",	0x3},
	{"MCFP02",	0x1, /*XXXXX1*/	"SYSMMU_S2_D2_MCFP0",	0x1},

	{"MCFP03",	0x0, /*XXXX00*/	"MCFP0_6",		0x3},
	{"MCFP03",	0x2, /*XXXX10*/	"SYSMMU_S1_D3_MCFP0",	0x3},
	{"MCFP03",	0x1, /*XXXXX1*/	"SYSMMU_S2_D3_MCFP0",	0x1},

	{"MCFP10",	0x0, /*X00000*/	"ORBMCH0_M0",		0x1F},
	{"MCFP10",	0x4, /*X00100*/	"ORBMCH0_M1",		0x1F},
	{"MCFP10",	0x8, /*X01000*/	"ORBMCH0_M2",		0x1F},
	{"MCFP10",	0xC, /*X01100*/	"ORBMCH0_R_VOTF",	0x1F},
	{"MCFP10",	0x10,/*X10000*/	"ORBMCH1_M0",		0x1F},
	{"MCFP10",	0x14,/*X10100*/	"ORBMCH1_M1",		0x1F},
	{"MCFP10",	0x18,/*X11000*/	"ORBMCH1_M2",		0x1F},
	{"MCFP10",	0x1C,/*X11100*/	"ORBMCH1_R_VOTF",	0x1F},
	{"MCFP10",	0x2, /*XXXX10*/	"SYSMMU_S1_MCFP1",	0x3},
	{"MCFP10",	0x1, /*XXXXX1*/	"SYSMMU_S2_MCFP1",	0x1},

	{"MCSC0",	0x0, /*XX0000*/	"GDC0_Y",		0xF},
	{"MCSC0",	0x4, /*XX0100*/	"GDC_C",		0xF},
	{"MCSC0",	0x8, /*XX1000*/	"GDC_W_VOTF",		0xF},
	{"MCSC0",	0xC, /*XX1100*/	"GDC_R_VOTF",		0xF},
	{"MCSC0",	0x2, /*XXXX10*/	"SYSMMU_S1_D0_MCSC",	0x3},
	{"MCSC0",	0x1, /*XXXXX1*/	"SYSMMU_S2_D0_MCSC",	0x1},

	{"MCSC1",	0x0, /*XX0000*/	"MCSC3",		0xF},
	{"MCSC1",	0x4, /*XX0100*/	"MCSC4",		0xF},
	{"MCSC1",	0x8, /*001000*/	"YUVPP_L2_Y0,L2_U1,HIST", 0x3F},
	{"MCSC1",	0x18,/*011000*/	"YUVPP_L2_Y1,L2_U2",	0x3F},
	{"MCSC1",	0x28,/*101000*/	"YUVPP_L2_Y2,NADD",	0x3F},
	{"MCSC1",	0x38,/*111000*/	"YUVPP_L2_Y0,DRC",	0x3F},
	{"MCSC1",	0x2, /*XXXX10*/	"SYSMMU_S1_D1_MCSC",	0x3},
	{"MCSC1",	0x1, /*XXXXX1*/	"SYSMMU_S2_D1_MCSC",	0x1},

	{"MCSC2",	0x0, /*X00000*/	"MCSC0",		0x1F},
	{"MCSC2",	0x4, /*X00100*/	"MCSC1",		0x1F},
	{"MCSC2",	0x8, /*X01000*/	"MCSC2",		0x1F},
	{"MCSC2",	0xC, /*X01100*/	"MCSC_W_VOTF",		0x1F},
	{"MCSC2",	0x10,/*X10000*/	"MCSC_R_VOTF",		0x1F},
	{"MCSC2",	0x2, /*XXXX10*/	"SYSMMU_S1_D2_MCSC",	0x3},
	{"MCSC2",	0x1, /*XXXXX1*/	"SYSMMU_S2_D2_MCSC",	0x1},

	{"MFC00",	0x0, /*XXXX00*/	"MFC0D0",		0x3},
	{"MFC00",	0x2, /*XXXX10*/	"SYSMMU_S1_D0_MFC0",	0x3},
	{"MFC00",	0x1, /*XXXXX1*/	"SYSMMU_S2_D0_MFC0",	0x1},

	{"MFC01",	0x0, /*XX0000*/	"MFC0D1",		0xF},
	{"MFC01",	0x4, /*XX0100*/	"WFD",			0xF},
	{"MFC01",	0x8, /*XX1000*/	"MFC0_VOTF",		0xF},
	{"MFC01",	0x2, /*XXXX10*/	"SYSMMU_S1_D1_MFC0",	0x3},
	{"MFC01",	0x1, /*XXXXX1*/	"SYSMMU_S2_D1_MFC0",	0x1},

	{"MFC10",	0x0, /*XXXX00*/	"MFC1D0",		0x3},
	{"MFC10",	0x2, /*XXXX10*/	"SYSMMU_S1_D0_MFC1",	0x3},
	{"MFC10",	0x1, /*XXXXX1*/	"SYSMMU_S2_D0_MFC1",	0x1},

	{"MFC11",	0x0, /*XXXX00*/	"MFC1D1",		0x3},
	{"MFC11",	0x2, /*XXXX10*/	"SYSMMU_S1_D1_MFC1",	0x3},
	{"MFC11",	0x1, /*XXXXX1*/	"SYSMMU_S2_D1_MFC1",	0x1},

	{"SSP",		0x0, /*XXXX00*/	"SSPCORE",		0x3},
	{"SSP",		0x1, /*XXXX01*/	"SSS",			0x3},
	{"SSP",		0x2, /*XXX010*/	"RTIC",			0x7},
	{"SSP",		0x6, /*XXX110*/	"SYSMMU_S2_RTIC",	0x7},

	{"TAA",		0x0, /*000000*/	"IPP_THSTAT",		0x3F},
	{"TAA",		0x4, /*000100*/	"IPP_DRC",		0x3F},
	{"TAA",		0x8, /*001000*/	"IPP_FDPIG",		0x3F},
	{"TAA",		0xC, /*001100*/	"IPP_RGBHIST0",		0x3F},
	{"TAA",		0x10,/*010000*/	"IPP_RGBHIST1",		0x3F},
	{"TAA",		0x14,/*010100*/	"IPP_RGBHIST2",		0x3F},
	{"TAA",		0x18,/*011000*/	"IPP_RGBHIST3",		0x3F},
	{"TAA",		0x1C,/*011100*/	"IPP_DS0",		0x3F},
	{"TAA",		0x20,/*100000*/	"IPP_DS1",		0x3F},
	{"TAA",		0x24,/*100100*/	"IPP_LME_DS",		0x3F},
	{"TAA",		0x28,/*101000*/	"IPP_THSTAT_LST",	0x3F},
	{"TAA",		0x2C,/*101100*/	"IPP_STAT_W_VOTF",	0x3F},
	{"TAA",		0x30,/*110000*/	"IPP_DS_W_VOTF",	0x3F},
	{"TAA",		0x2, /*XXXX10*/	"SYSMMU_S1_TAA",	0x3},
	{"TAA",		0x1, /*XXXXX1*/	"SYSMMU_S2_TAA",	0x1},

	{"YUVPP",	0x0, /*XX0000*/	"FRC_MC",		0xF},
	{"YUVPP",	0x4, /*XX0100*/	"YUVPP_L0Y",		0xF},
	{"YUVPP",	0x8, /*XX1000*/	"YUVPP_L0U",		0xF},
	{"YUVPP",	0xC, /*XX1100*/	"YUVPP_R_VOTF",		0xF},
	{"YUVPP",	0x2, /*XXXX10*/	"SYSMMU_S1_YUVPP",	0x3},
	{"YUVPP",	0x1, /*XXXXX1*/	"SYSMMU_S2_YUVPP",	0x1},

	{"NPUS0",	0x0, /*XXX000*/	"FLC00",		0x7},
	{"NPUS0",	0x4, /*XXX100*/	"FLC10",		0x7},
	{"NPUS0",	0x2, /*XXXX10*/	"SYSMMU_S1_D0_NPUS",	0x3},
	{"NPUS0",	0x1, /*XXXXX1*/	"SYSMMU_S2_D0_NPUS",	0x1},

	{"NPUS1",	0x0, /*XXXX00*/	"FLC01",		0x3},
	{"NPUS1",	0x2, /*XXXX10*/	"SYSMMU_S1_D1_NPUS",	0x3},
	{"NPUS1",	0x1, /*XXXXX1*/	"SYSMMU_S1_D1_NPUS",	0x1},

	{"NPUS2",	0x4, /*000100*/	"CMDQ00",		0x3F},
	{"NPUS2",	0xC, /*001100*/	"CMDQ01",		0x3F},
	{"NPUS2",	0x14,/*010100*/	"CMDQ10",		0x3F},
	{"NPUS2",	0x24,/*100100*/	"C2A0",			0x3F},
	{"NPUS2",	0x2C,/*101100*/	"C2A1",			0x3F},
	{"NPUS2",	0x18,/*X11000*/	"SDMAT",		0x1F},
	{"NPUS2",	0x8, /*X01000*/	"SDMAD",		0x1F},
	{"NPUS2",	0x10,/*X10000*/	"CA32",			0x1F},
	{"NPUS2",	0x2, /*XXXX10*/	"SYSMMU_S1_D2_NPUS",	0x3},
	{"NPUS2",	0x1, /*XXXXX1*/	"SYSMMU_S2_D2_NPUS",	0x1},

	{"CP_0",	0x15,/*010101*/	"CSXAP",		0x3F},
	{"CP_0",	0x0, /*000000*/	"UCPUM",		0x3F},
	{"CP_0",	0x1, /*000001*/	"LCPUM0",		0x3F},
	{"CP_0",	0x2, /*000010*/	"LCPUM1",		0x3F},
	{"CP_0",	0x3, /*000011*/	"LCPUM2",		0x3F},
	{"CP_0",	0x4, /*000100*/	"LCPUM3",		0x3F},
	{"CP_0",	0x5, /*000101*/	"LCPUM4",		0x3F},

	{"CP_1",	0x6, /*000110*/	"DMA0",			0x3F},
	{"CP_1",	0x7, /*000111*/	"DMA1",			0x3F},
	{"CP_1",	0x8, /*001000*/	"DMA2",			0x3F},
	{"CP_1",	0x9, /*001001*/	"DATAMOVER",		0x3F},
	{"CP_1",	0xA, /*001010*/	"BAYERS",		0x3F},
	{"CP_1",	0xB, /*001011*/	"LOGGER",		0x3F},
	{"CP_1",	0xC, /*001100*/	"REGMOVER",		0x3F},
	{"CP_1",	0xD, /*001101*/	"LMAC0",		0x3F},
	{"CP_1",	0xE, /*001110*/	"LMAC1",		0x3F},
	{"CP_1",	0xF, /*001111*/	"PktProc_DL0",		0x3F},
	{"CP_1",	0x10,/*010000*/	"PktProc_DL1",		0x3F},
	{"CP_1",	0x11,/*010001*/	"PktProc_UL",		0x3F},
	{"CP_1",	0x12,/*010010*/	"HarqMover0",		0x3F},
	{"CP_1",	0x13,/*010011*/	"HarqMover1",		0x3F},
	{"CP_1",	0x14,/*010100*/	"CBGMOVER",		0x3F},
	{"CP_1",	0x16,/*010110*/	"UCPUMP",		0x3F},
	{"CP_1",	0x1, /*000001*/	"LCPUM0",		0x3F},
	{"CP_1",	0x2, /*000010*/	"LCPUM1",		0x3F},
	{"CP_1",	0x3, /*000011*/	"LCPUM2",		0x3F},
	{"CP_1",	0x4, /*000100*/	"LCPUM3",		0x3F},
	{"CP_1",	0x5, /*000101*/	"LCPUM4",		0x3F},

	{"CP_2",	0x1, /*XXXXX1*/	"SYSMMU_S2_D2_CP",	0x1},
	{"CP_2",	0x1E,/*010000*/	"PktProc_DL0",		0x3F},
	{"CP_2",	0x20,/*100000*/	"PktProc_DL1",		0x3F},
	{"CP_2",	0x22,/*100010*/	"PktProc_UL",		0x3F},

	{"SSP",		0x0,/*XXXXXX*/	"",			0x0},
	{"VPC1",	0x0,/*XXXXXX*/	"",			0x0},
	{"VPC2",	0x0,/*XXXXXX*/	"",			0x0},
	{"VPC3",	0x0,/*XXXXXX*/	"",			0x0},
	{"G3D",		0x0,/*XXXXXX*/	"",			0x0},
};

static struct itmon_nodeinfo vec_d0[] = {
	{M_NODE, "ALIVE",	0x1B473000, 1, 1, 0, 0},
	{M_NODE, "DIT",		0x1B453000, 1, 1, 0, 0},
	{M_NODE, "DPU00",	0x1B403000, 1, 1, 0, 0},
	{M_NODE, "DPU01",	0x1B413000, 1, 1, 0, 0},
	{M_NODE, "DPU10",	0x1B423000, 1, 1, 0, 0},
	{M_NODE, "DPU11",	0x1B433000, 1, 1, 0, 0},
	{M_NODE, "HSI0",	0x1B443000, 1, 1, 0, 0},
	{M_NODE, "SBIC",	0x1B463000, 1, 1, 0, 0},
	{T_S_NODE, "BUS1_DP",	0x1B4A3000, 1, 1, 0, 0},
	{T_S_NODE, "BUS1_S0",	0x1B483000, 1, 1, 0, 0},
	{T_S_NODE, "BUS1_S1",	0x1B493000, 1, 1, 0, 0},
};

static struct itmon_nodeinfo vec_d1[] = {
	{M_NODE, "CSIS0",	0x1B603000, 1, 1, 0, 0},
	{M_NODE, "CSIS1",	0x1B613000, 1, 1, 0, 0},
	{M_NODE, "CSIS2",	0x1B623000, 1, 1, 0, 0},
	{M_NODE, "CSIS3",	0x1B633000, 1, 1, 0, 0},
	{M_NODE, "DNS0",	0x1B6A3000, 1, 1, 0, 0},
	{M_NODE, "DNS1",	0x1B6B3000, 1, 1, 0, 0},
	{M_NODE, "HSI1",	0x1B753000, 1, 1, 0, 0},
	{M_NODE, "LME",		0x1B703000, 1, 1, 0, 0},
	{M_NODE, "M2M",		0x1B763000, 1, 1, 0, 0},
	{M_NODE, "MCFP00",	0x1B653000, 1, 1, 0, 0},
	{M_NODE, "MCFP01",	0x1B663000, 1, 1, 0, 0},
	{M_NODE, "MCFP02",	0x1B673000, 1, 1, 0, 0},
	{M_NODE, "MCFP03",	0x1B683000, 1, 1, 0, 0},
	{M_NODE, "MCFP10",	0x1B693000, 1, 1, 0, 0},
	{M_NODE, "MCSC0",	0x1B6C3000, 1, 1, 0, 0},
	{M_NODE, "MCSC1",	0x1B6D3000, 1, 1, 0, 0},
	{M_NODE, "MCSC2",	0x1B6E3000, 1, 1, 0, 0},
	{M_NODE, "MFC00",	0x1B713000, 1, 1, 0, 0},
	{M_NODE, "MFC01",	0x1B723000, 1, 1, 0, 0},
	{M_NODE, "MFC10",	0x1B733000, 1, 1, 0, 0},
	{M_NODE, "MFC11",	0x1B743000, 1, 1, 0, 0},
	{M_NODE, "SSP",		0x1B773000, 1, 1, 0, 0},
	{M_NODE, "TAA",		0x1B643000, 1, 1, 0, 0},
	{M_NODE, "YUVPP",	0x1B6F3000, 1, 1, 0, 0},
	{T_S_NODE, "BUS2_DP",	0x1B7C3000, 1, 1, 0, 0},
	{T_S_NODE, "BUS2_S0",	0x1B783000, 1, 1, 0, 0},
	{T_S_NODE, "BUS2_S1",	0x1B793000, 1, 1, 0, 0},
	{T_S_NODE, "BUS2_S2",	0x1B7A3000, 1, 1, 0, 0},
	{T_S_NODE, "BUS2_S3",	0x1B7B3000, 1, 1, 0, 0},
};

static struct itmon_nodeinfo vec_d2[] = {
	{T_M_NODE, "BUS1_DP",	0x1B083000, 1, 1, 0, 0},
	{T_M_NODE, "BUS1_S0",	0x1B063000, 1, 1, 0, 0},
	{T_M_NODE, "BUS1_S1",	0x1B073000, 1, 1, 0, 0},
	{T_M_NODE, "BUS2_DP",	0x1B0D3000, 1, 1, 0, 0},
	{T_M_NODE, "BUS2_S0",	0x1B093000, 1, 1, 0, 0},
	{T_M_NODE, "BUS2_S1",	0x1B0A3000, 1, 1, 0, 0},
	{T_M_NODE, "BUS2_S2",	0x1B0B3000, 1, 1, 0, 0},
	{T_M_NODE, "BUS2_S3",	0x1B0C3000, 1, 1, 0, 0},
	{M_NODE, "NPUS0",	0x1B033000, 1, 1, 0, 0},
	{M_NODE, "NPUS1",	0x1B043000, 1, 1, 0, 0},
	{M_NODE, "NPUS2",	0x1B053000, 1, 1, 0, 0},
	{M_NODE, "VPC0",	0x1B003000, 1, 1, 0, 0},
	{M_NODE, "VPC1",	0x1B013000, 1, 1, 0, 0},
	{M_NODE, "VPC2",	0x1B023000, 1, 1, 0, 0},
	{S_NODE, "BUS0_DP",	0x1B123000, 1, 1, 0, 0, TMOUT, 1},
	{T_S_NODE, "BUS0_S0",	0x1B0E3000, 1, 1, 0, 0},
	{T_S_NODE, "BUS0_S1",	0x1B0F3000, 1, 1, 0, 0},
	{T_S_NODE, "BUS0_S2",	0x1B103000, 1, 1, 0, 0},
	{T_S_NODE, "BUS0_S3",	0x1B113000, 1, 1, 0, 0},
};

static struct itmon_nodeinfo vec_d3[] = {
	{T_M_NODE, "BUS0_M0",	0x1B203000, 1, 1, 0, 0},
	{T_M_NODE, "BUS0_M1",	0x1B213000, 1, 1, 0, 0},
	{T_M_NODE, "BUS0_M2",	0x1B223000, 1, 1, 0, 0},
	{T_M_NODE, "BUS0_M3",	0x1B233000, 1, 1, 0, 0},
	{T_S_NODE, "CORE0_M0",	0x1B243000, 1, 1, 0, 0},
	{T_S_NODE, "CORE0_M1",	0x1B253000, 1, 1, 0, 0},
	{T_S_NODE, "CORE0_M2",	0x1B263000, 1, 1, 0, 0},
	{T_S_NODE, "CORE0_M3",	0x1B273000, 1, 1, 0, 0},
};

static struct itmon_nodeinfo vec_d4[] = {
	{M_NODE, "AUD",		0x1A873000, 1, 1, 0, 0},
	{T_M_NODE, "CORE0_M0",	0x1A803000, 1, 1, 0, 0},
	{T_M_NODE, "CORE0_M1",	0x1A813000, 1, 1, 0, 0},
	{T_M_NODE, "CORE0_M2",	0x1A823000, 1, 1, 0, 0},
	{T_M_NODE, "CORE0_M3",	0x1A833000, 1, 1, 0, 0},
	{M_NODE, "CORESIGHT",	0x1A883000, 1, 1, 0, 0},
	{M_NODE, "CP_0",	0x1A843000, 1, 1, 0, 0},
	{M_NODE, "CP_1",	0x1A853000, 1, 1, 0, 0},
	{M_NODE, "CP_2",	0x1A863000, 1, 1, 0, 0},
	{M_NODE, "G3D",		0x1A893000, 1, 1, 0, 0},
	{S_NODE, "CORE_DP",	0x1A8E3000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "DREX_IRPS0",	0x1A8A3000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "DREX_IRPS1",	0x1A8B3000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "DREX_IRPS2",	0x1A8C3000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "DREX_IRPS3",	0x1A8D3000, 1, 1, 0, 0, TMOUT, 1},
};

static struct itmon_nodeinfo vec_p0[] = {
	{M_NODE, "CORE_DP",	0x1AA53000, 1, 1, 0, 0},
	{M_NODE, "SCI_CCM0",	0x1AA23000, 1, 1, 0, 0},
	{M_NODE, "SCI_CCM1",	0x1AA33000, 1, 1, 0, 0},
	{M_NODE, "SCI_IRPM",	0x1AA43000, 1, 1, 0, 0},
	{T_S_NODE, "CORE_BUS0",	0x1AA13000, 1, 1, 0, 0},
	{T_S_NODE, "CORE_P0P1",	0x1AA03000, 1, 1, 0, 0},
};

static struct itmon_nodeinfo vec_p1[] = {
	{T_M_NODE, "BUS0_CORE",	0x1AC13000, 1, 1, 0, 0},
	{T_M_NODE, "CORE_P0P1",	0x1AC03000, 1, 1, 0, 0},
	{S_NODE, "ALIVE",	0x1AC83000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "AUD",		0x1AC93000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "CP_",		0x1AC73000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "CPUCL0",	0x1AC43000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "G3D",		0x1AC53000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "PERIS",	0x1AC63000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "SFR_CORE",	0x1AC33000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "TREX_DP_CORE",0x1AC23000, 1, 1, 0, 0, TMOUT, 1},
};

static struct itmon_nodeinfo vec_p2[] = {
	{M_NODE, "BUS0_DP",	0x1B913000, 1, 1, 0, 0},
	{T_M_NODE, "CORE_BUS0",	0x1B923000, 1, 1, 0, 0},
	{T_S_NODE, "BUS0_BUS1",	0x1B833000, 1, 1, 0, 0},
	{T_S_NODE, "BUS0_BUS2",	0x1B843000, 1, 1, 0, 0},
	{T_S_NODE, "BUS0_CORE",	0x1B823000, 1, 1, 0, 0},
	{S_NODE, "MIF0",	0x1B853000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "MIF1",	0x1B863000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "MIF2",	0x1B873000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "MIF3",	0x1B883000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "NPU00",	0x1B8C3000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "NPU01",	0x1B8D3000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "NPU10",	0x1B8E3000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "NPUS",	0x1B8B3000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "PERIC0",	0x1B8F3000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "PERIC2",	0x1B903000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "PERICSGIC",	0x1B893000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "SFR_BUS0",	0x1B813000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "TREX_DP_BUS0",0x1B803000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "VPC",		0x1B8A3000, 1, 1, 0, 0, TMOUT, 1},
};

static struct itmon_nodeinfo vec_p3[] = {
	{T_M_NODE, "BUS0_BUS1",	0x1BA03000, 1, 1, 0},
	{S_NODE, "DPUB",	0x1BA63000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "DPUF0",	0x1BA43000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "DPUF1",	0x1BA53000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "HSI0",	0x1BA83000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "SFR0_BUS1",	0x1BA23000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "SFR1_BUS1",	0x1BA33000, 1, 1, 0, 0, TMOUT, 1},
	{M_NODE, "TREX_DP_BUS1",0x1BA13000, 1, 1, 0},
	{S_NODE, "VTS",		0x1BA73000, 1, 1, 0, 0, TMOUT, 1},
};

static struct itmon_nodeinfo vec_p4[] = {
	{T_M_NODE, "BUS0_BUS2",	0x1BC03000, 1, 1, 0},
	{S_NODE, "CSIS",	0x1BC33000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "HSI1",	0x1BCF3000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "ITP",		0x1BC73000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "LME",		0x1BC43000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "M2M",		0x1BCD3000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "MCFP0",	0x1BC63000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "MCSC",	0x1BC83000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "MFC0",	0x1BCA3000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "MFC1",	0x1BCB3000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "PERIC1",	0x1BCE3000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "SFR_BUS2",	0x1BC23000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "SSP",		0x1BCC3000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "TAA",		0x1BC53000, 1, 1, 0, 0, TMOUT, 1},
	{M_NODE, "TREX_DP_BUS2",0x1BC13000, 1, 1, 0, 0, TMOUT, 1},
	{S_NODE, "YUVPP",	0x1BC93000, 1, 1, 0, 0, TMOUT, 1},
};

static struct itmon_nodegroup nodegroup[] = {
	{"BUS_DATA0", 0x1B4C3000, 0, vec_d0, ARRAY_SIZE(vec_d0), DATA},
	{"BUS_DATA1", 0x1B7E3000, 0, vec_d1, ARRAY_SIZE(vec_d1), DATA},
	{"BUS_DATA2", 0x1B143000, 0, vec_d2, ARRAY_SIZE(vec_d2), DATA},
	{"BUS_DATA3", 0x1B293000, 0, vec_d3, ARRAY_SIZE(vec_d3), DATA},
	{"BUS_DATA4", 0x1A903000, 0, vec_d4, ARRAY_SIZE(vec_d4), DATA},
	{"BUS_PERI0", 0x1AA73000, 0, vec_p0, ARRAY_SIZE(vec_p0), PERI},
	{"BUS_PERI1", 0x1ACA3000, 0, vec_p1, ARRAY_SIZE(vec_p1), PERI},
	{"BUS_PERI2", 0x1B943000, 0, vec_p2, ARRAY_SIZE(vec_p2), PERI},
	{"BUS_PERI3", 0x1BA93000, 0, vec_p3, ARRAY_SIZE(vec_p3), PERI},
	{"BUS_PERI4", 0x1BD03000, 0, vec_p4, ARRAY_SIZE(vec_p4), PERI},
};

const static char *itmon_pathtype[] = {
	"DATA Path transaction",
	"Configuration(SFR) Path transaction",
};

/* Error Code Description */
const static char *itmon_errcode[] = {
	"Error Detect by the Slave(SLVERR)",
	"Decode error(DECERR)",
	"Unsupported transaction error",
	"Power Down access error",
	"Unsupported transaction",
	"Unsupported transaction",
	"Timeout error - response timeout in timeout value",
	"Invalid errorcode",
};

const static char *itmon_node_string[] = {
	"M_NODE",
	"TAXI_S_NODE",
	"TAXI_M_NODE",
	"S_NODE",
};

const static char *itmon_cpu_node_string[] = {
	"M_CPU",
	"SCI_IRPM",
	"SCI_CCM",
	"CCI",
};

const static char *itmon_drex_node_string[] = {
	"DREX_IRPS",
	"BUS0_DP",
	"CORE_DP",
};

const static unsigned int itmon_invalid_addr[] = {
	0x03000000,
	0x04000000,
};

static struct itmon_dev *g_itmon;

/* declare notifier_list */
ATOMIC_NOTIFIER_HEAD(itmon_notifier_list);

static const struct of_device_id itmon_dt_match[] = {
	{.compatible = "samsung,exynos-itmon",
	 .data = NULL,},
	{},
};
MODULE_DEVICE_TABLE(of, itmon_dt_match);

static struct itmon_rpathinfo *itmon_get_rpathinfo
					(struct itmon_dev *itmon,
					 unsigned int id,
					 char *dest_name)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_rpathinfo *rpath = NULL;
	int i;

	if (!dest_name)
		return NULL;

	for (i = 0; i < (int)ARRAY_SIZE(rpathinfo); i++) {
		if (pdata->rpathinfo[i].id == (id & pdata->rpathinfo[i].bits)) {
			if (dest_name && !strncmp(pdata->rpathinfo[i].dest_name,
						  dest_name,
						  strlen(pdata->rpathinfo[i].dest_name))) {
				rpath = (struct itmon_rpathinfo *)&pdata->rpathinfo[i];
				break;
			}
		}
	}
	return rpath;
}

static struct itmon_masterinfo *itmon_get_masterinfo
				(struct itmon_dev *itmon,
				 char *port_name,
				 unsigned int user)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_masterinfo *master = NULL;
	unsigned int val;
	int i;

	if (!port_name)
		return NULL;

	for (i = 0; i < (int)ARRAY_SIZE(masterinfo); i++) {
		if (!strncmp(pdata->masterinfo[i].port_name,
				port_name, strlen(port_name))) {
			val = user & pdata->masterinfo[i].bits;
			if (val == pdata->masterinfo[i].user) {
				master = (struct itmon_masterinfo *)&pdata->masterinfo[i];
				break;
			}
		}
	}
	return master;
}

static void itmon_enable_addr_detect(struct itmon_dev *itmon,
			             struct itmon_nodeinfo *node,
			             bool enabled)
{
	/* This feature is only for M_NODE */
	unsigned int tmp, val;
	unsigned int offset = OFFSET_PROT_CHK;

	val = __raw_readl(node->regs + offset + REG_PROT_CHK_CTL);
	val |= INTEND_ACCESS_INT_ENABLE;
	__raw_writel(val, node->regs + offset + REG_PROT_CHK_CTL);

	val = ((unsigned int)INTEND_ADDR_START & U32_MAX);
	__raw_writel(val, node->regs + offset + REG_PROT_CHK_START_ADDR_LOW);

	val = (unsigned int)(((unsigned long)INTEND_ADDR_START >> 32) & U16_MAX);
	__raw_writel(val, node->regs + offset
				+ REG_PROT_CHK_START_END_ADDR_UPPER);

	val = ((unsigned int)INTEND_ADDR_END & 0xFFFFFFFF);
	__raw_writel(val, node->regs + offset
			+ REG_PROT_CHK_END_ADDR_LOW);
	val = ((unsigned int)(((unsigned long)INTEND_ADDR_END >> 32)
							& 0XFFFF0000) << 16);
	tmp = readl(node->regs + offset + REG_PROT_CHK_START_END_ADDR_UPPER);
	__raw_writel(tmp | val, node->regs + offset
				+ REG_PROT_CHK_START_END_ADDR_UPPER);

	dev_dbg(itmon->dev, "ITMON - %s addr detect  enabled\n", node->name);
}

static void itmon_enable_prot_chk(struct itmon_dev *itmon,
			       struct itmon_nodeinfo *node,
			       bool enabled)
{
	unsigned int offset = OFFSET_PROT_CHK;
	unsigned int val = 0;

	if (enabled)
		val = RD_RESP_INT_ENABLE | WR_RESP_INT_ENABLE |
		     ARLEN_RLAST_INT_ENABLE | AWLEN_WLAST_INT_ENABLE;

	__raw_writel(val, node->regs + offset + REG_PROT_CHK_CTL);

	dev_dbg(itmon->dev, "ITMON - %s hw_assert enabled\n", node->name);
}

static void itmon_enable_err_report(struct itmon_dev *itmon,
			       struct itmon_nodeinfo *node,
			       bool enabled)
{
	struct itmon_platdata *pdata = itmon->pdata;
	unsigned int offset = OFFSET_REQ_R;

	if (!pdata->probed || !node->retention)
		__raw_writel(1, node->regs + offset + REG_INT_CLR);

	/* enable interrupt */
	__raw_writel(enabled, node->regs + offset + REG_INT_MASK);

	/* clear previous interrupt of req_write */
	offset = OFFSET_REQ_W;
	if (pdata->probed || !node->retention)
		__raw_writel(1, node->regs + offset + REG_INT_CLR);
	/* enable interrupt */
	__raw_writel(enabled, node->regs + offset + REG_INT_MASK);

	/* clear previous interrupt of response_read */
	offset = OFFSET_RESP_R;
	if (!pdata->probed || !node->retention)
		__raw_writel(1, node->regs + offset + REG_INT_CLR);
	/* enable interrupt */
	__raw_writel(enabled, node->regs + offset + REG_INT_MASK);

	/* clear previous interrupt of response_write */
	offset = OFFSET_RESP_W;
	if (!pdata->probed || !node->retention)
		__raw_writel(1, node->regs + offset + REG_INT_CLR);
	/* enable interrupt */
	__raw_writel(enabled, node->regs + offset + REG_INT_MASK);

	dev_dbg(itmon->dev,
		"ITMON - %s error reporting enabled\n", node->name);
}

static void itmon_enable_timeout(struct itmon_dev *itmon,
			       struct itmon_nodeinfo *node,
			       bool enabled)
{
	unsigned int offset = OFFSET_TMOUT_REG;

	/* Enable Timeout setting */
	__raw_writel(enabled, node->regs + offset + REG_DBG_CTL);

	/* set tmout interval value */
	__raw_writel(node->time_val,
		     node->regs + offset + REG_TMOUT_INIT_VAL);

	if (node->tmout_frz_enabled) {
		/* Enable freezing */
		__raw_writel(enabled,
			     node->regs + offset + REG_TMOUT_FRZ_EN);
	}
	dev_dbg(itmon->dev, "ITMON - %s timeout enabled\n", node->name);
}

static void itmon_init(struct itmon_dev *itmon, bool enabled)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodeinfo *node;
	int i, j;

	for (i = 0; i < (int)ARRAY_SIZE(nodegroup); i++) {
		node = pdata->nodegroup[i].nodeinfo;
		for (j = 0; j < pdata->nodegroup[i].nodesize; j++) {
			if (node[j].type == S_NODE && node[j].tmout_enabled)
				itmon_enable_timeout(itmon, &node[j], true);

			if (node[j].err_enabled)
				itmon_enable_err_report(itmon, &node[j], true);

			if (node[j].prot_chk_enabled)
				itmon_enable_prot_chk(itmon, &node[j], true);

			if (node[j].addr_detect_enabled)
				itmon_enable_addr_detect(itmon, &node[j], true);
		}
	}
}

void itmon_wa_enable(const char *node_name, int node_type, bool enabled)
{
	struct itmon_dev *itmon = g_itmon;
	struct itmon_platdata *pdata;
	struct itmon_nodeinfo *node;
	int i, j;

	if (!g_itmon)
		return;

	pdata = g_itmon->pdata;

	for (i = 0; i < (int)ARRAY_SIZE(nodegroup); i++) {
		node = pdata->nodegroup[i].nodeinfo;
		for (j = 0; j < pdata->nodegroup[i].nodesize; j++) {
			if (node[j].err_enabled &&
				node[j].type == node_type &&
				!strncmp(node[j].name, node_name, strlen(node_name))) {
				itmon_enable_err_report(itmon, &node[j], enabled);
			}
		}
	}
}
EXPORT_SYMBOL(itmon_wa_enable);

void itmon_enable(bool enabled)
{
	if (g_itmon)
		itmon_init(g_itmon, enabled);
}
EXPORT_SYMBOL(itmon_enable);

static void itmon_post_handler_apply_policy(struct itmon_dev *itmon,
					    int ret_value)
{
	struct itmon_platdata *pdata = itmon->pdata;

	switch (ret_value) {
	case NOTIFY_OK:
		dev_err(itmon->dev, "all notify calls response NOTIFY_OK\n");
		break;
	case NOTIFY_STOP:
		dev_err(itmon->dev, "notify calls response NOTIFY_STOP, refer to notifier log\n");
		pdata->policy[IP].error = true;
		break;
	case NOTIFY_BAD:
		dev_err(itmon->dev, "notify calls response NOTIFY_BAD\n");
		pdata->policy[FATAL].error = true;
		break;
	}
}

static void itmon_post_handler_to_notifier(struct itmon_dev *itmon,
					   struct itmon_traceinfo *info,
					   unsigned int trans_type)
{
	int ret = 0;

	/* After treatment by port */
	if (!info->port || strlen(info->port) < 1)
		return;

	itmon->notifier_info.port = info->port;
	itmon->notifier_info.master = info->master;
	itmon->notifier_info.dest = info->dest;
	itmon->notifier_info.read = info->read;
	itmon->notifier_info.target_addr = info->target_addr;
	itmon->notifier_info.errcode = info->errcode;
	itmon->notifier_info.onoff = info->onoff;

	dev_err(itmon->dev, "     +ITMON Notifier Call Information\n\n");

	/* call notifier_call_chain of itmon */
	ret = atomic_notifier_call_chain(&itmon_notifier_list,
						0, &itmon->notifier_info);
	itmon_post_handler_apply_policy(itmon, ret);

	dev_err(itmon->dev, "\n     -ITMON Notifier Call Information\n"
		"-------------------------------------"
		"-------------------------------------\n");
}

static void itmon_post_handler_by_dest(struct itmon_dev *itmon,
				       struct itmon_traceinfo *info,
				       unsigned int trans_type)
{
	struct itmon_platdata *pdata = itmon->pdata;

	if (!info->dest || strlen(info->dest) < 1)
		return;

	if (info->errcode == ERRCODE_TMOUT) {
		int i;

		for (i = 0; i < (int)ARRAY_SIZE(itmon_drex_node_string); i++) {
			if (!strncmp(info->dest, itmon_drex_node_string[i],
				strlen(itmon_drex_node_string[i] - 1))) {
				pdata->policy[DREX_TMOUT].error = true;
				break;
			}

		}
	}
}

static void itmon_post_handler_by_master(struct itmon_dev *itmon,
					 struct itmon_traceinfo *info,
					 unsigned int trans_type)
{
	struct itmon_platdata *pdata = itmon->pdata;

	/* After treatment by port */
	if (!info->port || strlen(info->port) < 1)
		return;

	if (test_bit(FROM_CPU, &info->from)) {
		ktime_t now, interval;

		now = ktime_get();
		interval = ktime_sub(now, pdata->last_time);
		pdata->last_time = now;
		pdata->err_cnt_by_cpu++;
		if (pdata->err_cnt_by_cpu > pdata->panic_threshold)
			pdata->policy[CPU].error = true;

		if (info->errcode == ERRCODE_TMOUT) {
			pdata->policy[FATAL].error = true;
			dev_err(itmon->dev,
				"Try to handle error, even CPU transaction detected - %s",
				itmon_errcode[info->errcode]);
		} else {
			dev_err(itmon->dev, "Skips CPU transaction detected - "
				"err_cnt_by_cpu: %u, interval: %lluns\n",
				pdata->err_cnt_by_cpu,
				(unsigned long long)ktime_to_ns(interval));
		}
	} else if (!strncmp(info->port, CP_COMMON_STR, strlen(CP_COMMON_STR))) {
		pdata->policy[CP].error = true;
	} else {
		pdata->err_cnt++;
	}
}

static void itmon_report_timeout(struct itmon_dev *itmon,
				struct itmon_nodeinfo *node,
				unsigned int trans_type)
{
	unsigned int id, payload0, payload1 = 0, payload2, payload3, payload4;
	unsigned int axid, user, valid, timeout, info;
	unsigned long addr;
	char *master_name, *port_name;
	struct itmon_rpathinfo *port;
	struct itmon_masterinfo *master;
	struct itmon_nodegroup *group = NULL;
	int i, num = (trans_type == TRANS_TYPE_READ ? SZ_128 : SZ_64);
	int rw_offset = (trans_type == TRANS_TYPE_READ ?
				0 : REG_TMOUT_BUF_WR_OFFSET);
	int path_offset = 0;

	if (!node)
		return;

	group = node->group;
	if (group && group->path_type == DATA)
		path_offset = SZ_4;

	dev_err(itmon->dev,
		"\n-----------------------------------------"
		"-----------------------------------------\n"
		"      ITMON Report (%s)\n"
		"-----------------------------------------"
		"-----------------------------------------\n"
		"      Timeout Error Occurred : Master --> %s\n\n",
		trans_type == TRANS_TYPE_READ ? "READ" : "WRITE", node->name);
	dev_err(itmon->dev,
		"      TIMEOUT_BUFFER Information(NODE: %s)\n"
		"	> NUM|   BLOCK|  MASTER|VALID|TIMEOUT|"
		"      ID| PAYLOAD0|         ADDRESS| PAYLOAD4|\n",
			node->name);

	for (i = 0; i < num; i++) {
		writel(i, node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_POINT_ADDR + rw_offset);
		id = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_ID + rw_offset);
		payload0 = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_0);
		if (path_offset == SZ_4)
			payload1 = readl(node->regs + OFFSET_TMOUT_REG +
					REG_TMOUT_BUF_PAYLOAD_1 + rw_offset);
		payload2 = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_2 + rw_offset);
		payload3 = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_3 + rw_offset);
		payload4 = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_4 + rw_offset);

		if (path_offset == SZ_4) {
			timeout = (payload0 & (unsigned int)(GENMASK(7, 4))) >> 4;
			user = payload1;
		} else {
			timeout = (payload0 & (unsigned int)(GENMASK(19, 16))) >> 16;
			user = (payload0 & (unsigned int)(GENMASK(15, 8))) >> 8;
		}

		addr = (((unsigned long)payload2 & GENMASK(15, 0)) << 32ULL);
		addr |= payload3;

		info = readl(node->regs + OFFSET_TMOUT_REG +
				REG_TMOUT_BUF_PAYLOAD_3 + rw_offset + path_offset);

		/* ID[5:0] 6bit : R-PATH */
		axid = id & GENMASK(5, 0);
		/* PAYLOAD[0] : Valid or Not valid */
		valid = payload0 & BIT(0);

		port = (struct itmon_rpathinfo *)
				itmon_get_rpathinfo(itmon, axid, node->name);
		if (port) {
			port_name = port->port_name;
			if (user) {
				master = (struct itmon_masterinfo *)
					itmon_get_masterinfo(itmon, port_name, user);
				if (master)
					master_name = master->master_name;
				else
					master_name = NOT_AVAILABLE_STR;
			} else {
				master_name = NOT_AVAILABLE_STR;
			}
		} else {
			port_name = NOT_AVAILABLE_STR;
			master_name = NOT_AVAILABLE_STR;
		}
		dev_err(itmon->dev,
			"      > %03d|%8s|%8s|%5u|%7x|%08x|%08X|%016zx|%08x\n",
				i, port_name, master_name, valid, timeout,
				id, payload0, addr, payload4);
	}
	dev_err(itmon->dev,
		"-----------------------------------------"
		"-----------------------------------------\n");
}

static unsigned int power(unsigned int param, unsigned int num)
{
	if (num == 0)
		return 1;
	return param * (power(param, num - 1));
}

static void itmon_report_prot_chk_rawdata(struct itmon_dev *itmon,
				     struct itmon_nodeinfo *node)
{
	unsigned int dbg_mo_cnt, prot_chk_ctl, prot_chk_info, prot_chk_int_id;
#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	char temp_buf[SZ_128];
#endif

	dbg_mo_cnt = __raw_readl(node->regs + OFFSET_PROT_CHK);
	prot_chk_ctl = __raw_readl(node->regs +
				OFFSET_PROT_CHK + REG_PROT_CHK_CTL);
	prot_chk_info = __raw_readl(node->regs +
				OFFSET_PROT_CHK + REG_PROT_CHK_INT);
	prot_chk_int_id = __raw_readl(node->regs +
				OFFSET_PROT_CHK + REG_PROT_CHK_INT_ID);

	/* Output Raw register information */
	dev_err(itmon->dev,
		"\n-----------------------------------------"
		"-----------------------------------------\n"
		"      Protocol Checker Raw Register Information"
		"(ITMON information)\n\n");
	dev_err(itmon->dev,
		"      > %s(%s, 0x%08X)\n"
		"      > REG(0x100~0x10C)      : 0x%08X, 0x%08X, 0x%08X, 0x%08X\n",
		node->name, itmon_node_string[node->type],
		node->phy_regs,
		dbg_mo_cnt,
		prot_chk_ctl,
		prot_chk_info,
		prot_chk_int_id);
#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	snprintf(temp_buf, SZ_128, "%s/ %s/ 0x%08X/ %s/ 0x%08X, 0x%08X, 0x%08X, 0x%08X",
		"Protocol Error", node->name, node->phy_regs,
		itmon_node_string[node->type],
		dbg_mo_cnt,
		prot_chk_ctl,
		prot_chk_info,
		prot_chk_int_id);
	secdbg_exin_set_busmon(temp_buf);
#endif
}

static void itmon_report_rawdata(struct itmon_dev *itmon,
				 struct itmon_nodeinfo *node,
				 unsigned int trans_type)
{
	struct itmon_tracedata *data = &node->tracedata;

	/* Output Raw register information */
	dev_err(itmon->dev,
		"Raw Register Information -----------------------------\n"
		"      > %s(%s, 0x%08X)\n"
		"      > REG(0x08~0x18,0x80)   : 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X\n"
		"      > REG(0x100~0x10C)      : 0x%08X, 0x%08X, 0x%08X, 0x%08X\n",
		node->name, itmon_node_string[node->type],
		node->phy_regs + data->offset,
		data->int_info,
		data->ext_info_0,
		data->ext_info_1,
		data->ext_info_2,
		data->ext_user,
		data->dbg_mo_cnt,
		data->prot_chk_ctl,
		data->prot_chk_info,
		data->prot_chk_int_id);
}

static void itmon_report_traceinfo(struct itmon_dev *itmon,
				   struct itmon_traceinfo *info,
				   unsigned int trans_type)
{
#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	char temp_buf[SZ_128];
#endif
	if (!info->dirty)
		return;

	pr_auto(ASL3,
		"\n-----------------------------------------"
		"-----------------------------------------\n"
		"      Transaction Information\n\n"
		"      > Master         : %s %s\n"
		"      > Target         : %s\n"
		"      > Target Address : 0x%lX %s\n"
		"      > Type           : %s\n"
		"      > Error code     : %s\n\n",
		info->port, info->master ? info->master : "",
		info->dest ? info->dest : NOT_AVAILABLE_STR,
		info->target_addr,
		info->baaw_prot == true ? "(BAAW Remapped address)" : "",
		trans_type == TRANS_TYPE_READ ? "READ" : "WRITE",
		itmon_errcode[info->errcode]);
#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	snprintf(temp_buf, SZ_128, "%s %s/ %s/ 0x%zx %s/ %s/ %s",
		info->port, info->master ? info->master : "",
		info->dest ? info->dest : NOT_AVAILABLE_STR,
		info->target_addr,
		info->baaw_prot == true ? "(by CP maybe)" : "",
		trans_type == TRANS_TYPE_READ ? "READ" : "WRITE",
		itmon_errcode[info->errcode]);
	secdbg_exin_set_busmon(temp_buf);
#endif

	pr_auto(ASL3,
		"\n-----------------------------------------"
		"-----------------------------------------\n"
		"      > Size           : %u bytes x %u burst => %u bytes\n"
		"      > Burst Type     : %u (0:FIXED, 1:INCR, 2:WRAP)\n"
		"      > Level          : %s\n"
		"      > Protection     : %s\n"
		"      > Path Type      : %s\n\n",
		power(2, info->axsize), info->axlen + 1,
		power(2, info->axsize) * (info->axlen + 1),
		info->axburst,
		info->axprot & BIT(0) ? "Privileged" : "Unprivileged",
		info->axprot & BIT(1) ? "Non-secure" : "Secure",
		itmon_pathtype[info->path_type]);
}

static void itmon_report_pathinfo(struct itmon_dev *itmon,
				  struct itmon_nodeinfo *node,
				  struct itmon_traceinfo *info,
				  unsigned int trans_type)

{
	struct itmon_tracedata *data = &node->tracedata;
	if (!info->path_dirty) {
		pr_auto(ASL3,
			"\n-----------------------------------------"
			"-----------------------------------------\n"
			"      ITMON Report (%s)\n"
			"-----------------------------------------"
			"-----------------------------------------\n"
			"      PATH Information\n\n",
			trans_type == TRANS_TYPE_READ ? "READ" : "WRITE");
		info->path_dirty = true;
	}
	switch (node->type) {
	case M_NODE:
		pr_auto(ASL3, "      > %14s, %8s(0x%08X)\n",
			node->name, "M_NODE", node->phy_regs + data->offset);
		break;
	case T_S_NODE:
		pr_auto(ASL3, "      > %14s, %8s(0x%08X)\n",
			node->name, "T_S_NODE", node->phy_regs + data->offset);
		break;
	case T_M_NODE:
		pr_auto(ASL3, "      > %14s, %8s(0x%08X)\n",
			node->name, "T_M_NODE", node->phy_regs + data->offset);
		break;
	case S_NODE:
		pr_auto(ASL3, "      > %14s, %8s(0x%08X)\n",
			node->name, "S_NODE", node->phy_regs + data->offset);
		break;
	}
}

static int itmon_parse_cpuinfo(struct itmon_dev *itmon,
			       struct itmon_nodeinfo *node,
			       struct itmon_traceinfo *info,
			       unsigned int userbit)
{
	struct itmon_tracedata *find_data = NULL;
	int cluster_num, core_num, i;
	int ret = -1;

	for (i = 0; i < (int)ARRAY_SIZE(itmon_cpu_node_string); i++) {
		if (!strncmp(node->name, itmon_cpu_node_string[i],
			strlen(itmon_cpu_node_string[i]) - 1)) {
				core_num = (userbit & (GENMASK(2, 0)));
				cluster_num = 0;
				snprintf(info->buf, SZ_32 - 1,
						"CPU%d Cluster%d", core_num, cluster_num);
				find_data = &node->tracedata;
				find_data->ref_info = info;
				info->port = info->buf;
				set_bit(FROM_CPU, &info->from);
				ret = 0;
				break;
			}
	};

	return ret;
}

static void itmon_parse_traceinfo(struct itmon_dev *itmon,
				   struct itmon_nodeinfo *node,
				   unsigned int trans_type)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_tracedata *data = &node->tracedata;
	struct itmon_traceinfo *new_info = NULL;
	struct itmon_masterinfo *master = NULL;
	struct itmon_rpathinfo *port = NULL;
	struct itmon_nodeinfo *find_node = NULL;
	struct itmon_tracedata *find_data = NULL;
	struct itmon_nodegroup *group = node->group;
	unsigned int errcode, axid;
	unsigned int userbit;
	int i;

	errcode = BIT_ERR_CODE(data->int_info);
	if (!BIT_ERR_VALID(data->int_info))
		return;

	if (node->type == M_NODE && errcode != ERRCODE_DECERR)
		return;

	new_info = kmalloc(sizeof(struct itmon_traceinfo), GFP_ATOMIC);
	if (!new_info) {
		dev_err(itmon->dev, "failed to kmalloc for %s node\n",
			node->name);
		return;
	}
	axid = (unsigned int)BIT_AXID(data->int_info);
	if (group->path_type == DATA)
		userbit = BIT_AXUSER(data->ext_user);
	else
		userbit = BIT_AXUSER_PERI(data->ext_info_2);

	new_info->port = NULL;
	new_info->master = NULL;

	switch (node->type) {
	case S_NODE:
		new_info->dest = node->name;
		port = (struct itmon_rpathinfo *)
				itmon_get_rpathinfo(itmon, axid, node->name);
		list_for_each_entry(find_node, &pdata->datalist[trans_type], list) {
			if (find_node->type != M_NODE)
				continue;
			if (!itmon_parse_cpuinfo(itmon, find_node, new_info, userbit)) {
				break;
			} else if (port && !strncmp(find_node->name, port->port_name,
							strlen(port->port_name) - 1)) {
				new_info->port = port->port_name;
				master = (struct itmon_masterinfo *)
						itmon_get_masterinfo(itmon,
							new_info->port, userbit);
				if (master)
					new_info->master = master->master_name;

				find_data = &find_node->tracedata;
				find_data->ref_info = new_info;
				break;
			} else {
				if (!port) {
					for (i = 0; i < (int)ARRAY_SIZE(rpathinfo); i++) {
						if (!strncmp(find_node->name,
								pdata->rpathinfo[i].port_name,
								strlen(find_node->name))) {
								new_info->port = find_node->name;
								new_info->master = " ";
								find_data = &find_node->tracedata;
								find_data->ref_info = new_info;
								break;
						}
					}
				}
			}
		}
		if (!new_info->port) {
			new_info->port = "Failed to parsing";
			new_info->master = "Refer to Raw Node information";
		}
		break;
	case M_NODE:
		new_info->dest = "Refer to address";

		if (!itmon_parse_cpuinfo(itmon, node, new_info, userbit)) {
			break;
		} else {
			new_info->port = node->name;
			master = (struct itmon_masterinfo *)
				itmon_get_masterinfo(itmon, node->name, userbit);
			if (master)
				new_info->master = master->master_name;
			else
				new_info->master = " ";
		}
		break;
	default:
		dev_err(itmon->dev,
			"Unknown Error - offset:%u\n", data->offset);
		kfree(new_info);
		return;
	}

	/* Last Information */
	new_info->path_type = group->path_type;
	new_info->target_addr =
		(((unsigned long)node->tracedata.ext_info_1
		& GENMASK(15, 0)) << 32ULL);
	new_info->target_addr |= node->tracedata.ext_info_0;
	new_info->errcode = errcode;
	new_info->dirty = true;
	new_info->axsize = BIT_AXSIZE(data->ext_info_1);
	new_info->axlen = BIT_AXLEN(data->ext_info_1);
	new_info->axburst = BIT_AXBURST(data->ext_info_2);
	new_info->axprot = BIT_AXPROT(data->ext_info_2);
	new_info->baaw_prot = false;

	for (i = 0; i < (int)ARRAY_SIZE(itmon_invalid_addr); i++) {
		if (new_info->target_addr == itmon_invalid_addr[i]) {
			new_info->baaw_prot = true;
			break;
		}
	}
	data->ref_info = new_info;
	list_add(&new_info->list, &pdata->infolist[trans_type]);
}

static void itmon_analyze_errnode(struct itmon_dev *itmon)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_traceinfo *info, *next_info;
	struct itmon_tracedata *data;
	struct itmon_nodeinfo *node, *next_node;
	unsigned int trans_type;
	int i;

	/* Parse */
	for (trans_type = 0; trans_type < TRANS_TYPE_NUM; trans_type++) {
		list_for_each_entry(node, &pdata->datalist[trans_type], list) {
			if (node->type == S_NODE || node->type == M_NODE)
				itmon_parse_traceinfo(itmon, node, trans_type);
		}
	}

	/* Report */
	for (trans_type = 0; trans_type < TRANS_TYPE_NUM; trans_type++) {
		list_for_each_entry(info, &pdata->infolist[trans_type], list) {
			info->path_dirty = false;
			list_for_each_entry(node, &pdata->datalist[trans_type], list) {
				if (node) {
					data = &node->tracedata;
					if (data->ref_info == info)
						itmon_report_pathinfo(itmon, node, info, trans_type);
				}
			}
			itmon_report_traceinfo(itmon, info, trans_type);
		}
	}

	/* Report Raw all tracedata and Clean-up tracedata and node */
	for (trans_type = 0; trans_type < TRANS_TYPE_NUM; trans_type++) {
		for (i = M_NODE; i < NODE_TYPE; i++) {
			list_for_each_entry_safe(node, next_node,
				&pdata->datalist[trans_type], list) {
				if (i == node->type) {
					itmon_report_rawdata(itmon, node, trans_type);
					list_del(&node->list);
					kfree(node);
				}
			}
		}
	}

	/* Rest works and Clean-up traceinfo */
	for (trans_type = 0; trans_type < TRANS_TYPE_NUM; trans_type++) {
		list_for_each_entry_safe(info, next_info,
				&pdata->infolist[trans_type], list) {
			itmon_post_handler_to_notifier(itmon, info, trans_type);
			itmon_post_handler_by_dest(itmon, info, trans_type);
			itmon_post_handler_by_master(itmon, info, trans_type);
			list_del(&info->list);
			kfree(info);
		}
	}
}

static void itmon_collect_errnode(struct itmon_dev *itmon,
			    struct itmon_nodegroup *group,
			    struct itmon_nodeinfo *node,
			    unsigned int offset)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodeinfo *new_node = NULL;
	unsigned int int_info, info0, info1, info2, user = 0;
	unsigned int prot_chk_ctl, prot_chk_info, prot_chk_int_id, dbg_mo_cnt;
	bool read = TRANS_TYPE_WRITE;
	bool req = false;

	int_info = __raw_readl(node->regs + offset + REG_INT_INFO);
	info0 = __raw_readl(node->regs + offset + REG_EXT_INFO_0);
	info1 = __raw_readl(node->regs + offset + REG_EXT_INFO_1);
	info2 = __raw_readl(node->regs + offset + REG_EXT_INFO_2);
	if (group->path_type == DATA)
		user = __raw_readl(node->regs + offset + REG_EXT_USER);

	dbg_mo_cnt = __raw_readl(node->regs + OFFSET_PROT_CHK);
	prot_chk_ctl = __raw_readl(node->regs +
				OFFSET_PROT_CHK + REG_PROT_CHK_CTL);
	prot_chk_info = __raw_readl(node->regs +
				OFFSET_PROT_CHK + REG_PROT_CHK_INT);
	prot_chk_int_id = __raw_readl(node->regs +
				OFFSET_PROT_CHK + REG_PROT_CHK_INT_ID);
	switch (offset) {
	case OFFSET_REQ_R:
		read = TRANS_TYPE_READ;
		/* fall down */
	case OFFSET_REQ_W:
		req = true;
		/* Only S-Node is able to make log to registers */
		break;
	case OFFSET_RESP_R:
		read = TRANS_TYPE_READ;
		/* fall down */
	case OFFSET_RESP_W:
		req = false;
		/* Only NOT S-Node is able to make log to registers */
		break;
	default:
		pr_auto(ASL3,
			"Unknown Error - node:%s offset:%u\n", node->name, offset);
		break;
	}

	new_node = kmalloc(sizeof(struct itmon_nodeinfo), GFP_ATOMIC);
	if (new_node) {
		/* Fill detected node information to tracedata's list */
		memcpy(new_node, node, sizeof(struct itmon_nodeinfo));
		new_node->tracedata.int_info = int_info;
		new_node->tracedata.ext_info_0 = info0;
		new_node->tracedata.ext_info_1 = info1;
		new_node->tracedata.ext_info_2 = info2;
		new_node->tracedata.ext_user = user;
		new_node->tracedata.dbg_mo_cnt = dbg_mo_cnt;
		new_node->tracedata.prot_chk_ctl = prot_chk_ctl;
		new_node->tracedata.prot_chk_info = prot_chk_info;
		new_node->tracedata.prot_chk_int_id = prot_chk_int_id;

		new_node->tracedata.offset = offset;
		new_node->tracedata.read = read;
		new_node->tracedata.ref_info = NULL;
		new_node->group = group;
		if (BIT_ERR_VALID(int_info))
			node->tracedata.logging = true;
		else
			node->tracedata.logging = false;

		list_add(&new_node->list, &pdata->datalist[read]);
	} else {
		pr_auto(ASL3,
			"failed to kmalloc for %s node %x offset\n",
			node->name, offset);
	}
}

static int __itmon_search_node(struct itmon_dev *itmon,
			       struct itmon_nodegroup *group,
			       bool clear)
{
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodeinfo *node = NULL;
	unsigned int val, offset, freeze;
	unsigned long vec, bit = 0;
	int i, ret = 0;

	if (group->phy_regs) {
		if (group->ex_table)
			vec = (unsigned long)__raw_readq(group->regs);
		else
			vec = (unsigned long)__raw_readl(group->regs);
	} else {
		vec = GENMASK(group->nodesize, 0);
	}

	if (!vec)
		goto exit;

	node = group->nodeinfo;

	for_each_set_bit(bit, &vec, group->nodesize) {
		/* exist array */
		for (i = 0; i < OFFSET_NUM; i++) {
			offset = i * OFFSET_ERR_REPT;
			/* Check Request information */
			val = __raw_readl(node[bit].regs + offset + REG_INT_INFO);
			if (BIT_ERR_OCCURRED(val)) {
				/* This node occurs the error */
				itmon_collect_errnode(itmon, group, &node[bit], offset);
				if (clear)
					__raw_writel(1, node[bit].regs
							+ offset + REG_INT_CLR);
				ret = true;
			}
		}
		/* Check H/W assertion */
		if (node[bit].prot_chk_enabled) {
			val = __raw_readl(node[bit].regs + OFFSET_PROT_CHK +
						REG_PROT_CHK_INT);
			/* if timeout_freeze is enable,
			 * PROT_CHK interrupt is able to assert without any information */
			if (BIT_PROT_CHK_ERR_OCCURRED(val) && (val & GENMASK(8, 1))) {
				itmon_report_prot_chk_rawdata(itmon, &node[bit]);
				pdata->policy[FATAL].error = true;
				ret = true;
			}
		}
		/* Check freeze enable node */
		if (node[bit].type == S_NODE && node[bit].tmout_frz_enabled) {
			val = __raw_readl(node[bit].regs + OFFSET_TMOUT_REG  +
						REG_TMOUT_FRZ_STATUS );
			freeze = val & (unsigned int)GENMASK(1, 0);
			if (freeze) {
				if (freeze & BIT(0))
					itmon_report_timeout(itmon, &node[bit], TRANS_TYPE_WRITE);
				if (freeze & BIT(1))
					itmon_report_timeout(itmon, &node[bit], TRANS_TYPE_READ);
				pdata->policy[FATAL].error = true;
				ret = true;
			}
		}
	}
exit:
	return ret;
}

static int itmon_search_node(struct itmon_dev *itmon,
			     struct itmon_nodegroup *group,
			     bool clear)
{
	int i, ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&itmon->ctrl_lock, flags);

	if (group) {
		ret = __itmon_search_node(itmon, group, clear);
	} else {
		/* Processing all group & nodes */
		for (i = 0; i < (int)ARRAY_SIZE(nodegroup); i++) {
			group = &nodegroup[i];
			ret |= __itmon_search_node(itmon, group, clear);
		}
	}
	itmon_analyze_errnode(itmon);

	spin_unlock_irqrestore(&itmon->ctrl_lock, flags);
	return ret;
}

static void itmon_do_dpm_policy(struct itmon_dev *itmon)
{
	struct itmon_platdata *pdata = itmon->pdata;
	int i;

	for (i = 0; i < TYPE_MAX; i++) {
		if (pdata->policy[i].error) {
			dev_err(itmon->dev, "%s: %s\n", __func__,
					pdata->policy[i].name);
			dbg_snapshot_do_dpm_policy(pdata->policy[i].policy);
			pdata->policy[i].error = false;
		}
	}
}

static irqreturn_t itmon_irq_handler(int irq, void *data)
{
	struct itmon_dev *itmon = (struct itmon_dev *)data;
	struct itmon_platdata *pdata = itmon->pdata;
	struct itmon_nodegroup *group = NULL;
	bool ret;
	int i;

	/* Search itmon group */
	for (i = 0; i < (int)ARRAY_SIZE(nodegroup); i++) {
		group = &pdata->nodegroup[i];
		dev_err(itmon->dev,
				"%s: %d irq, %s group, 0x%x vec",
				__func__, irq, group->name,
				group->phy_regs == 0 ? 0 : __raw_readl(group->regs));
	}

	ret = itmon_search_node(itmon, NULL, true);
	if (!ret) {
		dev_err(itmon->dev, "No errors found %s\n", __func__);
	} else {
		dev_err(itmon->dev,
			"Error detected: err_cnt:%u, err_cnt_by_cpu:%u\n",
			pdata->err_cnt, pdata->err_cnt_by_cpu);

		itmon_do_dpm_policy(itmon);
	}

	return IRQ_HANDLED;
}

void itmon_notifier_chain_register(struct notifier_block *block)
{
	atomic_notifier_chain_register(&itmon_notifier_list, block);
}
EXPORT_SYMBOL(itmon_notifier_chain_register);

static int itmon_logging_panic_handler(struct notifier_block *nb,
				     unsigned long l, void *buf)
{
	struct itmon_panic_block *itmon_panic = (struct itmon_panic_block *)nb;
	struct itmon_dev *itmon = itmon_panic->pdev;
	struct itmon_platdata *pdata = itmon->pdata;
	int ret;

	if (!IS_ERR_OR_NULL(itmon)) {
		/* Check error has been logged */
		ret = itmon_search_node(itmon, NULL, false);
		if (!ret) {
			dev_info(itmon->dev, "No errors found %s\n", __func__);
		} else {
			dev_err(itmon->dev,
				"Error detected: err_cnt:%u, err_cnt_by_cpu:%u\n",
				pdata->err_cnt, pdata->err_cnt_by_cpu);

			itmon_do_dpm_policy(itmon);
		}
	}
	return 0;
}

static void itmon_parse_dt(struct itmon_dev *itmon)
{
	struct device_node *np = itmon->dev->of_node;
	struct itmon_platdata *pdata = itmon->pdata;
	unsigned int val;
	int i;

	if (!of_property_read_u32(np, "panic_count", &val))
		pdata->panic_threshold = val;
	else
		pdata->panic_threshold = PANIC_THRESHOLD;

	dev_info(itmon->dev, "panic threshold: %d\n", pdata->panic_threshold);
	for (i = 0; i < TYPE_MAX; i++) {
		if (!of_property_read_u32(np, pdata->policy[i].name, &val))
			pdata->policy[i].policy = val;

		dev_info(itmon->dev, "policy: %s: [%d]\n",
				pdata->policy[i].name, pdata->policy[i].policy);
	}
}

static int itmon_probe(struct platform_device *pdev)
{
	struct itmon_dev *itmon;
	struct itmon_panic_block *itmon_panic = NULL;
	struct itmon_platdata *pdata;
	struct itmon_nodeinfo *node;
	unsigned int irq_option = 0, irq;
	char *dev_name;
	int ret, i, j;

	itmon = devm_kzalloc(&pdev->dev,
				sizeof(struct itmon_dev), GFP_KERNEL);
	if (!itmon)
		return -ENOMEM;

	itmon->dev = &pdev->dev;

	spin_lock_init(&itmon->ctrl_lock);

	pdata = devm_kzalloc(&pdev->dev,
				sizeof(struct itmon_platdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	itmon->pdata = pdata;
	itmon->pdata->masterinfo = masterinfo;
	itmon->pdata->rpathinfo = rpathinfo;
	itmon->pdata->nodegroup = nodegroup;
	itmon->pdata->policy = err_policy;

	itmon_parse_dt(itmon);

	for (i = 0; i < (int)ARRAY_SIZE(nodegroup); i++) {
		dev_name = nodegroup[i].name;
		node = nodegroup[i].nodeinfo;

		if (nodegroup[i].phy_regs) {
			nodegroup[i].regs =
				devm_ioremap_nocache(&pdev->dev,
						 nodegroup[i].phy_regs, SZ_16K);
			if (nodegroup[i].regs == NULL) {
				dev_err(&pdev->dev,
					"failed to claim register region - %s\n",
					dev_name);
				return -ENOENT;
			}
		}
#ifdef MULTI_IRQ_SUPPORT_ITMON
		irq_option = IRQF_GIC_MULTI_TARGET;
#endif
		irq = irq_of_parse_and_map(pdev->dev.of_node, i);
		nodegroup[i].irq = irq;

		ret = devm_request_irq(&pdev->dev, irq,
				       itmon_irq_handler, irq_option, dev_name, itmon);
		if (ret == 0) {
			dev_info(&pdev->dev,
				 "success to register request irq%u - %s\n",
				 irq, dev_name);
		} else {
			dev_err(&pdev->dev, "failed to request irq - %s\n",
				dev_name);
			return -ENOENT;
		}

		for (j = 0; j < nodegroup[i].nodesize; j++) {
			node[j].regs = devm_ioremap_nocache(&pdev->dev,
						node[j].phy_regs, SZ_16K);
			if (node[j].regs == NULL) {
				dev_err(&pdev->dev,
					"failed to claim register region - %s\n",
					dev_name);
				return -ENOENT;
			}
		}
	}

	itmon_panic = devm_kzalloc(&pdev->dev, sizeof(struct itmon_panic_block),
				 GFP_KERNEL);

	if (!itmon_panic) {
		dev_err(&pdev->dev, "failed to allocate memory for driver's "
				    "panic handler data\n");
	} else {
		itmon_panic->nb_panic_block.notifier_call = itmon_logging_panic_handler;
		itmon_panic->pdev = itmon;
		atomic_notifier_chain_register(&panic_notifier_list,
					       &itmon_panic->nb_panic_block);
	}

	platform_set_drvdata(pdev, itmon);

	pdata->cp_crash_in_progress = false;

	itmon_init(itmon, true);

	g_itmon = itmon;
	pdata->probed = true;

	for (i = 0; i < TRANS_TYPE_NUM; i++) {
		INIT_LIST_HEAD(&pdata->datalist[i]);
		INIT_LIST_HEAD(&pdata->infolist[i]);
	}

	dev_info(&pdev->dev, "success to probe Exynos ITMON driver\n");

	return 0;
}

static int itmon_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int itmon_suspend(struct device *dev)
{
	dev_info(dev, "%s\n", __func__);
	return 0;
}

static int itmon_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct itmon_dev *itmon = platform_get_drvdata(pdev);
	struct itmon_platdata *pdata = itmon->pdata;

	/* re-enable ITMON if cp-crash progress is not starting */
	if (!pdata->cp_crash_in_progress)
		itmon_init(itmon, true);

	dev_info(dev, "%s\n", __func__);
	return 0;
}

static SIMPLE_DEV_PM_OPS(itmon_pm_ops, itmon_suspend, itmon_resume);
#define ITMON_PM	(itmon_pm_ops)
#else
#define ITM_ONPM	NULL
#endif

static struct platform_driver exynos_itmon_driver = {
	.probe = itmon_probe,
	.remove = itmon_remove,
	.driver = {
		   .name = "exynos-itmon",
		   .of_match_table = itmon_dt_match,
		   .pm = &itmon_pm_ops,
		   },
};
module_platform_driver(exynos_itmon_driver);

MODULE_DESCRIPTION("Samsung Exynos ITMON DRIVER");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:exynos-itmon");
