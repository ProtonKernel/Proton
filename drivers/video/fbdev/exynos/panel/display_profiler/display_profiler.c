/*
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>

#include "../panel_drv.h"
#include "display_profiler.h"

#ifdef PANEL_PR_TAG
#undef PANEL_PR_TAG
#define PANEL_PR_TAG	"prof"
#endif

#define STORE_BUFFER_SIZE 1024
#define PROFILER_DECON_SYSTRACE_ENABLE 0

static char *profiler_config_names[] = {
	"profiler_en",
	"profiler_debug",
	"systrace",
	"timediff_en",
	"cycle_time",

	"fps_en",
	"fps_disp",
	"fps_debug",

	"te_en",
	"te_disp",
	"te_debug",

	"hiber_en",
	"hiber_disp",
	"hiber_debug",

	"log_en",
	"log_debug",
	"log_disp",
	"log_level",
	"log_filter_en",

	"mprint_en",
	"mprint_debug",
};

static char *plog_subtype_cmd_dsi[] = {
	"PLOG_DSI_GEN_CMD",
	"PLOG_DSI_CMD_NO_WAKE",
	"PLOG_DSI_DSC_CMD",
	"PLOG_DSI_PPS_CMD",
	"PLOG_DSI_GRAM_CMD",
	"PLOG_DSI_SRAM_CMD",
	"PLOG_DSI_SR_FAST_CMD",
};

static char *plog_subtype_panel[] = {
	"PLOG_PANEL_CMD_FLUSH_START",
	"PLOG_PANEL_CMD_FLUSH_END",
	"PLOG_PANEL_SEQ_START",
	"PLOG_PANEL_SEQ_END",
	"PLOG_PANEL_STATE",
};

static bool plog_cmd_filter[PLOG_CMD_FILTER_SIZE + 1] = { false, };

struct plog_type_map PLOG_TYPE_MAP[MAX_PLOG_TYPE] = {
	[PLOG_CMD_DSI] = {
		.name = "CMD_DSI",
		.flag = 0x0000,
		.sub = plog_subtype_cmd_dsi,
		.nr_sub = ARRAY_SIZE(plog_subtype_cmd_dsi),
	},
	[PLOG_CMD_DSI_FLUSH] = {
		.name = "CMD_DSI_FLUSH",
		.flag = 0x0000,
		.sub = plog_subtype_cmd_dsi,
		.nr_sub = ARRAY_SIZE(plog_subtype_cmd_dsi),
	},
	[PLOG_CMD_SPI] = {
		.name = "CMD_SPI",
		.flag = 0x0000,
		.sub = NULL,
		.nr_sub = 0,
	},
	[PLOG_PANEL] = {
		.name = "PANEL",
		.flag = 0x0014,
		.sub = plog_subtype_panel,
		.nr_sub = ARRAY_SIZE(plog_subtype_panel),
	},
};

static char *plog_get_subtype_str(int header)
{
	int i, sub;

	if (PLOG_GET_TYPE(header) >= MAX_PLOG_TYPE)
		return NULL;

	sub = PLOG_GET_SUBTYPE(header);
	for (i = 0; i < PLOG_TYPE_MAP[PLOG_GET_TYPE(header)].nr_sub; i++) {
		if ((sub >> i) & 0x1)
			return PLOG_TYPE_MAP[PLOG_GET_TYPE(header)].sub[i];
	}
	return NULL;
}

static inline char *plog_get_type_str(int header)
{
	if (PLOG_GET_TYPE(header) >= MAX_PLOG_TYPE)
		return NULL;

	return PLOG_TYPE_MAP[PLOG_GET_TYPE(header)].name;
}

static int plog_get_type_idx(char *f)
{
	int i, len;

	panel_info("++ %s\n", f);

	if (f == NULL)
		return -1;

	len = strlen(f);

	panel_info("%s len %d\n", f, len);

	for (i = 0; i < MAX_PLOG_TYPE; i++) {
		panel_info("[%d] %s %p\n", i, PLOG_TYPE_MAP[i].name, PLOG_TYPE_MAP[i].name);
		if (PLOG_TYPE_MAP[i].name == NULL)
			continue;
		panel_info("[%d] strlen %d\n", i, strlen(PLOG_TYPE_MAP[i].name));
		if (strlen(PLOG_TYPE_MAP[i].name) != len)
			continue;
		if (strncmp(f, PLOG_TYPE_MAP[i].name, len) == 0)
			return i;
	}
	return -1;

}

static inline int plog_get_type_flag(int header)
{
	if (PLOG_GET_TYPE(header) >= MAX_PLOG_TYPE)
		return 0;

	return PLOG_TYPE_MAP[PLOG_GET_TYPE(header)].flag;

}

static int profiler_do_seqtbl_by_index_nolock(struct profiler_device *p, int index)
{
	int ret;
	struct seqinfo *tbl;
	struct panel_info *panel_data;
	struct panel_device *panel = container_of(p, struct panel_device, profiler);
	struct timespec64 cur_ts = { 0, };
	struct timespec64 last_ts = { 0, };
	struct timespec64 delta_ts = { 0, };
	s64 elapsed_usec = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (!IS_PANEL_ACTIVE(panel))
		return 0;

	panel_data = &panel->panel_data;
	tbl = panel->profiler.seqtbl;
	ktime_get_ts64(&cur_ts);

#if 0
	ktime_get_ts64(&last_ts);
#endif

	if (unlikely(index < 0 || index >= MAX_PROFILER_SEQ)) {
		panel_err("invalid parameter (panel %p, index %d)\n",
				panel, index);
		ret = -EINVAL;
		goto do_exit;
	}

#if 0
	delta_ts = timespec_sub(last_ts, cur_ts);
	elapsed_usec = timespec64_to_ns(&delta_ts) / 1000;
	if (elapsed_usec > 34000)
		panel_dbg("seq:%s warn:elapsed %lld.%03lld msec to acquire lock\n",
				tbl[index].name, elapsed_usec / 1000, elapsed_usec % 1000);
#endif
	ret = panel_do_seqtbl(panel, &tbl[index]);
	if (unlikely(ret < 0)) {
		panel_err("failed to excute seqtbl %s\n",
				tbl[index].name);
		ret = -EIO;
		goto do_exit;
	}

do_exit:
	ktime_get_ts64(&last_ts);
	delta_ts = timespec64_sub(last_ts, cur_ts);
	elapsed_usec = timespec64_to_ns(&delta_ts) / 1000;
	panel_dbg("seq:%s done (elapsed %2lld.%03lld msec)\n",
			tbl[index].name, elapsed_usec / 1000, elapsed_usec % 1000);

	return 0;
}

int profiler_do_seqtbl_by_index(struct profiler_device *p, int index)
{
	int ret = 0;
	struct panel_device *panel = container_of(p, struct panel_device, profiler);

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	mutex_lock(&panel->op_lock);
	ret = profiler_do_seqtbl_by_index_nolock(p, index);
	mutex_unlock(&panel->op_lock);

	return ret;
}

int log_to_str(struct profiler_log *log, char *buf, int buf_size)
{
	int len = 0;
	bool dir_read, dir_write, time_print = true;
	u32 i;

	switch (PLOG_GET_TYPE(log->header)) {
	case PLOG_CMD_DSI_FLUSH:
		time_print = false;
		break;
	case PLOG_PANEL:
		switch (PLOG_GET_SUBTYPE(log->header)) {
		case PLOG_PANEL_CMD_FLUSH_END:
		case PLOG_PANEL_SEQ_END:
			time_print = false;
			break;
		default:
			break;
		}
		break;
	default:
		time_print = true;
		break;
	}

	if (time_print)
		len += snprintf(buf + len, buf_size - len,
				"[ %6ld.%06ld ] %s ",
				log->time.tv_sec,
				log->time.tv_nsec / 1000,
				plog_get_type_str(log->header));
	else
		len += snprintf(buf + len, buf_size - len,
				"                  %s ",
				plog_get_type_str(log->header));

	switch (PLOG_GET_TYPE(log->header)) {
	case PLOG_CMD_DSI_FLUSH:
	case PLOG_CMD_DSI:
		//command log print
		dir_read = PLOG_GET_DIR(log->header) == PLOG_DIRECTION_READ;
		dir_write = PLOG_GET_DIR(log->header) == PLOG_DIRECTION_WRITE;
		len += snprintf(buf + len, buf_size - len,
				"0x%02x %s%s reg 0x%02x, ofs 0x%02x, len %d",
				PLOG_GET_SUBTYPE(log->header),
				(dir_read ? "R":""),
				(dir_write ? "W":""),
				log->cmd.command,
				log->cmd.offset,
				log->cmd.size);
		if (log->cmd.data != NULL) {
			i = 0;
			while (i < PLOG_CMD_DATA_MAX_SIZE && i < log->cmd.size) {
				len += snprintf(buf + len, buf_size - len, "\n");
				len += hex_dump_to_buffer(log->cmd.data + i,
						log->cmd.size - i, 32, 1,
						buf + len, buf_size - len, false);
				if (len >= buf_size)
					break;
				i += 32;
			}
		}
		break;
	case PLOG_PANEL:
		//panel log print
		len += snprintf(buf + len, buf_size - len,
				"%s ", plog_get_subtype_str(log->header));
		switch (PLOG_GET_SUBTYPE(log->header)) {
		case PLOG_PANEL_CMD_FLUSH_START:
			len += snprintf(buf + len, buf_size - len,
					"cmdcnt %d, payload %d",
					log->panel.count, log->panel.size);
			break;
		case PLOG_PANEL_CMD_FLUSH_END:
			len += snprintf(buf + len, buf_size - len,
					"cmdcnt %d, payload %d, elapsed: %lld us",
					log->panel.count, log->panel.size,
					timespec64_to_ns(&log->time) / 1000);
			break;
		case PLOG_PANEL_SEQ_START:
		case PLOG_PANEL_SEQ_END:
			len += snprintf(buf + len, buf_size - len,
					"pktcnt %d depth %d\tname %s",
					log->panel.size, log->panel.index, log->panel.name);
			break;
		case PLOG_PANEL_STATE:
			len += snprintf(buf + len, buf_size - len,
					"%s(%d) -> %s(%d)",
					log->state.prev_name, log->state.prev,
					log->state.cur_name, log->state.cur);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return len;
}

#define PROFILER_SYSTRACE_BUF_SIZE	40
#if PROFILER_DECON_SYSTRACE_ENABLE
//extern int decon_systrace_enable;
#endif
void profiler_systrace_write(int pid, char id, char *str, int value)
{
#if PROFILER_DECON_SYSTRACE_ENABLE
	char buf[PROFILER_SYSTRACE_BUF_SIZE] = {0, };

	if (!decon_systrace_enable)
		return;

	switch (id) {
	case 'B':
		snprintf(buf, PROFILER_SYSTRACE_BUF_SIZE, "B|%d|%s", pid, str);
		break;
	case 'E':
		strcpy(buf, "E");
		break;
	case 'C':
		snprintf(buf, PROFILER_SYSTRACE_BUF_SIZE,
			"C|%d|%s|%d", pid, str, 1);
		break;
	default:
		panel_err("wrong argument : %c\n", id);
		return;
	}
	panel_info("%s\n", buf);
	trace_printk(buf);
#endif
}

static int update_profile_hiber(struct profiler_device *p, bool enter_exit, s64 time_us)
{
	int ret = 0;
	struct profiler_hiber *hiber_info;

	hiber_info = &p->hiber_info;

	if (hiber_info->hiber_status == enter_exit) {
		panel_err("status already %s\n", enter_exit ? "ENTER" : "EXIT");
		return ret;
	}

	if (enter_exit) {
		hiber_info->hiber_enter_cnt++;
		hiber_info->hiber_enter_time = time_us;
	} else {
		hiber_info->hiber_exit_cnt++;
		hiber_info->hiber_exit_time = time_us;
	}

	prof_info(p, hiber_debug, "status %s -> %s\n",
		hiber_info->hiber_status ? "ENTER" : "EXIT",
		enter_exit ? "ENTER" : "EXIT");

	hiber_info->hiber_status = enter_exit;

	return ret;
}

static int update_profile_te(struct profiler_device *p, s64 time_us)
{
	int ret = 0;
	struct profiler_te *te_info;

	te_info = &p->te_info;

	spin_lock(&te_info->slock);
	if (time_us == 0) {
		te_info->last_time = 0;
		te_info->last_diff = 0;
		spin_unlock(&te_info->slock);
		prof_info(p, te_debug, "reset\n");
		return ret;
	}
	if (te_info->last_time == time_us) {
		spin_unlock(&te_info->slock);
		prof_info(p, te_debug, "skipped\n");
		return ret;
	}
	if (te_info->last_time == 0) {
		te_info->last_time = time_us;
		spin_unlock(&te_info->slock);
		prof_info(p, te_debug, "last time: %lld\n", te_info->last_time);
		return ret;
	}

	te_info->last_diff = time_us - te_info->last_time;
	te_info->last_time = time_us;
	te_info->times[te_info->idx++] = te_info->last_diff;
	te_info->idx = te_info->idx % MAX_TE_CNT;
	spin_unlock(&te_info->slock);

	prof_info(p, te_debug, "last time: %lld, diff: %lld\n", te_info->last_time, te_info->last_diff);

	return ret;
}

static int update_profile_win_config(struct profiler_device *p)
{
	int ret = 0;

	s64 diff;
	ktime_t timestamp;

	timestamp = ktime_get();

	if (p->fps.frame_cnt < FPS_MAX)
		p->fps.frame_cnt++;
	else
		p->fps.frame_cnt = 0;

	if (ktime_to_us(p->fps.win_config_time) != 0) {
		diff = ktime_to_us(ktime_sub(timestamp, p->fps.win_config_time));
		p->fps.instant_fps = (int)(1000000 / diff);

		if (p->fps.instant_fps >= 59)
			p->fps.color = FPS_COLOR_RED;
		else
			p->fps.color = FPS_COLOR_GREEN;

#if PROFILER_DECON_SYSTRACE_ENABLE
		if (p->conf->systrace)
			decon_systrace(get_decon_drvdata(0), 'C', "fps_inst", (int)p->fps.instant_fps);
#endif
	}
	p->fps.win_config_time = timestamp;

	return ret;
}

struct win_config_backup {
	int state;
	struct decon_frame src;
	struct decon_frame dst;
};

static inline bool profiler_is_log_initialized(struct profiler_device *p)
{
	return p->log_initialized == LOG_INITIALIZED ? true : false;
}

static int profiler_log_init(struct profiler_device *p)
{
	if (p->log_initialized == LOG_INIT_FAILED)
		return -EINVAL;

	if (p->log_idx_head >= 0) {
		//already initialized
		p->log_idx_head = 0;
		p->log_idx_tail_print = 0;
		p->log_idx_tail_file = 0;
		p->log_data_idx = 0;
		return 0;
	}

	p->log_list = kvmalloc(sizeof(struct profiler_log) * PLOG_SIZE, GFP_KERNEL);
	if (!p->log_list)
		goto err_mem;

	p->log_data = kvmalloc(sizeof(u8) * PLOG_BUF_SIZE, GFP_KERNEL);
	if (!p->log_data) {
		kfree(p->log_list);
		goto err_mem;
	}
	p->log_idx_head = 0;
	p->log_idx_tail_print = 0;
	p->log_idx_tail_file = 0;
	p->log_data_idx = 0;
	p->log_initialized = LOG_INITIALIZED;
	return 0;

err_mem:
	p->log_initialized = LOG_INIT_FAILED;
	return -ENOMEM;
}

static inline int profiler_log_inc_head(struct profiler_device *p)
{
	if (!profiler_is_log_initialized(p)) {
		if (profiler_log_init(p) < 0) {
			panel_err("profiler log init failed\n");
			return -EINVAL;
		}
		return p->log_idx_head;
	}

	p->log_idx_head = (p->log_idx_head + 1) % PLOG_SIZE;
	if (p->log_idx_head == p->log_idx_tail_print)
		p->log_idx_tail_print = (p->log_idx_head + 1) % PLOG_SIZE;

	if (p->log_idx_head == p->log_idx_tail_file)
		p->log_idx_tail_file = (p->log_idx_head + 1) % PLOG_SIZE;

	return p->log_idx_head;
}

static inline int profiler_log_get_head(struct profiler_device *p)
{
	if (!profiler_is_log_initialized(p)) {
		panel_err("profiler is not initialized\n");
		return -EINVAL;
	}

	return p->log_idx_head;
}

static int print_plog(struct profiler_device *p)
{
	struct profiler_log *c;
	int h, t;
	char buf[1024];

	if (!profiler_is_log_initialized(p))
		return -EINVAL;

	if (!prof_disp(p, log))
		return 0;

	t = p->log_idx_tail_print;
	h = profiler_log_get_head(p);
	if (h < 0)
		return -EINVAL;

	if (t == h)
		return 0;

	do {
		t = (t + 1) % PLOG_SIZE;
		c = &p->log_list[t];
		log_to_str(c, buf, 1024);
		prof_info(p, log_disp, "log[%d] %s\n", t, buf);
	} while (t != h);

	p->log_idx_tail_print = t;
	return 0;
}

static u8 *insert_plog_data(struct profiler_device *p, u8 *data, int size)
{
	u8 *target;

	if (size > PLOG_BUF_SLICE_SIZE) {
		prof_info(p, log_debug, "data is too long(%d), skip logging after %d\n", size, PLOG_BUF_SLICE_SIZE);
		size = PLOG_BUF_SLICE_SIZE;
	}

	if (p->log_data_idx + size + 1 > PLOG_BUF_SIZE) {
		prof_info(p, log_debug, "databuffer rewind %d %d\n", p->log_data_idx, size);
		p->log_data_idx = 0;
	}

	target = p->log_data + p->log_data_idx;
	memcpy(target, data, size);
	// terminating null
	memset(target + size, '\0', sizeof(u8));
	p->log_data_idx = p->log_data_idx + size + 1;

	return target;
}

static int insert_plog(struct profiler_device *p, struct profiler_log *log)
{
	int ret = 0;
	int idx;
	struct profiler_config *conf = p->conf;
	struct profiler_log *c = NULL;
	struct timespec64 cur_ts = { 0, };
	struct timespec64 last_ts = { 0, };
	struct timespec64 delta_ts = { 0, };
	s64 elapsed_nsec;
	u32 header_type;
	u32 header_subtype;
	s32 subtype_filter;

	if (!profiler_is_log_initialized(p)) {
		//not initialized
		return -EINVAL;
	}

	subtype_filter = plog_get_type_flag(log->header);
	header_type = PLOG_GET_TYPE(log->header);
	header_subtype = PLOG_GET_SUBTYPE(log->header);

	if ((header_subtype & subtype_filter) == 0) {
		prof_info(p, log_debug, "filtered type %d subtype 0x%04X filter 0x%08X\n",
			header_type, header_subtype, subtype_filter);
		return 0;
	}

	if (header_type == PLOG_CMD_DSI && prof_en(p, log_filter)) {
		if ((log->cmd.command & 0xFF) < PLOG_CMD_FILTER_SIZE + 1) {
			if (plog_cmd_filter[(log->cmd.command & 0xFF)] == false) {
				prof_info(p, log_debug, "filtered command 0x%02x, type 0x%02x ofs 0x%02x, len %d\n",
						 log->cmd.command, log->header, log->cmd.offset, log->cmd.size);
				return 0;
			}
		}
	}

	if (prof_en(p, timediff))
		ktime_get_ts64(&cur_ts);

	idx = profiler_log_inc_head(p);
	if (idx < 0)
		return 0;
	c = &p->log_list[idx];
	memcpy(c, log, sizeof(struct profiler_log));

	switch (PLOG_GET_TYPE(c->header)) {
	case PLOG_CMD_DSI:
	case PLOG_CMD_DSI_FLUSH:
		c->cmd.data = NULL;
		if (conf->log_level >= LOG_LEVEL_CMD_DATA) {
			switch (PLOG_GET_SUBTYPE(c->header)) {
			case PLOG_DSI_GEN_CMD:
			case PLOG_DSI_CMD_NO_WAKE:
			case PLOG_DSI_DSC_CMD:
			case PLOG_DSI_PPS_CMD:
				if (log->cmd.data == NULL) {
					prof_info(p, log_debug,
						"data is NULL. log[%d] type 0x%02x cmd 0x%02x, ofs 0x%02x, len %d\n",
						idx, c->header, c->cmd.command, c->cmd.offset, c->cmd.size);
					break;
				}
				c->cmd.data = insert_plog_data(p, log->cmd.data, log->cmd.size);
				break;
			case PLOG_DSI_GRAM_CMD:
			case PLOG_DSI_SRAM_CMD:
			case PLOG_DSI_SR_FAST_CMD:
			default:
				break;
			}
		}
		break;
	default:
		break;
	}

	if (prof_en(p, timediff)) {
		ktime_get_ts64(&last_ts);
		delta_ts = timespec64_sub(last_ts, cur_ts);
		elapsed_nsec = timespec64_to_ns(&delta_ts);
		prof_info(p, timediff_en, "log elapsed = %lldns, 0x%02x len %d\n",
			elapsed_nsec, c->cmd.command, c->cmd.size);
	}

	if (p->conf->log_disp == LOG_DISP_SYNC) {
		prof_info(p, log_debug, "head %d tail_print %d tail_file %d\n",
			p->log_idx_head, p->log_idx_tail_print, p->log_idx_tail_file);
		print_plog(p);
	}

	prof_info(p, log_debug, "end idx head %d tail_print %d tail_file %d\n",
		p->log_idx_head, p->log_idx_tail_print, p->log_idx_tail_file);

	return ret;
}

static long profiler_v4l2_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct panel_device *panel;
	struct profiler_device *p = container_of(sd, struct profiler_device, sd);

	if (p->conf == NULL)
		return (long)ret;

	panel = container_of(p, struct panel_device, profiler);

	switch (cmd) {
	case PROFILE_REG_DECON:
		panel_info("PROFILE_REG_DECON was called\n");
		break;

	case PROFILE_WIN_UPDATE:
#if 0
		if (p->win_rect.disp_en)
			ret = update_profile_win(p, arg);
#endif
		break;

	case PROFILE_WIN_CONFIG:
		if (prof_en(p, fps))
			ret = update_profile_win_config(p);
		break;
	case PROFILE_TE:
		if (prof_en(p, te))
			update_profile_te(p, (s64)arg);
		break;
	case PROFILE_HIBER_ENTER:
#if 0
		if (prof_en(p, te))
			update_profile_te(p, 0);
#endif
		if (prof_en(p, hiber))
			update_profile_hiber(p, true, (s64)arg);
		break;
	case PROFILE_HIBER_EXIT:
#if 0
		if (prof_en(p, te))
			update_profile_te(p, 0);
#endif
		if (prof_en(p, hiber))
			update_profile_hiber(p, false, (s64)arg);
		break;
	case PROFILE_LOG:
		if (prof_en(p, log)) {
			if (unlikely(!profiler_is_log_initialized(p)))
				profiler_log_init(p);
			insert_plog(p, (struct profiler_log *)arg);
		}
		break;
	case PROFILER_SET_PID:
		p->systrace.pid = *(int *)arg;
		break;

	case PROFILER_COLOR_CIRCLE:
		break;

	}

	return (long)ret;
}

static const struct v4l2_subdev_core_ops profiler_v4l2_core_ops = {
	.ioctl = profiler_v4l2_ioctl,
};

static const struct v4l2_subdev_ops profiler_subdev_ops = {
	.core = &profiler_v4l2_core_ops,
};

static void profiler_init_v4l2_subdev(struct panel_device *panel)
{
	struct v4l2_subdev *sd;
	struct profiler_device *p = &panel->profiler;

	sd = &p->sd;

	v4l2_subdev_init(sd, &profiler_subdev_ops);
	sd->owner = THIS_MODULE;
	sd->grp_id = 0;

	snprintf(sd->name, sizeof(sd->name), "%s.%d", "panel-profiler", 0);

	v4l2_set_subdevdata(sd, p);
}


void profile_fps(struct profiler_device *p)
{
	s64 time_diff;
	unsigned int gap, c_frame_cnt;
	ktime_t c_time, p_time;
	struct profiler_fps *fps;

	fps = &p->fps;

	c_frame_cnt = fps->frame_cnt;
	c_time = ktime_get();

	if (c_frame_cnt >= fps->prev_frame_cnt)
		gap = c_frame_cnt - fps->prev_frame_cnt;
	else
		gap = (FPS_MAX - fps->prev_frame_cnt) + c_frame_cnt;

	p_time = fps->slot[fps->slot_cnt].timestamp;
	fps->slot[fps->slot_cnt].timestamp = c_time;

	fps->total_frame -= fps->slot[fps->slot_cnt].frame_cnt;
	fps->total_frame += gap;
	fps->slot[fps->slot_cnt].frame_cnt = gap;

	time_diff = ktime_to_us(ktime_sub(c_time, p_time));
	//panel_info("diff : %llu : slot_cnt %d\n", time_diff, fps->slot_cnt);

	/*To Do.. after lcd off->on, must clear fps->slot data to zero and comp_fps, instan_fps set to 60Hz (default)*/
	if (ktime_to_us(p_time) != 0) {
		time_diff = ktime_to_us(ktime_sub(c_time, p_time));

		fps->average_fps = fps->total_frame;
		fps->comp_fps = (unsigned int)(((1000000000 / time_diff) * fps->total_frame) + 500) / 1000;

		prof_info(p, fps_debug, "avg fps : %d\n", fps->comp_fps);

		time_diff = ktime_to_us(ktime_sub(c_time, p->fps.win_config_time));
		if (time_diff >= 100000) {
			fps->instant_fps = fps->average_fps;
#if PROFILER_DECON_SYSTRACE_ENABLE
			if (p->conf->systrace)
				decon_systrace(get_decon_drvdata(0), 'C', "fps_inst", (int)fps->instant_fps);
#endif
		}
#if PROFILER_DECON_SYSTRACE_ENABLE
		if (p->conf->systrace)
			decon_systrace(get_decon_drvdata(0), 'C', "fps_aver", (int)fps->comp_fps);
#endif
	}

	fps->prev_frame_cnt = c_frame_cnt;
	fps->slot_cnt = (fps->slot_cnt + 1) % MAX_SLOT_CNT;
}

static int profiler_mprint_update(struct profiler_device *p)
{
	int ret = 0;
	struct panel_device *panel = container_of(p, struct panel_device, profiler);
	struct timespec64 cur_ts, last_ts, delta_ts;
	s64 elapsed_usec = 0;

	struct pktinfo PKTINFO(self_mprint_data) = {
		.name = "self_mprint_data",
		.type = DSI_PKT_TYPE_WR_SR,
		.data = p->mask_props.data,
		.offset = 0,
		.dlen = p->mask_props.data_size,
		.pktui = NULL,
		.nr_pktui = 0,
		.option = PKT_OPTION_SR_ALIGN_16,
	};

	void *self_mprint_data_cmdtbl[] = {
		&PKTINFO(self_mprint_data),
	};

	struct seqinfo self_mprint_data_seq = SEQINFO_INIT("self_mprint_data_seq", self_mprint_data_cmdtbl);

	if (!IS_PANEL_ACTIVE(panel))
		return -EIO;

	panel_wake_lock(panel, WAKE_TIMEOUT_MSEC);
	ret = profiler_do_seqtbl_by_index(p, DISABLE_PROFILE_FPS_MASK_SEQ);

	ret = profiler_do_seqtbl_by_index_nolock(p, WAIT_PROFILE_FPS_MASK_SEQ);

	mutex_lock(&panel->op_lock);
	ret = profiler_do_seqtbl_by_index_nolock(p, MEM_SELECT_PROFILE_FPS_MASK_SEQ);

	//send mask data
	ktime_get_ts64(&cur_ts);
	ret = panel_do_seqtbl(panel, &self_mprint_data_seq);
	if (unlikely(ret < 0)) {
		panel_err("failed to excute self_mprint_data_cmdtbl(%d)\n",
				p->mask_props.data_size);
		ret = -EIO;
		goto do_exit;
	}

	if (prof_en(p, timediff)) {
		ktime_get_ts64(&last_ts);
		delta_ts = timespec64_sub(last_ts, cur_ts);
		elapsed_usec = timespec64_to_ns(&delta_ts) / 1000;
	}
	prof_info(p, timediff_en, "write mask image, elapsed = %lldus\n", elapsed_usec);
	prof_info(p, mprint_debug, "write mask image, size = %d\n", p->mask_props.data_size);

	ret = profiler_do_seqtbl_by_index_nolock(p, ENABLE_PROFILE_FPS_MASK_SEQ);
do_exit:
	mutex_unlock(&panel->op_lock);
	panel_wake_unlock(panel);

	if (prof_dbg(p, mprint))
		prof_info(p, mprint_debug, "enable fps mask ret %d\n", ret);

	return ret;
}

#define PROFILER_TEXT_OUTPUT_LEN 32
static int profiler_thread(void *data)
{
	int ret;
	struct profiler_device *p = data;
	//struct profiler_fps *fps = &p->fps;
	struct mprint_props *mask_props;
	struct profiler_te *te;
	struct profiler_hiber *hiber;
	char text_old[PROFILER_TEXT_OUTPUT_LEN] = {0, };
	char text[PROFILER_TEXT_OUTPUT_LEN] = {0, };
	int len = 0;
	int cycle_time = 0;
	struct timespec64 cur_ts = { 0, };
	struct timespec64 last_ts = { 0, };
	struct timespec64 delta_ts = { 0, };
	s64 elapsed_usec = 0;

	if (p->conf == NULL) {
		panel_err("profiler config is null\n");
		return -EINVAL;
	}

	mask_props = &p->mask_props;
	te = &p->te_info;
	hiber = &p->hiber_info;

	while (!kthread_should_stop()) {
		cycle_time = p->conf->cycle_time > 5 ? p->conf->cycle_time : CYCLE_TIME_DEFAULT;
		schedule_timeout_interruptible(cycle_time);

		if (!prof_en(p, profiler)) {
			schedule_timeout_interruptible(msecs_to_jiffies(cycle_time * 10));
			continue;
		}

		len = 0;
		if (prof_disp(p, hiber)) {
			len += snprintf(text + len, ARRAY_SIZE(text) - len, "%c ",
				hiber->hiber_status ? 'H' : ' ');
		}

		if (prof_disp(p, fps)) {
			profile_fps(p);
			len += snprintf(text + len, ARRAY_SIZE(text) - len, "%3d ", p->fps.comp_fps);
		}

		if (prof_disp(p, te)) {
			if (te->last_diff > 0) {
				len += snprintf(text + len, ARRAY_SIZE(text) - len, "%3lld.%02lld ",
					te->last_diff / 1000, (te->last_diff % 1000) / 10);
			}
		}

		if (prof_en(p, mprint) && len > 0 && strncmp(text_old, text, len) != 0) {
			if (prof_en(p, timediff))
				ktime_get_ts64(&cur_ts);

			ret = char_to_mask_img(mask_props, text);

			if (prof_en(p, timediff))
				ktime_get_ts64(&last_ts);

			if (ret < 0) {
				panel_err("err on mask img gen '%s'\n", text);
				continue;
			}
			if (prof_en(p, timediff)) {
				delta_ts = timespec64_sub(last_ts, cur_ts);
				elapsed_usec = timespec64_to_ns(&delta_ts) / 1000;
			}
			prof_info(p, mprint_debug, "generated img by '%s', size %d, cyc %d\n",
				text, mask_props->data_size, cycle_time);

			prof_info(p, timediff_en, "generated elapsed = %lldus, '%s'\n", elapsed_usec, text);

			profiler_mprint_update(p);
			memcpy(text_old, text, ARRAY_SIZE(text));
		}

		if (profiler_is_log_initialized(p) && p->conf->log_disp == LOG_DISP_ASYNC)
			print_plog(p);

	}
	return 0;
}

int profiler_find_string(char *f, char *l[], int lsize)
{
	int i, len;

	panel_info("++ %s %d\n", f, lsize);

	if (f == NULL || l == NULL)
		return -1;

	len = strlen(f);

	panel_info("%s len %d\n", f, len);

	for (i = 0; i < lsize; i++) {
		panel_info("[%d] %s %p\n", i, l[i], l[i]);
		if (l[i] == NULL)
			continue;
		panel_info("[%d] strlen %d\n", i, strlen(l[i]));
		if (strlen(l[i]) != len)
			continue;
		if (strncmp(f, l[i], len) == 0)
			return i;
	}
	return -1;
}

static ssize_t dprof_mprint_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct profiler_device *p;
	struct panel_device *panel = dev_get_drvdata(dev);
	int *data;
	int len = 0;
	size_t i;
	char *cfg_name;

	p = &panel->profiler;
	if (p == NULL) {
		panel_err("profiler is null\n");
		return -EINVAL;
	}

	if (p->mask_config == NULL) {
		panel_err("mask config is null\n");
		return -EINVAL;
	}

	data = (int *) p->mask_config;
	for (i = 0; i < sizeof(struct mprint_config) / sizeof(int); i++) {
		cfg_name = get_mprint_config_name(i);
		len += snprintf(buf + len, PAGE_SIZE - len, "%12s",
				cfg_name ? cfg_name : "(null)");
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	for (i = 0; i < sizeof(struct mprint_config) / sizeof(int); i++)
		len += snprintf(buf + len, PAGE_SIZE - len, "%12d", *(data + i));

	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	return strlen(buf);
}

static ssize_t dprof_mprint_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct profiler_device *p;
	struct panel_device *panel = dev_get_drvdata(dev);
	int *data;
	int val, len = 0, read, ret;
	size_t i;

	p = &panel->profiler;
	if (p == NULL) {
		panel_err("profiler is null\n");
		return -EINVAL;
	}

	if (p->mask_config == NULL) {
		panel_err("mask config is null\n");
		return -EINVAL;
	}

	data = (int *) p->mask_config;
	for (i = 0; i < sizeof(struct mprint_config) / sizeof(int); i++) {
		ret = sscanf(buf + len, "%d%n", &val, &read);
		if (ret < 1)
			break;
		*(data+i) = val;
		len += read;
		panel_info("[D_PROF] config[%lu] set to %d\n", i, val);
	}

	return size;
}

static ssize_t dprof_partial_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct profiler_device *p;
	struct panel_device *panel = dev_get_drvdata(dev);

	p = &panel->profiler;
	if (p == NULL) {
		panel_err("profiler is null\n");
		return -EINVAL;
	}

	return strlen(buf);
}


static ssize_t dprof_partial_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value, rc;
	struct profiler_device *p;
	struct panel_device *panel = dev_get_drvdata(dev);

	p = &panel->profiler;
	if (p == NULL) {
		panel_err("profiler is null\n");
		return -EINVAL;
	}

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	return size;
}

static ssize_t dprof_config_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct profiler_device *p;
	struct panel_device *panel = dev_get_drvdata(dev);
	int *data;
	int len = 0;
	size_t i, count;

	p = &panel->profiler;
	if (p == NULL) {
		panel_err("profiler is null\n");
		return -EINVAL;
	}

	if (p->conf == NULL) {
		panel_err("profiler config is null\n");
		return -EINVAL;
	}

	len += snprintf(buf + len, PAGE_SIZE - len,
		"DISPLAY_PROFILER_VER=%d\n", PROFILER_VERSION);

	count = sizeof(struct profiler_config) / sizeof(int);

	if (count != ARRAY_SIZE(profiler_config_names)) {
		len += snprintf(buf + len, PAGE_SIZE - len,
			"CONFIG SIZE MISMATCHED!! configurations are may be wrong(%lu, %lu)\n",
			count, ARRAY_SIZE(profiler_config_names));
	}

	if (count > ARRAY_SIZE(profiler_config_names))
		count = ARRAY_SIZE(profiler_config_names);

	data = (int *) p->conf;

	for (i = 0; i < count; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len, "%s=%d%s",
			(i < ARRAY_SIZE(profiler_config_names)) ? profiler_config_names[i] : "(null)",
			*(data+i),
			(i < count - 1) ? "," : "\n");
	}

	return strlen(buf);
}

static ssize_t dprof_config_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct profiler_device *p;
	struct panel_device *panel = dev_get_drvdata(dev);
	char str[STORE_BUFFER_SIZE] = { 0, };
	char *strbuf, *tok, *field, *value;
	int *data;
	int val, ret;
	size_t i;

	p = &panel->profiler;
	if (p == NULL) {
		panel_err("profiler is null\n");
		return -EINVAL;
	}

	if (p->conf == NULL) {
		panel_err("profiler config is null\n");
		return -EINVAL;
	}

	data = (int *) p->conf;

	memcpy(str, buf, min(size, (size_t)(STORE_BUFFER_SIZE - 1)));
	strbuf = str;

	while ((tok = strsep(&strbuf, ",")) != NULL) {
		field = strsep(&tok, "=");
		if (field == NULL) {
			panel_err("invalid field\n");
			return -EINVAL;
		}
		field = strim(field);
		if (strlen(field) < 1) {
			panel_err("invalid field\n");
			return -EINVAL;
		}
		value = strsep(&tok, "=");
		if (value == NULL) {
			panel_err("invalid value with field %s\n", field);
			return -EINVAL;
		}
		prof_info(p, profiler_debug, "[D_PROF]  field %s with value %s\n", field, value);
		ret = kstrtouint(strim(value), 0, &val);
		if (ret < 0) {
			panel_err("invalid value %s, ret %d\n", value, ret);
			return ret;
		}
		for (i = 0; i < ARRAY_SIZE(profiler_config_names); i++) {
			if (!strncmp(field, profiler_config_names[i], strlen(profiler_config_names[i])))
				break;
		}
		if (i < ARRAY_SIZE(profiler_config_names)) {
			*(data + i) = val;
			panel_info("[D_PROF]  config set %s->%d\n",
					profiler_config_names[i], val);
		}
	}

	return size;
}

static ssize_t dprof_cmd_filter_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct profiler_device *p;
	struct panel_device *panel = dev_get_drvdata(dev);
	int *data;
	int len = 0, start;
	u32 i, count;

	p = &panel->profiler;
	if (p == NULL) {
		panel_err("profiler is null\n");
		return -EINVAL;
	}

	if (p->conf == NULL) {
		panel_err("profiler config is null\n");
		return -EINVAL;
	}

	count = ARRAY_SIZE(plog_cmd_filter);

	data = (int *) p->conf;
	start = -1;
	len += snprintf(buf + len, PAGE_SIZE - len, "cmdlog filter: %s, cmds:\n",
		p->conf->log_filter_en ? "enabled" : "disabled");
	for (i = 0; i <= count; i++) {
		if (i < count && plog_cmd_filter[i]) {
			if (start == -1)
				start = i;
		} else {
			if (start == -1)
				continue;
			if (start == (i - 1))
				len += snprintf(buf + len, PAGE_SIZE - len, "0x%02x\n", start);
			else
				len += snprintf(buf + len, PAGE_SIZE - len, "0x%02x:0x%02x\n", start, (i - 1));
			start = -1;
		}
	}
	if (i == 0)
		len += snprintf(buf + len, PAGE_SIZE - len, "none\n");
	return strlen(buf);
}
static ssize_t dprof_cmd_filter_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct profiler_device *p;
	struct panel_device *panel = dev_get_drvdata(dev);
	char str[STORE_BUFFER_SIZE] = { 0, };
	char *strbuf, *tok;
	int *data;
	int ret;
	int val;
	size_t i;

	p = &panel->profiler;
	if (p == NULL) {
		panel_err("profiler is null\n");
		return -EINVAL;
	}

	if (p->conf == NULL) {
		panel_err("profiler config is null\n");
		return -EINVAL;
	}

	data = (int *) p->conf;
	memcpy(str, buf, min(size, (size_t)(STORE_BUFFER_SIZE - 1)));
	strbuf = str;

	//all on
	if (!strncmp(str, "allon", 5)) {
		for (i = 0; i < ARRAY_SIZE(plog_cmd_filter); i++)
			plog_cmd_filter[i] = true;
		return size;
	}
	if (!strncmp(str, "alloff", 6)) {
		for (i = 0; i < ARRAY_SIZE(plog_cmd_filter); i++)
			plog_cmd_filter[i] = false;
		return size;
	}

	while ((tok = strsep(&strbuf, ",")) != NULL) {
		ret = kstrtoint(strim(tok), 0, &val);
		if (ret < 0) {
			panel_err("invalid value %s, ret %d\n", tok, ret);
			return ret;
		}
		if (val < 0 || val >= PLOG_CMD_FILTER_SIZE) {
			panel_err("invalid value %s, ret %d\n", tok, ret);
			continue;
		}
		plog_cmd_filter[val] = true;
	}

	return size;
}


static ssize_t dprof_log_filter_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct profiler_device *p;
	struct panel_device *panel = dev_get_drvdata(dev);
	int i, k, len = 0, count;

	p = &panel->profiler;
	if (p == NULL) {
		panel_err("profiler is null\n");
		return -EINVAL;
	}

	if (p->conf == NULL) {
		panel_err("profiler config is null\n");
		return -EINVAL;
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "log filter: %s, types:\n",
		p->conf->log_filter_en ? "enabled" : "disabled");

	count = ARRAY_SIZE(PLOG_TYPE_MAP);
	for (i = 1; i < count; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len, "%s: %04X(%d)\n",
			PLOG_TYPE_MAP[i].name, PLOG_TYPE_MAP[i].flag, PLOG_TYPE_MAP[i].flag);
		for (k = 0; k < PLOG_TYPE_MAP[i].nr_sub; k++) {
			len += snprintf(buf + len, PAGE_SIZE - len, "\t%s: \t%s\n", PLOG_TYPE_MAP[i].sub[k],
				(PLOG_TYPE_MAP[i].flag & (1 << k)) ? "ON" : "OFF");
		}
	}

	return strlen(buf);
}

static ssize_t dprof_log_filter_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct profiler_device *p;
	struct panel_device *panel = dev_get_drvdata(dev);
	char str[STORE_BUFFER_SIZE] = { 0, };
	char *strbuf, *tok, *field, *value;
	int val, index, ret;

	p = &panel->profiler;
	if (p == NULL) {
		panel_err("profiler is null\n");
		return -EINVAL;
	}

	if (p->conf == NULL) {
		panel_err("profiler config is null\n");
		return -EINVAL;
	}

	memcpy(str, buf, min(size, (size_t)(STORE_BUFFER_SIZE - 1)));
	strbuf = str;

	while ((tok = strsep(&strbuf, ",")) != NULL) {
		field = strsep(&tok, "=");
		if (field == NULL) {
			panel_warn("invalid field\n");
			return -EINVAL;
		}
		field = strim(field);
		if (strlen(field) < 1) {
			panel_warn("invalid field.\n");
			return -EINVAL;
		}
		index = plog_get_type_idx(field);
		panel_info("field %s index %d\n", field, index);
		if (index < 0) {
			prof_info(p, profiler_debug, "[D_PROF] field %s not found\n", field);
			continue;
		}

		value = strsep(&tok, "=");
		if (value == NULL) {
			panel_err("invalid value with field %s\n", field);
			return -EINVAL;
		}
		ret = kstrtoint(strim(value), 0, &val);
		if (ret < 0) {
			panel_err("invalid value %s, ret %d\n", value, ret);
			return ret;
		}
		prof_info(p, profiler_debug, "[D_PROF] field %s with value %d -> %d\n",
			field, PLOG_TYPE_MAP[index].flag, val);
		PLOG_TYPE_MAP[index].flag = val;
	}

	return size;
}

struct device_attribute profiler_attrs[] = {
	__PANEL_ATTR_RW(dprof_partial, 0660),
	__PANEL_ATTR_RW(dprof_config, 0660),
	__PANEL_ATTR_RW(dprof_mprint, 0660),
	__PANEL_ATTR_RW(dprof_log_filter, 0660),
	__PANEL_ATTR_RW(dprof_cmd_filter, 0660)
};

static int profiler_log_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct profiler_device *p = container_of(dev, struct profiler_device, log_dev);

	if (p->log_dev_opened) {
		panel_warn("already opened\n");
		return -EBUSY;
	}

	file->private_data = p;
	p->log_dev_opened = true;

	return 0;
}

static int profiler_log_release(struct inode *inode, struct file *file)
{
	struct profiler_device *p = file->private_data;

	file->private_data = p;
	p->log_dev_opened = false;

	return 0;
}

static ssize_t profiler_log_read(struct file *file, char __user *buf, size_t count,
		loff_t *ppos)
{
	struct profiler_device *p = file->private_data;
	struct profiler_log *c;
	char log_string[1024] = { 0, };
	int log_size = 0;
	int h, t, ret;
	ssize_t res = 0;

	if (!profiler_is_log_initialized(p))
		goto end;

	h = p->log_idx_head;
	t = p->log_idx_tail_file;

	if (t == h)
		goto end;

	prof_info(p, log_debug, "t %d h %d p_log_str %p\n",
		t, h, log_string);

	do {
		t = (t + 1) % PLOG_SIZE;
		c = &p->log_list[t];
		log_size = log_to_str(c, log_string, sizeof(log_string) - 1);
		if (log_size <= 0 || log_size >= sizeof(log_string))
			break;

		if (res + log_size + 1 > count) {
			prof_info(p, log_debug,
					"limit exceeded %d %d %d\n",
					res, log_size, count);
			break;
		}

		log_string[log_size++] = '\n';
		ret = copy_to_user(buf + res, log_string, log_size);
		if (ret != 0) {
			panel_warn("copy failed %d %d %d\n", count, res, log_size);
			break;
		}
		res += log_size;
		prof_info(p, log_debug, "res %d ppos %d\n", res, *ppos);
	} while (t != h);
	p->log_idx_tail_file = t;
	*ppos += res;

end:
	return res;
}

static const struct file_operations profiler_log_fops = {
	.owner = THIS_MODULE,
	.read = profiler_log_read,
	.open = profiler_log_open,
	.release = profiler_log_release,
};

int profiler_probe(struct panel_device *panel, struct profiler_tune *tune)
{
	int ret = 0;
	size_t i;
	struct device *lcd_dev;
	struct profiler_device *p;

	if (!panel) {
		panel_err("panel is not exist\n");
		return -EINVAL;
	}

	if (!tune) {
		panel_err("tune is null\n");
		return -EINVAL;
	}

	lcd_dev = panel->lcd_dev;
	if (unlikely(!lcd_dev)) {
		panel_err("lcd device not exist\n");
		return -ENODEV;
	}

	if (tune->conf == NULL) {
		panel_err("profiler config is null\n");
		return -EINVAL;
	}

	p = &panel->profiler;
	profiler_init_v4l2_subdev(panel);

	p->seqtbl = tune->seqtbl;
	p->nr_seqtbl = tune->nr_seqtbl;
	p->maptbl = tune->maptbl;
	p->nr_maptbl = tune->nr_maptbl;

	for (i = 0; i < p->nr_maptbl; i++) {
		p->maptbl[i].pdata = p;
		maptbl_init(&p->maptbl[i]);
	}

	for (i = 0; i < ARRAY_SIZE(profiler_attrs); i++) {
		ret = device_create_file(lcd_dev, &profiler_attrs[i]);
		if (ret < 0) {
			dev_err(lcd_dev, "failed to add %s sysfs entries, %d\n",
					profiler_attrs[i].attr.name, ret);
			return -ENODEV;
		}
	}

	spin_lock_init(&p->te_info.slock);

	p->log_dev.minor = MISC_DYNAMIC_MINOR;
	p->log_dev.name = "dprof_log";
	p->log_dev.fops = &profiler_log_fops;
	p->log_dev.parent = NULL;
	ret = misc_register(&p->log_dev);
	if (ret) {
		panel_err("failed to register misc driver (ret %d)\n", ret);
		goto err;
	}

	p->log_initialized = false;
	p->log_idx_head = -1;
	p->log_data_idx = -1;
	p->conf = tune->conf;

// mask config
	p->mask_config = tune->mprint_config;

// mask props
	p->mask_props.conf = p->mask_config;
	p->mask_props.pkts_pos = 0;
	p->mask_props.pkts_size = 0;

	if (p->mask_config->max_len < 2)
		p->mask_config->max_len = MASK_DATA_SIZE_DEFAULT;

	p->mask_props.data = kvmalloc(p->mask_config->max_len, GFP_KERNEL);
	p->mask_props.data_max = p->mask_config->max_len;

	p->mask_props.pkts_max = p->mask_props.data_max / 2;
	p->mask_props.pkts = kvmalloc_array(sizeof(struct mprint_packet),
		p->mask_props.pkts_max, GFP_KERNEL);
	if (!p->mask_props.pkts) {
		panel_err("failed to allocate mask packet buffer\n");
		goto err;
	}

	p->thread = kthread_run(profiler_thread, p, "profiler");
	if (IS_ERR_OR_NULL(p->thread)) {
		panel_err("failed to run thread\n");
		ret = PTR_ERR(p->thread);
		goto err;
	}
	p->initialized = true;
err:
	return ret;
}

int profiler_remove(struct panel_device *panel)
{
	return 0;
}
