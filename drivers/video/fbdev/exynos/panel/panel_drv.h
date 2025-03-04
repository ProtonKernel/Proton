/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Author: Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_DRV_H__
#define __PANEL_DRV_H__

#include <linux/device.h>
#include <linux/fb.h>
#include <linux/notifier.h>
#include <linux/kernel.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <media/v4l2-subdev.h>
#include <linux/workqueue.h>
#include <linux/miscdevice.h>
#include <dt-bindings/gpio/gpio.h>

//Todo.. below header files need to move include directory and include using -I option
#include "../dpu30/disp_err.h"
#include "../dpu30/dsim.h"
#include "../dpu30/decon.h"
#include "../dpu30/panels/exynos_panel.h"

#include "panel.h"

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
#include "mdnie.h"
#endif

#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
#include "copr.h"
#endif

#ifdef CONFIG_SUPPORT_DDI_FLASH
#include "panel_poc.h"
#endif

#ifdef CONFIG_EXTEND_LIVE_CLOCK
#include "./aod/aod_drv.h"
#endif

#ifdef CONFIG_SUPPORT_MAFPC
#include "./mafpc/mafpc_drv.h"
#endif

#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
#include "./display_profiler/display_profiler.h"
#endif

#ifdef CONFIG_SUPPORT_POC_SPI
#include "panel_spi.h"
#endif

#if defined(CONFIG_TDMB_NOTIFIER)
#include <linux/tdmb_notifier.h>
#endif

#ifdef CONFIG_DYNAMIC_MIPI
#include "./dynamic_mipi/band_info.h"
#endif

#define PANEL_DEV_NAME ("panel")
#define MAX_PANEL_DEV_NAME_SIZE (32)


//Todo remove..
#if 0
#if defined(CONFIG_EXYNOS_DPU30)
typedef struct exynos_panel_info EXYNOS_PANEL_INFO;
#else
typedef struct decon_lcd EXYNOS_PANEL_INFO;
#endif
#else
typedef struct exynos_panel_info EXYNOS_PANEL_INFO;
#endif

extern void parse_lcd_info(struct device_node *node, EXYNOS_PANEL_INFO *lcd_info);

#ifdef CONFIG_SEC_PM
/* temporary disable */
#define CONFIG_DISP_PMIC_SSD
#endif
// #define CONFIG_SUPPORT_ERRFG_RECOVERY

void clear_pending_bit(int irq);

enum {
	PANEL_REGULATOR_TYPE_PWR = 0,
	PANEL_REGULATOR_TYPE_SSD = 1,
	PANEL_REGULATOR_TYPE_MAX,
};

enum {
	PANEL_REGULATOR_DDI_VCI = 0,
	PANEL_REGULATOR_DDI_VDD3,
	PANEL_REGULATOR_DDR_VDDR,
	PANEL_REGULATOR_SSD,
#ifdef CONFIG_EXYNOS_DPU30_DUAL
	PANEL_SUB_REGULATOR_DDI_VCI,
	PANEL_SUB_REGULATOR_DDI_VDD3,
	PANEL_SUB_REGULATOR_DDR_VDDR,
	PANEL_SUB_REGULATOR_SSD,
#endif
	PANEL_REGULATOR_MAX
};

enum panel_gpio_lists {
	PANEL_GPIO_RESET = 0,
	PANEL_GPIO_DISP_DET,
	PANEL_GPIO_PCD,
	PANEL_GPIO_ERR_FG,
	PANEL_GPIO_CONN_DET,
	PANEL_GPIO_MAX,
};

#define PANEL_GPIO_NAME_RESET ("disp-reset")
#define PANEL_GPIO_NAME_DISP_DET ("disp-det")
#define PANEL_GPIO_NAME_PCD ("pcd")
#define PANEL_GPIO_NAME_ERR_FG ("err-fg")
#define PANEL_GPIO_NAME_CONN_DET ("conn-det")

#define PANEL_REGULATOR_NAME_DDI_VCI ("ddi-vci")
#define PANEL_REGULATOR_NAME_DDI_VDD3 ("ddi-vdd3")
#define PANEL_REGULATOR_NAME_DDR_VDDR ("ddr-vddr")
#define PANEL_REGULATOR_NAME_SSD ("short-detect")

#ifdef CONFIG_EXYNOS_DPU30_DUAL
#define PANEL_SUB_REGULATOR_NAME_DDI_VCI ("ddi-sub-vci")
#define PANEL_SUB_REGULATOR_NAME_DDI_VDD3 ("ddi-sub-vdd3")
#define PANEL_SUB_REGULATOR_NAME_DDR_VDDR ("ddr-sub-vddr")
#define PANEL_SUB_REGULATOR_NAME_SSD ("short-sub-detect")
#endif

struct panel_gpio {
	const char *name;
	int num;
	bool active_low;
	int dir;		/* 0:out, 1:in */
	int irq;
	unsigned int irq_type;
	void __iomem *irq_pend_reg;
	int irq_pend_bit;
	bool irq_enable;
};

struct panel_regulator {
	const char *name;
	struct regulator *reg;
	int type;
	int def_voltage;
	int lpm_voltage;
	int from_off_current;
	int from_lpm_current;
};

struct cmd_set {
	u8 cmd_id;
	u32 offset;
	const u8 *buf;
	int size;
};

#define DSIM_OPTION_WAIT_TX_DONE	(1U << 0)
#define DSIM_OPTION_POINT_GPARA		(1U << 1)
#define DSIM_OPTION_2BYTE_GPARA		(1U << 2)

struct mipi_drv_ops {
	int (*read)(u32 id, u8 addr, u32 ofs, u8 *buf, int size, u32 option);
	int (*write)(u32 id, u8 cmd_id, const u8 *cmd, u32 ofs, int size, u32 option);
	int (*write_table)(u32 id, const struct cmd_set *cmd, int size, u32 option);
	int (*sr_write)(u32 id, u8 cmd_id, const u8 *cmd, u32 ofs, int size, u32 option);
	enum dsim_state(*get_state)(u32 id);
	void (*parse_dt)(struct device_node *node, EXYNOS_PANEL_INFO *lcd_info);
	EXYNOS_PANEL_INFO *(*get_lcd_info)(u32 id);
	int (*wait_for_vsync)(u32 id, u32 timeout);
	int (*set_bypass)(u32 id, bool on);
	int (*get_bypass)(u32 id);
	int (*wake_lock)(u32 id, unsigned long timeout);
	int (*wake_unlock)(u32 id);
	int (*flush_image)(u32 id);
};

#define PANEL_INIT_KERNEL		0
#define PANEL_INIT_BOOT			1

#define PANEL_DISP_DET_HIGH		1
#define PANEL_DISP_DET_LOW		0

enum {
	PANEL_STATE_NOK = 0,
	PANEL_STATE_OK = 1,
};

enum {
	PANEL_EL_OFF = 0,
	PANEL_EL_ON = 1,
};

enum {
	PANEL_UB_CONNECTED = 0,
	PANEL_UB_DISCONNECTED = 1,
};

enum {
	PANEL_DISCONNECT = 0,
	PANEL_CONNECT = 1,
};

enum {
	PANEL_PCD_BYPASS_OFF = 0,
	PANEL_PCD_BYPASS_ON = 1,
};

#define ALPM_MODE	0
#define HLPM_MODE	1

enum panel_lpm_lfd_fps {
	LPM_LFD_1HZ = 0,
	LPM_LFD_30HZ,
};

enum panel_active_state {
	PANEL_STATE_OFF = 0,
	PANEL_STATE_ON,
	PANEL_STATE_NORMAL,
	PANEL_STATE_ALPM,
	MAX_PANEL_STATE,
};

enum panel_power {
	PANEL_POWER_OFF = 0,
	PANEL_POWER_ON
};

enum {
	PANEL_DISPLAY_OFF = 0,
	PANEL_DISPLAY_ON,
};

enum {
	PANEL_HMD_OFF = 0,
	PANEL_HMD_ON,
};

enum {
	PANEL_WORK_DISP_DET = 0,
	PANEL_WORK_PCD,
	PANEL_WORK_ERR_FG,
	PANEL_WORK_CONN_DET,
	PANEL_WORK_DIM_FLASH,
	PANEL_WORK_CHECK_CONDITION,
	PANEL_WORK_UPDATE,
#ifdef CONFIG_EVASION_DISP_DET
	PANEL_WORK_EVASION_DISP_DET,
#endif
	PANEL_WORK_MAX,
};

enum {
	PANEL_THREAD_VRR_BRIDGE,
	PANEL_THREAD_MAX,
};

enum {
	PANEL_CB_VRR,
	PANEL_CB_DISPLAY_MODE,
	MAX_PANEL_CB,
};

struct panel_state {
	int init_at;
	int connect_panel;
	int connected;
	int cur_state;
	int power;
	int disp_on;
#ifdef CONFIG_SUPPORT_HMD
	int hmd_on;
#endif
	int lpm_brightness;
	int pcd_bypass;
};

struct copr_spi_gpios {
	int gpio_sck;
	int gpio_miso;
	int gpio_mosi;
	int gpio_cs;
};

struct host_cb {
	/* callback function type must be same with disp_cb */
	int (*cb)(void *data, void *arg);
	void *data;
};

enum {
	NO_CHECK_STATE = 0,
	PRINT_NORMAL_PANEL_INFO,
	CHECK_NORMAL_PANEL_INFO,
	PRINT_DOZE_PANEL_INFO,
	STATE_MAX
};

#define STR_NO_CHECK			("no state")
#define STR_NOMARL_ON			("after normal disp on")
#define STR_NOMARL_100FRAME		("check normal in 100frames")
#define STR_AOD_ON				("after aod disp on")

struct panel_condition_check {
	bool is_panel_check;
	u32 frame_cnt;
	u8 check_state;
	char str_state[STATE_MAX][30];
};

enum GAMMA_FLASH_RESULT {
	GAMMA_FLASH_ERROR_MTP_OFFSET = -4,
	GAMMA_FLASH_ERROR_CHECKSUM_MISMATCH = -3,
	GAMMA_FLASH_ERROR_NOT_EXIST = -2,
	GAMMA_FLASH_ERROR_READ_FAIL = -1,
	GAMMA_FLASH_PROGRESS = 0,
	GAMMA_FLASH_SUCCESS = 1,
};

struct dim_flash_result {
	atomic_t running;
	bool exist;
	u32 state;
	char result[256];
};

typedef void (*panel_wq_handler)(struct work_struct *);

struct panel_work {
	struct mutex lock;
	struct workqueue_struct *wq;
	struct delayed_work dwork;
	atomic_t running;
	void *data;
	int ret;
};

#define MAX_PANEL_CMD_QUEUE	(1024)

struct panel_cmd_queue {
	struct cmd_set cmd[MAX_PANEL_CMD_QUEUE];
	int top;
	int cmd_payload_size;
	int img_payload_size;
	struct mutex lock;
};
typedef int (*panel_thread_fn)(void *data);

struct panel_thread {
	wait_queue_head_t wait;
	atomic_t count;
	struct task_struct *thread;
	bool should_stop;
};

enum {
	PANEL_DEBUGFS_LOG,
	PANEL_DEBUGFS_CMD_LOG,
#if IS_ENABLED(CONFIG_PANEL_NOTIFY)
	PANEL_DEBUGFS_PANEL_EVENT,
#endif
	MAX_PANEL_DEBUGFS,
};

struct panel_debugfs {
	int id;
	struct dentry *dir;
	struct dentry *file;
	void *private;
};

struct panel_debug {
	struct dentry *dir;
	struct panel_debugfs *debugfs[MAX_PANEL_DEBUGFS];
};

struct panel_device {
	int id;
	int dsi_id;

	struct device_node *ddi_node;
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	struct panel_display_modes *panel_modes;
#endif

#ifdef CONFIG_EXYNOS_DECON_LCD_SPI
	struct spi_device *spi;
	struct copr_spi_gpios spi_gpio;
#endif

#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	struct copr_info copr;
#endif
	/* mutex lock for panel operations */
	struct mutex op_lock;
	/* mutex lock for panel's data */
	struct mutex data_lock;
	/* mutex lock for panel's ioctl */
	struct mutex io_lock;

	struct device *dev;

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	struct mdnie_info mdnie;
#endif
	struct device *lcd_dev;
	struct panel_bl_device panel_bl;

	unsigned char panel_id[3];

	struct v4l2_subdev sd;

	struct panel_gpio gpio[PANEL_GPIO_MAX];
	struct panel_regulator regulator[PANEL_REGULATOR_MAX];

	struct pinctrl *pinctrl;
	struct pinctrl_state *default_gpio_pinctrl;

	struct mipi_drv_ops mipi_drv;
	struct panel_cmd_queue cmdq;

	struct panel_state state;

	struct panel_work work[PANEL_WORK_MAX];
	struct panel_thread thread[PANEL_THREAD_MAX];

	struct notifier_block fb_notif;
#ifdef CONFIG_DISPLAY_USE_INFO
	struct notifier_block panel_dpui_notif;
#endif
	struct panel_info panel_data;

#ifdef CONFIG_EXTEND_LIVE_CLOCK
	struct aod_dev_info aod;
#endif

#ifdef CONFIG_SUPPORT_MAFPC
	struct mafpc_device mafpc;
	struct v4l2_subdev *mafpc_sd;
	s64 mafpc_write_time;
#endif

#ifdef CONFIG_SUPPORT_DDI_FLASH
	struct panel_poc_device poc_dev;
#endif
#ifdef CONFIG_SUPPORT_POC_SPI
	struct panel_spi_dev panel_spi_dev;
#endif
#ifdef CONFIG_SUPPORT_TDMB_TUNE
	struct notifier_block tdmb_notif;
#endif
#if defined(CONFIG_INPUT_TOUCHSCREEN)
	struct notifier_block input_notif;
#endif
	struct disp_error_cb_info error_cb_info;
	struct disp_cb_info cb_info[MAX_PANEL_CB];

	ktime_t ktime_panel_on;
	ktime_t ktime_panel_off;

	struct dim_flash_result flash_checksum_result;

#ifdef CONFIG_SUPPORT_DIM_FLASH
	struct panel_irc_info *irc_info;
#endif
	struct panel_condition_check condition_check;

#ifdef CONFIG_DYNAMIC_MIPI
	struct dynamic_mipi_info dynamic_mipi;
#endif

#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
	struct profiler_device profiler;
#endif
	struct panel_debug d;
};

#ifdef CONFIG_SUPPORT_DIM_FLASH
int panel_update_dim_type(struct panel_device *panel, u32 dim_type);
#endif
int panel_flash_checksum_calc(struct panel_device *panel);
int panel_mtp_gamma_check(struct panel_device *panel);

static inline bool IS_PANEL_PWR_ON_STATE(struct panel_device *panel)
{
	return (panel->state.cur_state == PANEL_STATE_ON ||
			panel->state.cur_state == PANEL_STATE_NORMAL ||
			panel->state.cur_state == PANEL_STATE_ALPM);
}

static inline bool IS_PANEL_PWR_OFF_STATE(struct panel_device *panel)
{
	return panel->state.cur_state == PANEL_STATE_OFF;
}

int panel_display_on(struct panel_device *panel);
int __set_panel_power(struct panel_device *panel, int power);

#ifdef CONFIG_EXTEND_LIVE_CLOCK

#define INIT_WITHOUT_LOCK	0
#define INIT_WITH_LOCK		1

static inline int panel_aod_init_panel(struct panel_device *panel, int lock)

{
	return (panel->aod.ops && panel->aod.ops->init_panel) ?
		panel->aod.ops->init_panel(&panel->aod, lock) : 0;
}

static inline int panel_aod_enter_to_lpm(struct panel_device *panel)
{
	return (panel->aod.ops && panel->aod.ops->enter_to_lpm) ?
		panel->aod.ops->enter_to_lpm(&panel->aod) : 0;
}

static inline int panel_aod_exit_from_lpm(struct panel_device *panel)
{
	return (panel->aod.ops && panel->aod.ops->exit_from_lpm) ?
		panel->aod.ops->exit_from_lpm(&panel->aod) : 0;
}

static inline int panel_aod_doze_suspend(struct panel_device *panel)
{
	return (panel->aod.ops && panel->aod.ops->doze_suspend) ?
		panel->aod.ops->doze_suspend(&panel->aod) : 0;
}

static inline int panel_aod_power_off(struct panel_device *panel)
{
	return (panel->aod.ops && panel->aod.ops->power_off) ?
		panel->aod.ops->power_off(&panel->aod) : 0;
}

static inline int panel_aod_black_grid_on(struct panel_device *panel)
{
	return (panel->aod.ops && panel->aod.ops->black_grid_on) ?
		panel->aod.ops->black_grid_on(&panel->aod) : 0;
}

static inline int panel_aod_black_grid_off(struct panel_device *panel)
{
	return (panel->aod.ops && panel->aod.ops->black_grid_off) ?
		panel->aod.ops->black_grid_off(&panel->aod) : 0;
}

#ifdef SUPPORT_NORMAL_SELF_MOVE
static inline int panel_self_move_pattern_update(struct panel_device *panel)
{
	return (panel->aod.ops && panel->aod.ops->self_move_pattern_update) ?
		panel->aod.ops->self_move_pattern_update(&panel->aod) : 0;
}
#endif
#endif

inline char *get_panel_state_names(enum panel_active_state);
struct panel_device *panel_device_create(void);
int panel_device_init(struct panel_device *panel);
int panel_device_exit(struct panel_device *panel);

bool ub_con_disconnected(struct panel_device *panel);
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
int panel_seq_cmdlog_dump(struct panel_device *panel);
#endif
bool panel_gpio_valid(struct panel_gpio *gpio);
void panel_send_ubconn_uevent(struct panel_device *panel);
int panel_set_gpio_irq(struct panel_gpio *gpio, bool enable);

int panel_create_debugfs(struct panel_device *panel);
void panel_destroy_debugfs(struct panel_device *panel);
#if defined(CONFIG_EXYNOS_DOZE) && defined(CONFIG_SEC_FACTORY)
int panel_seq_exit_alpm(struct panel_device *panel);
int panel_seq_set_alpm(struct panel_device *panel);
#endif
#if defined(CONFIG_PANEL_DISPLAY_MODE)
int panel_display_mode_cb(struct panel_device *panel);
int panel_update_display_mode(struct panel_device *panel);
int find_panel_mode_by_common_panel_display_mode(struct panel_device *panel,
		struct common_panel_display_mode *cpdm);
int panel_display_mode_find_panel_mode(struct panel_device *panel,
		struct panel_display_mode *pdm);
int panel_display_mode_get_native_mode(struct panel_device *panel);
int panel_set_display_mode_nolock(struct panel_device *panel, int panel_mode);
#endif

#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
int panel_fast_discharge_set(struct panel_device *panel);
#endif
#define PANEL_DRV_NAME "panel-drv"

#define PANEL_IOC_BASE	'P'

#define PANEL_IOC_DSIM_PROBE			_IOW(PANEL_IOC_BASE, 1, EXYNOS_PANEL_INFO *)
#define PANEL_IOC_GET_PANEL_STATE		_IOW(PANEL_IOC_BASE, 3, struct panel_state *)
#define PANEL_IOC_DSIM_PUT_MIPI_OPS		_IOR(PANEL_IOC_BASE, 4, struct mipi_ops *)
#define PANEL_IOC_PANEL_PROBE			_IO(PANEL_IOC_BASE, 5)

#define PANEL_IOC_SET_POWER				_IO(PANEL_IOC_BASE, 6)

#define PANEL_IOC_SLEEP_IN				_IO(PANEL_IOC_BASE, 7)
#define PANEL_IOC_SLEEP_OUT				_IO(PANEL_IOC_BASE, 8)

#define PANEL_IOC_DISP_ON				_IO(PANEL_IOC_BASE, 9)

#define PANEL_IOC_PANEL_DUMP			_IO(PANEL_IOC_BASE, 10)

#define PANEL_IOC_EVT_FRAME_DONE		_IOW(PANEL_IOC_BASE, 11, struct timespec *)
#define PANEL_IOC_EVT_VSYNC				_IOW(PANEL_IOC_BASE, 12, struct timespec *)

#define PANEL_IOC_PANEL_RESET			_IO(PANEL_IOC_BASE, 13)

#ifdef CONFIG_EXYNOS_DOZE
#define PANEL_IOC_DOZE					_IO(PANEL_IOC_BASE, 31)
#define PANEL_IOC_DOZE_SUSPEND			_IO(PANEL_IOC_BASE, 32)
#endif

#define PANEL_IOC_SET_MRES				_IOW(PANEL_IOC_BASE, 41, int *)
#define PANEL_IOC_GET_MRES				_IOR(PANEL_IOC_BASE, 42, struct panel_mres *)
#define PANEL_IOC_SET_VRR_INFO			_IOW(PANEL_IOC_BASE, 45, struct vrr_config_data *)
#if defined(CONFIG_PANEL_DISPLAY_MODE)
#define PANEL_IOC_GET_DISPLAY_MODE		_IOR(PANEL_IOC_BASE, 48, void *)
#define PANEL_IOC_SET_DISPLAY_MODE		_IOW(PANEL_IOC_BASE, 49, void *)
#define PANEL_IOC_REG_DISPLAY_MODE_CB	_IOR(PANEL_IOC_BASE, 53, struct host_cb *)
#endif

#define PANEL_IOC_REG_RESET_CB			_IOR(PANEL_IOC_BASE, 51, struct host_cb *)
#define PANEL_IOC_REG_VRR_CB			_IOR(PANEL_IOC_BASE, 52, struct host_cb *)
#ifdef CONFIG_SUPPORT_MASK_LAYER
#define PANEL_IOC_SET_MASK_LAYER		_IOW(PANEL_IOC_BASE, 61, struct mask_layer_data *)
#endif

#ifdef CONFIG_DYNAMIC_MIPI
#define MAGIC_DF_UPDATED				(0x0A55AA55)

#define PANEL_IOC_DM_GET_INFO			_IOR(PANEL_IOC_BASE, 85, struct dynamic_mipi_info *)
#define PANEL_IOC_DM_SET_PANEL_FFC			_IOR(PANEL_IOC_BASE, 82, int *)
#define PANEL_IOC_DM_OFF_PANEL_FFC			_IOR(PANEL_IOC_BASE, 83, int *)
#endif
#endif //__PANEL_DRV_H__
