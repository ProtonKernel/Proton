/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2012 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MODEM_V1_H__
#define __MODEM_V1_H__

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/types.h>
#include <linux/shm_ipc.h>
#include <linux/ratelimit.h>
#include <dt-bindings/soc/samsung/exynos-cpif.h>
#include <soc/samsung/memlogger.h>

#include "cp_btl.h"

#define MAX_STR_LEN		256
#define MAX_NAME_LEN		64
#define MAX_DUMP_LEN		20

#define SMC_ID		0x82000700
#define SMC_ID_CLK	0x82001011
#define SSS_CLK_ENABLE	0
#define SSS_CLK_DISABLE	1

enum modem_network {
	UMTS_NETWORK,
	CDMA_NETWORK,
	TDSCDMA_NETWORK,
	LTE_NETWORK,
	MAX_MODEM_NETWORK
};

struct __packed multi_frame_control {
	u8 id:7,
	   more:1;
};

enum io_mode {
	PIO,
	DMA
};

enum direction {
	TX = 0,
	UL = 0,
	AP2CP = 0,
	RX = 1,
	DL = 1,
	CP2AP = 1,
	TXRX = 2,
	ULDL = 2,
	MAX_DIR = 2
};

enum read_write {
	RD = 0,
	WR = 1,
	RDWR = 2
};

#define STR_CP_FAIL	"cp_fail"
#define STR_CP_WDT	"cp_wdt"	/* CP watchdog timer */

/**
 * struct modem_io_t - declaration for io_device
 * @name:	device name
 * @id:		for SIPC4, contains format & channel information
 *		(id & 11100000b)>>5 = format  (eg, 0=FMT, 1=RAW, 2=RFS)
 *		(id & 00011111b)    = channel (valid only if format is RAW)
 *		for SIPC5, contains only 8-bit channel ID
 * @format:	device format
 * @io_type:	type of this io_device
 * @link_type:	link_devices to use this io_device
 *		for example, LINKDEV_SHMEM or LINKDEV_PCIE
 */
struct modem_io_t {
	char *name;
	u32 ch;
	u32 format;
	u32 io_type;
	u32 link_type;
	u32 attrs;
	char *option_region;
#if 1/*IS_ENABLED(CONFIG_LINK_DEVICE_MEMORY_SBD)*/
	unsigned int ul_num_buffers;
	unsigned int ul_buffer_size;
	unsigned int dl_num_buffers;
	unsigned int dl_buffer_size;
#endif
};

struct modemlink_pm_data {
	char *name;
	struct device *dev;
	/* link power control 2 types : pin & regulator control */
	int (*link_ldo_enable)(bool enable);
	unsigned int gpio_link_enable;
	unsigned int gpio_link_active;
	unsigned int gpio_link_hostwake;
	unsigned int gpio_link_slavewake;
	int (*link_reconnect)(void *reconnect);

	/* usb hub only */
	int (*port_enable)(int i, int j);
	int (*hub_standby)(void *standby);
	void *hub_pm_data;
	bool has_usbhub;

	/* cpu/bus frequency lock */
	atomic_t freqlock;
	int (*freq_lock)(struct device *dev);
	int (*freq_unlock)(struct device *dev);

	int autosuspend_delay_ms; /* if zero, the default value is used */
	void (*ehci_reg_dump)(struct device *dev);
};

struct modemlink_pm_link_activectl {
	int gpio_initialized;
	int gpio_request_host_active;
};

#if IS_ENABLED(CONFIG_LINK_DEVICE_SHMEM) || IS_ENABLED(CONFIG_LINK_DEVICE_PCIE)
enum shmem_type {
	REAL_SHMEM,
	C2C_SHMEM,
	MAX_SHMEM_TYPE
};

struct modem_mbox {
	unsigned int mbx_ap2cp_msg;
	unsigned int mbx_cp2ap_msg;
	unsigned int mbx_ap2cp_wakeup;	/* CP_WAKEUP	*/
	unsigned int mbx_cp2ap_wakeup;	/* AP_WAKEUP	*/
	unsigned int mbx_ap2cp_status;	/* AP_STATUS	*/
	unsigned int mbx_cp2ap_status;	/* CP_STATUS	*/
	unsigned int mbx_cp2ap_wakelock; /* Wakelock for VoLTE */
	unsigned int mbx_cp2ap_ratmode; /* Wakelock for pcie */
	unsigned int mbx_ap2cp_kerneltime; /* Kernel time */

	unsigned int int_ap2cp_msg;
	unsigned int int_ap2cp_active;
	unsigned int int_ap2cp_wakeup;
	unsigned int int_ap2cp_status;
	unsigned int int_ap2cp_lcd_status;
	unsigned int int_ap2cp_llc_status;
	unsigned int int_ap2cp_uart_noti;

	unsigned int irq_cp2ap_msg;
	unsigned int irq_cp2ap_status;
	unsigned int irq_cp2ap_active;
	unsigned int irq_cp2ap_llc_status;
	unsigned int irq_cp2ap_wakeup;
	unsigned int irq_cp2ap_wakelock;
	unsigned int irq_cp2ap_rat_mode;
	unsigned int irq_cp2ap_change_ul_path;
};
#endif

/* platform data */
struct modem_data {
	char *name;
	u32 cp_num;

	struct modem_mbox *mbx;
	struct mem_link_device *mld;

	/* Modem component */
	enum modem_network modem_net;
	u32 modem_type;

	u32 link_type;
	char *link_name;
	unsigned long link_attrs;	/* Set of link_attr_bit flags	*/
	u32 interrupt_types;

	u32 protocol;

	/* SIPC version */
	u32 ipc_version;

	/* Information of IO devices */
	unsigned int num_iodevs;
	struct modem_io_t *iodevs;

	/* capability check */
	u32 capability_check;

	/* Modem link PM support */
	struct modemlink_pm_data *link_pm_data;

	/* SIM Detect polarity */
	bool sim_polarity;

	/* legacy buffer setting */
	u32 legacy_fmt_head_tail_offset;
	u32 legacy_fmt_buffer_offset;
	u32 legacy_fmt_txq_size;
	u32 legacy_fmt_rxq_size;
	u32 legacy_raw_head_tail_offset;
	u32 legacy_raw_buffer_offset;
	u32 legacy_raw_txq_size;
	u32 legacy_raw_rxq_size;

	/* several 4 byte length info in ipc region */
	u32 offset_ap_version;
	u32 offset_cp_version;
	u32 offset_cmsg_offset;
	u32 offset_srinfo_offset;
	u32 offset_clk_table_offset;
	u32 offset_buff_desc_offset;
	u32 offset_capability_offset;

	/* ctrl messages between cp and ap */
	u32 ap2cp_msg[2];
	u32 cp2ap_msg[2];
	u32 cp2ap_united_status[2];
	u32 ap2cp_united_status[2];
	u32 ap2cp_llc_status[2];
	u32 cp2ap_llc_status[2];
	u32 ap2cp_kerneltime[2];
	u32 ap2cp_kerneltime_sec[2];
	u32 ap2cp_kerneltime_usec[2];

	/* Status Bit Info */
	unsigned int sbi_lte_active_mask;
	unsigned int sbi_lte_active_pos;
	unsigned int sbi_cp_status_mask;
	unsigned int sbi_cp_status_pos;
	unsigned int sbi_cp2ap_wakelock_mask;
	unsigned int sbi_cp2ap_wakelock_pos;
	unsigned int sbi_cp2ap_rat_mode_mask;
	unsigned int sbi_cp2ap_rat_mode_pos;

	unsigned int sbi_pda_active_mask;
	unsigned int sbi_pda_active_pos;
	unsigned int sbi_ap_status_mask;
	unsigned int sbi_ap_status_pos;

#if IS_ENABLED(CONFIG_CP_LLC)
	unsigned int sbi_ap_llc_way_mask;
	unsigned int sbi_ap_llc_way_pos;
	unsigned int sbi_ap_llc_alloc_mask;
	unsigned int sbi_ap_llc_alloc_pos;
	unsigned int sbi_ap_llc_return_mask;
	unsigned int sbi_ap_llc_return_pos;
	unsigned int sbi_cp_llc_way_mask;
	unsigned int sbi_cp_llc_way_pos;
	unsigned int sbi_cp_llc_alloc_mask;
	unsigned int sbi_cp_llc_alloc_pos;
	unsigned int sbi_cp_llc_return_mask;
	unsigned int sbi_cp_llc_return_pos;
#endif

	unsigned int sbi_ap2cp_kerneltime_sec_mask;
	unsigned int sbi_ap2cp_kerneltime_sec_pos;
	unsigned int sbi_ap2cp_kerneltime_usec_mask;
	unsigned int sbi_ap2cp_kerneltime_usec_pos;

	unsigned int sbi_uart_noti_mask;
	unsigned int sbi_uart_noti_pos;
	unsigned int sbi_crash_type_mask;
	unsigned int sbi_crash_type_pos;
	unsigned int sbi_ds_det_mask;
	unsigned int sbi_ds_det_pos;
	unsigned int sbi_lcd_status_mask;
	unsigned int sbi_lcd_status_pos;

	/* ulpath offset for 2CP models */
	u32 ulpath_offset;

	/* control message offset */
	u32 cmsg_offset;

	/* srinfo settings */
	u32 srinfo_offset;
	u32 srinfo_size;

	/* clk_table offset */
	u32 clk_table_offset;

	/* new SIT buffer descriptor offset */
	u32 buff_desc_offset;

	/* capability */
	u32 capability_offset;
	u32 ap_capability_0;
	u32 ap_capability_1;

#if IS_ENABLED(CONFIG_MODEM_IF_LEGACY_QOS)
	/* SIT priority queue info */
	u32 legacy_raw_qos_head_tail_offset;
	u32 legacy_raw_qos_buffer_offset;
	u32 legacy_raw_qos_txq_size;
	u32 legacy_raw_qos_rxq_size; /* unused for now */
#endif
	struct cp_btl btl;	/* CP background trace log */

	void (*gpio_revers_bias_clear)(void);
	void (*gpio_revers_bias_restore)(void);
};

struct modem_irq {
	spinlock_t lock;
	unsigned int num;
	char name[MAX_NAME_LEN];
	unsigned long flags;
	bool active;
	bool registered;
};

struct cpif_gpio {
	unsigned int num;
	const char *label;
};

#if IS_ENABLED(CONFIG_OF)
#define mif_dt_read_enum(np, prop, dest) \
	do { \
		u32 val; \
		if (of_property_read_u32(np, prop, &val)) { \
			mif_err("%s is not defined\n", prop); \
			return -EINVAL; \
		} \
		dest = (__typeof__(dest))(val); \
	} while (0)

#define mif_dt_read_bool(np, prop, dest) \
	do { \
		u32 val; \
		if (of_property_read_u32(np, prop, &val)) { \
			mif_err("%s is not defined\n", prop); \
			return -EINVAL; \
		} \
		dest = val ? true : false; \
	} while (0)

#define mif_dt_read_string(np, prop, dest) \
	do { \
		if (of_property_read_string(np, prop, \
				(const char **)&dest)) { \
			mif_err("%s is not defined\n", prop); \
			return -EINVAL; \
		} \
	} while (0)

#define mif_dt_read_u32(np, prop, dest) \
	do { \
		u32 val; \
		if (of_property_read_u32(np, prop, &val)) { \
			mif_err("%s is not defined\n", prop); \
			return -EINVAL; \
		} \
		dest = val; \
	} while (0)

#define mif_dt_read_u32_noerr(np, prop, dest) \
	do { \
		u32 val; \
		if (!of_property_read_u32(np, prop, &val)) \
			dest = val; \
	} while (0)
#endif

#if IS_ENABLED(CONFIG_EXYNOS_MEMORY_LOGGER)
#define CPIF_MEMLOG_SIZE         (1*1024*1024)

struct cpif_memlog {
	struct memlog *desc;
	struct memlog_obj *log_obj_file;
	struct memlog_obj *log_obj;
	unsigned int log_enable;
};

extern struct cpif_memlog cpif_memlog;
extern unsigned int get_cpif_memlog_log_enabled(void);
extern struct memlog_obj *get_cpif_memlog_log_obj(void);

void cpif_init_memlog(struct platform_device *pdev);

#define pr_memlog(fmt, ...) 						\
({									\
	if (get_cpif_memlog_log_enabled())				\
		memlog_write_printf(get_cpif_memlog_log_obj(),		\
			MEMLOG_LEVEL_NOTICE,				\
			fmt, ##__VA_ARGS__);				\
})
#define pr_memlog_level(level, fmt, ...) 				\
({									\
	if (get_cpif_memlog_log_enabled())				\
		memlog_write_printf(get_cpif_memlog_log_obj(), level,	\
			fmt, ##__VA_ARGS__);				\
})
#define pr_memlog_level_ratelimited(level, fmt, ...)			\
({									\
	static DEFINE_RATELIMIT_STATE(_rs,				\
				      DEFAULT_RATELIMIT_INTERVAL,	\
				      DEFAULT_RATELIMIT_BURST);		\
									\
	if (__ratelimit(&_rs))						\
		if (get_cpif_memlog_log_enabled())			\
			memlog_write_printf(get_cpif_memlog_log_obj(),	\
				level,					\
				fmt, ##__VA_ARGS__);			\
})

#else
#define pr_memlog(fmt, args...)
#define pr_memlog_level(level, fmt, args...)
#define pr_memlog_level_ratelimited(level, fmt, ...)
#endif

#define LOG_TAG	"cpif: "
#define FUNC	(__func__)
#define CALLER	(__builtin_return_address(0))

#define mif_err_limited(fmt, ...)									\
({													\
	printk_ratelimited(KERN_ERR LOG_TAG "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__);		\
	pr_memlog_level_ratelimited(MEMLOG_LEVEL_ERR, "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__);	\
})
#define mif_err(fmt, ...)										\
({													\
	pr_err(LOG_TAG "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__);					\
	pr_memlog_level(MEMLOG_LEVEL_ERR, "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__);			\
})
#define mif_info_limited(fmt, ...)									\
({													\
	printk_ratelimited(KERN_INFO LOG_TAG "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__);		\
	pr_memlog_level_ratelimited(MEMLOG_LEVEL_NOTICE, "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__);	\
})
#define mif_info(fmt, ...)										\
({													\
	pr_info(LOG_TAG "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__);					\
	pr_memlog_level(MEMLOG_LEVEL_NOTICE, "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__);		\
})
#define mif_debug(fmt, ...)										\
({													\
	pr_debug(LOG_TAG "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__);					\
	pr_memlog_level(MEMLOG_LEVEL_DEBUG, "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__);		\
})
#define mif_trace(fmt, ...)										\
({													\
	printk(KERN_DEBUG "cpif: %s: %d: called(%pF): " fmt,						\
		__func__, __LINE__, __builtin_return_address(0), ##__VA_ARGS__);			\
	pr_memlog_level(MEMLOG_LEVEL_DEBUG, KERN_DEBUG "cpif: %s: %d: called(%pF): " fmt,		\
		__func__, __LINE__, __builtin_return_address(0), ##__VA_ARGS__); 			\
})
#endif
