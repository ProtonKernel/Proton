#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/cpufreq.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/poll.h>

#include <soc/samsung/exynos-migov.h>
#include <soc/samsung/exynos-migov-shared.h>
#include <soc/samsung/exynos-dm.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/bts.h>
#include <soc/samsung/freq-qos-tracer.h>
#include <soc/samsung/exynos-sci.h>
#include "exynos-gpu-api.h"

/******************************************************************************/
/*                            Structure for migov                             */
/******************************************************************************/
#define MAXNUM_OF_DVFS		30
#define NUM_OF_CPU_DOMAIN	(MIGOV_CL2 + 1)
#define MAXNUM_OF_CPU		8

enum profile_mode {
	PROFILE_FINISH,
	PROFILE_START_BY_GAME,
	PROFILE_START_BY_GPU,
	PROFILE_UPDATE,
	PROFILE_INVALID,
};

/* Structure for IP's Private Data */
struct private_data_cpu {
	struct private_fn_cpu *fn;

	s32	pm_qos_cpu;
	struct freq_qos_request pm_qos_min_req;
	struct freq_qos_request pm_qos_max_req;
	s32	pm_qos_min_freq;
	s32	pm_qos_max_freq;
	s32	hp_minlock_low_limit;
	s32	lp_minlock_low_limit;

	s32	num_of_cpu;
};

struct private_data_gpu {
	struct private_fn_gpu *fn;

	struct exynos_pm_qos_request pm_qos_max_req;
	struct exynos_pm_qos_request pm_qos_min_req;
	s32	pm_qos_max_class;
	s32	pm_qos_max_freq;
	s32	pm_qos_min_class;
	s32	pm_qos_min_freq;

	/* Tunables */
	u64	q0_empty_pct_thr;
	u64	q1_empty_pct_thr;
	u64	gpu_active_pct_thr;
};

struct private_data_mif {
	struct private_fn_mif *fn;

	struct exynos_pm_qos_request pm_qos_min_req;
	struct exynos_pm_qos_request pm_qos_max_req;
	s32	pm_qos_min_class;
	s32	pm_qos_max_class;
	s32	pm_qos_min_freq;
	s32	pm_qos_max_freq;
	s32	hp_minlock_low_limit;

	/* Tunables */
	s32	stats0_mode_min_freq;
	u64	stats0_sum_thr;
	u64	stats0_updown_delta_pct_thr;
};

struct domain_data {
	bool			enabled;
	s32			id;

	struct domain_fn	*fn;

	void			*private;

	struct attribute_group	attr_group;
};

struct device_file_operation {
	struct file_operations          fops;
	struct miscdevice               miscdev;
};

static struct migov {
	bool				running;
	bool				disable;
	bool				profile_only;
	bool				game_mode;
	bool				heavy_gpu_mode;
	u32				cur_mode;

	int				bts_idx;
	int				len_mo_id;
	int				*mo_id;

	bool				llc_enable;

	struct domain_data		domains[NUM_OF_DOMAIN];

	ktime_t				start_time;
	ktime_t				end_time;

	u64				start_frame_cnt;
	u64				end_frame_cnt;
	u64				start_frame_vsync_cnt;
	u64				end_frame_vsync_cnt;
	u64				start_fence_cnt;
	u64				end_fence_cnt;
	void				(*get_frame_cnt)(u64 *cnt, ktime_t *time);
	void				(*get_vsync_cnt)(u64 *cnt, ktime_t *time);
	void				(*get_fence_cnt)(u64 *cnt, ktime_t *time);

	s32				max_fps;

	struct work_struct		fps_work;

	bool				was_gpu_heavy;
	ktime_t				heavy_gpu_start_time;
	ktime_t				heavy_gpu_end_time;
	s32				heavy_gpu_ms_thr;
	s32				gpu_freq_thr;
	s32				fragutil;
	s32				fragutil_upper_thr;
	s32				fragutil_lower_thr;
	struct work_struct		fragutil_work;

	struct device_node		*dn;
	struct kobject			*kobj;

	u32				running_event;
	struct device_file_operation 	gov_fops;
	wait_queue_head_t		wq;
} migov;

struct fps_profile {
	/* store last profile average fps */
	int		start;
	ktime_t		profile_time;	/* sec */
	u64		fps;
} fps_profile;

/*  shared data with Platform */
struct profile_sharing_data psd;
struct delta_sharing_data dsd;
struct tunable_sharing_data tsd;

static struct mutex migov_lock;

/******************************************************************************/
/*                               Helper functions                             */
/******************************************************************************/
static inline bool is_enabled(s32 id)
{
	if (id >= NUM_OF_DOMAIN || id < 0)
		return false;

	return migov.domains[id].enabled;
}

static inline struct domain_data *next_domain(s32 *id)
{
	s32 idx = *id;

	for (++idx; idx < NUM_OF_DOMAIN; idx++)
		if (migov.domains[idx].enabled)
			break;
	*id = idx;

	return idx == NUM_OF_DOMAIN ? NULL : &migov.domains[idx];
}

#define for_each_domain(dom, id)	\
	for (id = -1; (dom) = next_domain(&id), id < NUM_OF_DOMAIN;)

/******************************************************************************/
/*                                      fops                                  */
/******************************************************************************/
#define IOCTL_MAGIC	'M'
#define IOCTL_READ_TSD	_IOR(IOCTL_MAGIC, 0x50, struct tunable_sharing_data)
#define IOCTL_READ_PSD	_IOR(IOCTL_MAGIC, 0x51, struct profile_sharing_data)
#define IOCTL_WR_DSD	_IOWR(IOCTL_MAGIC, 0x52, struct delta_sharing_data)

ssize_t migov_fops_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	if (!access_ok(buf, sizeof(s32)))
		return -EFAULT;

	mutex_lock(&migov_lock);
	if (copy_to_user(buf, &migov.running_event, sizeof(s32)))
		pr_info("MIGOV : Reading doesn't work!");

	migov.running_event = PROFILE_INVALID;
	mutex_unlock(&migov_lock);

	return count;
}

__poll_t migov_fops_poll(struct file *filp, struct poll_table_struct *wait)
{
	__poll_t mask = 0;

	poll_wait(filp, &migov.wq, wait);

	switch (migov.running_event) {
	case PROFILE_START_BY_GAME:
	case PROFILE_START_BY_GPU:
	case PROFILE_FINISH:
		mask = EPOLLIN | EPOLLRDNORM;
		break;
	default:
		break;
	}

	return mask;
}

long migov_fops_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case IOCTL_READ_TSD:
		{
			struct __user tunable_sharing_data *user_tsd = (struct __user tunable_sharing_data *)arg;
			if (!access_ok(user_tsd, sizeof(struct __user tunable_sharing_data)))
				return -EFAULT;
			if (copy_to_user(user_tsd, &tsd, sizeof(struct tunable_sharing_data)))
				pr_info("MIGOV : IOCTL_READ_TSD doesn't work!");
		}
		break;
	case IOCTL_READ_PSD:
		{
			struct __user profile_sharing_data *user_psd = (struct __user profile_sharing_data *)arg;
			if (!access_ok(user_psd, sizeof(struct __user profile_sharing_data)))
				return -EFAULT;
			if (copy_to_user(user_psd, &psd, sizeof(struct profile_sharing_data)))
				pr_info("MIGOV : IOCTL_READ_PSD doesn't work!");
		}
		break;
	case IOCTL_WR_DSD:
		{
			struct __user delta_sharing_data *user_dsd = (struct __user delta_sharing_data *)arg;
			if (!access_ok(user_dsd, sizeof(struct __user delta_sharing_data)))
				return -EFAULT;
			if (!copy_from_user(&dsd, user_dsd, sizeof(struct delta_sharing_data))) {
				struct domain_data *dom;
				if (!is_enabled(dsd.id))
					return -EINVAL;
				dom = &migov.domains[dsd.id];
				dom->fn->get_power_change(dsd.id, dsd.freq_delta_pct,
						&dsd.freq, &dsd.dyn_power, &dsd.st_power);
				if (copy_to_user(user_dsd, &dsd, sizeof(struct delta_sharing_data)))
					pr_info("MIGOV : IOCTL_RW_DSD doesn't work!");
			}
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

int migov_fops_release(struct inode *node, struct file *filp)
{
	return 0;
}

/******************************************************************************/
/*                                FPS functions                               */
/******************************************************************************/
void exynos_migov_register_frame_cnt(void (*fn)(u64 *cnt, ktime_t *time))
{
	migov.get_frame_cnt = fn;
}
EXPORT_SYMBOL_GPL(exynos_migov_register_frame_cnt);

void exynos_migov_register_vsync_cnt(void (*fn)(u64 *cnt, ktime_t *time))
{
	migov.get_vsync_cnt = fn;
}
EXPORT_SYMBOL_GPL(exynos_migov_register_vsync_cnt);

void exynos_migov_register_fence_cnt(void (*fn)(u64 *cnt, ktime_t *time))
{
	migov.get_fence_cnt = fn;
}
EXPORT_SYMBOL_GPL(exynos_migov_register_fence_cnt);

void migov_update_fps_change(u32 new_fps)
{
	if (migov.max_fps != new_fps * 10) {
		migov.max_fps = new_fps * 10;
		schedule_work(&migov.fps_work);
	}
}
EXPORT_SYMBOL_GPL(migov_update_fps_change);

static void migov_fps_change_work(struct work_struct *work)
{
	tsd.max_fps = migov.max_fps;
	psd.max_fps = migov.max_fps;
}

/******************************************************************************/
/*                             profile functions                              */
/******************************************************************************/
static void migov_get_profile_data(struct domain_data *dom)
{
	s32 id = dom->id;

	/* dvfs */
	psd.max_freq[id] = dom->fn->get_max_freq(id);
	psd.min_freq[id] = dom->fn->get_min_freq(id);
	psd.freq[id] = dom->fn->get_freq(id);

	/* power */
	dom->fn->get_power(id, &psd.dyn_power[id], &psd.st_power[id]);

	/* temperature */
	psd.temp[id] = dom->fn->get_temp(id);

	/* active pct */
	psd.active_pct[id] = dom->fn->get_active_pct(id);

	/* private */
	if (id <= MIGOV_CL2) {
		struct private_data_cpu *private = dom->private;

		private->fn->cpu_active_pct(dom->id, psd.cpu_active_pct[dom->id]);
	} else if (id == MIGOV_GPU) {
		struct private_data_gpu *private = dom->private;

		psd.q0_empty_pct = private->fn->get_q_empty_pct(0);
		psd.q1_empty_pct = private->fn->get_q_empty_pct(1);
		psd.input_nr_avg_cnt = private->fn->get_input_nr_avg_cnt();
	} else if (id == MIGOV_MIF) {
		struct private_data_mif *private = dom->private;

		psd.mif_pm_qos_cur_freq = exynos_pm_qos_request(private->pm_qos_min_class);
		psd.stats0_sum = private->fn->get_stats0_sum();
		psd.stats0_avg = private->fn->get_stats0_avg();
		psd.stats1_sum = private->fn->get_stats1_sum();
		psd.stats_ratio = private->fn->get_stats_ratio();
		psd.llc_status = private->fn->get_llc_status();
	}
}

void migov_set_tunable_data(void)
{
	struct domain_data *dom;
	s32 id;

	for_each_domain(dom, id) {
		tsd.enabled[id] = dom->enabled;

		if (id <= MIGOV_CL2) {
			struct private_data_cpu *private = dom->private;

			tsd.first_cpu[id] = private->pm_qos_cpu;
			tsd.num_of_cpu[id] = private->num_of_cpu;
			tsd.minlock_low_limit[id] = private->pm_qos_min_freq;
			tsd.maxlock_high_limit[id] = private->pm_qos_max_freq;
			tsd.hp_minlock_low_limit[id] = private->hp_minlock_low_limit;
			tsd.lp_minlock_low_limit[id] = private->lp_minlock_low_limit;
		} else if (id == MIGOV_GPU) {
			struct private_data_gpu *private = dom->private;

			tsd.q0_empty_pct_thr = private->q0_empty_pct_thr;
			tsd.q1_empty_pct_thr = private->q1_empty_pct_thr;
			tsd.gpu_active_pct_thr = private->gpu_active_pct_thr;
			tsd.minlock_low_limit[id] = private->pm_qos_min_freq;
		} else if (id == MIGOV_MIF) {
			struct private_data_mif *private = dom->private;

			tsd.stats0_mode_min_freq = private->stats0_mode_min_freq;
			tsd.stats0_sum_thr = private->stats0_sum_thr;
			tsd.stats0_updown_delta_pct_thr = private->stats0_updown_delta_pct_thr;
			tsd.mif_hp_minlock_low_limit = private->hp_minlock_low_limit;
			tsd.minlock_low_limit[id] = private->pm_qos_min_freq;
		}
	}
}

/******************************************************************************/
/*                              Profile functions                             */
/******************************************************************************/
void migov_update_profile(void)
{
	struct domain_data *dom;
	ktime_t time = 0;
	s32 id;

	if (!migov.running)
		return;

	migov.end_time = ktime_get();
	psd.profile_time_ms = ktime_to_ms(ktime_sub(migov.end_time, migov.start_time));

	if (migov.get_frame_cnt)
		migov.get_frame_cnt(&migov.end_frame_cnt, &time);
	psd.profile_frame_cnt = migov.end_frame_cnt - migov.start_frame_cnt;
	psd.frame_done_time_us = ktime_to_us(time);

	if (migov.get_vsync_cnt)
		migov.get_vsync_cnt(&migov.end_frame_vsync_cnt, &time);
	psd.profile_frame_vsync_cnt = migov.end_frame_vsync_cnt - migov.start_frame_vsync_cnt;
	psd.frame_vsync_time_us = ktime_to_us(time);

	if (migov.get_fence_cnt)
		migov.get_fence_cnt(&migov.end_fence_cnt, &time);
	psd.profile_fence_cnt = migov.end_fence_cnt - migov.start_fence_cnt;
	psd.profile_fence_time_us = ktime_to_us(time);

	for_each_domain(dom, id) {
		dom->fn->update_mode(id, 1);
		migov_get_profile_data(dom);
	}

	migov.start_time = migov.end_time;
	migov.start_frame_cnt = migov.end_frame_cnt;
	migov.start_frame_vsync_cnt = migov.end_frame_vsync_cnt;
	migov.start_fence_cnt = migov.end_fence_cnt;
}

void migov_start_profile(u32 mode)
{
	struct domain_data *dom;
	ktime_t time = 0;
	s32 id;

	if (migov.running)
		return;

	migov.running = true;
	migov.cur_mode = mode;
	migov.start_time = ktime_get();
	if (migov.get_frame_cnt)
		migov.get_frame_cnt(&migov.start_frame_cnt, &time);
	if (migov.get_vsync_cnt)
		migov.get_vsync_cnt(&migov.start_frame_vsync_cnt, &time);

	if (!(migov.profile_only || mode == PROFILE_START_BY_GPU)) {
		if (is_enabled(MIGOV_CL0) || is_enabled(MIGOV_CL1) || is_enabled(MIGOV_CL2))
			exynos_dm_dynamic_disable(1);
		if (is_enabled(MIGOV_GPU))
			exynos_migov_set_mode(1);

		if (tsd.dyn_mo_control && migov.bts_idx)
			bts_add_scenario(migov.bts_idx);
	}

	for_each_domain(dom, id) {
		dom->fn->update_mode(dom->id, 1);

		if (migov.profile_only || mode == PROFILE_START_BY_GPU)
			continue;

		if (id <= MIGOV_CL2) {
			struct private_data_cpu *private = dom->private;

			if (freq_qos_request_active(&private->pm_qos_min_req)) {
				freq_qos_update_request(&private->pm_qos_min_req,
						private->pm_qos_min_freq);
			}
			if (freq_qos_request_active(&private->pm_qos_max_req)) {
				freq_qos_update_request(&private->pm_qos_max_req, 0);
				freq_qos_update_request(&private->pm_qos_max_req,
						private->pm_qos_max_freq);
			}
		} else if (id == MIGOV_MIF) {
			struct private_data_mif *private = dom->private;
			if (exynos_pm_qos_request_active(&private->pm_qos_max_req))
				exynos_pm_qos_update_request(&private->pm_qos_max_req,
							private->pm_qos_max_freq);
			if (exynos_pm_qos_request_active(&private->pm_qos_min_req))
				exynos_pm_qos_update_request(&private->pm_qos_min_req,
							private->pm_qos_min_freq);
		} else {
			struct private_data_gpu *private = dom->private;
			if (exynos_pm_qos_request_active(&private->pm_qos_max_req))
				exynos_pm_qos_update_request(&private->pm_qos_max_req,
							private->pm_qos_max_freq);
			if (exynos_pm_qos_request_active(&private->pm_qos_min_req))
				exynos_pm_qos_update_request(&private->pm_qos_min_req,
							private->pm_qos_min_freq);
		}
	}

	migov_set_tunable_data();
}

void migov_finish_profile(void)
{
	struct domain_data *dom;
	s32 id;

	if (!migov.running)
		return;

	if (!migov.profile_only) {
		if (is_enabled(MIGOV_CL0) || is_enabled(MIGOV_CL1) || is_enabled(MIGOV_CL2))
			exynos_dm_dynamic_disable(0);
		if (is_enabled(MIGOV_GPU))
			exynos_migov_set_mode(0);

		if (tsd.dyn_mo_control && migov.bts_idx)
			bts_del_scenario(migov.bts_idx);
	}

	for_each_domain(dom, id) {
		dom->fn->update_mode(id, 0);

		if (migov.profile_only)
			continue;

		if (id <= MIGOV_CL2) {
			struct private_data_cpu *private = dom->private;

			if (freq_qos_request_active(&private->pm_qos_min_req))
				freq_qos_update_request(&private->pm_qos_min_req,
						PM_QOS_DEFAULT_VALUE);
			if (freq_qos_request_active(&private->pm_qos_max_req))
				freq_qos_update_request(&private->pm_qos_max_req,
						PM_QOS_DEFAULT_VALUE);
		} else if (id == MIGOV_MIF) {
			struct private_data_mif *private = dom->private;

			if (exynos_pm_qos_request_active(&private->pm_qos_min_req))
				exynos_pm_qos_update_request(&private->pm_qos_min_req,
						EXYNOS_PM_QOS_DEFAULT_VALUE);

			if (exynos_pm_qos_request_active(&private->pm_qos_max_req))
				exynos_pm_qos_update_request(&private->pm_qos_max_req,
							PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
		} else {
			struct private_data_gpu *private = dom->private;
			/* gpu use negative value for unlock */
			if (exynos_pm_qos_request_active(&private->pm_qos_max_req))
				exynos_pm_qos_update_request(&private->pm_qos_max_req, 0);
			if (exynos_pm_qos_request_active(&private->pm_qos_min_req))
				exynos_pm_qos_update_request(&private->pm_qos_min_req, 0);
		}
	}

	if (migov.llc_enable) {
		migov.llc_enable = false;
		llc_enable(migov.llc_enable);
	}

	migov.running = false;
	migov.cur_mode = PROFILE_FINISH;
}

static void wakeup_polling_task(void)
{
	if (migov.disable && !migov.running)
		return;

	mutex_lock(&migov_lock);
	if (migov.game_mode)
		migov.running_event = PROFILE_START_BY_GAME;
	else if (migov.heavy_gpu_mode)
		migov.running_event = PROFILE_START_BY_GPU;
	else
		migov.running_event = PROFILE_FINISH;
	mutex_unlock(&migov_lock);

	wake_up_interruptible(&migov.wq);
}

#if IS_ENABLED(CONFIG_MALI_NOTIFY_UTILISATION)
static void migov_fragutil_change_work(struct work_struct *work)
{
	u32 gpu_freq = gpu_dvfs_get_cur_clock();
	s64 elapsed_ms = 0;
	bool is_gpu_heavy = false;

	if (migov.fragutil >= migov.fragutil_upper_thr && gpu_freq >= migov.gpu_freq_thr)
		is_gpu_heavy = true;
	else if (migov.fragutil <= migov.fragutil_lower_thr || gpu_freq < migov.gpu_freq_thr)
		is_gpu_heavy = false;
	else
		return;

	if (migov.was_gpu_heavy != is_gpu_heavy) {
		migov.was_gpu_heavy = is_gpu_heavy;
		migov.heavy_gpu_start_time = migov.heavy_gpu_end_time;
	}
	migov.heavy_gpu_end_time = ktime_get();

	if ((!is_gpu_heavy && migov.cur_mode == PROFILE_START_BY_GPU)
			|| (is_gpu_heavy && migov.cur_mode == PROFILE_FINISH)) {
		elapsed_ms = ktime_to_ms(ktime_sub(migov.heavy_gpu_end_time,
					migov.heavy_gpu_start_time));
		if (elapsed_ms >= migov.heavy_gpu_ms_thr) {
			migov.heavy_gpu_mode = is_gpu_heavy;
			wakeup_polling_task();
		}
	}
}

static int migov_fragutil_change_callback(struct notifier_block *nb,
				unsigned long val, void *info)
{
	if (val > 100)
		return NOTIFY_DONE;

	migov.fragutil = val;
	schedule_work(&migov.fragutil_work);

	return NOTIFY_OK;
}

static struct notifier_block migov_fragutil_change_notifier = {
	.notifier_call = migov_fragutil_change_callback,
};
#endif

/******************************************************************************/
/*                               SYSFS functions                              */
/******************************************************************************/
/* only for fps profile without migov running and profile only mode */
static ssize_t fps_profile_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s32 ret = 0;
	u32 fps_ = fps_profile.fps / 10;
	u32 _fps = fps_profile.fps % 10;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "profile_time: %llu sec\n", fps_profile.profile_time);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "fps: %llu.%llu\n", fps_, _fps);

	return ret;
}
static ssize_t fps_profile_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 mode;

	if (sscanf(buf, "%u", &mode) != 1)
		return -EINVAL;

	if (!migov.get_frame_cnt)
		return -EINVAL;

	/* start */
	if (mode) {
		migov.get_frame_cnt(&fps_profile.fps, &fps_profile.profile_time);
	} else {
		u64 cur_cnt;
		ktime_t cur_time;

		migov.get_frame_cnt(&cur_cnt, &cur_time);
		fps_profile.profile_time = ktime_sub(cur_time, fps_profile.profile_time) / NSEC_PER_SEC;
		fps_profile.fps = ((cur_cnt - fps_profile.fps) * 10) / fps_profile.profile_time;
	}

	return count;
}
static DEVICE_ATTR_RW(fps_profile);

static ssize_t running_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", migov.running);
}
static ssize_t running_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 mode;

	if (sscanf(buf, "%u", &mode) != 1)
		return -EINVAL;

	migov.game_mode = (bool)mode;
	wakeup_polling_task();

	return count;
}
static DEVICE_ATTR_RW(running);

#define FRAGUTIL_THR_MARGIN	70
static ssize_t fragutil_thr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "upper=%d lower=%d\n",
			migov.fragutil_upper_thr, migov.fragutil_lower_thr);
}
static ssize_t fragutil_thr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	s32 val;

	if (sscanf(buf, "%u", &val) != 1)
		return -EINVAL;

	if (val > 100 | val < 0)
		return -EINVAL;

	migov.fragutil_upper_thr = val;
	migov.fragutil_lower_thr = val * FRAGUTIL_THR_MARGIN / 100;
	migov.fragutil_lower_thr = max(0, migov.fragutil_lower_thr);

	return count;
}
static DEVICE_ATTR_RW(fragutil_thr);

#define MIGOV_ATTR_RW(_name)							\
static ssize_t _name##_show(struct device *dev,					\
		struct device_attribute *attr, char *buf)			\
{										\
	return snprintf(buf, PAGE_SIZE, "%d\n", migov._name);			\
}										\
static ssize_t _name##_store(struct device *dev,				\
		struct device_attribute *attr, const char *buf, size_t count)	\
{										\
	s32 val;								\
										\
	if (sscanf(buf, "%d", &val) != 1)					\
		return -EINVAL;							\
										\
	migov._name = val;							\
										\
	return count;								\
}										\
static DEVICE_ATTR_RW(_name);

MIGOV_ATTR_RW(disable);
MIGOV_ATTR_RW(profile_only);
MIGOV_ATTR_RW(gpu_freq_thr);
MIGOV_ATTR_RW(heavy_gpu_ms_thr);

#define TUNABLE_ATTR_RW(_name)							\
static ssize_t _name##_show(struct device *dev,					\
		struct device_attribute *attr, char *buf)			\
{										\
	return snprintf(buf, PAGE_SIZE, "%d\n", tsd._name);			\
}										\
static ssize_t _name##_store(struct device *dev,				\
		struct device_attribute *attr, const char *buf, size_t count)	\
{										\
	s32 val;								\
										\
	if (sscanf(buf, "%d", &val) != 1)					\
		return -EINVAL;							\
										\
	tsd._name = val;							\
										\
	return count;								\
}										\
static DEVICE_ATTR_RW(_name);

TUNABLE_ATTR_RW(monitor);
TUNABLE_ATTR_RW(window_period);
TUNABLE_ATTR_RW(window_number);
TUNABLE_ATTR_RW(active_pct_thr);
TUNABLE_ATTR_RW(valid_freq_delta_pct);
TUNABLE_ATTR_RW(min_sensitivity);
TUNABLE_ATTR_RW(cpu_bottleneck_thr);
TUNABLE_ATTR_RW(gpu_bottleneck_thr);
TUNABLE_ATTR_RW(gpu_ar_bottleneck_thr);
TUNABLE_ATTR_RW(mif_bottleneck_thr);
TUNABLE_ATTR_RW(dt_ctrl_en);
TUNABLE_ATTR_RW(dt_over_thr);
TUNABLE_ATTR_RW(dt_under_thr);
TUNABLE_ATTR_RW(dt_up_step);
TUNABLE_ATTR_RW(dt_down_step);
TUNABLE_ATTR_RW(dpat_upper_thr);
TUNABLE_ATTR_RW(dpat_lower_thr);
TUNABLE_ATTR_RW(dpat_lower_cnt_thr);
TUNABLE_ATTR_RW(dpat_up_step);
TUNABLE_ATTR_RW(dpat_down_step);
TUNABLE_ATTR_RW(inc_perf_temp_thr);
TUNABLE_ATTR_RW(inc_perf_power_thr);
TUNABLE_ATTR_RW(inc_perf_thr);
TUNABLE_ATTR_RW(dec_perf_thr);
TUNABLE_ATTR_RW(hp_minlock_fps_delta_pct_thr);
TUNABLE_ATTR_RW(hp_minlock_power_upper_thr);
TUNABLE_ATTR_RW(hp_minlock_power_lower_thr);
TUNABLE_ATTR_RW(dyn_mo_control);


#define PER_CPU_TUNABLE_ATTR_RW(_name)						\
static ssize_t _name##_show(struct device *dev,					\
		struct device_attribute *attr, char *buf)			\
{										\
	struct domain_data *dom;						\
	s32 id, ret = 0;							\
										\
	for_each_domain(dom, id) {						\
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "[%d]%s: %d\n",	\
				id, domain_name[id], tsd._name[id]);		\
	}									\
										\
	return ret;								\
}										\
static ssize_t _name##_store(struct device *dev,				\
		struct device_attribute *attr, const char *buf, size_t count)	\
{										\
	s32 id, val;								\
										\
	if (sscanf(buf, "%d %d", &id, &val) != 2)				\
		return -EINVAL;							\
										\
	if (!is_enabled(id))							\
		return -EINVAL;							\
	tsd._name[id] = val;							\
										\
	return count;								\
}										\
static DEVICE_ATTR_RW(_name);

PER_CPU_TUNABLE_ATTR_RW(max_margin);
PER_CPU_TUNABLE_ATTR_RW(min_margin);
PER_CPU_TUNABLE_ATTR_RW(margin_up_step);
PER_CPU_TUNABLE_ATTR_RW(margin_down_step);
PER_CPU_TUNABLE_ATTR_RW(margin_default_step);

static struct attribute *migov_attrs[] = {
	&dev_attr_running.attr,
	&dev_attr_fps_profile.attr,
	&dev_attr_profile_only.attr,
	&dev_attr_monitor.attr,
	&dev_attr_disable.attr,
	&dev_attr_window_period.attr,
	&dev_attr_window_number.attr,
	&dev_attr_active_pct_thr.attr,
	&dev_attr_valid_freq_delta_pct.attr,
	&dev_attr_min_sensitivity.attr,
	&dev_attr_cpu_bottleneck_thr.attr,
	&dev_attr_gpu_bottleneck_thr.attr,
	&dev_attr_gpu_ar_bottleneck_thr.attr,
	&dev_attr_mif_bottleneck_thr.attr,
	&dev_attr_dt_ctrl_en.attr,
	&dev_attr_dt_over_thr.attr,
	&dev_attr_dt_under_thr.attr,
	&dev_attr_dt_up_step.attr,
	&dev_attr_dt_down_step.attr,
	&dev_attr_dpat_upper_thr.attr,
	&dev_attr_dpat_lower_thr.attr,
	&dev_attr_dpat_lower_cnt_thr.attr,
	&dev_attr_dpat_up_step.attr,
	&dev_attr_dpat_down_step.attr,
	&dev_attr_max_margin.attr,
	&dev_attr_min_margin.attr,
	&dev_attr_margin_up_step.attr,
	&dev_attr_margin_down_step.attr,
	&dev_attr_margin_default_step.attr,
	&dev_attr_inc_perf_temp_thr.attr,
	&dev_attr_inc_perf_power_thr.attr,
	&dev_attr_inc_perf_thr.attr,
	&dev_attr_dec_perf_thr.attr,
	&dev_attr_gpu_freq_thr.attr,
	&dev_attr_heavy_gpu_ms_thr.attr,
	&dev_attr_hp_minlock_fps_delta_pct_thr.attr,
	&dev_attr_hp_minlock_power_upper_thr.attr,
	&dev_attr_hp_minlock_power_lower_thr.attr,
	&dev_attr_dyn_mo_control.attr,
	NULL,
};
static struct attribute_group migov_attr_group = {
	.name = "migov",
	.attrs = migov_attrs,
};

#define PRIVATE_ATTR_RW(_priv, _id, _ip, _name)					\
static ssize_t _ip##_##_name##_show(struct device *dev,				\
		struct device_attribute *attr, char *buf)			\
{										\
	struct domain_data *dom = &migov.domains[_id];				\
	struct private_data_##_priv *private;					\
										\
	if (!dom)								\
		return 0;							\
	private = dom->private;							\
	return snprintf(buf, PAGE_SIZE, "%d\n", private->_name);		\
}										\
static ssize_t _ip##_##_name##_store(struct device *dev,			\
		struct device_attribute *attr, const char *buf, size_t count)	\
{										\
	struct domain_data *dom = &migov.domains[_id];				\
	struct private_data_##_priv *private;					\
	s32 val;								\
										\
	if (sscanf(buf, "%d", &val) != 1)					\
		return -EINVAL;							\
	if (!dom)								\
		return 0;							\
	private = dom->private;							\
	private->_name = val;							\
	return count;								\
}										\
static DEVICE_ATTR_RW(_ip##_##_name);
#define CPU_PRIVATE_ATTR_RW(_id, _ip, _name)					\
	PRIVATE_ATTR_RW(cpu, _id, _ip, _name)
#define GPU_PRIVATE_ATTR_RW(_id, _ip, _name)					\
	PRIVATE_ATTR_RW(gpu, _id, _ip, _name)
#define MIF_PRIVATE_ATTR_RW(_id, _ip, _name)					\
	PRIVATE_ATTR_RW(mif, _id, _ip, _name)

CPU_PRIVATE_ATTR_RW(MIGOV_CL0, cl0, pm_qos_min_freq);
CPU_PRIVATE_ATTR_RW(MIGOV_CL0, cl0, pm_qos_max_freq);
CPU_PRIVATE_ATTR_RW(MIGOV_CL0, cl0, hp_minlock_low_limit);
CPU_PRIVATE_ATTR_RW(MIGOV_CL0, cl0, lp_minlock_low_limit);
CPU_PRIVATE_ATTR_RW(MIGOV_CL1, cl1, pm_qos_min_freq);
CPU_PRIVATE_ATTR_RW(MIGOV_CL1, cl1, pm_qos_max_freq);
CPU_PRIVATE_ATTR_RW(MIGOV_CL1, cl1, hp_minlock_low_limit);
CPU_PRIVATE_ATTR_RW(MIGOV_CL1, cl1, lp_minlock_low_limit);
CPU_PRIVATE_ATTR_RW(MIGOV_CL2, cl2, pm_qos_min_freq);
CPU_PRIVATE_ATTR_RW(MIGOV_CL2, cl2, pm_qos_max_freq);
CPU_PRIVATE_ATTR_RW(MIGOV_CL2, cl2, hp_minlock_low_limit);
CPU_PRIVATE_ATTR_RW(MIGOV_CL2, cl2, lp_minlock_low_limit);
GPU_PRIVATE_ATTR_RW(MIGOV_GPU, gpu, q0_empty_pct_thr);
GPU_PRIVATE_ATTR_RW(MIGOV_GPU, gpu, q1_empty_pct_thr);
GPU_PRIVATE_ATTR_RW(MIGOV_GPU, gpu, gpu_active_pct_thr);
GPU_PRIVATE_ATTR_RW(MIGOV_GPU, gpu, pm_qos_max_freq);
GPU_PRIVATE_ATTR_RW(MIGOV_GPU, gpu, pm_qos_min_freq);
MIF_PRIVATE_ATTR_RW(MIGOV_MIF, mif, stats0_mode_min_freq);
MIF_PRIVATE_ATTR_RW(MIGOV_MIF, mif, stats0_sum_thr);
MIF_PRIVATE_ATTR_RW(MIGOV_MIF, mif, stats0_updown_delta_pct_thr);
MIF_PRIVATE_ATTR_RW(MIGOV_MIF, mif, hp_minlock_low_limit);
MIF_PRIVATE_ATTR_RW(MIGOV_MIF, mif, pm_qos_max_freq);
MIF_PRIVATE_ATTR_RW(MIGOV_MIF, mif, pm_qos_min_freq);

#define MAXNUM_OF_PRIV_ATTR 10
static struct attribute *private_attrs[NUM_OF_DOMAIN][MAXNUM_OF_PRIV_ATTR] = {
	{
		&dev_attr_cl0_pm_qos_min_freq.attr,
		&dev_attr_cl0_pm_qos_max_freq.attr,
		&dev_attr_cl0_hp_minlock_low_limit.attr,
		&dev_attr_cl0_lp_minlock_low_limit.attr,
		NULL,
	},
	{
		&dev_attr_cl1_pm_qos_min_freq.attr,
		&dev_attr_cl1_pm_qos_max_freq.attr,
		&dev_attr_cl1_hp_minlock_low_limit.attr,
		&dev_attr_cl1_lp_minlock_low_limit.attr,
		NULL,
	},
	{
		&dev_attr_cl2_pm_qos_min_freq.attr,
		&dev_attr_cl2_pm_qos_max_freq.attr,
		&dev_attr_cl2_hp_minlock_low_limit.attr,
		&dev_attr_cl2_lp_minlock_low_limit.attr,
		NULL,
	},
	{
		&dev_attr_gpu_pm_qos_max_freq.attr,
		&dev_attr_gpu_pm_qos_min_freq.attr,
		&dev_attr_gpu_q0_empty_pct_thr.attr,
		&dev_attr_gpu_q1_empty_pct_thr.attr,
		&dev_attr_gpu_gpu_active_pct_thr.attr,
		NULL,
	},
	{
		&dev_attr_mif_stats0_mode_min_freq.attr,
		&dev_attr_mif_stats0_sum_thr.attr,
		&dev_attr_mif_stats0_updown_delta_pct_thr.attr,
		&dev_attr_mif_hp_minlock_low_limit.attr,
		&dev_attr_mif_pm_qos_max_freq.attr,
		&dev_attr_mif_pm_qos_min_freq.attr,
		NULL,
	},
};

#define RATIO_UNIT	1000
static ssize_t set_margin_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

static void control_max_qos(int id, int value)
{
	struct domain_data *dom = &migov.domains[id];

	switch (id) {
	case MIGOV_CL0:
	case MIGOV_CL1:
	case MIGOV_CL2:
		{
		struct private_data_cpu *private = dom->private;
		if (freq_qos_request_active(&private->pm_qos_max_req))
			freq_qos_update_request(&private->pm_qos_max_req, value);
		}
		break;
	case MIGOV_MIF:
		{
		struct private_data_mif *private = dom->private;
		if (exynos_pm_qos_request_active(&private->pm_qos_max_req))
			exynos_pm_qos_update_request(&private->pm_qos_max_req, value);
		}
		break;
	case MIGOV_GPU:
		{
		struct private_data_gpu *private = dom->private;
		if (exynos_pm_qos_request_active(&private->pm_qos_max_req))
			exynos_pm_qos_update_request(&private->pm_qos_max_req, value);
		}
		break;
	default:
		break;
	}
}

static void control_min_qos(int id, int value)
{
	struct domain_data *dom = &migov.domains[id];

	switch (id) {
	case MIGOV_CL0:
	case MIGOV_CL1:
	case MIGOV_CL2:
		{
		struct private_data_cpu *private = dom->private;
		if (freq_qos_request_active(&private->pm_qos_min_req))
			freq_qos_update_request(&private->pm_qos_min_req, value);
		}
		break;
	case MIGOV_MIF:
		{
		struct private_data_mif *private = dom->private;
		if (exynos_pm_qos_request_active(&private->pm_qos_min_req))
			exynos_pm_qos_update_request(&private->pm_qos_min_req, value);
		}
		break;
	case MIGOV_GPU:
		{
		struct private_data_gpu *private = dom->private;
		if (exynos_pm_qos_request_active(&private->pm_qos_min_req))
			exynos_pm_qos_update_request(&private->pm_qos_min_req, value);
		}
		break;
	default:
		break;
	}
}

static void control_margin(int id, int value)
{
	struct domain_data *dom = &migov.domains[id];

	/* CPU uses pelt margin via ems service */
	if (id <= MIGOV_CL2)
		return;

	if (likely(dom->fn->set_margin))
		dom->fn->set_margin(id, value);
}
static void control_mo(int id, int value)
{
	int idx;

	if (!migov.bts_idx || !migov.len_mo_id)
		return;

	for (idx = 0; idx < migov.len_mo_id; idx++)
		bts_change_mo(migov.bts_idx, migov.mo_id[idx], value, value);

	return;
}

static void control_llc(int value)
{
	if (migov.llc_enable == !!value)
		return;

	migov.llc_enable = !!value;
	llc_enable(migov.llc_enable);
}

/* kernel only to decode */
#define get_op_code(x) (((x) >> OP_CODE_SHIFT) & OP_CODE_MASK)
#define get_ip_id(x) ((x) & IP_ID_MASK)
static ssize_t set_margin_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	s32 id, op, ctrl, value;

	if (migov.profile_only)
		return -EINVAL;

	if (sscanf(buf, "%d %d", &ctrl, &value) != 2)
		return -EINVAL;

	id = get_ip_id(ctrl);
	op = get_op_code(ctrl);

	if (!is_enabled(id) || op == OP_INVALID)
		return -EINVAL;

	switch (op) {
	case OP_PM_QOS_MAX:
		control_max_qos(id, value);
		break;
	case OP_PM_QOS_MIN:
		control_min_qos(id, value);
		break;
	case OP_MARGIN:
		control_margin(id, value);
		break;
	case OP_MO:
		control_mo(id, value);
		break;
	case OP_LLC:
		control_llc(value);
	default:
		break;
	}

	return count;
}
static DEVICE_ATTR_RW(set_margin);

static ssize_t control_profile_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	s32 ret = 0;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "profile_time_ms=%lld\n", psd.profile_time_ms);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "profile_frame_cnt=%llu\n", psd.profile_frame_cnt);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "max_fps=%d\n", psd.max_fps);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "max_freq=%d, %d, %d, %d, %d\n",
			psd.max_freq[0], psd.max_freq[1], psd.max_freq[2], psd.max_freq[3], psd.max_freq[4]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "min_freq=%d, %d, %d, %d, %d\n",
			psd.min_freq[0], psd.min_freq[1], psd.min_freq[2], psd.min_freq[3], psd.min_freq[4]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "freq=%d, %d, %d, %d, %d\n",
			psd.freq[0], psd.freq[1], psd.freq[2], psd.freq[3], psd.freq[4]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "dyn_power=%llu, %llu, %llu, %llu, %llu\n",
			psd.dyn_power[0], psd.dyn_power[1], psd.dyn_power[2], psd.dyn_power[3], psd.dyn_power[4]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "st_power=%llu, %llu, %llu, %llu, %llu\n",
			psd.st_power[0], psd.st_power[1], psd.st_power[2], psd.st_power[3], psd.st_power[4]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "temp=%d, %d, %d, %d, %d\n",
			psd.temp[0], psd.temp[1], psd.temp[2], psd.temp[3], psd.temp[4]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "active_pct=%d, %d, %d, %d, %d\n",
			psd.active_pct[0], psd.active_pct[1], psd.active_pct[2], psd.active_pct[3], psd.active_pct[4]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "q0_empty_pct=%llu\n", psd.q0_empty_pct);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "q1_empty_pct=%llu\n", psd.q1_empty_pct);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "input_nr_avg_cnt=%llu\n", psd.input_nr_avg_cnt);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "stats0_sum=%llu\n", psd.stats0_sum);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "stats0_avg=%llu\n", psd.stats0_avg);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "stats_ratio=%llu\n", psd.stats_ratio);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "mif_pm_qos_cur_freq=%d\n", psd.mif_pm_qos_cur_freq);

	return ret;
}
static ssize_t control_profile_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u32 mode;

	if (sscanf(buf, "%u", &mode) != 1)
		return -EINVAL;

	if (!migov.running && (mode == PROFILE_START_BY_GAME || mode == PROFILE_START_BY_GPU))
		migov_start_profile(mode);
	else if (migov.running && mode == PROFILE_FINISH)
		migov_finish_profile();
	else if (migov.running && mode == PROFILE_UPDATE)
		migov_update_profile();

	return count;
}
static DEVICE_ATTR_RW(control_profile);

static struct attribute *control_attrs[] = {
	&dev_attr_set_margin.attr,
	&dev_attr_control_profile.attr,
	&dev_attr_fragutil_thr.attr,
	NULL,
};
static struct attribute_group control_attr_group = {
	.name = "control",
	.attrs = control_attrs,
};

/******************************************************************************/
/*                                Initialize                                  */
/******************************************************************************/
static s32 init_domain_data(struct device_node *root,
			struct domain_data *dom, void *private_fn)
{
	struct device_node *dn;
	s32 cnt, ret = 0, id = dom->id;

	dn = of_get_child_by_name(root, domain_name[dom->id]);
	if (!dn) {
		pr_err("migov: Failed to get node of domain(id=%d)\n", dom->id);
		return -ENODEV;
	}

	tsd.freq_table_cnt[id] = dom->fn->get_table_cnt(id);
	cnt = dom->fn->get_freq_table(id, tsd.freq_table[id]);
	if (tsd.freq_table_cnt[id] != cnt) {
		pr_err("migov: table cnt(%u) is un-synced with freq table cnt(%u)(id=%d)\n",
					id, tsd.freq_table_cnt[id], cnt);
		return -EINVAL;
	}

	ret |= of_property_read_s32(dn, "max", &tsd.max_margin[id]);
	ret |= of_property_read_s32(dn, "min", &tsd.min_margin[id]);
	ret |= of_property_read_s32(dn, "up-step", &tsd.margin_up_step[id]);
	ret |= of_property_read_s32(dn, "down-step", &tsd.margin_down_step[id]);
	ret |= of_property_read_s32(dn, "default-step", &tsd.margin_default_step[id]);
	if (ret)
		return -EINVAL;

	if (id <= MIGOV_CL2) {
		struct private_data_cpu *private = dom->private;
		struct cpufreq_policy *policy;

		private = kzalloc(sizeof(struct private_data_cpu), GFP_KERNEL);
		if (!private)
			return -ENOMEM;

		private->fn = private_fn;
		
		tsd.asv_ids[id] = private->fn->cpu_asv_ids(id);

		ret |= of_property_read_s32(dn, "pm-qos-cpu", &private->pm_qos_cpu);
		policy = cpufreq_cpu_get(private->pm_qos_cpu);
		if (!policy) {
			kfree(private);
			return -EINVAL;
		}
		private->num_of_cpu = cpumask_weight(policy->related_cpus);

		if (of_property_read_s32(dn, "pm-qos-min-freq", &private->pm_qos_min_freq))
			private->pm_qos_min_freq = PM_QOS_DEFAULT_VALUE;
		if (of_property_read_s32(dn, "pm-qos-max-freq", &private->pm_qos_max_freq))
			private->pm_qos_max_freq = PM_QOS_DEFAULT_VALUE;
		if (of_property_read_s32(dn, "hp-minlock-low-limit", &private->hp_minlock_low_limit))
			private->hp_minlock_low_limit = PM_QOS_DEFAULT_VALUE;
		if (of_property_read_s32(dn, "lp-minlock-low-limit", &private->lp_minlock_low_limit))
			private->lp_minlock_low_limit = PM_QOS_DEFAULT_VALUE;

		freq_qos_tracer_add_request(&policy->constraints, &private->pm_qos_min_req,
				FREQ_QOS_MIN, PM_QOS_DEFAULT_VALUE);
		freq_qos_tracer_add_request(&policy->constraints, &private->pm_qos_max_req,
				FREQ_QOS_MAX, PM_QOS_DEFAULT_VALUE);

		if (!ret) {
			dom->private = private;
		} else {
			kfree(private);
		}
	} else if (dom->id == MIGOV_GPU) {
		struct private_data_gpu *private;
		s32 val;

		private = kzalloc(sizeof(struct private_data_gpu), GFP_KERNEL);
		if (!private)
			return -ENOMEM;

		private->fn = private_fn;

		ret |= of_property_read_s32(dn, "pm-qos-max-class", &private->pm_qos_max_class);
		if (of_property_read_s32(dn, "pm-qos-max-freq", &private->pm_qos_max_freq))
			private->pm_qos_max_freq = PM_QOS_DEFAULT_VALUE;

		ret |= of_property_read_s32(dn, "pm-qos-min-class", &private->pm_qos_min_class);
		if (of_property_read_s32(dn, "pm-qos-min-freq", &private->pm_qos_min_freq))
			private->pm_qos_min_freq = PM_QOS_DEFAULT_VALUE;

		ret |= of_property_read_s32(dn, "q0-empty-pct-thr", &val);
		private->q0_empty_pct_thr = val;
		ret |= of_property_read_s32(dn, "q1-empty-pct-thr", &val);
		private->q1_empty_pct_thr = val;
		ret |= of_property_read_s32(dn, "active-pct-thr", &val);
		private->gpu_active_pct_thr = val;

		if (!ret) {
			/* gpu use negative value for unlock */
			exynos_pm_qos_add_request(&private->pm_qos_min_req,
					private->pm_qos_min_class, EXYNOS_PM_QOS_DEFAULT_VALUE);
			exynos_pm_qos_add_request(&private->pm_qos_max_req,
					private->pm_qos_max_class, EXYNOS_PM_QOS_DEFAULT_VALUE);

			dom->private = private;
		} else {
			kfree(private);
		}
	} else if (dom->id == MIGOV_MIF) {
		struct private_data_mif *private;
		s32 val;

		private = kzalloc(sizeof(struct private_data_mif), GFP_KERNEL);
		if (!private)
			return -ENOMEM;

		private->fn = private_fn;

		ret |= of_property_read_s32(dn, "pm-qos-max-class", &private->pm_qos_max_class);
		ret |= of_property_read_s32(dn, "pm-qos-min-class", &private->pm_qos_min_class);
		ret |= of_property_read_s32(dn, "freq-stats0-mode-min-freq",
				&private->stats0_mode_min_freq);
		ret |= of_property_read_s32(dn, "freq-stats0-thr", &val);
		private->stats0_sum_thr = val;
		ret |= of_property_read_s32(dn, "freq-stats0-updown-delta-pct-thr", &val);
		private->stats0_updown_delta_pct_thr = val;
		ret |= of_property_read_s32(dn, "hp-minlock-low-limit", &private->hp_minlock_low_limit);

		if (of_property_read_s32(dn, "pm-qos-max-freq", &private->pm_qos_max_freq))
			private->pm_qos_max_freq = PM_QOS_DEFAULT_VALUE;
		if (of_property_read_s32(dn, "pm-qos-min-freq", &private->pm_qos_min_freq))
			private->pm_qos_min_freq = PM_QOS_DEFAULT_VALUE;

		if (!ret) {
			exynos_pm_qos_add_request(&private->pm_qos_min_req,
					private->pm_qos_min_class, EXYNOS_PM_QOS_DEFAULT_VALUE);
			exynos_pm_qos_add_request(&private->pm_qos_max_req,
					private->pm_qos_max_class, EXYNOS_PM_QOS_DEFAULT_VALUE);

			dom->private = private;
		} else {
			kfree(private);
		}
	}

	if (!ret) {
		dom->attr_group.name = domain_name[dom->id];
		dom->attr_group.attrs = private_attrs[dom->id];
		sysfs_create_group(migov.kobj, &dom->attr_group);
	}

	return 0;
}

s32 exynos_migov_register_misc_device(void)
{
	s32 ret;

	migov.gov_fops.fops.owner		= THIS_MODULE;
	migov.gov_fops.fops.llseek		= no_llseek;
	migov.gov_fops.fops.read		= migov_fops_read;
	migov.gov_fops.fops.poll		= migov_fops_poll;
	migov.gov_fops.fops.unlocked_ioctl	= migov_fops_ioctl;
	migov.gov_fops.fops.compat_ioctl	= migov_fops_ioctl;
	migov.gov_fops.fops.release		= migov_fops_release;

	migov.gov_fops.miscdev.minor		= MISC_DYNAMIC_MINOR;
	migov.gov_fops.miscdev.name		= "exynos-migov";
	migov.gov_fops.miscdev.fops		= &migov.gov_fops.fops;

	ret = misc_register(&migov.gov_fops.miscdev);
        if (ret) {
                pr_err("exynos-migov couldn't register misc device!");
                return ret;
        }

	return 0;
}

s32 exynos_migov_register_domain(s32 id, struct domain_fn *fn, void *private_fn)
{
	struct domain_data *dom = &migov.domains[id];

	if (id >= NUM_OF_DOMAIN || (dom->enabled)) {
		pr_err("migov: invalid id or duplicated register (id: %d)\n", id);
		return -EINVAL;
	}

	if (!fn) {
		pr_err("migov: there is no callback address (id: %d)\n", id);
		return -EINVAL;
	}

	dom->id = id;
	dom->fn = fn;

	if (init_domain_data(migov.dn, dom, private_fn)) {
		pr_err("migov: failed to init domain data(id=%d)\n", id);
		return -EINVAL;
	}

	dom->enabled = true;

	return 0;
}
EXPORT_SYMBOL_GPL(exynos_migov_register_domain);

static s32 parse_migov_dt(struct device_node *dn)
{
	s32 ret = 0;

	ret |= of_property_read_s32(dn, "window-period", &tsd.window_period);
	ret |= of_property_read_s32(dn, "window-number", &tsd.window_number);
	ret |= of_property_read_s32(dn, "active-pct-thr", &tsd.active_pct_thr);
	ret |= of_property_read_s32(dn, "valid-freq-delta-pct", &tsd.valid_freq_delta_pct);
	ret |= of_property_read_s32(dn, "min-sensitivity", &tsd.min_sensitivity);
	ret |= of_property_read_s32(dn, "cpu-bottleneck-thr", &tsd.cpu_bottleneck_thr);
	ret |= of_property_read_s32(dn, "gpu-bottleneck-thr", &tsd.gpu_bottleneck_thr);
	ret |= of_property_read_s32(dn, "gpu-ar-bottleneck-thr", &tsd.gpu_ar_bottleneck_thr);
	ret |= of_property_read_s32(dn, "mif-bottleneck-thr", &tsd.mif_bottleneck_thr);
	ret |= of_property_read_s32(dn, "frame-src", &tsd.frame_src);
	ret |= of_property_read_s32(dn, "max-fps", &tsd.max_fps);
	psd.max_fps = tsd.max_fps;
	ret |= of_property_read_s32(dn, "dt-ctrl-en", &tsd.dt_ctrl_en);
	ret |= of_property_read_s32(dn, "dt-over-thr", &tsd.dt_over_thr);
	ret |= of_property_read_s32(dn, "dt-under-thr", &tsd.dt_under_thr);
	ret |= of_property_read_s32(dn, "dt-up-step", &tsd.dt_up_step);
	ret |= of_property_read_s32(dn, "dt-down-step", &tsd.dt_down_step);
	ret |= of_property_read_s32(dn, "dpat-upper-thr", &tsd.dpat_upper_thr);
	ret |= of_property_read_s32(dn, "dpat-lower-thr", &tsd.dpat_lower_thr);
	ret |= of_property_read_s32(dn, "dpat-lower-cnt-thr", &tsd.dpat_lower_cnt_thr);
	ret |= of_property_read_s32(dn, "dpat-up-step", &tsd.dpat_up_step);
	ret |= of_property_read_s32(dn, "dpat-down-step", &tsd.dpat_down_step);
	ret |= of_property_read_s32(dn, "inc-perf-temp-thr", &tsd.inc_perf_temp_thr);
	ret |= of_property_read_s32(dn, "inc-perf-power-thr", &tsd.inc_perf_power_thr);
	ret |= of_property_read_s32(dn, "inc-perf-thr", &tsd.inc_perf_thr);
	ret |= of_property_read_s32(dn, "dec-perf-thr", &tsd.dec_perf_thr);
	ret |= of_property_read_s32(dn, "fragutil-thr", &migov.fragutil_upper_thr);
	migov.fragutil_lower_thr = migov.fragutil_upper_thr * FRAGUTIL_THR_MARGIN / 100;
	migov.fragutil_lower_thr = max(0, migov.fragutil_lower_thr);
	ret |= of_property_read_s32(dn, "gpu-freq-thr", &migov.gpu_freq_thr);
	ret |= of_property_read_s32(dn, "heavy-gpu-ms-thr", &migov.heavy_gpu_ms_thr);
	ret |= of_property_read_s32(dn, "hp-minlock-fps-delta-pct-thr", &tsd.hp_minlock_fps_delta_pct_thr);
	ret |= of_property_read_s32(dn, "hp-minlock-power-upper-thr", &tsd.hp_minlock_power_upper_thr);
	ret |= of_property_read_s32(dn, "hp-minlock-power-lower-thr", &tsd.hp_minlock_power_lower_thr);

	ret |= of_property_read_s32(dn, "dyn-mo-control", &tsd.dyn_mo_control);
	migov.bts_idx = bts_get_scenindex("g3d_heavy");
	migov.len_mo_id = of_property_count_u32_elems(dn, "mo-id");
	if (migov.len_mo_id > 0) {
		migov.mo_id = kzalloc(sizeof(int) * migov.len_mo_id, GFP_KERNEL);
		if (!migov.mo_id)
			return ret;

		ret |= of_property_read_u32_array(dn, "mo-id", migov.mo_id, migov.len_mo_id);
	}

	return ret;
}

static s32 exynos_migov_suspend(struct device *dev)
{
	if (migov.running) {
		migov.game_mode = false;
		migov.heavy_gpu_mode = false;
		wakeup_polling_task();
	}

	return 0;
}

static s32 exynos_migov_resume(struct device *dev)
{
	return 0;
}

static s32 exynos_migov_probe(struct platform_device *pdev)
{
	s32 ret;

	migov.disable = false;
	migov.running = false;

	migov.dn = pdev->dev.of_node;
	if (!migov.dn) {
		pr_err("migov: Failed to get device tree\n");
		return -EINVAL;
	}

	ret = parse_migov_dt(migov.dn);
	if (ret) {
		pr_err("migov: Failed to parse device tree\n");
		return ret;
	}

	migov.kobj = &pdev->dev.kobj;
	ret += sysfs_create_group(migov.kobj, &migov_attr_group);
	ret += sysfs_create_group(migov.kobj, &control_attr_group);
	if (ret) {
		pr_err("migov: Failed to init sysfs\n");
		return ret;
	}

	INIT_WORK(&migov.fps_work, migov_fps_change_work);

#if IS_ENABLED(CONFIG_MALI_NOTIFY_UTILISATION)
	INIT_WORK(&migov.fragutil_work, migov_fragutil_change_work);
	ret = register_frag_utils_change_notifier(&migov_fragutil_change_notifier);
	if (ret) {
		pr_err("migov: Failed to register fragutil change notifier\n");
		return ret;
	}
#endif

	migov.running_event = PROFILE_INVALID;
	init_waitqueue_head(&migov.wq);
	exynos_migov_register_misc_device();
	mutex_init(&migov_lock);

	pr_info("migov: complete to probe migov\n");

	return ret;
}

static const struct of_device_id exynos_migov_match[] = {
	{ .compatible	= "samsung,exynos-migov", },
	{ },
};
MODULE_DEVICE_TABLE(of, exynos_migov_match);

static const struct dev_pm_ops exynos_migov_pm_ops = {
	.suspend	= exynos_migov_suspend,
	.resume		= exynos_migov_resume,
};

static struct platform_driver exynos_migov_driver = {
	.probe		= exynos_migov_probe,
	.driver	= {
		.name	= "exynos-migov",
		.owner	= THIS_MODULE,
		.pm	= &exynos_migov_pm_ops,
		.of_match_table = exynos_migov_match,
	},
};

static s32 exynos_migov_init(void)
{
	return platform_driver_register(&exynos_migov_driver);
}
late_initcall(exynos_migov_init);

MODULE_DESCRIPTION("Exynos MIGOV");
MODULE_LICENSE("GPL");
