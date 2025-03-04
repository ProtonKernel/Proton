#ifndef __DEVFREQ_EXYNOS_PPMU_H
#define __DEVFREQ_EXYNOS_PPMU_H

#include <linux/workqueue.h>
#include <linux/mutex.h>

#define IDLE_PROFILER_NAME_LENGTH 16

enum exynos_idle_profiler_type {
	CPU = 0,
	GPU,
	NUM_OF_IDLE_PROFILER_TYPES
};

enum exynos_cpu_idle_type {
	CPU_C1 = 0,
	CPU_C2,
	CPU_ALL,
	NUM_OF_CPU_IDLE_TYPES
};

enum exynos_gpu_idle_type {
	GPU_IDLE = 0,
	GPU_ALL,
	NUM_OF_GPU_IDLE_TYPES
};

struct exynos_idle_profiler_ip_data {
	char name[IDLE_PROFILER_NAME_LENGTH];
	u32 id;
	u64 *counter;
	void __iomem *perf_cnt;
	u32 pa_perf_cnt;
	u32 num_idle_type;
};

struct exynos_idle_profiler_domain_data {
	char name[IDLE_PROFILER_NAME_LENGTH];
	enum exynos_idle_profiler_type type;
	unsigned int num_ip;
	void __iomem *enable;
	void __iomem *clear;
	u32 pa_enable;
	u32 pa_clear;
	u32 num_idle_type;

	struct exynos_idle_profiler_ip_data *ip_data;
};
struct exynos_idle_profiler_data {
	bool enable;
	unsigned int num_domain;
	struct mutex lock;
	struct exynos_idle_profiler_domain_data *domain_data;

	struct delayed_work work;
	u32 work_interval;
};
#endif /* __DEVFREQ_EXYNOS_PPMU_H */
