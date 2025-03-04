// SPDX-License-Identifier: GPL-2.0
/*
 * dma-buf-trace to track the relationship between dmabuf and process.
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 */

#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/mm_types.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/debugfs.h>
#include <linux/dma-buf.h>
#include <linux/anon_inodes.h>
#include <linux/sched/signal.h>
#include <linux/sched/mm.h>
#include <linux/oom.h>
#include <linux/ratelimit.h>
#include <linux/ion.h>

#include "dma-buf-trace.h"

struct dmabuf_trace_ref {
	struct list_head task_node;
	struct list_head buffer_node;

	struct dmabuf_trace_task *task;
	struct dmabuf_trace_buffer *buffer;
	int refcount;
};

struct dmabuf_trace_task {
	struct list_head node;
	struct list_head ref_list;

	struct task_struct *task;
	struct file *file;
	struct dentry *debug_task;
};

struct dmabuf_trace_buffer {
	struct list_head node;
	struct list_head ref_list;

	struct dma_buf *dmabuf;
	int shared_count;
};

static struct list_head buffer_list = LIST_HEAD_INIT(buffer_list);

/*
 * head_task.node is the head node of all other dmabuf_trace_task.node.
 * At the same time, head_task itself maintains the buffer information allocated
 * by the kernel threads.
 */
static struct dmabuf_trace_task head_task;
static DEFINE_MUTEX(trace_lock);

static inline int dmabuf_trace_skip(struct dma_buf *dmabuf)
{
	return strncmp(dmabuf->exp_name, "ion", 3);
}

#ifdef CONFIG_DEBUG_FS
static struct dentry *dmabuf_trace_debug_root;

static int dmabuf_trace_debug_show(struct seq_file *s, void *unused)
{
	struct dmabuf_trace_task *task = s->private;
	struct dmabuf_trace_ref *ref;
	unsigned long flags = 0;

	mutex_lock(&trace_lock);
	seq_puts(s, "\nDma-buf-trace Objects:\n");
	seq_printf(s, "%12s %12s %16s %12s %10s\n",
		   "ino", "size", "flag", "share", "refcount");

	list_for_each_entry(ref, &task->ref_list, task_node) {
		 /*
		  * The built-in heaps doesn't support get_flags callback currently.
		  * That dmabuf is classified as not graphics but other memtrack type.
		  */
		if (dma_buf_get_flags(ref->buffer->dmabuf, &flags))
			flags = 0;

		seq_printf(s, "%12lu %12zu %#16lx %12zu %10d\n",
			   file_inode(ref->buffer->dmabuf->file)->i_ino,
			   ref->buffer->dmabuf->size, flags,
			   ref->buffer->dmabuf->size /
			   ref->buffer->shared_count,
			   ref->refcount);
	}
	mutex_unlock(&trace_lock);

	return 0;
}

static int dmabuf_trace_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, dmabuf_trace_debug_show, inode->i_private);
}

static const struct file_operations dmabuf_trace_debug_fops = {
	.open = dmabuf_trace_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int dmabuf_trace_create_debugfs(struct dmabuf_trace_task *task,
				       const unsigned char *name)
{
	task->debug_task = debugfs_create_file(name, 0444,
					       dmabuf_trace_debug_root, task,
					       &dmabuf_trace_debug_fops);
	if (IS_ERR(task->debug_task))
		return PTR_ERR(task->debug_task);

	return 0;
}

static void dmabuf_trace_remove_debugfs(struct dmabuf_trace_task *task)
{
	debugfs_remove(task->debug_task);
}

static int dmabuf_trace_init_debugfs(void)
{
	struct dentry *d;

	d = debugfs_create_dir("footprint", dma_buf_debugfs_dir);
	if (IS_ERR(d))
		return PTR_ERR(d);

	dmabuf_trace_debug_root = d;
	/*
	 * PID 1 is actually the pid of the init process. dma-buf trace borrows
	 * pid 1 for the buffers allocated by the kernel threads to provide
	 * full buffer information to Android Memory Tracker.
	 */
	d = debugfs_create_file("0", 0444, dmabuf_trace_debug_root, &head_task,
				&dmabuf_trace_debug_fops);
	if (IS_ERR(d)) {
		pr_err("dma_buf_trace: debugfs: failed to create for pid 0\n");
		debugfs_remove_recursive(dmabuf_trace_debug_root);
		dmabuf_trace_debug_root = NULL;

		return PTR_ERR(d);
	}
	head_task.debug_task = d;

	return 0;
}
#else
#define dmabuf_trace_remove_debugfs(task) do { } while (0)
static int dmabuf_trace_create_debugfs(struct dmabuf_trace_task *task,
				       const unsigned char *name)
{
	return 0;
}
static int dmabuf_trace_init_debugfs(void)
{
	return 0;
}
#endif

static int dmabuf_oom_notifier_fn(struct notifier_block *nb,
				  unsigned long action, void *data)
{
	struct dmabuf_trace_task *task = &head_task;
	struct dmabuf_trace_ref *ref;
	unsigned long flags = 0;
	int count = 0;
	static DEFINE_RATELIMIT_STATE(dmabuf_oom_ratelimit, HZ * 10, 1);

	if (!__ratelimit(&dmabuf_oom_ratelimit))
		return 0;

	if (!mutex_trylock(&trace_lock))
		return 0;

	pr_cont("Dmabuf trace between process and buffers");
	pr_err("(sizeKB(#heapprotid,#heaptype,#bufferflag,#shared_count,#refcount)\n");
	do {
		pr_err("%10s(%4d) :\n",
		       (task->task) ? task->task->comm : "Kernel",
		       (task->task) ? task->task->pid : 0);

		list_for_each_entry(ref, &task->ref_list, task_node) {
			if (dma_buf_get_flags(ref->buffer->dmabuf, &flags))
				flags = 0;

			pr_cont("%8zu(%2lu,%2lu,%#8llx,%2d,%d)", ref->buffer->dmabuf->size / 1024,
				ION_HEAP_MASK(flags >> ION_HEAP_PROTID_SHIFT),
				ION_HEAP_MASK(flags >> ION_HEAP_SHIFT),
				ION_BUFFER_MASK(flags), ref->buffer->shared_count, ref->refcount);

			if (++count % 6 == 0)
				pr_cont("\n");
		}

		task = list_next_entry(task, node);
		pr_err("\n");
	} while (&task->node != &head_task.node);

	mutex_unlock(&trace_lock);

	return 0;
}

static struct notifier_block dmabuf_oom_notifier = {
	.notifier_call = dmabuf_oom_notifier_fn,
};

static void dmabuf_trace_free_ref_force(struct dmabuf_trace_ref *ref)
{
	ref->buffer->shared_count--;

	list_del(&ref->buffer_node);
	list_del(&ref->task_node);

	kfree(ref);
}

static int dmabuf_trace_free_ref(struct dmabuf_trace_ref *ref)
{
	/* The reference has never been registered */
	if (WARN_ON(ref->refcount == 0))
		return -EINVAL;

	if (--ref->refcount == 0)
		dmabuf_trace_free_ref_force(ref);

	return 0;
}

static int dmabuf_trace_task_release(struct inode *inode, struct file *file)
{
	struct dmabuf_trace_task *task = file->private_data;
	struct dmabuf_trace_ref *ref, *tmp;

	if (!(task->task->flags & PF_EXITING)) {
		pr_err("%s: Invalid to close '%d' on process '%s'(%x, %lx)\n",
		       __func__, task->task->pid, task->task->comm,
		       task->task->flags, task->task->state);

		dump_stack();
	}

	put_task_struct(task->task);

	mutex_lock(&trace_lock);

	list_for_each_entry_safe(ref, tmp, &task->ref_list, task_node)
		dmabuf_trace_free_ref_force(ref);

	list_del(&task->node);

	mutex_unlock(&trace_lock);

	dmabuf_trace_remove_debugfs(task);

	kfree(task);

	return 0;
}

static const struct file_operations dmabuf_trace_task_fops = {
	.release = dmabuf_trace_task_release,
};

static struct dmabuf_trace_buffer *dmabuf_trace_get_buffer(
		struct dma_buf *dmabuf)
{
	struct dmabuf_trace_buffer *buffer;

	list_for_each_entry(buffer, &buffer_list, node)
		if (buffer->dmabuf == dmabuf)
			return buffer;

	return NULL;
}

static struct dmabuf_trace_task *dmabuf_trace_get_task_noalloc(void)
{
	struct dmabuf_trace_task *task;

	if (!current->mm && (current->flags & PF_KTHREAD))
		return &head_task;

	/*
	 * init process, pid 1 closes file descriptor after booting.
	 * At that time, the trace buffers of init process are released,
	 * so we use head task to track the buffer of init process instead of
	 * creating dmabuf_trace_task for init process.
	 */
	if (current->group_leader->pid == 1)
		return &head_task;

	list_for_each_entry(task, &head_task.node, node)
		if (task->task == current->group_leader)
			return task;

	return NULL;
}

static struct dmabuf_trace_task *dmabuf_trace_get_task(void)
{
	struct dmabuf_trace_task *task;
	unsigned char name[10];
	int ret, fd;

	task = dmabuf_trace_get_task_noalloc();
	if (task)
		return task;

	task = kzalloc(sizeof(*task), GFP_KERNEL);
	if (!task)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&task->node);
	INIT_LIST_HEAD(&task->ref_list);

	scnprintf(name, 10, "%d", current->group_leader->pid);

	get_task_struct(current->group_leader);

	task->task = current->group_leader;

	ret = dmabuf_trace_create_debugfs(task, name);
	if (ret)
		goto err_debugfs;

	ret = get_unused_fd_flags(O_RDONLY | O_CLOEXEC);
	if (ret < 0)
		goto err_fd;
	fd = ret;

	task->file = anon_inode_getfile(name, &dmabuf_trace_task_fops,
					task, O_RDWR);
	if (IS_ERR(task->file)) {
		ret = PTR_ERR(task->file);

		goto err_inode;
	}

	fd_install(fd, task->file);

	list_add_tail(&task->node, &head_task.node);

	return task;

err_inode:
	put_unused_fd(fd);
err_fd:
	dmabuf_trace_remove_debugfs(task);
err_debugfs:
	put_task_struct(current->group_leader);

	kfree(task);

	pr_err("%s: Failed to get task(err %d)\n", __func__, ret);

	return ERR_PTR(ret);

}

static struct dmabuf_trace_ref *dmabuf_trace_get_ref_noalloc(
		struct dmabuf_trace_buffer *buffer,
		struct dmabuf_trace_task *task)
{
	struct dmabuf_trace_ref *ref;

	list_for_each_entry(ref, &task->ref_list, task_node)
		if (ref->buffer == buffer)
			return ref;

	return NULL;
}

static struct dmabuf_trace_ref *dmabuf_trace_get_ref(
		struct dmabuf_trace_buffer *buffer,
		struct dmabuf_trace_task *task)
{
	struct dmabuf_trace_ref *ref;

	ref = dmabuf_trace_get_ref_noalloc(buffer, task);
	if (ref) {
		ref->refcount++;
		return ref;
	}

	ref = kzalloc(sizeof(*ref), GFP_KERNEL);
	if (!ref)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&ref->buffer_node);
	INIT_LIST_HEAD(&ref->task_node);

	ref->task = task;
	ref->buffer = buffer;
	ref->refcount = 1;

	list_add_tail(&ref->task_node, &task->ref_list);
	list_add_tail(&ref->buffer_node, &buffer->ref_list);

	buffer->shared_count++;

	return ref;
}

/**
 * dmabuf_trace_alloc - get reference after creating dmabuf.
 * @dmabuf : buffer to register reference.
 *
 * This create a ref that has relationship between dmabuf
 * and process that requested allocation, and also create
 * the buffer object to trace.
 */
int dmabuf_trace_alloc(struct dma_buf *dmabuf)
{
	struct dmabuf_trace_buffer *buffer;
	struct dmabuf_trace_task *task;
	struct dmabuf_trace_ref *ref;

	if (dmabuf_trace_skip(dmabuf))
		return 0;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	INIT_LIST_HEAD(&buffer->ref_list);
	buffer->dmabuf = dmabuf;

	mutex_lock(&trace_lock);
	list_add_tail(&buffer->node, &buffer_list);
	mutex_unlock(&trace_lock);

	ref = kzalloc(sizeof(*ref), GFP_KERNEL);
	if (!ref)
		return -ENOMEM;
	ref->buffer = buffer;

	/*
	 * The interim ref created in dmabuf_trace_alloc is just for counting
	 * the buffer references correctly. The interim ref has zero refcount.
	 * If user explicitly registers a ref, it is handled in
	 * dmabuf_trace_track_buffer(). If first dmabuf_trace_track_buffer() is
	 * called in the same task that allocated the buffer, the interim ref
	 * becomes the regular ref that has larget refcount than 0. If the first
	 * call to dmabuf_trace_track_buffer() made in other tasks, the interim
	 * ref is removed and a new regular ref is created and registered
	 * instead of the interim ref.
	 */
	ref->refcount = 0;

	mutex_lock(&trace_lock);

	task = dmabuf_trace_get_task();
	if (IS_ERR(task)) {
		mutex_unlock(&trace_lock);
		kfree(ref);
		return PTR_ERR(task);
	}
	ref->task = task;

	list_add_tail(&ref->task_node, &task->ref_list);
	list_add_tail(&ref->buffer_node, &buffer->ref_list);

	buffer->shared_count++;

	mutex_unlock(&trace_lock);

	return 0;
}

/**
 * dmabuf_trace_free - release references after removing buffer.
 * @dmabuf : buffer to release reference.
 *
 * This remove refs that connected with released dmabuf.
 */
void dmabuf_trace_free(struct dma_buf *dmabuf)
{
	struct dmabuf_trace_buffer *buffer;
	struct dmabuf_trace_ref *ref, *tmp;

	if (dmabuf_trace_skip(dmabuf))
		return;

	mutex_lock(&trace_lock);

	buffer = dmabuf_trace_get_buffer(dmabuf);
	if (!buffer) {
		mutex_unlock(&trace_lock);
		return;
	}

	list_for_each_entry_safe(ref, tmp, &buffer->ref_list, buffer_node)
		dmabuf_trace_free_ref_force(ref);

	list_del(&buffer->node);

	mutex_unlock(&trace_lock);

	kfree(buffer);
}

/**
 * dmabuf_trace_register - create ref between task and buffer.
 * @dmabuf : buffer to register reference.
 *
 * This create ref between current task and buffer.
 */
int dmabuf_trace_track_buffer(struct dma_buf *dmabuf)
{
	struct dmabuf_trace_buffer *buffer;
	struct dmabuf_trace_task *task;
	struct dmabuf_trace_ref *ref;
	int ret = 0;

	if (dmabuf_trace_skip(dmabuf))
		return 0;

	mutex_lock(&trace_lock);
	task = dmabuf_trace_get_task();
	if (IS_ERR(task)) {
		ret = PTR_ERR(task);
		goto err;
	}

	buffer = dmabuf_trace_get_buffer(dmabuf);
	if (!buffer) {
		ret = -ENOENT;
		goto err;
	}

	ref = dmabuf_trace_get_ref(buffer, task);
	if (IS_ERR(ref)) {
		/*
		 * task allocated by dmabuf_trace_get_task() is not freed here.
		 * It is deallocated when the process exits.
		 */
		ret = PTR_ERR(ref);
		goto err;
	}

	ref = list_first_entry(&buffer->ref_list, struct dmabuf_trace_ref,
			       buffer_node);
	/*
	 * If first ref of buffer has zero refcount, it is an interim ref
	 * created in dmabuf_trace_alloc() called by other tasks than current.
	 * The interim ref should be removed after the regular ref created here
	 * is registered.
	 */
	if (ref->refcount == 0)
		dmabuf_trace_free_ref_force(ref);
err:
	mutex_unlock(&trace_lock);

	if (ret)
		pr_err("%s: Failed to trace dmabuf (err %d)\n", __func__, ret);
	return ret;
}

/**
 * dmabuf_trace_unregister - remove ref between task and buffer.
 * @dmabuf : buffer to unregister reference.
 *
 * This remove ref between current task and buffer.
 */
int dmabuf_trace_untrack_buffer(struct dma_buf *dmabuf)
{
	struct dmabuf_trace_buffer *buffer;
	struct dmabuf_trace_task *task;
	struct dmabuf_trace_ref *ref;
	int ret;

	if (dmabuf_trace_skip(dmabuf))
		return 0;

	mutex_lock(&trace_lock);
	task = dmabuf_trace_get_task_noalloc();
	if (!task) {
		ret = -ESRCH;
		goto err_unregister;
	}

	buffer = dmabuf_trace_get_buffer(dmabuf);
	if (!buffer) {
		ret = -ENOENT;
		goto err_unregister;
	}

	ref = dmabuf_trace_get_ref_noalloc(buffer, task);
	if (!ref) {
		ret = -ENOENT;
		goto err_unregister;
	}

	ret = dmabuf_trace_free_ref(ref);

err_unregister:
	if (ret)
		pr_err("%s: Failed to untrace dmabuf(err %d)", __func__, ret);

	mutex_unlock(&trace_lock);

	return ret;
}

struct dmabuf_trace_memory {
	__u32 version;
	__u32 pid;
	__u32 count;
	__u32 type;
	__u32 *flags;
	__u32 *size_in_bytes;
	__u32 reserved[2];
};

#define DMABUF_TRACE_BASE	't'
#define DMABUF_TRACE_IOCTL_GET_MEMORY \
	_IOWR(DMABUF_TRACE_BASE, 0, struct dmabuf_trace_memory)

#ifdef CONFIG_COMPAT
struct compat_dmabuf_trace_memory {
	__u32 version;
	__u32 pid;
	__u32 count;
	__u32 type;
	compat_uptr_t flags;
	compat_uptr_t size_in_bytes;
	__u32 reserved[2];
};
#define DMABUF_TRACE_COMPAT_IOCTL_GET_MEMORY \
	_IOWR(DMABUF_TRACE_BASE, 0, struct compat_dmabuf_trace_memory)
#endif

#define MAX_DMABUF_TRACE_MEMORY 64

static int dmabuf_trace_get_user_data(unsigned int cmd, void __user *arg,
				      unsigned int *pid, unsigned int *count,
				      unsigned int *type, unsigned int flags[])
{
	unsigned int __user *uflags;
	unsigned int ucnt;
	int ret;

#ifdef CONFIG_COMPAT
	if (cmd == DMABUF_TRACE_COMPAT_IOCTL_GET_MEMORY) {
		struct compat_dmabuf_trace_memory __user *udata = arg;
		compat_uptr_t cptr;

		ret = get_user(cptr, &udata->flags);
		ret |= get_user(ucnt, &udata->count);
		ret |= get_user(*pid, &udata->pid);
		ret |= get_user(*type, &udata->type);
		uflags = compat_ptr(cptr);
	} else
#endif
	{
		struct dmabuf_trace_memory __user *udata = arg;

		ret = get_user(uflags, &udata->flags);
		ret |= get_user(ucnt, &udata->count);
		ret |= get_user(*pid, &udata->pid);
		ret |= get_user(*type, &udata->type);
	}

	if (ret) {
		pr_err("%s: failed to read data from user\n", __func__);
		return -EFAULT;
	}

	if ((ucnt < 1) || (ucnt > MAX_DMABUF_TRACE_MEMORY)) {
		pr_err("%s: invalid buffer count %u\n", __func__, ucnt);
		return -EINVAL;
	}

	if (copy_from_user(flags, uflags, sizeof(flags[0]) * ucnt)) {
		pr_err("%s: failed to read %u dma_bufs from user\n",
		       __func__, ucnt);
		return -EFAULT;
	}

	*count = ucnt;

	return 0;
}

static int dmabuf_trace_put_user_data(unsigned int cmd, void __user *arg,
				      unsigned int sizes[], int count)
{
	int __user *usize;
	int ret;

#ifdef CONFIG_COMPAT
	if (cmd == DMABUF_TRACE_COMPAT_IOCTL_GET_MEMORY) {
		struct compat_dmabuf_trace_memory __user *udata = arg;
		compat_uptr_t cptr;

		ret = get_user(cptr, &udata->size_in_bytes);
		usize = compat_ptr(cptr);
	} else
#endif
	{
		struct dmabuf_trace_memory __user *udata = arg;

		ret = get_user(usize, &udata->size_in_bytes);
	}

	if (ret) {
		pr_err("%s: failed to read data from user\n", __func__);
		return -EFAULT;
	}

	if (copy_to_user(usize, sizes, sizeof(sizes[0]) * count)) {
		pr_err("%s: failed to read %u dma_bufs from user\n",
		       __func__, count);
		return -EFAULT;
	}

	return 0;
}

/**
 * Flags to differentiate memory that can already be accounted for in
 * /proc/<pid>/smaps,
 * (Shared_Clean + Shared_Dirty + Private_Clean + Private_Dirty = Size).
 * In general, memory mapped in to a userspace process is accounted unless
 * it was mapped with remap_pfn_range.
 * Exactly one of these should be set.
 */
#define MEMTRACK_FLAG_SMAPS_ACCOUNTED   (1 << 1)
#define MEMTRACK_FLAG_SMAPS_UNACCOUNTED (1 << 2)

/**
 * Flags to differentiate memory shared across multiple processes vs. memory
 * used by a single process.  Only zero or one of these may be set in a record.
 * If none are set, record is assumed to count shared + private memory.
 */
#define MEMTRACK_FLAG_SHARED      (1 << 3)
#define MEMTRACK_FLAG_SHARED_PSS  (1 << 4) /* shared / num_procesess */
#define MEMTRACK_FLAG_PRIVATE     (1 << 5)

/**
 * Flags to differentiate memory taken from the kernel's allocation pool vs.
 * memory that is dedicated to non-kernel allocations, for example a carveout
 * or separate video memory.  Only zero or one of these may be set in a record.
 * If none are set, record is assumed to count system + dedicated memory.
 */
#define MEMTRACK_FLAG_SYSTEM     (1 << 6)
#define MEMTRACK_FLAG_DEDICATED  (1 << 7)

/**
 * Flags to differentiate memory accessible by the CPU in non-secure mode vs.
 * memory that is protected.  Only zero or one of these may be set in a record.
 * If none are set, record is assumed to count secure + nonsecure memory.
 */
#define MEMTRACK_FLAG_NONSECURE  (1 << 8)
#define MEMTRACK_FLAG_SECURE     (1 << 9)

enum memtrack_type {
	MEMTRACK_TYPE_OTHER,
	MEMTRACK_TYPE_GL,
	MEMTRACK_TYPE_GRAPHICS,
	MEMTRACK_TYPE_MULTIMEDIA,
	MEMTRACK_TYPE_CAMERA,
	MEMTRACK_NUM_TYPES,
};

/*
 * These definitions below are defined in ion_exynos modules.
 * That will not be changed later. We define with prefix MEMTRACK in duplicate
 *
 * ION_HEAP_TYPE_CARVEOUT
 * ION_EXYNOS_FLAG_PROTECTED
 * ION_FLAG_MAY_HWRENDER
 */
#define MEMTRACK_ION_HEAP_TYPE_CARVEOUT (ION_HEAP_TYPE_CUSTOM + 1)
#define MEMTRACK_ION_EXYNOS_FLAG_PROTECTED BIT(16)
#define MEMTRACK_ION_FLAG_MAY_HWRENDER BIT(6)

static unsigned int dmabuf_trace_get_memtrack_flags(struct dma_buf *dmabuf)
{
	unsigned long mflags, flags = 0;

	mflags = MEMTRACK_FLAG_SMAPS_UNACCOUNTED | MEMTRACK_FLAG_SHARED_PSS;

	if (dma_buf_get_flags(dmabuf, &flags))
		return 0;

	if (ION_HEAP_MASK(flags >> ION_HEAP_SHIFT) ==
	    MEMTRACK_ION_HEAP_TYPE_CARVEOUT)
		mflags |= MEMTRACK_FLAG_DEDICATED;
	else
		mflags |= MEMTRACK_FLAG_SYSTEM;

	if (ION_BUFFER_MASK(flags) & MEMTRACK_ION_EXYNOS_FLAG_PROTECTED)
		mflags |= MEMTRACK_FLAG_SECURE;
	else
		mflags |= MEMTRACK_FLAG_NONSECURE;

	return mflags;
}

static unsigned int dmabuf_trace_get_memtrack_type(struct dma_buf *dmabuf)
{
	unsigned long flags = 0;

	if (dma_buf_get_flags(dmabuf, &flags))
		return 0;

	if (ION_BUFFER_MASK(flags) & MEMTRACK_ION_FLAG_MAY_HWRENDER)
		return MEMTRACK_TYPE_GRAPHICS;

	return MEMTRACK_TYPE_OTHER;
}

static void dmabuf_trace_set_sizes(unsigned int pid, unsigned int count,
				   unsigned int type, unsigned int flags[],
				   unsigned int sizes[])
{
	struct dmabuf_trace_task *task;
	struct dmabuf_trace_ref *ref;
	int i;

	mutex_lock(&trace_lock);
	list_for_each_entry(task, &head_task.node, node)
		if (task->task->pid == pid)
			break;

	if (&task->node == &head_task.node) {
		mutex_unlock(&trace_lock);
		return;
	}

	list_for_each_entry(ref, &task->ref_list, task_node) {
		unsigned int mflags, mtype;

		mflags = dmabuf_trace_get_memtrack_flags(ref->buffer->dmabuf);
		mtype = dmabuf_trace_get_memtrack_type(ref->buffer->dmabuf);

		for (i = 0; i < count; i++) {
			if ((flags[i] == mflags) && (type == mtype))
				sizes[i] += ref->buffer->dmabuf->size /
					ref->buffer->shared_count;
		}
	}
	mutex_unlock(&trace_lock);
}

static int dmabuf_trace_get_memory(unsigned int cmd, unsigned long arg)
{
	unsigned int flags[MAX_DMABUF_TRACE_MEMORY];
	unsigned int sizes[MAX_DMABUF_TRACE_MEMORY] = {0, };
	unsigned int count, pid, type;
	int ret;

	ret = dmabuf_trace_get_user_data(cmd, (void __user *)arg, &pid,
					 &count, &type, flags);
	if (ret)
		return ret;

	dmabuf_trace_set_sizes(pid, count, type, flags, sizes);

	return dmabuf_trace_put_user_data(cmd, (void __user *)arg,
					  sizes, count);
}

static long dmabuf_trace_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg)
{
	switch (cmd) {
#ifdef CONFIG_COMPAT
	case DMABUF_TRACE_COMPAT_IOCTL_GET_MEMORY:
#endif
	case DMABUF_TRACE_IOCTL_GET_MEMORY:
		return dmabuf_trace_get_memory(cmd, arg);
	default:
		return -ENOTTY;
	}
}

static const struct file_operations dmabuf_trace_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = dmabuf_trace_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= dmabuf_trace_ioctl,
#endif
};

static struct miscdevice dmabuf_trace_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "dmabuf_trace",
	.fops = &dmabuf_trace_fops,
};

static int __init dmabuf_trace_create(void)
{
	int ret;

	ret = misc_register(&dmabuf_trace_dev);
	if (ret) {
		pr_err("failed to register dmabuf_trace dev\n");
		return ret;
	}

	ret = dmabuf_trace_init_debugfs();
	if (ret) {
		misc_deregister(&dmabuf_trace_dev);
		return ret;
	}

	register_oom_notifier(&dmabuf_oom_notifier);

	INIT_LIST_HEAD(&head_task.node);
	INIT_LIST_HEAD(&head_task.ref_list);

	pr_info("Initialized dma-buf trace successfully.\n");

	return 0;
}
fs_initcall_sync(dmabuf_trace_create);
