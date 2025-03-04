/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Kimyung Lee <kernel.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_SPI_DEVICE_H__
#define __PANEL_SPI_DEVICE_H__

#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/bitops.h>
#include "panel.h"

#define DRIVER_NAME "panel_spi"
#define PANEL_SPI_DRIVER_NAME "panel_spi_driver"
#define MAX_PANEL_SPI_RX_BUF	(2048)
#define MAX_PANEL_SPI_RX_DATA	(MAX_PANEL_SPI_RX_BUF)
#define MAX_PANEL_SPI_TX_BUF	(260)
#define MAX_PANEL_SPI_TX_DATA	(256)
#define MAX_ADDRESSING_BYTE		(4)

#define MAX_CMD_RETRY			(5)
#define MAX_STATUS_RETRY		(10)
#define MAX_BUSY_RETRY			(50)

enum {
	PANEL_SPI_CTRL_DATA_RX = 0,
	PANEL_SPI_CTRL_DATA_TX = 1,
	PANEL_SPI_CTRL_RESET = 2,
	PANEL_SPI_CTRL_CMD = 3,
	PANEL_SPI_STATUS_1 = 4,
	PANEL_SPI_STATUS_2 = 5,
	PANEL_SPI_UNLOCK = 6,
	PANEL_SPI_LOCK = 7,
	PANEL_SPI_WRITE_ENABLE = 8,
};

enum {
	PANEL_SPI_CMD_OPTION_NONE = BIT(0),
	PANEL_SPI_CMD_OPTION_READ_COMPARE = BIT(1),
	PANEL_SPI_CMD_OPTION_ADDR_3BYTE = BIT(2),
	PANEL_SPI_CMD_OPTION_ADDR_4BYTE = BIT(3),
	PANEL_SPI_CMD_OPTION_ONLY_DELAY = BIT(4),
};

enum {
	PANEL_SPI_CMD_NONE,
	PANEL_SPI_CMD_FLASH_INIT1,
	PANEL_SPI_CMD_FLASH_INIT1_DONE,
	PANEL_SPI_CMD_FLASH_INIT2,
	PANEL_SPI_CMD_FLASH_INIT2_DONE,
	PANEL_SPI_CMD_FLASH_EXIT1,
	PANEL_SPI_CMD_FLASH_EXIT1_DONE,
	PANEL_SPI_CMD_FLASH_EXIT2,
	PANEL_SPI_CMD_FLASH_EXIT2_DONE,
	PANEL_SPI_CMD_FLASH_BUSY_CLEAR,
	PANEL_SPI_CMD_FLASH_READ,
	PANEL_SPI_CMD_FLASH_WRITE_ENABLE,
	PANEL_SPI_CMD_FLASH_WRITE_DISABLE,
	PANEL_SPI_CMD_FLASH_WRITE,
	PANEL_SPI_CMD_FLASH_WRITE_DONE,
	PANEL_SPI_CMD_FLASH_ERASE_4K,
	PANEL_SPI_CMD_FLASH_ERASE_4K_DONE,
	PANEL_SPI_CMD_FLASH_ERASE_32K,
	PANEL_SPI_CMD_FLASH_ERASE_32K_DONE,
	PANEL_SPI_CMD_FLASH_ERASE_64K,
	PANEL_SPI_CMD_FLASH_ERASE_64K_DONE,
	PANEL_SPI_CMD_FLASH_ERASE_256K,
	PANEL_SPI_CMD_FLASH_ERASE_256K_DONE,
	PANEL_SPI_CMD_FLASH_ERASE_CHIP,
	PANEL_SPI_CMD_FLASH_ERASE_CHIP_DONE,
	MAX_PANEL_SPI_CMD,
};

enum {
	PANEL_SPI_ERASE_TYPE_BLOCK,
	PANEL_SPI_ERASE_TYPE_CHIP,
	MAX_PANEL_SPI_ERASE,
};

enum {
	PANEL_SPI_CTRL_NONE,
	PANEL_SPI_CTRL_INIT,
	PANEL_SPI_CTRL_EXIT,
	PANEL_SPI_CTRL_ID_READ,
	PANEL_SPI_CTRL_BUSY_CHECK,
	PANEL_SPI_CTRL_GET_READ_SIZE,
	PANEL_SPI_CTRL_GET_WRITE_SIZE,
	MAX_PANEL_SPI_CTRL,
};

enum {
	PANEL_SPI_GET_READ_SIZE,
	PANEL_SPI_GET_WRITE_SIZE,
};

enum {
	PANEL_SPI_PACKET_TYPE_ERASE_NONBLOCK = 0x1,
};

struct panel_spi_dev;

struct spi_cmd {
	int reg;
	int opt;
	int addr;
	int wlen;
	u8 *wval;
	int rlen;
	u8 *rval;
	u8 *rmask;
	u32 delay_before_usecs;
	u32 delay_after_usecs;
	u32 delay_retry_usecs;
	int wait_response_bytes;
};

// spi_data_packet : for read/write/erase func
struct spi_data_packet {
	int type;
	u32 addr;
	u32 size;
	u8 *buf;
};

struct spi_drv_ops {
	int (*ctl)(struct panel_spi_dev *spi_dev, int msg, void *data);
	int (*cmd)(struct panel_spi_dev *spi_dev, const u8 *wbuf, int wsize, u8 *rbuf, int rsize);
	int (*erase)(struct panel_spi_dev *spi_dev, struct spi_data_packet *data_buf);
	int (*read)(struct panel_spi_dev *spi_dev, struct spi_data_packet *data_buf);
	int (*write)(struct panel_spi_dev *spi_dev, struct spi_data_packet *data_buf);
	int (*init)(struct panel_spi_dev *spi_dev);
	int (*exit)(struct panel_spi_dev *spi_dev);
	int (*get_buf_size)(struct panel_spi_dev *spi_dev, int type);
};

//deprecated: accessing via common_panel_driver
struct spi_drv_pdrv_ops {
	int (*pdrv_read)(struct panel_spi_dev *spi_dev, const u8 cmd, u8 *buf, int size);
	int (*pdrv_cmd)(struct panel_spi_dev *spi_dev, const u8 *wbuf, int wsize, u8 *rbuf, int rsize);
	int (*pdrv_read_param)(struct panel_spi_dev *spi_dev, const u8 *wbuf, int wsize);
};

struct spi_dev_info {
	bool ready;
	u32 id;
	u32 size;
	u32 speed_hz;
	int erase_type;
	int byte_per_write;
	int byte_per_read;
	char *vendor;
	char *model;
	struct spi_cmd **cmd_list;
};

struct spi_data {
	int spi_addr;
	u32 size;
	u32 speed_hz;
	u32 compat_mask;
	u32 compat_id;
	int erase_type;
	int byte_per_write;
	int byte_per_read;
	char *vendor;
	char *model;
	struct spi_cmd **cmd_list;
};

struct panel_spi_dev {
	struct miscdevice dev;
	struct spi_drv_ops *ops;
	struct spi_drv_pdrv_ops *pdrv_ops;		//accessing via common_panel_driver
	struct spi_device *spi;
	struct spi_dev_info spi_info;
	u8 *setparam_buffer;
	u32 setparam_buffer_size;
	u8 *read_buf_data;
	u8 read_buf_cmd;
	u32 read_buf_size;

	//for file ops
	struct mutex f_lock;
	bool fopened;
	bool auto_erase;
};

struct ioc_erase_info {
	u32 offset;
	u32 length;
};

#define PANEL_SPI_IOCTL_MAGIC	'S'

#define IOCTL_AUTO_ERASE_ENABLE		_IO(PANEL_SPI_IOCTL_MAGIC, 1)
#define IOCTL_AUTO_ERASE_DISABLE	_IO(PANEL_SPI_IOCTL_MAGIC, 2)
#define IOCTL_CHECK_STATE			_IOR(PANEL_SPI_IOCTL_MAGIC, 3, int)
#define IOCTL_ERASE			_IOW(PANEL_SPI_IOCTL_MAGIC, 4, struct ioc_erase_info)

int panel_spi_drv_probe(struct panel_device *panel, struct spi_data **spi_data_tbl, int nr_spi_data_tbl);
int panel_spi_drv_remove(struct panel_device *panel);

static inline bool SPI_IS_READY(struct panel_spi_dev *dev)
{
	return dev->spi_info.ready;
}

#endif

