/*
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DISPLAY_PROFILER_H__
#define __DISPLAY_PROFILER_H__

#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/workqueue.h>

#include <media/v4l2-subdev.h>

#include "maskgen.h"
#include "../panel_debug.h"

#define PROFILER_VERSION 201029

#define prof_en(p, enable)	\
		(((p)->conf->profiler_en) && ((p)->conf->enable##_en) ? true : false)

#define prof_disp(p, enable)	\
		((((p)->conf->enable##_en) && ((p)->conf->enable##_disp)) ? true : false)

#define prof_dbg(p, enable)	\
		(((p)->conf->enable##_debug) ? true : false)

#define prof_info(p, en, fmt, ...)							\
	do {									\
		if ((p)->conf->en) {				\
			panel_info(pr_fmt(fmt), ##__VA_ARGS__);		\
		}								\
	} while (0)

#define CYCLE_TIME_DEFAULT 100

enum PROFILER_SEQ {
	PROFILE_WIN_UPDATE_SEQ = 0,
	PROFILE_DISABLE_WIN_SEQ,
	SEND_PROFILE_FONT_SEQ,
	INIT_PROFILE_FPS_SEQ,
	DISPLAY_PROFILE_FPS_SEQ,
	PROFILE_SET_COLOR_SEQ,
	PROFILER_SET_CIRCLR_SEQ,
	DISABLE_PROFILE_FPS_MASK_SEQ,
	WAIT_PROFILE_FPS_MASK_SEQ,
	MEM_SELECT_PROFILE_FPS_MASK_SEQ,
	ENABLE_PROFILE_FPS_MASK_SEQ,
	MAX_PROFILER_SEQ,
};


enum PROFILER_MAPTBL {
	PROFILE_WIN_UPDATE_MAP = 0,
	DISPLAY_PROFILE_FPS_MAP,
	PROFILE_SET_COLOR_MAP,
	PROFILE_SET_CIRCLE,
	PROFILE_FPS_MASK_POSITION_MAP,
	PROFILE_FPS_MASK_COLOR_MAP,
	MAX_PROFILER_MAPTBL,
};

#define FPS_COLOR_RED	0
#define FPS_COLOR_BLUE	1
#define FPS_COLOR_GREEN	2

struct profiler_tune {
	char *name;
	struct seqinfo *seqtbl;
	u32 nr_seqtbl;
	struct maptbl *maptbl;
	u32 nr_maptbl;
	struct profiler_config *conf;
	struct mprint_config *mprint_config;
};

struct prifiler_rect {
	u32 left;
	u32 top;
	u32 right;
	u32 bottom;

	bool disp_en;
};


struct fps_slot {
	ktime_t timestamp;
	unsigned int frame_cnt;
};


struct profiler_systrace {
	int pid;
};

#define FPS_MAX 100000
#define MAX_SLOT_CNT 5

struct profiler_fps {
	unsigned int frame_cnt;
	unsigned int prev_frame_cnt;

	unsigned int instant_fps;
	unsigned int average_fps;
	unsigned int comp_fps;

	ktime_t win_config_time;

	struct fps_slot slot[MAX_SLOT_CNT];
	unsigned int slot_cnt;
	unsigned int total_frame;
	unsigned int color;
};

#define MAX_TE_CNT 10
struct profiler_te {
	s64 last_time;
	s64 last_diff;
	s64 times[MAX_TE_CNT];
	int idx;
//	int consume_idx;
	spinlock_t slock;
};

struct profiler_hiber {
	bool hiber_status;
	int hiber_enter_cnt;
	int hiber_exit_cnt;
	s64 hiber_enter_time;
	s64 hiber_exit_time;
};

#define PLOG_SIZE 1024
#define PLOG_BUF_SIZE 524288
#define PLOG_BUF_SLICE_SIZE 32767

struct profiler_log_cmd {
	u8 *data;
	u32 size;
	u32 command;
	u32 offset;
	u32 option;
};

struct profiler_log_panel {
	char *name;
	u32 index;
	u32 size;
	u32 count;
};

struct profiler_log_state {
	u32 prev;
	u32 cur;
	char *prev_name;
	char *cur_name;
};

struct profiler_log {
	u32 header;
	struct timespec64 time;
	union {
		struct profiler_log_cmd cmd;
		struct profiler_log_panel panel;
		struct profiler_log_state state;
	};
};
#define PLOG_SET_DIR(x, val) ((x) = ((x) & 0x3FFFFFFF) | ((val & 0x3) << 30))
#define PLOG_GET_DIR(x) (((x) >> 30) & 0x3)
/* plog_header_direction[31:30] */
enum {
	PLOG_DIRECTION_READ = 0b01,
	PLOG_DIRECTION_WRITE = 0b10,
};

#define PLOG_SET_TYPE(x, val) ((x) = ((x) & 0xC0FFFFFF) | ((val & 0x3F) << 24))
#define PLOG_GET_TYPE(x) (((x) >> 24) & 0x3F)
/* plog_header_type[29:24] */
enum {
	PLOG_CMD_DSI = 1,
	PLOG_CMD_DSI_FLUSH = 2,
	PLOG_CMD_SPI = 3,
	PLOG_PANEL = 4,
	MAX_PLOG_TYPE,
};

#define PLOG_SET_SUBTYPE(x, val) ((x) = ((x) & 0xFFFF0000) | (val & 0xFFFF))
#define PLOG_GET_SUBTYPE(x) ((x) & 0xFFFF)
/* pkt_type_subtype[15:0] */
enum {
	PLOG_DSI_UNKNOWN = 0,
	PLOG_DSI_GEN_CMD = 1 << 0,
	PLOG_DSI_CMD_NO_WAKE = 1 << 1,
	PLOG_DSI_DSC_CMD = 1 << 2,
	PLOG_DSI_PPS_CMD = 1 << 3,
	PLOG_DSI_GRAM_CMD = 1 << 4,
	PLOG_DSI_SRAM_CMD = 1 << 5,
	PLOG_DSI_SR_FAST_CMD = 1 << 6,
};
enum {
	PLOG_PANEL_UNKNOWN = 0,
	PLOG_PANEL_CMD_FLUSH_START = 1 << 0,
	PLOG_PANEL_CMD_FLUSH_END = 1 << 1,
	PLOG_PANEL_SEQ_START = 1 << 2,
	PLOG_PANEL_SEQ_END = 1 << 3,
	PLOG_PANEL_STATE = 1 << 4,
};
/* pkt_header[15:0] Reserved */

enum {
	LOG_LEVEL_NONE = 0,
	LOG_LEVEL_CMD = 1,
	LOG_LEVEL_CMD_DATA = 2,
};

enum {
	LOG_DISP_NONE = 0,
	LOG_DISP_ASYNC = 1,
	LOG_DISP_SYNC = 2,
};

enum log_initialize_state {
	LOG_INIT_FAILED = -1,
	LOG_UNINITIALIZED = 0,
	LOG_INITIALIZED = 1,
};

#define PLOG_CMD_DATA_MAX_SIZE 256
#define PLOG_CMD_FILTER_SIZE 255
//#define PLOG_PROTO_FILTER_SIZE 63

struct plog_type_map {
	char *name;
	int flag;
	int nr_sub;
	char **sub;
};

struct profiler_config {
	int profiler_en;
	int profiler_debug;
	int systrace;
	int timediff_en;
	int cycle_time;

	int fps_en;
	int fps_disp;
	int fps_debug;

	int te_en;
	int te_disp;
	int te_debug;

	int hiber_en;
	int hiber_disp;
	int hiber_debug;

	int log_en;
	int log_debug;
	int log_disp;
	int log_level;
	int log_filter_en;

	int mprint_en;
	int mprint_debug;
};

struct profiler_device {
	bool initialized;
	struct v4l2_subdev sd;
	struct miscdevice log_dev;
	bool log_dev_opened;

	struct seqinfo *seqtbl;
	u32 nr_seqtbl;
	struct maptbl *maptbl;
	u32 nr_maptbl;

	struct prifiler_rect win_rect;
	//struct profile_data *data;

	struct profiler_fps fps;

	int prev_win_cnt;
	int flag_font;
	unsigned int circle_color;

	struct profiler_systrace systrace;

	struct task_struct *thread;

	struct profiler_config *conf;

	struct profiler_te te_info;
	struct profiler_hiber hiber_info;

	struct mprint_config *mask_config;
	struct mprint_props mask_props;

	enum log_initialize_state log_initialized;
	int log_idx_head;
	int log_idx_tail_print;
	int log_idx_tail_file;
	struct profiler_log *log_list;
	int log_data_idx;
	u8 *log_data;

};

#define PROFILER_IOC_BASE	'P'

#define PROFILE_REG_DECON	_IOW(PROFILER_IOC_BASE, 1, struct profile_data *)
#define PROFILE_WIN_UPDATE	_IOW(PROFILER_IOC_BASE, 2, struct decon_rect *)
#define PROFILE_WIN_CONFIG	_IOW(PROFILER_IOC_BASE, 3, struct decon_win_config_data *)
#define PROFILER_SET_PID	_IOW(PROFILER_IOC_BASE, 4, int *)
#define PROFILER_COLOR_CIRCLE	_IOW(PROFILER_IOC_BASE, 5, int *)
#define PROFILE_TE			_IOW(PROFILER_IOC_BASE, 6, s64)
#define PROFILE_HIBER_ENTER _IOW(PROFILER_IOC_BASE, 7, s64)
#define PROFILE_HIBER_EXIT  _IOW(PROFILER_IOC_BASE, 8, s64)
#define PROFILE_LOG	_IOW(PROFILER_IOC_BASE, 9, struct profiler_log_cmd *)

int profiler_probe(struct panel_device *panel, struct profiler_tune *profile_tune);
int profiler_remove(struct panel_device *panel);
void profiler_log_show(struct seq_file *s, struct panel_device *panel);

#endif //__DISPLAY_PROFILER_H__

