// SPDX-License-Identifier: GPL-2.0
/*
 *  Samsung Block Statistics
 *
 *  Copyright (C) 2021 Manjong Lee
 *  Copyright (C) 2021 Junho Kim
 *  Copyright (C) 2021 Changheun Lee
 */

#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/genhd.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/blkdev.h>
#include <linux/blk_types.h>

#define NAME_LEN 16
#define MAX_PIO_NODE_NUM 10000
#define SORT_PIO_NODE_NUM 100

struct gendisk *internal_disk;

struct accumulated_stats {
	struct timespec uptime;
	unsigned long sectors[3];       /* READ, WRITE, DISCARD */
	unsigned long ios[3];
	unsigned long iot;
};

struct pio_node {
	struct list_head list;
	pid_t tgid;
	char name[NAME_LEN];
	u64 real_start_time;
	unsigned long long bytes[REQ_OP_DISCARD + 1];
};

static struct pio_node others = {
	.list = LIST_HEAD_INIT(others.list),
	.tgid = 99999,
	.name = "others",
	.real_start_time = 9999999,
	.bytes = {0, 0, 0, 0},
};

static DEFINE_SPINLOCK(pio_list_lock);
static DEFINE_SPINLOCK(others_pio_lock);
LIST_HEAD(pio_list);
static int pio_cnt;
static int pio_enabled;
static unsigned int pio_duration_ms = 5000;
static unsigned long pio_timeout;
static struct kmem_cache *pio_cache;

struct accumulated_stats old, new;

static inline void get_monotonic_boottime(struct timespec *ts)
{
        *ts = ktime_to_timespec(ktime_get_boottime());
}

struct gendisk *get_internal_gendisk(void)
{
	dev_t dev;
	struct gendisk *gd;
	struct block_device *bdev;

	if (internal_disk)
		return internal_disk;

	/* Assume that internal storage is not removed */
	dev = blk_lookup_devt("sda", 0);
	if (!dev)
		dev = blk_lookup_devt("mmcblk0", 0);

	bdev = blkdev_get_by_dev(dev, FMODE_WRITE|FMODE_READ, NULL);
	if (IS_ERR(bdev)) {
		pr_err("%s: No device detected.\n", __func__);
		return 0;
	}

	gd = bdev->bd_disk;
	if (IS_ERR(internal_disk)) {
		pr_err("%s: For an unknown reason, gendisk lost.\n", __func__);
		return 0;
	}
	
	return gd;
}

#define UNSIGNED_DIFF(n, o) (((n) >= (o)) ? ((n) - (o)) : ((n) + (0 - (o))))
#define SECTORS2KB(x) ((x) / 2)

static ssize_t diskios_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int ret;
	struct hd_struct *hd;
	long hours;

	if (!internal_disk)
		internal_disk = get_internal_gendisk();

	if (!internal_disk) {
		pr_err("%s: Internal gendisk ptr error.\n", __func__);
		return -1;
	}

	hd = &internal_disk->part0;

	new.ios[STAT_READ] = part_stat_read(hd, ios[STAT_READ]);
	new.ios[STAT_WRITE] = part_stat_read(hd, ios[STAT_WRITE]);
	new.ios[STAT_DISCARD] = part_stat_read(hd, ios[STAT_DISCARD]);
	new.sectors[STAT_READ] = part_stat_read(hd, sectors[STAT_READ]);
	new.sectors[STAT_WRITE] = part_stat_read(hd, sectors[STAT_WRITE]);
	new.sectors[STAT_DISCARD] = part_stat_read(hd, sectors[STAT_DISCARD]);
	new.iot = jiffies_to_msecs(part_stat_read(hd, io_ticks)) / 1000;

	get_monotonic_boottime(&(new.uptime));
	hours = (new.uptime.tv_sec - old.uptime.tv_sec) / 60; 
	hours = (hours + 30) / 60;

	ret = sprintf(buf, "\"ReadC\":\"%lu\",\"ReadKB\":\"%lu\","
			"\"WriteC\":\"%lu\",\"WriteKB\":\"%lu\","
			"\"DiscardC\":\"%lu\",\"DiscardKB\":\"%lu\","
			"\"IOT\":\"%lu\","
			"\"Hours\":\"%ld\"\n",
			UNSIGNED_DIFF(new.ios[STAT_READ], old.ios[STAT_READ]),
			SECTORS2KB(UNSIGNED_DIFF(new.sectors[STAT_READ], old.sectors[STAT_READ])),
			UNSIGNED_DIFF(new.ios[STAT_WRITE], old.ios[STAT_WRITE]),
			SECTORS2KB(UNSIGNED_DIFF(new.sectors[STAT_WRITE], old.sectors[STAT_WRITE])),
			UNSIGNED_DIFF(new.ios[STAT_DISCARD], old.ios[STAT_DISCARD]),
			SECTORS2KB(UNSIGNED_DIFF(new.sectors[STAT_DISCARD], old.sectors[STAT_DISCARD])),
			UNSIGNED_DIFF(new.iot, old.iot),
			hours);

	old.ios[STAT_READ] = new.ios[STAT_READ];
	old.ios[STAT_WRITE] = new.ios[STAT_WRITE];
	old.ios[STAT_DISCARD] = new.ios[STAT_DISCARD];
	old.sectors[STAT_READ] = new.sectors[STAT_READ];
	old.sectors[STAT_WRITE] = new.sectors[STAT_WRITE];
	old.sectors[STAT_DISCARD] = new.sectors[STAT_DISCARD];
	old.uptime = new.uptime;
	old.iot = new.iot;

	return ret;
}

static void add_pio_node(unsigned long size, int op)
{
	struct pio_node *pio = NULL;
	struct task_struct *gleader = current->group_leader;

	if (pio_cnt >= MAX_PIO_NODE_NUM) {
add_others:
		spin_lock(&others_pio_lock);
		others.bytes[op] += size;
		spin_unlock(&others_pio_lock);
		return;
	}

	pio = kmem_cache_alloc(pio_cache, GFP_NOWAIT);
	if (!pio)
		goto add_others;

	INIT_LIST_HEAD(&pio->list);

	pio->tgid = task_tgid_nr(gleader);
	strncpy(pio->name, gleader->comm, NAME_LEN - 1);
	pio->name[NAME_LEN - 1] = '\0';
	pio->real_start_time = gleader->real_start_time;

	pio->bytes[REQ_OP_READ] = 0;
	pio->bytes[REQ_OP_WRITE] = 0;
	pio->bytes[REQ_OP_FLUSH] = 0;
	pio->bytes[REQ_OP_DISCARD] = 0;
	pio->bytes[op] = size;

	spin_lock(&pio_list_lock);
	list_add(&pio->list, &pio_list);
	spin_unlock(&pio_list_lock);

	pio_cnt++;
}

static void free_pio_node(struct list_head *remove_list)
{
	struct list_head *ptr, *ptrn;
	struct pio_node *node;

	list_for_each_safe(ptr, ptrn, remove_list) {
		node = list_entry(ptr, struct pio_node, list);
		list_del(&node->list);
		kmem_cache_free(pio_cache, node);
	}

	spin_lock(&others_pio_lock);
	others.bytes[REQ_OP_READ] = 0;
	others.bytes[REQ_OP_WRITE] = 0;
	others.bytes[REQ_OP_FLUSH] = 0;
	others.bytes[REQ_OP_DISCARD] = 0;
	spin_unlock(&others_pio_lock);

	pio_cnt = 0;
}

void blk_sec_account_process_IO(struct bio *bio)
{
	struct pio_node *node;
	struct list_head *ptr;
	struct task_struct *gleader = current->group_leader;
	int tgid = task_tgid_nr(gleader);
	unsigned long size = 0;
	LIST_HEAD(remove_list);

	if (!bio)
		return;
	if (pio_enabled == 0)
		return;
	if (time_after(jiffies, pio_timeout))
		return;
	if (bio_op(bio) > REQ_OP_DISCARD)
		return;

	size = (bio_op(bio) == REQ_OP_FLUSH) ? 1 : bio->bi_iter.bi_size;

	spin_lock(&pio_list_lock);
	list_for_each(ptr, &pio_list) {
		node = list_entry(ptr, struct pio_node, list);
		if (node->tgid == tgid) {
			if (node->real_start_time != gleader->real_start_time)
				continue;
			node->bytes[bio_op(bio)] += size;
			spin_unlock(&pio_list_lock);
			return;
		}
	}
	spin_unlock(&pio_list_lock);

	add_pio_node(size, bio_op(bio));
}
EXPORT_SYMBOL(blk_sec_account_process_IO);

#define GET_PIO_PRIO(pio) \
	((pio)->bytes[REQ_OP_READ] + (pio)->bytes[REQ_OP_WRITE]*2)

static void sort_pios(struct list_head *remove_list)
{
	struct list_head *tmp;
	struct pio_node *max_node = NULL;
	struct pio_node *node;
	unsigned long long max = 0;
	LIST_HEAD(sorted_list);
	int i;

	for (i = 0; i < SORT_PIO_NODE_NUM; i++) {
		list_for_each(tmp, remove_list) {
			node = list_entry(tmp, struct pio_node, list);
			if (GET_PIO_PRIO(node) > max) {
				max = GET_PIO_PRIO(node);
				max_node = node;
			}
		}
		if (max_node != NULL)
			list_move_tail(&max_node->list, &sorted_list);

		max = 0;
		max_node = NULL;
	}
	list_splice_init(&sorted_list, remove_list);
}

static ssize_t pio_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct list_head *ptr;
	struct pio_node *node;
	int len = 0;
	LIST_HEAD(remove_list);

	spin_lock(&pio_list_lock);
	list_replace_init(&pio_list, &remove_list);
	spin_unlock(&pio_list_lock);

	if (pio_cnt > SORT_PIO_NODE_NUM)
		sort_pios(&remove_list);

	list_for_each(ptr, &remove_list) {
		node = list_entry(ptr, struct pio_node, list);
		if (PAGE_SIZE - len > 80) {
			/* pid read(KB) write(KB) comm printed */
			len += scnprintf(buf + len, PAGE_SIZE - len, "%d %llu %llu %s\n",
					node->tgid, node->bytes[REQ_OP_READ] / 1024,
					node->bytes[REQ_OP_WRITE] / 1024,  node->name);
		} else {
			spin_lock(&others_pio_lock);
			others.bytes[REQ_OP_READ] += node->bytes[REQ_OP_READ];
			others.bytes[REQ_OP_WRITE] += node->bytes[REQ_OP_WRITE];
			others.bytes[REQ_OP_FLUSH] += node->bytes[REQ_OP_FLUSH];
			others.bytes[REQ_OP_DISCARD] += node->bytes[REQ_OP_DISCARD];
			spin_unlock(&others_pio_lock);
		}
	}
	spin_lock(&others_pio_lock);

	if (others.bytes[REQ_OP_READ] + others.bytes[REQ_OP_WRITE])
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d %llu %llu %s\n",
				others.tgid, others.bytes[REQ_OP_READ] / 1024,
				others.bytes[REQ_OP_WRITE] / 1024,  others.name);
	spin_unlock(&others_pio_lock);

	free_pio_node(&remove_list);

	pio_timeout = jiffies + msecs_to_jiffies(pio_duration_ms);

	return len;
}

static ssize_t pio_enabled_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int enable = 0;
	LIST_HEAD(remove_list);

	sscanf(buf, "%d", &enable);
	pio_enabled = (enable >= 1) ? 1 : 0;

	spin_lock(&pio_list_lock);
	list_replace_init(&pio_list, &remove_list);
	spin_unlock(&pio_list_lock);
	free_pio_node(&remove_list);

	pio_timeout = jiffies + msecs_to_jiffies(pio_duration_ms);

	return count;
}

static ssize_t pio_enabled_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len = scnprintf(buf, PAGE_SIZE, "%d\n", pio_enabled);

	return len;
}

static ssize_t pio_duration_ms_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	sscanf(buf, "%d", &pio_duration_ms);

	if (pio_duration_ms > 10000)
		pio_duration_ms = 10000;

	return count;
}

static ssize_t pio_duration_ms_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int len = 0;

	len = scnprintf(buf, PAGE_SIZE, "%u\n", pio_duration_ms);

	return len;
}

static struct kobj_attribute diskios_attr = __ATTR(diskios, 0444, diskios_show,  NULL);
static struct kobj_attribute pios_attr = __ATTR(pios, 0444, pio_show,  NULL);
static struct kobj_attribute pios_enable_attr = __ATTR(pios_enable, 0644,
		pio_enabled_show,  pio_enabled_store);
static struct kobj_attribute pios_duration_ms_attr = __ATTR(pios_duration_ms, 0644,
		pio_duration_ms_show, pio_duration_ms_store);

static struct attribute *blk_sec_stats_attrs[] = {
	&diskios_attr.attr,
	&pios_attr.attr,
	&pios_enable_attr.attr,
	&pios_duration_ms_attr.attr,
	NULL,
};

static struct attribute_group blk_sec_stats_group = {
	.attrs = blk_sec_stats_attrs,
	NULL,
};

static struct kobject *blk_sec_stats_kobj;

static int __init blk_sec_stats_init(void)
{
	int retval;

	blk_sec_stats_kobj = kobject_create_and_add("blk_sec_stats", kernel_kobj);
	if (!blk_sec_stats_kobj)
		return -ENOMEM;

	pio_cache = kmem_cache_create("pio_node", sizeof(struct pio_node), 0, 0, NULL);
	if (!pio_cache)
		return -ENOMEM;

	retval = sysfs_create_group(blk_sec_stats_kobj, &blk_sec_stats_group);
	if (retval)
		kobject_put(blk_sec_stats_kobj);

	return 0;
}

static void __exit blk_sec_stats_exit(void)
{
	kmem_cache_destroy(pio_cache);
	kobject_put(blk_sec_stats_kobj);
}

module_init(blk_sec_stats_init);
module_exit(blk_sec_stats_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Manjong Lee <mj0123.lee@samsung.com>");
MODULE_DESCRIPTION("SEC Storage stats module for various purposes");
MODULE_VERSION("0.1");
