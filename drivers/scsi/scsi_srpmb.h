/*
 * Secure RPMB header for Exynos scsi rpmb
 *
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _SCSI_SRPMB_H
#define _SCSI_SRPMB_H

#include <linux/pm_wakeup.h>

#define GET_WRITE_COUNTER			1
#define WRITE_DATA				2
#define READ_DATA				3

#define AUTHEN_KEY_PROGRAM_RES			0x0100
#define AUTHEN_KEY_PROGRAM_REQ			0x0001
#define RESULT_READ_REQ				0x0005
#define RPMB_END_ADDRESS			0x4000

#define RPMB_PACKET_SIZE			512
#define RPMB_REQRES				510
#define RPMB_RESULT				508

#define WRITE_COUTNER_DATA_LEN_ERROR		0x601
#define WRITE_COUTNER_SECURITY_OUT_ERROR	0x602
#define WRITE_COUTNER_SECURITY_IN_ERROR		0x603
#define WRITE_DATA_LEN_ERROR			0x604
#define WRITE_DATA_SECURITY_OUT_ERROR		0x605
#define WRITE_DATA_RESULT_SECURITY_OUT_ERROR	0x606
#define WRITE_DATA_SECURITY_IN_ERROR		0x607
#define READ_LEN_ERROR				0x608
#define READ_DATA_SECURITY_OUT_ERROR		0x609
#define READ_DATA_SECURITY_IN_ERROR		0x60A
#define RPMB_INVALID_COMMAND			0x60B
#define RPMB_FAIL_SUSPEND_STATUS		0x60C

#define RPMB_IN_PROGRESS			0xDCDC
#define RPMB_PASSED				0xBABA

#define IS_INCLUDE_RPMB_DEVICE			"0:0:0:1"

#define ON					1
#define OFF					0

#define RPMB_BUF_MAX_SIZE			(64 * 1024)

#define SCSI_IOCTL_SECURITY_PROTOCOL_IN 	7
#define SCSI_IOCTL_SECURITY_PROTOCOL_OUT 	8

#define MAX_BUFFLEN				(64 * 512)
#define NORMAL_RETRIES				5
#define SECU_PROT_UFS				0xEC
#define SECU_PROT_SPEC_CERT_DATA		0x0001

#define SCSI_W_LUN_BASE 			0xc100
#define UFS_UPIU_MAX_UNIT_NUM_ID		0x7F
#define UFS_UPIU_WLUN_ID			(1 << 7)
#define UFS_SENSE_SIZE				18

#define MAX_RETRY				0x100

struct rpmb_irq_ctx {
	struct device *dev;
	int irq;
	u8 *vir_addr;
	dma_addr_t phy_addr;
	struct work_struct work;
	struct workqueue_struct *srpmb_queue;
	struct notifier_block pm_notifier;
	struct wakeup_source wakesrc;
	spinlock_t lock;
};

struct rpmb_packet {
	u16	request;
	u16	result;
	u16	count;
	u16	address;
	u32	write_counter;
	u8	nonce[16];
	u8	data[256];
	u8	Key_MAC[32];
	u8	stuff[196];
};

typedef struct rpmb_req {
	u32 cmd;
	volatile u32 status_flag;
	u32 type;
	u32 data_len;
	u32 inlen;
	u32 outlen;
	u8 rpmb_data[0];
} Rpmb_Req;

int srpmb_scsi_ioctl(struct scsi_device *, Rpmb_Req *req);

int init_wsm(struct device *dev);

extern void scsi_print_sense_hdr(const struct scsi_device *, const char *,
                                 const struct scsi_sense_hdr *);
extern int scsi_block_when_processing_errors(struct scsi_device *);

#endif /* _SCSI_SRPMB_H */

