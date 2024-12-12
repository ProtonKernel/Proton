#ifndef _SYSFS_ERROR_H_
#define _SYSFS_ERROR_H_

/*
 * DPU Device dirver error flag
 * ** DO NOT MODiFY **
 */

/* DECON */
#define SYSFS_ERR_DECON_INT_PEND_TIME_OUT			(1<<0)
#define SYSFS_ERR_DECON_INT_PEND_RESOURCE_CONFLICT	(1<<1)


/* DMA */
#define SYSFS_ERR_DMA_CONFIG_ERROR_IRQ			(1<<0)


/* DPP */
#define SYSFS_ERR_DPP_CONFIG_ERROR_IRQ			(1<<0)


/* DSIM */
#define SYSFS_ERR_DSIM_ERR_RX_ECC				(1<<0)
#define SYSFS_ERR_DSIM_UNDER_RUN				(1<<1)


/* DP */
/* not support */


#endif	/* _SYSFS_ERROR_H_ */
