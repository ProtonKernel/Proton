#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include <soc/samsung/cal-if.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include "fvmap.h"
#include "cmucal.h"
#include "vclk.h"
#include "ra.h"

#define FVMAP_SIZE		(SZ_8K)
#define STEP_UV			(6250)

void __iomem *fvmap_base;
void __iomem *sram_fvmap_base;

static int init_margin_table[MAX_MARGIN_ID];
static int percent_margin_table[MAX_MARGIN_ID];

static int margin_mif;
static int margin_int;
static int margin_cpucl0;
static int margin_cpucl1;
static int margin_cpucl2;
static int margin_npu;
static int margin_dsu;
static int margin_disp;
static int margin_aud;
static int margin_cp;
static int margin_modem;
static int margin_g3d;
static int margin_intcam;
static int margin_cam;
static int margin_csis;
static int margin_isp;
static int margin_vpc;
static int margin_mfc;
static int margin_mfc1;
static int margin_intsci;
static int volt_offset_percent;
module_param(margin_mif, int, 0);
module_param(margin_int, int, 0);
module_param(margin_cpucl0, int, 0);
module_param(margin_cpucl1, int, 0);
module_param(margin_cpucl2, int, 0);
module_param(margin_npu, int, 0);
module_param(margin_dsu, int, 0);
module_param(margin_disp, int, 0);
module_param(margin_aud, int, 0);
module_param(margin_cp, int, 0);
module_param(margin_modem, int, 0);
module_param(margin_g3d, int, 0);
module_param(margin_intcam, int, 0);
module_param(margin_cam, int, 0);
module_param(margin_csis, int, 0);
module_param(margin_isp, int, 0);
module_param(margin_vpc, int, 0);
module_param(margin_mfc, int, 0);
module_param(margin_mfc1, int, 0);
module_param(margin_intsci, int, 0);
module_param(volt_offset_percent, int, 0);

void margin_table_init(void)
{
	init_margin_table[MARGIN_MIF] = margin_mif;
	init_margin_table[MARGIN_INT] = margin_int;
	init_margin_table[MARGIN_CPUCL0] = margin_cpucl0;
	init_margin_table[MARGIN_CPUCL1] = margin_cpucl1;
	init_margin_table[MARGIN_CPUCL2] = margin_cpucl2;
	init_margin_table[MARGIN_NPU] = margin_npu;
	init_margin_table[MARGIN_DSU] = margin_dsu;
	init_margin_table[MARGIN_DISP] = margin_disp;
	init_margin_table[MARGIN_AUD] = margin_aud;
	init_margin_table[MARGIN_CP] = margin_cp;
	init_margin_table[MARGIN_MODEM] = margin_modem;
	init_margin_table[MARGIN_G3D] = margin_g3d;
	init_margin_table[MARGIN_INTCAM] = margin_intcam;
	init_margin_table[MARGIN_CAM] = margin_cam;
	init_margin_table[MARGIN_CSIS] = margin_csis;
	init_margin_table[MARGIN_ISP] = margin_isp;
	init_margin_table[MARGIN_VPC] = margin_vpc;
	init_margin_table[MARGIN_MFC] = margin_mfc;
	init_margin_table[MARGIN_MFC1] = margin_mfc1;
	init_margin_table[MARGIN_INTSCI] = margin_intsci;
}

int fvmap_set_raw_voltage_table(unsigned int id, int uV)
{
	struct fvmap_header *fvmap_header;
	struct rate_volt_header *fv_table;
	int num_of_lv;
	int idx, i;

	idx = GET_IDX(id);

	fvmap_header = sram_fvmap_base;
	fv_table = sram_fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < num_of_lv; i++)
		fv_table->table[i].volt += uV / STEP_UV;

	return 0;
}

static int get_vclk_id_from_margin_id(int margin_id)
{
	int size = cmucal_get_list_size(ACPM_VCLK_TYPE);
	int i;
	struct vclk *vclk;

	for (i = 0; i < size; i++) {
		vclk = cmucal_get_node(ACPM_VCLK_TYPE | i);

		if (vclk->margin_id == margin_id)
			return i;
	}

	return -EINVAL;
}

#define attr_percent(margin_id, type)								\
static ssize_t show_##type##_percent								\
(struct kobject *kobj, struct kobj_attribute *attr, char *buf)					\
{												\
	return snprintf(buf, PAGE_SIZE, "%d\n", percent_margin_table[margin_id]);		\
}												\
												\
static ssize_t store_##type##_percent								\
(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)		\
{												\
	int input, vclk_id;									\
												\
	if (!sscanf(buf, "%d", &input))								\
		return -EINVAL;									\
												\
	if (input < -100 || input > 100)							\
		return -EINVAL;									\
												\
	vclk_id = get_vclk_id_from_margin_id(margin_id);					\
	if (vclk_id == -EINVAL)									\
		return vclk_id;									\
	percent_margin_table[margin_id] = input;						\
	cal_dfs_set_volt_margin(vclk_id | ACPM_VCLK_TYPE, input);				\
												\
	return count;										\
}												\
												\
static struct kobj_attribute type##_percent =							\
__ATTR(type##_percent, 0600,									\
	show_##type##_percent, store_##type##_percent)

attr_percent(MARGIN_MIF, mif_margin);
attr_percent(MARGIN_INT, int_margin);
attr_percent(MARGIN_CPUCL0, cpucl0_margin);
attr_percent(MARGIN_CPUCL1, cpucl1_margin);
attr_percent(MARGIN_CPUCL2, cpucl2_margin);
attr_percent(MARGIN_NPU, npu_margin);
attr_percent(MARGIN_DSU, dsu_margin);
attr_percent(MARGIN_DISP, disp_margin);
attr_percent(MARGIN_AUD, aud_margin);
attr_percent(MARGIN_CP, cp_margin);
attr_percent(MARGIN_MODEM, modem_margin);
attr_percent(MARGIN_G3D, g3d_margin);
attr_percent(MARGIN_INTCAM, intcam_margin);
attr_percent(MARGIN_CAM, cam_margin);
attr_percent(MARGIN_CSIS, csis_margin);
attr_percent(MARGIN_ISP, isp_margin);
attr_percent(MARGIN_VPC, vpc_margin);
attr_percent(MARGIN_MFC, mfc_margin);
attr_percent(MARGIN_MFC1, mfc1_margin);
attr_percent(MARGIN_INTSCI, intsci_margin);

static struct attribute *percent_margin_attrs[] = {
	&mif_margin_percent.attr,
	&int_margin_percent.attr,
	&cpucl0_margin_percent.attr,
	&cpucl1_margin_percent.attr,
	&cpucl2_margin_percent.attr,
	&npu_margin_percent.attr,
	&dsu_margin_percent.attr,
	&disp_margin_percent.attr,
	&aud_margin_percent.attr,
	&cp_margin_percent.attr,
	&modem_margin_percent.attr,
	&g3d_margin_percent.attr,
	&intcam_margin_percent.attr,
	&cam_margin_percent.attr,
	&csis_margin_percent.attr,
	&isp_margin_percent.attr,
	&vpc_margin_percent.attr,
	&mfc_margin_percent.attr,
	&mfc1_margin_percent.attr,
	&intsci_margin_percent.attr,
	NULL,
};

static const struct attribute_group percent_margin_group = {
	.attrs = percent_margin_attrs,
};

#ifdef CONFIG_SEC_FACTORY
#include <soc/samsung/ect_parser.h>
#include <soc/samsung/cal-if.h>
#include "asv.h"

enum spec_volt_type {
	ASV_G_FUSED = 0,
	ASV_G_GRP
};

extern struct vclk acpm_vclk_list[];
extern unsigned int acpm_vclk_size;
#define get_domain_name(type)	(acpm_vclk_list[type].name)
#define SPEC_STR_SIZE		(30)

static ssize_t show_asv_g_spec(int id, enum spec_volt_type type,
		char *buf, unsigned int fused_addr, unsigned int bit_num)
{
	void *gen_block;
	struct ect_gen_param_table *spec;
	int asv_tbl_ver, asv_grp, tbl_size, j, vtyp_freq, num_lv;
	unsigned int fused_volt, grp_volt = 0, volt;
	struct dvfs_rate_volt rate_volt[48];
	unsigned int *spec_table = NULL;
	ssize_t size = 0, len = 0;
	char spec_str[SPEC_STR_SIZE] = {0, };
	unsigned long fused_val;
	void __iomem *fused_addr_va;

	if (id >= acpm_vclk_size) {
		pr_err("%s: cannot found dvfs domain: %d\n", __func__, id);
		goto out;
	}

	gen_block = ect_get_block("GEN");
	if (gen_block == NULL) {
		pr_err("%s: Failed to get gen block from ECT\n", __func__);
		goto out;
	}

	asv_grp = asv_get_grp(id | ACPM_VCLK_TYPE);
	if (!asv_grp) {
		pr_err("%s: There has no ASV-G information for %s group 0\n",
				__func__, get_domain_name(id));
		goto out;
	}

	len = snprintf(spec_str, SPEC_STR_SIZE, "SPEC_%s", get_domain_name(id));
	if (len < 0)
		goto out;

	spec = ect_gen_param_get_table(gen_block, spec_str);
	if (spec == NULL) {
		pr_err("%s: Failed to get spec table from ECT\n", __func__);
		goto out;
	}

	asv_tbl_ver = asv_get_table_ver();
	for (j = 0; j < spec->num_of_row; j++) {
		spec_table = &spec->parameter[spec->num_of_col * j];
		if (spec_table[0] == asv_tbl_ver) {
			grp_volt = spec_table[asv_grp + 2];
			vtyp_freq = spec_table[1];
			break;
		}
	}
	if (j == spec->num_of_row) {
		pr_err("%s: Do not support ASV-G, asv table version:%d\n", __func__, asv_tbl_ver);
		goto out;
	}

	if (!grp_volt) {
		pr_err("%s: Failed to get grp volt\n", __func__);
		goto out;
	}

	num_lv = cal_dfs_get_lv_num(id | ACPM_VCLK_TYPE);
	tbl_size = cal_dfs_get_rate_asv_table(id | ACPM_VCLK_TYPE, rate_volt);
	if (!tbl_size) {
		pr_err("%s: Failed to get asv table\n", __func__);
		goto out;
	}

	if (fused_addr) {
		if ((fused_addr & 0x9000) == 0x9000) {
			exynos_smc_readsfr((unsigned long)fused_addr, &fused_val);
		} else {
			fused_addr_va = ioremap(fused_addr, 4);
			fused_val = __raw_readl(fused_addr_va);
			iounmap(fused_addr_va);
		}

		fused_volt = (((unsigned int)fused_val & (0xff << bit_num)) >> bit_num) * 6250;
	} else {
		for (j = 0; j < num_lv; j++){
			if (rate_volt[j].rate == vtyp_freq) {
				fused_volt = rate_volt[j].volt;
				break;
			}
		}
		if (j == num_lv) {
			pr_err("%s: There has no frequency %d on %d domain\n", __func__,
					vtyp_freq, get_domain_name(id));
			goto out;
		}
	}

	volt = (type == ASV_G_FUSED) ? fused_volt : grp_volt;

	size += snprintf(buf + size, PAGE_SIZE, "%d\n", volt);
out:

	return size;
}

#define asv_g_spec(domain, id, fused_addr, bit_num)						\
static ssize_t show_asv_g_spec_##domain##_fused_volt						\
(struct kobject *kobj, struct kobj_attribute *attr, char *buf)					\
{												\
	return show_asv_g_spec(id, ASV_G_FUSED, buf, fused_addr, bit_num);			\
}												\
static struct kobj_attribute asv_g_spec_##domain##_fused_volt =					\
__ATTR(domain##_fused_volt, 0400, show_asv_g_spec_##domain##_fused_volt, NULL);			\
static ssize_t show_asv_g_spec_##domain##_grp_volt						\
(struct kobject *kobj, struct kobj_attribute *attr, char *buf)					\
{												\
	return show_asv_g_spec(id, ASV_G_GRP, buf, fused_addr, bit_num);			\
}												\
static struct kobj_attribute asv_g_spec_##domain##_grp_volt =					\
__ATTR(domain##_grp_volt, 0400, show_asv_g_spec_##domain##_grp_volt, NULL)

#define asv_g_spec_attr(domain)									\
	&asv_g_spec_##domain##_fused_volt.attr,							\
	&asv_g_spec_##domain##_grp_volt.attr

asv_g_spec(cpucl2, 4, 0x10009010, 8);
asv_g_spec(cpucl1, 3, 0x10009014, 8);
asv_g_spec(cpucl0, 2, 0x10009018, 8);
asv_g_spec(g3d, 11, 0x1000901C, 16);
asv_g_spec(mif, 0, 0x10000018, 8);
asv_g_spec(dsu, 6, 0x1000001C, 8);
asv_g_spec(intsci, 19, 0x10000020, 8);
asv_g_spec(modem, 10, 0, 24);

static struct attribute *asv_g_spec_attrs[] = {
	asv_g_spec_attr(cpucl2),
	asv_g_spec_attr(cpucl1),
	asv_g_spec_attr(cpucl0),
	asv_g_spec_attr(g3d),
	asv_g_spec_attr(mif),
	asv_g_spec_attr(dsu),
	asv_g_spec_attr(intsci),
	asv_g_spec_attr(modem),
	NULL,
};

static const struct attribute_group asv_g_spec_grp = {
	.attrs = asv_g_spec_attrs,
};
#endif	/* CONFIG_SEC_FACTORY */

unsigned int dvfs_calibrate_voltage(unsigned int rate_target, unsigned int rate_up,
		unsigned int rate_down, unsigned int volt_up, unsigned int volt_down)
{
	unsigned int volt_diff_step;
	unsigned int rate_diff;
	unsigned int rate_per_step;
	unsigned int ret;

	if (rate_up < 0x100)
		return volt_down * STEP_UV;

	if (rate_down == 0)
		return volt_up * STEP_UV;

	if ((rate_up == rate_down) || (volt_up == volt_down))
		return volt_up * STEP_UV;

	volt_diff_step = (volt_up - volt_down);
	rate_diff = rate_up - rate_down;
	rate_per_step = rate_diff / volt_diff_step;
	ret = (unsigned int)((volt_up - ((rate_up - rate_target) / rate_per_step)) + 0) * STEP_UV;

	return ret;
}

int fvmap_get_freq_volt_table(unsigned int id, void *freq_volt_table, unsigned int table_size)
{
	struct fvmap_header *fvmap_header = fvmap_base;
	struct rate_volt_header *fv_table;
	int idx, i, j;
	int num_of_lv;
	unsigned int target, rate_up, rate_down, volt_up, volt_down;
	struct freq_volt *table = (struct freq_volt *)freq_volt_table;

	if (!IS_ACPM_VCLK(id))
		return -EINVAL;

	if (!table)
		return -ENOMEM;

	idx = GET_IDX(id);

	fvmap_header = fvmap_base;
	fv_table = fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < table_size; i++) {
		for (j = 1; j < num_of_lv; j++) {
			rate_up = fv_table->table[j - 1].rate;
			rate_down = fv_table->table[j].rate;
			if ((table[i].rate <= rate_up) && (table[i].rate >= rate_down)) {
				volt_up = fv_table->table[j - 1].volt;
				volt_down = fv_table->table[j].volt;
				target = table[i].rate;
				table[i].volt = dvfs_calibrate_voltage(target,
						rate_up, rate_down, volt_up, volt_down);
				break;
			}
		}

		if (table[i].volt == 0) {
			if (table[i].rate > fv_table->table[0].rate)
				table[i].volt = fv_table->table[0].volt * STEP_UV;
			else if (table[i].rate < fv_table->table[num_of_lv - 1].rate)
				table[i].volt = fv_table->table[num_of_lv - 1].volt * STEP_UV;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(fvmap_get_freq_volt_table);

int fvmap_get_voltage_table(unsigned int id, unsigned int *table)
{
	struct fvmap_header *fvmap_header = fvmap_base;
	struct rate_volt_header *fv_table;
	int idx, i;
	int num_of_lv;

	if (!IS_ACPM_VCLK(id))
		return 0;

	idx = GET_IDX(id);

	fvmap_header = fvmap_base;
	fv_table = fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < num_of_lv; i++)
		table[i] = fv_table->table[i].volt * STEP_UV;

	return num_of_lv;

}
EXPORT_SYMBOL_GPL(fvmap_get_voltage_table);

int fvmap_get_raw_voltage_table(unsigned int id)
{
	struct fvmap_header *fvmap_header;
	struct rate_volt_header *fv_table;
	int idx, i;
	int num_of_lv;
	unsigned int table[20];

	idx = GET_IDX(id);

	fvmap_header = sram_fvmap_base;
	fv_table = sram_fvmap_base + fvmap_header[idx].o_ratevolt;
	num_of_lv = fvmap_header[idx].num_of_lv;

	for (i = 0; i < num_of_lv; i++)
		table[i] = fv_table->table[i].volt * STEP_UV;

	for (i = 0; i < num_of_lv; i++)
		printk("dvfs id : %d  %d Khz : %d uv\n", ACPM_VCLK_TYPE | id, fv_table->table[i].rate, table[i]);

	return 0;
}

static void fvmap_copy_from_sram(void __iomem *map_base, void __iomem *sram_base)
{
	struct fvmap_header *fvmap_header, *header;
	struct rate_volt_header *old, *new;
	struct clocks *clks;
	struct pll_header *plls;
	struct vclk *vclk;
	unsigned int member_addr;
	unsigned int blk_idx;
	int size, margin;
	int i, j;

	fvmap_header = map_base;
	header = sram_base;

	size = cmucal_get_list_size(ACPM_VCLK_TYPE);

	for (i = 0; i < size; i++) {
		/* load fvmap info */
		fvmap_header[i].domain_id = header[i].domain_id;
		fvmap_header[i].num_of_lv = header[i].num_of_lv;
		fvmap_header[i].num_of_members = header[i].num_of_members;
		fvmap_header[i].num_of_pll = header[i].num_of_pll;
		fvmap_header[i].num_of_mux = header[i].num_of_mux;
		fvmap_header[i].num_of_div = header[i].num_of_div;
		fvmap_header[i].o_famrate = header[i].o_famrate;
		fvmap_header[i].init_lv = header[i].init_lv;
		fvmap_header[i].num_of_child = header[i].num_of_child;
		fvmap_header[i].parent_id = header[i].parent_id;
		fvmap_header[i].parent_offset = header[i].parent_offset;
		fvmap_header[i].block_addr[0] = header[i].block_addr[0];
		fvmap_header[i].block_addr[1] = header[i].block_addr[1];
		fvmap_header[i].block_addr[2] = header[i].block_addr[2];
		fvmap_header[i].o_members = header[i].o_members;
		fvmap_header[i].o_ratevolt = header[i].o_ratevolt;
		fvmap_header[i].o_tables = header[i].o_tables;

		vclk = cmucal_get_node(ACPM_VCLK_TYPE | i);
		if (vclk == NULL)
			continue;
#ifdef CONFIG_EXYNOS_DEBUG_INFO
		pr_info("domain_id : %s - id : %x\n",
			vclk->name, fvmap_header[i].domain_id);
		pr_info("  num_of_lv      : %d\n", fvmap_header[i].num_of_lv);
		pr_info("  num_of_members : %d\n", fvmap_header[i].num_of_members);
#endif
		old = sram_base + fvmap_header[i].o_ratevolt;
		new = map_base + fvmap_header[i].o_ratevolt;

		margin = init_margin_table[vclk->margin_id];
		if (margin)
			cal_dfs_set_volt_margin(i | ACPM_VCLK_TYPE, margin);

		if (volt_offset_percent) {
			if ((volt_offset_percent < 100) && (volt_offset_percent > -100)) {
				percent_margin_table[i] = volt_offset_percent;
				cal_dfs_set_volt_margin(i | ACPM_VCLK_TYPE, volt_offset_percent);
			}
		}

		for (j = 0; j < fvmap_header[i].num_of_members; j++) {
			clks = sram_base + fvmap_header[i].o_members;

			if (j < fvmap_header[i].num_of_pll) {
				plls = sram_base + clks->addr[j];
				member_addr = plls->addr - 0x90000000;
			} else {

				member_addr = (clks->addr[j] & ~0x3) & 0xffff;
				blk_idx = clks->addr[j] & 0x3;

				member_addr |= ((fvmap_header[i].block_addr[blk_idx]) << 16) - 0x90000000;
			}

			vclk->list[j] = cmucal_get_id_by_addr(member_addr);
#ifdef CONFIG_EXYNOS_DEBUG_INFO
			if (vclk->list[j] == INVALID_CLK_ID)
				pr_info("  Invalid addr :0x%x\n", member_addr);
			else
				pr_info("  DVFS CMU addr:0x%x\n", member_addr);
#endif
		}
#ifdef CONFIG_EXYNOS_DEBUG_INFO
		for (j = 0; j < fvmap_header[i].num_of_lv; j++) {
			new->table[j].rate = old->table[j].rate;
			new->table[j].volt = old->table[j].volt;
			pr_info("  lv : [%7d], volt = %d uV (%d %%) \n",
				new->table[j].rate, new->table[j].volt * STEP_UV,
				volt_offset_percent);
		}
#endif
	}
}

int fvmap_init(void __iomem *sram_base)
{
	void __iomem *map_base;
	struct kobject *kobj;

	map_base = kzalloc(FVMAP_SIZE, GFP_KERNEL);

	fvmap_base = map_base;
	sram_fvmap_base = sram_base;
	pr_info("%s:fvmap initialize %p\n", __func__, sram_base);
	margin_table_init();
	fvmap_copy_from_sram(map_base, sram_base);

	/* percent margin for each doamin at runtime */
	kobj = kobject_create_and_add("percent_margin", kernel_kobj);
	if (!kobj)
		pr_err("Fail to create percent_margin kboject\n");

	if (sysfs_create_group(kobj, &percent_margin_group))
		pr_err("Fail to create percent_margin group\n");

#ifdef CONFIG_SEC_FACTORY
	kobj = kobject_create_and_add("asv-g", kernel_kobj);
	if (!kobj)
		pr_err("Fail to create asv-g kboject\n");

	if (sysfs_create_group(kobj, &asv_g_spec_grp))
		pr_err("Fail to create asv_g_spec group\n");
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(fvmap_init);

MODULE_LICENSE("GPL");
