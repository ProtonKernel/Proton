#include <linux/input/input_booster.h>
#include <linux/cpufreq.h>
#include <linux/sched.h>
#include <linux/ems.h>
#include <linux/syscalls.h>
#include <linux/module.h>
#include <soc/samsung/exynos_pm_qos.h>
#include <soc/samsung/exynos-ufcc.h>

struct exynos_pm_qos_request mif_qos;
struct exynos_pm_qos_request int_qos;
static bool ib_ufc_initialized;

struct ufc_req ib_ufc_req;

static DEFINE_MUTEX(input_lock);

void ufc_qos_init(void);

void ib_set_booster(long *qos_values)
{
	int res_type = 0;
	int cur_res_idx;
	long value = -1;

	for (res_type = 0; res_type < allowed_res_count; res_type++) {

		cur_res_idx = allowed_resources[res_type];
		value = qos_values[cur_res_idx];
		if (value <= 0)
			continue;

		switch (cur_res_idx) {
		case CLUSTER2:
			mutex_lock(&input_lock);
			if (ib_ufc_initialized) {
				ufc_update_request(&ib_ufc_req, value);
			} else {
				ufc_qos_init();
			}
			mutex_unlock(&input_lock);
			break;
		case MIF:
			exynos_pm_qos_update_request(&mif_qos, value);
			break;
		case INT:
			exynos_pm_qos_update_request(&int_qos, value);
			break;
		default:
			break;
		}
	}
}

void ib_release_booster(long *rel_flags)
{
	int res_type = 0;
	int cur_res_idx;
	long flag = -1;

	for (res_type = 0; res_type < allowed_res_count; res_type++) {

		cur_res_idx = allowed_resources[res_type];
		flag = rel_flags[cur_res_idx];

		if (flag <= 0)
			continue;

		switch (cur_res_idx) {
		case CLUSTER2:
			ufc_update_request(&ib_ufc_req, release_val[CLUSTER2]);
			break;
		case MIF:
			exynos_pm_qos_update_request(&mif_qos, release_val[MIF]);
			break;
		case INT:
			exynos_pm_qos_update_request(&int_qos, release_val[INT]);
			break;
		default:
			break;
		}
	}
}

void ufc_qos_init(void)
{
	int ret = 0;
	ret = ufc_add_request(&ib_ufc_req);

	if (ret < 0)
		ib_ufc_initialized = false;
	else
		ib_ufc_initialized = true;
}

int input_booster_init_vendor(void)
{
	int res_type = 0;
	for (res_type = 0; res_type < allowed_res_count; res_type++) {
		switch (allowed_resources[res_type]) {
		case CLUSTER2:
			if (ib_ufc_initialized)
				ufc_add_request(&ib_ufc_req)
			else
				ufc_qos_init();
			break;
		case MIF:
			exynos_pm_qos_add_request(&mif_qos,
				PM_QOS_BUS_THROUGHPUT, PM_QOS_BUS_THROUGHPUT_DEFAULT_VALUE);
			break;
		case INT:
			exynos_pm_qos_add_request(&int_qos,
				PM_QOS_DEVICE_THROUGHPUT, PM_QOS_DEVICE_THROUGHPUT_DEFAULT_VALUE);
			break;
		default:
			break;
		}
	}

	return 1;
}

void input_booster_exit_vendor(void)
{
	int res_type = 0;

	for (res_type = 0; res_type < allowed_res_count; res_type++) {
		switch (allowed_resources[res_type]) {
		case CLUSTER2:
			ufc_remove_request(&ib_ufc_req);
			break;
		case MIF:
			exynos_pm_qos_remove_request(&mif_qos);
			break;
		case INT:
			exynos_pm_qos_remove_request(&int_qos);
			break;
		default:
			break;
		}
	}
}
