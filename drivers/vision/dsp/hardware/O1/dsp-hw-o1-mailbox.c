// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-o1-system.h"
#include "dsp-hw-o1-memory.h"
#include "dsp-hw-o1-mailbox.h"

static struct dsp_mailbox_pool *dsp_hw_o1_mailbox_alloc_pool(
		struct dsp_mailbox *mbox, unsigned int size)
{
	int ret;
	struct dsp_mailbox_pool_manager *pmgr;
	unsigned int aligned_size;
	struct dsp_mailbox_pool *pool;
	unsigned long flags;

	dsp_enter();
	pmgr = mbox->pool_manager;

	if (!size) {
		ret = -EINVAL;
		dsp_err("size of mailbox pool must be not zero\n");
		goto p_err;
	}

	aligned_size = size + pmgr->block_size - 1;
	if (aligned_size < size) {
		ret = -EINVAL;
		dsp_err("size overflowed(%#x/%#x/%#x)\n",
				aligned_size, size, pmgr->block_size);
		goto p_err;
	}

	pool = kzalloc(sizeof(*pool), GFP_KERNEL);
	if (!pool) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc pool header\n");
		goto p_err;
	}
	pool->owner = pmgr;
	pool->block_count = aligned_size / pmgr->block_size;

	spin_lock_irqsave(&pmgr->slock, flags);
	pool->block_start = dsp_util_bitmap_set_region(&pmgr->pool_map,
			pool->block_count);
	if (pool->block_start < 0) {
		spin_unlock_irqrestore(&pmgr->slock, flags);
		ret = pool->block_start;
		dsp_err("Failed to allocate pool bitmap(%d)\n", ret);
		goto p_err_bitmap;
	}

	list_add_tail(&pool->list, &pmgr->pool_list);
	pmgr->pool_count++;
	spin_unlock_irqrestore(&pmgr->slock, flags);

	pool->size = pmgr->block_size * pool->block_count;
	pool->used_size = size;
	pool->iova = pmgr->iova + (unsigned int)pool->block_start *
		pmgr->block_size;
	pool->kva = pmgr->kva + (unsigned int)pool->block_start *
		pmgr->block_size;

	dsp_leave();
	return pool;
p_err_bitmap:
	kfree(pool);
p_err:
	return ERR_PTR(ret);
}

static void dsp_hw_o1_mailbox_free_pool(struct dsp_mailbox *mbox,
		struct dsp_mailbox_pool *pool)
{
	struct dsp_mailbox_pool_manager *pmgr;
	unsigned long flags;

	dsp_enter();
	pmgr = pool->owner;

	spin_lock_irqsave(&pmgr->slock, flags);
	pmgr->pool_count--;
	list_del(&pool->list);
	dsp_util_bitmap_clear_region(&pmgr->pool_map, pool->block_start,
			pool->block_count);
	spin_unlock_irqrestore(&pmgr->slock, flags);

	kfree(pool);
	dsp_leave();
}

static void dsp_hw_o1_mailbox_dump_pool(struct dsp_mailbox *mbox,
		struct dsp_mailbox_pool *pool)
{
	size_t idx;

	dsp_enter();
	dsp_notice("[DSP_MAILBOX_DUMP_POOL][dump][%5zdbytes]\n",
			pool->used_size);
	for (idx = 0; idx < (pool->used_size >> 2); idx++) {
		dsp_notice("[DSP_MAILBOX_DUMP_POOL][%4zu][0x%8x]\n", idx,
				*((unsigned int *)pool->kva + idx));
	}

	dsp_leave();
}

static int __dsp_hw_o1_mailbox_check_version(struct dsp_mailbox *mbox,
		unsigned int mailbox_version, unsigned int message_version)
{
	int ret;

	dsp_enter();
	if (!(mailbox_version > DSP_MAILBOX_VERSION_START &&
		mailbox_version < DSP_MAILBOX_VERSION_END)) {
		ret = -EINVAL;
		dsp_err("mailbox is not a supported version(v%u/v%u ~ v%u)\n",
				mailbox_version,
				DSP_MAILBOX_VERSION_START + 1,
				DSP_MAILBOX_VERSION_END - 1);
		goto p_err;
	}

	if (!(message_version > DSP_MESSAGE_VERSION_START &&
		message_version < DSP_MESSAGE_VERSION_END)) {
		ret = -EINVAL;
		dsp_err("message is not a supported version(v%u/v%u ~ v%u)\n",
				message_version,
				DSP_MESSAGE_VERSION_START + 1,
				DSP_MESSAGE_VERSION_END - 1);
		goto p_err;
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_o1_mailbox_send_mail(struct dsp_mailbox *mbox,
		struct dsp_mailbox_to_fw *mail)
{
	int ret;
	unsigned int repeat = 100;

	dsp_enter();
	mutex_lock(&mbox->lock);
	while (1) {
		ret = dsp_util_queue_check_full(mbox->to_fw);
		if (!ret || !repeat)
			break;
		repeat--;
		udelay(10);
	}

	if (ret) {
		ret = -EBUSY;
		dsp_err("There is no free space in the mailbox(%d)\n", ret);
		dsp_util_queue_dump(mbox->to_fw);
		goto p_err_full;
	}

	ret = dsp_util_queue_enqueue(mbox->to_fw, mail, sizeof(*mail));
	if (ret)
		goto p_err_enqueue;

	mutex_unlock(&mbox->lock);
	mbox->sys->interface.ops->send_irq(&mbox->sys->interface,
			BIT(DSP_TO_CC_INT_MAILBOX));
	dsp_leave();
	return 0;
p_err_enqueue:
p_err_full:
	mutex_unlock(&mbox->lock);
	return ret;
}

static int dsp_hw_o1_mailbox_send_message(struct dsp_mailbox *mbox,
		unsigned int message_id)
{
	int ret;
	struct dsp_mailbox_to_fw mail = { 0, };

	dsp_enter();
	mail.mailbox_version = mbox->mailbox_version;
	mail.message_version = mbox->message_version;
	mail.message_id = message_id;

	ret = __dsp_hw_o1_mailbox_send_mail(mbox, &mail);
	if (ret)
		goto p_err_send;

	dsp_leave();
	return 0;
p_err_send:
	return ret;
}

static int dsp_hw_o1_mailbox_send_task(struct dsp_mailbox *mbox,
		struct dsp_task *task)
{
	int ret;
	struct dsp_mailbox_to_fw mail = { 0, };
	unsigned long flags;

	dsp_enter();
	mail.mailbox_version = mbox->mailbox_version;
	mail.message_version = mbox->message_version;
	mail.task_id = task->id;
	mail.pool_iova = task->pool->iova;
	mail.pool_size = (unsigned int)task->pool->size;
	mail.message_id = task->message_id;
	mail.message_size = (unsigned int)task->pool->used_size;

	if (mail.message_version != task->message_version) {
		dsp_err("message_version is different(FW:%u/USER:%u)\n",
		mail.message_version, task->message_version);
		ret = -EINVAL;
		goto p_err;
	}

	spin_lock_irqsave(&task->owner->slock, flags);
	ret = dsp_task_trans_ready_to_process(task);
	if (ret) {
		task->result = ret;
		dsp_task_trans_any_to_complete(task);
		spin_unlock_irqrestore(&task->owner->slock, flags);
		goto p_err;
	}
	spin_unlock_irqrestore(&task->owner->slock, flags);

	ret = __dsp_hw_o1_mailbox_send_mail(mbox, &mail);
	if (ret)
		goto p_err_send;

	dsp_time_get_timestamp(&task->pool->time, TIMESTAMP_START);
	dsp_leave();
	return 0;
p_err_send:
	task->result = ret;
	spin_lock_irqsave(&task->owner->slock, flags);
	dsp_task_trans_process_to_complete(task);
	spin_unlock_irqrestore(&task->owner->slock, flags);
p_err:
	return ret;
}

static int dsp_hw_o1_mailbox_receive_task(struct dsp_mailbox *mbox)
{
	int ret;
	struct dsp_task_manager *tmgr;
	struct dsp_mailbox_to_host mail;
	struct dsp_task *task;
	unsigned long flags;

	dsp_enter();
	tmgr = &mbox->sys->task_manager;

	__ioread32_copy(&mail, mbox->to_host, sizeof(mail) >> 2);

	ret = __dsp_hw_o1_mailbox_check_version(mbox, mail.mailbox_version,
			mail.message_version);
	if (ret)
		goto p_err;

	spin_lock_irqsave(&tmgr->slock, flags);

	task = dsp_task_get_process_by_id(&mbox->sys->task_manager,
			mail.task_id);
	if (!task) {
		spin_unlock_irqrestore(&tmgr->slock, flags);
		ret = -EINVAL;
		dsp_err("response including wrong task id was sent(%u)\n",
				mail.task_id);
		goto p_err;
	}

	task->result = mail.task_ret;
	dsp_task_trans_process_to_complete(task);
	dsp_time_get_timestamp(&task->pool->time, TIMESTAMP_END);
	spin_unlock_irqrestore(&tmgr->slock, flags);

	wake_up(&task->owner->done_wq);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int __dsp_hw_o1_mailbox_set_version(struct dsp_mailbox *mbox)
{
	int ret;
	unsigned int mailbox_version, message_version;

	dsp_enter();
	mailbox_version = dsp_ctrl_dhcp_readl(
			DSP_O1_DHCP_IDX(DSP_O1_DHCP_MAILBOX_VERSION));
	message_version = dsp_ctrl_dhcp_readl(
			DSP_O1_DHCP_IDX(DSP_O1_DHCP_MESSAGE_VERSION));

	ret = __dsp_hw_o1_mailbox_check_version(mbox, mailbox_version,
			message_version);
	if (ret)
		goto p_err;

	mbox->mailbox_version = mailbox_version;
	mbox->message_version = message_version;
	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_mailbox_start(struct dsp_mailbox *mbox)
{
	int ret;
	struct dsp_priv_mem *pmem;

	dsp_enter();
	ret = __dsp_hw_o1_mailbox_set_version(mbox);
	if (ret)
		goto p_err;

	pmem = &mbox->sys->memory.priv_mem[DSP_O1_PRIV_MEM_DHCP];
	mbox->to_fw = pmem->kvaddr + DSP_O1_DHCP_IDX(DSP_O1_DHCP_TO_CC_MBOX);
	mbox->to_host = pmem->kvaddr +
		DSP_O1_DHCP_IDX(DSP_O1_DHCP_TO_HOST_MBOX);

	pmem = &mbox->sys->memory.priv_mem[DSP_O1_PRIV_MEM_MBOX_MEMORY];
	dsp_util_queue_init(mbox->to_fw, sizeof(struct dsp_mailbox_to_fw),
			pmem->size, (unsigned int)pmem->iova,
			(unsigned long long)pmem->kvaddr);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static int dsp_hw_o1_mailbox_stop(struct dsp_mailbox *mbox)
{
	dsp_enter();
	mbox->to_fw = NULL;
	mbox->to_host = NULL;
	dsp_leave();
	return 0;
}

static int __dsp_hw_o1_mailbox_pool_manager_init(struct dsp_mailbox *mbox)
{
	int ret;
	struct dsp_priv_mem *pmem;
	struct dsp_mailbox_pool_manager *pmgr;

	dsp_enter();
	pmem = &mbox->sys->memory.priv_mem[DSP_O1_PRIV_MEM_MBOX_POOL];
	pmgr = mbox->pool_manager;

	pmgr->pool_count = 0;
	pmgr->size = pmem->size;
	pmgr->iova = pmem->iova;
	pmgr->kva = pmem->kvaddr;

	pmgr->block_size = SZ_1K;
	pmgr->block_count = (unsigned int)(pmgr->size / pmgr->block_size);
	pmgr->used_count = 0;

	ret = dsp_util_bitmap_init(&pmgr->pool_map, "pool_bitmap",
			pmgr->block_count);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void __dsp_hw_o1_mailbox_pool_manager_deinit(
		struct dsp_mailbox_pool_manager *pmgr)
{
	struct dsp_mailbox_pool *pool, *temp;

	dsp_enter();
	list_for_each_entry_safe(pool, temp, &pmgr->pool_list, list)
		dsp_mailbox_free_pool(pool);

	dsp_util_bitmap_deinit(&pmgr->pool_map);
	dsp_leave();
}

static int dsp_hw_o1_mailbox_open(struct dsp_mailbox *mbox)
{
	dsp_enter();
	mbox->mailbox_version = 0;
	mbox->message_version = 0;

	__dsp_hw_o1_mailbox_pool_manager_init(mbox);
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_mailbox_close(struct dsp_mailbox *mbox)
{
	dsp_enter();
	__dsp_hw_o1_mailbox_pool_manager_deinit(mbox->pool_manager);
	dsp_leave();
	return 0;
}

static int dsp_hw_o1_mailbox_probe(struct dsp_system *sys)
{
	int ret;
	struct dsp_mailbox *mbox;
	struct dsp_mailbox_pool_manager *pmgr;

	dsp_enter();
	mbox = &sys->mailbox;
	mbox->sys = sys;

	mutex_init(&mbox->lock);
	pmgr = kzalloc(sizeof(*mbox->pool_manager), GFP_KERNEL);
	if (!pmgr) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc pool manager\n");
		goto p_err;
	}
	spin_lock_init(&pmgr->slock);
	INIT_LIST_HEAD(&pmgr->pool_list);
	pmgr->mbox = mbox;
	mbox->pool_manager = pmgr;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void dsp_hw_o1_mailbox_remove(struct dsp_mailbox *mbox)
{
	dsp_enter();
	kfree(mbox->pool_manager);
	mutex_destroy(&mbox->lock);
	dsp_leave();
}

static const struct dsp_mailbox_ops o1_mailbox_ops = {
	.alloc_pool	= dsp_hw_o1_mailbox_alloc_pool,
	.free_pool	= dsp_hw_o1_mailbox_free_pool,
	.dump_pool	= dsp_hw_o1_mailbox_dump_pool,
	.send_task	= dsp_hw_o1_mailbox_send_task,
	.receive_task	= dsp_hw_o1_mailbox_receive_task,
	.send_message	= dsp_hw_o1_mailbox_send_message,

	.start		= dsp_hw_o1_mailbox_start,
	.stop		= dsp_hw_o1_mailbox_stop,

	.open		= dsp_hw_o1_mailbox_open,
	.close		= dsp_hw_o1_mailbox_close,
	.probe		= dsp_hw_o1_mailbox_probe,
	.remove		= dsp_hw_o1_mailbox_remove,
};

int dsp_hw_o1_mailbox_init(void)
{
	int ret;

	dsp_enter();
	ret = dsp_mailbox_register_ops(DSP_DEVICE_ID_O1, &o1_mailbox_ops);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
