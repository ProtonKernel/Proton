#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/sched/clock.h>
#include <linux/suspend.h>
#include <linux/of.h>

#include "exynos-idle-profiler.h"

static struct exynos_idle_profiler_data idle_profiler;

static int exynos_idle_profiler_domain_start(struct exynos_idle_profiler_domain_data *domain)
{
	u32 mask = (1 << domain->num_ip) - 1;
	u32 val;

	/* start logging */
	val = __raw_readl(domain->enable);
	val |= mask;
	__raw_writel(val, domain->enable);

	return 0;
}

static int exynos_idle_profiler_domain_stop(struct exynos_idle_profiler_domain_data *domain)
{
	u32 mask = (1 << domain->num_ip) - 1;
	u32 val;

	/* Stop logging */
	val = __raw_readl(domain->enable);
	val &= ~mask;
	__raw_writel(val, domain->enable);

	return 0;
}

static int exynos_idle_profiler_domain_clear(struct exynos_idle_profiler_domain_data *domain)
{
	u32 mask = (1 << domain->num_ip) - 1;

	/* clear domain data */
	__raw_writel(mask, domain->clear);
	__raw_writel(0, domain->clear);

	return 0;
}

static int exynos_idle_profiler_ip_update(struct exynos_idle_profiler_ip_data *ip)
{
	u32 i;

	for (i = 0; i < ip->num_idle_type; i++)
		ip->counter[i] += __raw_readl((u32*)ip->perf_cnt+i);

	return 0;
}

static int exynos_idle_profiler_enable(bool enable)
{
	int i, j, k;

	mutex_lock(&idle_profiler.lock);

	/* if enable is true -> enable */
	if (enable == true && idle_profiler.enable == false) {
		/* Stop & Clear all domains */
		for (i = 0; i < idle_profiler.num_domain; i++) {
			struct exynos_idle_profiler_domain_data *domain = &idle_profiler.domain_data[i];

			exynos_idle_profiler_domain_stop(domain);
			exynos_idle_profiler_domain_clear(domain);

			for (j = 0; j < domain->num_ip; j++)
				for (k = 0; k < domain->num_idle_type; k++)
					domain->ip_data[j].counter[k] = 0;
		}

		/* Start all domains */
		for (i = 0; i < idle_profiler.num_domain; i++) {
			struct exynos_idle_profiler_domain_data *domain = &idle_profiler.domain_data[i];

			exynos_idle_profiler_domain_start(domain);
		}
		/* Start delayed worker */
		mod_delayed_work(system_wq, &idle_profiler.work, msecs_to_jiffies(idle_profiler.work_interval));

	}
	/* else enable is false -> disable */
	else if (enable == false && idle_profiler.enable == true) {
		for (i = 0; i < idle_profiler.num_domain; i++) {
			struct exynos_idle_profiler_domain_data *domain = &idle_profiler.domain_data[i];

			exynos_idle_profiler_domain_stop(domain);
		}

		cancel_delayed_work_sync(&idle_profiler.work);
	}

	idle_profiler.enable = enable;

	mutex_unlock(&idle_profiler.lock);

	return 0;
}

static void __exynos_idle_profiler_update(void)
{
	int i, j;

	/* For each domain */
	for (i = 0; i < idle_profiler.num_domain; i++) {
		struct exynos_idle_profiler_domain_data *domain = &idle_profiler.domain_data[i];

		/* Stop Domain */
		exynos_idle_profiler_domain_stop(domain);

		/* For each ip */
		for (j = 0; j < domain->num_ip; j++) {
			struct exynos_idle_profiler_ip_data *ip = &domain->ip_data[j];

			/* Gather IP counter */
			exynos_idle_profiler_ip_update(ip);
		}

		/* Clear Domain */
		exynos_idle_profiler_domain_clear(domain);

		/* Start Domain */
		exynos_idle_profiler_domain_start(domain);
	}
}

static int exynos_idle_profiler_update(void)
{
	int ret = 0;

	mutex_lock(&idle_profiler.lock);

	if (idle_profiler.enable == false) {
		pr_info("%s: exynos idle profiler does not enabled!\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	cancel_delayed_work_sync(&idle_profiler.work);
	__exynos_idle_profiler_update();
	mod_delayed_work(system_wq, &idle_profiler.work, msecs_to_jiffies(idle_profiler.work_interval));

out:
	mutex_unlock(&idle_profiler.lock);

	return ret;
}

static void exynos_idle_profiler_work(struct work_struct *work)
{
	mutex_lock(&idle_profiler.lock);

	__exynos_idle_profiler_update();
	/* Start delayed worker */
	mod_delayed_work(system_wq, &idle_profiler.work, msecs_to_jiffies(idle_profiler.work_interval));

	mutex_unlock(&idle_profiler.lock);
}

static ssize_t show_exynos_idle_profiler_get_profile(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	u32 i, j, k;
	ssize_t count = 0;
	int ret = 0;

	ret = exynos_idle_profiler_update();
	if (ret != 0) {
		count += snprintf(buf + count, PAGE_SIZE, "Exynos Idle Profiler is not working\n");
		return count;

	}

	for (i = 0; i < idle_profiler.num_domain; i++) {
		struct exynos_idle_profiler_domain_data *domain = &idle_profiler.domain_data[i];

		switch (domain->type) {
			case CPU:
				count += snprintf(buf + count, PAGE_SIZE, "ip_name\tC1_Cycle\tC2_Cycle\tTotal_Cycle\n");
				break;
			case GPU:
				count += snprintf(buf + count, PAGE_SIZE, "ip_name\tIdle_Cycle\tTotal_Cycle\n");
				break;
			default:
				break;
		}
		for (j = 0; j < domain->num_ip; j++) {
			struct exynos_idle_profiler_ip_data *ip = &domain->ip_data[j];
			count += snprintf(buf + count, PAGE_SIZE, "%s\t", ip->name);

			for (k = 0; k < ip->num_idle_type; k++)
				count += snprintf(buf + count, PAGE_SIZE, "%llu\t", ip->counter[k]);
			count += snprintf(buf + count, PAGE_SIZE, "\n", ip->name);
		}
	}

	return count;
}

static ssize_t store_exynos_idle_profiler_get_profile(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int ret;
	u32 enable;

	ret = sscanf(buf, "%u", &enable);
	if (ret != 1)
		return -EINVAL;

	exynos_idle_profiler_enable(!!enable);

	return count;
}

static ssize_t show_exynos_idle_profiler_info(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	u32 i, j;
	ssize_t count = 0;

	count += snprintf(buf + count, PAGE_SIZE, "Profiler Info >> enable(%d), num_domain(%u)\n", idle_profiler.enable, idle_profiler.num_domain);
	for (i = 0; i < idle_profiler.num_domain; i++) {
		struct exynos_idle_profiler_domain_data *domain = &idle_profiler.domain_data[i];

		count += snprintf(buf + count, PAGE_SIZE, "Domain Info >> name(%s), type(%d), num_ip(%d), num_idle_type(%d), pa_enable(0x%x), pa_clear(0x%x), _va_enable(0x%p), va_clear(0x%p)\n",
				domain->name, domain->type, domain->num_ip, domain->num_idle_type, domain->pa_enable, domain->pa_clear, domain->enable, domain->clear);
		for (j = 0; j < domain->num_ip; j++) {
			struct exynos_idle_profiler_ip_data *ip = &domain->ip_data[j];
			count += snprintf(buf + count, PAGE_SIZE, "Ip Info >> name(%s), pa_perf_cnt(0x%x), va_perf_cnt(0x%p)\n", ip->name, ip->pa_perf_cnt, ip->perf_cnt);
		}
	}

	return count;

}

static DEVICE_ATTR(exynos_idle_profiler_get_profile, 0640, show_exynos_idle_profiler_get_profile, store_exynos_idle_profiler_get_profile);
static DEVICE_ATTR(exynos_idle_profiler_info, 0440, show_exynos_idle_profiler_info, NULL);
static struct attribute *exynos_idle_profiler_sysfs_entries[] = {
	&dev_attr_exynos_idle_profiler_info.attr,
	&dev_attr_exynos_idle_profiler_get_profile.attr,
	NULL,
};

static struct attribute_group exynos_idle_profiler_attr_group = {
	.name = "exynos_idle_profiler",
	.attrs = exynos_idle_profiler_sysfs_entries,
};

/* Idle profiler root probe */
static int exynos_idle_profiler_probe(struct platform_device *pdev)
{
	struct device_node *np, *domain_np;
	unsigned int i = 0;
	int ret;

	np = pdev->dev.of_node;

	/* Do my own jobs to init idle-profiler */

	mutex_init(&idle_profiler.lock);

	/* alloc memory for idle profiler data structure */
	idle_profiler.num_domain = of_get_child_count(np);
	idle_profiler.domain_data = kzalloc(sizeof(struct exynos_idle_profiler_domain_data)
			* idle_profiler.num_domain, GFP_KERNEL);

	if (of_property_read_u32(np, "work_interval", &idle_profiler.work_interval))
		return -ENODEV;

	/* Get num_domains and probe each domain */
	for_each_child_of_node(np, domain_np) {
		struct device_node *ip_np;
		struct exynos_idle_profiler_domain_data *domain = &idle_profiler.domain_data[i];
		const char *name;
		unsigned int j = 0;


		if (!of_property_read_string(domain_np, "domain_name", &name))
			strncpy(domain->name, name, IDLE_PROFILER_NAME_LENGTH);

		/* init enable/clear regs */
		if (of_property_read_u32(domain_np, "enable", &domain->pa_enable))
			return -ENODEV;

		if (of_property_read_u32(domain_np, "clear", &domain->pa_clear))
			return -ENODEV;

		if (of_property_read_u32(domain_np, "type", &domain->type))
			return -ENODEV;

		switch (domain->type) {
			case CPU:
				domain->num_idle_type = NUM_OF_CPU_IDLE_TYPES;
				break;
			case GPU:
				domain->num_idle_type = NUM_OF_GPU_IDLE_TYPES;
				break;
			default:
				break;
		}

		domain->enable = ioremap(domain->pa_enable, 0x4);
		domain->clear = ioremap(domain->pa_clear, 0x4);

		/* Get num_ips and probe each ip */
		domain->num_ip = of_get_child_count(domain_np);
		domain->ip_data = kzalloc(sizeof(struct exynos_idle_profiler_ip_data) * domain->num_ip, GFP_KERNEL);

		for_each_child_of_node(domain_np, ip_np) {
			struct exynos_idle_profiler_ip_data *ip = &domain->ip_data[j];

			if (of_property_read_u32(ip_np, "id", &ip->id))
				return -ENODEV;

			snprintf(ip->name, IDLE_PROFILER_NAME_LENGTH, "%s%d", domain->name, ip->id);

			ip->num_idle_type = domain->num_idle_type;

			if (of_property_read_u32(ip_np, "perf_cnt", &ip->pa_perf_cnt))
				return -ENODEV;

			ip->perf_cnt = ioremap(ip->pa_perf_cnt, 0x4 * ip->num_idle_type);
			ip->counter = kzalloc(sizeof(u64) * ip->num_idle_type, GFP_KERNEL);

			j++;
		}

		i++;
	}

	INIT_DELAYED_WORK(&idle_profiler.work, exynos_idle_profiler_work);

	ret = sysfs_create_group(&pdev->dev.kobj, &exynos_idle_profiler_attr_group);
	if (ret)
		dev_warn(&pdev->dev, "failed create sysfs for devfreq data\n");

	return 0;
}

static const struct of_device_id exynos_idle_profiler_match[] = {
	{
		.compatible = "samsung,exynos-idle-profiler",
	},
	{},
};

static struct platform_driver exynos_idle_profiler_driver = {
	.probe = exynos_idle_profiler_probe,
	.driver = {
		.name = "exynos-idle-profiler",
		.owner = THIS_MODULE,
		.of_match_table = exynos_idle_profiler_match,
	},
};

module_platform_driver(exynos_idle_profiler_driver);
MODULE_AUTHOR("Hanjun Shin <hanjun.shin@samsung.com>");
MODULE_DESCRIPTION("Samsung EXYNOS Soc series idle profiler common driver");
MODULE_LICENSE("GPL");

