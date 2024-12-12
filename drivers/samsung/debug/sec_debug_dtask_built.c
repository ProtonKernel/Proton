// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/rwsem.h>
#include <linux/rtmutex.h>
#include <linux/sec_debug.h>

#include <trace/hooks/dtask.h>

#define BUF_SIZE	128

/* use ANDROID_VENDOR_DATA_ARRAY(1,2) */
static inline long __wait_type(struct task_struct *task)
{
	return (long)task->android_vendor_data1[0];
}

static inline void *__wait_data(struct task_struct *task)
{
	return (void *)task->android_vendor_data1[1];
}

static inline void __set_wait_type(struct task_struct *task, long type)
{
	task->android_vendor_data1[0] = (u64)type;
}

static inline void __set_wait_data(struct task_struct *task, void *data)
{
	task->android_vendor_data1[1] = (u64)data;
}

/* from kernel/locking/mutex.c */
#define MUTEX_FLAGS		0x07
static inline struct task_struct *__mutex_owner_task(unsigned long owner)
{
	return (struct task_struct *)(owner & ~MUTEX_FLAGS);
}

static void secdbg_print_mutex_info(struct task_struct *task, struct sec_debug_wait *winfo, bool raw)
{
	struct mutex *wmutex = (struct mutex *)winfo->data;
	struct task_struct *owner_task = __mutex_owner_task(atomic_long_read(&wmutex->owner));

	pr_info("Mutex: %pS", wmutex);
	if (owner_task) {
		if (raw)
			pr_cont(": owner[0x%px %s :%d]", owner_task, owner_task->comm, owner_task->pid);
		else
			pr_cont(": owner[%s :%d]", owner_task->comm, owner_task->pid);
	}
	pr_cont("\n");
}

static void secdbg_print_rtmutex_info(struct task_struct *task, struct sec_debug_wait *winfo, bool raw)
{
	struct rt_mutex *wmutex = (struct rt_mutex *)winfo->data;
	struct task_struct *owner_task = __mutex_owner_task((unsigned long)READ_ONCE(wmutex->owner));

	pr_info("RTmutex: %pS", wmutex);
	if (owner_task) {
		if (raw)
			pr_cont(": owner[0x%px %s :%d]", owner_task, owner_task->comm, owner_task->pid);
		else
			pr_cont(": owner[%s :%d]", owner_task->comm, owner_task->pid);
	}
	pr_cont("\n");
}

/* from kernel/locking/rwsem.c */
#define RWSEM_FLAGS		0x07
#define RWSEM_READER_OWNED	(1UL << 0)
#define RWSEM_WRITER_LOCKED	(1UL << 0)
#define RWSEM_READER_SHIFT	8

static inline struct task_struct *__rwsem_owner_task(unsigned long owner)
{
	return (struct task_struct *)(owner & ~RWSEM_FLAGS);
}

static inline bool __rwsem_writer_locked(struct rw_semaphore *sem)
{
	return (atomic_long_read(&sem->count) & RWSEM_WRITER_LOCKED);
}

static inline bool __rwsem_reader_owned(struct rw_semaphore *sem)
{
	return (atomic_long_read(&sem->owner) & RWSEM_READER_OWNED);
}

static inline long __rwsem_readers_count(struct rw_semaphore *sem)
{
	return atomic_long_read(&sem->count) >> RWSEM_READER_SHIFT;
}

static void secdbg_print_rwsem_info(struct task_struct *task, struct sec_debug_wait *winfo, bool raw)
{
	const char *owned_str;
	struct rw_semaphore *rwsem = (struct rw_semaphore *)winfo->data;
	struct task_struct *owner_task = __rwsem_owner_task(atomic_long_read(&rwsem->owner));
	char tmp[BUF_SIZE] = {0,};

	if (__rwsem_writer_locked(rwsem)) {
		owned_str = "owned by writer";
	} else if (__rwsem_reader_owned(rwsem)) {
		scnprintf(tmp, BUF_SIZE, "%ld readers", __rwsem_readers_count(rwsem));
		owned_str = tmp;
	} else {
		owned_str = "not owned";
	}

	/* it may not be the real owner nor one of the real owner */
	if (!__rwsem_writer_locked(rwsem) && !IS_ENABLED(CONFIG_DEBUG_RWSEMS))
		owner_task = NULL;

	pr_info("RWsem: %pS: %s", rwsem, owned_str);
	if (owner_task) {
		if (raw)
			pr_cont("[0x%px %s :%d]", owner_task,
					owner_task->comm, owner_task->pid);
		else
			pr_cont("[%s :%d]",
					owner_task->comm, owner_task->pid);
	}
	pr_cont("\n");
}

void secdbg_dtsk_built_print_info(struct task_struct *task, bool raw)
{
	struct sec_debug_wait winfo;

	winfo.type = __wait_type(task);
	winfo.data = __wait_data(task);

	if (!(winfo.type && winfo.data))
		return;

	switch (winfo.type) {
	case DTYPE_MUTEX:
		secdbg_print_mutex_info(task, &winfo, raw);
		break;
	case DTYPE_RTMUTEX:
		secdbg_print_rtmutex_info(task, &winfo, raw);
		break;
	case DTYPE_RWSEM:
		secdbg_print_rwsem_info(task, &winfo, raw);
		break;
	default:
		/* unknown type */
		break;
	}
}

static void secdbg_dtsk_built_rwsem_start(void *ignore, struct rw_semaphore *sem)
{
	secdbg_dtsk_built_set_data(DTYPE_RWSEM, sem);
}

static void secdbg_dtsk_built_rwsem_finish(void *ignore, struct rw_semaphore *sem)
{
	secdbg_dtsk_built_clear_data();
}

void secdbg_dtsk_built_set_data(long type, void *data)
{
	struct task_struct *tsk = current;

	__set_wait_type(tsk, type);
	__set_wait_data(tsk, data);
}

static __init int secdbg_dtsk_built_init(void)
{
	pr_info("%s: init\n", __func__);

	register_trace_android_vh_rwsem_read_wait_start(secdbg_dtsk_built_rwsem_start, NULL);
	register_trace_android_vh_rwsem_write_wait_start(secdbg_dtsk_built_rwsem_start, NULL);
	register_trace_android_vh_rwsem_read_wait_finish(secdbg_dtsk_built_rwsem_finish, NULL);
	register_trace_android_vh_rwsem_write_wait_finish(secdbg_dtsk_built_rwsem_finish, NULL);

	return 0;
}
subsys_initcall(secdbg_dtsk_built_init);
