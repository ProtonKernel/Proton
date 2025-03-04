#include <linux/input/input_booster.h>
#include <linux/device.h>
#include <linux/syscalls.h>

int current_hmp_boost = 0;
struct pm_qos_request lpm_bias_pm_qos_request;

static struct device *dev;
struct icc_path *path_touch_bw;
int register_ddr;

#define MHZ_TO_BPS(mhz, w) ((uint64_t)mhz * 1000 * 1000 * w)
#define MHZ_TO_KBPS(mhz, w) ((uint64_t)mhz * 1000 * w)

//Refer to "include/dt-bindings/interconnect/qcom,lahaina.h"
#define MASTER_APPSS_PROC				2
#define SLAVE_EBI1				512

void set_hmp(int level);

#ifndef CONFIG_CPU_FREQ_LIMIT
#define DVFS_TOUCH_ID	0
int set_freq_limit(unsigned long id, unsigned int freq)
{
	pr_err("%s is not yet implemented\n", __func__);
	return 0;
}
#else
	/* TEMP: for KPI */
#define DVFS_TOUCH_ID 1
#endif

#if defined(CONFIG_ARCH_LAHAINA)
#define NUM_BUS_TABLE 14
#define BUS_W 4	/* SM8350 DDR Voting('w' for DDR is 4) */

int ab_ib_bus_vectors[NUM_BUS_TABLE][2] = {
	//{ab, ib}
	{0, 0},		/* 0 */
	{0, 200},	/* 1 */
	{0, 451},	/* 2 */
	{0, 547},	/* 3 */
	{0, 681},	/* 4 */
	{0, 768},	/* 5 */

	{0, 1017},	/* 6 */
	{0, 1353},	/* 7 */
	{0, 1555},	/* 8 */
	{0, 1708},	/* 9 */
	{0, 2092},	/* 10 */
	{0, 2133},	/* 11 */
	{0, 2736},	/* 12 */
	{0, 3196}	/* 13 */
};
#else
#define NUM_BUS_TABLE 1
#define BUS_W 0

int ab_ib_bus_vectors[NUM_BUS_TABLE][2] = {
	{0, 0},		/* 0 */
};
#endif	//set null for other chipset


struct msm_touch_bus_vector_bps {
	uint32_t ab;
	uint32_t ib;
};
struct msm_touch_bus_vector_bps touch_bus_vectors_bps[NUM_BUS_TABLE];
void fill_bus_vector(void)
{
	int i = 0;

	for (i = 0; i < NUM_BUS_TABLE; i++) {
		touch_bus_vectors_bps[i].ab = ab_ib_bus_vectors[i][0];
		touch_bus_vectors_bps[i].ib = MHZ_TO_KBPS(ab_ib_bus_vectors[i][1], BUS_W);
	}
}

int trans_freq_to_idx(long request_ddr_freq)
{
	int i = 0;

	if (request_ddr_freq <= 0) {
		return 0;
	}

	// in case of null table, return 0
	if (NUM_BUS_TABLE == 1) {
		return 0;
	}

	for (i = 0; i < NUM_BUS_TABLE-1; i++) {
		if (request_ddr_freq > ab_ib_bus_vectors[i][1] &&
			request_ddr_freq <= ab_ib_bus_vectors[i+1][1]) {
			return (i+1);
		}
	}

	return i+1;
}

void ib_set_booster(long* qos_values)
{
	long value = -1;
	int ddr_idx = 0;
	int res_type =0;
	int cur_res_idx;
	int rc = 0;

	for (res_type = 0; res_type < allowed_res_count; res_type++) {

		cur_res_idx = allowed_resources[res_type];
		value = qos_values[cur_res_idx];

		if (value <= 0)
			continue;

		switch (cur_res_idx) {
		case CPUFREQ:
			set_freq_limit(DVFS_TOUCH_ID, value);
			pr_booster("%s :: cpufreq value : %ld", __func__, value);
			break;
		case DDRFREQ:
			ddr_idx = trans_freq_to_idx(value);
			pr_booster("%s :: bus value : %ld, %u", __func__, value, touch_bus_vectors_bps[ddr_idx].ib);
			if (register_ddr)
				rc = icc_set_bw(path_touch_bw,
						touch_bus_vectors_bps[ddr_idx].ab, touch_bus_vectors_bps[ddr_idx].ib);
			break;
		case HMPBOOST:
			set_hmp(value);
			pr_booster("%s :: hmpboost value : %ld", __func__, value);
			break;
		case LPMBIAS:
			pm_qos_update_request(&lpm_bias_pm_qos_request, value);
			pr_booster("%s :: LPM Bias value : %ld", __func__, value);
			break;
		default:
			break;
		}
	}
}

void ib_release_booster(long *rel_flags)
{
	//cpufreq : -1, ddrfreq : 0, HMP : 0, lpm_bias = 0
	int ddr_idx;
	int flag;
	int rc = 0;
	int value;

	int res_type = 0;
	int cur_res_idx;

	for (res_type = 0; res_type < allowed_res_count; res_type++) {

		cur_res_idx = allowed_resources[res_type];
		flag = rel_flags[cur_res_idx];
		if (flag <= 0)
			continue;

		value = release_val[cur_res_idx];

		switch (cur_res_idx) {
		case CPUFREQ:
			set_freq_limit(DVFS_TOUCH_ID, value);
			break;
		case DDRFREQ:
			ddr_idx = trans_freq_to_idx(value);
			if (register_ddr)
				rc = icc_set_bw(path_touch_bw,
						touch_bus_vectors_bps[ddr_idx].ab, touch_bus_vectors_bps[ddr_idx].ib);
			break;
		case HMPBOOST:
			set_hmp(value);
			break;
		case LPMBIAS:
			pm_qos_update_request(&lpm_bias_pm_qos_request, value);
			break;
		default:
			break;
		}
	}
}

#ifdef USE_HMP_BOOST
void set_hmp(int level)
{
	if (level != current_hmp_boost) {
		if (level == 0) {
			level = -current_hmp_boost;
			current_hmp_boost = 0;
		} else {
			current_hmp_boost = level;
		}
		pr_booster("[Input Booster2] ******     hmp_boost : %d ( %s )\n", level, __func__);
		if (sched_set_boost(level) < 0)
			pr_err("[Input Booster2] ******            !!! fail to HMP !!!\n");
	}
}
#else
void set_hmp(int level)
{
	pr_booster("It does not use hmp\n", level, __func__);
}
#endif

static void icc_release(struct device *dev)
{
	pr_booster("Release icc path of device\n");
}

int input_booster_init_vendor(void)
{

	int bus_ret = 0;

//Bus boost factors initailized
	dev = kzalloc(sizeof(struct device), GFP_KERNEL);
	dev_set_name(dev, "input_booster");
	dev->release = icc_release;
	device_register(dev);

	if (!register_ddr) {
		path_touch_bw = icc_get(dev, MASTER_APPSS_PROC, SLAVE_EBI1);
		if (IS_ERR(path_touch_bw)) {
			bus_ret = PTR_ERR(path_touch_bw);
			if (bus_ret != -EPROBE_DEFER)
				pr_err("Failed to get path from input bus booster. ret=%d\n", bus_ret);
		} else {
			pr_info(ITAG" Making icc path for bus success");
			register_ddr = 1;
		}
	}

	fill_bus_vector();
	pm_qos_add_request(&lpm_bias_pm_qos_request,
		PM_QOS_BIAS_HYST, PM_QOS_BIAS_HYST_DEFAULT_VALUE);
	return 1;
}

void input_booster_exit_vendor(void)
{
	if (register_ddr)
		icc_put(path_touch_bw);

	device_unregister(dev);
	kfree(dev);
	pm_qos_remove_request(&lpm_bias_pm_qos_request);
}

MODULE_LICENSE("GPL");
