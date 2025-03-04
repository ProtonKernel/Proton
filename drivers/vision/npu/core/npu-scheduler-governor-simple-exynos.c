
#include "npu-scheduler-governor.h"

#define NPU_SIMPLE_EXYNOS_NAME	"simple-exynos"

#define NPU_SIMPLE_EXYNOS_ATTR(_name)	NPU_SET_ATTR(_name, simple_exynos)

char *npu_governor_simple_exynos_prop_name[] = {
	"init_freq",
	"init_delay",
	"up_threshold",
	"up_delay",
	"down_threshold",
	"down_delay",
	"cur_freq",
	"mode_freq",
	"curr_up_delay",
	"curr_down_delay",
};

struct npu_governor_simple_exynos_prop {
	u32 init_freq;		/* required for every governor */
	u32 init_delay;		/* required for every governor */
	u32 up_threshold;
	u32 up_delay;
	u32 down_threshold;
	u32 down_delay;

	/* internal temperary variables */
	s32 curr_up_delay;
	s32 curr_down_delay;
};

static ssize_t npu_show_attrs_governor_simple_exynos(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t npu_store_attrs_governor_simple_exynos(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count);
static ssize_t npu_store_attrs_governor_simple_exynos_args(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count);

static struct device_attribute npu_simple_exynos_attrs[] = {
	__ATTR(init_freq, 0664,
		npu_show_attrs_governor_simple_exynos,
		npu_store_attrs_governor_simple_exynos),
	__ATTR(init_delay, 0664,
		npu_show_attrs_governor_simple_exynos,
		npu_store_attrs_governor_simple_exynos),
	__ATTR(up_threshold, 0664,
		npu_show_attrs_governor_simple_exynos,
		npu_store_attrs_governor_simple_exynos),
	__ATTR(up_delay, 0664,
		npu_show_attrs_governor_simple_exynos,
		npu_store_attrs_governor_simple_exynos),
	__ATTR(down_threshold, 0664,
		npu_show_attrs_governor_simple_exynos,
		npu_store_attrs_governor_simple_exynos),
	__ATTR(down_delay, 0664,
		npu_show_attrs_governor_simple_exynos,
		npu_store_attrs_governor_simple_exynos),
	__ATTR(cur_freq, 0664,
			npu_show_attrs_governor_simple_exynos,
			npu_store_attrs_governor_simple_exynos),
	__ATTR(mode_freq, 0664,
		npu_show_attrs_governor_simple_exynos,
		npu_store_attrs_governor_simple_exynos_args),
};

static struct attribute *npu_simple_exynos_sysfs_entries[] = {
	&npu_simple_exynos_attrs[0].attr,
	&npu_simple_exynos_attrs[1].attr,
	&npu_simple_exynos_attrs[2].attr,
	&npu_simple_exynos_attrs[3].attr,
	&npu_simple_exynos_attrs[4].attr,
	&npu_simple_exynos_attrs[5].attr,
	&npu_simple_exynos_attrs[6].attr,
	&npu_simple_exynos_attrs[7].attr,
	NULL,
};

static struct attribute_group npu_simple_exynos_attr_group = {
	.name = "simple-exynos",
	.attrs = npu_simple_exynos_sysfs_entries,
};

enum {
	NPU_SIMPLE_EXYNOS_INIT_FREQ = 0,
	NPU_SIMPLE_EXYNOS_INIT_DELAY,
	NPU_SIMPLE_EXYNOS_UP_THRESHOLD,
	NPU_SIMPLE_EXYNOS_UP_DELAY,
	NPU_SIMPLE_EXYNOS_DOWN_THRESHOLD,
	NPU_SIMPLE_EXYNOS_DOWN_DELAY,
	NPU_SIMPLE_EXYNOS_CUR_FREQ,
	NPU_SIMPLE_EXYNOS_MODE_FREQ,
	NPU_SIMPLE_EXYNOS_ATTR_NUM,
};

static void *npu_governor_simple_exynos_init_prop(
		struct npu_scheduler_dvfs_info *,
		struct device *dev, uint32_t args[]);
static void npu_governor_simple_exynos_start(struct npu_scheduler_dvfs_info *);
static int npu_governor_simple_exynos_target(
		struct npu_scheduler_info *info,
		struct npu_scheduler_dvfs_info *d,
		s32 *freq);
static void npu_governor_simple_exynos_stop(struct npu_scheduler_dvfs_info *);

struct npu_scheduler_governor_ops npu_governor_simple_exynos_ops = {
	.init_prop = npu_governor_simple_exynos_init_prop,
	.start = npu_governor_simple_exynos_start,
	.target = npu_governor_simple_exynos_target,
	.stop = npu_governor_simple_exynos_stop,
};

static struct npu_scheduler_governor npu_gov_simple_exynos = {
	.name = "simple-exynos",
	.ops = &npu_governor_simple_exynos_ops,
	.dev_list = LIST_HEAD_INIT(npu_gov_simple_exynos.dev_list),
};

struct npu_scheduler_dvfs_info *npu_governor_simple_exynos_get_dev(const char *name)
{
	struct npu_scheduler_dvfs_info *d;
	struct npu_scheduler_dvfs_info *t;

	t = NULL;
	list_for_each_entry(d, &npu_gov_simple_exynos.dev_list, dev_list) {
		if (!strcmp(name, d->name)) {
			t = d;
			break;
		}
	}

	return t;
}

static ssize_t npu_show_attrs_governor_simple_exynos(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ks, ke, i = 0;
	struct npu_scheduler_dvfs_info *d;
	struct npu_governor_simple_exynos_prop *p;
	const ptrdiff_t offset = attr - npu_simple_exynos_attrs;

	i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
		npu_governor_simple_exynos_prop_name[offset]);

	list_for_each_entry(d, &npu_gov_simple_exynos.dev_list, dev_list)
		i += scnprintf(buf + i, PAGE_SIZE - i, "\t%s", d->name);
	i += scnprintf(buf + i, PAGE_SIZE - i, "\n");

	if (offset == NPU_SIMPLE_EXYNOS_MODE_FREQ)
		ke = NPU_PERF_MODE_NUM;
	else
		ke = 1;
	for (ks = 0; ks < ke; ks++) {
		list_for_each_entry(d, &npu_gov_simple_exynos.dev_list, dev_list) {
			p = (struct npu_governor_simple_exynos_prop *)d->gov_prop;
			switch (offset) {
			case NPU_SIMPLE_EXYNOS_INIT_FREQ:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->init_freq);
				break;
			case NPU_SIMPLE_EXYNOS_INIT_DELAY:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->init_delay);
				break;
			case NPU_SIMPLE_EXYNOS_UP_THRESHOLD:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->up_threshold);
				break;
			case NPU_SIMPLE_EXYNOS_UP_DELAY:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->up_delay);
				break;
			case NPU_SIMPLE_EXYNOS_DOWN_THRESHOLD:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->down_threshold);
				break;
			case NPU_SIMPLE_EXYNOS_DOWN_DELAY:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->down_delay);
				break;
			case NPU_SIMPLE_EXYNOS_CUR_FREQ:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						d->cur_freq);
				break;
			case NPU_SIMPLE_EXYNOS_MODE_FREQ:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						d->mode_min_freq[ks]);
				break;
			}
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "\n");
	}

	return i;
}

static ssize_t npu_store_attrs_governor_simple_exynos(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret = 0;
	int x = 0;
	char name[256];
	struct npu_scheduler_dvfs_info *d;
	struct npu_governor_simple_exynos_prop *p;
	const ptrdiff_t offset = attr - npu_simple_exynos_attrs;

	if (sscanf(buf, "%255s* %d", name, &x) > 0) {
		d = npu_governor_simple_exynos_get_dev(name);
		if (!d) {
			npu_err("No device : %s %d\n", name, x);
			return -ENODEV;
		}
		p = (struct npu_governor_simple_exynos_prop *)d->gov_prop;
		switch (offset) {
		case NPU_SIMPLE_EXYNOS_INIT_DELAY:
			p->init_delay = x;
			break;
		case NPU_SIMPLE_EXYNOS_INIT_FREQ:
			p->init_freq = x;
			break;
		case NPU_SIMPLE_EXYNOS_UP_THRESHOLD:
			p->up_threshold = x;
			break;
		case NPU_SIMPLE_EXYNOS_UP_DELAY:
			p->up_delay = x;
			break;
		case NPU_SIMPLE_EXYNOS_DOWN_THRESHOLD:
			p->down_threshold = x;
			break;
		case NPU_SIMPLE_EXYNOS_DOWN_DELAY:
			p->down_delay = x;
			break;
		default:
			break;
		}
	}

	ret = count;
	return ret;
}

static ssize_t npu_store_attrs_governor_simple_exynos_args(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret = 0;
	int x = 0, y = 0;
	char name[256];
	struct npu_scheduler_dvfs_info *d;
	struct npu_governor_simple_exynos_prop *p;
	const ptrdiff_t offset = attr - npu_simple_exynos_attrs;

	if (sscanf(buf, "%255s* %d %d", name, &x, &y) > 0) {
		d = npu_governor_simple_exynos_get_dev(name);
		if (!d) {
			npu_err("No device : %s %d %d\n", name, x, y);
			return -ENODEV;
		}
		p = (struct npu_governor_simple_exynos_prop *)d->gov_prop;
		switch (offset) {
		case NPU_SIMPLE_EXYNOS_MODE_FREQ:
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

static void *npu_governor_simple_exynos_init_prop(
		struct npu_scheduler_dvfs_info *d,
		struct device *dev, uint32_t args[])
{
	int i;
	struct npu_governor_simple_exynos_prop *p = NULL;

	BUG_ON(!dev);

	p = (struct npu_governor_simple_exynos_prop *)devm_kzalloc(dev,
			sizeof(struct npu_governor_simple_exynos_prop),
			GFP_KERNEL);
	if (!p) {
		npu_err("failed to alloc simple-exynos governor prop\n");
		goto err_exit;
	}

	p->init_freq = args[0];
	p->init_delay = args[1];
	p->up_threshold = args[2];
	p->up_delay = args[3];
	p->down_threshold = args[4];
	p->down_delay = args[5];
	/* no minlock argument of NPU_PERF_MODE_NORMAL in dt */
	d->mode_min_freq[0] = 0;
	for (i = 1; i < NPU_PERF_MODE_NUM; i++)
		d->mode_min_freq[i] =
			args[i + NPU_SCHEDULER_DVFS_ARG_NUM - 1];

	/* internal temperary variables */
	p->curr_up_delay = 0;
	p->curr_down_delay = 0;

err_exit:
	return (void *)p;
}

static void npu_governor_simple_exynos_start(struct npu_scheduler_dvfs_info *d)
{
	struct npu_governor_simple_exynos_prop *p;

	p = (struct npu_governor_simple_exynos_prop *)d->gov_prop;

	npu_pm_qos_update_request(d, &d->qos_req_min, p->init_freq);

	d->cur_freq = p->init_freq;
	d->delay = p->init_delay;

	/* internal temperary variables */
	p->curr_up_delay = p->up_delay;
	p->curr_down_delay = p->down_delay;

	npu_info("%s start done : init freq %d\n", d->name, p->init_freq);
}

static int npu_governor_simple_exynos_target(
		struct npu_scheduler_info *info,
		struct npu_scheduler_dvfs_info *d,
		s32 *freq)
{
	struct npu_governor_simple_exynos_prop *p;
	struct dev_pm_opp *opp;
	unsigned long f;
	int no_issue = 1;
	u32 load_idle;

	/* get property for governor of the ip */
	p = (struct npu_governor_simple_exynos_prop *)d->gov_prop;

	npu_trace("target : l%d, d%d, lit%llu\n",
			info->load, d->delay, info->load_idle_time);

	if (info->load_policy == NPU_SCHEDULER_LOAD_FPS_RQ)
		load_idle = info->rq_load;
	else
		load_idle = info->load;

	if (info->load > p->up_threshold * 100) {
		p->curr_up_delay -= (info->time_diff / 1000);
		p->curr_down_delay = p->down_delay;
		//npu_dbg("curr_up_delay %d\n", p->curr_up_delay);
		if (p->curr_up_delay <= 0) {
			if (d->cur_freq < d->max_freq) {
				f = d->cur_freq + 1;
				opp = dev_pm_opp_find_freq_ceil(
						&d->dvfs_dev->dev, &f);
				if (IS_ERR(opp))
					*freq = d->cur_freq;
				else {
					dev_pm_opp_put(opp);
					*freq = (s32)f;
				}
				//npu_dbg("freq %d, opp %p\n", *freq, opp);
			}
			p->curr_up_delay = p->up_delay;
		}
		no_issue = 0;
	}
	if (info->load < p->down_threshold * 100) {
		p->curr_up_delay = p->up_delay;
		p->curr_down_delay -= (info->time_diff / 1000);
		//npu_dbg("curr_down_delay %d\n", p->curr_down_delay);
		if (p->curr_down_delay <= 0) {
			if (d->cur_freq > d->min_freq) {
				f = d->cur_freq - 1;
				opp = dev_pm_opp_find_freq_floor(
						&d->dvfs_dev->dev, &f);
				if (IS_ERR(opp))
					*freq = d->cur_freq;
				else {
					dev_pm_opp_put(opp);
					*freq = (s32)f;
				}
				//npu_dbg("freq %d, opp %p\n", *freq, opp);
			}
			p->curr_down_delay = p->down_delay;
		}
		no_issue = 0;
	}
	if (no_issue) {
		p->curr_up_delay = p->up_delay;
		p->curr_down_delay = p->down_delay;
	}
	//npu_dbg("cur_freq %d, freq %d\n", d->cur_freq, *freq);

	return 0;
}

static void npu_governor_simple_exynos_stop(struct npu_scheduler_dvfs_info *d)
{
	struct npu_governor_simple_exynos_prop *p;

	p = (struct npu_governor_simple_exynos_prop *)d->gov_prop;

	npu_pm_qos_update_request(d, &d->qos_req_min, 0);

	d->cur_freq = d->min_freq;
	d->delay = 0;

	/* internal temperary variables */
	p->curr_up_delay = p->up_delay;
	p->curr_down_delay = p->down_delay;

	npu_info("%s stop done\n", d->name);
}

void npu_governor_simple_exynos_register(struct npu_scheduler_info *info)
{
	BUG_ON(!info);

	probe_info("register simple exynos governor\n");

	/* register governor in list */
	list_add(&npu_gov_simple_exynos.list, &info->gov_list);

	probe_info("creating sysfs group %s\n",
			npu_simple_exynos_attr_group.name);

	if (sysfs_create_group(&info->dev->kobj, &npu_simple_exynos_attr_group))
		probe_err("failed to create sysfs for %s\n",
			npu_simple_exynos_attr_group.name);
}

int npu_governor_simple_exynos_unregister(struct npu_scheduler_info *info)
{
	npu_info("unregister simple exynos governor\n");

	return 0;
}
