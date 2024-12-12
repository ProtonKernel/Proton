/*
 * Exynos CPU Performance Profiling
 * Author: Jungwook Kim, jwook1.kim@samsung.com
 * Created: 2014
 * Updated: 2015, 2019
 */

#ifndef EXYNOS_PERF_CPU_H
#define EXYNOS_PERF_CPU_H

struct register_type {
	const char *name;
	u64 (*read_reg)(void);
};

struct core_register {
	struct register_type *reg;
	u64 val;
};

#define MRS_ASM(func_name, reg_name) static inline u64 mrs_##func_name##_read(void) \
	{	\
		u64 val;	\
		asm volatile("mrs %0, "#reg_name : "=r"(val));	\
		return val;	\
	}

/* Cortex-A57/A53 registers */
MRS_ASM(SCTLR, sctlr_el1)
MRS_ASM(MAIR, mair_el1)
MRS_ASM(MPIDR, mpidr_el1)
MRS_ASM(MIDR, midr_el1)
MRS_ASM(REVIDR, revidr_el1)

MRS_ASM(CPUACTLR, s3_1_c15_c2_0)
MRS_ASM(CPUECTLR, s3_1_c15_c2_1)
MRS_ASM(L2CTLR, s3_1_c11_c0_2)
MRS_ASM(L2ACTLR, s3_1_c15_c0_0)
MRS_ASM(L2ECTLR, s3_1_c11_c0_3)

/* ananke */
MRS_ASM(CPUACTLR_v82, s3_0_c15_c1_0)
MRS_ASM(CPUACTLR2_v82, s3_0_c15_c1_1)
MRS_ASM(CPUACTLR3_v82, s3_0_c15_c1_2)
MRS_ASM(CPUECTLR_v82, s3_0_c15_c1_4)
MRS_ASM(CPUPWRCTLR, s3_0_c15_c2_7)
MRS_ASM(CLUSTERPWRCTLR, s3_0_c15_c3_5)
MRS_ASM(CLUSTERECTLR, s3_0_c15_c3_4)

/* MONTBLANC */
MRS_ASM(PPMPDPCR, s3_0_c15_c2_4)

#endif
