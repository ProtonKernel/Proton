
#ifndef _NPU_SCHEDULER_GOVERNOR_H_
#define _NPU_SCHEDULER_GOVERNOR_H_

#include "npu-scheduler.h"
#include "npu-device.h"


struct npu_scheduler_governor_ops {
	void *(*init_prop)(struct npu_scheduler_dvfs_info *,
			struct device *, uint32_t args[]);
	void (*start)(struct npu_scheduler_dvfs_info *);
	int (*target)(struct npu_scheduler_info *,
		struct npu_scheduler_dvfs_info *, s32 *);
	void (*stop)(struct npu_scheduler_dvfs_info *);
};

struct npu_scheduler_governor {
	char *name;
	struct npu_scheduler_governor_ops *ops;

	struct list_head dev_list;	/* device list */
	struct list_head list;	/* list to scheduler */
};

struct npu_scheduler_governor *npu_scheduler_governor_get(
		struct npu_scheduler_info *info, char *name);
ptrdiff_t npu_scheduler_governor_get_attr_offset(
		struct device_attribute *attr,
		struct npu_scheduler_governor *g,
		struct npu_scheduler_dvfs_info **d);
struct device_attribute *npu_scheduler_governor_init_attrs(
		struct device *dev,
		struct npu_scheduler_dvfs_info *d,
		int attr_num,
		const char *attr_name[],
		ssize_t (*show)(struct device *dev,
			struct device_attribute *attr, char *buf),
		ssize_t (*store)(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count));
int npu_scheduler_governor_register(struct npu_scheduler_info *info);
int npu_scheduler_governor_unregister(struct npu_scheduler_info *info);

/* governors */
#ifdef CONFIG_NPU_GOVERNOR_SIMPLE_EXYNOS
void npu_governor_simple_exynos_register(struct npu_scheduler_info *info);
int npu_governor_simple_exynos_unregister(struct npu_scheduler_info *info);
#endif
#ifdef CONFIG_NPU_GOVERNOR_EXYNOS_INTERACTIVE
int npu_governor_exynos_interactive_register(struct npu_scheduler_info *info);
int npu_governor_exynos_interactive_unregister(struct npu_scheduler_info *info);
#endif
#ifdef CONFIG_NPU_GOVERNOR_USERSPACE
void npu_governor_userspace_register(struct npu_scheduler_info *info);
int npu_governor_userspace_unregister(struct npu_scheduler_info *info);
#endif

#endif
