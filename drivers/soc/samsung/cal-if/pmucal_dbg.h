/*
 * Exynos PMUCAL debug interface support.
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __PMUCAL_DBG_H
#define __PMUCAL_DBG_H __FILE__

enum PMUCAL_DBG_BLOCK {
	BLK_CPU,
	BLK_CLUSTER,
	BLK_LOCAL,
	BLK_SYSTEM,
	NUM_BLKS,
};

struct pmucal_latency {
	u64 min;
	u64 avg;
	u64 max;
};

struct pmucal_dbg_info {
	u32 block_id;
	void *pmucal_data;
	u32 emul_offset;
	u32 emul_bit;
	u32 emul_en;
	u32 emul_enabled;
	spinlock_t profile_lock;
	/* on/off latency profile - Pure PMU latency (FullSWPMU) */
	bool profile_started;
	u32 latency_offset;
	u64 off_cnt;
	struct pmucal_latency on;
	struct pmucal_latency* on1;
	struct pmucal_latency off;
	struct pmucal_latency* off1;
	/* aux latency profile - Kernel / EL3-Mon / FlexPMU */
	u32 aux_offset;
	u32 aux_size;
	struct pmucal_latency *aux;
};

#if defined(CONFIG_PMUCAL_DBG) || defined(CONFIG_PMUCAL_DBG_MODULE)
void pmucal_dbg_set_emulation(struct pmucal_dbg_info *dbg);
void pmucal_dbg_req_emulation(struct pmucal_dbg_info *dbg, bool en);
void pmucal_dbg_do_profile(struct pmucal_dbg_info *dbg, bool is_on);
int pmucal_dbg_init(void);
int pmucal_dbg_debugfs_init(void);
#else
static inline void pmucal_dbg_set_emulation(struct pmucal_dbg_info *dbg)
{
	return;
}
static inline void pmucal_dbg_req_emulation(struct pmucal_dbg_info *dbg, bool en)
{
	return;
}
static inline void pmucal_dbg_do_profile(struct pmucal_dbg_info *dbg, bool is_on)
{
	return;
}
static inline int pmucal_dbg_init(void)
{
	return 0;
}
static inline int pmucal_dbg_debugfs_init(void)
{
	return 0;
}
#endif

#endif /* __PMUCAL_DBG_H */
