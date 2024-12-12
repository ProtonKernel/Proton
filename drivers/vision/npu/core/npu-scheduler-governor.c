
#include "npu-scheduler-governor.h"

struct npu_scheduler_governor *npu_scheduler_governor_get(
		struct npu_scheduler_info *info, char *name)
{
	struct npu_scheduler_governor *g;

	list_for_each_entry(g, &info->gov_list, list) {
		if (!strcmp(name, g->name))
			return g;
	}
	return (struct npu_scheduler_governor *)NULL;
}

int npu_scheduler_governor_register(struct npu_scheduler_info *info)
{
	probe_info("register scheduler governor\n");

#ifdef CONFIG_NPU_GOVERNOR_SIMPLE_EXYNOS
	npu_governor_simple_exynos_register(info);
#endif
#ifdef CONFIG_NPU_GOVERNOR_EXYNOS_INTERACTIVE
	npu_governor_exynos_interactive_register(info);
#endif
#ifdef CONFIG_NPU_GOVERNOR_USERSPACE
	npu_governor_userspace_register(info);
#endif
	return 0;
}

int npu_scheduler_governor_unregister(struct npu_scheduler_info *info)
{
	probe_info("unregister scheduler governor\n");

#ifdef CONFIG_NPU_GOVERNOR_SIMPLE_EXYNOS
	npu_governor_simple_exynos_unregister(info);
#endif
#ifdef CONFIG_NPU_GOVERNOR_EXYNOS_INTERACTIVE
	npu_governor_exynos_interactive_unregister(info);
#endif
#ifdef CONFIG_NPU_GOVERNOR_USERSPACE
	npu_governor_userspace_unregister(info);
#endif
	return 0;
}
