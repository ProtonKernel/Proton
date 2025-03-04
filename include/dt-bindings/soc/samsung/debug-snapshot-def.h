/*
 * Debug-SnapShot for Samsung's SoC's.
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef DEBUG_SNAPSHOT_TABLE_H
#define DEBUG_SNAPSHOT_TABLE_H

/* MEMLOGGER */
#define DSS_MEMLOG_BL_BASE		(0x290)

/* KEVENT ID */
#define DSS_ITEM_HEADER                 "header"
#define DSS_ITEM_KERNEL                 "log_kernel"
#define DSS_ITEM_PLATFORM               "log_platform"
#define DSS_ITEM_FATAL                  "log_fatal"
#define DSS_ITEM_KEVENTS                "log_kevents"
#define DSS_ITEM_S2D                    "log_s2d"
#define DSS_ITEM_ARRDUMP_RESET          "log_arrdumpreset"
#define DSS_ITEM_ARRDUMP_PANIC          "log_arrdumppanic"
#define DSS_ITEM_FIRST		        "log_first"
#define DSS_ITEM_INIT_TASK		"log_init"

#define DSS_LOG_TASK                    "task_log"
#define DSS_LOG_WORK                    "work_log"
#define DSS_LOG_CPUIDLE                 "cpuidle_log"
#define DSS_LOG_SUSPEND                 "suspend_log"
#define DSS_LOG_IRQ                     "irq_log"
#define DSS_LOG_SPINLOCK                "spinlock_log"
#define DSS_LOG_HRTIMER                 "hrtimer_log"
#define DSS_LOG_CLK                     "clk_log"
#define DSS_LOG_PMU                     "pmu_log"
#define DSS_LOG_FREQ                    "freq_log"
#define DSS_LOG_DM                      "dm_log"
#define DSS_LOG_REGULATOR               "regulator_log"
#define DSS_LOG_THERMAL                 "thermal_log"
#define DSS_LOG_ACPM                    "acpm_log"
#define DSS_LOG_PRINTK                  "printk_log"

/* MODE */
#define NONE_DUMP                       0
#define FULL_DUMP                       1
#define QUICK_DUMP                      2

/* ACTION */
#define GO_DEFAULT                      "default"
#define GO_DEFAULT_ID                   0
#define GO_PANIC                        "panic"
#define GO_PANIC_ID                     1
#define GO_WATCHDOG                     "watchdog"
#define GO_WATCHDOG_ID                  2
#define GO_S2D                          "s2d"
#define GO_S2D_ID                       3
#define GO_ARRAYDUMP                    "arraydump"
#define GO_ARRAYDUMP_ID                 4
#define GO_SCANDUMP                     "scandump"
#define GO_SCANDUMP_ID                  5

/* EXCEPTION POLICY */
#define DPM_F                           "feature"
#define DPM_P                           "policy"

#define DPM_P_EL1_DA                    "el1_da"
#define DPM_P_EL1_IA                    "el1_ia"
#define DPM_P_EL1_UNDEF                 "el1_undef"
#define DPM_P_EL1_SP_PC                 "el1_sp_pc"
#define DPM_P_EL1_INV                   "el1_inv"
#define DPM_P_EL1_SERROR                "el1_serror"
#endif
