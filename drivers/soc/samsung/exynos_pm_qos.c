// SPDX-License-Identifier: GPL-2.0-only
/*
 * This module exposes the interface to kernel space for specifying
 * QoS dependencies.  It provides infrastructure for registration of:
 *
 * Dependents on a QoS value : register requests
 * Watchers of QoS value : get notified when target QoS value changes
 *
 * This QoS design is best effort based.  Dependents register their QoS needs.
 * Watchers register to keep track of the current QoS needs of the system.
 *
 * There are 3 basic classes of QoS parameter: latency, timeout, throughput
 * each have defined units:
 * latency: usec
 * timeout: usec <-- currently not used.
 * throughput: kbs (kilo byte / sec)
 *
 * There are lists of exynos_pm_qos_objects each one wrapping requests, notifiers
 *
 * User mode requests on a QOS parameter register themselves to the
 * subsystem by opening the device node /dev/... and writing there request to
 * the node.  As long as the process holds a file handle open to the node the
 * client continues to be accounted for.  Upon file release the usermode
 * request is removed and a new qos target is computed.  This way when the
 * request that the application has is cleaned up when closes the file
 * pointer or exits the exynos_pm_qos_object will get an opportunity to clean up.
 *
 */

/*#define DEBUG*/

#include <linux/module.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/plist.h>

#include <linux/uaccess.h>
#include <linux/export.h>
//#include <trace/events/power.h>

void exynos_plist_del(struct plist_node *node, struct plist_head *head);
void exynos_plist_add(struct plist_node *node, struct plist_head *head);

/*
 * locking rule: all changes to constraints or notifiers lists
 * or exynos_pm_qos_object list and exynos_pm_qos_objects need to happen with exynos_pm_qos_lock
 * held, taken with _irqsave.  One lock to rule them all
 */
struct exynos_pm_qos_object {
	struct exynos_pm_qos_constraints *constraints;
	struct miscdevice exynos_pm_qos_power_miscdev;
	char *name;
};

static struct exynos_pm_qos_object null_exynos_pm_qos;

static BLOCKING_NOTIFIER_HEAD(network_lat_notifier);
static struct exynos_pm_qos_constraints network_lat_constraints = {
	.list = PLIST_HEAD_INIT(network_lat_constraints.list),
	.target_value = PM_QOS_NETWORK_LAT_DEFAULT_VALUE,
	.default_value = PM_QOS_NETWORK_LAT_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_NETWORK_LAT_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MIN,
	.notifiers = &network_lat_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(network_lat),
	.mlock = __MUTEX_INITIALIZER(network_lat_constraints.mlock),
};
static struct exynos_pm_qos_object network_lat_pm_qos = {
	.constraints = &network_lat_constraints,
	.name = "network_latency",
};

static BLOCKING_NOTIFIER_HEAD(device_throughput_notifier);
static struct exynos_pm_qos_constraints device_tput_constraints = {
	.list = PLIST_HEAD_INIT(device_tput_constraints.list),
	.target_value = PM_QOS_DEVICE_THROUGHPUT_DEFAULT_VALUE,
	.default_value = PM_QOS_DEVICE_THROUGHPUT_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MAX,
	.notifiers = &device_throughput_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(device_throughput),
	.mlock = __MUTEX_INITIALIZER(device_tput_constraints.mlock),
};
static struct exynos_pm_qos_object device_throughput_pm_qos = {
	.constraints = &device_tput_constraints,
	.name = "device_throughput",
};

static BLOCKING_NOTIFIER_HEAD(device_throughput_max_notifier);
static struct exynos_pm_qos_constraints device_tput_max_constraints = {
	.list = PLIST_HEAD_INIT(device_tput_max_constraints.list),
	.target_value = PM_QOS_DEVICE_THROUGHPUT_MAX_DEFAULT_VALUE,
	.default_value = PM_QOS_DEVICE_THROUGHPUT_MAX_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MIN,
	.notifiers = &device_throughput_max_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(device_throughput_max),
	.mlock = __MUTEX_INITIALIZER(device_tput_max_constraints.mlock),
};
static struct exynos_pm_qos_object device_throughput_max_pm_qos = {
	.constraints = &device_tput_max_constraints,
	.name = "device_throughput_max",
};

static BLOCKING_NOTIFIER_HEAD(intcam_throughput_notifier);
static struct exynos_pm_qos_constraints intcam_tput_constraints = {
	.list = PLIST_HEAD_INIT(intcam_tput_constraints.list),
	.target_value = PM_QOS_INTCAM_THROUGHPUT_DEFAULT_VALUE,
	.default_value = PM_QOS_INTCAM_THROUGHPUT_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MAX,
	.notifiers = &intcam_throughput_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(intcam_throughput),
	.mlock = __MUTEX_INITIALIZER(intcam_tput_constraints.mlock),
};
static struct exynos_pm_qos_object intcam_throughput_pm_qos = {
	.constraints = &intcam_tput_constraints,
	.name = "intcam_throughput",
};

static BLOCKING_NOTIFIER_HEAD(intcam_throughput_max_notifier);
static struct exynos_pm_qos_constraints intcam_tput_max_constraints = {
	.list = PLIST_HEAD_INIT(intcam_tput_max_constraints.list),
	.target_value = PM_QOS_INTCAM_THROUGHPUT_MAX_DEFAULT_VALUE,
	.default_value = PM_QOS_INTCAM_THROUGHPUT_MAX_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MIN,
	.notifiers = &intcam_throughput_max_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(intcam_throughput_max),
	.mlock = __MUTEX_INITIALIZER(intcam_tput_max_constraints.mlock),
};
static struct exynos_pm_qos_object intcam_throughput_max_pm_qos = {
	.constraints = &intcam_tput_max_constraints,
	.name = "intcam_throughput_max",
};

static BLOCKING_NOTIFIER_HEAD(bus_throughput_notifier);
static struct exynos_pm_qos_constraints bus_tput_constraints = {
	.list = PLIST_HEAD_INIT(bus_tput_constraints.list),
	.target_value = PM_QOS_BUS_THROUGHPUT_DEFAULT_VALUE,
	.default_value = PM_QOS_BUS_THROUGHPUT_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MAX,
	.notifiers = &bus_throughput_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(bus_throughput),
	.mlock = __MUTEX_INITIALIZER(bus_tput_constraints.mlock),
};
static struct exynos_pm_qos_object bus_throughput_pm_qos = {
	.constraints = &bus_tput_constraints,
	.name = "bus_throughput",
};

static BLOCKING_NOTIFIER_HEAD(bus_throughput_max_notifier);
static struct exynos_pm_qos_constraints bus_tput_max_constraints = {
	.list = PLIST_HEAD_INIT(bus_tput_max_constraints.list),
	.target_value = PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE,
	.default_value = PM_QOS_BUS_THROUGHPUT_MAX_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MIN,
	.notifiers = &bus_throughput_max_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(bus_throughput_max),
	.mlock = __MUTEX_INITIALIZER(bus_tput_max_constraints.mlock),
};
static struct exynos_pm_qos_object bus_throughput_max_pm_qos = {
	.constraints = &bus_tput_max_constraints,
	.name = "bus_throughput_max",
};

static BLOCKING_NOTIFIER_HEAD(cluster2_freq_min_notifier);
static struct exynos_pm_qos_constraints cluster2_freq_min_constraints = {
	.list = PLIST_HEAD_INIT(cluster2_freq_min_constraints.list),
	.target_value = PM_QOS_CLUSTER2_FREQ_MIN_DEFAULT_VALUE,
	.default_value = PM_QOS_CLUSTER2_FREQ_MIN_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MAX,
	.notifiers = &cluster2_freq_min_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(cluster2_freq_min),
	.mlock = __MUTEX_INITIALIZER(cluster2_freq_min_constraints.mlock),
};
static struct exynos_pm_qos_object cluster2_freq_min_pm_qos = {
	.constraints = &cluster2_freq_min_constraints,
	.name = "cluster2_freq_min",
};

static BLOCKING_NOTIFIER_HEAD(cluster2_freq_max_notifier);
static struct exynos_pm_qos_constraints cluster2_freq_max_constraints = {
	.list = PLIST_HEAD_INIT(cluster2_freq_max_constraints.list),
	.target_value = PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE,
	.default_value = PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MIN,
	.notifiers = &cluster2_freq_max_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(cluster2_freq_max),
	.mlock = __MUTEX_INITIALIZER(cluster2_freq_max_constraints.mlock),
};
static struct exynos_pm_qos_object cluster2_freq_max_pm_qos = {
	.constraints = &cluster2_freq_max_constraints,
	.name = "cluster2_freq_max",
};

static BLOCKING_NOTIFIER_HEAD(cluster1_freq_min_notifier);
static struct exynos_pm_qos_constraints cluster1_freq_min_constraints = {
	.list = PLIST_HEAD_INIT(cluster1_freq_min_constraints.list),
	.target_value = PM_QOS_CLUSTER1_FREQ_MIN_DEFAULT_VALUE,
	.default_value = PM_QOS_CLUSTER1_FREQ_MIN_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MAX,
	.notifiers = &cluster1_freq_min_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(cluster1_freq_min),
	.mlock = __MUTEX_INITIALIZER(cluster1_freq_min_constraints.mlock),
};
static struct exynos_pm_qos_object cluster1_freq_min_pm_qos = {
	.constraints = &cluster1_freq_min_constraints,
	.name = "cluster1_freq_min",
};

static BLOCKING_NOTIFIER_HEAD(cluster1_freq_max_notifier);
static struct exynos_pm_qos_constraints cluster1_freq_max_constraints = {
	.list = PLIST_HEAD_INIT(cluster1_freq_max_constraints.list),
	.target_value = PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE,
	.default_value = PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MIN,
	.notifiers = &cluster1_freq_max_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(cluster1_freq_max),
	.mlock = __MUTEX_INITIALIZER(cluster1_freq_max_constraints.mlock),
};
static struct exynos_pm_qos_object cluster1_freq_max_pm_qos = {
	.constraints = &cluster1_freq_max_constraints,
	.name = "cluster1_freq_max",
};

static BLOCKING_NOTIFIER_HEAD(cluster0_freq_min_notifier);
static struct exynos_pm_qos_constraints cluster0_freq_min_constraints = {
	.list = PLIST_HEAD_INIT(cluster0_freq_min_constraints.list),
	.target_value = PM_QOS_CLUSTER0_FREQ_MIN_DEFAULT_VALUE,
	.default_value = PM_QOS_CLUSTER0_FREQ_MIN_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MAX,
	.notifiers = &cluster0_freq_min_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(cluster0_freq_min),
	.mlock = __MUTEX_INITIALIZER(cluster0_freq_min_constraints.mlock),
};
static struct exynos_pm_qos_object cluster0_freq_min_pm_qos = {
	.constraints = &cluster0_freq_min_constraints,
	.name = "cluster0_freq_min",
};

static BLOCKING_NOTIFIER_HEAD(cluster0_freq_max_notifier);
static struct exynos_pm_qos_constraints cluster0_freq_max_constraints = {
	.list = PLIST_HEAD_INIT(cluster0_freq_max_constraints.list),
	.target_value = PM_QOS_CLUSTER0_FREQ_MAX_DEFAULT_VALUE,
	.default_value = PM_QOS_CLUSTER0_FREQ_MAX_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MIN,
	.notifiers = &cluster0_freq_max_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(cluster0_freq_max),
	.mlock = __MUTEX_INITIALIZER(cluster0_freq_max_constraints.mlock),
};
static struct exynos_pm_qos_object cluster0_freq_max_pm_qos = {
	.constraints = &cluster0_freq_max_constraints,
	.name = "cluster0_freq_max",
};

static BLOCKING_NOTIFIER_HEAD(cpu_online_min_notifier);
static struct exynos_pm_qos_constraints cpu_online_min_constraints = {
	.list = PLIST_HEAD_INIT(cpu_online_min_constraints.list),
	.target_value = PM_QOS_CPU_ONLINE_MIN_DEFAULT_VALUE,
	.default_value = PM_QOS_CPU_ONLINE_MIN_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MAX,
	.notifiers = &cpu_online_min_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(cpu_online_min),
	.mlock = __MUTEX_INITIALIZER(cpu_online_min_constraints.mlock),
};
static struct exynos_pm_qos_object cpu_online_min_pm_qos = {
	.constraints = &cpu_online_min_constraints,
	.name = "cpu_online_min",
};

static BLOCKING_NOTIFIER_HEAD(cpu_online_max_notifier);
static struct exynos_pm_qos_constraints cpu_online_max_constraints = {
	.list = PLIST_HEAD_INIT(cpu_online_max_constraints.list),
	.target_value = PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE,
	.default_value = PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MIN,
	.notifiers = &cpu_online_max_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(cpu_online_max),
	.mlock = __MUTEX_INITIALIZER(cpu_online_max_constraints.mlock),
};
static struct exynos_pm_qos_object cpu_online_max_pm_qos = {
	.constraints = &cpu_online_max_constraints,
	.name = "cpu_online_max",
};

static BLOCKING_NOTIFIER_HEAD(display_throughput_notifier);
static struct exynos_pm_qos_constraints display_tput_constraints = {
	.list = PLIST_HEAD_INIT(display_tput_constraints.list),
	.target_value = PM_QOS_DISPLAY_THROUGHPUT_DEFAULT_VALUE,
	.default_value = PM_QOS_DISPLAY_THROUGHPUT_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MAX,
	.notifiers = &display_throughput_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(display_throughput),
	.mlock = __MUTEX_INITIALIZER(display_tput_constraints.mlock),
};
static struct exynos_pm_qos_object display_throughput_pm_qos = {
	.constraints = &display_tput_constraints,
	.name = "display_throughput",
};

static BLOCKING_NOTIFIER_HEAD(display_throughput_max_notifier);
static struct exynos_pm_qos_constraints display_tput_max_constraints = {
	.list = PLIST_HEAD_INIT(display_tput_max_constraints.list),
	.target_value = PM_QOS_DISPLAY_THROUGHPUT_MAX_DEFAULT_VALUE,
	.default_value = PM_QOS_DISPLAY_THROUGHPUT_MAX_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MIN,
	.notifiers = &display_throughput_max_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(display_throughput_max),
	.mlock = __MUTEX_INITIALIZER(display_tput_max_constraints.mlock),
};
static struct exynos_pm_qos_object display_throughput_max_pm_qos = {
	.constraints = &display_tput_max_constraints,
	.name = "display_throughput_max",
};

static BLOCKING_NOTIFIER_HEAD(cam_throughput_notifier);
static struct exynos_pm_qos_constraints cam_tput_constraints = {
	.list = PLIST_HEAD_INIT(cam_tput_constraints.list),
	.target_value = PM_QOS_CAM_THROUGHPUT_DEFAULT_VALUE,
	.default_value = PM_QOS_CAM_THROUGHPUT_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MAX,
	.notifiers = &cam_throughput_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(cam_throughput),
	.mlock = __MUTEX_INITIALIZER(cam_tput_constraints.mlock),
};
static struct exynos_pm_qos_object cam_throughput_pm_qos = {
	.constraints = &cam_tput_constraints,
	.name = "cam_throughput",
};

static BLOCKING_NOTIFIER_HEAD(aud_throughput_notifier);
static struct exynos_pm_qos_constraints aud_tput_constraints = {
	.list = PLIST_HEAD_INIT(aud_tput_constraints.list),
	.target_value = PM_QOS_AUD_THROUGHPUT_DEFAULT_VALUE,
	.default_value = PM_QOS_AUD_THROUGHPUT_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MAX,
	.notifiers = &aud_throughput_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(aud_throughput),
	.mlock = __MUTEX_INITIALIZER(aud_tput_constraints.mlock),
};
static struct exynos_pm_qos_object aud_throughput_pm_qos = {
	.constraints = &aud_tput_constraints,
	.name = "aud_throughput",
};

static BLOCKING_NOTIFIER_HEAD(cam_throughput_max_notifier);
static struct exynos_pm_qos_constraints cam_tput_max_constraints = {
	.list = PLIST_HEAD_INIT(cam_tput_max_constraints.list),
	.target_value = PM_QOS_CAM_THROUGHPUT_MAX_DEFAULT_VALUE,
	.default_value = PM_QOS_CAM_THROUGHPUT_MAX_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MIN,
	.notifiers = &cam_throughput_max_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(cam_throughput_max),
	.mlock = __MUTEX_INITIALIZER(cam_tput_max_constraints.mlock),
};
static struct exynos_pm_qos_object cam_throughput_max_pm_qos = {
	.constraints = &cam_tput_max_constraints,
	.name = "cam_throughput_max",
};

static BLOCKING_NOTIFIER_HEAD(aud_throughput_max_notifier);
static struct exynos_pm_qos_constraints aud_tput_max_constraints = {
	.list = PLIST_HEAD_INIT(aud_tput_max_constraints.list),
	.target_value = PM_QOS_AUD_THROUGHPUT_MAX_DEFAULT_VALUE,
	.default_value = PM_QOS_AUD_THROUGHPUT_MAX_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MIN,
	.notifiers = &aud_throughput_max_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(aud_throughput_max),
	.mlock = __MUTEX_INITIALIZER(aud_tput_max_constraints.mlock),
};
static struct exynos_pm_qos_object aud_throughput_max_pm_qos = {
	.constraints = &aud_tput_max_constraints,
	.name = "aud_throughput_max",
};

static BLOCKING_NOTIFIER_HEAD(mfc_throughput_notifier);
static struct exynos_pm_qos_constraints mfc_tput_constraints = {
	.list = PLIST_HEAD_INIT(mfc_tput_constraints.list),
	.target_value = PM_QOS_MFC_THROUGHPUT_DEFAULT_VALUE,
	.default_value = PM_QOS_MFC_THROUGHPUT_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MAX,
	.notifiers = &mfc_throughput_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(mfc_throughput),
	.mlock = __MUTEX_INITIALIZER(mfc_tput_constraints.mlock),
};
static struct exynos_pm_qos_object mfc_throughput_pm_qos = {
	.constraints = &mfc_tput_constraints,
	.name = "mfc_throughput",
};

static BLOCKING_NOTIFIER_HEAD(npu_throughput_notifier);
static struct exynos_pm_qos_constraints npu_tput_constraints = {
	.list = PLIST_HEAD_INIT(npu_tput_constraints.list),
	.target_value = PM_QOS_NPU_THROUGHPUT_DEFAULT_VALUE,
	.default_value = PM_QOS_NPU_THROUGHPUT_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MAX,
	.notifiers = &npu_throughput_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(npu_throughput),
	.mlock = __MUTEX_INITIALIZER(npu_tput_constraints.mlock),
};
static struct exynos_pm_qos_object npu_throughput_pm_qos = {
	.constraints = &npu_tput_constraints,
	.name = "npu_throughput",
};

static BLOCKING_NOTIFIER_HEAD(gpu_freq_min_notifier);
static struct exynos_pm_qos_constraints gpu_freq_min_constraints = {
	.list = PLIST_HEAD_INIT(gpu_freq_min_constraints.list),
	.target_value = PM_QOS_GPU_FREQ_MIN_DEFAULT_VALUE,
	.default_value = PM_QOS_GPU_FREQ_MIN_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MAX,
	.notifiers = &gpu_freq_min_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(gpu_freq_min),
	.mlock = __MUTEX_INITIALIZER(gpu_freq_min_constraints.mlock),
};
static struct exynos_pm_qos_object gpu_freq_min_pm_qos = {
	.constraints = &gpu_freq_min_constraints,
	.name = "gpu_freq_min",
};

static BLOCKING_NOTIFIER_HEAD(gpu_freq_max_notifier);
static struct exynos_pm_qos_constraints gpu_freq_max_constraints = {
	.list = PLIST_HEAD_INIT(gpu_freq_max_constraints.list),
	.target_value = PM_QOS_GPU_FREQ_MAX_DEFAULT_VALUE,
	.default_value = PM_QOS_GPU_FREQ_MAX_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MIN,
	.notifiers = &gpu_freq_max_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(gpu_freq_max),
	.mlock = __MUTEX_INITIALIZER(gpu_freq_max_constraints.mlock),
};
static struct exynos_pm_qos_object gpu_freq_max_pm_qos = {
	.constraints = &gpu_freq_max_constraints,
	.name = "gpu_freq_max",
};

static BLOCKING_NOTIFIER_HEAD(mfc_throughput_max_notifier);
static struct exynos_pm_qos_constraints mfc_tput_max_constraints = {
	.list = PLIST_HEAD_INIT(mfc_tput_max_constraints.list),
	.target_value = PM_QOS_MFC_THROUGHPUT_MAX_DEFAULT_VALUE,
	.default_value = PM_QOS_MFC_THROUGHPUT_MAX_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MIN,
	.notifiers = &mfc_throughput_max_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(mfc_throughput_max),
	.mlock = __MUTEX_INITIALIZER(mfc_tput_max_constraints.mlock),
};
static struct exynos_pm_qos_object mfc_throughput_max_pm_qos = {
	.constraints = &mfc_tput_max_constraints,
	.name = "mfc_throughput_max",
};

static BLOCKING_NOTIFIER_HEAD(npu_throughput_max_notifier);
static struct exynos_pm_qos_constraints npu_tput_max_constraints = {
	.list = PLIST_HEAD_INIT(npu_tput_max_constraints.list),
	.target_value = PM_QOS_NPU_THROUGHPUT_MAX_DEFAULT_VALUE,
	.default_value = PM_QOS_NPU_THROUGHPUT_MAX_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MIN,
	.notifiers = &npu_throughput_max_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(npu_throughput_max),
	.mlock = __MUTEX_INITIALIZER(npu_tput_max_constraints.mlock),
};
static struct exynos_pm_qos_object npu_throughput_max_pm_qos = {
	.constraints = &npu_tput_max_constraints,
	.name = "npu_throughput_max",
};

static BLOCKING_NOTIFIER_HEAD(vpc_throughput_notifier);
static struct exynos_pm_qos_constraints vpc_tput_constraints = {
	.list = PLIST_HEAD_INIT(vpc_tput_constraints.list),
	.target_value = PM_QOS_VPC_THROUGHPUT_DEFAULT_VALUE,
	.default_value = PM_QOS_VPC_THROUGHPUT_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MAX,
	.notifiers = &vpc_throughput_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(vpc_throughput),
	.mlock = __MUTEX_INITIALIZER(vpc_tput_constraints.mlock),
};
static struct exynos_pm_qos_object vpc_throughput_pm_qos = {
	.constraints = &vpc_tput_constraints,
	.name = "vpc_throughput",
};

static BLOCKING_NOTIFIER_HEAD(vpc_throughput_max_notifier);
static struct exynos_pm_qos_constraints vpc_tput_max_constraints = {
	.list = PLIST_HEAD_INIT(vpc_tput_max_constraints.list),
	.target_value = PM_QOS_VPC_THROUGHPUT_MAX_DEFAULT_VALUE,
	.default_value = PM_QOS_VPC_THROUGHPUT_MAX_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MIN,
	.notifiers = &vpc_throughput_max_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(vpc_throughpu_max),
	.mlock = __MUTEX_INITIALIZER(vpc_tput_max_constraints.mlock),
};
static struct exynos_pm_qos_object vpc_throughput_max_pm_qos = {
	.constraints = &vpc_tput_max_constraints,
	.name = "vpc_throughput_max",
};

static BLOCKING_NOTIFIER_HEAD(csis_throughput_notifier);
static struct exynos_pm_qos_constraints csis_tput_constraints = {
	.list = PLIST_HEAD_INIT(csis_tput_constraints.list),
	.target_value = PM_QOS_CSIS_THROUGHPUT_DEFAULT_VALUE,
	.default_value = PM_QOS_CSIS_THROUGHPUT_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MAX,
	.notifiers = &csis_throughput_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(csis_throughpu_max),
	.mlock = __MUTEX_INITIALIZER(csis_tput_constraints.mlock),
};
static struct exynos_pm_qos_object csis_throughput_pm_qos = {
	.constraints = &csis_tput_constraints,
	.name = "csis_throughput",
};

static BLOCKING_NOTIFIER_HEAD(csis_throughput_max_notifier);
static struct exynos_pm_qos_constraints csis_tput_max_constraints = {
	.list = PLIST_HEAD_INIT(csis_tput_max_constraints.list),
	.target_value = PM_QOS_CSIS_THROUGHPUT_MAX_DEFAULT_VALUE,
	.default_value = PM_QOS_CSIS_THROUGHPUT_MAX_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MIN,
	.notifiers = &csis_throughput_max_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(csis_throughpu_max),
	.mlock = __MUTEX_INITIALIZER(csis_tput_max_constraints.mlock),
};
static struct exynos_pm_qos_object csis_throughput_max_pm_qos = {
	.constraints = &csis_tput_max_constraints,
	.name = "csis_throughput_max",
};

static BLOCKING_NOTIFIER_HEAD(isp_throughput_notifier);
static struct exynos_pm_qos_constraints isp_tput_constraints = {
	.list = PLIST_HEAD_INIT(isp_tput_constraints.list),
	.target_value = PM_QOS_ISP_THROUGHPUT_DEFAULT_VALUE,
	.default_value = PM_QOS_ISP_THROUGHPUT_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MAX,
	.notifiers = &isp_throughput_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(isp_throughpu_max),
	.mlock = __MUTEX_INITIALIZER(isp_tput_constraints.mlock),
};
static struct exynos_pm_qos_object isp_throughput_pm_qos = {
	.constraints = &isp_tput_constraints,
	.name = "isp_throughput",
};

static BLOCKING_NOTIFIER_HEAD(isp_throughput_max_notifier);
static struct exynos_pm_qos_constraints isp_tput_max_constraints = {
	.list = PLIST_HEAD_INIT(isp_tput_max_constraints.list),
	.target_value = PM_QOS_ISP_THROUGHPUT_MAX_DEFAULT_VALUE,
	.default_value = PM_QOS_ISP_THROUGHPUT_MAX_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MIN,
	.notifiers = &isp_throughput_max_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(isp_throughpu_max),
	.mlock = __MUTEX_INITIALIZER(isp_tput_max_constraints.mlock),
};
static struct exynos_pm_qos_object isp_throughput_max_pm_qos = {
	.constraints = &isp_tput_max_constraints,
	.name = "isp_throughput_max",
};

static BLOCKING_NOTIFIER_HEAD(mfc1_throughput_notifier);
static struct exynos_pm_qos_constraints mfc1_tput_constraints = {
	.list = PLIST_HEAD_INIT(mfc1_tput_constraints.list),
	.target_value = PM_QOS_MFC1_THROUGHPUT_DEFAULT_VALUE,
	.default_value = PM_QOS_MFC1_THROUGHPUT_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MAX,
	.notifiers = &mfc1_throughput_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(mfc1_throughpu_max),
	.mlock = __MUTEX_INITIALIZER(mfc1_tput_constraints.mlock),
};
static struct exynos_pm_qos_object mfc1_throughput_pm_qos = {
	.constraints = &mfc1_tput_constraints,
	.name = "mfc1_throughput",
};

static BLOCKING_NOTIFIER_HEAD(mfc1_throughput_max_notifier);
static struct exynos_pm_qos_constraints mfc1_tput_max_constraints = {
	.list = PLIST_HEAD_INIT(mfc1_tput_max_constraints.list),
	.target_value = PM_QOS_MFC1_THROUGHPUT_MAX_DEFAULT_VALUE,
	.default_value = PM_QOS_MFC1_THROUGHPUT_MAX_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MIN,
	.notifiers = &mfc1_throughput_max_notifier,
	.lock = __SPIN_LOCK_UNLOCKED(mfc1_throughpu_max),
	.mlock = __MUTEX_INITIALIZER(mfc1_tput_max_constraints.mlock),
};
static struct exynos_pm_qos_object mfc1_throughput_max_pm_qos = {
	.constraints = &mfc1_tput_max_constraints,
	.name = "mfc1_throughput_max",
};

#if IS_ENABLED(CONFIG_ARGOS)
static BLOCKING_NOTIFIER_HEAD(network_throughput_notifier);
static struct exynos_pm_qos_constraints network_tput_constraints = {
	.list = PLIST_HEAD_INIT(network_tput_constraints.list),
	.target_value = PM_QOS_NETWORK_THROUGHPUT_DEFAULT_VALUE,
	.default_value = PM_QOS_NETWORK_THROUGHPUT_DEFAULT_VALUE,
	.no_constraint_value = PM_QOS_NETWORK_THROUGHPUT_DEFAULT_VALUE,
	.type = EXYNOS_PM_QOS_MAX,
	.notifiers = &network_throughput_notifier,
};
static struct exynos_pm_qos_object network_throughput_pm_qos = {
	.constraints = &network_tput_constraints,
	.name = "network_throughput",
};
#endif

static struct exynos_pm_qos_object *exynos_pm_qos_array[] = {
	&null_exynos_pm_qos,
	&network_lat_pm_qos,
	&cluster0_freq_min_pm_qos,
	&cluster0_freq_max_pm_qos,
	&cluster1_freq_min_pm_qos,
	&cluster1_freq_max_pm_qos,
	&cluster2_freq_min_pm_qos,
	&cluster2_freq_max_pm_qos,
	&cpu_online_min_pm_qos,
	&cpu_online_max_pm_qos,
	&device_throughput_pm_qos,
	&intcam_throughput_pm_qos,
	&device_throughput_max_pm_qos,
	&intcam_throughput_max_pm_qos,
	&bus_throughput_pm_qos,
	&bus_throughput_max_pm_qos,
	&display_throughput_pm_qos,
	&display_throughput_max_pm_qos,
	&cam_throughput_pm_qos,
	&aud_throughput_pm_qos,
	&cam_throughput_max_pm_qos,
	&aud_throughput_max_pm_qos,
	&mfc_throughput_pm_qos,
	&npu_throughput_pm_qos,
	&mfc_throughput_max_pm_qos,
	&npu_throughput_max_pm_qos,
	&gpu_freq_min_pm_qos,
	&gpu_freq_max_pm_qos,
	&vpc_throughput_pm_qos,
	&vpc_throughput_max_pm_qos,
	&csis_throughput_pm_qos,
	&csis_throughput_max_pm_qos,
	&isp_throughput_pm_qos,
	&isp_throughput_max_pm_qos,
	&mfc1_throughput_pm_qos,
	&mfc1_throughput_max_pm_qos,
#if IS_ENABLED(CONFIG_ARGOS)
	&network_throughput_pm_qos,
#endif
};

static ssize_t exynos_pm_qos_power_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos);
static ssize_t exynos_pm_qos_power_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos);
static int exynos_pm_qos_power_open(struct inode *inode, struct file *filp);
static int exynos_pm_qos_power_release(struct inode *inode, struct file *filp);

static const struct file_operations exynos_pm_qos_power_fops = {
	.write = exynos_pm_qos_power_write,
	.read = exynos_pm_qos_power_read,
	.open = exynos_pm_qos_power_open,
	.release = exynos_pm_qos_power_release,
	.llseek = noop_llseek,
};

/* unlocked internal variant */
static inline int exynos_pm_qos_get_value(struct exynos_pm_qos_constraints *c)
{
	struct plist_node *node;
	int total_value = 0;

	if (plist_head_empty(&c->list))
		return c->no_constraint_value;

	switch (c->type) {
	case EXYNOS_PM_QOS_MIN:
		return plist_first(&c->list)->prio;

	case EXYNOS_PM_QOS_MAX:
		return plist_last(&c->list)->prio;

	case EXYNOS_PM_QOS_SUM:
		plist_for_each(node, &c->list)
			total_value += node->prio;

		return total_value;

	default:
		/* runtime check for not using enum */
		BUG();
		return EXYNOS_PM_QOS_DEFAULT_VALUE;
	}
}

s32 exynos_pm_qos_read_value(struct exynos_pm_qos_constraints *c)
{
	return c->target_value;
}
 /**
  *   * pm_qos_read_req_value - returns requested qos value
  *     * @pm_qos_class: identification of which qos value is requested
  *       * @req: request wanted to find set value
  *         *
  *           * This function returns the requested qos value by sysfs node.
  *             */
int exynos_pm_qos_read_req_value(int pm_qos_class, struct exynos_pm_qos_request *req)
{
	struct plist_node *p;
	unsigned long flags;
	struct exynos_pm_qos_constraints *c = exynos_pm_qos_array[pm_qos_class]->constraints;

	spin_lock_irqsave(&c->lock, flags);

	plist_for_each(p, &c->list) {
		if (req == container_of(p, struct exynos_pm_qos_request, node)) {
			spin_unlock_irqrestore(&c->lock, flags);
			return p->prio;
		}
	}

	spin_unlock_irqrestore(&c->lock, flags);

	return -ENODATA;
}
EXPORT_SYMBOL_GPL(exynos_pm_qos_read_req_value);

static inline void exynos_pm_qos_set_value(struct exynos_pm_qos_constraints *c, s32 value)
{
	c->target_value = value;
}

void show_exynos_pm_qos_data(int index)
{
	struct exynos_pm_qos_constraints *c;
	struct exynos_pm_qos_request *req;
	char *type;
	unsigned long flags;
	int tot_reqs = 0;
	int active_reqs = 0;

	if (index >= EXYNOS_PM_QOS_NUM_CLASSES) {
		pr_err("Bad pm_qos_index: %d\n", index);
		return;
	}

	c = exynos_pm_qos_array[index]->constraints;
	if (IS_ERR_OR_NULL(c)) {
		pr_err("%s: Bad constraints on qos?\n", __func__);
		return;
	}

	pr_info("exynos_pm_qos class name: %s\n", exynos_pm_qos_array[index]->name);

	/* Lock to ensure we have a snapshot */
	spin_lock_irqsave(&c->lock, flags);
	if (plist_head_empty(&c->list)) {
		pr_info("Empty!\n");
		goto out;
	}

	switch (c->type) {
	case EXYNOS_PM_QOS_MIN:
		type = "Minimum";
		break;
	case EXYNOS_PM_QOS_MAX:
		type = "Maximum";
		break;
	case EXYNOS_PM_QOS_SUM:
		type = "Sum";
		break;
	default:
		type = "Unknown";
	}

	plist_for_each_entry(req, &c->list, node) {
		char *state = "Default";

		if ((req->node).prio != c->default_value) {
			active_reqs++;
			state = "Active";
		} else {
			continue;
		}

		tot_reqs++;
		pr_info("%d: %d: %s(%s:%d)\n", tot_reqs,
			   (req->node).prio, state,
			   req->func,
			   req->line);
	}

	pr_info("Type=%s, Value=%d, Requests: active=%d / total=%d\n",
			type, exynos_pm_qos_get_value(c), active_reqs, tot_reqs);
out:
	spin_unlock_irqrestore(&c->lock, flags);
	return;
}
EXPORT_SYMBOL_GPL(show_exynos_pm_qos_data);

static int exynos_pm_qos_debug_show(struct seq_file *s, void *unused)
{
	struct exynos_pm_qos_object *qos = (struct exynos_pm_qos_object *)s->private;
	struct exynos_pm_qos_constraints *c;
	struct exynos_pm_qos_request *req;
	char *type;
	unsigned long flags;
	int tot_reqs = 0;
	int active_reqs = 0;

	if (IS_ERR_OR_NULL(qos)) {
		pr_err("%s: bad qos param!\n", __func__);
		return -EINVAL;
	}
	c = qos->constraints;
	if (IS_ERR_OR_NULL(c)) {
		pr_err("%s: Bad constraints on qos?\n", __func__);
		return -EINVAL;
	}

	/* Lock to ensure we have a snapshot */
	spin_lock_irqsave(&c->lock, flags);
	if (plist_head_empty(&c->list)) {
		seq_puts(s, "Empty!\n");
		goto out;
	}

	switch (c->type) {
	case EXYNOS_PM_QOS_MIN:
		type = "Minimum";
		break;
	case EXYNOS_PM_QOS_MAX:
		type = "Maximum";
		break;
	case EXYNOS_PM_QOS_SUM:
		type = "Sum";
		break;
	default:
		type = "Unknown";
	}

	plist_for_each_entry(req, &c->list, node) {
		char *state = "Default";

		if ((req->node).prio != c->default_value) {
			active_reqs++;
			state = "Active";
		}
		tot_reqs++;
		seq_printf(s, "%d: %d: %s(%s:%d)\n", tot_reqs,
			   (req->node).prio, state,
			   req->func,
			   req->line);
	}

	seq_printf(s, "Type=%s, Value=%d, Requests: active=%d / total=%d\n",
		   type, exynos_pm_qos_get_value(c), active_reqs, tot_reqs);

out:
	spin_unlock_irqrestore(&c->lock, flags);
	return 0;
}

DEFINE_SHOW_ATTRIBUTE(exynos_pm_qos_debug);

/**
 * exynos_pm_qos_update_target - manages the constraints list and calls the notifiers
 *  if needed
 * @c: constraints data struct
 * @node: request to add to the list, to update or to remove
 * @action: action to take on the constraints list
 * @value: value of the request to add or update
 *
 * This function returns 1 if the aggregated constraint value has changed, 0
 *  otherwise.
 */
int exynos_pm_qos_update_target(struct exynos_pm_qos_constraints *c, struct plist_node *node,
			 enum exynos_pm_qos_req_action action, int value)
{
	unsigned long flags;
	int prev_value, curr_value, new_value;
	int ret;

	mutex_lock(&c->mlock);
	spin_lock_irqsave(&c->lock, flags);
	prev_value = exynos_pm_qos_get_value(c);
	if (value == EXYNOS_PM_QOS_DEFAULT_VALUE)
		new_value = c->default_value;
	else
		new_value = value;

	switch (action) {
	case EXYNOS_PM_QOS_REMOVE_REQ:
		exynos_plist_del(node, &c->list);
		break;
	case EXYNOS_PM_QOS_UPDATE_REQ:
		/*
		 * to change the list, we atomically remove, reinit
		 * with new value and add, then see if the extremal
		 * changed
		 */
		exynos_plist_del(node, &c->list);
		/* fall through */
	case EXYNOS_PM_QOS_ADD_REQ:
		plist_node_init(node, new_value);
		exynos_plist_add(node, &c->list);
		break;
	default:
		/* no action */
		;
	}

	curr_value = exynos_pm_qos_get_value(c);
	exynos_pm_qos_set_value(c, curr_value);

	spin_unlock_irqrestore(&c->lock, flags);

	//	trace_pm_qos_update_target((enum pm_qos_req_action)action, prev_value, curr_value);
	if (prev_value != curr_value) {
		ret = 1;
		if (c->notifiers) {
			struct exynos_pm_qos_request *req =
				container_of(node, struct exynos_pm_qos_request, node);

			blocking_notifier_call_chain(c->notifiers,
					(unsigned long)curr_value,
					(void *)&req->exynos_pm_qos_class);
		}
	} else {
		ret = 0;
	}
	mutex_unlock(&c->mlock);
	return ret;
}

/**
 * exynos_pm_qos_request - returns current system wide qos expectation
 * @exynos_pm_qos_class: identification of which qos value is requested
 *
 * This function returns the current target value.
 */
int exynos_pm_qos_request(int exynos_pm_qos_class)
{
	return exynos_pm_qos_read_value(exynos_pm_qos_array[exynos_pm_qos_class]->constraints);
}
EXPORT_SYMBOL_GPL(exynos_pm_qos_request);

int exynos_pm_qos_request_active(struct exynos_pm_qos_request *req)
{
	return req->exynos_pm_qos_class != 0;
}
EXPORT_SYMBOL_GPL(exynos_pm_qos_request_active);

static void __exynos_pm_qos_update_request(struct exynos_pm_qos_request *req,
			   s32 new_value)
{
//	trace_pm_qos_update_request(req->exynos_pm_qos_class, new_value);

	if (new_value != req->node.prio)
		exynos_pm_qos_update_target(
			exynos_pm_qos_array[req->exynos_pm_qos_class]->constraints,
			&req->node, EXYNOS_PM_QOS_UPDATE_REQ, new_value);
}

/**
 * exynos_pm_qos_work_fn - the timeout handler of exynos_pm_qos_update_request_timeout
 * @work: work struct for the delayed work (timeout)
 *
 * This cancels the timeout request by falling back to the default at timeout.
 */
static void exynos_pm_qos_work_fn(struct work_struct *work)
{
	struct exynos_pm_qos_request *req = container_of(to_delayed_work(work),
						  struct exynos_pm_qos_request,
						  work);

	__exynos_pm_qos_update_request(req, EXYNOS_PM_QOS_DEFAULT_VALUE);
}

/**
 * exynos_pm_qos_add_request_trace - inserts new qos request into the list
 * @req: pointer to a preallocated handle
 * @exynos_pm_qos_class: identifies which list of qos request to use
 * @value: defines the qos request
 *
 * This function inserts a new entry in the exynos_pm_qos_class list of requested qos
 * performance characteristics.  It recomputes the aggregate QoS expectations
 * for the exynos_pm_qos_class of parameters and initializes the exynos_pm_qos_request
 * handle.  Caller needs to save this handle for later use in updates and
 * removal.
 */

void exynos_pm_qos_add_request_trace(char *func, unsigned int line,
			struct exynos_pm_qos_request *req, int exynos_pm_qos_class,
			s32 value)
{
	if (!req) /*guard against callers passing in null */
		return;

	if (exynos_pm_qos_request_active(req)) {
		WARN(1, KERN_ERR "exynos_pm_qos_add_request() called for already added request\n");
		return;
	}
	req->exynos_pm_qos_class = exynos_pm_qos_class;
	req->func = func;
	req->line = line;
	INIT_DELAYED_WORK(&req->work, exynos_pm_qos_work_fn);
//	trace_pm_qos_add_request(exynos_pm_qos_class, value);
	exynos_pm_qos_update_target(exynos_pm_qos_array[exynos_pm_qos_class]->constraints,
			     &req->node, EXYNOS_PM_QOS_ADD_REQ, value);
}
EXPORT_SYMBOL_GPL(exynos_pm_qos_add_request_trace);

/**
 * exynos_pm_qos_update_request - modifies an existing qos request
 * @req : handle to list element holding a exynos_pm_qos request to use
 * @value: defines the qos request
 *
 * Updates an existing qos request for the exynos_pm_qos_class of parameters along
 * with updating the target exynos_pm_qos_class value.
 *
 * Attempts are made to make this code callable on hot code paths.
 */
void exynos_pm_qos_update_request(struct exynos_pm_qos_request *req,
			   s32 new_value)
{
	if (!req) /*guard against callers passing in null */
		return;

	if (!exynos_pm_qos_request_active(req)) {
		WARN(1, KERN_ERR "exynos_pm_qos_update_request() called for unknown object\n");
		return;
	}

	cancel_delayed_work_sync(&req->work);
	__exynos_pm_qos_update_request(req, new_value);
}
EXPORT_SYMBOL_GPL(exynos_pm_qos_update_request);

/**
 * exynos_pm_qos_update_request_timeout - modifies an existing qos request temporarily.
 * @req : handle to list element holding a exynos_pm_qos request to use
 * @new_value: defines the temporal qos request
 * @timeout_us: the effective duration of this qos request in usecs.
 *
 * After timeout_us, this qos request is cancelled automatically.
 */
void exynos_pm_qos_update_request_timeout(struct exynos_pm_qos_request *req, s32 new_value,
				   unsigned long timeout_us)
{
	if (!req)
		return;
	if (WARN(!exynos_pm_qos_request_active(req),
		 "%s called for unknown object.", __func__))
		return;

	cancel_delayed_work_sync(&req->work);

//	trace_pm_qos_update_request_timeout(req->exynos_pm_qos_class,
//					    new_value, timeout_us);
	if (new_value != req->node.prio)
		exynos_pm_qos_update_target(
			exynos_pm_qos_array[req->exynos_pm_qos_class]->constraints,
			&req->node, EXYNOS_PM_QOS_UPDATE_REQ, new_value);

	schedule_delayed_work(&req->work, usecs_to_jiffies(timeout_us));
}
EXPORT_SYMBOL_GPL(exynos_pm_qos_update_request_timeout);

/**
 * exynos_pm_qos_remove_request - modifies an existing qos request
 * @req: handle to request list element
 *
 * Will remove pm qos request from the list of constraints and
 * recompute the current target value for the exynos_pm_qos_class.  Call this
 * on slow code paths.
 */
void exynos_pm_qos_remove_request(struct exynos_pm_qos_request *req)
{
	if (!req) /*guard against callers passing in null */
		return;
		/* silent return to keep pcm code cleaner */

	if (!exynos_pm_qos_request_active(req)) {
		WARN(1, KERN_ERR "exynos_pm_qos_remove_request() called for unknown object\n");
		return;
	}

	cancel_delayed_work_sync(&req->work);

//	trace_pm_qos_remove_request(req->exynos_pm_qos_class, EXYNOS_PM_QOS_DEFAULT_VALUE);
	exynos_pm_qos_update_target(exynos_pm_qos_array[req->exynos_pm_qos_class]->constraints,
			     &req->node, EXYNOS_PM_QOS_REMOVE_REQ,
			     EXYNOS_PM_QOS_DEFAULT_VALUE);
	memset(req, 0, sizeof(*req));
}
EXPORT_SYMBOL_GPL(exynos_pm_qos_remove_request);

/* User space interface to PM QoS classes via misc devices */
static int register_pm_qos_misc(struct exynos_pm_qos_object *qos, struct dentry *d)
{
	qos->exynos_pm_qos_power_miscdev.minor = MISC_DYNAMIC_MINOR;
	qos->exynos_pm_qos_power_miscdev.name = qos->name;
	qos->exynos_pm_qos_power_miscdev.fops = &exynos_pm_qos_power_fops;

	debugfs_create_file(qos->name, S_IRUGO, d, (void *)qos,
			    &exynos_pm_qos_debug_fops);

	return misc_register(&qos->exynos_pm_qos_power_miscdev);
}

/**
 * exynos_pm_qos_add_notifier - sets notification entry for changes to target value
 * @exynos_pm_qos_class: identifies which qos target changes should be notified.
 * @notifier: notifier block managed by caller.
 *
 * will register the notifier into a notification chain that gets called
 * upon changes to the exynos_pm_qos_class target value.
 */
int exynos_pm_qos_add_notifier(int exynos_pm_qos_class, struct notifier_block *notifier)
{
	int retval;

	retval = blocking_notifier_chain_register(
			exynos_pm_qos_array[exynos_pm_qos_class]->constraints->notifiers,
			notifier);

	return retval;
}
EXPORT_SYMBOL_GPL(exynos_pm_qos_add_notifier);

/**
 * exynos_pm_qos_remove_notifier - deletes notification entry from chain.
 * @exynos_pm_qos_class: identifies which qos target changes are notified.
 * @notifier: notifier block to be removed.
 *
 * will remove the notifier from the notification chain that gets called
 * upon changes to the exynos_pm_qos_class target value.
 */
int exynos_pm_qos_remove_notifier(int exynos_pm_qos_class, struct notifier_block *notifier)
{
	int retval;

	retval = blocking_notifier_chain_unregister(
			exynos_pm_qos_array[exynos_pm_qos_class]->constraints->notifiers,
			notifier);

	return retval;
}
EXPORT_SYMBOL_GPL(exynos_pm_qos_remove_notifier);

static int find_exynos_pm_qos_object_by_minor(int minor)
{
	int exynos_pm_qos_class;

	for (exynos_pm_qos_class = PM_QOS_NETWORK_LATENCY;
		exynos_pm_qos_class < EXYNOS_PM_QOS_NUM_CLASSES; exynos_pm_qos_class++) {
		if (minor ==
			exynos_pm_qos_array[exynos_pm_qos_class]->exynos_pm_qos_power_miscdev.minor)
			return exynos_pm_qos_class;
	}
	return -1;
}

static int exynos_pm_qos_power_open(struct inode *inode, struct file *filp)
{
	long exynos_pm_qos_class;

	exynos_pm_qos_class = find_exynos_pm_qos_object_by_minor(iminor(inode));
	if (exynos_pm_qos_class >= PM_QOS_NETWORK_LATENCY) {
		struct exynos_pm_qos_request *req = kzalloc(sizeof(*req), GFP_KERNEL);
		if (!req)
			return -ENOMEM;

		exynos_pm_qos_add_request(req, exynos_pm_qos_class, EXYNOS_PM_QOS_DEFAULT_VALUE);
		filp->private_data = req;

		return 0;
	}
	return -EPERM;
}

static int exynos_pm_qos_power_release(struct inode *inode, struct file *filp)
{
	struct exynos_pm_qos_request *req;

	req = filp->private_data;
	exynos_pm_qos_remove_request(req);
	kfree(req);

	return 0;
}


static ssize_t exynos_pm_qos_power_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	s32 value;
	unsigned long flags;
	struct exynos_pm_qos_request *req = filp->private_data;
	struct exynos_pm_qos_constraints *c;

	if (!req)
		return -EINVAL;
	if (!exynos_pm_qos_request_active(req))
		return -EINVAL;

	c = exynos_pm_qos_array[req->exynos_pm_qos_class]->constraints;

	spin_lock_irqsave(&c->lock, flags);
	value = exynos_pm_qos_get_value(c);
	spin_unlock_irqrestore(&c->lock, flags);

	return simple_read_from_buffer(buf, count, f_pos, &value, sizeof(s32));
}

static ssize_t exynos_pm_qos_power_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	s32 value;
	struct exynos_pm_qos_request *req;

	if (count == sizeof(s32)) {
		if (copy_from_user(&value, buf, sizeof(s32)))
			return -EFAULT;
	} else {
		int ret;

		ret = kstrtos32_from_user(buf, count, 16, &value);
		if (ret)
			return ret;
	}

	req = filp->private_data;
	exynos_pm_qos_update_request(req, value);

	return count;
}

#if IS_ENABLED(CONFIG_SEC_PM_LOG)
static int exynos_pm_qos_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_pm_qos_debug_show, PDE_DATA(inode));
}

static const struct file_operations exynos_pm_qos_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= exynos_pm_qos_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void exynos_pm_qos_procfs_init(struct proc_dir_entry *parent)
{
	struct proc_dir_entry *d, *entry;
	int i;

	if (!parent) {
		pr_err("%s: parent is NULL\n", __func__);
		return;
	}

	d = proc_mkdir("current_exynos_pm_qos", parent);

	for (i = PM_QOS_CPU_ONLINE_MIN; i < EXYNOS_PM_QOS_NUM_CLASSES; i++) {
		entry = proc_create_data(exynos_pm_qos_array[i]->name, 0444,
				d, &exynos_pm_qos_proc_fops,
				(void *)exynos_pm_qos_array[i]);
		if (!entry) {
			pr_err("%s: %s: failed to create procfs node\n",
			       __func__, exynos_pm_qos_array[i]->name);
			return;
		}
	}
}
EXPORT_SYMBOL_GPL(exynos_pm_qos_procfs_init);
#endif /* CONFIG_SEC_PM_LOG */

static int exynos_pm_qos_power_init(void)
{
	int ret = 0;
	int i;
	struct dentry *d;

	BUILD_BUG_ON(ARRAY_SIZE(exynos_pm_qos_array) != EXYNOS_PM_QOS_NUM_CLASSES);

	d = debugfs_create_dir("exynos_pm_qos", NULL);

	for (i = PM_QOS_CPU_ONLINE_MIN; i < EXYNOS_PM_QOS_NUM_CLASSES; i++) {
		ret = register_pm_qos_misc(exynos_pm_qos_array[i], d);
		if (ret < 0) {
			pr_err("%s: %s setup failed\n",
			       __func__, exynos_pm_qos_array[i]->name);
			return ret;
		}
	}

	return ret;
}
late_initcall(exynos_pm_qos_power_init);
# define plist_check_head(h)	do { } while (0)
/**
 * plist_add - add @node to @head
 *
 * @node:	&struct plist_node pointer
 * @head:	&struct plist_head pointer
 */
void exynos_plist_add(struct plist_node *node, struct plist_head *head)
{
	struct plist_node *first, *iter, *prev = NULL;
	struct list_head *node_next = &head->node_list;

	plist_check_head(head);
	WARN_ON(!plist_node_empty(node));
	WARN_ON(!list_empty(&node->prio_list));

	if (plist_head_empty(head))
		goto ins_node;

	first = iter = plist_first(head);

	do {
		if (node->prio < iter->prio) {
			node_next = &iter->node_list;
			break;
		}

		prev = iter;
		iter = list_entry(iter->prio_list.next,
				struct plist_node, prio_list);
	} while (iter != first);

	if (!prev || prev->prio != node->prio)
		list_add_tail(&node->prio_list, &iter->prio_list);
ins_node:
	list_add_tail(&node->node_list, node_next);

	plist_check_head(head);
}

/**
 * plist_del - Remove a @node from plist.
 *
 * @node:	&struct plist_node pointer - entry to be removed
 * @head:	&struct plist_head pointer - list head
 */
void exynos_plist_del(struct plist_node *node, struct plist_head *head)
{
	plist_check_head(head);

	if (!list_empty(&node->prio_list)) {
		if (node->node_list.next != &head->node_list) {
			struct plist_node *next;

			next = list_entry(node->node_list.next,
					struct plist_node, node_list);

			/* add the next plist_node into prio_list */
			if (list_empty(&next->prio_list))
				list_add(&next->prio_list, &node->prio_list);
		}
		list_del_init(&node->prio_list);
	}

	list_del_init(&node->node_list);

	plist_check_head(head);
}

MODULE_LICENSE("GPL");
