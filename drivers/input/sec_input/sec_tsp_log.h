
#ifndef _SEC_TSP_LOG_H_
#define _SEC_TSP_LOG_H_

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/tick.h>
#include <linux/module.h>
#include <linux/memblock.h>
#include <linux/sched/clock.h>

#define SEC_TSP_LOG_BUF_SIZE		(256 * 1024)	/* 256 KB */
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
#define SEC_TSP_RAW_DATA_BUF_SIZE	(2 * 50 * 1024)	/* 100 KB */
#else
#define SEC_TSP_RAW_DATA_BUF_SIZE	(50 * 1024)	/* 50 KB */
#endif
#define SEC_TSP_COMMAND_HISTORY_BUF_SIZE	(10 * 1024)	/* 10 KB */
#define SEC_TSP_SPONGE_LOG_BUF_SIZE	(128 * 1024)	/* 128 KB */

#define SEC_TSP_LOG_TIMESTAMP_SIZE	50
#define SEC_TSP_LOG_EXTRA_SIZE		10

/**
 * sec_debug_tsp_log : Leave tsp log in tsp_msg file.
 * ( Timestamp + Tsp logs )
 * sec_debug_tsp_log_msg : Leave tsp log in tsp_msg file and
 * add additional message between timestamp and tsp log.
 * ( Timestamp + additional Message + Tsp logs )
 */
void sec_tsp_sponge_log(char *buf);
void sec_debug_tsp_log(char *fmt, ...);
void sec_debug_tsp_log_msg(char *msg, char *fmt, ...);
void sec_tsp_log_fix(void);
void sec_debug_tsp_raw_data(char *fmt, ...);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
void sec_debug_tsp_raw_data_msg(char mode, char *msg, char *fmt, ...);
void sec_tsp_raw_data_clear(char mode);
#else
void sec_debug_tsp_raw_data_msg(char *msg, char *fmt, ...);
void sec_tsp_raw_data_clear(void);
#endif

void sec_debug_tsp_command_history(char *buf);
#endif /* _SEC_TSP_LOG_H_ */
