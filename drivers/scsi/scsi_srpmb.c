/*
 * Secure RPMB Driver for Exynos scsi rpmb
 *
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/smc.h>
#include <linux/pm_wakeup.h>
#include <soc/samsung/exynos-smc.h>

#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_ioctl.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_proto.h>

#include "ufs/ufshcd.h"
#include "ufs/ufs-exynos.h"

#include "scsi_srpmb.h"
#include "scsi_logging.h"
#include "sd.h"

#define SRPMB_DEVICE_PROPNAME	"samsung,ufs-srpmb"
#define SDA_BLOCK_NAME		"/dev/block/sda"

struct platform_device *sr_pdev;

#if defined(DEBUG_SRPMB)
static void dump_packet(u8 *data, int len)
{
	u8 s[17];
	int i, j;

	s[16] = '\0';
	for (i = 0; i < len; i += 16) {
		printk("%06x :", i);
		for (j = 0; j < 16; j++) {
			printk(" %02x", data[i+j]);
			s[j] = (data[i+j] < ' ' ? '.' : (data[i+j] > '}' ? '.' : data[i+j]));
		}
		printk(" |%s|\n", s);
	}
	printk("\n");
}
#endif

static void swap_packet(u8 *p, u8 *d)
{
	int i;

	for (i = 0; i < RPMB_PACKET_SIZE; i++)
		d[i] = p[RPMB_PACKET_SIZE - 1 - i];
}

static struct scsi_disk *srpmb_scsi_disk_get(struct gendisk *disk)
{
	struct scsi_disk *sdkp = NULL;

	if (disk->private_data) {
		sdkp = scsi_disk(disk);
		if (scsi_device_get(sdkp->device) == 0)
			get_device(&sdkp->dev);
		else
			sdkp = NULL;
	}
	return sdkp;
}

static int rpmb_send_request_sense(struct scsi_device *sdev)
{
	unsigned char cmd[6] = {REQUEST_SENSE,
				0,
				0,
				0,
				UFS_SENSE_SIZE,
				0};
	char *buffer;
	int ret;

	buffer = kzalloc(UFS_SENSE_SIZE, GFP_KERNEL);
	if (!buffer) {
		ret = -ENOMEM;
		goto out;
	}

	ret = scsi_execute(sdev, cmd, DMA_FROM_DEVICE, buffer,
					UFS_SENSE_SIZE, NULL, NULL,
					msecs_to_jiffies(1000), 3, 0, RQF_PM, NULL);
	if (ret)
		pr_err("%s: failed with err %d\n", __func__, ret);

	kfree(buffer);
out:
	return ret;
}

static inline u16 ufshcd_upiu_wlun_to_scsi_wlun(u8 upiu_wlun_id)
{
	return (upiu_wlun_id & ~UFS_UPIU_WLUN_ID) | SCSI_W_LUN_BASE;
}

static void update_rpmb_status_flag(struct rpmb_irq_ctx *ctx,
				Rpmb_Req *req, int status)
{
	unsigned long flags;

	spin_lock_irqsave(&ctx->lock, flags);
	req->status_flag = status;
	spin_unlock_irqrestore(&ctx->lock, flags);
}

static int srpmb_ioctl_secu_prot_command(struct scsi_device *sdev, char *cmd,
					Rpmb_Req *req, struct scsi_sense_hdr *sshdr,
					int timeout, int retries)
{
	int result, dma_direction;
	unsigned char *buf = NULL;
	unsigned int bufflen;
	int prot_in_out = req->cmd;

	SCSI_LOG_IOCTL(1, printk("Trying ioctl with scsi command %d\n", *cmd));
	if (prot_in_out == SCSI_IOCTL_SECURITY_PROTOCOL_IN) {
		dma_direction = DMA_FROM_DEVICE;
		bufflen = req->inlen;
		if (bufflen <= 0 || bufflen > MAX_BUFFLEN) {
			sdev_printk(KERN_INFO, sdev,
					"Invalid bufflen : %x\n", bufflen);
			result = -EFAULT;
			goto err_pre_buf_alloc;
		}
		buf = kzalloc(bufflen, GFP_KERNEL);
		if (!virt_addr_valid(buf)) {
			result = -ENOMEM;
			goto err_kzalloc;
		}
	} else if (prot_in_out == SCSI_IOCTL_SECURITY_PROTOCOL_OUT) {
		dma_direction = DMA_TO_DEVICE;
		bufflen = req->outlen;
		if (bufflen <= 0 || bufflen > MAX_BUFFLEN) {
			sdev_printk(KERN_INFO, sdev,
					"Invalid bufflen : %x\n", bufflen);
			result = -EFAULT;
			goto err_pre_buf_alloc;
		}
		buf = kzalloc(bufflen, GFP_KERNEL);
		if (!virt_addr_valid(buf)) {
			result = -ENOMEM;
			goto err_kzalloc;
		}
		memcpy(buf, req->rpmb_data, bufflen);
	} else {
		sdev_printk(KERN_INFO, sdev,
				"prot_in_out not set!! %d\n", prot_in_out);
		result = -EFAULT;
		goto err_pre_buf_alloc;
	}

	result = scsi_execute_req(sdev, cmd, dma_direction, buf, bufflen,
				  sshdr, timeout, retries, NULL);

	if (prot_in_out == SCSI_IOCTL_SECURITY_PROTOCOL_IN) {
		memcpy(req->rpmb_data, buf, bufflen);
	}
	SCSI_LOG_IOCTL(2, printk("Ioctl returned  0x%x\n", result));

	if ((driver_byte(result) & DRIVER_SENSE) &&
	    (scsi_sense_valid(sshdr))) {
		sdev_printk(KERN_INFO, sdev,
			    "ioctl_secu_prot_command return code = %x\n",
			    result);
		scsi_print_sense_hdr(sdev, NULL, sshdr);
	}

	kfree(buf);
err_pre_buf_alloc:
	SCSI_LOG_IOCTL(2, printk("IOCTL Releasing command\n"));
	return result;
err_kzalloc:
	if (buf)
		kfree(buf);
	printk(KERN_INFO "%s kzalloc faild\n", __func__);
	return result;
}

int srpmb_scsi_ioctl(struct scsi_device *sdev, Rpmb_Req *req)
{
	char scsi_cmd[MAX_COMMAND_SIZE];
	unsigned short prot_spec;
	unsigned long t_len;
	struct scsi_target *starget_rpmb;
	static struct scsi_device *sdev_rpmb = NULL;
	struct scsi_sense_hdr sshdr;
	int ret, count;

	if (!sdev) {
		printk(KERN_ERR "sdev empty\n");
		return -ENXIO;
	}

	memset(scsi_cmd, 0x0, MAX_COMMAND_SIZE);
	/*
	 * If we are in the middle of error recovery, don't let anyone
	 * else try and use this device. Also, if error recovery fails, it
	 * may try and take the device offline, in which case all further
	 * access to the device is prohibited.
	 */
	if (!scsi_block_when_processing_errors(sdev))
		return -ENODEV;

	//Get RPMB's scsi_device by accessing scsi_target.
	if (sdev_rpmb == NULL) {
		starget_rpmb = scsi_target(sdev);
		if (starget_rpmb == NULL) {
			dev_err(&sr_pdev->dev, "getting starget_rpmb failed\n");
			return -ENXIO;
		}
		sdev_rpmb = __scsi_device_lookup_by_target(starget_rpmb, ufshcd_upiu_wlun_to_scsi_wlun(UFS_UPIU_RPMB_WLUN));
		if (sdev_rpmb == NULL) {
			dev_err(&sr_pdev->dev, "getting sdev_rpmb failed\n");
			return -ENXIO;
		}
	}

	if (exynos_ufs_srpmb_get_wlun_uac()) {
		ret = rpmb_send_request_sense(sdev_rpmb);
		if (ret) {
			dev_err(&sr_pdev->dev, "%s clearing uac failed: %d\n", __func__);
			return -EIO;
		}
		exynos_ufs_srpmb_set_wlun_uac(false);
	}

	prot_spec = SECU_PROT_SPEC_CERT_DATA;
	if (req->cmd == SCSI_IOCTL_SECURITY_PROTOCOL_IN)
		t_len = req->inlen;
	else
		t_len = req->outlen;

	scsi_cmd[0] = (req->cmd == SCSI_IOCTL_SECURITY_PROTOCOL_IN) ?
		SECURITY_PROTOCOL_IN :
		SECURITY_PROTOCOL_OUT;
	scsi_cmd[1] = SECU_PROT_UFS;
	scsi_cmd[2] = ((unsigned char)(prot_spec >> 8)) & 0xff;
	scsi_cmd[3] = ((unsigned char)(prot_spec)) & 0xff;
	scsi_cmd[4] = 0;
	scsi_cmd[5] = 0;
	scsi_cmd[6] = ((unsigned char)(t_len >> 24)) & 0xff;
	scsi_cmd[7] = ((unsigned char)(t_len >> 16)) & 0xff;
	scsi_cmd[8] = ((unsigned char)(t_len >> 8)) & 0xff;
	scsi_cmd[9] = (unsigned char)t_len & 0xff;
	scsi_cmd[10] = 0;
	scsi_cmd[11] = 0;

	/* Retry when UAC occurs */
	for (count = 0; count < MAX_RETRY; count++) {
		ret = srpmb_ioctl_secu_prot_command(sdev_rpmb, scsi_cmd,
			req, &sshdr,
			START_STOP_TIMEOUT, NORMAL_RETRIES);

		if (sshdr.sense_key == UNIT_ATTENTION)
			dev_warn(&sr_pdev->dev, "RPMB UAC detected: Retry! (count = %d)\n", count + 1);
		else
			break;
	}

	return ret;
}

static void srpmb_worker(struct work_struct *data)
{
	int ret;
	struct rpmb_packet packet;
	struct rpmb_irq_ctx *rpmb_ctx;
	struct scsi_device *sdp;
	Rpmb_Req *req;
	static struct block_device *bdev = NULL;
	static struct scsi_disk *sdkp = NULL;

	if (!data) {
		dev_err(&sr_pdev->dev, "rpmb work_struct data invalid\n");
		return;
	}
	rpmb_ctx = container_of(data, struct rpmb_irq_ctx, work);
	if (!rpmb_ctx->dev) {
		dev_err(&sr_pdev->dev, "rpmb_ctx->dev invalid\n");
		return;
	}

	if (bdev == NULL) {
		bdev = blkdev_get_by_path(SDA_BLOCK_NAME, FMODE_READ, NULL);
		if (IS_ERR(bdev)) {
			dev_err(&sr_pdev->dev, "Fail to get sda lock device\n");
			bdev = NULL;
			return;
		}
	}

	if (sdkp == NULL) {
		sdkp = srpmb_scsi_disk_get(bdev->bd_disk);
		if (sdkp == NULL) {
			dev_err(&sr_pdev->dev, "Fail to get sdkp scsi device\n");
			return;
		}
	}

	sdp = sdkp->device;
	if (!rpmb_ctx->vir_addr) {
		dev_err(&sr_pdev->dev, "rpmb_ctx->vir_addr invalid\n");
		return;
	}
	req = (Rpmb_Req *)rpmb_ctx->vir_addr;

	__pm_stay_awake(&rpmb_ctx->wakesrc);

	dev_dbg(&sr_pdev->dev, "start rpmb workqueue with command(%d)\n", req->type);

	switch (req->type) {
	case GET_WRITE_COUNTER:
		if (req->data_len != RPMB_PACKET_SIZE) {
			update_rpmb_status_flag(rpmb_ctx, req,
					WRITE_COUTNER_DATA_LEN_ERROR);
			dev_err(&sr_pdev->dev, "data len is invalid\n");
			break;
		}

		req->cmd = SCSI_IOCTL_SECURITY_PROTOCOL_OUT;
		req->outlen = RPMB_PACKET_SIZE;

		ret = srpmb_scsi_ioctl(sdp, req);
		if (ret < 0) {
			update_rpmb_status_flag(rpmb_ctx, req,
					WRITE_COUTNER_SECURITY_OUT_ERROR);
			dev_err(&sr_pdev->dev, "ioctl read_counter error: %x\n", ret);
			break;
		}

		memset(req->rpmb_data, 0x0, req->data_len);
		req->cmd = SCSI_IOCTL_SECURITY_PROTOCOL_IN;
		req->inlen = req->data_len;

		ret = srpmb_scsi_ioctl(sdp, req);
		if (ret < 0) {
			update_rpmb_status_flag(rpmb_ctx, req,
					WRITE_COUTNER_SECURITY_IN_ERROR);
			dev_err(&sr_pdev->dev, "ioctl error : %x\n", ret);
			break;
		}
		if (req->rpmb_data[RPMB_RESULT] || req->rpmb_data[RPMB_RESULT+1]) {
			dev_info(&sr_pdev->dev, "GET_WRITE_COUNTER: REQ/RES = %02x%02x, RESULT = %02x%02x\n",
				req->rpmb_data[RPMB_REQRES], req->rpmb_data[RPMB_REQRES+1],
				req->rpmb_data[RPMB_RESULT], req->rpmb_data[RPMB_RESULT+1]);
		}

		update_rpmb_status_flag(rpmb_ctx, req, RPMB_PASSED);

		break;
	case WRITE_DATA:
		if (req->data_len < RPMB_PACKET_SIZE ||
			req->data_len > RPMB_PACKET_SIZE * 64) {
			update_rpmb_status_flag(rpmb_ctx, req,
					WRITE_DATA_LEN_ERROR);
			dev_err(&sr_pdev->dev, "data len is invalid\n");
			break;
		}

		req->cmd = SCSI_IOCTL_SECURITY_PROTOCOL_OUT;
		req->outlen = req->data_len;

		ret = srpmb_scsi_ioctl(sdp, req);
		if (ret < 0) {
			update_rpmb_status_flag(rpmb_ctx, req,
					WRITE_DATA_SECURITY_OUT_ERROR);
			dev_err(&sr_pdev->dev, "ioctl write data error: %x\n", ret);
			break;
		}

		memset(req->rpmb_data, 0x0, req->data_len);
		memset(&packet, 0x0, RPMB_PACKET_SIZE);
		packet.request = RESULT_READ_REQ;
		swap_packet((uint8_t *)&packet, req->rpmb_data);
		req->cmd = SCSI_IOCTL_SECURITY_PROTOCOL_OUT;
		req->outlen = RPMB_PACKET_SIZE;

		ret = srpmb_scsi_ioctl(sdp, req);
		if (ret < 0) {
			update_rpmb_status_flag(rpmb_ctx, req,
					WRITE_DATA_RESULT_SECURITY_OUT_ERROR);
			dev_err(&sr_pdev->dev,
					"ioctl write_data result error: %x\n", ret);
			break;
		}

		memset(req->rpmb_data, 0x0, req->data_len);
		req->cmd = SCSI_IOCTL_SECURITY_PROTOCOL_IN;
		req->inlen = RPMB_PACKET_SIZE;

		ret = srpmb_scsi_ioctl(sdp, req);
		if (ret < 0) {
			update_rpmb_status_flag(rpmb_ctx, req,
					WRITE_DATA_SECURITY_IN_ERROR);
			dev_err(&sr_pdev->dev,
					"ioctl write_data result error: %x\n", ret);
			break;
		}
		if (req->rpmb_data[RPMB_RESULT] || req->rpmb_data[RPMB_RESULT+1]) {
			dev_info(&sr_pdev->dev, "WRITE_DATA: REQ/RES = %02x%02x, RESULT = %02x%02x\n",
				req->rpmb_data[RPMB_REQRES], req->rpmb_data[RPMB_REQRES+1],
				req->rpmb_data[RPMB_RESULT], req->rpmb_data[RPMB_RESULT+1]);
		}

		update_rpmb_status_flag(rpmb_ctx, req, RPMB_PASSED);

		break;
	case READ_DATA:
		if (req->data_len < RPMB_PACKET_SIZE ||
			req->data_len > RPMB_PACKET_SIZE * 64) {
			update_rpmb_status_flag(rpmb_ctx, req, READ_LEN_ERROR);
			dev_err(&sr_pdev->dev, "data len is invalid\n");
			break;
		}

		req->cmd = SCSI_IOCTL_SECURITY_PROTOCOL_OUT;
		req->outlen = RPMB_PACKET_SIZE;

		ret = srpmb_scsi_ioctl(sdp, req);
		if (ret < 0) {
			update_rpmb_status_flag(rpmb_ctx, req,
					READ_DATA_SECURITY_OUT_ERROR);
			dev_err(&sr_pdev->dev, "ioctl read data error: %x\n", ret);
			break;
		}

		memset(req->rpmb_data, 0x0, req->data_len);
		req->cmd = SCSI_IOCTL_SECURITY_PROTOCOL_IN;
		req->inlen = req->data_len;

		ret = srpmb_scsi_ioctl(sdp, req);
		if (ret < 0) {
			update_rpmb_status_flag(rpmb_ctx, req,
					READ_DATA_SECURITY_IN_ERROR);
			dev_err(&sr_pdev->dev,
					"ioctl result read data error : %x\n", ret);
			break;
		}
		if (req->rpmb_data[RPMB_RESULT] || req->rpmb_data[RPMB_RESULT+1]) {
			dev_info(&sr_pdev->dev, "READ_DATA: REQ/RES = %02x%02x, RESULT = %02x%02x\n",
				req->rpmb_data[RPMB_REQRES], req->rpmb_data[RPMB_REQRES+1],
				req->rpmb_data[RPMB_RESULT], req->rpmb_data[RPMB_RESULT+1]);
		}

		update_rpmb_status_flag(rpmb_ctx, req, RPMB_PASSED);

		break;
	default:
		dev_err(&sr_pdev->dev, "invalid requset type : %x\n", req->type);
	}

	__pm_relax(&rpmb_ctx->wakesrc);
	dev_dbg(&sr_pdev->dev, "finish rpmb workqueue with command(%d)\n", req->type);
}

static int srpmb_suspend_notifier(struct notifier_block *nb, unsigned long event,
				void *dummy)
{
	struct rpmb_irq_ctx *rpmb_ctx;
	struct device *dev;
	Rpmb_Req *req;

	if (!nb) {
		dev_err(&sr_pdev->dev, "noti_blk work_struct data invalid\n");
		return -1;
	}
	rpmb_ctx = container_of(nb, struct rpmb_irq_ctx, pm_notifier);
	dev = rpmb_ctx->dev;
	req = (Rpmb_Req *)rpmb_ctx->vir_addr;
	if (!req) {
		dev_err(dev, "Invalid wsm address for rpmb\n");
		return -EINVAL;
	}

	switch (event) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
	case PM_RESTORE_PREPARE:
		flush_workqueue(rpmb_ctx->srpmb_queue);
		if (req->status_flag != RPMB_PASSED)
			update_rpmb_status_flag(rpmb_ctx, req, RPMB_FAIL_SUSPEND_STATUS);
		break;
	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
		if (req->status_flag != RPMB_PASSED)
			update_rpmb_status_flag(rpmb_ctx, req, 0);
		break;
	default:
		break;
	}

	return 0;
}

static irqreturn_t rpmb_irq_handler(int intr, void *arg)
{
	struct rpmb_irq_ctx *rpmb_ctx = (struct rpmb_irq_ctx *)arg;
	struct device *dev;
	Rpmb_Req *req;

	dev = rpmb_ctx->dev;
	req = (Rpmb_Req *)rpmb_ctx->vir_addr;
	if (!req) {
		dev_err(dev, "Invalid wsm address for rpmb\n");
		return IRQ_HANDLED;
	}

	update_rpmb_status_flag(rpmb_ctx, req, RPMB_IN_PROGRESS);
	queue_work(rpmb_ctx->srpmb_queue, &rpmb_ctx->work);

	return IRQ_HANDLED;
}

int init_wsm(struct device *dev)
{
	int ret;
	unsigned long smc_ret;
	struct rpmb_irq_ctx *rpmb_ctx;
	struct irq_data *rpmb_irqd = NULL;
	irq_hw_number_t hwirq = 0;

	rpmb_ctx = kzalloc(sizeof(struct rpmb_irq_ctx), GFP_KERNEL);
	if (!rpmb_ctx) {
		dev_err(&sr_pdev->dev, "kzalloc failed\n");
		goto out_srpmb_ctx_alloc_fail;
	}

	/* buffer init */
	rpmb_ctx->vir_addr = dma_alloc_coherent(&sr_pdev->dev,
			sizeof(Rpmb_Req) + RPMB_BUF_MAX_SIZE,
			&rpmb_ctx->phy_addr, GFP_KERNEL);

	if (rpmb_ctx->vir_addr && rpmb_ctx->phy_addr) {
		dev_info(dev, "%s: srpmb: wsm initialized successfully\n", __func__);

		rpmb_ctx->irq = irq_of_parse_and_map(sr_pdev->dev.of_node, 0);
		if (rpmb_ctx->irq <= 0) {
			dev_err(&sr_pdev->dev, "No IRQ number, aborting\n");
			goto out_srpmb_init_fail;
		}

		/* Get irq_data for secure log */
		rpmb_irqd = irq_get_irq_data(rpmb_ctx->irq);
		if (!rpmb_irqd) {
			dev_err(&sr_pdev->dev, "Fail to get irq_data\n");
			goto out_srpmb_init_fail;
		}

		/* Get hardware interrupt number */
		hwirq = irqd_to_hwirq(rpmb_irqd);
		dev_dbg(&sr_pdev->dev, "hwirq for srpmb (%ld)\n", hwirq);

		rpmb_ctx->dev = dev;
		rpmb_ctx->srpmb_queue = alloc_workqueue("srpmb_wq",
				WQ_MEM_RECLAIM | WQ_UNBOUND | WQ_HIGHPRI, 1);
		if (!rpmb_ctx->srpmb_queue) {
			dev_err(&sr_pdev->dev,
				"Fail to alloc workqueue for ufs sprmb\n");
			goto out_srpmb_init_fail;
		}

		ret = request_irq(rpmb_ctx->irq, rpmb_irq_handler,
				IRQF_TRIGGER_RISING, sr_pdev->name, rpmb_ctx);
		if (ret) {
			dev_err(&sr_pdev->dev, "request irq failed: %x\n", ret);
			goto out_srpmb_init_fail;
		}

		rpmb_ctx->pm_notifier.notifier_call = srpmb_suspend_notifier;
		ret = register_pm_notifier(&rpmb_ctx->pm_notifier);
		if (ret) {
			dev_err(&sr_pdev->dev, "Failed to setup pm notifier\n");
			goto out_srpmb_free_irq_req;
		}

		memset(&rpmb_ctx->wakesrc, 0, sizeof(rpmb_ctx->wakesrc));
		(&rpmb_ctx->wakesrc)->name = "srpmb";
		wakeup_source_add(&rpmb_ctx->wakesrc);
		spin_lock_init(&rpmb_ctx->lock);
		INIT_WORK(&rpmb_ctx->work, srpmb_worker);

		smc_ret = exynos_smc(SMC_SRPMB_WSM, rpmb_ctx->phy_addr, hwirq, 0);
		if (smc_ret) {
			dev_err(&sr_pdev->dev, "wsm smc init failed: %x\n", smc_ret);
			goto out_srpmb_unregister_pm;
		}

	} else {
		dev_err(&sr_pdev->dev, "wsm dma alloc failed\n");
		goto out_srpmb_dma_alloc_fail;
	}

	return 0;

out_srpmb_unregister_pm:
	wakeup_source_remove(&rpmb_ctx->wakesrc);
	unregister_pm_notifier(&rpmb_ctx->pm_notifier);
out_srpmb_free_irq_req:
	free_irq(rpmb_ctx->irq, rpmb_ctx);
out_srpmb_init_fail:
	if (rpmb_ctx->srpmb_queue)
		destroy_workqueue(rpmb_ctx->srpmb_queue);

	dma_free_coherent(&sr_pdev->dev, sizeof(Rpmb_Req) + RPMB_BUF_MAX_SIZE,
			rpmb_ctx->vir_addr, rpmb_ctx->phy_addr);

out_srpmb_dma_alloc_fail:
	kfree(rpmb_ctx);

out_srpmb_ctx_alloc_fail:
	return -ENOMEM;
}

static int srpmb_probe(struct platform_device *pdev)
{
	int ret;

	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(36));
	dev_info(&pdev->dev, "srpmb_probe has been inited\n");

	sr_pdev = pdev;

	ret = init_wsm(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "srpmb init_wsm failed: %x\n", ret);
		return ret;
	}

	exynos_ufs_srpmb_set_wlun_uac(true);
	return 0;
}

static const struct of_device_id of_match_table[] = {
	{ .compatible = SRPMB_DEVICE_PROPNAME },
	{ }
};

static struct platform_driver srpmb_plat_driver = {
	.probe = srpmb_probe,
	.driver = {
		.name = "exynos-ufs-srpmb",
		.owner = THIS_MODULE,
		.of_match_table = of_match_table,
	}
};

static int __init srpmb_init(void)
{
	return platform_driver_register(&srpmb_plat_driver);
}

static void __exit srpmb_exit(void)
{
	platform_driver_unregister(&srpmb_plat_driver);
}

module_init(srpmb_init);
module_exit(srpmb_exit);

MODULE_AUTHOR("Yongtaek Kwon <ycool.kwon@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("UFS SRPMB driver");
