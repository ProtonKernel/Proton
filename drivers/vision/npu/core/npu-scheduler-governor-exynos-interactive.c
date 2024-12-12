
#include "npu-scheduler-governor.h"

#define NPU_EXYNOS_INTERACTIVE_NAME	"exynos-interactive"

#define NPU_EXYNOS_INTERACTIVE_ATTR(_name)	NPU_SET_ATTR(_name, exynos_interactive)

char *npu_governor_exynos_interactive_prop_name[] = {
	"init_freq",
	"init_delay",
	"up_threshold",
	"up_delay",
	"down_threshold",
	"down_delay",
	"hispeed_idle_delay",
	"hispeed_delay",
	"hispeed_freq",
	"hispeed_load",
	"lowspeed_idle_delay",
	"cur_freq",
	"mode_freq",
	"curr_up_delay",
	"curr_down_delay",
};

struct npu_governor_exynos_interactive_prop {
	u32 init_freq;		/* required for every governor */
	u32 init_delay;		/* required for every governor */
	u32 up_threshold;
	u32 up_delay;
	u32 down_threshold;
	u32 down_delay;
	u32 hispeed_idle_delay;
	u32 hispeed_delay;
	u32 hispeed_load;
	u32 lowspeed_idle_delay;

	/* internal temperary variables */
	u32 hispeed_freq;
};

static ssize_t npu_show_attrs_governor_exynos_interactive(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t npu_store_attrs_governor_exynos_interactive(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count);
static ssize_t npu_store_attrs_governor_exynos_interactive_args(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count);

static struct device_attribute npu_exynos_interactive_attrs[] = {
	__ATTR(init_freq, 0664,
		npu_show_attrs_governor_exynos_interactive,
		npu_store_attrs_governor_exynos_interactive),
	__ATTR(init_delay, 0664,
		npu_show_attrs_governor_exynos_interactive,
		npu_store_attrs_governor_exynos_interactive),
	__ATTR(up_threshold, 0664,
		npu_show_attrs_governor_exynos_interactive,
		npu_store_attrs_governor_exynos_interactive),
	__ATTR(up_delay, 0664,
		npu_show_attrs_governor_exynos_interactive,
		npu_store_attrs_governor_exynos_interactive),
	__ATTR(down_threshold, 0664,
		npu_show_attrs_governor_exynos_interactive,
		npu_store_attrs_governor_exynos_interactive),
	__ATTR(down_delay, 0664,
		npu_show_attrs_governor_exynos_interactive,
		npu_store_attrs_governor_exynos_interactive),
	__ATTR(hispeed_idle_delay, 0664,
		npu_show_attrs_governor_exynos_interactive,
		npu_store_attrs_governor_exynos_interactive),
	__ATTR(hispeed_delay, 0664,
		npu_show_attrs_governor_exynos_interactive,
		npu_store_attrs_governor_exynos_interactive),
	__ATTR(hispeed_freq, 0664,
		npu_show_attrs_governor_exynos_interactive,
		npu_store_attrs_governor_exynos_interactive),
	__ATTR(hispeed_load, 0664,
		npu_show_attrs_governor_exynos_interactive,
		npu_store_attrs_governor_exynos_interactive),
	__ATTR(lowspeed_idle_delay, 0664,
		npu_show_attrs_governor_exynos_interactive,
		npu_store_attrs_governor_exynos_interactive),
	__ATTR(cur_freq, 0664,
		npu_show_attrs_governor_exynos_interactive,
		npu_store_attrs_governor_exynos_interactive),
	__ATTR(mode_freq, 0664,
		npu_show_attrs_governor_exynos_interactive,
		npu_store_attrs_governor_exynos_interactive_args),
};

static struct attribute *npu_exynos_interactive_sysfs_entries[] = {
	&npu_exynos_interactive_attrs[0].attr,
	&npu_exynos_interactive_attrs[1].attr,
	&npu_exynos_interactive_attrs[2].attr,
	&npu_exynos_interactive_attrs[3].attr,
	&npu_exynos_interactive_attrs[4].attr,
	&npu_exynos_interactive_attrs[5].attr,
	&npu_exynos_interactive_attrs[6].attr,
	&npu_exynos_interactive_attrs[7].attr,
	&npu_exynos_interactive_attrs[8].attr,
	&npu_exynos_interactive_attrs[9].attr,
	&npu_exynos_interactive_attrs[10].attr,
	&npu_exynos_interactive_attrs[11].attr,
	&npu_exynos_interactive_attrs[12].attr,
	NULL,
};

static struct attribute_group npu_exynos_interactive_attr_group = {
	.name = "exynos-interactive",
	.attrs = npu_exynos_interactive_sysfs_entries,
};

enum {
	NPU_EXYNOS_INTERACTIVE_INIT_FREQ = 0,
	NPU_EXYNOS_INTERACTIVE_INIT_DELAY,
	NPU_EXYNOS_INTERACTIVE_UP_THRESHOLD,
	NPU_EXYNOS_INTERACTIVE_UP_DELAY,
	NPU_EXYNOS_INTERACTIVE_DOWN_THRESHOLD,
	NPU_EXYNOS_INTERACTIVE_DOWN_DELAY,
	NPU_EXYNOS_INTERACTIVE_HISPEED_IDLE_DELAY,
	NPU_EXYNOS_INTERACTIVE_HISPEED_DELAY,
	NPU_EXYNOS_INTERACTIVE_HISPEED_FREQ,
	NPU_EXYNOS_INTERACTIVE_HISPEED_LOAD,
	NPU_EXYNOS_INTERACTIVE_LOWSPEED_IDLE_DELAY,
	NPU_EXYNOS_INTERACTIVE_CUR_FREQ,
	NPU_EXYNOS_INTERACTIVE_MODE_FREQ,
	NPU_EXYNOS_INTERACTIVE_ATTR_NUM,
};

static void *npu_governor_exynos_interactive_init_prop(
		struct npu_scheduler_dvfs_info *,
		struct device *dev, uint32_t args[]);
static void npu_governor_exynos_interactive_start(struct npu_scheduler_dvfs_info *);
static int npu_governor_exynos_interactive_target(
		struct npu_scheduler_info *info,
		struct npu_scheduler_dvfs_info *d,
		s32 *freq);
static void npu_governor_exynos_interactive_stop(struct npu_scheduler_dvfs_info *);

struct npu_scheduler_governor_ops npu_governor_exynos_interactive_ops = {
	.init_prop = npu_governor_exynos_interactive_init_prop,
	.start = npu_governor_exynos_interactive_start,
	.target = npu_governor_exynos_interactive_target,
	.stop = npu_governor_exynos_interactive_stop,
};

static struct npu_scheduler_governor npu_gov_exynos_interactive = {
	.name = "exynos-interactive",
	.ops = &npu_governor_exynos_interactive_ops,
	.dev_list = LIST_HEAD_INIT(npu_gov_exynos_interactive.dev_list),
};

struct npu_scheduler_dvfs_info *npu_governor_exynos_interactive_get_dev(const char *name)
{
	struct npu_scheduler_dvfs_info *d;
	struct npu_scheduler_dvfs_info *t;

	t = NULL;
	list_for_each_entry(d, &npu_gov_exynos_interactive.dev_list, dev_list) {
		if (!strcmp(name, d->name)) {
			t = d;
			break;
		}
	}

	return t;
}

static ssize_t npu_show_attrs_governor_exynos_interactive(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ks, ke, i = 0;
	struct npu_scheduler_dvfs_info *d;
	struct npu_governor_exynos_interactive_prop *p;
	const ptrdiff_t offset = attr - npu_exynos_interactive_attrs;

	i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
		npu_governor_exynos_interactive_prop_name[offset]);

	list_for_each_entry(d, &npu_gov_exynos_interactive.dev_list, dev_list)
		i += scnprintf(buf + i, PAGE_SIZE - i, "\t%s", d->name);
	i += scnprintf(buf + i, PAGE_SIZE - i, "\n");

	if (offset == NPU_EXYNOS_INTERACTIVE_MODE_FREQ)
		ke = NPU_PERF_MODE_NUM;
	else
		ke = 1;
	for (ks = 0; ks < ke; ks++) {
		list_for_each_entry(d, &npu_gov_exynos_interactive.dev_list, dev_list) {
			p = (struct npu_governor_exynos_interactive_prop *)d->gov_prop;
			switch (offset) {
			case NPU_EXYNOS_INTERACTIVE_INIT_FREQ:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->init_freq);
				break;
			case NPU_EXYNOS_INTERACTIVE_INIT_DELAY:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->init_delay);
				break;
			case NPU_EXYNOS_INTERACTIVE_UP_THRESHOLD:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->up_threshold);
				break;
			case NPU_EXYNOS_INTERACTIVE_UP_DELAY:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->up_delay);
				break;
			case NPU_EXYNOS_INTERACTIVE_DOWN_THRESHOLD:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->down_threshold);
				break;
			case NPU_EXYNOS_INTERACTIVE_DOWN_DELAY:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->down_delay);
				break;
			case NPU_EXYNOS_INTERACTIVE_HISPEED_IDLE_DELAY:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->hispeed_idle_delay);
				break;
			case NPU_EXYNOS_INTERACTIVE_HISPEED_DELAY:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->hispeed_delay);
				break;
			case NPU_EXYNOS_INTERACTIVE_HISPEED_FREQ:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->hispeed_freq);
				break;
			case NPU_EXYNOS_INTERACTIVE_HISPEED_LOAD:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->hispeed_load);
				break;
			case NPU_EXYNOS_INTERACTIVE_LOWSPEED_IDLE_DELAY:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						p->lowspeed_idle_delay);
				break;
			case NPU_EXYNOS_INTERACTIVE_CUR_FREQ:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						d->cur_freq);
				break;
			case NPU_EXYNOS_INTERACTIVE_MODE_FREQ:
				i += scnprintf(buf + i, PAGE_SIZE - i, "\t%d",
						d->mode_min_freq[ks]);
				break;
			}
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "\n");
	}

	return i;
}

static ssize_t npu_store_attrs_governor_exynos_interactive(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret = 0;
	int x = 0;
	char name[256];
	struct npu_scheduler_dvfs_info *d;
	struct npu_governor_exynos_interactive_prop *p;
	const ptrdiff_t offset = attr - npu_exynos_interactive_attrs;

	if (sscanf(buf, "%255s* %d", name, &x) > 0) {
		d = npu_governor_exynos_interactive_get_dev(name);
		if (!d) {
			npu_err("No device : %s %d\n", name, x);
			return -ENODEV;
		}
		p = (struct npu_governor_exynos_interactive_prop *)d->gov_prop;
		switch (offset) {
		case NPU_EXYNOS_INTERACTIVE_INIT_DELAY:
			p->init_delay = x;
			break;
		case NPU_EXYNOS_INTERACTIVE_INIT_FREQ:
			p->init_freq = x;
			break;
		case NPU_EXYNOS_INTERACTIVE_UP_THRESHOLD:
			p->up_threshold = x;
			break;
		case NPU_EXYNOS_INTERACTIVE_UP_DELAY:
			p->up_delay = x;
			break;
		case NPU_EXYNOS_INTERACTIVE_DOWN_THRESHOLD:
			p->down_threshold = x;
			break;
		case NPU_EXYNOS_INTERACTIVE_DOWN_DELAY:
			p->down_delay = x;
			break;
		case NPU_EXYNOS_INTERACTIVE_HISPEED_IDLE_DELAY:
			p->hispeed_idle_delay = x;
			break;
		case NPU_EXYNOS_INTERACTIVE_HISPEED_DELAY:
			p->hispeed_delay = x;
			break;
		case NPU_EXYNOS_INTERACTIVE_HISPEED_LOAD:
			p->hispeed_load = x;
			break;
		case NPU_EXYNOS_INTERACTIVE_LOWSPEED_IDLE_DELAY:
			p->lowspeed_idle_delay = x;
			break;

		default:
			break;
		}
	}

	ret = count;
	return ret;
}

static ssize_t npu_store_attrs_governor_exynos_interactive_args(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret = 0;
	int x = 0, y = 0;
	char name[256];
	struct npu_scheduler_dvfs_info *d;
	struct npu_governor_exynos_interactive_prop *p;
	const ptrdiff_t offset = attr - npu_exynos_interactive_attrs;

	if (sscanf(buf, "%255s* %d %d", name, &x, &y) > 0) {
		d = npu_governor_exynos_interactive_get_dev(name);
		if (!d) {
			npu_err("No device : %s %d %d\n", name, x, y);
			return -ENODEV;
		}
		p = (struct npu_governor_exynos_interactive_prop *)d->gov_prop;
		switch (offset) {
		case NPU_EXYNOS_INTERACTIVE_MODE_FREQ:
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

static void *npu_governor_exynos_interactive_init_prop(
		struct npu_scheduler_dvfs_info *d,
		struct device *dev, uint32_t args[])
{
	int i;
	struct npu_governor_exynos_interactive_prop *p = NULL;

	BUG_ON(!dev);

	p = (struct npu_governor_exynos_interactive_prop *)devm_kzalloc(dev,
			sizeof(struct npu_governor_exynos_interactive_prop),
			GFP_KERNEL);
	if (!p) {
		npu_err("failed to alloc exynos-interactive governor prop\n");
		goto err_exit;
	}

	p->init_freq = args[0];
	p->init_delay = args[1];
	p->up_threshold = args[2];
	p->up_delay = args[3];
	p->down_threshold = args[4];
	p->down_delay = args[5];
	p->hispeed_idle_delay = args[6];
	p->hispeed_delay = args[7];
	p->hispeed_load = args[8];
	p->lowspeed_idle_delay = args[9];
	/* no minlock argument of NPU_PERF_MODE_NORMAL in dt */
	d->mode_min_freq[0] = 0;
	for (i = 1; i < NPU_PERF_MODE_NUM; i++)
		d->mode_min_freq[i] =
			args[i + NPU_SCHEDULER_DVFS_ARG_NUM - 1];

	/* internal temperary variables */
	p->hispeed_freq = 0;

err_exit:
	return (void *)p;
}

static void npu_governor_exynos_interactive_start(struct npu_scheduler_dvfs_info *d)
{
	struct npu_governor_exynos_interactive_prop *p;

	p = (struct npu_governor_exynos_interactive_prop *)d->gov_prop;

	/* internal temperary variables */
	p->hispeed_freq = 0;
	d->curr_up_delay = 0;
	d->curr_down_delay = p->down_delay;

	npu_info("%s start done : init freq %d\n", d->name, p->init_freq);
}

static int npu_governor_exynos_interactive_target(
		struct npu_scheduler_info *info,
		struct npu_scheduler_dvfs_info *d,
		s32 *freq)
{
	struct npu_governor_exynos_interactive_prop *p;
	struct dev_pm_opp *opp;
	unsigned long f;
	int no_issue = 1;
	u32 load_idle;

	if ((!info) || (!d)) {
		npu_err("invalid info or dvfs_info\n");
		return -EINVAL;
	}

	/* get property for governor of the ip */
	p = (struct npu_governor_exynos_interactive_prop *)d->gov_prop;

	if (info->load_policy == NPU_SCHEDULER_LOAD_FPS_RQ)
		load_idle = info->rq_load;
	else
		load_idle = info->load;

	if (load_idle) {
		if (info->load == 0) {
			return 0;
		}
	} else {
		if (p->lowspeed_idle_delay &&
				info->load_idle_time > p->lowspeed_idle_delay * 1000) {
			*freq = 0;
			d->is_init_freq = 0;
			return 0;
		}
	}

	d->is_init_freq = 1;

	if (info->load > p->up_threshold * 100) {
		d->curr_up_delay -= (info->time_diff / 1000);
		d->curr_down_delay = p->down_delay;
		//npu_dbg("curr_up_delay %d\n", d->curr_up_delay);
		if (d->curr_up_delay <= 0) {
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
			d->curr_up_delay = p->up_delay;
		}
		no_issue = 0;
	}
	if (info->load < p->down_threshold * 100) {
		d->curr_up_delay = p->up_delay;
		d->curr_down_delay -= (info->time_diff / 1000);
		//npu_dbg("curr_down_delay %d\n", d->curr_down_delay);
		if (d->curr_down_delay <= 0) {
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
			d->curr_down_delay = p->down_delay;
		}
		no_issue = 0;
	}
	if (no_issue) {
		d->curr_up_delay = p->up_delay;
		d->curr_down_delay = p->down_delay;
	}
	//npu_dbg("cur_freq %d, freq %d\n", d->cur_freq, *freq);

	return 0;
}

static void npu_governor_exynos_interactive_stop(struct npu_scheduler_dvfs_info *d)
{
	struct npu_governor_exynos_interactive_prop *p;

	p = (struct npu_governor_exynos_interactive_prop *)d->gov_prop;

	npu_pm_qos_update_request(d, &d->qos_req_min, 0);

	d->cur_freq = d->min_freq;
	d->delay = 0;

	/* internal temperary variables */
	p->hispeed_freq = 0;
	d->curr_up_delay = 0;
	d->curr_down_delay = p->down_delay;

	npu_info("%s stop done\n", d->name);
}

int npu_governor_exynos_interactive_register(struct npu_scheduler_info *info)
{
	int ret = 0;

	BUG_ON(!info);

	probe_info("register simple exynos governor\n");

	/* register governor in list */
	list_add(&npu_gov_exynos_interactive.list, &info->gov_list);

	probe_info("creating sysfs group %s\n",
			npu_exynos_interactive_attr_group.name);


	if (sysfs_create_group(&info->dev->kobj, &npu_exynos_interactive_attr_group)) {
		probe_err("failed to create sysfs for %s\n",
			npu_exynos_interactive_attr_group.name);
		ret = -EINVAL;
	}

	return ret;
}

int npu_governor_exynos_interactive_unregister(struct npu_scheduler_info *info)
{
	npu_info("unregister simple exynos governor\n");

	return 0;
}
