
#include "npu-scheduler-governor.h"

#define NPU_USERSPACE_NAME	"userspace"

#define NPU_USERSPACE_ATTR(_name)	NPU_SET_ATTR(_name, userspace)

static char *npu_governor_userspace_prop_name[] = {
	"init_freq",
	"init_delay",
	"target_freq",
	"cur_freq",
	"mode_freq",
};

struct npu_governor_userspace_prop {
	u32 init_freq;		/* required for every governor */
	u32 init_delay;		/* required for every governor */
	u32 target_freq;
};

static ssize_t npu_show_attrs_governor_userspace(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t npu_store_attrs_governor_userspace(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count);
static ssize_t npu_store_attrs_governor_userspace_args(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count);

static struct device_attribute npu_userspace_attrs[] = {
	__ATTR(init_freq, 0664,
		npu_show_attrs_governor_userspace,
		npu_store_attrs_governor_userspace),
	__ATTR(init_delay, 0664,
		npu_show_attrs_governor_userspace,
		npu_store_attrs_governor_userspace),
	__ATTR(target_freq, 0664,
		npu_show_attrs_governor_userspace,
		npu_store_attrs_governor_userspace),
	__ATTR(cur_freq, 0664,
		npu_show_attrs_governor_userspace,
		npu_store_attrs_governor_userspace),
	__ATTR(mode_freq, 0664,
		npu_show_attrs_governor_userspace,
		npu_store_attrs_governor_userspace_args),
};

static struct attribute *npu_userspace_sysfs_entries[] = {
	&npu_userspace_attrs[0].attr,
	&npu_userspace_attrs[1].attr,
	&npu_userspace_attrs[2].attr,
	&npu_userspace_attrs[3].attr,
	&npu_userspace_attrs[4].attr,
	NULL,
};

static struct attribute_group npu_userspace_attr_group = {
	.name = "userspace",
	.attrs = npu_userspace_sysfs_entries,
};

enum {
	NPU_USERSPACE_INIT_FREQ = 0,
	NPU_USERSPACE_INIT_DELAY,
	NPU_USERSPACE_TARGET_FREQ,
	NPU_USERSPACE_CUR_FREQ,
	NPU_USERSPACE_MODE_FREQ,
};

static void *npu_governor_userspace_init_prop(
		struct npu_scheduler_dvfs_info *,
		struct device *dev, uint32_t args[]);
static void npu_governor_userspace_start(struct npu_scheduler_dvfs_info *);
static int npu_governor_userspace_target(
		struct npu_scheduler_info *info,
		struct npu_scheduler_dvfs_info *d,
		s32 *freq);
static void npu_governor_userspace_stop(struct npu_scheduler_dvfs_info *);

struct npu_scheduler_governor_ops npu_governor_userspace_ops = {
	.init_prop = npu_governor_userspace_init_prop,
	.start = npu_governor_userspace_start,
	.target = npu_governor_userspace_target,
	.stop = npu_governor_userspace_stop,
};

static struct npu_scheduler_governor npu_gov_userspace = {
	.name = "userspace",
	.ops = &npu_governor_userspace_ops,
	.dev_list = LIST_HEAD_INIT(npu_gov_userspace.dev_list),
};

struct npu_scheduler_dvfs_info *npu_governor_userspace_get_dev(const char *name)
{
	struct npu_scheduler_dvfs_info *d;
	struct npu_scheduler_dvfs_info *t;

	t = NULL;
	list_for_each_entry(d, &npu_gov_userspace.dev_list, dev_list) {
		if (!strcmp(name, d->name)) {
			t = d;
			break;
		}
	}

	return t;
}

ssize_t npu_show_attrs_governor_userspace(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ks, ke, i = 0;
	struct npu_scheduler_dvfs_info *d;
	struct npu_governor_userspace_prop *p;
	const ptrdiff_t offset = attr - npu_userspace_attrs;

	i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
		npu_governor_userspace_prop_name[offset]);

	list_for_each_entry(d, &npu_gov_userspace.dev_list, dev_list)
		i += scnprintf(buf + i, PAGE_SIZE - i, "\t%s", d->name);
	i += scnprintf(buf + i, PAGE_SIZE - i, "\n");

	if (offset == NPU_USERSPACE_MODE_FREQ)
		ke = NPU_PERF_MODE_NUM;
	else
		ke = 1;
	for (ks = 0; ks < ke; ks++) {
		list_for_each_entry(d, &npu_gov_userspace.dev_list, dev_list) {
			p = (struct npu_governor_userspace_prop *)d->gov_prop;
			switch (offset) {
			case NPU_USERSPACE_INIT_FREQ:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->init_freq);
				break;
			case NPU_USERSPACE_INIT_DELAY:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->init_delay);
				break;
			case NPU_USERSPACE_TARGET_FREQ:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->target_freq);
				break;
			case NPU_USERSPACE_CUR_FREQ:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						d->cur_freq);
				break;
			case NPU_USERSPACE_MODE_FREQ:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						d->mode_min_freq[ks]);
				break;
			}
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "\n");
	}

	return i;
}

ssize_t npu_store_attrs_governor_userspace(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret = 0;
	int x = 0;
	char name[256];
	struct npu_scheduler_dvfs_info *d;
	struct npu_governor_userspace_prop *p;
	const ptrdiff_t offset = attr - npu_userspace_attrs;

	if (sscanf(buf, "%255s* %d", name, &x) > 0) {
		d = npu_governor_userspace_get_dev(name);
		if (!d) {
			npu_err("No device : %s %d\n", name, x);
			return -ENODEV;
		}
		p = (struct npu_governor_userspace_prop *)d->gov_prop;

		switch (offset) {
		case NPU_USERSPACE_INIT_DELAY:
			p->init_delay = x;
			break;
		case NPU_USERSPACE_INIT_FREQ:
			p->init_freq = x;
			break;
		case NPU_USERSPACE_TARGET_FREQ:
			p->target_freq = x;
			break;

		default:
			break;
		}
	}

	ret = count;
	return ret;
}

static ssize_t npu_store_attrs_governor_userspace_args(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret = 0;
	int x = 0, y = 0;
	char name[256];
	struct npu_scheduler_dvfs_info *d;
	struct npu_governor_userspace_prop *p;
	const ptrdiff_t offset = attr - npu_userspace_attrs;

	if (sscanf(buf, "%255s* %d %d", name, &x, &y) > 0) {
		d = npu_governor_userspace_get_dev(name);
		if (!d) {
			npu_err("No device : %s %d %d\n", name, x, y);
			return -ENODEV;
		}
		p = (struct npu_governor_userspace_prop *)d->gov_prop;
		switch (offset) {
		case NPU_USERSPACE_MODE_FREQ:
			if (x >= NPU_PERF_MODE_NUM) {
				npu_err("Invalid mode frequency\n");
				return -EINVAL;
			}
			d->mode_min_freq[x] = y;
			break;

		default:
			break;
		}
	}

	ret = count;
	return ret;
}

void *npu_governor_userspace_init_prop(
		struct npu_scheduler_dvfs_info *d,
		struct device *dev, uint32_t args[])
{
	int i;
	struct npu_governor_userspace_prop *p = NULL;

	BUG_ON(!dev);

	p = (struct npu_governor_userspace_prop *)devm_kzalloc(dev,
			sizeof(struct npu_governor_userspace_prop),
			GFP_KERNEL);
	if (!p) {
		npu_err("failed to alloc userspace governor prop\n");
		goto err_exit;
	}

	p->init_freq = args[0];
	p->init_delay = args[1];
	p->target_freq = args[2];

	/* no minlock argument of NPU_PERF_MODE_NORMAL in dt */
	d->mode_min_freq[0] = 0;
	for (i = 1; i < NPU_PERF_MODE_NUM; i++)
		d->mode_min_freq[i] =
			args[i + NPU_SCHEDULER_DVFS_ARG_NUM - 1];

err_exit:
	return (void *)p;
}

void npu_governor_userspace_start(struct npu_scheduler_dvfs_info *d)
{
	struct npu_governor_userspace_prop *p;

	p = (struct npu_governor_userspace_prop *)d->gov_prop;

	exynos_pm_qos_update_request(&d->qos_req_min, p->init_freq);

	d->cur_freq = p->init_freq;
	d->delay = p->init_delay;

	npu_info("%s start done : init freq %d\n", d->name, p->init_freq);
}

int npu_governor_userspace_target(
		struct npu_scheduler_info *info,
		struct npu_scheduler_dvfs_info *d,
		s32 *freq)
{
	struct npu_governor_userspace_prop *p;

	/* get property for governor of the ip */
	p = (struct npu_governor_userspace_prop *)d->gov_prop;

	*freq = p->target_freq;

	//npu_dbg("cur_freq %d, freq %d\n", d->cur_freq, *freq);

	return 0;
}

void npu_governor_userspace_stop(struct npu_scheduler_dvfs_info *d)
{
	struct npu_governor_userspace_prop *p;

	p = (struct npu_governor_userspace_prop *)d->gov_prop;

	exynos_pm_qos_update_request(&d->qos_req_min, 0);

	d->cur_freq = d->min_freq;
	d->delay = 0;

	npu_info("%s stop done\n", d->name);
}

void npu_governor_userspace_register(struct npu_scheduler_info *info)
{
	BUG_ON(!info);

	probe_info("register userspace governor\n");

	/* register governor in list */
	list_add(&npu_gov_userspace.list, &info->gov_list);

	probe_info("creating sysfs group %s\n",
			npu_userspace_attr_group.name);

	if (sysfs_create_group(&info->dev->kobj, &npu_userspace_attr_group))
		probe_err("failed to create sysfs for %s\n",
			npu_userspace_attr_group.name);
}

int npu_governor_userspace_unregister(struct npu_scheduler_info *info)
{
	npu_info("unregister userspace governor\n");

	return 0;
}
