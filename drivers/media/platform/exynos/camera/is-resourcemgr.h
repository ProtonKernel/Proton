/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_RESOURCE_MGR_H
#define IS_RESOURCE_MGR_H

#include <linux/notifier.h>
#include <linux/reboot.h>
#include "is-groupmgr.h"
#include "is-interface.h"
#include "is-hw-chain.h"

#if defined(CONFIG_EXYNOS_PM_QOS_MODULE)
#include <soc/samsung/exynos_pm_qos.h>
#else
#include <linux/pm_qos.h>
#endif

#define RESOURCE_TYPE_SENSOR0	0
#define RESOURCE_TYPE_SENSOR1	1
#define RESOURCE_TYPE_SENSOR2	2
#define RESOURCE_TYPE_SENSOR3	3
#define RESOURCE_TYPE_SENSOR4	4
#define RESOURCE_TYPE_SENSOR5	5
#define RESOURCE_TYPE_ISCHAIN	6
#define RESOURCE_TYPE_MAX	8

#if defined(ENABLE_CLOG_RESERVED_MEM)
#define CLOG_DSS_NAME		"log_camera"
#endif

/* Configuration of Timer function type by kernel version */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0))
#define IS_TIMER_PARAM_TYPE unsigned long
#else
#define IS_TIMER_PARAM_TYPE struct timer_list*
#endif
#define IS_TIMER_FUNC(__FUNC_NAME)	\
	static void __FUNC_NAME(IS_TIMER_PARAM_TYPE data)

/* Configuration of memblock_alloc function type by kernel version */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 20, 0))
#define is_memblock_alloc		memblock_alloc
#else
#define is_memblock_alloc		memblock_phys_alloc
#endif

#if defined(CONFIG_EXYNOS_PM_QOS) || defined(CONFIG_EXYNOS_PM_QOS_MODULE)
#define is_pm_qos_request		exynos_pm_qos_request
#define is_pm_qos_add_request		exynos_pm_qos_add_request
#define is_pm_qos_update_request	exynos_pm_qos_update_request
#define is_pm_qos_remove_request	exynos_pm_qos_remove_request
#define is_pm_qos_request_active	exynos_pm_qos_request_active
#else
#define is_pm_qos_request		pm_qos_request
#define is_pm_qos_add_request		pm_qos_add_request
#define is_pm_qos_update_request	pm_qos_update_request
#define is_pm_qos_remove_request	pm_qos_remove_request
#define is_pm_qos_request_active	pm_qos_request_active
#endif

#define LIB_VM_PHYS_ADDR	0x900000000

enum is_resourcemgr_state {
	IS_RM_COM_POWER_ON,
	IS_RM_SS0_POWER_ON,
	IS_RM_SS1_POWER_ON,
	IS_RM_SS2_POWER_ON,
	IS_RM_SS3_POWER_ON,
	IS_RM_SS4_POWER_ON,
	IS_RM_SS5_POWER_ON,
	IS_RM_ISC_POWER_ON,
	IS_RM_POWER_ON
};

enum is_dvfs_state {
	IS_DVFS_SEL_TABLE
};

enum is_binary_state {
	IS_BINARY_LOADED
};

#if defined(ENABLE_CLOG_RESERVED_MEM)
/* Reserved memory data */
struct is_resource_rmem {
	void *virt_addr;
	unsigned long phys_addr;
	size_t size;
};
#endif

struct is_dvfs_ctrl {
	struct mutex lock;
#if defined(QOS_INTCAM)
	int cur_int_cam_qos;
#endif
#if defined(QOS_TNR)
	int cur_tnr_qos;
#endif
#if defined(QOS_CSIS)
	int cur_csis_qos;
#endif
#if defined(QOS_ISP)
	int cur_isp_qos;
#endif
	int cur_int_qos;
	int cur_mif_qos;
	int cur_cam_qos;
	int cur_i2c_qos;
	int cur_disp_qos;
	int cur_hpg_qos;
	int cur_hmp_bst;
	int thr_int_qos;
	int thr_int_cam_qos;
	int thr_mif_qos;
	u32 dvfs_table_idx;
	u32 dvfs_table_max;
	ulong state;
	atomic_t thrott_state;
	const char *cur_cpus;

	struct is_dvfs_scenario_ctrl *static_ctrl;
	struct is_dvfs_scenario_ctrl *dynamic_ctrl;
	struct is_dvfs_scenario_ctrl *external_ctrl;

	bool bundle_operating;
};

struct is_clk_gate_ctrl {
	spinlock_t lock;
	unsigned long msk_state;
	int msk_cnt[GROUP_ID_MAX];
	u32 msk_lock_by_ischain[IS_STREAM_COUNT];
	struct exynos_is_clk_gate_info *gate_info;
	u32 msk_clk_on_off_state; /* on/off(1/0) state per ip */
	/*
	 * For check that there's too long clock-on period.
	 * This var will increase when clock on,
	 * And will decrease when clock off.
	 */
	unsigned long chk_on_off_cnt[GROUP_ID_MAX];
};

struct is_resource {
        struct platform_device                  *pdev;
        void __iomem                            *regs;
        atomic_t                                rsccount;
        u32                                     private_data;
};

#define LLC_DISABLE	0
struct is_global_param {
	struct mutex				lock;
	bool					video_mode;
	ulong					llc_state;
	ulong					state;
};

struct is_lic_sram {
	size_t					taa_sram[10];
	atomic_t				taa_sram_sum;
};

struct is_bts_scen {
	unsigned int		index;
	const char		*name;
};

struct is_resourcemgr {
	unsigned long				state;
	atomic_t				rsccount;
	atomic_t				qos_refcount; /* For multi-instance with SOC sensor */
	struct is_resource			resource_preproc;
	struct is_resource			resource_sensor0;
	struct is_resource			resource_sensor1;
	struct is_resource			resource_sensor2;
	struct is_resource			resource_sensor3;
	struct is_resource			resource_sensor4;
	struct is_resource			resource_sensor5;
	struct is_resource			resource_ischain;

	struct is_mem			mem;
	struct is_minfo			minfo;

	struct is_dvfs_ctrl		dvfs_ctrl;
	struct is_clk_gate_ctrl		clk_gate_ctrl;
	u32					cluster0;
	u32					cluster1;
	u32					cluster2;
	u32					hal_version;
#ifdef ENABLE_FW_SHARE_DUMP
	ulong					fw_share_dump_buf;
#endif

	/* tmu */
	struct notifier_block			tmu_notifier;
	u32					tmu_state;
	u32					limited_fps;

	/* bus monitor */
	struct notifier_block			itmon_notifier;

	void					*private_data;
	/* binary loading */
	unsigned long				binary_state;

#ifdef ENABLE_SHARED_METADATA
	/* shared meta data */
	spinlock_t			shared_meta_lock;
	struct camera2_shot			shared_shot;
#endif
#ifdef ENABLE_KERNEL_LOG_DUMP
	unsigned long long			kernel_log_time;
	void					*kernel_log_buf;
#endif
#if defined(ENABLE_CLOG_RESERVED_MEM)
	spinlock_t				slock_cdump;
	ulong					cdump_ptr;
	ulong					sfrdump_ptr;
#endif
	struct is_global_param		global_param;

	struct is_lic_sram			lic_sram;

	/* for critical section at get/put */
	struct mutex				rsc_lock;
	/* for sysreg setting */
	struct mutex				sysreg_lock;
	/* for qos setting */
	struct mutex				qos_lock;

	/* BTS */
	struct is_bts_scen			*bts_scen;
	u32					num_bts_scen;
	int					cur_bts_scen_idx;

	u32					shot_timeout;
	int					shot_timeout_tick;

#if defined(DISABLE_CORE_IDLE_STATE)
	struct work_struct                      c2_disable_work;
#endif
	u32					streaming_cnt;

	struct list_head			regulator_list;
};

int is_resourcemgr_probe(struct is_resourcemgr *resourcemgr, void *private_data, struct platform_device *pdev);
int is_resource_open(struct is_resourcemgr *resourcemgr, u32 rsc_type, void **device);
int is_resource_get(struct is_resourcemgr *resourcemgr, u32 rsc_type);
int is_resource_put(struct is_resourcemgr *resourcemgr, u32 rsc_type);
int is_resource_ioctl(struct is_resourcemgr *resourcemgr, struct v4l2_control *ctrl);
int is_logsync(struct is_interface *itf, u32 sync_id, u32 msg_test_id);
void is_resource_set_global_param(struct is_resourcemgr *resourcemgr, void *device);
void is_resource_clear_global_param(struct is_resourcemgr *resourcemgr, void *device);
int is_resource_update_lic_sram(struct is_resourcemgr *resourcemgr, void *device, bool on);
int is_resource_dump(void);
int is_kernel_log_dump(bool overwrite);
#ifdef ENABLE_HWACG_CONTROL
extern void is_hw_csi_qchannel_enable_all(bool enable);
#endif
void is_bts_scen(struct is_resourcemgr *resourcemgr,
	unsigned int index, bool enable);

#if defined(ENABLE_DYNAMIC_HEAP_FOR_DDK_RTA)
int is_heap_mem_alloc_dynamic(struct is_resourcemgr *resourcemgr,
	int type, int heap_size);
int is_heap_mem_free(struct is_resourcemgr *resourcemgr);
#endif

#if defined(ENABLE_CLOG_RESERVED_MEM)
int is_resource_cdump(void);
int cdump(const char *str, ...);
void hex_cdump(const char *level, const char *prefix_str, int prefix_type,
		    int rowsize, int groupsize,
		    const void *buf, size_t len, bool ascii);
#define cinfo(fmt, args...)	\
	cdump(fmt, ##args)
#else
#define cinfo(prefix, fmt, args...)	\
	info_common("", fmt, ##args)
#endif

#define GET_RESOURCE(resourcemgr, type) \
	((type == RESOURCE_TYPE_SENSOR0) ? &resourcemgr->resource_sensor0 : \
	((type == RESOURCE_TYPE_SENSOR1) ? &resourcemgr->resource_sensor1 : \
	((type == RESOURCE_TYPE_SENSOR2) ? &resourcemgr->resource_sensor2 : \
	((type == RESOURCE_TYPE_SENSOR3) ? &resourcemgr->resource_sensor3 : \
	((type == RESOURCE_TYPE_SENSOR4) ? &resourcemgr->resource_sensor4 : \
	((type == RESOURCE_TYPE_SENSOR5) ? &resourcemgr->resource_sensor5 : \
	((type == RESOURCE_TYPE_ISCHAIN) ? &resourcemgr->resource_ischain : \
	NULL)))))))

/* cache maintenance for user buffer */
#define MAX_DBUF_LIST	10

struct is_dbuf_list {
	struct list_head	list;
	struct dma_buf		*dbuf[IS_MAX_PLANES];
};

struct is_dbuf_q {
	struct is_dbuf_list	*dbuf_list[ID_DBUF_MAX];
	struct mutex		lock[ID_DBUF_MAX];

	u32			queu_count[ID_DBUF_MAX];
	struct list_head	queu_list[ID_DBUF_MAX];

	u32			free_count[ID_DBUF_MAX];
	struct list_head	free_list[ID_DBUF_MAX];
};

struct is_dbuf_q *is_init_dbuf_q(void);
void is_deinit_dbuf_q(struct is_dbuf_q *dbuf_q);
void is_q_dbuf_q(struct is_dbuf_q *dbuf_q, struct is_sub_dma_buf *sdbuf, u32 qcnt);
void is_dq_dbuf_q(struct is_dbuf_q *dbuf_q, u32 dma_id, enum dma_data_direction dir);

#endif
