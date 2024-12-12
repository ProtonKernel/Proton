/*
 * Copyright (c) 2018 Park Bumgyu, Samsung Electronics Co., Ltd <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Exynos CPU Power Management driver implementation
 */

#include <linux/cpumask.h>
#include <linux/slab.h>
#include <linux/tick.h>
#include <linux/cpu.h>
#include <linux/cpuhotplug.h>
#include <linux/cpu_pm.h>
#include <linux/cpuidle.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <soc/samsung/exynos-cpupm.h>
#include <soc/samsung/cal-if.h>
#include <soc/samsung/exynos-pmu-if.h>
#include <soc/samsung/exynos-pm.h>

#if IS_ENABLED(CONFIG_SEC_PM) && IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#endif /* CONFIG_PDIC_NOTIFIER */
static void exynos_cpupm_muic_notifier_init(void);
#else
static inline void exynos_cpupm_muic_notifier_init(void) {}
#endif /* !CONFIG_SEC_PM && !CONFIG_MUIC_NOTIFIER */

/*
 * State of CPUPM objects
 * All CPUPM objects have 2 states, BUSY and IDLE.
 *
 * @BUSY
 * a state in which the power domain referred to by the object is turned on.
 *
 * @IDLE
 * a state in which the power domain referred to by the object is turned off.
 * However, the power domain is not necessarily turned off even if the object
 * is in IDLE state because the cpu may be booting or executing power off
 * sequence.
 */
enum {
	CPUPM_STATE_BUSY = 0,
	CPUPM_STATE_IDLE,
};

/* Length of power mode name */
#define NAME_LEN	32

/* CPUPM statistics */
struct cpupm_stats {
	/* count of power mode entry */
	unsigned int entry_count;

	/* count of power mode entry cancllations */
	unsigned int cancel_count;

	/* power mode residency time */
	s64 residency_time;

	/* time entered power mode */
	ktime_t entry_time;
};

struct wakeup_mask {
	int mask_reg_offset;
	int stat_reg_offset;
	int mask;
};

/* wakeup mask configuration for system idle */
struct wakeup_mask_config {
	int num_wakeup_mask;
	struct wakeup_mask *wakeup_masks;

	int num_eint_wakeup_mask;
	int *eint_wakeup_mask_reg_offset;
} *wm_config;

/*
 * Power modes
 * In CPUPM, power mode controls the power domain consisting of cpu and enters
 * the power mode by cpuidle. Basically, to enter power mode, all cpus in power
 * domain must be in IDLE state, and sleep length of cpus must be smaller than
 * target_residency.
 */
struct power_mode {
	/* name of power mode, it is declared in device tree */
	char		name[NAME_LEN];

	/* power mode state, BUSY or IDLE */
	int		state;

	/* sleep length criterion of cpus to enter power mode */
	int		target_residency;

	/* type according to range of power domain */
	int		type;

	/* index of cal, for POWERMODE_TYPE_CLUSTER */
	int		cal_id;

	/* cpus belonging to the power domain */
	struct cpumask	siblings;

	/*
	 * Among siblings, the cpus that can enter the power mode.
	 * Due to H/W constraint, only certain cpus need to enter power mode.
	 */
	struct cpumask	entry_allowed;

	/* disable count */
	atomic_t	disable;

	/*
	 * device attribute for sysfs,
	 * it supports for enabling or disabling this power mode
	 */
	struct device_attribute	attr;

	/* user's request for enabling/disabling power mode */
	bool		user_request;

	/* list of power mode */
	struct list_head	list;

	/* CPUPM statistics */
	struct cpupm_stats	stat;
	struct cpupm_stats	stat_snapshot;
};

static LIST_HEAD(mode_list);

/*
 * Main struct of CPUPM
 * Each cpu has its own data structure and main purpose of this struct is to
 * manage the state of the cpu and the power modes containing the cpu.
 */
struct exynos_cpupm {
	/* cpu state, BUSY or IDLE */
	int			state;

	/* array to manage the power mode that contains the cpu */
	struct power_mode *	modes[POWERMODE_TYPE_END];
};

static struct exynos_cpupm __percpu *cpupm;
static bool __percpu *hotplug_ing;
static int system_suspended;

#define NSCODE_BASE		(0xBFFFF000)
#define CPU_STATE_BASE_OFFSET	(0x2C)
#define CPU_STATE_OFFSET(i)	(CPU_STATE_BASE_OFFSET + (i * 0x4))

#define CPU_NET_BASE_OFFSET	(0x500)
#define CPU_NET_OFFSET(i)	(CPU_NET_BASE_OFFSET + (i * 0x8))

#define CPU_PIPI_BASE_OFFSET	(0x540)
#define CPU_PIPI_OFFSET(i)	(CPU_PIPI_BASE_OFFSET + (i * 0x4))

#define CPU_STATE_SPARE_OFFSET	(0x54)
#define CANCEL_FLAG		(1 << 5)

#define BACKUP_CPU_STATE_BASE	(0xBFFFF400)

static void __iomem *nscode_base;
static void __iomem *backup_cpu_state_base;

static void do_nothing(void *unused) { }

/******************************************************************************
 *                                    Notifier                                *
 ******************************************************************************/
static DEFINE_RWLOCK(notifier_lock);
static RAW_NOTIFIER_HEAD(notifier_chain);

int exynos_cpupm_notifier_register(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	write_lock_irqsave(&notifier_lock, flags);
	ret = raw_notifier_chain_register(&notifier_chain, nb);
	write_unlock_irqrestore(&notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_cpupm_notifier_register);

static int exynos_cpupm_notify(int event, int v)
{
	int ret = 0;

	read_lock(&notifier_lock);
	ret = raw_notifier_call_chain(&notifier_chain, event, &v);
	read_unlock(&notifier_lock);

	return notifier_to_errno(ret);
}

/******************************************************************************
 *                                  IDLE_IP                                   *
 ******************************************************************************/
struct idle_ip {
	/* list node of ip */
	struct list_head	list;

	/* ip name */
	const char		*name;

	/* ip index, unique */
	unsigned int		index;

	/* IO coherency */
	unsigned int		io_cc;

	/* ip type , 0:normal ip, 1:extern ip */
	unsigned int		type;

	/* ip idle state, 0:busy, 1:io-co */
	unsigned int		idle;

	/* busy count for cpuidle-profiler */
	unsigned int		busy_count;
	unsigned int		busy_count_profile;

	/* pmu offset for extern idle-ip */
	unsigned int		pmu_offset;
};

static DEFINE_SPINLOCK(idle_ip_lock);

static LIST_HEAD(ip_list);

#define NORMAL_IP	0
#define EXTERN_IP	1
static bool __ip_busy(struct idle_ip *ip)
{
	unsigned int val;

	if (ip->type == NORMAL_IP) {
		if (ip->idle == CPUPM_STATE_BUSY)
			return true;
		else
			return false;
	}

	if (ip->type == EXTERN_IP) {
		exynos_pmu_read(ip->pmu_offset, &val);
		if (!!val == CPUPM_STATE_BUSY)
			return true;
		else
			return false;
	}

	return false;
}

static void cpupm_profile_idle_ip(void);

static bool ip_busy(struct power_mode *mode)
{
	struct idle_ip *ip;
	unsigned long flags;

	spin_lock_irqsave(&idle_ip_lock, flags);

	cpupm_profile_idle_ip();

	list_for_each_entry(ip, &ip_list, list) {
		if (__ip_busy(ip)) {
			/*
			 * IP that does not do IO coherency with DSU does
			 * not need to be checked in DSU off mode.
			 */
			if (mode->type == POWERMODE_TYPE_DSU && !ip->io_cc)
				continue;

			spin_unlock_irqrestore(&idle_ip_lock, flags);

			/* IP is busy */
			return true;
		}
	}
	spin_unlock_irqrestore(&idle_ip_lock, flags);

	/* IPs are idle */
	return false;
}

static struct idle_ip *find_ip(int index)
{
	struct idle_ip *ip = NULL;

	list_for_each_entry(ip, &ip_list, list)
		if (ip->index == index)
			break;

	return ip;
}

/*
 * @index: index of idle-ip
 * @idle: idle status, (idle == 0)non-idle or (idle == 1)idle
 */
void exynos_update_ip_idle_status(int index, int idle)
{
	struct idle_ip *ip;
	unsigned long flags;

	spin_lock_irqsave(&idle_ip_lock, flags);
	ip = find_ip(index);
	if (!ip) {
		pr_err("unknown idle-ip index %d\n", index);
		spin_unlock_irqrestore(&idle_ip_lock, flags);
		return;
	}

	ip->idle = idle;
	spin_unlock_irqrestore(&idle_ip_lock, flags);
}
EXPORT_SYMBOL_GPL(exynos_update_ip_idle_status);

/*
 * register idle-ip dynamically by name, return idle-ip index.
 */
int exynos_get_idle_ip_index(const char *name, int io_cc)
{
	struct idle_ip *ip;
	unsigned long flags;
	int new_index;

	ip = kzalloc(sizeof(struct idle_ip), GFP_KERNEL);
	if (!ip) {
		pr_err("Failed to allocate idle_ip. [ip=%s].", name);
		return -ENOMEM;
	}

	spin_lock_irqsave(&idle_ip_lock, flags);

	if (list_empty(&ip_list))
		new_index = 0;
	else
		new_index = list_last_entry(&ip_list,
				struct idle_ip, list)->index + 1;

	ip->name = name;
	ip->index = new_index;
	ip->type = NORMAL_IP;
	ip->io_cc = io_cc;
	list_add_tail(&ip->list, &ip_list);

	spin_unlock_irqrestore(&idle_ip_lock, flags);

	exynos_update_ip_idle_status(ip->index, CPUPM_STATE_BUSY);

	return ip->index;
}
EXPORT_SYMBOL_GPL(exynos_get_idle_ip_index);

/******************************************************************************
 *                               CPUPM profiler                               *
 ******************************************************************************/
static ktime_t cpupm_init_time;
static void cpupm_profile_begin(struct cpupm_stats *stat)
{
	stat->entry_time = ktime_get();
	stat->entry_count++;
}

static void cpupm_profile_end(struct cpupm_stats *stat, int cancel)
{
	if (!stat->entry_time)
		return;

	if (cancel) {
		stat->cancel_count++;
		return;
	}

	stat->residency_time +=
		ktime_to_us(ktime_sub(ktime_get(), stat->entry_time));
	stat->entry_time = 0;
}

static u32 idle_ip_check_count;
static u32 idle_ip_check_count_profile;

static void cpupm_profile_idle_ip(void)
{
	struct idle_ip *ip;

	idle_ip_check_count++;

	list_for_each_entry(ip, &ip_list, list)
		if (__ip_busy(ip))
			ip->busy_count++;
}

static int profiling;
static ktime_t profile_time;

static ssize_t profile_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct power_mode *mode;
	struct idle_ip *ip;
	s64 total;
	int ret = 0;

	if (profiling)
		return scnprintf(buf, PAGE_SIZE, "Profile is ongoing\n");

	ret += scnprintf(buf + ret, PAGE_SIZE - ret,
			 "format : [mode] [entry_count] [cancel_count] [time] [(ratio)]\n\n");

	total = ktime_to_us(profile_time);
	list_for_each_entry(mode, &mode_list, list) {
		ret += scnprintf(buf + ret, PAGE_SIZE - ret,
				 "%s %d %d %lld (%d%%)\n",
				 mode->name,
				 mode->stat_snapshot.entry_count,
				 mode->stat_snapshot.cancel_count,
				 mode->stat_snapshot.residency_time,
				 mode->stat_snapshot.residency_time * 100 / total);
	}

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	ret += scnprintf(buf + ret, PAGE_SIZE - ret,
			"IDLE-IP statistics\n\n");
	ret += scnprintf(buf + ret, PAGE_SIZE - ret,
			"(I:IO-CC, E:Extern IP)\n");
	list_for_each_entry(ip, &ip_list, list)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret,
				 "%s%s %-20s : busy %d/%d\n",
				 ip->io_cc ? "[I]" : "[-]",
				 ip->type == EXTERN_IP ? "[E]" : "[-]",
				 ip->name, ip->busy_count_profile,
				 idle_ip_check_count_profile);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "(total %lldus)\n", total);

	return ret;
}

static ssize_t profile_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct power_mode *mode;
	struct idle_ip *ip;
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	input = !!input;
	if (profiling == input)
		return count;

	profiling = input;

	if (!input)
		goto stop_profile;

	preempt_disable();
	smp_call_function(do_nothing, NULL, 1);
	preempt_enable();

	list_for_each_entry(mode, &mode_list, list)
		mode->stat_snapshot = mode->stat;

	list_for_each_entry(ip, &ip_list, list)
		ip->busy_count_profile = ip->busy_count;

	profile_time = ktime_get();
	idle_ip_check_count_profile = idle_ip_check_count;

	return count;

stop_profile:
#define delta(a, b)	(a = b - a)
#define field_delta(field)			\
	delta(mode->stat_snapshot.field, mode->stat.field);

	preempt_disable();
	smp_call_function(do_nothing, NULL, 1);
	preempt_enable();

	list_for_each_entry(mode, &mode_list, list) {
		field_delta(entry_count);
		field_delta(cancel_count);
		field_delta(residency_time);
	}

	list_for_each_entry(ip, &ip_list, list)
		ip->busy_count_profile = ip->busy_count - ip->busy_count_profile;

	profile_time = ktime_sub(ktime_get(), profile_time);
	idle_ip_check_count_profile = idle_ip_check_count
				- idle_ip_check_count_profile;

	return count;
}

static ssize_t time_in_state_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct power_mode *mode;
	struct idle_ip *ip;
	s64 total = ktime_to_us(ktime_sub(ktime_get(), cpupm_init_time));
	int ret = 0;

	ret += scnprintf(buf + ret, PAGE_SIZE - ret,
			"format : [mode] [entry_count] [cancel_count] [time] [(ratio)]\n\n");

	list_for_each_entry(mode, &mode_list, list) {
		ret += scnprintf(buf + ret, PAGE_SIZE - ret,
				 "%-7s %d %d %lld (%d%%)\n",
				 mode->name,
				 mode->stat.entry_count,
				 mode->stat.cancel_count,
				 mode->stat.residency_time,
				 mode->stat.residency_time * 100 / total);
	}

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	ret += scnprintf(buf + ret, PAGE_SIZE - ret,
			"[IDLE-IP statistics]\n");
	ret += scnprintf(buf + ret, PAGE_SIZE - ret,
			"(I:IO-CC, E:Extern IP)\n");
	list_for_each_entry(ip, &ip_list, list)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret,
				 "%s%s %-20s : busy %d/%d\n",
				 ip->io_cc ? "[I]" : "[-]",
				 ip->type == EXTERN_IP ? "[E]" : "[-]",
				 ip->name, ip->busy_count,
				 idle_ip_check_count);
	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += scnprintf(buf + ret, PAGE_SIZE - ret, "(total %lldus)\n", total);

	return ret;
}

static DEVICE_ATTR_RO(time_in_state);
static DEVICE_ATTR_RW(profile);

/******************************************************************************
 *                            CPU idle management                             *
 ******************************************************************************/
/* Macros for CPUPM state */
#define set_state_busy(object)		((object)->state = CPUPM_STATE_BUSY)
#define set_state_idle(object)		((object)->state = CPUPM_STATE_IDLE)
#define check_state_busy(object)	((object)->state == CPUPM_STATE_BUSY)
#define check_state_idle(object)	((object)->state == CPUPM_STATE_IDLE)
#define valid_powermode(type)		((type >= 0) && (type < POWERMODE_TYPE_END))
/*
 * State of each cpu is managed by a structure declared by percpu, so there
 * is no need for protection for synchronization. However, when entering
 * the power mode, it is necessary to set the critical section to check the
 * state of cpus in the power domain, cpupm_lock is used for it.
 */
static spinlock_t cpupm_lock;

static void awake_cpus(const struct cpumask *cpus)
{
	int cpu;

	for_each_cpu_and(cpu, cpus, cpu_online_mask)
		smp_call_function_single(cpu, do_nothing, NULL, 1);
}

/*
 * disable_power_mode/enable_power_mode
 * It provides "disable" function to enable/disable power mode as required by
 * user or device driver. To handle multiple disable requests, it use the
 * atomic disable count, and disable the mode that contains the given cpu.
 */
static void __disable_power_mode(struct power_mode *mode)
{
	/*
	 * There are no entry allowed cpus, it means that mode is
	 * disabled, skip awaking cpus.
	 */
	if (cpumask_empty(&mode->entry_allowed))
		return;

	/*
	 * The first disable request wakes the cpus to exit power mode
	 */
	if (atomic_inc_return(&mode->disable) == 1)
		awake_cpus(&mode->siblings);
}

void disable_power_mode(int cpu, int type)
{
	struct exynos_cpupm *pm;
	struct power_mode *mode;

	if (!valid_powermode(type))
		return;

	pm = per_cpu_ptr(cpupm, cpu);
	mode = pm->modes[type];
	if (mode == NULL)
		return;

	__disable_power_mode(mode);
}
EXPORT_SYMBOL_GPL(disable_power_mode);

static void __enable_power_mode(struct power_mode *mode)
{
	atomic_dec(&mode->disable);
	awake_cpus(&mode->siblings);
}

void enable_power_mode(int cpu, int type)
{
	struct exynos_cpupm *pm;
	struct power_mode *mode;

	if (!valid_powermode(type))
		return;

	pm = per_cpu_ptr(cpupm, cpu);
	mode = pm->modes[type];
	if (mode == NULL)
		return;

	__enable_power_mode(mode);
}
EXPORT_SYMBOL_GPL(enable_power_mode);

static s64 get_sleep_length(int cpu, ktime_t now)
{
	ktime_t next_event = readq_relaxed(nscode_base + CPU_NET_OFFSET(cpu));
	return ktime_to_us(ktime_sub(next_event, now));
}

static bool pending_ipi(int cpu)
{
	return readl_relaxed(nscode_base + CPU_PIPI_OFFSET(cpu));
}

static int cpus_busy(int target_residency, const struct cpumask *cpus)
{
	int cpu;
	ktime_t now = ktime_get();

	/*
	 * If there is even one cpu which is not in POWERDOWN state or has
	 * the smaller sleep length than target_residency, CPUPM regards
	 * it as BUSY.
	 */
	for_each_cpu_and(cpu, cpu_online_mask, cpus) {
		struct exynos_cpupm *pm = per_cpu_ptr(cpupm, cpu);

		if (check_state_busy(pm))
			return -EBUSY;

		if (get_sleep_length(cpu, now) < target_residency)
			return -EBUSY;

		if (pending_ipi(cpu))
			return -EBUSY;
	}

	return 0;
}

static int cpus_last_core_detecting(int request_cpu, const struct cpumask *cpus)
{
	int cpu;

	for_each_cpu_and(cpu, cpu_online_mask, cpus) {
		if (cpu == request_cpu)
			continue;

		if (cal_is_lastcore_detecting(cpu))
			return -EBUSY;
	}

	return 0;
}

#define BOOT_CPU	0
static int cluster_busy(void)
{
	int cpu;
	struct power_mode *mode = NULL;

	for_each_online_cpu(cpu) {
		if (mode && cpumask_test_cpu(cpu, &mode->siblings))
			continue;

		mode = per_cpu_ptr(cpupm, cpu)->modes[POWERMODE_TYPE_CLUSTER];
		if (mode == NULL)
			continue;

		if (cpumask_test_cpu(BOOT_CPU, &mode->siblings))
			continue;

		if (check_state_busy(mode))
			return 1;
	}

	return 0;
}

static bool system_busy(struct power_mode *mode)
{
	if (mode->type < POWERMODE_TYPE_DSU)
		return false;

	if (cluster_busy())
		return true;

	if (ip_busy(mode))
		return true;

	return false;
}

/*
 * In order to enter the power mode, the following conditions must be met:
 * 1. power mode should not be disabled
 * 2. the cpu attempting to enter must be a cpu that is allowed to enter the
 *    power mode.
 */
static bool entry_allow(int cpu, struct power_mode *mode)
{
	if (atomic_read(&mode->disable))
		return false;

	if (!cpumask_test_cpu(cpu, &mode->entry_allowed))
		return false;

	if (cpus_busy(mode->target_residency, &mode->siblings))
		return false;

	if (cpus_last_core_detecting(cpu, &mode->siblings))
		return false;

	if (system_busy(mode))
		return false;

	if (mode->type == POWERMODE_TYPE_CLUSTER && exynos_cpupm_notify(CPD_ENTER, 0))
		return false;

	return true;
}

extern u32 exynos_eint_wake_mask_array[3];
static void set_wakeup_mask(void)
{
	int i;

	if (!wm_config) {
		WARN_ONCE(1, "no wakeup mask information\n");
		return;
	}

	for (i = 0; i < wm_config->num_wakeup_mask; i++) {
		exynos_pmu_write(wm_config->wakeup_masks[i].stat_reg_offset, 0);
		exynos_pmu_write(wm_config->wakeup_masks[i].mask_reg_offset,
				 wm_config->wakeup_masks[i].mask);
	}

	for (i = 0; i < wm_config->num_eint_wakeup_mask; i++)
		exynos_pmu_write(wm_config->eint_wakeup_mask_reg_offset[i],
				exynos_eint_wake_mask_array[i]);
}

static void cluster_disable(struct power_mode *mode)
{
	cal_cluster_disable(mode->cal_id);
}

static void cluster_enable(struct power_mode *mode)
{
	cal_cluster_enable(mode->cal_id);
}

static bool system_disabled;
static int system_disable(struct power_mode *mode)
{
	if (mode->type == POWERMODE_TYPE_DSU)
		acpm_noti_dsu_cpd(true);
	else
		acpm_noti_dsu_cpd(false);

	if (system_disabled)
		return 0;

	if (mode->type == POWERMODE_TYPE_SYSTEM) {
		if (unlikely(exynos_cpupm_notify(SICD_ENTER, 0)))
			return -EBUSY;
	} else if (mode->type == POWERMODE_TYPE_DSU) {
		if (unlikely(exynos_cpupm_notify(DSUPD_ENTER, 0)))
			return -EBUSY;
	}

	cal_pm_enter(mode->cal_id);
	set_wakeup_mask();
	system_disabled = 1;

	return 0;
}

static void system_enable(struct power_mode *mode, int cancel)
{
	if (!system_disabled)
		return;

	if (cancel)
		cal_pm_earlywakeup(mode->cal_id);
	else
		cal_pm_exit(mode->cal_id);

	if (mode->type == POWERMODE_TYPE_SYSTEM)
		exynos_cpupm_notify(SICD_EXIT, cancel);
	else if (mode->type == POWERMODE_TYPE_DSU)
		exynos_cpupm_notify(DSUPD_EXIT, cancel);

	system_disabled = 0;
}

static void enter_power_mode(int cpu, struct power_mode *mode)
{
	switch (mode->type) {
	case POWERMODE_TYPE_CLUSTER:
		cluster_disable(mode);
		break;
	case POWERMODE_TYPE_DSU:
	case POWERMODE_TYPE_SYSTEM:
		if (system_disable(mode))
			return;
		break;
	}

	dbg_snapshot_cpuidle_mod(mode->name, 0, 0, DSS_FLAG_IN);
	set_state_idle(mode);

	cpupm_profile_begin(&mode->stat);
}

static void exit_power_mode(int cpu, struct power_mode *mode, int cancel)
{
	cpupm_profile_end(&mode->stat, cancel);

	/*
	 * Configure settings to exit power mode. This is executed by the
	 * first cpu exiting from power mode.
	 */
	set_state_busy(mode);
	dbg_snapshot_cpuidle_mod(mode->name, 0, 0, DSS_FLAG_OUT);

	switch (mode->type) {
	case POWERMODE_TYPE_CLUSTER:
		cluster_enable(mode);
		break;
	case POWERMODE_TYPE_DSU:
	case POWERMODE_TYPE_SYSTEM:
		system_enable(mode, cancel);
		break;
	}
}

/*
 * exynos_cpu_pm_enter() and exynos_cpu_pm_exit() called by cpu_pm_notify
 * to handle platform specific configuration to control cpu power domain.
 */
static void exynos_cpupm_enter(int cpu)
{
	struct exynos_cpupm *pm;
	int i;

	spin_lock(&cpupm_lock);
	pm = per_cpu_ptr(cpupm, cpu);

	/* Configure PMUCAL to power down core */
	cal_cpu_disable(cpu);

	/* Set cpu state to IDLE */
	set_state_idle(pm);

	/* Try to enter power mode */
	for (i = 0; i < POWERMODE_TYPE_END; i++) {
		struct power_mode *mode = pm->modes[i];

		if (mode == NULL)
			continue;

		if (entry_allow(cpu, mode))
			enter_power_mode(cpu, mode);
	}

	spin_unlock(&cpupm_lock);
}

static void exynos_cpupm_exit(int cpu, int cancel)
{
	struct exynos_cpupm *pm;
	int i;

	spin_lock(&cpupm_lock);
	pm = per_cpu_ptr(cpupm, cpu);

	/* Make settings to exit from mode */
	for (i = 0; i < POWERMODE_TYPE_END; i++) {
		struct power_mode *mode = pm->modes[i];

		if (mode == NULL)
			continue;

		if (check_state_idle(mode))
			exit_power_mode(cpu, mode, cancel);
	}

	/* Set cpu state to BUSY */
	set_state_busy(pm);

	/* Configure PMUCAL to power up core */
	cal_cpu_enable(cpu);

	spin_unlock(&cpupm_lock);
}

static int exynos_cpu_pm_notify_callback(struct notifier_block *self,
		unsigned long action, void *v)
{
	int cpu = smp_processor_id();
	int cpu_state;

	/* ignore CPU_PM event in suspend sequence */
	if (system_suspended)
		return NOTIFY_OK;

	switch (action) {
	case CPU_PM_ENTER:
		/*
		 * There are few block condition of C2.
		 *  - UCC don't allow enter c-state. (notify)
		 *  - while cpu is hotpluging.
		 */
		if (exynos_cpupm_notify(C2_ENTER, 0) ||
			*per_cpu_ptr(hotplug_ing, cpu))
			return NOTIFY_BAD;

		exynos_cpupm_enter(cpu);
		break;
	case CPU_PM_EXIT:
		cpu_state = readl_relaxed(nscode_base + CPU_STATE_OFFSET(cpu));
		exynos_cpupm_exit(cpu, cpu_state & CANCEL_FLAG);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_cpu_pm_notifier = {
	.notifier_call = exynos_cpu_pm_notify_callback,
};

/******************************************************************************
 *                               sysfs interface                              *
 ******************************************************************************/
static ssize_t debug_net_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cpu, ret = 0;

	for_each_online_cpu(cpu)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "[cpu%d] %lld\n",
				cpu, readq_relaxed(nscode_base + CPU_NET_OFFSET(cpu)));

	return ret;
}
DEVICE_ATTR_RO(debug_net);

static ssize_t debug_pipi_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cpu, ret = 0;

	for_each_online_cpu(cpu)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "[cpu%d] %d\n",
				cpu, readl_relaxed(nscode_base + CPU_PIPI_OFFSET(cpu)));

	return ret;
}
DEVICE_ATTR_RO(debug_pipi);

static ssize_t idle_ip_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct idle_ip *ip;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&idle_ip_lock, flags);

	list_for_each_entry(ip, &ip_list, list)
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "[%d] %s %s\n",
				 ip->index, ip->name,
				 ip->type == EXTERN_IP ? "(E)" : "");

	spin_unlock_irqrestore(&idle_ip_lock, flags);

	return ret;
}
DEVICE_ATTR_RO(idle_ip);

static ssize_t power_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret, i;
	struct power_mode *mode = container_of(attr, struct power_mode, attr);

	ret = scnprintf(buf, PAGE_SIZE, "%s\n",
			 atomic_read(&mode->disable) > 0 ? "disabled" : "enabled");

	for (i = 0; i < 8; i++)
		ret += scnprintf(buf + ret, PAGE_SIZE, "%X\n",
			readl_relaxed(backup_cpu_state_base + (i * 0x4)));

	return ret;
}

static ssize_t power_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_mode *mode = container_of(attr, struct power_mode, attr);
	unsigned int val;
	int cpu, type;

	if (!sscanf(buf, "%u", &val))
		return -EINVAL;

	cpu = cpumask_any(&mode->siblings);
	type = mode->type;

	val = !!val;
	if (mode->user_request == val)
		return count;

	mode->user_request = val;
	if (val)
		__enable_power_mode(mode);
	else
		__disable_power_mode(mode);

	return count;
}

static struct attribute *exynos_cpupm_attrs[] = {
	&dev_attr_debug_net.attr,
	&dev_attr_debug_pipi.attr,
	&dev_attr_idle_ip.attr,
	&dev_attr_time_in_state.attr,
	&dev_attr_profile.attr,
	NULL,
};

static struct attribute_group exynos_cpupm_group = {
	.name = "cpupm",
	.attrs = exynos_cpupm_attrs,
};

#define CPUPM_ATTR(_attr, _name, _mode, _show, _store)		\
	sysfs_attr_init(&_attr.attr);				\
	_attr.attr.name	= _name;				\
	_attr.attr.mode	= VERIFY_OCTAL_PERMISSIONS(_mode);	\
	_attr.show	= _show;				\
	_attr.store	= _store;

/******************************************************************************
 *                               CPU HOTPLUG                                  *
 ******************************************************************************/
static int cpuhp_cpupm_online(unsigned int cpu)
{
	struct exynos_cpupm *pm = per_cpu_ptr(cpupm, cpu);
	struct power_mode *mode = pm->modes[POWERMODE_TYPE_CLUSTER];

	if (mode) {
		struct cpumask mask;

		cpumask_and(&mask, &mode->siblings, cpu_online_mask);

		if (cpumask_weight(&mask) == 0)
			cluster_enable(mode);
	}

	cal_cpu_enable(cpu);

	/*
	 * At this point, mark this cpu as finished the hotplug.
	 * Because the hotplug in sequence is done, this cpu could enter C2.
	 */
	*per_cpu_ptr(hotplug_ing, cpu) = false;

	return 0;
}

static int cpuhp_cpupm_offline(unsigned int cpu)
{
	struct exynos_cpupm *pm = per_cpu_ptr(cpupm, cpu);
	struct power_mode *mode = pm->modes[POWERMODE_TYPE_CLUSTER];

	/*
	 * At this point, mark this cpu as entering the hotplug.
	 * In order not to confusing ACPM, the cpu that entering the hotplug
	 * should not enter C2.
	 */
	*per_cpu_ptr(hotplug_ing, cpu) = true;

	cal_cpu_disable(cpu);

	if (mode) {
		struct cpumask mask;

		cpumask_and(&mask, &mode->siblings, cpu_online_mask);

		if (cpumask_weight(&mask) == 1)
			cluster_disable(mode);
	}

	return 0;
}

/******************************************************************************
 *                                Initialization                              *
 ******************************************************************************/
static void wakeup_mask_init(struct device_node *cpupm_dn)
{
	struct device_node *root_dn, *wm_dn, *dn;
	int count, i;

	root_dn = of_find_node_by_name(cpupm_dn, "wakeup-mask");
	if (!root_dn) {
		pr_warn("wakeup-mask is omitted in device tree\n");
		return;
	}

	wm_config = kzalloc(sizeof(struct wakeup_mask_config), GFP_KERNEL);
	if (!wm_config) {
		pr_warn("failed to allocate wakeup mask config\n");
		return;
	}

	/* initialize wakeup-mask */
	wm_dn = of_find_node_by_name(root_dn, "wakeup-masks");
	if (!wm_dn) {
		pr_warn("wakeup-masks is omitted in device tree\n");
		goto fail;
	}

	count = of_get_child_count(wm_dn);
	wm_config->num_wakeup_mask = count;
	wm_config->wakeup_masks = kcalloc(count,
			sizeof(struct wakeup_mask), GFP_KERNEL);
	if (!wm_config->wakeup_masks) {
		pr_warn("failed to allocate wakeup masks\n");
		goto fail;
	}

	i = 0;
	for_each_child_of_node(wm_dn, dn) {
		of_property_read_u32(dn, "mask-reg-offset",
			&wm_config->wakeup_masks[i].mask_reg_offset);
		of_property_read_u32(dn, "stat-reg-offset",
			&wm_config->wakeup_masks[i].stat_reg_offset);
		of_property_read_u32(dn, "mask",
			&wm_config->wakeup_masks[i].mask);
		i++;
	}

	/* initialize eint-wakeup-mask */
	wm_dn = of_find_node_by_name(root_dn, "eint-wakeup-masks");
	if (!wm_dn) {
		pr_warn("eint-wakeup-masks is omitted in device tree\n");
		goto fail;
	}

	count = of_get_child_count(wm_dn);
	wm_config->num_eint_wakeup_mask = count;
	wm_config->eint_wakeup_mask_reg_offset = kcalloc(count,
			sizeof(int), GFP_KERNEL);
	if (!wm_config->eint_wakeup_mask_reg_offset) {
		pr_warn("failed to allocate eint wakeup masks\n");
		goto fail;
	}

	i = 0;
	for_each_child_of_node(wm_dn, dn) {
		of_property_read_u32(dn, "mask-reg-offset",
			&wm_config->eint_wakeup_mask_reg_offset[i]);
		i++;
	}

	return;

fail:
	kfree(wm_config->wakeup_masks);
	kfree(wm_config->eint_wakeup_mask_reg_offset);
	kfree(wm_config);
}

static int exynos_cpupm_mode_init(struct platform_device *pdev)
{
	struct device_node *dn = pdev->dev.of_node;

	cpupm = alloc_percpu(struct exynos_cpupm);

	while ((dn = of_find_node_by_type(dn, "cpupm"))) {
		struct power_mode *mode;
		const char *buf;
		int cpu, ret;

		/*
		 * Power mode is dynamically generated according to
		 * what is defined in device tree.
		 */
		mode = kzalloc(sizeof(struct power_mode), GFP_KERNEL);
		if (!mode) {
			pr_err("Failed to allocate powermode.\n");
			return -ENOMEM;
		}

		strncpy(mode->name, dn->name, NAME_LEN - 1);

		ret = of_property_read_u32(dn, "target-residency",
				&mode->target_residency);
		if (ret)
			return ret;

		ret = of_property_read_u32(dn, "type", &mode->type);
		if (ret)
			return ret;

		if (!valid_powermode(mode->type))
			return -EINVAL;

		if (!of_property_read_string(dn, "siblings", &buf)) {
			cpulist_parse(buf, &mode->siblings);
			cpumask_and(&mode->siblings, &mode->siblings,
					cpu_possible_mask);
		}

		if (!of_property_read_string(dn, "entry-allowed", &buf)) {
			cpulist_parse(buf, &mode->entry_allowed);
			cpumask_and(&mode->entry_allowed, &mode->entry_allowed,
					cpu_possible_mask);
		}

		ret = of_property_read_u32(dn, "cal-id", &mode->cal_id);
		if (ret) {
			if (mode->type >= POWERMODE_TYPE_DSU)
				mode->cal_id = SYS_SICD;
			else
				return ret;
		}

		atomic_set(&mode->disable, 0);

		/*
		 * The users' request is set to enable since initialization
		 * state of power mode is enabled.
		 */
		mode->user_request = true;

		/*
		 * Initialize attribute for sysfs.
		 * The absence of entry allowed cpu is equivalent to this power
		 * mode being disabled. In this case, no attribute is created.
		 */
		if (!cpumask_empty(&mode->entry_allowed)) {
			CPUPM_ATTR(mode->attr, mode->name, 0644,
					power_mode_show, power_mode_store);

			ret = sysfs_add_file_to_group(&pdev->dev.kobj,
					&mode->attr.attr,
					exynos_cpupm_group.name);
			if (ret)
				pr_warn("Failed to add sysfs or POWERMODE\n");
		}

		/* Connect power mode to the cpus in the power domain */
		for_each_cpu(cpu, &mode->siblings) {
			struct exynos_cpupm *pm = per_cpu_ptr(cpupm, cpu);

			pm->modes[mode->type] = mode;
		}

		list_add_tail(&mode->list, &mode_list);

		/*
		 * At the point of CPUPM initialization, all CPUs are already
		 * hotplugged in without calling cluster_enable() because
		 * CPUPM cannot know cal-id at that time.
		 * Explicitly call cluster_enable() here.
		 */
		if (mode->type == POWERMODE_TYPE_CLUSTER)
			cluster_enable(mode);
	}

	wakeup_mask_init(pdev->dev.of_node);

	return 0;
}

#define PMU_IDLE_IP(x)			(0x03E0 + (0x4 * x))
#define EXTERN_IDLE_IP_MAX		4
static int extern_idle_ip_init(struct device_node *dn)
{
	struct device_node *child = of_get_child_by_name(dn, "idle-ip");
	int i, count, new_index;
	unsigned long flags;

	if (!child)
		return 0;

	count = of_property_count_strings(child, "extern-idle-ip");
	if (count <= 0 || count > EXTERN_IDLE_IP_MAX)
		return 0;

	spin_lock_irqsave(&idle_ip_lock, flags);

	for (i = 0; i < count; i++) {
		struct idle_ip *ip;
		const char *name;

		ip = kzalloc(sizeof(struct idle_ip), GFP_KERNEL);
		if (!ip) {
			pr_err("Failed to allocate idle_ip [ip=%s].", name);
			spin_unlock_irqrestore(&idle_ip_lock, flags);
			return -ENOMEM;
		}

		of_property_read_string_index(child, "extern-idle-ip", i, &name);

		new_index = list_last_entry(&ip_list,
				struct idle_ip, list)->index + 1;

		ip->name = name;
		ip->index = new_index;
		ip->type = EXTERN_IP;
		ip->pmu_offset = PMU_IDLE_IP(i);
		list_add_tail(&ip->list, &ip_list);
	}

	spin_unlock_irqrestore(&idle_ip_lock, flags);

	return 0;
}

static int exynos_cpupm_suspend_noirq(struct device *dev)
{
	system_suspended = 1;
	return 0;
}

static int exynos_cpupm_resume_noirq(struct device *dev)
{
	system_suspended = 0;
	return 0;
}

static const struct dev_pm_ops cpupm_pm_ops = {
	.suspend_noirq = exynos_cpupm_suspend_noirq,
	.resume_noirq = exynos_cpupm_resume_noirq,
};

static int exynos_cpupm_probe(struct platform_device *pdev)
{
	int ret, cpu;

	ret = extern_idle_ip_init(pdev->dev.of_node);
	if (ret)
		return ret;

	ret = sysfs_create_group(&pdev->dev.kobj, &exynos_cpupm_group);
	if (ret)
		pr_warn("Failed to create sysfs for CPUPM\n");

	/* Link CPUPM sysfs to /sys/devices/system/cpu/cpupm */
	if (sysfs_create_link(&cpu_subsys.dev_root->kobj,
				&pdev->dev.kobj, "cpupm"))
		pr_err("Failed to link CPUPM sysfs to cpu\n");

	ret = exynos_cpupm_mode_init(pdev);
	if (ret)
		return ret;

	/* set PMU to power on */
	for_each_online_cpu(cpu)
		cal_cpu_enable(cpu);

	hotplug_ing = alloc_percpu(bool);
	cpuhp_setup_state(CPUHP_BP_PREPARE_DYN,
			"AP_EXYNOS_CPU_POWER_UP_CONTROL",
			cpuhp_cpupm_online, NULL);

	cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
			"AP_EXYNOS_CPU_POWER_DOWN_CONTROL",
			NULL, cpuhp_cpupm_offline);

	spin_lock_init(&cpupm_lock);

	nscode_base = ioremap(NSCODE_BASE, SZ_4K);
	backup_cpu_state_base = ioremap(BACKUP_CPU_STATE_BASE, SZ_4K);

	ret = cpu_pm_register_notifier(&exynos_cpu_pm_notifier);
	if (ret)
		return ret;

	/*
	 * The power off sequence is executed after CPU_STATE_SPARE is filled.
	 * Before writing to value to CPU_STATE_SPARE, interrupt is asserted to
	 * all CPUs to cancel power off sequence in progress. Because there may
	 * be a CPU that tries to power off without setting PMUCAL handled in
	 * CPU_PM_ENTER event.
	 */
	smp_call_function_many(cpu_online_mask, do_nothing, NULL, 1);
	writel_relaxed(0xBABECAFE, nscode_base + CPU_STATE_SPARE_OFFSET);

	cpupm_init_time = ktime_get();

	exynos_cpupm_muic_notifier_init();

	return 0;
}

static const struct of_device_id of_exynos_cpupm_match[] = {
	{ .compatible = "samsung,exynos-cpupm", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_exynos_cpupm_match);

static struct platform_driver exynos_cpupm_driver = {
	.driver = {
		.name = "exynos-cpupm",
		.owner = THIS_MODULE,
		.of_match_table = of_exynos_cpupm_match,
#ifdef CONFIG_PM_SLEEP
		.pm = &cpupm_pm_ops,
#endif
	},
	.probe		= exynos_cpupm_probe,
};

static int __init exynos_cpupm_driver_init(void)
{
	return platform_driver_register(&exynos_cpupm_driver);
}
arch_initcall(exynos_cpupm_driver_init);

static void __exit exynos_cpupm_driver_exit(void)
{
	platform_driver_unregister(&exynos_cpupm_driver);
}
module_exit(exynos_cpupm_driver_exit);

#if IS_ENABLED(CONFIG_SEC_PM) && IS_ENABLED(CONFIG_MUIC_NOTIFIER)
struct notifier_block cpuidle_muic_nb;

static int exynos_cpupm_muic_notifier(struct notifier_block *nb,
				unsigned long action, void *data)
{
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	PD_NOTI_ATTACH_TYPEDEF *p_noti = (PD_NOTI_ATTACH_TYPEDEF *)data;
	muic_attached_dev_t attached_dev = p_noti->cable_type;
#else
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
#endif

	switch (attached_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH) {
			pr_info("%s: JIG(%d) is detached\n", __func__, attached_dev);
			enable_power_mode(0, POWERMODE_TYPE_SYSTEM);
			enable_power_mode(0, POWERMODE_TYPE_DSU);
		} else if (action == MUIC_NOTIFY_CMD_ATTACH) {
			pr_info("%s: JIG(%d) is attached\n", __func__, attached_dev);
			disable_power_mode(0, POWERMODE_TYPE_DSU);
			disable_power_mode(0, POWERMODE_TYPE_SYSTEM);
		} else {
			pr_err("%s: ACTION Error!(%lu)\n", __func__, action);
		}
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static void exynos_cpupm_muic_notifier_init(void)
{
	muic_notifier_register(&cpuidle_muic_nb, exynos_cpupm_muic_notifier,
			MUIC_NOTIFY_DEV_CPUIDLE);
}
#endif /* CONFIG_MUIC_NOTIFIER && CONFIG_SEC_PM */

MODULE_DESCRIPTION("Exynos CPUPM driver");
MODULE_LICENSE("GPL");
