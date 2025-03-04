#ifndef _SEC_CMD_H_
#define _SEC_CMD_H_
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/stat.h>
#include <linux/err.h>
#include <linux/input.h>
#include <linux/sched/clock.h>
#include <linux/uaccess.h>
#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#endif

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
#define USE_SEC_CMD_QUEUE
#include <linux/kfifo.h>
#endif

#define SEC_CLASS_DEVT_TSP		10
#define SEC_CLASS_DEVT_TKEY		11
#define SEC_CLASS_DEVT_WACOM		12
#define SEC_CLASS_DEVT_SIDEKEY		13
#define SEC_CLASS_DEV_NAME_TSP		"tsp"
#define SEC_CLASS_DEV_NAME_TKEY		"sec_touchkey"
#define SEC_CLASS_DEV_NAME_WACOM	"sec_epen"
#define SEC_CLASS_DEV_NAME_SIDEKEY	"sec_sidekey"

#define SEC_CLASS_DEVT_TSP1		15
#define SEC_CLASS_DEVT_TSP2		16
#define SEC_CLASS_DEV_NAME_TSP1		"tsp1"
#define SEC_CLASS_DEV_NAME_TSP2		"tsp2"

#define SEC_CMD(name, func)		.cmd_name = name, .cmd_func = func
#define SEC_CMD_H(name, func)		.cmd_name = name, .cmd_func = func, .cmd_log = 1
#define SEC_CMD_BUF_SIZE		(4096 - 1)
#define SEC_CMD_STR_LEN			256
#define SEC_CMD_RESULT_STR_LEN		(4096 - 1)
#define SEC_CMD_RESULT_STR_LEN_EXPAND	(SEC_CMD_RESULT_STR_LEN * 6)
#define SEC_CMD_PARAM_NUM		8

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
#define DEV_COUNT		2

#define FLIP_STATUS_DEFAULT	-1
#define FLIP_STATUS_MAIN	0
#define FLIP_STATUS_SUB		1

#define PATH_MAIN_SEC_CMD		"/sys/class/sec/tsp1/cmd"
#define PATH_MAIN_SEC_CMD_STATUS	"/sys/class/sec/tsp1/cmd_status"
#define PATH_MAIN_SEC_CMD_RESULT	"/sys/class/sec/tsp1/cmd_result"
#define PATH_MAIN_SEC_CMD_STATUS_ALL	"/sys/class/sec/tsp1/cmd_status_all"
#define PATH_MAIN_SEC_CMD_RESULT_ALL	"/sys/class/sec/tsp1/cmd_result_all"
#define PATH_MAIN_SEC_SYSFS_SUPPORT_FEATURE "/sys/class/sec/tsp1/support_feature"
#define PATH_MAIN_SEC_SYSFS_PROX_POWER_OFF "/sys/class/sec/tsp1/prox_power_off"
#define PATH_MAIN_SEC_SYSFS_DUALSCREEN_POLICY "/sys/class/sec/tsp1/dualscreen_policy"

#define PATH_SUB_SEC_CMD		"/sys/class/sec/tsp2/cmd"
#define PATH_SUB_SEC_CMD_STATUS		"/sys/class/sec/tsp2/cmd_status"
#define PATH_SUB_SEC_CMD_RESULT		"/sys/class/sec/tsp2/cmd_result"
#define PATH_SUB_SEC_CMD_STATUS_ALL	"/sys/class/sec/tsp2/cmd_status_all"
#define PATH_SUB_SEC_CMD_RESULT_ALL	"/sys/class/sec/tsp2/cmd_result_all"
#define PATH_SUB_SEC_SYSFS_PROX_POWER_OFF "/sys/class/sec/tsp2/prox_power_off"
#define PATH_SUB_SEC_SYSFS_DUALSCREEN_POLICY "/sys/class/sec/tsp2/dualscreen_policy"
#endif

struct sec_cmd {
	struct list_head	list;
	const char		*cmd_name;
	void			(*cmd_func)(void *device_data);
	int			cmd_log;
};

enum SEC_CMD_STATUS {
	SEC_CMD_STATUS_WAITING = 0,
	SEC_CMD_STATUS_RUNNING,		// = 1
	SEC_CMD_STATUS_OK,		// = 2
	SEC_CMD_STATUS_FAIL,		// = 3
	SEC_CMD_STATUS_EXPAND,		// = 4
	SEC_CMD_STATUS_NOT_APPLICABLE,	// = 5
};

#define INPUT_CMD_RESULT_NOT_EXIT	0
#define INPUT_CMD_RESULT_NEED_EXIT	1

#define input_cmd_result(cmd_state_parm, need_exit)				\
({										\
	if (need_exit == INPUT_CMD_RESULT_NEED_EXIT) {				\
		if (cmd_state_parm == SEC_CMD_STATUS_OK)			\
			snprintf(buff, sizeof(buff), "OK");			\
		else if (cmd_state_parm == SEC_CMD_STATUS_FAIL)			\
			snprintf(buff, sizeof(buff), "NG");			\
		else if (cmd_state_parm == SEC_CMD_STATUS_NOT_APPLICABLE)	\
			snprintf(buff, sizeof(buff), "NA");			\
	}									\
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));		\
	sec->cmd_state = cmd_state_parm;					\
	if (need_exit == INPUT_CMD_RESULT_NEED_EXIT)				\
		sec_cmd_set_cmd_exit(sec);					\
	input_info(true, ptsp, "%s: %s\n", __func__, buff);			\
})

#ifdef USE_SEC_CMD_QUEUE
#define SEC_CMD_MAX_QUEUE	10

struct command {
	char	cmd[SEC_CMD_STR_LEN];
};
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
struct sec_ts_virtual_sysfs_function {
	ssize_t (*sec_tsp_support_feature_show)(struct device *dev, struct device_attribute *attr, char *buf);
	ssize_t (*sec_tsp_prox_power_off_show)(struct device *dev, struct device_attribute *attr, char *buf);
	ssize_t (*sec_tsp_prox_power_off_store)(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
	ssize_t (*dualscreen_policy_store)(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
};
#endif

struct sec_cmd_data {
	struct device		*fac_dev;
	struct list_head	cmd_list_head;
	u8			cmd_state;
	char			cmd[SEC_CMD_STR_LEN];
	int			cmd_param[SEC_CMD_PARAM_NUM];
	char			*cmd_result;
	int			cmd_result_expand;
	int			cmd_result_expand_count;
	int			cmd_buffer_size;
	volatile bool		cmd_is_running;
	struct mutex		cmd_lock;
	struct mutex		fs_lock;
#ifdef USE_SEC_CMD_QUEUE
	struct kfifo		cmd_queue;
	struct mutex		fifo_lock;
	struct delayed_work	cmd_work;
	struct mutex		wait_lock;
	struct completion	cmd_result_done;
#endif
	bool			wait_cmd_result_done;
	int item_count;
	char cmd_result_all[SEC_CMD_RESULT_STR_LEN];
	u8 cmd_all_factory_state;

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
	struct sec_ts_virtual_sysfs_function *sysfs_functions;
#endif
};

extern void sec_cmd_set_cmd_exit(struct sec_cmd_data *data);
extern void sec_cmd_set_default_result(struct sec_cmd_data *data);
extern void sec_cmd_set_cmd_result(struct sec_cmd_data *data, char *buff, int len);
extern void sec_cmd_set_cmd_result_all(struct sec_cmd_data *data, char *buff, int len, char *item);
extern int sec_cmd_init(struct sec_cmd_data *data, struct sec_cmd *cmds, int len, int devt);
extern void sec_cmd_exit(struct sec_cmd_data *data, int devt);
extern void sec_cmd_send_event_to_user(struct sec_cmd_data *data, char *test, char *result);

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUAL_FOLDABLE)
void sec_cmd_virtual_tsp_register(struct sec_cmd_data *sec);
int sec_cmd_virtual_tsp_read_sysfs(struct sec_cmd_data *sec, const char *path, char *buf, int len);
int sec_cmd_virtual_tsp_write_sysfs(struct sec_cmd_data *sec, const char *path, const char *cmd);
int sec_cmd_virtual_tsp_write_cmd(struct sec_cmd_data *sec, bool main, bool sub);
void sec_cmd_virtual_tsp_write_cmd_factory_all(struct sec_cmd_data *sec, bool main, bool sub);
#endif
#endif /* _SEC_CMD_H_ */
