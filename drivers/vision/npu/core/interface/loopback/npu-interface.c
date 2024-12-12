/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/delay.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include "npu-interface.h"
#include "npu-util-llq.h"

#define LINE_TO_SGMT(sgmt_len, ptr)	((ptr) & ((sgmt_len) - 1))
#ifdef CONFIG_NPU_LOOPBACK
struct message GoodMsg = {
		.magic		= MESSAGE_MAGIC,
		.command	= COMMAND_DONE,
		.length		= sizeof(struct command),
		.mid		= 0,
};

struct message BadMsg = {
	.magic		= MESSAGE_MAGIC,
	.command	= COMMAND_NDONE,
	.length		= sizeof(struct command),
	.mid		= 0,
};

struct command GoodDone = {
	.c.done.fid = 0x5A1AD,
	.length = 0,
	.payload = 0,
};

struct command NotDone = {
	.c.ndone.error = 0xE2202,
	.length = 0,
	.payload = 0,
};

struct command TimedOut = {
	.c.ndone.error = ETIMEDOUT,
	.length = 0,
	.payload = 0,
};
#endif
struct npu_interface interface = {
	.mbox_hdr = NULL,
	.sfr = NULL,
	.addr = NULL,
};

void dbg_dump_mbox(void)
{
	return;
}

void dbg_print_error(void)
{
	return;
}

void dbg_print_ncp_header(struct ncp_header *nhdr)
{
	return;
}

void dbg_print_interface(void)
{
	return;
}
#ifdef CONFIG_NPU_LOOPBACK
int Fake_FW(int mbox_idx, int mid)
{
	int ret = 0;
	u32 val = 0;
	struct message rMsg = {};
	struct message sMsg = {};
	struct command rCmd = {};
	struct command sCmd = {};
	val = interface.sfr->grp[mbox_idx].ms;

	if(val) {
		interface.sfr->grp[mbox_idx].c = val;
		ret = mbx_ipc_get_msg((void *)interface.addr, &interface.mbox_hdr->h2fctrl[mbox_idx], &rMsg);
		if(ret <=0) {
			if(ret < 0)
				npu_info("mbx_ipc_get_msg Error - %d\n", ret);
			return -1;
		}
		ret = mbx_ipc_get_cmd((void *)interface.addr, &interface.mbox_hdr->h2fctrl[mbox_idx], &rMsg, &rCmd);
		if(ret) {
			npu_info("mbx_ipc_get_cmd Error - %d\n", ret);
			return -1;
		}
	}
	switch(rMsg.command) {
	case COMMAND_LOAD:
	case COMMAND_UNLOAD:
	case COMMAND_PROCESS:
	case COMMAND_PURGE:
	case COMMAND_POWERDOWN:
		npu_info("[BAE] FAKE FW with Message. mbox_idx : %d, cmd : %d\n", mbox_idx, rMsg.command);
		sMsg = GoodMsg;
		sCmd = GoodDone;
		break;
	default:
		break;
	}
	sMsg.mid = rMsg.mid;
	ret = mbx_ipc_put((void *)interface.addr, &interface.mbox_hdr->f2hctrl[0], &sMsg, &sCmd);
	mbx_ipc_print((void *)interface.addr, &interface.mbox_hdr->f2hctrl[0]);
	npu_info("[BAE] addr : %p\n", (void*)interface.addr);
	interface.sfr->grp[3].g = 0xFFFFFFFF;
	interface.rslt_notifier(NULL);
	return ret;
}
#endif

static irqreturn_t mailbox_isr2(int irq, void *data)
{
	u32 val;

	val = interface.sfr->grp[3].ms;
	if (val)
		interface.sfr->grp[3].c = val;
	if (interface.rslt_notifier != NULL)
		interface.rslt_notifier(NULL);
	return IRQ_HANDLED;
}

static irqreturn_t mailbox_isr3(int irq, void *data)
{
	u32 val;

	val = interface.sfr->grp[4].ms;
	if (val)
		interface.sfr->grp[4].c = val;
	return IRQ_HANDLED;
}

static void __send_interrupt(u32 cmdType)
{
	u32 val;

	switch (cmdType) {
	case COMMAND_LOAD:
	case COMMAND_UNLOAD:
	case COMMAND_PROFILE_CTL:
	case COMMAND_PURGE:
	case COMMAND_POWERDOWN:
	case COMMAND_FW_TEST:
		interface.sfr->grp[0].g = 0x10000;
		val = interface.sfr->grp[0].g;
		//npu_info("g0.g = %x\n", val);
		break;
	case COMMAND_PROCESS:
		interface.sfr->grp[2].g = 0xFFFFFFFF;
		val = interface.sfr->grp[2].g;
		//npu_info("g2.g = %x\n", val);
		break;
	case COMMAND_DONE:
		interface.sfr->grp[3].g = 0xFFFFFFFF;
		val = interface.sfr->grp[3].g;
		//npu_info("g3.g = %x\n", val);
		break;
	default:
		break;
	}
}


static int npu_set_cmd(struct message *msg, struct command *cmd, u32 cmdType)
{
	int ret = 0;
	//int mbox_idx = 0;
	ret = mbx_ipc_put((void *)interface.addr, &interface.mbox_hdr->h2fctrl[cmdType], msg, cmd);
	if (ret)
		goto I_ERR;
	__send_interrupt(msg->command);
	/*
	switch(msg->command) {
	case COMMAND_LOAD:
	case COMMAND_UNLOAD:
	case COMMAND_PURGE:
	case COMMAND_POWERDOWN:
		mbox_idx = 0;
		break;
	case COMMAND_PROCESS:
		mbox_idx = 1;
		break;
	default:
		break;
	}
	*/
	Fake_FW(cmdType, msg->mid);
	return ret;
I_ERR:
	switch (ret) {
	case -ERESOURCE:
		npu_warn("No space left on mailbox : ret = %d\n", ret);
		break;
	default:
		npu_err("mbx_ipc_put err with %d\n", ret);
		break;
	}
	return ret;
}

static ssize_t get_ncp_hdr_size(const struct npu_nw *nw)
{
	struct ncp_header *ncp_header;

	BUG_ON(!nw);

	if (nw->ncp_addr.vaddr == NULL) {
		npu_err("not specified in ncp_addr.kvaddr");
		return -EINVAL;
	}

	ncp_header = (struct ncp_header *)nw->ncp_addr.vaddr;
	//dbg_print_ncp_header(ncp_header);
	if (ncp_header->magic_number1 != NCP_MAGIC1) {
		npu_info("invalid MAGIC of NCP header (0x%08x) at (%pK)", ncp_header->magic_number1, ncp_header);
		return -EINVAL;
	}
	return ncp_header->hdr_size;
}

int npu_interface_probe(struct device *dev, void *regs)
{
	int ret = 0;

	BUG_ON(!dev);
	BUG_ON(!irq2);
	BUG_ON(!irq3);

	interface.sfr = (volatile struct mailbox_sfr *)regs;
	mutex_init(&interface.lock);

	probe_info("complete in %s\n", __func__);

	return ret;
}

int npu_interface_open(struct npu_system *system)
{
	int ret = 0;
	unsigned int isr_cpu_affinity = 0;
	struct npu_device *device;

	ret = devm_request_irq(dev, system->irq0, mailbox_isr2, 0, "exynos-npu", NULL);
	if (ret) {
		probe_err("fail(%d) in devm_request_irq(2)\n", ret);
		goto err_exit;
	}
	ret = devm_request_irq(dev, system->irq1, mailbox_isr3, 0, "exynos-npu", NULL);
	if (ret) {
		probe_err("fail(%d) in devm_request_irq(3)\n", ret);
		goto err_probe_irq2;
	}
	ret = of_property_read_u32(dev->of_node,
		"samsung,npuinter-isr-cpu-affinity", &isr_cpu_affinity);
	if (ret)
		isr_cpu_affinity = 0;
	npu_info("set the CPU affinity of ISR to %u\n", isr_cpu_affinity);
	ret = irq_set_affinity_hint(system->irq0, cpumask_of(isr_cpu_affinity));
	if (ret)
		npu_warn("fail(%d) in irq_set_affinity_hint(irq0)\n", ret);
	ret = irq_set_affinity_hint(system->irq1, cpumask_of(isr_cpu_affinity));
	if (ret)
		npu_warn("fail(%d) in irq_set_affinity_hint(irq1)\n", ret);
	BUG_ON(!system);
	device = container_of(system, struct npu_device, system);
	interface.addr = (void *)((system->fw_npu_memory_buffer->vaddr) + configs[NPU_MAILBOX_BASE]);//[BAE] : need chk
	interface.mbox_hdr = system->mbox_hdr;

	ret = mailbox_init(interface.mbox_hdr);
	if (ret) {
		npu_err("error(%d) in npu_mailbox_init\n", ret);
		goto err_exit;
	}
	return ret;

err_probe_irq2:
	devm_free_irq(dev, system->irq0, NULL);
err_exit:
	interface.addr = NULL;
	interface.mbox_hdr = NULL;
	devm_free_irq(dev, system->irq0, NULL);
	devm_free_irq(dev, system->irq1, NULL);
	set_bit(NPU_DEVICE_ERR_STATE_EMERGENCY, &device->err_state);
	npu_err("EMERGENCY_RECOVERY is triggered.\n");
	return ret;

}
int npu_interface_close(struct npu_system *system)
{
	struct device *dev = &system->pdev->dev;

	interface.addr = NULL;
	interface.mbox_hdr = NULL;
	devm_free_irq(dev, system->irq0, NULL);
	devm_free_irq(dev, system->irq1, NULL);
	return 0;
}

int register_rslt_notifier(protodrv_notifier func)
{
	interface.rslt_notifier = func;
	return 0;
}

int register_msgid_get_type(int (*msgid_get_type_func)(int))
{
	interface.msgid_get_type = msgid_get_type_func;
	return 0;
}

int nw_req_manager(int msgid, struct npu_nw *nw)
{
	int ret = 0;
	struct command cmd = {};
	struct message msg = {};
	ssize_t hdr_size;

	switch (nw->cmd) {
	case NPU_NW_CMD_BASE:
		npu_info("abnormal command type\n");
		break;
	case NPU_NW_CMD_LOAD:
		cmd.c.load.oid = nw->uid;
		cmd.c.load.tid = configs[NPU_MAILBOX_DEFAULT_TID];
		cmd.c.load.tid = 0;
		hdr_size = get_ncp_hdr_size(nw);
		if (hdr_size <= 0) {
			npu_info("fail in get_ncp_hdr_size: (%zd)", hdr_size);
			ret = FALSE;
			goto nw_req_err;
		}

		cmd.length = (u32)hdr_size;
		cmd.payload = nw->ncp_addr.daddr;
		msg.command = COMMAND_LOAD;
		msg.length = sizeof(struct command);
		break;
	case NPU_NW_CMD_UNLOAD:
		cmd.c.unload.oid = nw->uid;//NPU_MAILBOX_DEFAULT_OID;
		cmd.payload = 0;
		cmd.length = 0;
		msg.command = COMMAND_UNLOAD;
		msg.length = sizeof(struct command);
		break;
	case NPU_NW_CMD_PROFILE_START:
		cmd.c.profile_ctl.ctl = PROFILE_CTL_CODE_START;
		cmd.payload = nw->ncp_addr.daddr;
		cmd.length = nw->ncp_addr.size;
		msg.command = COMMAND_PROFILE_CTL;
		msg.length = sizeof(struct command);
		break;
	case NPU_NW_CMD_PROFILE_STOP:
		cmd.c.profile_ctl.ctl = PROFILE_CTL_CODE_STOP;
		cmd.payload = 0;
		cmd.length = 0;
		msg.command = COMMAND_PROFILE_CTL;
		msg.length = sizeof(struct command);
		break;
	case NPU_NW_CMD_STREAMOFF:
		cmd.c.purge.oid = nw->uid;
		cmd.payload = 0;
		msg.command = COMMAND_PURGE;
		msg.length = sizeof(struct command);
		break;
	case NPU_NW_CMD_POWER_DOWN:
		cmd.c.powerdown.dummy = nw->uid;
		cmd.payload = 0;
		msg.command = COMMAND_POWERDOWN;
		msg.length = sizeof(struct command);
		break;
	case NPU_NW_CMD_FW_TC_EXECUTE:
		cmd.c.fw_test.param = nw->param0;
		cmd.payload = 0;
		cmd.length = 0;
		msg.command = COMMAND_FW_TEST;
		msg.length = sizeof(struct command);
		break;
	case NPU_NW_CMD_END:
		break;
	default:
		npu_err("invalid CMD ID: (%db)", nw->cmd);
		ret = FALSE;
		goto nw_req_err;
	}
	msg.mid = msgid;

	ret = npu_set_cmd(&msg, &cmd, NPU_MBOX_REQUEST_LOW);
	if (ret)
		goto nw_req_err;

	//mbx_ipc_print_dbg((void *)interface.addr, &interface.mbox_hdr->h2fctrl[0]);
	return TRUE;
nw_req_err:
	return ret;
}

int fr_req_manager(int msgid, struct npu_frame *frame)
{
	int ret = FALSE;
	struct command cmd = {};
	struct message msg = {};

	switch (frame->cmd) {
	case NPU_FRAME_CMD_BASE:
		break;
	case NPU_FRAME_CMD_Q:
		cmd.c.process.oid = frame->uid;
		cmd.c.process.fid = frame->frame_id;
		cmd.length = frame->mbox_process_dat.address_vector_cnt;
		cmd.payload = frame->mbox_process_dat.address_vector_start_daddr;
		msg.command = COMMAND_PROCESS;
		msg.length = sizeof(struct command);
		break;
	case NPU_FRAME_CMD_END:
		break;
	}
	msg.mid = msgid;
	ret = npu_set_cmd(&msg, &cmd, NPU_MBOX_REQUEST_HIGH);
	if (ret)
		goto fr_req_err;

	//mbx_ipc_print_dbg((void *)interface.addr, &interface.mbox_hdr->h2fctrl[1]);
	return TRUE;
fr_req_err:
	return ret;
}

int nw_rslt_manager(int *ret_msgid, struct npu_nw *nw)
{
	int ret;
	struct message msg;
	struct command cmd;

	//[BAE] Alternative is needed
	ret = mbx_ipc_peek_msg((void *)interface.addr, &interface.mbox_hdr->f2hctrl[0], &msg);
	if (ret <= 0)
		return FALSE;

	ret = interface.msgid_get_type(msg.mid);
	if (ret != PROTO_DRV_REQ_TYPE_NW)
		return FALSE;//report error content:

	ret = mbx_ipc_get_msg((void *)interface.addr, &interface.mbox_hdr->f2hctrl[0], &msg);
	if (ret <= 0)
		return FALSE;

	ret = mbx_ipc_get_cmd((void *)interface.addr, &interface.mbox_hdr->f2hctrl[0], &msg, &cmd);
	if (ret) {
		npu_err("get command error\n");
		return FALSE;
	}

	if (msg.command == COMMAND_DONE) {
		npu_info("COMMAND_DONE for mid: (%d), val(0x%x)\n", msg.mid,
			 cmd.c.done.request_specific_value);
		nw->result_code = NPU_ERR_NO_ERROR;
		nw->result_value = cmd.c.done.request_specific_value;
	} else if (msg.command == COMMAND_NDONE) {
		npu_err("COMMAND_NDONE for mid: (%d) error(%u/0x%08x)\n"
			, msg.mid, cmd.c.ndone.error, cmd.c.ndone.error);
		nw->result_code = cmd.c.ndone.error;
	} else {
		npu_err("invalid msg.command: (%d)\n", msg.command);
		return FALSE;
	}
	fw_rprt_manager();
	*ret_msgid = msg.mid;
	return TRUE;
}

int fr_rslt_manager(int *ret_msgid, struct npu_frame *frame)
{

	int ret = FALSE;
	struct message msg;
	struct command cmd;

	ret = mbx_ipc_peek_msg((void *)interface.addr, &interface.mbox_hdr->f2hctrl[0], &msg);
	if (ret <= 0)
		return FALSE;

	ret = interface.msgid_get_type(msg.mid);
	if (ret != PROTO_DRV_REQ_TYPE_FRAME) {
		//npu_info("get type error: (%d)\n", ret);
		return FALSE;
	}

	ret = mbx_ipc_get_msg((void *)interface.addr, &interface.mbox_hdr->f2hctrl[0], &msg);
	if (ret <= 0)// 0 : no msg, less than zero : Err
		return FALSE;

	ret = mbx_ipc_get_cmd((void *)interface.addr, &interface.mbox_hdr->f2hctrl[0], &msg, &cmd);
	if (ret)
		return FALSE;

	if (msg.command == COMMAND_DONE) {
		npu_info("COMMAND_DONE for mid: (%d)\n", msg.mid);
		frame->result_code = NPU_ERR_NO_ERROR;
	} else if (msg.command == COMMAND_NDONE) {
		npu_err("COMMAND_NDONE for mid: (%d) error(%u/0x%08x)\n"
			, msg.mid, cmd.c.ndone.error, cmd.c.ndone.error);
		frame->result_code = cmd.c.ndone.error;
	} else {
		npu_err("invalid msg.command: (%d)\n", msg.command);
		return FALSE;
	}
	*ret_msgid = msg.mid;
	fw_rprt_manager();
	return TRUE;
}

//Print log which was written with last 128 byte.
int npu_check_unposted_mbox(int nCtrl)
{
	int ret = 0;
	return ret;
}
/*
static void __rprt_manager(struct work_struct *w)
{
	return;

}
*/
void fw_rprt_manager(void)
{
	return;
}

int mbx_rslt_fault_listener(void)
{
	return 0;
}

int nw_rslt_available(void)
{
	int ret;
	struct message msg;
	ret = mbx_ipc_peek_msg(
		(void *)interface.addr,
		&interface.mbox_hdr->f2hctrl[0], &msg);
	//npu_info("[BAE] nw_rslt_available with addr : %p, ret = %d\n", (void*)interface.addr, ret);
	return ret;
}
int fr_rslt_available(void)
{
	int ret;
	struct message msg;

	ret = mbx_ipc_peek_msg(
		(void *)interface.addr,
		&interface.mbox_hdr->f2hctrl[0], &msg);
	//npu_info("[BAE] fr_rslt_available with ret : %d\n", ret);
	return ret;
}
/* Unit test */
#ifdef CONFIG_VISION_UNITTEST
#define IDIOT_TESTCASE_IMPL "npu-interface.idiot"
#include "idiot-def.h"
#endif
