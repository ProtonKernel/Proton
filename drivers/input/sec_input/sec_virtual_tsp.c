
#include "sec_cmd.h"
#include "sec_input.h"
#include "sec_tsp_log.h"

#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
#include <linux/hall/hall_ic_notifier.h>
static struct notifier_block hall_ic_nb;
#endif
#if IS_ENABLED(CONFIG_SUPPORT_SENSOR_FOLD)
#include <linux/sensor/sensors_core.h>
static struct notifier_block hall_ic_nb_ssh;
#endif

static int flip_status;
static struct mutex switching_mutex;
static int fac_flip_status;

static struct sec_cmd_data *dual_sec;

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
#include "sec_tsp_dumpkey.h"
extern struct tsp_dump_callbacks dump_callbacks;
struct tsp_dump_callbacks *tsp_callbacks;
EXPORT_SYMBOL(tsp_callbacks);
static struct tsp_dump_callbacks callbacks[DEV_COUNT];

static void sec_virtual_tsp_dump(struct device *dev)
{
	input_info(true, dual_sec->fac_dev, "%s: flip_status %s[%d]\n", __func__,
		flip_status ? "close:sub_tsp" : "open:main_tsp", flip_status);

	if (flip_status == FLIP_STATUS_MAIN) {
		if (callbacks[FLIP_STATUS_MAIN].inform_dump)
			callbacks[FLIP_STATUS_MAIN].inform_dump(dual_sec->fac_dev);
	} else if (flip_status == FLIP_STATUS_SUB) {
		if (callbacks[FLIP_STATUS_SUB].inform_dump)
			callbacks[FLIP_STATUS_SUB].inform_dump(dual_sec->fac_dev);
	}
}
#endif

static void sec_virtual_tsp_dual_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	sec_cmd_virtual_tsp_write_cmd(sec, true, true);
}

#if !IS_ENABLED(CONFIG_TOUCHSCREEN_STM_SUB)
static void sec_virtual_tsp_main_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	bool main = true;
	bool sub = false;

	mutex_lock(&switching_mutex);
	if (!((fac_flip_status == FLIP_STATUS_DEFAULT && flip_status == FLIP_STATUS_MAIN)
			|| fac_flip_status == FLIP_STATUS_MAIN)) {
		input_err(true, sec->fac_dev, "%s: flip is sub, skip[%d,%d]\n",
			__func__, fac_flip_status, flip_status);
		main = false;
	}
	mutex_unlock(&switching_mutex);

	sec_cmd_virtual_tsp_write_cmd(sec, main, sub);
}
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_ZINITIX_ZTW522)
static void sec_virtual_tsp_sub_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	bool main = false;
	bool sub = true;

	mutex_lock(&switching_mutex);
	if (!((fac_flip_status == FLIP_STATUS_DEFAULT && flip_status == FLIP_STATUS_SUB)
			|| fac_flip_status == FLIP_STATUS_SUB)) {
		input_err(true, sec->fac_dev, "%s: flip is main, skip[%d,%d]\n",
			__func__, fac_flip_status, flip_status);
		sub = false;
	}
	mutex_unlock(&switching_mutex);

	sec_cmd_virtual_tsp_write_cmd(sec, main, sub);
}
#endif

static void sec_virtual_not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	sec->cmd_state = SEC_CMD_STATUS_OK;
	snprintf(buff, sizeof(buff), "%s", "NA");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void sec_virtual_tsp_switch_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	bool main = false;
	bool sub = false;

	mutex_lock(&switching_mutex);
	if (fac_flip_status == FLIP_STATUS_DEFAULT) {
		/* Using hall_ic */
		if (flip_status == FLIP_STATUS_MAIN) {
			main = true;
		} else if (flip_status == FLIP_STATUS_SUB) {
			sub = true;
		}
	} else if (fac_flip_status == FLIP_STATUS_MAIN) {
		main = true;
	} else if (fac_flip_status == FLIP_STATUS_SUB) {
		sub = true;
	}
	input_dbg(true, sec->fac_dev, "%s: %d,%d\n", __func__, fac_flip_status, flip_status);
	mutex_unlock(&switching_mutex);

	sec_cmd_virtual_tsp_write_cmd(sec, main, sub);
}

static void sec_virtual_tsp_factory_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	bool main = false;
	bool sub = false;

	mutex_lock(&switching_mutex);
	if (fac_flip_status == FLIP_STATUS_DEFAULT) {
		/* Using hall_ic */
		if (flip_status == FLIP_STATUS_MAIN) {
			main = true;
		} else if (flip_status == FLIP_STATUS_SUB) {
			sub = true;
		}
	} else if (fac_flip_status == FLIP_STATUS_MAIN) {
		main = true;
	} else if (fac_flip_status == FLIP_STATUS_SUB) {
		sub = true;
	}
	input_dbg(true, sec->fac_dev, "%s: %d,%d\n", __func__, fac_flip_status, flip_status);
	mutex_unlock(&switching_mutex);

	sec_cmd_virtual_tsp_write_cmd_factory_all(sec, main, sub);
}

static void set_factory_panel(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < FLIP_STATUS_DEFAULT || sec->cmd_param[0] > FLIP_STATUS_SUB) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto err;
	}

	mutex_lock(&switching_mutex);
	fac_flip_status = sec->cmd_param[0];
	input_err(true, sec->fac_dev, "%s: %d\n", __func__, fac_flip_status);
	mutex_unlock(&switching_mutex);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

err:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void dev_count(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%s,%d", "OK", DEV_COUNT);
	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
static int sec_virtual_tsp_hall_ic_notify(struct notifier_block *nb,
			unsigned long flip_cover, void *v)
{
	struct hall_notifier_context *hall_notifier;

	hall_notifier = v;

	if (strncmp(hall_notifier->name, "flip", 4))
		return 0;

	input_info(true, dual_sec->fac_dev, "%s: %s\n", __func__,
			 flip_cover ? "close" : "open");

	mutex_lock(&switching_mutex);
	flip_status = flip_cover;
	mutex_unlock(&switching_mutex);

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_SUPPORT_SENSOR_FOLD)
static int sec_virtual_tsp_hall_ic_ssh_notify(struct notifier_block *nb,
			unsigned long flip_cover, void *v)
{
	input_info(true, dual_sec->fac_dev, "%s: %s\n", __func__,
			 flip_cover ? "close" : "open");

	mutex_lock(&switching_mutex);
	flip_status = flip_cover;
	mutex_unlock(&switching_mutex);

	return 0;
}
#endif

static struct sec_cmd tsp_commands[] = {
	{SEC_CMD_H("glove_mode", sec_virtual_tsp_dual_cmd),},
	{SEC_CMD_H("set_wirelesscharger_mode", sec_virtual_tsp_dual_cmd),},
	{SEC_CMD_H("spay_enable", sec_virtual_tsp_dual_cmd),},
	{SEC_CMD_H("aot_enable", sec_virtual_tsp_dual_cmd),},
	{SEC_CMD_H("aod_enable", sec_virtual_tsp_dual_cmd),},
	{SEC_CMD_H("fod_enable", sec_virtual_tsp_dual_cmd),},
	{SEC_CMD("fod_lp_mode", sec_virtual_tsp_dual_cmd),},
	{SEC_CMD_H("external_noise_mode", sec_virtual_tsp_dual_cmd),},
	{SEC_CMD_H("brush_enable", sec_virtual_tsp_dual_cmd),},
	{SEC_CMD_H("singletap_enable", sec_virtual_tsp_dual_cmd),},
	{SEC_CMD_H("set_touchable_area", sec_virtual_tsp_dual_cmd),},
	{SEC_CMD("clear_cover_mode", sec_virtual_tsp_dual_cmd),},
	{SEC_CMD("ear_detect_enable", sec_virtual_tsp_dual_cmd),},
	{SEC_CMD("pocket_mode_enable", sec_virtual_tsp_dual_cmd),},

	/* SemInputDeviceManagerService */
	{SEC_CMD("set_game_mode", sec_virtual_tsp_dual_cmd),},
	{SEC_CMD("set_sip_mode", sec_virtual_tsp_dual_cmd),},
	{SEC_CMD("set_note_mode", sec_virtual_tsp_dual_cmd),},
	{SEC_CMD("set_scan_rate", sec_virtual_tsp_switch_cmd),},
	{SEC_CMD("refresh_rate_mode", sec_virtual_tsp_switch_cmd),},
	{SEC_CMD("prox_lp_scan_mode", sec_virtual_tsp_switch_cmd),},
	{SEC_CMD("set_grip_data", sec_virtual_tsp_switch_cmd),},
	{SEC_CMD("set_temperature", sec_virtual_tsp_switch_cmd),},
	{SEC_CMD("set_aod_rect", sec_virtual_tsp_switch_cmd),},
	{SEC_CMD("fod_icon_visible", sec_virtual_tsp_switch_cmd),},
	{SEC_CMD("set_fod_rect", sec_virtual_tsp_switch_cmd),},
	{SEC_CMD("fp_int_control", sec_virtual_tsp_switch_cmd),},
	{SEC_CMD("sync_changed", sec_virtual_tsp_switch_cmd),},

	/* run_xxx_read_all common */
	{SEC_CMD("run_cs_raw_read_all", sec_virtual_tsp_switch_cmd),},
	{SEC_CMD("run_cs_delta_read_all", sec_virtual_tsp_switch_cmd),},
	{SEC_CMD("run_rawcap_read_all", sec_virtual_tsp_switch_cmd),},

	/* only main */
	/* only sub */
#if IS_ENABLED(CONFIG_TOUCHSCREEN_STM_SUB)
	/* run_xxx_read_all stm */
	{SEC_CMD("run_ix_data_read_all", sec_virtual_tsp_switch_cmd),},
	{SEC_CMD("run_self_raw_read_all", sec_virtual_tsp_switch_cmd),},
	{SEC_CMD("run_cx_data_read_all", sec_virtual_tsp_switch_cmd),},
	{SEC_CMD("run_cx_gap_data_rx_all", sec_virtual_tsp_switch_cmd),},
	{SEC_CMD("run_cx_gap_data_tx_all", sec_virtual_tsp_switch_cmd),},
	{SEC_CMD("run_factory_miscalibration_read_all", sec_virtual_tsp_switch_cmd),},
#else
	/* run_xxx_read_all main(stm) */
	{SEC_CMD("run_ix_data_read_all", sec_virtual_tsp_main_cmd),},
	{SEC_CMD("run_self_raw_read_all", sec_virtual_tsp_main_cmd),},
	{SEC_CMD("run_cx_data_read_all", sec_virtual_tsp_main_cmd),},
	{SEC_CMD("run_cx_gap_data_rx_all", sec_virtual_tsp_main_cmd),},
	{SEC_CMD("run_cx_gap_data_tx_all", sec_virtual_tsp_main_cmd),},
	{SEC_CMD("run_factory_miscalibration_read_all", sec_virtual_tsp_main_cmd),},
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_ZINITIX_ZTW522)
	/* run_xxx_read_all sub(zinitix) */
	{SEC_CMD("run_dnd_read_all", sec_virtual_tsp_sub_cmd),},
	{SEC_CMD("run_dnd_v_gap_read_all", sec_virtual_tsp_sub_cmd),},
	{SEC_CMD("run_dnd_h_gap_read_all", sec_virtual_tsp_sub_cmd),},
	{SEC_CMD("run_selfdnd_read_all", sec_virtual_tsp_sub_cmd),},
	{SEC_CMD("run_selfdnd_h_gap_read_all", sec_virtual_tsp_sub_cmd),},
	{SEC_CMD("run_jitter_read_all", sec_virtual_tsp_sub_cmd),},
	{SEC_CMD("run_self_saturation_read_all", sec_virtual_tsp_sub_cmd),},
	/* sub touch test (fold open) */
	{SEC_CMD("dead_zone_enable", sec_virtual_tsp_dual_cmd),},
#endif
	{SEC_CMD("two_finger_doubletap_enable", sec_virtual_tsp_dual_cmd),},
	{SEC_CMD("factory_cmd_result_all", sec_virtual_tsp_factory_cmd_result_all),},
	{SEC_CMD("factory_cmd_result_all_imagetest", sec_virtual_tsp_factory_cmd_result_all),},

	{SEC_CMD_H("set_factory_panel", set_factory_panel),},
	{SEC_CMD("dev_count", dev_count),},
	{SEC_CMD("ligthsensor_detect_enable", sec_virtual_not_support_cmd),},
	{SEC_CMD("not_support_cmd", sec_virtual_tsp_switch_cmd),},
};

static ssize_t sec_virtual_tsp_support_feature_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char buffer[16];
	int ret;

	memset(buffer, 0x00, sizeof(buffer));

	ret = sec_cmd_virtual_tsp_read_sysfs(dual_sec, PATH_MAIN_SEC_SYSFS_SUPPORT_FEATURE, buffer, sizeof(buffer));
	if (ret < 0)
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NG\n");

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buffer);
}

static ssize_t sec_virtual_tsp_prox_power_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char buffer[16];
	int ret;

	memset(buffer, 0x00, sizeof(buffer));

	if (flip_status)
		ret = sec_cmd_virtual_tsp_read_sysfs(dual_sec, PATH_SUB_SEC_SYSFS_PROX_POWER_OFF, buffer, sizeof(buffer));
	else
		ret = sec_cmd_virtual_tsp_read_sysfs(dual_sec, PATH_MAIN_SEC_SYSFS_PROX_POWER_OFF, buffer, sizeof(buffer));

	input_info(false, dual_sec->fac_dev, "%s: %s, ret:%d\n", __func__,
			 flip_status ? "close" : "open", ret);

	if (ret < 0)
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NG\n");

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s\n", buffer);
}

static ssize_t sec_virtual_tsp_prox_power_off_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;

	if (flip_status)
		ret = sec_cmd_virtual_tsp_write_sysfs(dual_sec, PATH_SUB_SEC_SYSFS_PROX_POWER_OFF, buf);
	else
		ret = sec_cmd_virtual_tsp_write_sysfs(dual_sec, PATH_MAIN_SEC_SYSFS_PROX_POWER_OFF, buf);

	input_info(false, dual_sec->fac_dev, "%s: %s, ret:%d\n", __func__,
			 flip_status ? "close" : "open", ret);

	return count;
}

static ssize_t dualscreen_policy_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret, value;

	ret = kstrtoint(buf, 10, &value);
	if (ret != 0)
		return ret;

	if (value <= FLIP_STATUS_DEFAULT || value > FLIP_STATUS_SUB) {
		input_info(false, dual_sec->fac_dev, "%s: value=%d %s\n", __func__, value,
				 flip_status ? "close" : "open");
		return count;
	}

	mutex_lock(&switching_mutex);
	flip_status = value;
	mutex_unlock(&switching_mutex);

	if (value == FLIP_STATUS_MAIN) {
		sec_cmd_virtual_tsp_write_sysfs(dual_sec, PATH_MAIN_SEC_SYSFS_DUALSCREEN_POLICY, buf);
		sec_cmd_virtual_tsp_write_sysfs(dual_sec, PATH_SUB_SEC_SYSFS_DUALSCREEN_POLICY, buf);
	}

	input_info(true, dual_sec->fac_dev, "%s: value=%d %s\n", __func__, value,
			 flip_status ? "close" : "open");

	return count;
}

static DEVICE_ATTR(dualscreen_policy, 0644, NULL, dualscreen_policy_store);
static DEVICE_ATTR(support_feature, 0444, sec_virtual_tsp_support_feature_show, NULL);
static DEVICE_ATTR(prox_power_off, 0644, sec_virtual_tsp_prox_power_off_show, sec_virtual_tsp_prox_power_off_store);

static struct attribute *sec_virtual_tsp_attrs[] = {
	&dev_attr_support_feature.attr,
	&dev_attr_prox_power_off.attr,
	&dev_attr_dualscreen_policy.attr,
	NULL,
};

static struct attribute_group sec_virtual_tsp_attrs_group = {
	.attrs = sec_virtual_tsp_attrs,
};

static int __init __init_sec_virtual_tsp(void)
{
	int ret;

	dual_sec = kzalloc(sizeof(struct sec_cmd_data), GFP_KERNEL);
	if (!dual_sec) {
		input_err(true, NULL, "%s: failed to alloc sec_cmd_data for dual tsp\n", __func__);
		return -ENOMEM;
	}

	sec_cmd_init(dual_sec, tsp_commands,
		ARRAY_SIZE(tsp_commands), SEC_CLASS_DEVT_TSP);

	input_info(true, dual_sec->fac_dev, "%s\n", __func__);

	mutex_init(&switching_mutex);

	mutex_lock(&switching_mutex);
	fac_flip_status = FLIP_STATUS_DEFAULT;
	flip_status = FLIP_STATUS_MAIN;
	mutex_unlock(&switching_mutex);

#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
	hall_ic_nb.priority = 1;
	hall_ic_nb.notifier_call = sec_virtual_tsp_hall_ic_notify;
	hall_notifier_register(&hall_ic_nb);
	input_info(true, dual_sec->fac_dev, "%s: hall ic register\n", __func__);
#endif
#if IS_ENABLED(CONFIG_SUPPORT_SENSOR_FOLD)
	hall_ic_nb_ssh.priority = 1;
	hall_ic_nb_ssh.notifier_call = sec_virtual_tsp_hall_ic_ssh_notify;
	sensorfold_notifier_register(&hall_ic_nb_ssh);
	input_info(true, dual_sec->fac_dev, "%s: hall ic(ssh) register\n", __func__);
#endif
#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
	tsp_callbacks = callbacks;
	dump_callbacks.inform_dump = sec_virtual_tsp_dump;
#endif

	ret = sysfs_create_group(&dual_sec->fac_dev->kobj, &sec_virtual_tsp_attrs_group);
	if (ret < 0) {
		pr_err("%s %s: failed to create sysfs group\n", SECLOG, __func__);
		return -ENODEV;
	}

	return 0;
}

module_init(__init_sec_virtual_tsp);

MODULE_DESCRIPTION("Samsung virtual tsp functions");
MODULE_LICENSE("GPL");
