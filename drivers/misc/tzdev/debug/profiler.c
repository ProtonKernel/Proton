/*
 * Copyright (C) 2012-2019, Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/rtc.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include "lib/circ_buf.h"

#include "tzdev_internal.h"
#include "core/cdev.h"
#include "core/cred.h"
#include "core/iwio.h"
#include "core/log.h"
#include "core/notifier.h"
#include "core/subsystem.h"
#include "debug/profiler.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eugene Mandrenko <i.mandrenko@samsung.com>");
MODULE_DESCRIPTION("Trustzone profiler driver");

#define TZPROFILER_IOC_MAGIC		'p'
#define TZPROFILER_INCREASE_POOL	_IOW(TZPROFILER_IOC_MAGIC, 0, uint32_t)
#define TZPROFILER_START		_IO(TZPROFILER_IOC_MAGIC, 1)
#define TZPROFILER_STOP			_IO(TZPROFILER_IOC_MAGIC, 2)
#define TZPROFILER_SET_DEPTH		_IOW(TZPROFILER_IOC_MAGIC, 3, uint32_t)
#define TZPROFILER_SET_UUID		_IOW(TZPROFILER_IOC_MAGIC, 4, struct tz_uuid)
#define TZPROFILER_SET_START_ADDR	_IOW(TZPROFILER_IOC_MAGIC, 5, uint64_t)
#define TZPROFILER_SET_STOP_ADDR	_IOW(TZPROFILER_IOC_MAGIC, 6, uint64_t)
#define TZPROFILER_SET_STEPS_NUMBER	_IOW(TZPROFILER_IOC_MAGIC, 7, uint32_t)

#define TZDEV_PROFILER_BUF_SIZE	(CONFIG_TZPROFILER_BUF_PG_CNT * PAGE_SIZE -\
		sizeof(struct circ_buf))

enum {
	TZPROFILER_LIST1_NEED_CLEAN = 0,
	TZPROFILER_LIST2_NEED_CLEAN
};

enum {
	TZDEV_PROFILER_CMD_START,
	TZDEV_PROFILER_CMD_STOP,
	TZDEV_PROFILER_CMD_SET_DEPTH,
	TZDEV_PROFILER_CMD_SET_UUID,
	TZDEV_PROFILER_CMD_SET_START_ADDR,
	TZDEV_PROFILER_CMD_SET_STOP_ADDR,
	TZDEV_PROFILER_CMD_SET_STEPS_NUMBER,
	TZDEV_PROFILER_CMD_COUNT,
};

struct profiler_buf_entry {
	struct list_head list;
	struct circ_buf *tzio_buf;
};

/* Specifies if the buffer pool is initialized */
static DECLARE_COMPLETION(tzprofiler_pool_completion);

/* There are two lists. One list contains buffers that wait for reading data from them
 * (data from one of them is currently read). Another list contains buffers that have been read.
 * After reading of all data from a buffer from the first list the buffer moves to the second
 * list, and the next buffer in the first list begin to be read. When all buffers are moved from
 * the first list to the second one, the lists swap roles, and after that the second list contains
 * buffer that have to be read, and after reading they move to the first list. This process is
 * cyclic. current_full_pool variable specifies list with buffers that have currently to be read. */
LIST_HEAD(profiler_buf_list1);
LIST_HEAD(profiler_buf_list2);
static int current_full_pool = TZPROFILER_LIST1_NEED_CLEAN;

/* 1 if Profiler is initialized, 0 otherwise */
static atomic_t tzprofiler_init_done = ATOMIC_INIT(0);
/* Number of threads in SWd */
static atomic_t sk_is_active_count = ATOMIC_INIT(0);
/* Check if the data is currently being read */
static atomic_t tzprofiler_data_is_being_written = ATOMIC_INIT(0);
/* Specifies how many additional idle iterations remain to do after exit from SWd.
 * The value of the variable is set in post-SMC and decreases in reading loop at each
 * iteration when its value is nonzero and no data was read. We need it in order to read
 * remaining data that might be written from SWd in buffers that at the time of exit from
 * SWd are located in the list where the buffers move to after reading. */
static atomic_t tzprofiler_last_passing = ATOMIC_INIT(0);

/* Waitqueue for waiting until SWd becomes active */
static DECLARE_WAIT_QUEUE_HEAD(sk_wait);
/* Waitqueue for waiting until data reading finish */
static DECLARE_WAIT_QUEUE_HEAD(tzprofiler_data_writing);

/**
 * Initialize buffer pool in which profiling data will being written in SWd
 * @param[in] bufs_cnt	number of buffers
 * @param[in] buf_pg_cnt	number of pages in each buffer
 * @return 0 if succeess, -ENOMEM if memory allocation failed, -ENXIO if IW
 * channel allocation failed
 */
static int tzprofiler_init_buf_pool(unsigned int bufs_cnt, unsigned int buf_pg_cnt)
{
	struct profiler_buf_entry *profiler_buf;
	unsigned int i;

	for (i = 0; i < bufs_cnt; ++i) {
		profiler_buf = kmalloc(sizeof(struct profiler_buf_entry), GFP_KERNEL);
		if (profiler_buf == NULL)
			return -ENOMEM;
		profiler_buf->tzio_buf = tz_iwio_alloc_iw_channel(
				TZ_IWIO_CONNECT_PROFILER, buf_pg_cnt, NULL, NULL, NULL);
		if (IS_ERR(profiler_buf->tzio_buf)) {
			kfree(profiler_buf);

			return -ENXIO;
		}
		profiler_buf->tzio_buf->write_count = profiler_buf->tzio_buf->read_count = 0;
		list_add(&profiler_buf->list, &profiler_buf_list1);
	}

	complete_all(&tzprofiler_pool_completion);

	return 0;
}

/**
 * Set depth (max number of nested functions to be profiled) in SWd
 * @param[in] depth
 * @return result of SMC
 */
static int tzprofiler_set_depth(unsigned int depth)
{
	int ret;

	ret = tzdev_smc_profiler_control(TZDEV_PROFILER_CMD_SET_DEPTH, depth);
	if (ret == 0) {
		log_debug(tzdev_profiler, "Set depth successfully\n");
	} else if (ret == -ENOSYS) {
		ret = 0;
		log_debug(tzdev_profiler, "SWd is built without profiler\n");
	} else {
		log_error(tzdev_profiler, "failed: depth setup error. ret = %d\n", ret);
	}

	return ret;
}

/**
 * Start profiler in SWd
 * @return result of SMC
 */
static int tzprofiler_start(void)
{
	int ret;

	ret = tzdev_smc_profiler_control(TZDEV_PROFILER_CMD_START, 0);
	if (ret == 0) {
		log_debug(tzdev_profiler, "Profiler has started successfully\n");
	} else if (ret == -ENOSYS) {
		ret = 0;
		log_debug(tzdev_profiler, "SWd is built without profiler\n");
	} else {
		log_error(tzdev_profiler, "failed: Profiler has not started. ret = %d\n", ret);
	}

	return ret;
}

/**
 * Stop profiler in SWd
 * @return result of SMC
 */
static int tzprofiler_stop(void)
{
	int ret;

	ret = tzdev_smc_profiler_control(TZDEV_PROFILER_CMD_STOP, 0);
	if (ret == 0) {
		log_debug(tzdev_profiler, "Profiler has stopped successfully\n");
	} else if (ret == -ENOSYS) {
		ret = 0;
		log_debug(tzdev_profiler, "SWd is built without profiler\n");
	} else {
		log_error(tzdev_profiler, "failed: Profiler has not stopped. ret = %d\n", ret);
	}

	return ret;
}

/**
 * Set UUID of the app to be profiled
 * @param[in] uuid
 * @return result of SMC
 */
static int tzprofiler_set_uuid(struct tz_uuid *uuid)
{
	int ret = 0;
	struct tz_iwio_aux_channel *ch;

	ch = tz_iwio_get_aux_channel();
	memcpy(ch->buffer, uuid, sizeof(struct tz_uuid));
	ret = tzdev_smc_profiler_control(TZDEV_PROFILER_CMD_SET_UUID, 0);
	tz_iwio_put_aux_channel();

	if (ret == 0) {
		log_debug(tzdev_profiler, "Set uuid successfully\n");
	} else if (ret == -ENOSYS) {
		ret = 0;
		log_debug(tzdev_profiler, "SWd is built without profiler\n");
	} else {
		log_error(tzdev_profiler, "failed: uuid setup error. ret = %d\n", ret);
	}

	return ret;
}

/**
 * Set start address (the address, after which profiling will be performed)
 * @param[in] addr
 * @return result of SMC
 */
static int tzprofiler_set_start_addr(uint64_t *addr)
{
	int ret;
	struct tz_iwio_aux_channel *ch;

	ch = tz_iwio_get_aux_channel();
	memcpy(ch->buffer, addr, sizeof(uint64_t));
	ret = tzdev_smc_profiler_control(TZDEV_PROFILER_CMD_SET_START_ADDR, 0);
	tz_iwio_put_aux_channel();

	if (ret == 0) {
		log_debug(tzdev_profiler, "Set start addr successfully\n");
	} else if (ret == -ENOSYS) {
		ret = 0;
		log_debug(tzdev_profiler, "SWd is built without profiler\n");
	} else {
		log_error(tzdev_profiler, "failed: start addr setup error. ret = %d\n", ret);
	}

	return ret;
}

/**
 * Set stop address (the address, before which profiling will be performed)
 * @param[in] addr
 * @return result of SMC
 */
static int tzprofiler_set_stop_addr(uint64_t *addr)
{
	int ret;
	struct tz_iwio_aux_channel *ch;

	ch = tz_iwio_get_aux_channel();
	memcpy(ch->buffer, addr, sizeof(uint64_t));
	ret = tzdev_smc_profiler_control(TZDEV_PROFILER_CMD_SET_STOP_ADDR, 0);
	tz_iwio_put_aux_channel();

	if (ret == 0) {
		log_debug(tzdev_profiler, "Set stop addr successfully\n");
	} else if (ret == -ENOSYS) {
		ret = 0;
		log_debug(tzdev_profiler, "SWd is built without profiler\n");
	} else {
		log_error(tzdev_profiler, "failed: stop addr setup error. ret = %d\n", ret);
	}

	return ret;
}

/**
 * Set steps number (the number of function calls under profiler)
 * @param[in] steps
 * @return result of SMC
 */
static int tzprofiler_set_steps_number(uint32_t steps)
{
	int ret;
	struct tz_iwio_aux_channel *ch;

	ch = tz_iwio_get_aux_channel();
	memcpy(ch->buffer, &steps, sizeof(uint32_t));
	ret = tzdev_smc_profiler_control(TZDEV_PROFILER_CMD_SET_STEPS_NUMBER, 0);
	tz_iwio_put_aux_channel();

	if (ret == 0) {
		log_debug(tzdev_profiler, "Set number of steps successfully\n");
	} else if (ret == -ENOSYS) {
		ret = 0;
		log_debug(tzdev_profiler, "SWd is built without profiler\n");
	} else {
		log_error(tzdev_profiler, "failed: number of steps setup error. ret = %d\n", ret);
	}

	return ret;
}

/**
 * Add buffers to the buffer pool
 * @param[in] number	number of buffers to be added
 * @return 0 if succeess, -ENOMEM if memory allocation failed, -ENXIO if IW
 * channel allocation failed
 */
static int tzprofiler_add_buffers(unsigned int number)
{
	int ret;

	ret = tzprofiler_stop();
	if (ret) {
		log_error(tzdev_profiler, "tzprofiler_stop failed: %d\n", ret);
		return ret;
	}

	ret = tzprofiler_init_buf_pool(number, CONFIG_TZPROFILER_BUF_PG_CNT);
	if (ret) {
		log_error(tzdev_profiler, "tzprofiler_init_buf_pool failed: %d\n", ret);
		return ret;
	}

	ret = tzprofiler_start();
	if (ret)
		log_error(tzdev_profiler, "tzprofiler_start failed: %d\n", ret);

	return ret;
}

/**
 * Read data from s_buf (kernel space memory) to d_buf (user space memory)
 * @param[in] s_buf	the buffer which the data is read from
 * @param[in] quantity	number of bytes to read
 * @param[out] d_buf	the buffer which the data is written to
 * @param[inout] saved_count	pointer to a variable that contains beginning position of writing
 * to d_buf. The variable is increased by the number of currently written bytes.
 * @param[in] count	full size of s_buf
 * @return number of currently written bytes
 */
static ssize_t tzprofiler_write_buf(unsigned char *s_buf, ssize_t quantity,
		unsigned char __user *d_buf, ssize_t *saved_count, size_t count)
{
	ssize_t real_saved;

	if ((*saved_count + quantity) <= count)
		real_saved = quantity;
	else
		real_saved = count - *saved_count;

	if (copy_to_user(&d_buf[*saved_count], s_buf, real_saved))
		log_error(tzdev_profiler, "can't copy to user\n");

	*saved_count += real_saved;

	return real_saved;
}

/**
 * Read count bytes from the buffer pool to buf
 * @param[out] buf	output buffer of the driver's reading operation
 * @param[in] count	max number of bytes to read
 * @param[inout] head	list of buffers from which data is currently read
 * @param[inout] cleaned_head	list of buffers that have been read
 * @return number of actually read symbols
 */
static ssize_t __read(char __user *buf, size_t count, struct list_head *head,
		struct list_head *cleaned_head)
{
	struct circ_buf *tzio_buf;
	struct profiler_buf_entry *profiler_buf, *tmp;
	ssize_t bytes, quantity, write_count, saved_count = 0;

	/* Iterate through each buffer of list "head" until "head" become empty or
	 * some data will be read from a buffer */
	list_for_each_entry_safe(profiler_buf, tmp, head, list) {
		tzio_buf = profiler_buf->tzio_buf;
		if (tzio_buf->read_count == tzio_buf->write_count) {
			list_del(&profiler_buf->list);
			list_add(&profiler_buf->list, cleaned_head);
			continue;
		}

		write_count = tzio_buf->write_count;
		if (write_count < tzio_buf->read_count) {
			quantity = TZDEV_PROFILER_BUF_SIZE - tzio_buf->read_count;
			bytes = tzprofiler_write_buf(tzio_buf->buffer + tzio_buf->read_count,
				quantity, buf, &saved_count, count);
			if (bytes < quantity) {
				tzio_buf->read_count += bytes;
				atomic_set(&tzprofiler_data_is_being_written, 1);
				return saved_count;
			}
			tzio_buf->read_count = 0;
		}
		quantity = write_count - tzio_buf->read_count;
		bytes = tzprofiler_write_buf(tzio_buf->buffer + tzio_buf->read_count, quantity,
						buf, &saved_count, count);
		if (bytes < quantity) {
			tzio_buf->read_count += bytes;
			atomic_set(&tzprofiler_data_is_being_written, 1);
			return saved_count;
		}
		tzio_buf->read_count += quantity;
		list_del(&profiler_buf->list);
		list_add(&profiler_buf->list, cleaned_head);
	}
	return saved_count;
}

/**
 * Reading operation handler of the char driver. It reads the data from the buffer pool
 */
static ssize_t tzprofiler_read(struct file *filp, char __user *buf, size_t count,
		loff_t *f_pos)
{
	struct list_head *current_head, *cleaned_head;
	size_t saved_count;
	int ret;

	atomic_set(&tzprofiler_data_is_being_written, 0);

	/* Hung in the loop until some data is read. It is implemented since reading operation handler
	 * is required to read some data, otherwise reading of zero-length data might be considered
	 * as the end of all data. */
	while (1) {
		if ((atomic_read(&sk_is_active_count) == 0) && (atomic_read(&tzprofiler_last_passing) == 0)) {
			/* Wait until a thread switches to SWd, a signal is received or 5 seconds expire.
			* It is implemented to avoid continuous looping while there are no threads in SWd. */
			ret = wait_event_interruptible_timeout(sk_wait,
					atomic_read(&sk_is_active_count) != 0, msecs_to_jiffies(5000));
			if (ret < 0)
				return (ssize_t)-EINTR;
		}

		/* Wait until the buffer pool is initialized */
		ret = wait_for_completion_interruptible(&tzprofiler_pool_completion);
		if (unlikely(ret < 0))
			return (ssize_t)-EINTR;

		if (current_full_pool == TZPROFILER_LIST1_NEED_CLEAN) {
			current_head = &profiler_buf_list1;
			cleaned_head = &profiler_buf_list2;
		} else {
			current_head = &profiler_buf_list2;
			cleaned_head = &profiler_buf_list1;
		}
		/* Read data from the buffer pool to buf */
		saved_count = __read(buf, count, current_head, cleaned_head);
		/* Check if some data has been read */
		if (saved_count) {
			atomic_set(&tzprofiler_data_is_being_written, 1);
			return saved_count;
		}

		/* The following code (to the end of the loop) is executed only if no data has been read */
		wake_up(&tzprofiler_data_writing);
		if (atomic_read(&tzprofiler_last_passing) > 0)
			atomic_dec(&tzprofiler_last_passing);

		/* Swap lists' roles */
		if (current_full_pool == TZPROFILER_LIST1_NEED_CLEAN)
			current_full_pool = TZPROFILER_LIST2_NEED_CLEAN;
		else
			current_full_pool = TZPROFILER_LIST1_NEED_CLEAN;
	}

	return 0;
}

/**
 * Opening operation handler of the char driver.
 */
static int tzprofiler_open(struct inode *inode, struct file *filp)
{
	return 0;
}

/**
 * unlocked_ioctl operation handler of the char driver.
 */
static long tzprofiler_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	uint64_t addr;
	struct tz_uuid uuid;

	switch (cmd) {
	case TZPROFILER_INCREASE_POOL:
		ret = tzprofiler_add_buffers(arg);
		break;
	case TZPROFILER_SET_DEPTH:
		ret = tzprofiler_set_depth(arg);
		break;
	case TZPROFILER_START:
		ret = tzprofiler_start();
		break;
	case TZPROFILER_STOP:
		ret = tzprofiler_stop();
		break;
	case TZPROFILER_SET_UUID:
		if (copy_from_user(&uuid, (void *)arg, sizeof(struct tz_uuid)))
			return -EFAULT;

		ret = tzprofiler_set_uuid(&uuid);
		break;
	case TZPROFILER_SET_START_ADDR:
		if (copy_from_user(&addr, (void *)arg, sizeof(uint64_t)))
			return -EFAULT;

		ret = tzprofiler_set_start_addr(&addr);
		break;
	case TZPROFILER_SET_STOP_ADDR:
		if (copy_from_user(&addr, (void *)arg, sizeof(uint64_t)))
			return -EFAULT;

		ret = tzprofiler_set_stop_addr(&addr);
		break;
	case TZPROFILER_SET_STEPS_NUMBER:
		ret = tzprofiler_set_steps_number(arg);
		break;
	default:
		ret = -ENOTTY;
		log_error(tzdev_profiler, "Unexpected command %u\n", cmd);
		break;
	}

	return ret;
}

/**
 * Releasing operation handler of the char driver.
 */
static int tzprofiler_release(struct inode *inode, struct file *filp)
{
	atomic_set(&tzprofiler_data_is_being_written, 0);
	wake_up(&tzprofiler_data_writing);

	return 0;
}

static const struct file_operations tzprofiler_fops = {
	.owner = THIS_MODULE,
	.read = tzprofiler_read,
	.open = tzprofiler_open,
	.unlocked_ioctl = tzprofiler_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tzprofiler_unlocked_ioctl,
#endif /* CONFIG_COMPAT */
	.release = tzprofiler_release,
};

static struct tz_cdev tzprofiler_cdev = {
	.name = "tzprofiler",
	.fops = &tzprofiler_fops,
	.owner = THIS_MODULE,
};

/**
 * The function will be invoked before each SMC
 */
void tzprofiler_pre_smc_call_direct(void)
{
	atomic_inc(&sk_is_active_count);
	atomic_set(&tzprofiler_last_passing, 2);
	wake_up(&sk_wait);
}

/**
 * Wait until all data will be read from the buffer pool
 */
static void tzprofiler_wait_for_bufs(void)
{
	int ret;

	if (atomic_read(&tzprofiler_data_is_being_written) == 0)
		return;

	if (!in_atomic()) {
		ret = wait_event_interruptible(tzprofiler_data_writing,
				(atomic_read(&tzprofiler_data_is_being_written) == 0) ||
				(atomic_read(&tzprofiler_last_passing) == 0));
		if (ret < 0) {
			log_debug(tzdev_profiler, "waiting for buffers interrupted!\n");
			atomic_set(&tzprofiler_data_is_being_written, 0);
		}
	}
}

/**
 * The function will be invoked after each SMC
 */
void tzprofiler_post_smc_call_direct(void)
{
	tzprofiler_wait_for_bufs();

	atomic_set(&tzprofiler_last_passing, 2);
	wake_up(&sk_wait);
	atomic_dec(&sk_is_active_count);
}

/**
 * Profiler initialization
 */
static int tzprofiler_init_call(struct notifier_block *cb, unsigned long code, void *unused)
{
	int rc;

	(void)cb;
	(void)code;
	(void)unused;

	rc = tzprofiler_init_buf_pool(CONFIG_TZPROFILER_BUFS_CNT, CONFIG_TZPROFILER_BUF_PG_CNT);
	if (rc)
		return NOTIFY_DONE;

	rc = tzprofiler_start();
	if (rc)
		return NOTIFY_DONE;

	rc = tz_cdev_register(&tzprofiler_cdev);
	if (rc)
		goto profiler_device_reg_failed;

	atomic_set(&tzprofiler_init_done, 1);

	log_info(tzdev_profiler, "Profiler initialization done.\n");

	return NOTIFY_DONE;

profiler_device_reg_failed:
	tzprofiler_stop();

	log_error(tzdev_profiler, "Profiler initialization failed!\n");

	return NOTIFY_DONE;
}

/**
 * Profiler finalization
 */
static int tzprofiler_fini_call(struct notifier_block *cb, unsigned long code, void *unused)
{
	(void)cb;
	(void)code;
	(void)unused;

	/* Check if the profiler is initialized */
	if (!atomic_cmpxchg(&tzprofiler_init_done, 1, 0)) {
		log_info(tzdev_profiler, "Profile not initialized.\n");
		return NOTIFY_DONE;
	}

	tz_cdev_unregister(&tzprofiler_cdev);
	tzprofiler_stop();

	log_info(tzdev_profiler, "Profiler finalization done.\n");

	return NOTIFY_DONE;
}

static struct notifier_block tzprofiler_init_notifier = {
	.notifier_call = tzprofiler_init_call,
};

static struct notifier_block tzprofiler_fini_notifier = {
	.notifier_call = tzprofiler_fini_call,
};

/**
 * Profiler's driver initialization. It registers profiler's initialization
 * and finalization function in corresponding global notifiers.
 */
int tzprofiler_init(void)
{
	int rc;

	rc = tzdev_blocking_notifier_register(TZDEV_INIT_NOTIFIER, &tzprofiler_init_notifier);
	if (rc)
		return rc;

	rc = tzdev_blocking_notifier_register(TZDEV_FINI_NOTIFIER, &tzprofiler_fini_notifier);
	if (rc) {
		tzdev_blocking_notifier_unregister(TZDEV_INIT_NOTIFIER, &tzprofiler_init_notifier);
		return rc;
	}
	log_info(tzdev_profiler, "Profiler callbacks registration done\n");

	return 0;
}

tzdev_early_initcall(tzprofiler_init);
