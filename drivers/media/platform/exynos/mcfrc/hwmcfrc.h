/* include/uapi/linux/hwmcfrc.h
*
* Userspace API for Samsung MCFRC driver
*
* Copyright (c) 2020 Samsung Electronics Co., Ltd.
*		http://www.samsung.com
*
* Author: Igor Kovliga <i.kovliga@samsung.com>
*         Sergei Ilichev <s.ilichev@samsung.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#ifndef __HWMCFRC_H__
#define __HWMCFRC_H__

#include <linux/ioctl.h>
#include <linux/types.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

/*
* NOTE: THESE ENUMS ARE ALSO USED BY USERSPACE TO SPECIFY SETTINGS AND THEY'RE
*       SUPPOSED TO WORK WITH FUTURE HARDWARE AS WELL (WHICH MAY OR MAY NOT
*       SUPPORT ALL THE SETTINGS.
*
*       FOR THAT REASON, YOU MUST NOT CHANGE THE ORDER OF VALUES,
*       AND NEW VALUES MUST BE ADDED AT THE END OF THE ENUMS TO AVOID
*       API BREAKAGES.
*/

#define MCFRC_DBG_MODE	(0)

/**
* @brief The available MC FRC functions
*/
enum hwMCFRC_processing_mode {
	MCFRC_MODE_FULL_CURR = 0,    /**< full interpolation mode (fill curr by timeout)*/
	MCFRC_MODE_FALL_CURR = 1,    /**< copy mode - fallback previous frame*/
	MCFRC_MODE_FULL_PREV = 2,    /**< full interpolation mode (fill prev by timeout)*/
	MCFRC_MODE_FALL_PREV = 3     /**< copy mode - fallback current frame*/
};


/**
* @brief the image format and size
*/
struct hwMCFRC_img_info {
	__u32 height;               /**< height of the image (luma component)*/
	__u32 width;                /**< width of the image (luma component)*/
	__u32 stride;               /**< stride of the image (luma component)*/
	__u32 height_uv;            /**< height of the image (chroma component)*/
	__u32 width_uv;             /**< width of the image (chroma component)*/
	__u32 offset;          /**< width of the image (chroma component)*/
	__u32 offset_uv;        /**< width of the image (chroma component)*/
	//__u32 stride_uv;            /**< stride of the image (chroma component)*/
};

/**
* @brief the type of the buffer sent to the driver
*/
enum hwMCFRC_buffer_type {
	HWMCFRC_BUFFER_NONE,	/**< no buffer is set */
	HWMCFRC_BUFFER_DMABUF,	/**< DMA buffer */
	HWMCFRC_BUFFER_USERPTR	/**< a pointer to user memory */
};

/**
* @brief the struct holding the description of a buffer
*
* A buffer can either be represented by a DMA file descriptor OR by a pointer
* to user memory. The len must be initialized with the size of the buffer,
* and the type must be set to the correct type.
*/
struct hwMCFRC_buffer {
	union {
		__s32 fd;				/**< the DMA file descriptor, if the buffer is a DMA buffer. This is in a union with userptr, only one should be set. */
		unsigned long userptr;	/**< the ptr to user memory, if the buffer is in user memory. This is in a union with fd, only one should be set. */
	};
	size_t len;					/**< the declared buffer length, in bytes */
	__u8 type;					/**< the declared buffer type @sa hwMCFRC_buffer_type */
};

/**
* @brief parameters for HW processing
*
* The fields needs to be explicitly set by userspace to the desired values,
* using the corresponding enumerations.
*/
struct hwMCFRC_config {
	enum hwMCFRC_processing_mode        mode;           /**< mode of MC FRC */
	__u32                               phase;          /**< phase parameter of MC FRC */
	__u32                               dbl_on;         /**< switching deblock of MC FRC */
};


#define REG_COMMAND_TYPE_READ       (0x1<<0)
#define REG_COMMAND_TYPE_WRITE      (0x1<<1)
#define REG_COMMAND_TYPE_END        (0x1<<31)
struct hwMCFRC_reg_command {
	__u32       type;
	__u32       address;
	__u32       data_write;
	__u32       data_read;
};
#define HWMCFRC_REG_PROGRAMM_MAX_LEN        (64)
struct hwMCFRC_reg_programm {
	struct hwMCFRC_reg_command command[HWMCFRC_REG_PROGRAMM_MAX_LEN];
};


#define RESULTS_LEN                 (64)
/**
* @brief the task information to be sent to the driver
*
* This is the struct holding all the task information that the driver needs
* to process a task.
*
* The client needs to fill it in with the necessary information and send it
* to the driver through the IOCTL.
*/
struct hwMCFRC_task {
	struct hwMCFRC_img_info img_markup;  /**< info related to the image sent to the HW for processing */
	struct hwMCFRC_buffer buf_inout[5];  /**< info related to the buffer sent to the HW for processing: in1, in2, out, params, temp*/
	struct hwMCFRC_config config;        /**< the configuration for HW IP */
	__u32                 results[RESULTS_LEN];

#if MCFRC_DBG_MODE // remove debug parameters
	/* for debugging purposes*/
	__u32                 dbg_control_flags;
	struct hwMCFRC_reg_programm    programms[5];
#endif
};

#define DBG_CONTROL_FLAG_0      (1<<0)
#define DBG_CONTROL_FLAG_1      (1<<1)
#define DBG_CONTROL_FLAG_2      (1<<2)
#define DBG_CONTROL_FLAG_3      (1<<3)
#define DBG_CONTROL_FLAG_4      (1<<4)
#define DBG_CONTROL_FLAG_5      (1<<5)
#define DBG_CONTROL_FLAG_6      (1<<6)
#define DBG_CONTROL_FLAG_7      (1<<7)
#define DBG_CONTROL_FLAG_8      (1<<8)
#define DBG_CONTROL_FLAG_9      (1<<9)
#define DBG_CONTROL_FLAG_10     (1<<10)
#define DBG_CONTROL_FLAG_11     (1<<11)
#define DBG_CONTROL_FLAG_12     (1<<12)
#define DBG_CONTROL_FLAG_13     (1<<13)
#define DBG_CONTROL_FLAG_14     (1<<14)
#define DBG_CONTROL_FLAG_15     (1<<15)


#define HWMCFRC_IOC_PROCESS	_IOWR('M', 0, struct hwMCFRC_task)

#define HWMCFRC_IOC_PROCESS_DBG_01	_IOWR('M', 1, struct hwMCFRC_task)
#define HWMCFRC_IOC_PROCESS_DBG_02	_IOWR('M', 2, struct hwMCFRC_task)
#define HWMCFRC_IOC_PROCESS_DBG_03	_IOWR('M', 3, struct hwMCFRC_task)
#define HWMCFRC_IOC_PROCESS_DBG_04	_IOWR('M', 4, struct hwMCFRC_task)
#define HWMCFRC_IOC_PROCESS_DBG_05	_IOWR('M', 5, struct hwMCFRC_task)

#endif // __HWMCFRC_H__

