#include "../pmucal_common.h"
#include "../pmucal_cpu.h"
#include "../pmucal_local.h"
#include "../pmucal_rae.h"
#include "../pmucal_system.h"
#include "../pmucal_powermode.h"

#include "flexpmu_cal_cpu_exynos2100.h"
#include "flexpmu_cal_local_exynos2100.h"
#include "flexpmu_cal_p2vmap_exynos2100.h"
#include "flexpmu_cal_system_exynos2100.h"
#include "flexpmu_cal_powermode_exynos2100.h"
#include "flexpmu_cal_define_exynos2100.h"

#if IS_ENABLED(CONFIG_CP_PMUCAL)
#include "../pmucal_cp.h"
#include "pmucal_cp_exynos2100.h"
#endif

#include "cmucal-node.c"
#include "cmucal-qch.c"
#include "cmucal-sfr.c"
#include "cmucal-vclk.c"
#include "cmucal-vclklut.c"

#include "clkout_exynos2100.c"
#include "acpm_dvfs_exynos2100.h"
#include "asv_exynos2100.h"
#include "../ra.h"
#include "cmu-pmu_map.h"
#include <linux/smc.h>

#include <soc/samsung/exynos-cpupm.h>

/* defines for EWF WA */
#include <soc/samsung/cmu_ewf.h>
#define EXYNOS2100_CMU_BUS0_BASE	(0x1A300000)
#define QCH_CON_TREX_D0_BUS0_QCH_OFFSET	(0x30f0)
#define IGNORE_FORCE_PM_EN		(2)

/* defines for PLL_MMC SSC settings */
#define EXYNOS2100_CMU_TOP_BASE		(0x1a330000)

#define PLL_CON0_PLL_MMC	(0x140)
#define PLL_CON1_PLL_MMC	(0x144)
#define PLL_CON2_PLL_MMC	(0x148)
#define PLL_CON3_PLL_MMC	(0x14c)
#define PLL_CON4_PLL_MMC	(0x150)
#define PLL_CON5_PLL_MMC	(0x154)

#define PLL_ENABLE_SHIFT	(31)
#define MANUAL_MODE		(0x2)
#define PLL_MMC_MUX_BUSY_SHIFT	(16)
#define MFR_MASK		(0xff)
#define MRR_MASK		(0x3f)
#define MFR_SHIFT		(16)
#define MRR_SHIFT		(24)
#define SSCG_EN			(16)

/* defines for CPUCL2 smpl_warn SW release */
//#define EXYNOS9830_CCMU_CPUCL2_BASE		(0x1d200000)
//#define CCMU_SMPL_WARN_CFG		(0x9c)
//#define SW_RELEASE			(1 << 26)

void __iomem *cmu_top;
void __iomem *cmu_bus0;
void __iomem *ccmu_cpucl2;

unsigned int frac_rpll_list[10];
unsigned int frac_rpll_size;

unsigned int exynos2100_frac_rpll_list[] = {
	PLL_AUD0,
	PLL_AUD1,
	PLL_MMC,
};

static int cmu_stable_done(void __iomem *cmu,
			unsigned char shift,
			unsigned int done,
			int usec)
{
	unsigned int result;

	do {
		result = get_bit(cmu, shift);

		if (result == done)
			return 0;
		udelay(1);
	} while (--usec > 0);

	return -EVCLKTIMEOUT;
}

int pll_mmc_enable(int enable)
{
	unsigned int reg;
	unsigned int cmu_mode;
	int ret;

	if (!cmu_top) {
		pr_err("%s: cmu_top cmuioremap failed\n", __func__);
		return -1;
	}

	/* set PLL to manual mode */
	cmu_mode = readl(cmu_top + PLL_CON1_PLL_MMC);
	writel(MANUAL_MODE, cmu_top + PLL_CON1_PLL_MMC);

	if (!enable) {
		/* select oscclk */
		reg = readl(cmu_top + PLL_CON0_PLL_MMC);
		reg &= ~(PLL_MUX_SEL);
		writel(reg, cmu_top + PLL_CON0_PLL_MMC);

		ret = cmu_stable_done(cmu_top + PLL_CON0_PLL_MMC, PLL_MMC_MUX_BUSY_SHIFT, 0, 100);
		if (ret)
			pr_err("pll mux change time out, \'PLL_MMC\'\n");
	}

	/* setting ENABLE of PLL */
	reg = readl(cmu_top + PLL_CON3_PLL_MMC);
	if (enable)
		reg |= 1 << PLL_ENABLE_SHIFT;
	else
		reg &= ~(1 << PLL_ENABLE_SHIFT);
	writel(reg, cmu_top + PLL_CON3_PLL_MMC);

	if (enable) {
		/* wait for PLL stable */
		ret = cmu_stable_done(cmu_top + PLL_CON3_PLL_MMC, PLL_STABLE_SHIFT, 1, 100);
		if (ret)
			pr_err("pll time out, \'PLL_MMC\' %d\n", enable);

		/* select FOUT_PLL_MMC */
		reg = readl(cmu_top + PLL_CON0_PLL_MMC);
		reg |= PLL_MUX_SEL;
		writel(reg, cmu_top + PLL_CON0_PLL_MMC);

		ret = cmu_stable_done(cmu_top + PLL_CON0_PLL_MMC, PLL_MMC_MUX_BUSY_SHIFT, 0, 100);
		if (ret)
			pr_err("pll mux change time out, \'PLL_MMC\'\n");
	}

	/* restore PLL mode */
	writel(cmu_mode, cmu_top + PLL_CON1_PLL_MMC);

	return ret;
}

int cal_pll_mmc_check(void)
{
       unsigned int reg;
       bool ret = false;

       reg = readl(cmu_top + PLL_CON4_PLL_MMC);

       if (reg & (1 << SSCG_EN))
               ret = true;

       return ret;
}EXPORT_SYMBOL(cal_pll_mmc_check);

int cal_pll_mmc_set_ssc(unsigned int mfr, unsigned int mrr, unsigned int ssc_on)
{
	unsigned int reg;
	int ret = 0;

	/* disable PLL_MMC */
	ret = pll_mmc_enable(0);
	if (ret) {
		pr_err("%s: pll_mmc_disable failed\n", __func__);
		return ret;
	}

	/* setting MFR, MRR */
	reg = readl(cmu_top + PLL_CON5_PLL_MMC);
	reg &= ~((MFR_MASK << MFR_SHIFT) | (MRR_MASK << MRR_SHIFT));

	if (ssc_on)
		reg |= ((mfr & MFR_MASK) << MFR_SHIFT) | ((mrr & MRR_MASK) << MRR_SHIFT);
	writel(reg, cmu_top + PLL_CON5_PLL_MMC);

	/* setting SSCG_EN */
	reg = readl(cmu_top + PLL_CON4_PLL_MMC);
	if (ssc_on)
		reg |= 1 << SSCG_EN;
	else
		reg &= ~(1 << SSCG_EN);
	writel(reg, cmu_top + PLL_CON4_PLL_MMC);

	/* enable PLL_MMC */
	ret = pll_mmc_enable(1);
	if (ret)
		pr_err("%s: pll_mmc_enable failed\n", __func__);

	return ret;
}EXPORT_SYMBOL(cal_pll_mmc_set_ssc);

void exynos2100_cal_data_init(void)
{
	int i;

	pr_info("%s: cal data init\n", __func__);

	/* cpu inform sfr initialize */
	pmucal_sys_powermode[SYS_SICD] = CPU_INFORM_SICD;
	pmucal_sys_powermode[SYS_SLEEP] = CPU_INFORM_SLEEP;
	pmucal_sys_powermode[SYS_SLEEP_HSI1ON] = CPU_INFORM_SLEEP_HSI1ON;

	cpu_inform_c2 = CPU_INFORM_C2;
	cpu_inform_cpd = CPU_INFORM_CPD;

	cmu_top = ioremap(EXYNOS2100_CMU_TOP_BASE, SZ_4K);
	if (!cmu_top)
		pr_err("%s: cmu_top ioremap failed\n", __func__);

	cmu_bus0 = ioremap(EXYNOS2100_CMU_BUS0_BASE, SZ_16K);
	if (!cmu_bus0)
		pr_err("%s: cmu_bus0 ioremap failed\n", __func__);

//	ccmu_cpucl2 = ioremap(EXYNOS2100_CCMU_CPUCL2_BASE, SZ_4K);
//	if (!ccmu_cpucl2)
//		pr_err("%s: ccmu_cpucl2 ioremap failed\n", __func__);

	frac_rpll_size = ARRAY_SIZE(exynos2100_frac_rpll_list);
	for (i = 0; i < frac_rpll_size; i++)
		frac_rpll_list[i] = exynos2100_frac_rpll_list[i];
}

void (*cal_data_init)(void) = exynos2100_cal_data_init;

static void __exynos2100_set_cmuewf(unsigned int index, unsigned int en, void *cmu_cmu)
{
	unsigned int reg;
	unsigned int reg_idx;

	if (index >= 32) {
		reg_idx = EARLY_WAKEUP_FORCED_ENABLE1;
		index = index - 32;
	} else {
		reg_idx = EARLY_WAKEUP_FORCED_ENABLE0;
	}

	if (en) {
		reg = __raw_readl(cmu_cmu + reg_idx);
		reg |= 1 << index;
		__raw_writel(reg, cmu_cmu + reg_idx);
	} else {
		reg = __raw_readl(cmu_cmu + reg_idx);
		reg &= ~(1 << index);
		__raw_writel(reg, cmu_cmu + reg_idx);
	}
}

int exynos2100_set_cmuewf(unsigned int index, unsigned int en, void *cmu_cmu, int *ewf_refcnt)
{
	unsigned int reg;
	int ret = 0;
	int tmp;

	if (en) {
		__exynos2100_set_cmuewf(index, en, cmu_cmu);

		reg = __raw_readl(cmu_bus0 + QCH_CON_TREX_D0_BUS0_QCH_OFFSET);
		reg |= 1 << IGNORE_FORCE_PM_EN;
		__raw_writel(reg, cmu_bus0 + QCH_CON_TREX_D0_BUS0_QCH_OFFSET);

		ewf_refcnt[index] += 1;
	} else {

		tmp = ewf_refcnt[index] - 1;

		if (tmp == 0) {
			reg = __raw_readl(cmu_bus0 + QCH_CON_TREX_D0_BUS0_QCH_OFFSET);
			reg &= ~(1 << IGNORE_FORCE_PM_EN);
			__raw_writel(reg, cmu_bus0 + QCH_CON_TREX_D0_BUS0_QCH_OFFSET);

			__exynos2100_set_cmuewf(index, en, cmu_cmu);

		} else if (tmp < 0) {
			pr_err("[EWF]%s ref count mismatch. ewf_index:%u\n",__func__,  index);

			ret = -EINVAL;
			goto exit;
		}

		ewf_refcnt[index] -= 1;
	}

exit:
	return ret;
}
int (*wa_set_cmuewf)(unsigned int index, unsigned int en, void *cmu_cmu, int *ewf_refcnt) = exynos2100_set_cmuewf;

void exynos2100_set_cmu_smpl_warn(void)
{
}
void (*cal_set_cmu_smpl_warn)(void) = exynos2100_set_cmu_smpl_warn;

char *exynos2100_get_pd_name_by_cmu(unsigned int addr)
{
	int i, map_size;

	map_size = (sizeof(cmu_pmu_map) / sizeof(struct cmu_pmu));
	for (i = 0; i < map_size; i++) {
		if (cmu_pmu_map[i].cmu == addr)
			break;
	}

	if (i < map_size)
		return cmu_pmu_map[i].pmu;
	else
		return NULL;
}
char *(*cal_get_pd_name_by_cmu)(unsigned int addr) = exynos2100_get_pd_name_by_cmu;
