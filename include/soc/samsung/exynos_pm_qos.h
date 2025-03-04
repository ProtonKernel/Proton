/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_EXYNOS_PM_QOS_H
#define _LINUX_EXYNOS_PM_QOS_H
/* interface for the exynos_pm_qos_power infrastructure of the linux kernel.
 *
 * Mark Gross <mgross@linux.intel.com>
 */
#include <linux/plist.h>
#include <linux/notifier.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>

enum {
	PM_QOS_NETWORK_LATENCY = 1,
	PM_QOS_CLUSTER0_FREQ_MIN,
	PM_QOS_CLUSTER0_FREQ_MAX,
	PM_QOS_CLUSTER1_FREQ_MIN,
	PM_QOS_CLUSTER1_FREQ_MAX,
	PM_QOS_CLUSTER2_FREQ_MIN,
	PM_QOS_CLUSTER2_FREQ_MAX,
	PM_QOS_CPU_ONLINE_MIN,
	PM_QOS_CPU_ONLINE_MAX,
	PM_QOS_DEVICE_THROUGHPUT,
	PM_QOS_INTCAM_THROUGHPUT,
	PM_QOS_DEVICE_THROUGHPUT_MAX,
	PM_QOS_INTCAM_THROUGHPUT_MAX,
	PM_QOS_BUS_THROUGHPUT,
	PM_QOS_BUS_THROUGHPUT_MAX,
	PM_QOS_DISPLAY_THROUGHPUT,
	PM_QOS_DISPLAY_THROUGHPUT_MAX,
	PM_QOS_CAM_THROUGHPUT,
	PM_QOS_AUD_THROUGHPUT,
	PM_QOS_CAM_THROUGHPUT_MAX,
	PM_QOS_AUD_THROUGHPUT_MAX,
	PM_QOS_MFC_THROUGHPUT,
	PM_QOS_NPU_THROUGHPUT,
	PM_QOS_MFC_THROUGHPUT_MAX,
	PM_QOS_NPU_THROUGHPUT_MAX,
	PM_QOS_GPU_THROUGHPUT_MIN,
	PM_QOS_GPU_THROUGHPUT_MAX,
	PM_QOS_VPC_THROUGHPUT,
	PM_QOS_VPC_THROUGHPUT_MAX,
	PM_QOS_CSIS_THROUGHPUT,
	PM_QOS_CSIS_THROUGHPUT_MAX,
	PM_QOS_ISP_THROUGHPUT,
	PM_QOS_ISP_THROUGHPUT_MAX,
	PM_QOS_MFC1_THROUGHPUT,
	PM_QOS_MFC1_THROUGHPUT_MAX,
	/* insert new class ID */
#if IS_ENABLED(CONFIG_ARGOS)
	PM_QOS_NETWORK_THROUGHPUT,
#endif
	EXYNOS_PM_QOS_NUM_CLASSES,
};

enum exynos_pm_qos_flags_status {
	EXYNOS_PM_QOS_FLAGS_UNDEFINED = -1,
	EXYNOS_PM_QOS_FLAGS_NONE,
	EXYNOS_PM_QOS_FLAGS_SOME,
	EXYNOS_PM_QOS_FLAGS_ALL,
};

#define EXYNOS_PM_QOS_DEFAULT_VALUE	(-1)
#define EXYNOS_PM_QOS_LATENCY_ANY	S32_MAX
#define EXYNOS_PM_QOS_LATENCY_ANY_NS	((s64)EXYNOS_PM_QOS_LATENCY_ANY * NSEC_PER_USEC)

#define EXYNOS_PM_QOS_CPU_DMA_LAT_DEFAULT_VALUE	(2000 * USEC_PER_SEC)
#define PM_QOS_NETWORK_LAT_DEFAULT_VALUE	(2000 * USEC_PER_SEC)
#define PM_QOS_DEVICE_THROUGHPUT_DEFAULT_VALUE	0
#define PM_QOS_INTCAM_THROUGHPUT_DEFAULT_VALUE	0
#define PM_QOS_DEVICE_THROUGHPUT_MAX_DEFAULT_VALUE	INT_MAX
#define PM_QOS_INTCAM_THROUGHPUT_MAX_DEFAULT_VALUE	INT_MAX
#define PM_QOS_BUS_THROUGHPUT_DEFAULT_VALUE	0
#define PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE	INT_MAX
#define PM_QOS_DISPLAY_THROUGHPUT_DEFAULT_VALUE	0
#define PM_QOS_DISPLAY_THROUGHPUT_MAX_DEFAULT_VALUE	INT_MAX
#define PM_QOS_CAM_THROUGHPUT_DEFAULT_VALUE	0
#define PM_QOS_AUD_THROUGHPUT_DEFAULT_VALUE	0
#define PM_QOS_DSP_THROUGHPUT_DEFAULT_VALUE	0
#define PM_QOS_DNC_THROUGHPUT_DEFAULT_VALUE	0
#define PM_QOS_FSYS0_THROUGHPUT_DEFAULT_VALUE	0
#define PM_QOS_CAM_THROUGHPUT_MAX_DEFAULT_VALUE		INT_MAX
#define PM_QOS_AUD_THROUGHPUT_MAX_DEFAULT_VALUE		INT_MAX
#define PM_QOS_DSP_THROUGHPUT_MAX_DEFAULT_VALUE		INT_MAX
#define PM_QOS_DNC_THROUGHPUT_MAX_DEFAULT_VALUE	INT_MAX
#define PM_QOS_FSYS0_THROUGHPUT_MAX_DEFAULT_VALUE	INT_MAX
#define PM_QOS_MFC_THROUGHPUT_DEFAULT_VALUE	0
#define PM_QOS_NPU_THROUGHPUT_DEFAULT_VALUE	0
#define PM_QOS_MFC_THROUGHPUT_MAX_DEFAULT_VALUE	INT_MAX
#define PM_QOS_NPU_THROUGHPUT_MAX_DEFAULT_VALUE	INT_MAX
#define PM_QOS_NETWORK_THROUGHPUT_DEFAULT_VALUE	0
#define PM_QOS_MEMORY_BANDWIDTH_DEFAULT_VALUE	0
#define PM_QOS_LATENCY_TOLERANCE_DEFAULT_VALUE	0
#define PM_QOS_MIN_FREQUENCY_DEFAULT_VALUE	0
#define PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE	FREQ_QOS_MAX_DEFAULT_VALUE
#define PM_QOS_LATENCY_TOLERANCE_NO_CONSTRAINT	(-1)
#define PM_QOS_CLUSTER0_FREQ_MIN_DEFAULT_VALUE	0
#define PM_QOS_CLUSTER0_FREQ_MAX_DEFAULT_VALUE	INT_MAX
#define PM_QOS_CLUSTER1_FREQ_MIN_DEFAULT_VALUE	0
#define PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE	INT_MAX
#define PM_QOS_CLUSTER2_FREQ_MIN_DEFAULT_VALUE	0
#define PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE	INT_MAX
#define PM_QOS_CPU_ONLINE_MIN_DEFAULT_VALUE	1
#define PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE	NR_CPUS
#define PM_QOS_TNR_THROUGHPUT_DEFAULT_VALUE	0
#define PM_QOS_TNR_THROUGHPUT_MAX_DEFAULT_VALUE	INT_MAX
#define PM_QOS_GPU_FREQ_MIN_DEFAULT_VALUE	0
#define PM_QOS_GPU_FREQ_MAX_DEFAULT_VALUE	INT_MAX
#define PM_QOS_VPC_THROUGHPUT_DEFAULT_VALUE	0
#define PM_QOS_VPC_THROUGHPUT_MAX_DEFAULT_VALUE	INT_MAX
#define PM_QOS_CSIS_THROUGHPUT_DEFAULT_VALUE	0
#define PM_QOS_CSIS_THROUGHPUT_MAX_DEFAULT_VALUE	INT_MAX
#define PM_QOS_ISP_THROUGHPUT_DEFAULT_VALUE	0
#define PM_QOS_ISP_THROUGHPUT_MAX_DEFAULT_VALUE	INT_MAX
#define PM_QOS_MFC1_THROUGHPUT_DEFAULT_VALUE	0
#define PM_QOS_MFC1_THROUGHPUT_MAX_DEFAULT_VALUE	INT_MAX

#define EXYNOS_PM_QOS_FLAG_NO_POWER_OFF	(1 << 0)

#define exynos_pm_qos_add_request(arg...)	do {				\
	exynos_pm_qos_add_request_trace((char *)__func__, __LINE__, ##arg);	\
} while(0)

struct exynos_pm_qos_request {
	struct plist_node node;
	int exynos_pm_qos_class;
	struct delayed_work work; /* for exynos_pm_qos_update_request_timeout */
	char *func;
	unsigned int line;
};

struct exynos_pm_qos_flags_request {
	struct list_head node;
	s32 flags;	/* Do not change to 64 bit */
};

enum exynos_pm_qos_type {
	EXYNOS_PM_QOS_UNITIALIZED,
	EXYNOS_PM_QOS_MAX,		/* return the largest value */
	EXYNOS_PM_QOS_MIN,		/* return the smallest value */
	EXYNOS_PM_QOS_SUM		/* return the sum */
};

/*
 * Note: The lockless read path depends on the CPU accessing target_value
 * or effective_flags atomically.  Atomic access is only guaranteed on all CPU
 * types linux supports for 32 bit quantites
 */
struct exynos_pm_qos_constraints {
	struct plist_head list;
	s32 target_value;	/* Do not change to 64 bit */
	s32 default_value;
	s32 no_constraint_value;
	enum exynos_pm_qos_type type;
	struct blocking_notifier_head *notifiers;
	spinlock_t lock;
	struct mutex mlock;
};

struct exynos_pm_qos_flags {
	struct list_head list;
	s32 effective_flags;	/* Do not change to 64 bit */
};


/* Action requested to exynos_pm_qos_update_target */
enum exynos_pm_qos_req_action {
	EXYNOS_PM_QOS_ADD_REQ,		/* Add a new request */
	EXYNOS_PM_QOS_UPDATE_REQ,	/* Update an existing request */
	EXYNOS_PM_QOS_REMOVE_REQ	/* Remove an existing request */
};

extern int exynos_pm_qos_update_target(struct exynos_pm_qos_constraints *c, struct plist_node *node,
		enum exynos_pm_qos_req_action action, int value);
extern void exynos_pm_qos_add_request_trace(char *func, unsigned int line,
		struct exynos_pm_qos_request *req, int exynos_pm_qos_class,
		s32 value);
extern void exynos_pm_qos_update_request(struct exynos_pm_qos_request *req,
		s32 new_value);
extern void exynos_pm_qos_update_request_timeout(struct exynos_pm_qos_request *req,
		s32 new_value, unsigned long timeout_us);
extern void exynos_pm_qos_remove_request(struct exynos_pm_qos_request *req);

extern int exynos_pm_qos_request(int exynos_pm_qos_class);
extern int exynos_pm_qos_add_notifier(int exynos_pm_qos_class, struct notifier_block *notifier);
extern int exynos_pm_qos_remove_notifier(int exynos_pm_qos_class, struct notifier_block *notifier);
extern int exynos_pm_qos_request_active(struct exynos_pm_qos_request *req);
extern s32 exynos_pm_qos_read_value(struct exynos_pm_qos_constraints *c);
extern int exynos_pm_qos_read_req_value(int pm_qos_class, struct exynos_pm_qos_request *req);
extern void show_exynos_pm_qos_data(int index);

#include <linux/proc_fs.h>
#if IS_ENABLED(CONFIG_SEC_PM_LOG)
extern void exynos_pm_qos_procfs_init(struct proc_dir_entry *parent);
#else
static inline void exynos_pm_qos_procfs_init(struct proc_dir_entry *parent) {}
#endif /* !CONFIG_SEC_PM_LOG */
#endif
