/*
 * Copyrights (C) 2017 Samsung Electronics, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/battery/sec_pd.h>

static SEC_PD_SINK_STATUS *g_psink_status;

int sec_pd_select_pdo(int num)
{
	if (!g_psink_status) {
		pr_err("%s: g_psink_status is NULL\n", __func__);
		return -1;
	}

	if (!g_psink_status->fp_sec_pd_select_pdo) {
		pr_err("%s: not exist\n", __func__);
		return -1;
	}

	g_psink_status->fp_sec_pd_select_pdo(num);

	return 0;
}
EXPORT_SYMBOL(sec_pd_select_pdo);

int sec_pd_select_pps(int num, int ppsVol, int ppsCur)
{
	if (!g_psink_status) {
		pr_err("%s: g_psink_status is NULL\n", __func__);
		return -1;
	}

	if (!g_psink_status->fp_sec_pd_select_pps) {
		pr_err("%s: not exist\n", __func__);
		return -1;
	}

		return g_psink_status->fp_sec_pd_select_pps(num, ppsVol, ppsCur);
}
EXPORT_SYMBOL(sec_pd_select_pps);

int sec_pd_get_apdo_max_power(unsigned int *pdo_pos, unsigned int *taMaxVol, unsigned int *taMaxCur, unsigned int *taMaxPwr)
{
	int i;
	int ret = 0;
	int max_current = 0, max_voltage = 0, max_power = 0;

	if (!g_psink_status) {
		pr_err("%s: g_psink_status is NULL\n", __func__);
		return -1;
	}

	if (!g_psink_status->has_apdo) {
		pr_info("%s: pd don't have apdo\n", __func__);
		return -1;
	}

	/* First, get TA maximum power from the fixed PDO */
	for (i = 1; i <= g_psink_status->available_pdo_num; i++) {
		if (!(g_psink_status->power_list[i].apdo)) {
			max_voltage = g_psink_status->power_list[i].max_voltage;
			max_current = g_psink_status->power_list[i].max_current;
			max_power = (max_voltage * max_current > max_power) ? (max_voltage * max_current) : max_power;
			*taMaxPwr = max_power;	/* uW */
		}
	}

	if (*pdo_pos == 0) {
		/* Get the proper PDO */
		for (i = 1; i <= g_psink_status->available_pdo_num; i++) {
			if (g_psink_status->power_list[i].apdo
				&& g_psink_status->power_list[i].accept) {
				if (g_psink_status->power_list[i].max_voltage >= *taMaxVol) {
					*pdo_pos = i;
					*taMaxVol = g_psink_status->power_list[i].max_voltage;
					*taMaxCur = g_psink_status->power_list[i].max_current;
					break;
				}
			}
			if (*pdo_pos)
				break;
		}

		if (*pdo_pos == 0) {
			pr_info("mv (%d) and ma (%d) out of range of APDO\n",
				*taMaxVol, *taMaxCur);
			ret = -EINVAL;
		}
	} else {
		/* If we already have pdo object position, we don't need to search max current */
		ret = -ENOTSUPP;
	}

	pr_info("%s : *pdo_pos(%d), *taMaxVol(%d), *maxCur(%d), *maxPwr(%d)\n",
		__func__, *pdo_pos, *taMaxVol, *taMaxCur, *taMaxPwr);

	return ret;
}
EXPORT_SYMBOL(sec_pd_get_apdo_max_power);

int sec_pd_get_selected_pdo(unsigned int *pdo)
{
	if (pdo == NULL) {
		pr_err("%s: invalid argument\n", __func__);
		return -1;
	}

	if (!g_psink_status) {
		pr_err("%s: g_psink_status is NULL\n", __func__);
		return -1;
	}

	*pdo = g_psink_status->selected_pdo_num;
	return 0;
}
EXPORT_SYMBOL(sec_pd_get_selected_pdo);

void sec_pd_init_data(SEC_PD_SINK_STATUS* psink_status)
{
	g_psink_status = psink_status;
	if (g_psink_status)
		pr_info("%s: done.\n", __func__);
	else
		pr_err("%s: g_psink_status is NULL\n", __func__);
}
EXPORT_SYMBOL(sec_pd_init_data);

int sec_pd_register_chg_info_cb(void *cb)
{
	if (!g_psink_status) {
		pr_err("%s: g_psink_status is NULL\n", __func__);
		return -1;
	}
	g_psink_status->fp_sec_pd_ext_cb = cb;

	return 0;
}
EXPORT_SYMBOL(sec_pd_register_chg_info_cb);

void sec_pd_get_vid_pid(unsigned short *vid, unsigned short *pid, unsigned int *xid)
{
	if (!g_psink_status) {
		pr_err("%s: g_psink_status is NULL\n", __func__);
		return;
	}
	*vid = g_psink_status->vid;
	*pid = g_psink_status->pid;
	*xid = g_psink_status->xid;
}
EXPORT_SYMBOL(sec_pd_get_vid_pid);

void sec_pd_manual_ccopen_req(int is_on)
{
	if (!g_psink_status) {
		pr_err("%s: g_psink_status is NULL\n", __func__);
		return;
	}

	if (!g_psink_status->fp_sec_pd_manual_ccopen_req) {
		pr_err("%s: not exist\n", __func__);
		return;
	}

	g_psink_status->fp_sec_pd_manual_ccopen_req(is_on);
}
EXPORT_SYMBOL(sec_pd_manual_ccopen_req);

int sec_pd_change_src(int max_cur)
{
	if (!g_psink_status) {
		pr_err("%s: g_psink_status is NULL\n", __func__);
		return -1;
	}

	if (!g_psink_status->fp_sec_pd_change_src) {
		pr_err("%s: not exist\n", __func__);
		return -1;
	}

	g_psink_status->fp_sec_pd_change_src(max_cur);

	return 0;
}
EXPORT_SYMBOL(sec_pd_change_src);

static int __init sec_pd_init(void)
{
	pr_info("%s: \n", __func__);
	return 0;
}

module_init(sec_pd_init);
MODULE_DESCRIPTION("Samsung PD control");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
