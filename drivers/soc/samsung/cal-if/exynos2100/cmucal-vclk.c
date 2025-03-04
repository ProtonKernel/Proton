#include "../cmucal.h"
#include "cmucal-node.h"
#include "cmucal-vclk.h"
#include "cmucal-vclklut.h"

/* DVFS VCLK -> Clock Node List */
enum clk_id cmucal_vclk_vdd_mif[] = {
	PLL_MIF_MAIN,
	PLL_MIF_SUB,
};
enum clk_id cmucal_vclk_vdd_cam[] = {
	DIV_CLK_AUD_CPU_PCLKDBG,
	DIV_CLK_AUD_CPU,
	DIV_CLK_AUD_BUS,
	DIV_CLK_AUD_AUDIF,
	CLKAUD_HSI0_BUS,
	CLKAUD_HSI0_USB31DRD,
	PLL_AUD0,
};
enum clk_id cmucal_vclk_vdd_cpucl2[] = {
	PLL_CPUCL2,
};
enum clk_id cmucal_vclk_vdd_cpucl0[] = {
	PLL_CPUCL0,
	DIV_CLK_CLUSTER_BCLK,
	PLL_DSU,
};
enum clk_id cmucal_vclk_vdd_cpucl1[] = {
	PLL_CPUCL1,
};
enum clk_id cmucal_vclk_vdd_int_cmu[] = {
	CLKCMU_PERIS_BUS,
	MUX_CLKCMU_PERIS_BUS,
	MUX_CLKCMU_HSI0_USBDP_DEBUG,
	CLKCMU_CPUCL0_SWITCH,
	MUX_CLKCMU_CPUCL0_SWITCH,
	CLKCMU_CPUCL1_SWITCH,
	MUX_CLKCMU_CPUCL1_SWITCH,
	CLKCMU_CPUCL2_SWITCH,
	MUX_CLKCMU_CPUCL2_SWITCH,
	CLKCMU_DSU_SWITCH,
	MUX_CLKCMU_DSU_SWITCH,
	CLKCMU_CORE_BUS,
	MUX_CLKCMU_CORE_BUS,
	CLKCMU_CSIS_CSIS,
	MUX_CLKCMU_CSIS_CSIS,
	CLKCMU_DNS_BUS,
	MUX_CLKCMU_DNS_BUS,
	CLKCMU_ITP_BUS,
	MUX_CLKCMU_ITP_BUS,
	CLKCMU_LME_BUS,
	MUX_CLKCMU_LME_BUS,
	CLKCMU_MCFP0_BUS,
	MUX_CLKCMU_MCFP0_BUS,
	CLKCMU_MCFP1_ORBMCH,
	MUX_CLKCMU_MCFP1_ORBMCH,
	CLKCMU_MCSC_BUS,
	MUX_CLKCMU_MCSC_BUS,
	CLKCMU_MCSC_GDC,
	MUX_CLKCMU_MCSC_GDC,
	CLKCMU_MFC0_MFC0,
	MUX_CLKCMU_MFC0_MFC0,
	CLKCMU_MFC1_MFC1,
	MUX_CLKCMU_MFC1_MFC1,
	CLKCMU_NPUS_BUS,
	MUX_CLKCMU_NPUS_BUS,
	CLKCMU_TAA_BUS,
	MUX_CLKCMU_TAA_BUS,
	CLKCMU_VPC_BUS,
	MUX_CLKCMU_VPC_BUS,
	CLKCMU_VPD_BUS,
	MUX_CLKCMU_VPD_BUS,
	CLKCMU_YUVPP_BUS,
	MUX_CLKCMU_YUVPP_BUS,
	CLKCMU_YUVPP_FRC,
	MUX_CLKCMU_YUVPP_FRC,
	CLKCMU_CSIS_PDP,
	MUX_CLKCMU_CSIS_PDP,
	CLKCMU_MCFP1_MCFP1,
	MUX_CLKCMU_MCFP1_MCFP1,
	CLKCMU_NPU_BUS,
	MUX_CLKCMU_NPU_BUS,
	DIV_CLKCMU_G3D_SWITCH,
	MUX_CLKCMU_G3D_SWITCH,
	CLKCMU_G3D_BUS,
	MUX_CLKCMU_G3D_BUS,
	PLL_SHARED_MIF,
	MUX_CLKCMU_HSI1_PCIE,
	MUX_CLKCMU_ALIVE_BUS,
	CLKCMU_HSI0_BUS,
	CLKCMU_CSIS_OIS_MCU,
	MUX_CLKCMU_CSIS_OIS_MCU,
	PLL_G3D,
	MUX_CLKCMU_HSI1_MMC_CARD,
	PLL_MMC,
	CLKCMU_BUS0_BUS,
	MUX_CLKCMU_BUS0_BUS,
	CLKCMU_BUS2_BUS,
	MUX_CLKCMU_BUS2_BUS,
	CLKCMU_BUS1_BUS,
	MUX_CLKCMU_BUS1_BUS,
	CLKCMU_M2M_BUS,
	MUX_CLKCMU_M2M_BUS,
	CLKCMU_CPUCL0_DBG_BUS,
	MUX_CLKCMU_CPUCL0_DBG_BUS,
	DIV_CLKCMU_DPUB_ALT,
	MUX_CLKCMU_DPUB_ALT,
	CLKCMU_DPUF0_BUS,
	DIV_CLKCMU_DPUF0_ALT,
	MUX_CLKCMU_DPUF0_ALT,
	DIV_CLKCMU_DPUB,
	MUX_CLKCMU_DPUB,
	DIV_CLKCMU_DPUF0,
	MUX_CLKCMU_DPUF0,
	DIV_CLKCMU_DPUF1,
	MUX_CLKCMU_DPUF1,
	DIV_CLKCMU_DPUF1_ALT,
	MUX_CLKCMU_DPUF1_ALT,
	CLKCMU_BUS1_SBIC,
	MUX_CLKCMU_BUS1_SBIC,
	CLKCMU_MFC0_WFD,
	MUX_CLKCMU_MFC0_WFD,
	CLKCMU_HSI0_USB31DRD,
	MUX_CLKCMU_HSI0_USB31DRD,
	CLKCMU_HSI0_DPGTC,
	MUX_CLKCMU_HSI0_DPGTC,
	CLKCMU_HSI1_BUS,
	MUX_CLKCMU_HSI1_BUS,
	CLKCMU_MIF_BUSP,
	MUX_CLKCMU_MIF_BUSP,
	CLKCMU_PERIC0_IP0,
	MUX_CLKCMU_PERIC0_IP0,
	CLKCMU_PERIC0_IP1,
	MUX_CLKCMU_PERIC0_IP1,
	CLKCMU_PERIC1_IP1,
	MUX_CLKCMU_PERIC1_IP1,
	CLKCMU_PERIC1_IP0,
	MUX_CLKCMU_PERIC1_IP0,
	CLKCMU_SSP_BUS,
	MUX_CLKCMU_SSP_BUS,
	CLKCMU_SSP_SSPCORE,
	MUX_CLKCMU_SSP_SSPCORE,
	MUX_CMU_CMUREF,
	CLKCMU_CMU_BOOST,
	CLKCMU_CMU_BOOST_MIF,
	CLKCMU_CMU_BOOST_CPU,
	CLKCMU_PERIC2_IP0,
	MUX_CLKCMU_PERIC2_IP0,
	CLKCMU_PERIC2_IP1,
	MUX_CLKCMU_PERIC2_IP1,
	CLKCMU_CPUCL0_BUSP,
	MUX_CLKCMU_CPUCL0_BUSP,
	CLKCMU_HSI1_UFS_EMBD,
	MUX_CLKCMU_HSI1_UFS_EMBD,
	PLL_SHARED4,
	CLKCMU_HPM,
	CLKCMU_PERIC0_BUS,
	MUX_CLKCMU_PERIC0_BUS,
	CLKCMU_PERIC1_BUS,
	MUX_CLKCMU_PERIC1_BUS,
	CLKCMU_PERIC2_BUS,
	MUX_CLKCMU_PERIC2_BUS,
};
/* SPECIAL VCLK -> Clock Node List */
enum clk_id cmucal_vclk_mux_clk_alive_i3c_pmic[] = {
	MUX_CLK_ALIVE_I3C_PMIC,
	DIV_CLK_ALIVE_I3C_PMIC,
};
enum clk_id cmucal_vclk_clkcmu_alive_bus[] = {
	CLKCMU_ALIVE_BUS,
};
enum clk_id cmucal_vclk_mux_clk_aud_dsif[] = {
	MUX_CLK_AUD_DSIF,
	DIV_CLK_AUD_DSIF,
	MUX_CLK_AUD_PCMC,
	DIV_CLK_AUD_PCMC,
	DIV_CLK_AUD_UAIF0,
	MUX_CLK_AUD_UAIF0,
	DIV_CLK_AUD_UAIF1,
	MUX_CLK_AUD_UAIF1,
	DIV_CLK_AUD_UAIF2,
	MUX_CLK_AUD_UAIF2,
	DIV_CLK_AUD_UAIF3,
	MUX_CLK_AUD_UAIF3,
	DIV_CLK_AUD_CPU_ACLK,
	DIV_CLK_AUD_CNT,
	MUX_CLK_AUD_CNT,
	DIV_CLK_AUD_UAIF4,
	MUX_CLK_AUD_UAIF4,
	DIV_CLK_AUD_UAIF5,
	MUX_CLK_AUD_UAIF5,
	DIV_CLK_AUD_SCLK,
	DIV_CLK_AUD_UAIF6,
	MUX_CLK_AUD_UAIF6,
};
enum clk_id cmucal_vclk_mux_bus0_cmuref[] = {
	MUX_BUS0_CMUREF,
};
enum clk_id cmucal_vclk_mux_clkcmu_cmu_boost[] = {
	MUX_CLKCMU_CMU_BOOST,
};
enum clk_id cmucal_vclk_mux_bus1_cmuref[] = {
	MUX_BUS1_CMUREF,
};
enum clk_id cmucal_vclk_mux_bus2_cmuref[] = {
	MUX_BUS2_CMUREF,
};
enum clk_id cmucal_vclk_clk_cmgp_adc[] = {
	CLK_CMGP_ADC,
};
enum clk_id cmucal_vclk_clkcmu_cmgp_adc[] = {
	CLKCMU_CMGP_ADC,
	MUX_CLKCMU_CMGP_ADC,
};
enum clk_id cmucal_vclk_mux_core_cmuref[] = {
	MUX_CORE_CMUREF,
};
enum clk_id cmucal_vclk_mux_cpucl0_cmuref[] = {
	MUX_CPUCL0_CMUREF,
};
enum clk_id cmucal_vclk_mux_clkcmu_cmu_boost_cpu[] = {
	MUX_CLKCMU_CMU_BOOST_CPU,
};
enum clk_id cmucal_vclk_mux_cpucl1_cmuref[] = {
	MUX_CPUCL1_CMUREF,
};
enum clk_id cmucal_vclk_mux_cpucl2_cmuref[] = {
	MUX_CPUCL2_CMUREF,
};
enum clk_id cmucal_vclk_mux_dsu_cmuref[] = {
	MUX_DSU_CMUREF,
};
enum clk_id cmucal_vclk_mux_clk_hsi0_usb31drd[] = {
	MUX_CLK_HSI0_USB31DRD,
};
enum clk_id cmucal_vclk_mux_mif_cmuref[] = {
	MUX_MIF_CMUREF,
};
enum clk_id cmucal_vclk_mux_clkcmu_cmu_boost_mif[] = {
	MUX_CLKCMU_CMU_BOOST_MIF,
};
enum clk_id cmucal_vclk_clkcmu_g3d_shader[] = {
	CLKCMU_G3D_SHADER,
};
enum clk_id cmucal_vclk_div_clk_alive_dbgcore_uart[] = {
	DIV_CLK_ALIVE_DBGCORE_UART,
};
enum clk_id cmucal_vclk_div_clk_cmgp_i2c0[] = {
	DIV_CLK_CMGP_I2C0,
	MUX_CLK_CMGP_I2C0,
};
enum clk_id cmucal_vclk_clkcmu_cmgp_peri[] = {
	CLKCMU_CMGP_PERI,
	MUX_CLKCMU_CMGP_PERI,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi1[] = {
	DIV_CLK_CMGP_USI1,
	MUX_CLK_CMGP_USI1,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi0[] = {
	DIV_CLK_CMGP_USI0,
	MUX_CLK_CMGP_USI0,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi2[] = {
	DIV_CLK_CMGP_USI2,
	MUX_CLK_CMGP_USI2,
};
enum clk_id cmucal_vclk_div_clk_cmgp_usi3[] = {
	DIV_CLK_CMGP_USI3,
	MUX_CLK_CMGP_USI3,
};
enum clk_id cmucal_vclk_div_clk_cmgp_i2c1[] = {
	DIV_CLK_CMGP_I2C1,
	MUX_CLK_CMGP_I2C1,
};
enum clk_id cmucal_vclk_div_clk_cmgp_i2c2[] = {
	DIV_CLK_CMGP_I2C2,
	MUX_CLK_CMGP_I2C2,
};
enum clk_id cmucal_vclk_div_clk_cmgp_i2c3[] = {
	DIV_CLK_CMGP_I2C3,
	MUX_CLK_CMGP_I2C3,
};
enum clk_id cmucal_vclk_div_clk_cmgp_i3c[] = {
	DIV_CLK_CMGP_I3C,
	MUX_CLK_CMGP_I3C,
};
enum clk_id cmucal_vclk_mux_clkcmu_hpm[] = {
	MUX_CLKCMU_HPM,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk0[] = {
	CLKCMU_CIS_CLK0,
	MUX_CLKCMU_CIS_CLK0,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk1[] = {
	CLKCMU_CIS_CLK1,
	MUX_CLKCMU_CIS_CLK1,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk2[] = {
	CLKCMU_CIS_CLK2,
	MUX_CLKCMU_CIS_CLK2,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk3[] = {
	CLKCMU_CIS_CLK3,
	MUX_CLKCMU_CIS_CLK3,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk4[] = {
	CLKCMU_CIS_CLK4,
	MUX_CLKCMU_CIS_CLK4,
};
enum clk_id cmucal_vclk_clkcmu_cis_clk5[] = {
	CLKCMU_CIS_CLK5,
	MUX_CLKCMU_CIS_CLK5,
};
enum clk_id cmucal_vclk_div_clk_cpucl0_shortstop_core[] = {
	DIV_CLK_CPUCL0_SHORTSTOP_CORE,
};
enum clk_id cmucal_vclk_div_clk_cpucl1_shortstop_core[] = {
	DIV_CLK_CPUCL1_SHORTSTOP_CORE,
};
enum clk_id cmucal_vclk_div_clk_cpucl1_htu[] = {
	DIV_CLK_CPUCL1_HTU,
};
enum clk_id cmucal_vclk_div_clk_cpucl2_shortstop_core[] = {
	DIV_CLK_CPUCL2_SHORTSTOP_CORE,
};
enum clk_id cmucal_vclk_div_clk_cpucl2_htu[] = {
	DIV_CLK_CPUCL2_HTU,
};
enum clk_id cmucal_vclk_div_clk_dsu_shortstop_cluster[] = {
	DIV_CLK_DSU_SHORTSTOP_CLUSTER,
};
enum clk_id cmucal_vclk_div_clk_peric0_usi00_usi[] = {
	DIV_CLK_PERIC0_USI00_USI,
	MUX_CLK_PERIC0_USI00_USI,
};
enum clk_id cmucal_vclk_div_clk_peric0_usi01_usi[] = {
	DIV_CLK_PERIC0_USI01_USI,
	MUX_CLK_PERIC0_USI01_USI,
};
enum clk_id cmucal_vclk_div_clk_peric0_usi02_usi[] = {
	DIV_CLK_PERIC0_USI02_USI,
	MUX_CLK_PERIC0_USI02_USI,
};
enum clk_id cmucal_vclk_div_clk_peric0_usi03_usi[] = {
	DIV_CLK_PERIC0_USI03_USI,
	MUX_CLK_PERIC0_USI03_USI,
};
enum clk_id cmucal_vclk_div_clk_peric0_usi04_usi[] = {
	DIV_CLK_PERIC0_USI04_USI,
	MUX_CLK_PERIC0_USI04_USI,
};
enum clk_id cmucal_vclk_div_clk_peric0_usi05_usi[] = {
	DIV_CLK_PERIC0_USI05_USI,
	MUX_CLK_PERIC0_USI05_USI,
};
enum clk_id cmucal_vclk_div_clk_peric0_uart_dbg[] = {
	DIV_CLK_PERIC0_UART_DBG,
	MUX_CLK_PERIC0_UART_DBG,
};
enum clk_id cmucal_vclk_div_clk_peric0_usi13_usi[] = {
	DIV_CLK_PERIC0_USI13_USI,
	MUX_CLK_PERIC0_USI13_USI,
};
enum clk_id cmucal_vclk_div_clk_peric0_usi14_usi[] = {
	DIV_CLK_PERIC0_USI14_USI,
	MUX_CLK_PERIC0_USI14_USI,
};
enum clk_id cmucal_vclk_div_clk_peric0_usi15_usi[] = {
	DIV_CLK_PERIC0_USI15_USI,
	MUX_CLK_PERIC0_USI15_USI,
};
enum clk_id cmucal_vclk_div_clk_peric1_uart_bt[] = {
	DIV_CLK_PERIC1_UART_BT,
	MUX_CLK_PERIC1_UART_BT,
};
enum clk_id cmucal_vclk_div_clk_peric1_usi11_usi[] = {
	DIV_CLK_PERIC1_USI11_USI,
	MUX_CLK_PERIC1_USI11_USI,
};
enum clk_id cmucal_vclk_div_clk_peric1_usi12_usi[] = {
	DIV_CLK_PERIC1_USI12_USI,
	MUX_CLK_PERIC1_USI12_USI,
};
enum clk_id cmucal_vclk_div_clk_peric1_usi16_usi[] = {
	DIV_CLK_PERIC1_USI16_USI,
	MUX_CLK_PERIC1_USI16_USI,
};
enum clk_id cmucal_vclk_div_clk_peric1_usi17_usi[] = {
	DIV_CLK_PERIC1_USI17_USI,
	MUX_CLK_PERIC1_USI17_USI,
};
enum clk_id cmucal_vclk_div_clk_peric1_usi18_usi[] = {
	DIV_CLK_PERIC1_USI18_USI,
	MUX_CLK_PERIC1_USI18_USI,
};
enum clk_id cmucal_vclk_div_clk_peric2_usi06_usi[] = {
	DIV_CLK_PERIC2_USI06_USI,
	MUX_CLK_PERIC2_USI06_USI,
};
enum clk_id cmucal_vclk_div_clk_peric2_usi07_usi[] = {
	DIV_CLK_PERIC2_USI07_USI,
	MUX_CLK_PERIC2_USI07_USI,
};
enum clk_id cmucal_vclk_div_clk_peric2_usi08_usi[] = {
	DIV_CLK_PERIC2_USI08_USI,
	MUX_CLK_PERIC2_USI08_USI,
};
enum clk_id cmucal_vclk_div_clk_peric2_usi09_usi[] = {
	DIV_CLK_PERIC2_USI09_USI,
	MUX_CLK_PERIC2_USI09_USI,
};
enum clk_id cmucal_vclk_div_clk_peric2_usi10_usi[] = {
	DIV_CLK_PERIC2_USI10_USI,
	MUX_CLK_PERIC2_USI10_USI,
};
enum clk_id cmucal_vclk_div_clk_top_hsi0_bus[] = {
	CLKCMU_HSI0_BUS,
	MUX_CLKCMU_HSI0_BUS,
};

/* COMMON VCLK -> Clock Node List */
enum clk_id cmucal_vclk_blk_aud[] = {
	DIV_CLK_AUD_DMIC1,
	PLL_AUD1,
	DIV_CLK_AUD_BUSP,
	CLKAUD_VTS_DMIC0,
};
enum clk_id cmucal_vclk_blk_cmu[] = {
	MUX_CLKCMU_MIF_SWITCH,
	MUX_CP_UCPU_CLK,
	MUX_CP_LCPU_CLK,
	CLKCMU_DPUB_BUS,
	MUX_CLKCMU_HSI0_BUS,
	CLKCMU_DPUF1_BUS,
	CLKCMU_HSI1_MMC_CARD,
	PLL_SHARED0,
	PLL_SHARED1,
	CP_SHARED0_CLK,
	CP_HISPEEDY_CLK,
	MUX_CP_HISPEEDY_CLK,
	PLL_SHARED2,
	CP_SHARED1_CLK,
	PLL_SHARED3,
	CP_SHARED2_CLK,
};
enum clk_id cmucal_vclk_blk_s2d[] = {
	MUX_CLK_S2D_CORE,
	PLL_MIF_S2D,
};
enum clk_id cmucal_vclk_blk_alive[] = {
	CLKCMU_CMGP_BUS,
	MUX_CLKCMU_CMGP_BUS,
	DIV_CLK_ALIVE_BUS,
	MUX_CLK_ALIVE_BUS,
	CLKCMU_VTS_BUS,
	MUX_CLKCMU_VTS_BUS,
};
enum clk_id cmucal_vclk_blk_cpucl0[] = {
	MUX_CLK_CPUCL0_CORE,
	MUX_CLK_CPUCL0_CORE_DELAY,
	MUX_PLL_CPUCL0_DELAY,
};
enum clk_id cmucal_vclk_blk_cpucl1[] = {
	MUX_CLK_CPUCL1_CORE,
};
enum clk_id cmucal_vclk_blk_cpucl2[] = {
	MUX_CLK_CPUCL2_CORE,
};
enum clk_id cmucal_vclk_blk_dsu[] = {
	DIV_CLK_CLUSTER_ACLK,
	DIV_CLK_CLUSTER_PCLK,
	DIV_CLK_CLUSTER_PERIPHCLK,
	DIV_CLK_CLUSTER_ATCLK,
	MUX_CLK_DSU_CLUSTER,
	MUX_CLK_DSU_CLUSTER_DELAY,
	MUX_PLL_DSU_DELAY,
};
enum clk_id cmucal_vclk_blk_g3d[] = {
	DIV_CLK_G3D_BUSP,
	MUX_CLK_G3D_BUS,
};
enum clk_id cmucal_vclk_blk_hsi0[] = {
	MUX_CLK_HSI0_BUS,
};
enum clk_id cmucal_vclk_blk_peric0[] = {
	DIV_CLK_PERIC0_USI_I2C,
	MUX_CLK_PERIC0_USI_I2C,
};
enum clk_id cmucal_vclk_blk_peric1[] = {
	DIV_CLK_PERIC1_USI_I2C,
	MUX_CLK_PERIC1_USI_I2C,
};
enum clk_id cmucal_vclk_blk_peric2[] = {
	DIV_CLK_PERIC2_USI_I2C,
	MUX_CLK_PERIC2_USI_I2C,
};
enum clk_id cmucal_vclk_blk_vts[] = {
	DIV_CLK_VTS_DMIC_IF_DIV2,
	DIV_CLK_VTS_DMIC_IF,
	MUX_CLK_VTS_DMIC_IF,
	DIV_CLK_VTS_DMIC_AUD_DIV2,
	DIV_CLK_VTS_DMIC_AUD,
	MUX_CLK_VTS_DMIC_AUD,
	DIV_CLK_VTS_SERIAL_LIF,
	MUX_CLK_VTS_SERIAL_LIF,
	DIV_CLK_VTS_DMIC_AHB,
	MUX_CLK_VTS_DMIC_AHB,
	DIV_CLK_VTS_SERIAL_LIF_CORE,
	MUX_CLK_VTS_SERIAL_LIF_CORE,
	DIV_CLK_VTS_BUS,
};
enum clk_id cmucal_vclk_blk_bus0[] = {
	DIV_CLK_BUS0_BUSP,
};
enum clk_id cmucal_vclk_blk_bus1[] = {
	DIV_CLK_BUS1_BUSP,
};
enum clk_id cmucal_vclk_blk_bus2[] = {
	DIV_CLK_BUS2_BUSP,
};
enum clk_id cmucal_vclk_blk_core[] = {
	DIV_CLK_CORE_BUSP,
};
enum clk_id cmucal_vclk_blk_cpucl0_glb[] = {
	DIV_CLK_CPUCL0_DBG_BUS,
	DIV_CLK_CPUCL0_DBG_PCLKDBG,
};
enum clk_id cmucal_vclk_blk_csis[] = {
	DIV_CLK_CSIS_BUSP,
};
enum clk_id cmucal_vclk_blk_dns[] = {
	DIV_CLK_DNS_BUSP,
};
enum clk_id cmucal_vclk_blk_dpub[] = {
	DIV_CLK_DPUB_BUSP,
};
enum clk_id cmucal_vclk_blk_dpuf0[] = {
	DIV_CLK_DPUF0_BUSP,
};
enum clk_id cmucal_vclk_blk_dpuf1[] = {
	DIV_CLK_DPUF1_BUSP,
};
enum clk_id cmucal_vclk_blk_itp[] = {
	DIV_CLK_ITP_BUSP,
};
enum clk_id cmucal_vclk_blk_lme[] = {
	DIV_CLK_LME_BUSP,
};
enum clk_id cmucal_vclk_blk_m2m[] = {
	DIV_CLK_M2M_BUSP,
};
enum clk_id cmucal_vclk_blk_mcfp0[] = {
	DIV_CLK_MCFP0_BUSP,
};
enum clk_id cmucal_vclk_blk_mcfp1[] = {
	DIV_CLK_MCFP1_BUSP,
};
enum clk_id cmucal_vclk_blk_mcsc[] = {
	DIV_CLK_MCSC_BUSP,
};
enum clk_id cmucal_vclk_blk_mfc0[] = {
	DIV_CLK_MFC0_BUSP,
};
enum clk_id cmucal_vclk_blk_mfc1[] = {
	DIV_CLK_MFC1_BUSP,
};
enum clk_id cmucal_vclk_blk_npu[] = {
	DIV_CLK_NPU_BUSP,
};
enum clk_id cmucal_vclk_blk_npu01[] = {
	DIV_CLK_NPU01_BUSP,
};
enum clk_id cmucal_vclk_blk_npu10[] = {
	DIV_CLK_NPU10_BUSP,
};
enum clk_id cmucal_vclk_blk_npus[] = {
	DIV_CLK_NPUS_BUSP,
};
enum clk_id cmucal_vclk_blk_peris[] = {
	DIV_CLK_PERIS_BUSP,
};
enum clk_id cmucal_vclk_blk_ssp[] = {
	DIV_CLK_SSP_BUSP,
};
enum clk_id cmucal_vclk_blk_taa[] = {
	DIV_CLK_TAA_BUSP,
};
enum clk_id cmucal_vclk_blk_vpc[] = {
	DIV_CLK_VPC_BUSP,
};
enum clk_id cmucal_vclk_blk_vpd[] = {
	DIV_CLK_VPD_BUSP,
};
enum clk_id cmucal_vclk_blk_yuvpp[] = {
	DIV_CLK_YUVPP_BUSP,
};
/* GATE VCLK -> Clock Node List */
enum clk_id cmucal_vclk_ip_lhs_axi_d_apm[] = {
	GOUT_BLK_ALIVE_UID_LHS_AXI_D_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_apm[] = {
	GOUT_BLK_ALIVE_UID_LHM_AXI_P_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_wdt_alive[] = {
	GOUT_BLK_ALIVE_UID_WDT_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_alive[] = {
	GOUT_BLK_ALIVE_UID_SYSREG_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_ap[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_APM_AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_pmu_alive[] = {
	GOUT_BLK_ALIVE_UID_APBIF_PMU_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_intmem[] = {
	GOUT_BLK_ALIVE_UID_INTMEM_IPCLKPORT_ACLK,
	GOUT_BLK_ALIVE_UID_INTMEM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_g_scan2dram[] = {
	GOUT_BLK_ALIVE_UID_LHS_AXI_G_SCAN2DRAM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_pmu_intr_gen[] = {
	GOUT_BLK_ALIVE_UID_PMU_INTR_GEN_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pem[] = {
	GOUT_BLK_ALIVE_UID_PEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_dp_alive[] = {
	GOUT_BLK_ALIVE_UID_XIU_DP_ALIVE_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_alive_cmu_alive[] = {
	CLK_BLK_ALIVE_UID_ALIVE_CMU_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_grebeintegration[] = {
	GOUT_BLK_ALIVE_UID_GREBEINTEGRATION_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_gpio_alive[] = {
	GOUT_BLK_ALIVE_UID_GPIO_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_top_rtc[] = {
	GOUT_BLK_ALIVE_UID_APBIF_TOP_RTC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ss_dbgcore[] = {
	GOUT_BLK_ALIVE_UID_SS_DBGCORE_IPCLKPORT_SS_DBGCORE_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_dtzpc_alive[] = {
	GOUT_BLK_ALIVE_UID_DTZPC_ALIVE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_dbgcore[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_AP_DBGCORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_lp_vts[] = {
	GOUT_BLK_ALIVE_UID_LHS_AXI_LP_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_g_dbgcore[] = {
	GOUT_BLK_ALIVE_UID_LHS_AXI_G_DBGCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_apbif_rtc[] = {
	GOUT_BLK_ALIVE_UID_APBIF_RTC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_c_cmgp[] = {
	GOUT_BLK_ALIVE_UID_LHS_AXI_C_CMGP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_alive[] = {
	GOUT_BLK_ALIVE_UID_VGEN_LITE_ALIVE_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_rom_crc32_host[] = {
	GOUT_BLK_ALIVE_UID_ROM_CRC32_HOST_IPCLKPORT_PCLK,
	GOUT_BLK_ALIVE_UID_ROM_CRC32_HOST_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_i3c_pmic[] = {
	GOUT_BLK_ALIVE_UID_I3C_PMIC_IPCLKPORT_I_PCLK,
	GOUT_BLK_ALIVE_UID_I3C_PMIC_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_c_modem[] = {
	GOUT_BLK_ALIVE_UID_LHM_AXI_C_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_cp[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_APM_CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_cp[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_AP_CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_cp_s[] = {
	GOUT_BLK_ALIVE_UID_MAILBOX_AP_CP_S_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_c_vts[] = {
	GOUT_BLK_ALIVE_UID_LHM_AXI_C_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_apbif_sysreg_vgpio2ap[] = {
	GOUT_BLK_ALIVE_UID_APBIF_SYSREG_VGPIO2AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_sysreg_vgpio2apm[] = {
	GOUT_BLK_ALIVE_UID_APBIF_SYSREG_VGPIO2APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_sysreg_vgpio2pmu[] = {
	GOUT_BLK_ALIVE_UID_APBIF_SYSREG_VGPIO2PMU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sweeper_p_alive[] = {
	GOUT_BLK_ALIVE_UID_SWEEPER_P_ALIVE_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_clkmon[] = {
	GOUT_BLK_ALIVE_UID_CLKMON_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dbgcore_uart[] = {
	GOUT_BLK_ALIVE_UID_DBGCORE_UART_IPCLKPORT_PCLK,
	GOUT_BLK_ALIVE_UID_DBGCORE_UART_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_double_ip_batcher[] = {
	GOUT_BLK_ALIVE_UID_DOUBLE_IP_BATCHER_IPCLKPORT_I_PCLK_APM,
	GOUT_BLK_ALIVE_UID_DOUBLE_IP_BATCHER_IPCLKPORT_I_PCLK_CPU,
	GOUT_BLK_ALIVE_UID_DOUBLE_IP_BATCHER_IPCLKPORT_I_PCLK_SEMA,
};
enum clk_id cmucal_vclk_ip_hw_scandump_clkstop_ctrl[] = {
	GOUT_BLK_ALIVE_UID_HW_SCANDUMP_CLKSTOP_CTRL_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_aud_cmu_aud[] = {
	CLK_BLK_AUD_UID_AUD_CMU_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_aud[] = {
	GOUT_BLK_AUD_UID_LHS_AXI_D_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_aud[] = {
	GOUT_BLK_AUD_UID_PPMU_AUD_IPCLKPORT_ACLK,
	GOUT_BLK_AUD_UID_PPMU_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_aud[] = {
	GOUT_BLK_AUD_UID_SYSREG_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_abox[] = {
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF0,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF1,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF3,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_DSIF,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF2,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_ACLK,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_CCLK_DAP,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_ACLK_IRQ,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_ACLK_IRQ,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_CNT,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF4,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF5,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_CCLK_ASB,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_CCLK_CA32,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_SCLK,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_BCLK_UAIF6,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_XCLK,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_PCMC_CLK,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_C2A0_CLK,
	GOUT_BLK_AUD_UID_ABOX_IPCLKPORT_C2A1_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_aud[] = {
	GOUT_BLK_AUD_UID_LHM_AXI_P_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_peri_axi_asb[] = {
	GOUT_BLK_AUD_UID_PERI_AXI_ASB_IPCLKPORT_PCLK,
	GOUT_BLK_AUD_UID_PERI_AXI_ASB_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_wdt_aud[] = {
	GOUT_BLK_AUD_UID_WDT_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_smmu_aud[] = {
	GOUT_BLK_AUD_UID_SMMU_AUD_IPCLKPORT_CLK_S1,
	GOUT_BLK_AUD_UID_SMMU_AUD_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_ad_apb_smmu_aud[] = {
	GOUT_BLK_AUD_UID_AD_APB_SMMU_AUD_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_smmu_aud_s[] = {
	GOUT_BLK_AUD_UID_AD_APB_SMMU_AUD_S_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_vgen_lite_aud[] = {
	GOUT_BLK_AUD_UID_VGEN_LITE_AUD_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_smmu_aud_ns1[] = {
	GOUT_BLK_AUD_UID_AD_APB_SMMU_AUD_NS1_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_mailbox_aud0[] = {
	GOUT_BLK_AUD_UID_MAILBOX_AUD0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_aud1[] = {
	GOUT_BLK_AUD_UID_MAILBOX_AUD1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_aud[] = {
	GOUT_BLK_AUD_UID_D_TZPC_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_hsi0aud[] = {
	GOUT_BLK_AUD_UID_LHM_AXI_D_HSI0AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_audhsi0[] = {
	GOUT_BLK_AUD_UID_LHS_AXI_D_AUDHSI0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_audvts[] = {
	GOUT_BLK_AUD_UID_LHS_AXI_D_AUDVTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_axi_us_32to128[] = {
	GOUT_BLK_AUD_UID_AXI_US_32TO128_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_trex_aud[] = {
	GOUT_BLK_AUD_UID_TREX_AUD_IPCLKPORT_CLK,
	GOUT_BLK_AUD_UID_TREX_AUD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_aud2[] = {
	GOUT_BLK_AUD_UID_MAILBOX_AUD2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_aud3[] = {
	GOUT_BLK_AUD_UID_MAILBOX_AUD3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_d_audvts[] = {
	GOUT_BLK_AUD_UID_BAAW_D_AUDVTS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_bus0_cmu_bus0[] = {
	CLK_BLK_BUS0_UID_BUS0_CMU_BUS0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_bus0[] = {
	GOUT_BLK_BUS0_UID_SYSREG_BUS0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_d0_bus0[] = {
	GOUT_BLK_BUS0_UID_TREX_D0_BUS0_IPCLKPORT_PCLK,
	GOUT_BLK_BUS0_UID_TREX_D0_BUS0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mif0[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_MIF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mif1[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_MIF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mif2[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_MIF2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mif3[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_MIF3_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_npu10[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_NPU10_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_vpc[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_VPC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_npus[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_NPUS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_npu01[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_NPU01_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_perisgic[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_PERISGIC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_npu00[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_NPU00_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_trex_p_bus0[] = {
	GOUT_BLK_BUS0_UID_TREX_P_BUS0_IPCLKPORT_ACLK_BUS0,
	GOUT_BLK_BUS0_UID_TREX_P_BUS0_IPCLKPORT_PCLK_BUS0,
	GOUT_BLK_BUS0_UID_TREX_P_BUS0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_d1_bus0[] = {
	GOUT_BLK_BUS0_UID_TREX_D1_BUS0_IPCLKPORT_ACLK,
	GOUT_BLK_BUS0_UID_TREX_D1_BUS0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d1_vpc[] = {
	GOUT_BLK_BUS0_UID_LHM_ACEL_D1_VPC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d2_vpc[] = {
	GOUT_BLK_BUS0_UID_LHM_ACEL_D2_VPC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_npus[] = {
	GOUT_BLK_BUS0_UID_LHM_AXI_D0_NPUS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_bus0[] = {
	GOUT_BLK_BUS0_UID_D_TZPC_BUS0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_dbg_g_bus0[] = {
	GOUT_BLK_BUS0_UID_LHS_DBG_G_BUS0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d2_npus[] = {
	GOUT_BLK_BUS0_UID_LHM_AXI_D2_NPUS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d0_vpc[] = {
	GOUT_BLK_BUS0_UID_LHM_ACEL_D0_VPC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_npus[] = {
	GOUT_BLK_BUS0_UID_LHM_AXI_D1_NPUS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_busif_cmutopc[] = {
	GOUT_BLK_BUS0_UID_BUSIF_CMUTOPC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cacheaid_bus0[] = {
	GOUT_BLK_BUS0_UID_CACHEAID_BUS0_IPCLKPORT_ACLK,
	GOUT_BLK_BUS0_UID_CACHEAID_BUS0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_vpc[] = {
	GOUT_BLK_BUS0_UID_BAAW_P_VPC_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_peric0[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_PERIC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_peric2[] = {
	GOUT_BLK_BUS0_UID_LHS_AXI_P_PERIC2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_asyncsfr_wr_smc[] = {
	GOUT_BLK_BUS0_UID_ASYNCSFR_WR_SMC_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_bus1_cmu_bus1[] = {
	CLK_BLK_BUS1_UID_BUS1_CMU_BUS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_dit[] = {
	GOUT_BLK_BUS1_UID_AD_APB_DIT_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_d_tzpc_bus1[] = {
	GOUT_BLK_BUS1_UID_D_TZPC_BUS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dit[] = {
	GOUT_BLK_BUS1_UID_DIT_IPCLKPORT_ICLKL2A,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_dpub[] = {
	GOUT_BLK_BUS1_UID_LHS_AXI_P_DPUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_hsi0[] = {
	GOUT_BLK_BUS1_UID_LHS_AXI_P_HSI0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_vts[] = {
	GOUT_BLK_BUS1_UID_LHS_AXI_P_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_dbg_g_bus1[] = {
	GOUT_BLK_BUS1_UID_LHS_DBG_G_BUS1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_qe_pdma[] = {
	GOUT_BLK_BUS1_UID_QE_PDMA_IPCLKPORT_ACLK,
	GOUT_BLK_BUS1_UID_QE_PDMA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pdma[] = {
	GOUT_BLK_BUS1_UID_PDMA_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_spdma[] = {
	GOUT_BLK_BUS1_UID_QE_SPDMA_IPCLKPORT_ACLK,
	GOUT_BLK_BUS1_UID_QE_SPDMA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_spdma[] = {
	GOUT_BLK_BUS1_UID_SPDMA_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_bus1[] = {
	GOUT_BLK_BUS1_UID_SYSREG_BUS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_d_bus1[] = {
	GOUT_BLK_BUS1_UID_TREX_D_BUS1_IPCLKPORT_ACLK,
	GOUT_BLK_BUS1_UID_TREX_D_BUS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_p_bus1[] = {
	GOUT_BLK_BUS1_UID_TREX_P_BUS1_IPCLKPORT_PCLK_BUS1,
	GOUT_BLK_BUS1_UID_TREX_P_BUS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_rb_bus1[] = {
	GOUT_BLK_BUS1_UID_TREX_RB_BUS1_IPCLKPORT_CLK,
	GOUT_BLK_BUS1_UID_TREX_RB_BUS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d0_bus1[] = {
	GOUT_BLK_BUS1_UID_XIU_D0_BUS1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_dpuf0[] = {
	GOUT_BLK_BUS1_UID_LHM_AXI_D0_DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d_hsi0[] = {
	GOUT_BLK_BUS1_UID_LHM_ACEL_D_HSI0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_dpuf0[] = {
	GOUT_BLK_BUS1_UID_LHM_AXI_D1_DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_apm[] = {
	GOUT_BLK_BUS1_UID_LHM_AXI_D_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_vts[] = {
	GOUT_BLK_BUS1_UID_LHM_AXI_D_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_vgen_pdma[] = {
	GOUT_BLK_BUS1_UID_AD_APB_VGEN_PDMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_pdma[] = {
	GOUT_BLK_BUS1_UID_AD_APB_PDMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_spdma[] = {
	GOUT_BLK_BUS1_UID_AD_APB_SPDMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_acvps[] = {
	GOUT_BLK_BUS1_UID_AD_APB_SYSMMU_ACVPS_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_dit[] = {
	GOUT_BLK_BUS1_UID_AD_APB_SYSMMU_DIT_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_sbic[] = {
	GOUT_BLK_BUS1_UID_AD_APB_SYSMMU_SBIC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_vgen_lite_bus1[] = {
	GOUT_BLK_BUS1_UID_VGEN_LITE_BUS1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_baaw_p_vts[] = {
	GOUT_BLK_BUS1_UID_BAAW_P_VTS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_s2_acvps[] = {
	GOUT_BLK_BUS1_UID_SYSMMU_S2_ACVPS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_s2_dit[] = {
	GOUT_BLK_BUS1_UID_SYSMMU_S2_DIT_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_s2_sbic[] = {
	GOUT_BLK_BUS1_UID_SYSMMU_S2_SBIC_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_dpuf0[] = {
	GOUT_BLK_BUS1_UID_LHS_AXI_P_DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_pdma[] = {
	GOUT_BLK_BUS1_UID_VGEN_PDMA_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_sbic[] = {
	GOUT_BLK_BUS1_UID_LHM_AXI_D_SBIC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_sbic[] = {
	GOUT_BLK_BUS1_UID_LHS_AXI_D_SBIC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_sbic[] = {
	GOUT_BLK_BUS1_UID_AD_APB_SBIC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_sbic[] = {
	GOUT_BLK_BUS1_UID_SBIC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_dpuf1[] = {
	GOUT_BLK_BUS1_UID_LHM_AXI_D0_DPUF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_dpuf1[] = {
	GOUT_BLK_BUS1_UID_LHM_AXI_D1_DPUF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_dpuf1[] = {
	GOUT_BLK_BUS1_UID_LHS_AXI_P_DPUF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_bus2_cmu_bus2[] = {
	CLK_BLK_BUS2_UID_BUS2_CMU_BUS2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_itp[] = {
	GOUT_BLK_BUS2_UID_LHS_AXI_P_ITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_lme[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D_LME_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_trex_d_bus2[] = {
	GOUT_BLK_BUS2_UID_TREX_D_BUS2_IPCLKPORT_PCLK,
	GOUT_BLK_BUS2_UID_TREX_D_BUS2_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_mcfp0[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D0_MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_yuvpp[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D_YUVPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_dns[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D0_DNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mcfp0[] = {
	GOUT_BLK_BUS2_UID_LHS_AXI_P_MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mcsc[] = {
	GOUT_BLK_BUS2_UID_LHS_AXI_P_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_trex_p_bus2[] = {
	GOUT_BLK_BUS2_UID_TREX_P_BUS2_IPCLKPORT_PCLK,
	GOUT_BLK_BUS2_UID_TREX_P_BUS2_IPCLKPORT_PCLK_BUS2,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_mcfp0[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D1_MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_lme[] = {
	GOUT_BLK_BUS2_UID_LHS_AXI_P_LME_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_mcsc[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D1_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_dbg_g_bus2[] = {
	GOUT_BLK_BUS2_UID_LHS_DBG_G_BUS2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_taa[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D_TAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_bus2[] = {
	GOUT_BLK_BUS2_UID_D_TZPC_BUS2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_bus2[] = {
	GOUT_BLK_BUS2_UID_SYSREG_BUS2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_csis[] = {
	GOUT_BLK_BUS2_UID_LHS_AXI_P_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_csis[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D1_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_hsi1[] = {
	GOUT_BLK_BUS2_UID_LHS_AXI_P_HSI1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d0_mcsc[] = {
	GOUT_BLK_BUS2_UID_LHM_ACEL_D0_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_taa[] = {
	GOUT_BLK_BUS2_UID_LHS_AXI_P_TAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d_hsi1[] = {
	GOUT_BLK_BUS2_UID_LHM_ACEL_D_HSI1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_csis[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D0_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_yuvpp[] = {
	GOUT_BLK_BUS2_UID_LHS_AXI_P_YUVPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d2_csis[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D2_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_mcfp1[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D_MCFP1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_dns[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D1_DNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d2_mcsc[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D2_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_mfc0[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D0_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_mfc1[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D0_MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_mfc0[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D1_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_mfc1[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D1_MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d2_mcfp0[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D2_MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d3_csis[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D3_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d3_mcfp0[] = {
	GOUT_BLK_BUS2_UID_LHM_AXI_D3_MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_m2m[] = {
	GOUT_BLK_BUS2_UID_LHS_AXI_P_M2M_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mfc0[] = {
	GOUT_BLK_BUS2_UID_LHS_AXI_P_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mfc1[] = {
	GOUT_BLK_BUS2_UID_LHS_AXI_P_MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_ssp[] = {
	GOUT_BLK_BUS2_UID_LHS_AXI_P_SSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d_ssp[] = {
	GOUT_BLK_BUS2_UID_LHM_ACEL_D_SSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d_m2m[] = {
	GOUT_BLK_BUS2_UID_LHM_ACEL_D_M2M_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_peric1[] = {
	GOUT_BLK_BUS2_UID_LHS_AXI_P_PERIC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cmgp_cmu_cmgp[] = {
	CLK_BLK_CMGP_UID_CMGP_CMU_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_adc_cmgp[] = {
	GOUT_BLK_CMGP_UID_ADC_CMGP_IPCLKPORT_PCLK_S0,
	GOUT_BLK_CMGP_UID_ADC_CMGP_IPCLKPORT_PCLK_S1,
	CLK_BLK_CMGP_UID_ADC_CMGP_IPCLKPORT_I_OSCCLK,
};
enum clk_id cmucal_vclk_ip_gpio_cmgp[] = {
	GOUT_BLK_CMGP_UID_GPIO_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp0[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP0_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP0_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp1[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP1_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP1_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp2[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP2_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP2_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_i2c_cmgp3[] = {
	GOUT_BLK_CMGP_UID_I2C_CMGP3_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_I2C_CMGP3_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp0[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP0_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP0_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp1[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP1_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP1_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp2[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP2_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP2_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_usi_cmgp3[] = {
	GOUT_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_PCLK,
	GOUT_BLK_CMGP_UID_USI_CMGP3_IPCLKPORT_IPCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2pmu_ap[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2PMU_AP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_cmgp[] = {
	GOUT_BLK_CMGP_UID_D_TZPC_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_c_cmgp[] = {
	GOUT_BLK_CMGP_UID_LHM_AXI_C_CMGP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2apm[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2APM_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_i3c_cmgp[] = {
	GOUT_BLK_CMGP_UID_I3C_CMGP_IPCLKPORT_I_SCLK,
	GOUT_BLK_CMGP_UID_I3C_CMGP_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cmgp2cp[] = {
	GOUT_BLK_CMGP_UID_SYSREG_CMGP2CP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbif_gpio_cmgp[] = {
	GOUT_BLK_CMGP_UID_APBIF_GPIO_CMGP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_core_cmu_core[] = {
	CLK_BLK_CORE_UID_CORE_CMU_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_core[] = {
	GOUT_BLK_CORE_UID_SYSREG_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mpace2axi_0[] = {
	GOUT_BLK_CORE_UID_MPACE2AXI_0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_mpace2axi_1[] = {
	GOUT_BLK_CORE_UID_MPACE2AXI_1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppc_debug_cci[] = {
	GOUT_BLK_CORE_UID_PPC_DEBUG_CCI_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_DEBUG_CCI_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_p0_core[] = {
	GOUT_BLK_CORE_UID_TREX_P0_CORE_IPCLKPORT_ACLK_CORE,
	GOUT_BLK_CORE_UID_TREX_P0_CORE_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_TREX_P0_CORE_IPCLKPORT_PCLK_CORE,
};
enum clk_id cmucal_vclk_ip_lhs_atb_t_bdu[] = {
	GOUT_BLK_CORE_UID_LHS_ATB_T_BDU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_bdu[] = {
	GOUT_BLK_CORE_UID_BDU_IPCLKPORT_I_CLK,
	GOUT_BLK_CORE_UID_BDU_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_trex_p1_core[] = {
	GOUT_BLK_CORE_UID_TREX_P1_CORE_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_TREX_P1_CORE_IPCLKPORT_PCLK_CORE,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_g3d[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_cpucl0[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ace_d0_g3d[] = {
	GOUT_BLK_CORE_UID_LHM_ACE_D0_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ace_d1_g3d[] = {
	GOUT_BLK_CORE_UID_LHM_ACE_D1_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ace_d2_g3d[] = {
	GOUT_BLK_CORE_UID_LHM_ACE_D2_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ace_d3_g3d[] = {
	GOUT_BLK_CORE_UID_LHM_ACE_D3_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_trex_d_core[] = {
	GOUT_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_TREX_D_CORE_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppcfw_g3d[] = {
	GOUT_BLK_CORE_UID_PPCFW_G3D_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPCFW_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_apm[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_APM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_core[] = {
	GOUT_BLK_CORE_UID_D_TZPC_CORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_g3d0[] = {
	GOUT_BLK_CORE_UID_PPC_G3D0_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_G3D0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_g3d1[] = {
	GOUT_BLK_CORE_UID_PPC_G3D1_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_G3D1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_g3d2[] = {
	GOUT_BLK_CORE_UID_PPC_G3D2_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_G3D2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_g3d3[] = {
	GOUT_BLK_CORE_UID_PPC_G3D3_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_G3D3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_irps0[] = {
	GOUT_BLK_CORE_UID_PPC_IRPS0_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_IRPS0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_irps1[] = {
	GOUT_BLK_CORE_UID_PPC_IRPS1_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_IRPS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mpace_asb_d0_mif[] = {
	GOUT_BLK_CORE_UID_MPACE_ASB_D0_MIF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mpace_asb_d1_mif[] = {
	GOUT_BLK_CORE_UID_MPACE_ASB_D1_MIF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mpace_asb_d2_mif[] = {
	GOUT_BLK_CORE_UID_MPACE_ASB_D2_MIF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mpace_asb_d3_mif[] = {
	GOUT_BLK_CORE_UID_MPACE_ASB_D3_MIF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_axi_asb_cssys[] = {
	GOUT_BLK_CORE_UID_AXI_ASB_CSSYS_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_AXI_ASB_CSSYS_IPCLKPORT_ACLKM,
	GOUT_BLK_CORE_UID_AXI_ASB_CSSYS_IPCLKPORT_ACLKS,
};
enum clk_id cmucal_vclk_ip_lhm_axi_g_cssys[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_G_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cci[] = {
	CLK_BLK_CORE_UID_CCI_IPCLKPORT_PCLK,
	CLK_BLK_CORE_UID_CCI_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_g3d0[] = {
	GOUT_BLK_CORE_UID_PPMU_G3D0_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_PPMU_G3D0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_g3d1[] = {
	GOUT_BLK_CORE_UID_PPMU_G3D1_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_PPMU_G3D1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_g3d2[] = {
	GOUT_BLK_CORE_UID_PPMU_G3D2_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_PPMU_G3D2_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_g3d3[] = {
	GOUT_BLK_CORE_UID_PPMU_G3D3_IPCLKPORT_PCLK,
	GOUT_BLK_CORE_UID_PPMU_G3D3_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_g3d0[] = {
	GOUT_BLK_CORE_UID_SYSMMU_G3D0_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_g3d1[] = {
	GOUT_BLK_CORE_UID_SYSMMU_G3D1_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_g3d2[] = {
	GOUT_BLK_CORE_UID_SYSMMU_G3D2_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_g3d3[] = {
	GOUT_BLK_CORE_UID_SYSMMU_G3D3_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_xiu_d_core[] = {
	GOUT_BLK_CORE_UID_XIU_D_CORE_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_apb_async_sysmmu_g3d0[] = {
	GOUT_BLK_CORE_UID_APB_ASYNC_SYSMMU_G3D0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ace_slice_g3d0[] = {
	GOUT_BLK_CORE_UID_ACE_SLICE_G3D0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ace_slice_g3d1[] = {
	GOUT_BLK_CORE_UID_ACE_SLICE_G3D1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ace_slice_g3d2[] = {
	GOUT_BLK_CORE_UID_ACE_SLICE_G3D2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ace_slice_g3d3[] = {
	GOUT_BLK_CORE_UID_ACE_SLICE_G3D3_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_modem[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D0_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_modem[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D1_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_acel_d2_modem[] = {
	GOUT_BLK_CORE_UID_LHM_ACEL_D2_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_aud[] = {
	GOUT_BLK_CORE_UID_LHM_AXI_D_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_aud[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_modem[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_MODEM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppc_irps2[] = {
	GOUT_BLK_CORE_UID_PPC_IRPS2_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_IRPS2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_irps3[] = {
	GOUT_BLK_CORE_UID_PPC_IRPS3_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_IRPS3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_cpucl0_0[] = {
	GOUT_BLK_CORE_UID_PPC_CPUCL0_0_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_CPUCL0_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_cpucl0_1[] = {
	GOUT_BLK_CORE_UID_PPC_CPUCL0_1_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPC_CPUCL0_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_cpucl0_0[] = {
	GOUT_BLK_CORE_UID_PPMU_CPUCL0_0_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPMU_CPUCL0_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_cpucl0_1[] = {
	GOUT_BLK_CORE_UID_PPMU_CPUCL0_1_IPCLKPORT_ACLK,
	GOUT_BLK_CORE_UID_PPMU_CPUCL0_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_ace_d0_cluster0[] = {
	GOUT_BLK_CORE_UID_LHM_ACE_D0_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ace_d1_cluster0[] = {
	GOUT_BLK_CORE_UID_LHM_ACE_D1_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_baaw_cp[] = {
	GOUT_BLK_CORE_UID_BAAW_CP_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_modem[] = {
	GOUT_BLK_CORE_UID_SYSMMU_MODEM_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_peris[] = {
	GOUT_BLK_CORE_UID_LHS_AXI_P_PERIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mif0[] = {
	GOUT_BLK_CORE_UID_PPMU_MIF0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mif1[] = {
	GOUT_BLK_CORE_UID_PPMU_MIF1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mif2[] = {
	GOUT_BLK_CORE_UID_PPMU_MIF2_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mif3[] = {
	GOUT_BLK_CORE_UID_PPMU_MIF3_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_modem[] = {
	GOUT_BLK_CORE_UID_VGEN_LITE_MODEM_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_cpucl0_cmu_cpucl0[] = {
	CLK_BLK_CPUCL0_UID_CPUCL0_CMU_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_add_cpucl0_0[] = {
	CLK_BLK_CPUCL0_UID_ADD_CPUCL0_0_IPCLKPORT_CH_CLK,
	CLK_BLK_CPUCL0_UID_ADD_CPUCL0_0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_busif_add_cpucl0_0[] = {
	GOUT_BLK_CPUCL0_UID_BUSIF_ADD_CPUCL0_0_IPCLKPORT_CLK_CORE,
	GOUT_BLK_CPUCL0_UID_BUSIF_ADD_CPUCL0_0_IPCLKPORT_CLK_CORE,
	CLK_BLK_CPUCL0_UID_BUSIF_ADD_CPUCL0_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_ddd_cpucl0_0[] = {
	GOUT_BLK_CPUCL0_UID_BUSIF_DDD_CPUCL0_0_IPCLKPORT_CK_IN,
	CLK_BLK_CPUCL0_UID_BUSIF_DDD_CPUCL0_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_str_cpucl0_0[] = {
	GOUT_BLK_CPUCL0_UID_BUSIF_STR_CPUCL0_0_IPCLKPORT_CLK_CORE,
	GOUT_BLK_CPUCL0_UID_BUSIF_STR_CPUCL0_0_IPCLKPORT_CLK_CORE,
	CLK_BLK_CPUCL0_UID_BUSIF_STR_CPUCL0_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_htu_cpucl0[] = {
	CLK_BLK_CPUCL0_UID_HTU_CPUCL0_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_hwacg_busif_ddd_cpucl0_0[] = {
	CLK_BLK_CPUCL0_UID_HWACG_BUSIF_DDD_CPUCL0_0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ddd_cpucl0_0[] = {
	CLK_BLK_CPUCL0_UID_DDD_CPUCL0_0_IPCLKPORT_CK_IN,
};
enum clk_id cmucal_vclk_ip_adm_apb_g_cluster0[] = {
	CLK_BLK_CPUCL0_GLB_UID_ADM_APB_G_CLUSTER0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhm_axi_g_int_etr[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHM_AXI_G_INT_ETR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_g_cssys[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHS_AXI_G_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_g_int_stm[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHM_AXI_G_INT_STM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_cpucl0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHM_AXI_P_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_trex_cpucl0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_TREX_CPUCL0_IPCLKPORT_CLK,
	GOUT_BLK_CPUCL0_GLB_UID_TREX_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_secjtag[] = {
	GOUT_BLK_CPUCL0_GLB_UID_SECJTAG_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_bps_cpucl0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_BPS_CPUCL0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_g_int_cssys[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHM_AXI_G_INT_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_t_bdu[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHM_ATB_T_BDU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_p_cpucl0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_XIU_P_CPUCL0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_cpucl0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_D_TZPC_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_cpucl0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_SYSREG_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_g_dbgcore[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHM_AXI_G_DBGCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_busif_hpm_cpucl0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_BUSIF_HPM_CPUCL0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_dp_cssys[] = {
	GOUT_BLK_CPUCL0_GLB_UID_XIU_DP_CSSYS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_g_int_stm[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHS_AXI_G_INT_STM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_g_int_dbgcore[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHM_AXI_G_INT_DBGCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_apb_async_p_cssys_0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_APB_ASYNC_P_CSSYS_0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_cssys[] = {
	GOUT_BLK_CPUCL0_GLB_UID_CSSYS_IPCLKPORT_ATCLK,
	GOUT_BLK_CPUCL0_GLB_UID_CSSYS_IPCLKPORT_PCLKDBG,
};
enum clk_id cmucal_vclk_ip_lhs_axi_g_int_cssys[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHS_AXI_G_INT_CSSYS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_g_int_etr[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHS_AXI_G_INT_ETR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_g_int_dbgcore[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHS_AXI_G_INT_DBGCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cpucl0_glb_cmu_cpucl0_glb[] = {
	CLK_BLK_CPUCL0_GLB_UID_CPUCL0_GLB_CMU_CPUCL0_GLB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_hpm_cpucl0_0[] = {
	CLK_BLK_CPUCL0_GLB_UID_HPM_CPUCL0_0_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_hpm_cpucl0_1[] = {
	CLK_BLK_CPUCL0_GLB_UID_HPM_CPUCL0_1_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_hpm_cpucl0_2[] = {
	CLK_BLK_CPUCL0_GLB_UID_HPM_CPUCL0_2_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_lhm_atb_t0_cluster0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHM_ATB_T0_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_t1_cluster0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHM_ATB_T1_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_t2_cluster0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHM_ATB_T2_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_t3_cluster0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHM_ATB_T3_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_t4_cluster0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHM_ATB_T4_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_t5_cluster0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHM_ATB_T5_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_t6_cluster0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHM_ATB_T6_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_atb_t7_cluster0[] = {
	GOUT_BLK_CPUCL0_GLB_UID_LHM_ATB_T7_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cpucl1_cmu_cpucl1[] = {
	CLK_BLK_CPUCL1_UID_CPUCL1_CMU_CPUCL1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_add_cpucl0_1[] = {
	CLK_BLK_CPUCL1_UID_ADD_CPUCL0_1_IPCLKPORT_CH_CLK,
	CLK_BLK_CPUCL1_UID_ADD_CPUCL0_1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_busif_add_cpucl0_1[] = {
	CLK_BLK_CPUCL1_UID_BUSIF_ADD_CPUCL0_1_IPCLKPORT_PCLK,
	GOUT_BLK_CPUCL1_UID_BUSIF_ADD_CPUCL0_1_IPCLKPORT_CLK_CORE,
	GOUT_BLK_CPUCL1_UID_BUSIF_ADD_CPUCL0_1_IPCLKPORT_CLK_CORE,
};
enum clk_id cmucal_vclk_ip_busif_ddd_cpucl0_2[] = {
	GOUT_BLK_CPUCL1_UID_BUSIF_DDD_CPUCL0_2_IPCLKPORT_CK_IN,
	CLK_BLK_CPUCL1_UID_BUSIF_DDD_CPUCL0_2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_ddd_cpucl0_3[] = {
	GOUT_BLK_CPUCL1_UID_BUSIF_DDD_CPUCL0_3_IPCLKPORT_CK_IN,
	CLK_BLK_CPUCL1_UID_BUSIF_DDD_CPUCL0_3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_ddd_cpucl0_4[] = {
	GOUT_BLK_CPUCL1_UID_BUSIF_DDD_CPUCL0_4_IPCLKPORT_CK_IN,
	CLK_BLK_CPUCL1_UID_BUSIF_DDD_CPUCL0_4_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_str_cpucl0_1[] = {
	GOUT_BLK_CPUCL1_UID_BUSIF_STR_CPUCL0_1_IPCLKPORT_CLK_CORE,
	GOUT_BLK_CPUCL1_UID_BUSIF_STR_CPUCL0_1_IPCLKPORT_CLK_CORE,
	CLK_BLK_CPUCL1_UID_BUSIF_STR_CPUCL0_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_htu_cpucl1[] = {
	CLK_BLK_CPUCL1_UID_HTU_CPUCL1_IPCLKPORT_I_PCLK,
	GOUT_BLK_CPUCL1_UID_HTU_CPUCL1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_hwacg_busif_ddd_cpucl0_2[] = {
	CLK_BLK_CPUCL1_UID_HWACG_BUSIF_DDD_CPUCL0_2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_hwacg_busif_ddd_cpucl0_4[] = {
	CLK_BLK_CPUCL1_UID_HWACG_BUSIF_DDD_CPUCL0_4_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cpucl1[] = {
	CLK_BLK_CPUCL1_UID_CPUCL1_IPCLKPORT_HHA11STQ_DDD_CK_IN_HC0,
	CLK_BLK_CPUCL1_UID_CPUCL1_IPCLKPORT_HHA11STQ_DDD_CK_IN_HC1,
	CLK_BLK_CPUCL1_UID_CPUCL1_IPCLKPORT_HHA11STQ_DDD_CK_IN_HC2,
};
enum clk_id cmucal_vclk_ip_hwacg_busif_ddd_cpucl0_3[] = {
	CLK_BLK_CPUCL1_UID_HWACG_BUSIF_DDD_CPUCL0_3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_cpucl2_cmu_cpucl2[] = {
	CLK_BLK_CPUCL2_UID_CPUCL2_CMU_CPUCL2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_str_cpucl0_2[] = {
	CLK_BLK_CPUCL2_UID_BUSIF_STR_CPUCL0_2_IPCLKPORT_PCLK,
	GOUT_BLK_CPUCL2_UID_BUSIF_STR_CPUCL0_2_IPCLKPORT_CLK_CORE,
	GOUT_BLK_CPUCL2_UID_BUSIF_STR_CPUCL0_2_IPCLKPORT_CLK_CORE,
};
enum clk_id cmucal_vclk_ip_add_cpucl0_2[] = {
	CLK_BLK_CPUCL2_UID_ADD_CPUCL0_2_IPCLKPORT_CH_CLK,
	CLK_BLK_CPUCL2_UID_ADD_CPUCL0_2_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_busif_add_cpucl0_2[] = {
	CLK_BLK_CPUCL2_UID_BUSIF_ADD_CPUCL0_2_IPCLKPORT_PCLK,
	GOUT_BLK_CPUCL2_UID_BUSIF_ADD_CPUCL0_2_IPCLKPORT_CLK_CORE,
	GOUT_BLK_CPUCL2_UID_BUSIF_ADD_CPUCL0_2_IPCLKPORT_CLK_CORE,
};
enum clk_id cmucal_vclk_ip_busif_ddd_cpucl0_1[] = {
	CLK_BLK_CPUCL2_UID_BUSIF_DDD_CPUCL0_1_IPCLKPORT_PCLK,
	GOUT_BLK_CPUCL2_UID_BUSIF_DDD_CPUCL0_1_IPCLKPORT_CK_IN,
};
enum clk_id cmucal_vclk_ip_htu_cpucl2[] = {
	CLK_BLK_CPUCL2_UID_HTU_CPUCL2_IPCLKPORT_I_PCLK,
	GOUT_BLK_CPUCL2_UID_HTU_CPUCL2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_hwacg_busif_ddd_cpucl0_1[] = {
	CLK_BLK_CPUCL2_UID_HWACG_BUSIF_DDD_CPUCL0_1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ddd_cpucl0_1[] = {
	CLK_BLK_CPUCL2_UID_DDD_CPUCL0_1_IPCLKPORT_CK_IN,
};
enum clk_id cmucal_vclk_ip_mipi_phy_link_wrap[] = {
	GOUT_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_ACLK_CSIS0,
	GOUT_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_ACLK_CSIS1,
	GOUT_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_ACLK_CSIS2,
	GOUT_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_ACLK_CSIS3,
	GOUT_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_ACLK_CSIS4,
	GOUT_BLK_CSIS_UID_MIPI_PHY_LINK_WRAP_IPCLKPORT_ACLK_CSIS5,
};
enum clk_id cmucal_vclk_ip_pdp_top[] = {
	GOUT_BLK_CSIS_UID_PDP_TOP_IPCLKPORT_CLK,
	GOUT_BLK_CSIS_UID_PDP_TOP_IPCLKPORT_C2CLK,
};
enum clk_id cmucal_vclk_ip_qe_pdp_stat_img0[] = {
	GOUT_BLK_CSIS_UID_QE_PDP_STAT_IMG0_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_PDP_STAT_IMG0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_csis_dma3[] = {
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA3_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_pdp_stat_af0[] = {
	GOUT_BLK_CSIS_UID_QE_PDP_STAT_AF0_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_PDP_STAT_AF0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_pdp_af1[] = {
	GOUT_BLK_CSIS_UID_QE_PDP_AF1_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_PDP_AF1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_pdp_stat_img1[] = {
	GOUT_BLK_CSIS_UID_QE_PDP_STAT_IMG1_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_PDP_STAT_IMG1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_strp1[] = {
	GOUT_BLK_CSIS_UID_QE_STRP1_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_STRP1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_strp2[] = {
	GOUT_BLK_CSIS_UID_QE_STRP2_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_STRP2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_zsl1[] = {
	GOUT_BLK_CSIS_UID_QE_ZSL1_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_ZSL1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_zsl2[] = {
	GOUT_BLK_CSIS_UID_QE_ZSL2_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_ZSL2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_pdp_core[] = {
	GOUT_BLK_CSIS_UID_AD_APB_PDP_CORE_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_vgen_lite_d1[] = {
	GOUT_BLK_CSIS_UID_VGEN_LITE_D1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_d2[] = {
	GOUT_BLK_CSIS_UID_VGEN_LITE_D2_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf0_csistaa[] = {
	GOUT_BLK_CSIS_UID_LHS_AST_OTF0_CSISTAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf1_csistaa[] = {
	GOUT_BLK_CSIS_UID_LHS_AST_OTF1_CSISTAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf2_csistaa[] = {
	GOUT_BLK_CSIS_UID_LHS_AST_OTF2_CSISTAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_int_vo_pdpcsis[] = {
	GOUT_BLK_CSIS_UID_LHS_AST_INT_VO_PDPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_csis_cmu_csis[] = {
	CLK_BLK_CSIS_UID_CSIS_CMU_CSIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_csis[] = {
	GOUT_BLK_CSIS_UID_LHS_AXI_D0_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_csis[] = {
	GOUT_BLK_CSIS_UID_LHS_AXI_D1_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_csis[] = {
	GOUT_BLK_CSIS_UID_D_TZPC_CSIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_csisx6[] = {
	GOUT_BLK_CSIS_UID_CSISX6_IPCLKPORT_ACLK_VOTF0,
	GOUT_BLK_CSIS_UID_CSISX6_IPCLKPORT_ACLK_DMA,
	GOUT_BLK_CSIS_UID_CSISX6_IPCLKPORT_ACLK_MCB,
	GOUT_BLK_CSIS_UID_CSISX6_IPCLKPORT_ACLK_VOTF1,
};
enum clk_id cmucal_vclk_ip_ppmu_d0[] = {
	GOUT_BLK_CSIS_UID_PPMU_D0_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_PPMU_D0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d1[] = {
	GOUT_BLK_CSIS_UID_PPMU_D1_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_PPMU_D1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d0_csis[] = {
	GOUT_BLK_CSIS_UID_SYSMMU_D0_CSIS_IPCLKPORT_CLK_S1,
	GOUT_BLK_CSIS_UID_SYSMMU_D0_CSIS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_csis[] = {
	GOUT_BLK_CSIS_UID_SYSMMU_D1_CSIS_IPCLKPORT_CLK_S1,
	GOUT_BLK_CSIS_UID_SYSMMU_D1_CSIS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_csis[] = {
	GOUT_BLK_CSIS_UID_SYSREG_CSIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_d0[] = {
	GOUT_BLK_CSIS_UID_VGEN_LITE_D0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_csis[] = {
	GOUT_BLK_CSIS_UID_LHM_AXI_P_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_int_vo_pdpcsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_INT_VO_PDPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_zotf0_taacsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_ZOTF0_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_zotf1_taacsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_ZOTF1_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_zotf2_taacsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_ZOTF2_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_csis0[] = {
	GOUT_BLK_CSIS_UID_AD_APB_CSIS0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_xiu_d0_csis[] = {
	GOUT_BLK_CSIS_UID_XIU_D0_CSIS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d1_csis[] = {
	GOUT_BLK_CSIS_UID_XIU_D1_CSIS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_csis_dma1[] = {
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA1_IPCLKPORT_PCLK,
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_zsl0[] = {
	GOUT_BLK_CSIS_UID_QE_ZSL0_IPCLKPORT_PCLK,
	GOUT_BLK_CSIS_UID_QE_ZSL0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_strp0[] = {
	GOUT_BLK_CSIS_UID_QE_STRP0_IPCLKPORT_PCLK,
	GOUT_BLK_CSIS_UID_QE_STRP0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_sotf0_taacsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_SOTF0_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_sotf1_taacsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_SOTF1_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_sotf2_taacsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_SOTF2_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d2[] = {
	GOUT_BLK_CSIS_UID_PPMU_D2_IPCLKPORT_PCLK,
	GOUT_BLK_CSIS_UID_PPMU_D2_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ois_mcu_top[] = {
	GOUT_BLK_CSIS_UID_OIS_MCU_TOP_IPCLKPORT_I_ACLK,
};
enum clk_id cmucal_vclk_ip_ad_axi_ois_mcu_top[] = {
	GOUT_BLK_CSIS_UID_AD_AXI_OIS_MCU_TOP_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_qe_csis_dma0[] = {
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA0_IPCLKPORT_PCLK,
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_csisperic1[] = {
	GOUT_BLK_CSIS_UID_LHS_AXI_P_CSISPERIC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf3_csistaa[] = {
	GOUT_BLK_CSIS_UID_LHS_AST_OTF3_CSISTAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_zotf3_taacsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_ZOTF3_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_sotf3_taacsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_SOTF3_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_int_vo_csispdp[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_INT_VO_CSISPDP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_int_vo_csispdp[] = {
	GOUT_BLK_CSIS_UID_LHS_AST_INT_VO_CSISPDP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d2_csis[] = {
	GOUT_BLK_CSIS_UID_XIU_D2_CSIS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d3[] = {
	GOUT_BLK_CSIS_UID_PPMU_D3_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_PPMU_D3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d2_csis[] = {
	GOUT_BLK_CSIS_UID_SYSMMU_D2_CSIS_IPCLKPORT_CLK_S1,
	GOUT_BLK_CSIS_UID_SYSMMU_D2_CSIS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_d3_csis[] = {
	GOUT_BLK_CSIS_UID_SYSMMU_D3_CSIS_IPCLKPORT_CLK_S1,
	GOUT_BLK_CSIS_UID_SYSMMU_D3_CSIS_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_qe_csis_dma2[] = {
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA2_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_CSIS_DMA2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d2_csis[] = {
	GOUT_BLK_CSIS_UID_LHS_AXI_D2_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d3_csis[] = {
	GOUT_BLK_CSIS_UID_LHS_AXI_D3_CSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_qe_pdp_stat_img2[] = {
	GOUT_BLK_CSIS_UID_QE_PDP_STAT_IMG2_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_PDP_STAT_IMG2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_pdp_af2[] = {
	GOUT_BLK_CSIS_UID_QE_PDP_AF2_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_PDP_AF2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d4_csis[] = {
	GOUT_BLK_CSIS_UID_XIU_D4_CSIS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d3_csis[] = {
	GOUT_BLK_CSIS_UID_XIU_D3_CSIS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ad_axi_strp[] = {
	GOUT_BLK_CSIS_UID_AD_AXI_STRP_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_lhm_ast_int_otf0_csispdp[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_INT_OTF0_CSISPDP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_int_otf1_csispdp[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_INT_OTF1_CSISPDP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_int_otf2_csispdp[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_INT_OTF2_CSISPDP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_int_otf3_csispdp[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_INT_OTF3_CSISPDP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_int_otf0_csispdp[] = {
	GOUT_BLK_CSIS_UID_LHS_AST_INT_OTF0_CSISPDP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_int_otf1_csispdp[] = {
	GOUT_BLK_CSIS_UID_LHS_AST_INT_OTF1_CSISPDP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_int_otf2_csispdp[] = {
	GOUT_BLK_CSIS_UID_LHS_AST_INT_OTF2_CSISPDP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_int_otf3_csispdp[] = {
	GOUT_BLK_CSIS_UID_LHS_AST_INT_OTF3_CSISPDP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_int_otf0_pdpcsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_INT_OTF0_PDPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_int_otf1_pdpcsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_INT_OTF1_PDPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_int_otf2_pdpcsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_INT_OTF2_PDPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_int_otf3_pdpcsis[] = {
	GOUT_BLK_CSIS_UID_LHM_AST_INT_OTF3_PDPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_int_otf0_pdpcsis[] = {
	GOUT_BLK_CSIS_UID_LHS_AST_INT_OTF0_PDPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_int_otf1_pdpcsis[] = {
	GOUT_BLK_CSIS_UID_LHS_AST_INT_OTF1_PDPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_int_otf2_pdpcsis[] = {
	GOUT_BLK_CSIS_UID_LHS_AST_INT_OTF2_PDPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_int_otf3_pdpcsis[] = {
	GOUT_BLK_CSIS_UID_LHS_AST_INT_OTF3_PDPCSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_qe_strp3[] = {
	GOUT_BLK_CSIS_UID_QE_STRP3_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_STRP3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_zsl3[] = {
	GOUT_BLK_CSIS_UID_QE_ZSL3_IPCLKPORT_ACLK,
	GOUT_BLK_CSIS_UID_QE_ZSL3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dns_cmu_dns[] = {
	CLK_BLK_DNS_UID_DNS_CMU_DNS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf4_itpdns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_OTF4_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d0_dns[] = {
	GOUT_BLK_DNS_UID_XIU_D0_DNS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf_taadns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_OTF_TAADNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_dns[] = {
	GOUT_BLK_DNS_UID_SYSMMU_D1_DNS_IPCLKPORT_CLK_S2,
	GOUT_BLK_DNS_UID_SYSMMU_D1_DNS_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_itpdns[] = {
	GOUT_BLK_DNS_UID_LHM_AXI_P_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf3_itpdns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_OTF3_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_dns[] = {
	GOUT_BLK_DNS_UID_D_TZPC_DNS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf_mcfp1dns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_OTF_MCFP1DNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf2_itpdns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_OTF2_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_dns0[] = {
	GOUT_BLK_DNS_UID_AD_APB_DNS0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_dns[] = {
	GOUT_BLK_DNS_UID_DNS_IPCLKPORT_I_CLK,
	GOUT_BLK_DNS_UID_DNS_IPCLKPORT_I_CLK_VOTF0,
	GOUT_BLK_DNS_UID_DNS_IPCLKPORT_I_CLK_VOTF0,
	GOUT_BLK_DNS_UID_DNS_IPCLKPORT_I_CLK_VOTF1,
	GOUT_BLK_DNS_UID_DNS_IPCLKPORT_I_CLK_VOTF1,
	GOUT_BLK_DNS_UID_DNS_IPCLKPORT_I_CLK_VOTF2,
	GOUT_BLK_DNS_UID_DNS_IPCLKPORT_I_CLK_VOTF2,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf0_itpdns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_OTF0_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf1_itpdns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_OTF1_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d1_dns[] = {
	GOUT_BLK_DNS_UID_XIU_D1_DNS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf9_dnsitp[] = {
	GOUT_BLK_DNS_UID_LHS_AST_OTF9_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf8_dnsitp[] = {
	GOUT_BLK_DNS_UID_LHS_AST_OTF8_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_dns[] = {
	GOUT_BLK_DNS_UID_SYSREG_DNS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_dns[] = {
	GOUT_BLK_DNS_UID_LHS_AXI_D0_DNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d0_dns[] = {
	GOUT_BLK_DNS_UID_PPMU_D0_DNS_IPCLKPORT_PCLK,
	GOUT_BLK_DNS_UID_PPMU_D0_DNS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf5_dnsitp[] = {
	GOUT_BLK_DNS_UID_LHS_AST_OTF5_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf4_dnsitp[] = {
	GOUT_BLK_DNS_UID_LHS_AST_OTF4_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf6_dnsitp[] = {
	GOUT_BLK_DNS_UID_LHS_AST_OTF6_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf0_dnsitp[] = {
	GOUT_BLK_DNS_UID_LHS_AST_OTF0_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf1_dnsitp[] = {
	GOUT_BLK_DNS_UID_LHS_AST_OTF1_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf2_dnsitp[] = {
	GOUT_BLK_DNS_UID_LHS_AST_OTF2_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf3_dnsitp[] = {
	GOUT_BLK_DNS_UID_LHS_AST_OTF3_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf7_dnsitp[] = {
	GOUT_BLK_DNS_UID_LHS_AST_OTF7_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_dns[] = {
	GOUT_BLK_DNS_UID_LHS_AXI_D1_DNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_ctl_itpdns[] = {
	GOUT_BLK_DNS_UID_LHM_AST_CTL_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d1_dns[] = {
	GOUT_BLK_DNS_UID_PPMU_D1_DNS_IPCLKPORT_PCLK,
	GOUT_BLK_DNS_UID_PPMU_D1_DNS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d0_dns[] = {
	GOUT_BLK_DNS_UID_SYSMMU_D0_DNS_IPCLKPORT_CLK_S2,
	GOUT_BLK_DNS_UID_SYSMMU_D0_DNS_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_lhs_ast_ctl_dnsitp[] = {
	GOUT_BLK_DNS_UID_LHS_AST_CTL_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_d0_dns[] = {
	GOUT_BLK_DNS_UID_VGEN_LITE_D0_DNS_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_d1_dns[] = {
	GOUT_BLK_DNS_UID_VGEN_LITE_D1_DNS_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_dpub_cmu_dpub[] = {
	CLK_BLK_DPUB_UID_DPUB_CMU_DPUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_dpub[] = {
	GOUT_BLK_DPUB_UID_LHM_AXI_P_DPUB_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_dpub[] = {
	GOUT_BLK_DPUB_UID_D_TZPC_DPUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_dpub[] = {
	GOUT_BLK_DPUB_UID_SYSREG_DPUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dpub[] = {
	GOUT_BLK_DPUB_UID_DPUB_IPCLKPORT_ACLK_DECON,
};
enum clk_id cmucal_vclk_ip_ad_apb_decon_main[] = {
	GOUT_BLK_DPUB_UID_AD_APB_DECON_MAIN_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_dpuf0_cmu_dpuf0[] = {
	CLK_BLK_DPUF0_UID_DPUF0_CMU_DPUF0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_dpuf0[] = {
	GOUT_BLK_DPUF0_UID_SYSREG_DPUF0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_dpuf0d0[] = {
	GOUT_BLK_DPUF0_UID_SYSMMU_DPUF0D0_IPCLKPORT_CLK_S1,
	GOUT_BLK_DPUF0_UID_SYSMMU_DPUF0D0_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_dpuf0[] = {
	GOUT_BLK_DPUF0_UID_LHM_AXI_P_DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_dpuf0[] = {
	GOUT_BLK_DPUF0_UID_LHS_AXI_D1_DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_dpuf0_dma[] = {
	GOUT_BLK_DPUF0_UID_AD_APB_DPUF0_DMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_sysmmu_dpuf0d1[] = {
	GOUT_BLK_DPUF0_UID_SYSMMU_DPUF0D1_IPCLKPORT_CLK_S1,
	GOUT_BLK_DPUF0_UID_SYSMMU_DPUF0D1_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_ppmu_dpuf0d0[] = {
	GOUT_BLK_DPUF0_UID_PPMU_DPUF0D0_IPCLKPORT_ACLK,
	GOUT_BLK_DPUF0_UID_PPMU_DPUF0D0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dpuf0d1[] = {
	GOUT_BLK_DPUF0_UID_PPMU_DPUF0D1_IPCLKPORT_ACLK,
	GOUT_BLK_DPUF0_UID_PPMU_DPUF0D1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_dpuf0[] = {
	GOUT_BLK_DPUF0_UID_LHS_AXI_D0_DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_dpuf0[] = {
	GOUT_BLK_DPUF0_UID_DPUF0_IPCLKPORT_ACLK_DMA,
	GOUT_BLK_DPUF0_UID_DPUF0_IPCLKPORT_ACLK_DPP,
	GOUT_BLK_DPUF0_UID_DPUF0_IPCLKPORT_ACLK_C2SERV,
};
enum clk_id cmucal_vclk_ip_d_tzpc_dpuf0[] = {
	GOUT_BLK_DPUF0_UID_D_TZPC_DPUF0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_dpuf1dpuf0[] = {
	GOUT_BLK_DPUF0_UID_LHM_AXI_D_DPUF1DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_dpuf1_cmu_dpuf1[] = {
	CLK_BLK_DPUF1_UID_DPUF1_CMU_DPUF1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_dpuf1_dma[] = {
	GOUT_BLK_DPUF1_UID_AD_APB_DPUF1_DMA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_dpuf1[] = {
	GOUT_BLK_DPUF1_UID_LHS_AXI_D1_DPUF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_dpuf1[] = {
	GOUT_BLK_DPUF1_UID_DPUF1_IPCLKPORT_ACLK_DMA,
	GOUT_BLK_DPUF1_UID_DPUF1_IPCLKPORT_ACLK_DPP,
	GOUT_BLK_DPUF1_UID_DPUF1_IPCLKPORT_ACLK_C2SERV,
};
enum clk_id cmucal_vclk_ip_ppmu_dpuf1d0[] = {
	GOUT_BLK_DPUF1_UID_PPMU_DPUF1D0_IPCLKPORT_PCLK,
	GOUT_BLK_DPUF1_UID_PPMU_DPUF1D0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_dpuf1d1[] = {
	GOUT_BLK_DPUF1_UID_PPMU_DPUF1D1_IPCLKPORT_PCLK,
	GOUT_BLK_DPUF1_UID_PPMU_DPUF1D1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_dpuf1[] = {
	GOUT_BLK_DPUF1_UID_D_TZPC_DPUF1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_dpuf1[] = {
	GOUT_BLK_DPUF1_UID_LHS_AXI_D0_DPUF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_dpuf1[] = {
	GOUT_BLK_DPUF1_UID_LHM_AXI_P_DPUF1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_dpuf1d0[] = {
	GOUT_BLK_DPUF1_UID_SYSMMU_DPUF1D0_IPCLKPORT_CLK_S2,
	GOUT_BLK_DPUF1_UID_SYSMMU_DPUF1D0_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_sysmmu_dpuf1d1[] = {
	GOUT_BLK_DPUF1_UID_SYSMMU_DPUF1D1_IPCLKPORT_CLK_S2,
	GOUT_BLK_DPUF1_UID_SYSMMU_DPUF1D1_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_sysreg_dpuf1[] = {
	GOUT_BLK_DPUF1_UID_SYSREG_DPUF1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_dpuf1dpuf0[] = {
	GOUT_BLK_DPUF1_UID_LHS_AXI_D_DPUF1DPUF0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_cluster0[] = {
	CLK_BLK_DSU_UID_CLUSTER0_IPCLKPORT_GICCLK,
	CLK_BLK_DSU_UID_CLUSTER0_IPCLKPORT_PCLK,
	CLK_BLK_DSU_UID_CLUSTER0_IPCLKPORT_PERIPHCLK,
	CLK_BLK_DSU_UID_CLUSTER0_IPCLKPORT_ATCLK,
};
enum clk_id cmucal_vclk_ip_dsu_cmu_dsu[] = {
	CLK_BLK_DSU_UID_DSU_CMU_DSU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_iri_giccpu_cluster0[] = {
	GOUT_BLK_DSU_UID_LHM_AST_IRI_GICCPU_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_icc_cpugic_cluster0[] = {
	GOUT_BLK_DSU_UID_LHS_AST_ICC_CPUGIC_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_atb_t0_cluster0[] = {
	GOUT_BLK_DSU_UID_LHS_ATB_T0_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_atb_t1_cluster0[] = {
	GOUT_BLK_DSU_UID_LHS_ATB_T1_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_atb_t2_cluster0[] = {
	GOUT_BLK_DSU_UID_LHS_ATB_T2_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_atb_t3_cluster0[] = {
	GOUT_BLK_DSU_UID_LHS_ATB_T3_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_atb_t4_cluster0[] = {
	GOUT_BLK_DSU_UID_LHS_ATB_T4_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_atb_t5_cluster0[] = {
	GOUT_BLK_DSU_UID_LHS_ATB_T5_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_atb_t6_cluster0[] = {
	GOUT_BLK_DSU_UID_LHS_ATB_T6_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_atb_t7_cluster0[] = {
	GOUT_BLK_DSU_UID_LHS_ATB_T7_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_busif_str_cpucl0_3[] = {
	GOUT_BLK_DSU_UID_BUSIF_STR_CPUCL0_3_IPCLKPORT_CLK_CORE,
	GOUT_BLK_DSU_UID_BUSIF_STR_CPUCL0_3_IPCLKPORT_CLK_CORE,
	CLK_BLK_DSU_UID_BUSIF_STR_CPUCL0_3_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_htu_dsu[] = {
	CLK_BLK_DSU_UID_HTU_DSU_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_instrret_cluster0_0[] = {
	CLK_BLK_DSU_UID_PPC_INSTRRET_CLUSTER0_0_IPCLKPORT_PCLK,
	GOUT_BLK_DSU_UID_PPC_INSTRRET_CLUSTER0_0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppc_instrret_cluster0_1[] = {
	CLK_BLK_DSU_UID_PPC_INSTRRET_CLUSTER0_1_IPCLKPORT_PCLK,
	GOUT_BLK_DSU_UID_PPC_INSTRRET_CLUSTER0_1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppc_instrrun_cluster0_0[] = {
	CLK_BLK_DSU_UID_PPC_INSTRRUN_CLUSTER0_0_IPCLKPORT_PCLK,
	GOUT_BLK_DSU_UID_PPC_INSTRRUN_CLUSTER0_0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppc_instrrun_cluster0_1[] = {
	CLK_BLK_DSU_UID_PPC_INSTRRUN_CLUSTER0_1_IPCLKPORT_PCLK,
	GOUT_BLK_DSU_UID_PPC_INSTRRUN_CLUSTER0_1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ace_us_128to256_d0_cluster0[] = {
	GOUT_BLK_DSU_UID_ACE_US_128TO256_D0_CLUSTER0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ace_us_128to256_d1_cluster0[] = {
	GOUT_BLK_DSU_UID_ACE_US_128TO256_D1_CLUSTER0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_ace_d0_cluster0[] = {
	GOUT_BLK_DSU_UID_LHS_ACE_D0_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ace_d1_cluster0[] = {
	GOUT_BLK_DSU_UID_LHS_ACE_D1_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_g3d[] = {
	GOUT_BLK_G3D_UID_LHM_AXI_P_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_busif_hpmg3d[] = {
	GOUT_BLK_G3D_UID_BUSIF_HPMG3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_hpm_g3d[] = {
	CLK_BLK_G3D_UID_HPM_G3D_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_sysreg_g3d[] = {
	GOUT_BLK_G3D_UID_SYSREG_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_g3d_cmu_g3d[] = {
	CLK_BLK_G3D_UID_G3D_CMU_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_int_g3d[] = {
	GOUT_BLK_G3D_UID_LHS_AXI_P_INT_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_g3d[] = {
	GOUT_BLK_G3D_UID_VGEN_LITE_G3D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_int_g3d[] = {
	GOUT_BLK_G3D_UID_LHM_AXI_P_INT_G3D_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_gray2bin_g3d[] = {
	GOUT_BLK_G3D_UID_GRAY2BIN_G3D_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_g3d[] = {
	GOUT_BLK_G3D_UID_D_TZPC_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_add_apbif_g3d[] = {
	GOUT_BLK_G3D_UID_ADD_APBIF_G3D_IPCLKPORT_PCLK,
	GOUT_BLK_G3D_UID_ADD_APBIF_G3D_IPCLKPORT_CLK_CORE,
	GOUT_BLK_G3D_UID_ADD_APBIF_G3D_IPCLKPORT_CLK_CORE,
};
enum clk_id cmucal_vclk_ip_add_g3d[] = {
	CLK_BLK_G3D_UID_ADD_G3D_IPCLKPORT_CLK,
	CLK_BLK_G3D_UID_ADD_G3D_IPCLKPORT_CH_CLK,
};
enum clk_id cmucal_vclk_ip_ddd_apbif_g3d[] = {
	GOUT_BLK_G3D_UID_DDD_APBIF_G3D_IPCLKPORT_CK_IN,
	GOUT_BLK_G3D_UID_DDD_APBIF_G3D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpu[] = {
	CLK_BLK_G3D_UID_GPU_IPCLKPORT_CLK,
	CLK_BLK_G3D_UID_GPU_IPCLKPORT_CLK_COREGROUP,
	CLK_BLK_G3D_UID_GPU_IPCLKPORT_CLK_COREGROUP,
	CLK_BLK_G3D_UID_GPU_IPCLKPORT_CLK_STACKS,
	CLK_BLK_G3D_UID_GPU_IPCLKPORT_CLK_STACKS,
};
enum clk_id cmucal_vclk_ip_busif_str_g3d[] = {
	GOUT_BLK_G3D_UID_BUSIF_STR_G3D_IPCLKPORT_PCLK,
	GOUT_BLK_G3D_UID_BUSIF_STR_G3D_IPCLKPORT_CLK_CORE,
	GOUT_BLK_G3D_UID_BUSIF_STR_G3D_IPCLKPORT_CLK_CORE,
};
enum clk_id cmucal_vclk_ip_ddd_g3d[] = {
	GOUT_BLK_G3D_UID_DDD_G3D_IPCLKPORT_CK_IN,
};
enum clk_id cmucal_vclk_ip_htu_g3d[] = {
	GOUT_BLK_G3D_UID_HTU_G3D_IPCLKPORT_I_CLK,
	GOUT_BLK_G3D_UID_HTU_G3D_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_usb31drd[] = {
	GOUT_BLK_HSI0_UID_USB31DRD_IPCLKPORT_I_USBDPPHY_REF_SOC_PLL,
	GOUT_BLK_HSI0_UID_USB31DRD_IPCLKPORT_I_USB31DRD_REF_CLK_40,
	GOUT_BLK_HSI0_UID_USB31DRD_IPCLKPORT_ACLK_PHYCTRL,
	GOUT_BLK_HSI0_UID_USB31DRD_IPCLKPORT_I_USBDPPHY_SCL_APB_PCLK,
	GOUT_BLK_HSI0_UID_USB31DRD_IPCLKPORT_I_USBPCS_APB_CLK,
	GOUT_BLK_HSI0_UID_USB31DRD_IPCLKPORT_BUS_CLK_EARLY,
	GOUT_BLK_HSI0_UID_USB31DRD_IPCLKPORT_USBDPPHY_I_ACLK,
	GOUT_BLK_HSI0_UID_USB31DRD_IPCLKPORT_USBDPPHY_UDBG_I_APB_PCLK,
};
enum clk_id cmucal_vclk_ip_dp_link[] = {
	GOUT_BLK_HSI0_UID_DP_LINK_IPCLKPORT_I_DP_GTC_CLK,
	GOUT_BLK_HSI0_UID_DP_LINK_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_hsi0_bus1[] = {
	GOUT_BLK_HSI0_UID_PPMU_HSI0_BUS1_IPCLKPORT_ACLK,
	GOUT_BLK_HSI0_UID_PPMU_HSI0_BUS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d_hsi0[] = {
	GOUT_BLK_HSI0_UID_LHS_ACEL_D_HSI0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_hsi0[] = {
	GOUT_BLK_HSI0_UID_VGEN_LITE_HSI0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_hsi0[] = {
	GOUT_BLK_HSI0_UID_D_TZPC_HSI0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_hsi0[] = {
	GOUT_BLK_HSI0_UID_LHM_AXI_P_HSI0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_usb[] = {
	GOUT_BLK_HSI0_UID_SYSMMU_USB_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysreg_hsi0[] = {
	GOUT_BLK_HSI0_UID_SYSREG_HSI0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_hsi0_cmu_hsi0[] = {
	CLK_BLK_HSI0_UID_HSI0_CMU_HSI0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d1_hsi0[] = {
	GOUT_BLK_HSI0_UID_XIU_D1_HSI0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_audhsi0[] = {
	GOUT_BLK_HSI0_UID_LHM_AXI_D_AUDHSI0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_hsi0aud[] = {
	GOUT_BLK_HSI0_UID_LHS_AXI_D_HSI0AUD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d0_hsi0[] = {
	GOUT_BLK_HSI0_UID_XIU_D0_HSI0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_hsi1[] = {
	GOUT_BLK_HSI1_UID_SYSMMU_HSI1_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_hsi1_cmu_hsi1[] = {
	GOUT_BLK_HSI1_UID_HSI1_CMU_HSI1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pcie_gen2[] = {
	GOUT_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_IEEE1500_WRAPPER_FOR_PCIEG2_PHY_X1_INST_0_I_SCL_APB_PCLK,
	CLK_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_PHY_REFCLK_IN,
	GOUT_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_SLV_ACLK,
	GOUT_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_DBI_ACLK,
	GOUT_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_PCIE_SUB_CTRL_INST_0_I_DRIVER_APB_CLK,
	GOUT_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_PIPE2_DIGITAL_X1_WRAP_INST_0_I_APB_PCLK_SCL,
	GOUT_BLK_HSI1_UID_PCIE_GEN2_IPCLKPORT_MSTR_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_hsi1[] = {
	GOUT_BLK_HSI1_UID_SYSREG_HSI1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_gpio_hsi1[] = {
	GOUT_BLK_HSI1_UID_GPIO_HSI1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d_hsi1[] = {
	GOUT_BLK_HSI1_UID_LHS_ACEL_D_HSI1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_hsi1[] = {
	GOUT_BLK_HSI1_UID_LHM_AXI_P_HSI1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_hsi1[] = {
	GOUT_BLK_HSI1_UID_XIU_D_HSI1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_p_hsi1[] = {
	GOUT_BLK_HSI1_UID_XIU_P_HSI1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_hsi1[] = {
	GOUT_BLK_HSI1_UID_PPMU_HSI1_IPCLKPORT_ACLK,
	GOUT_BLK_HSI1_UID_PPMU_HSI1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_hsi1[] = {
	GOUT_BLK_HSI1_UID_VGEN_LITE_HSI1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_pcie_ia_gen2[] = {
	GOUT_BLK_HSI1_UID_PCIE_IA_GEN2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_hsi1[] = {
	GOUT_BLK_HSI1_UID_D_TZPC_HSI1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pcie_gen4_0[] = {
	GOUT_BLK_HSI1_UID_PCIE_GEN4_0_IPCLKPORT_PCIE_003_PCIE_SUB_CTRL_INST_0_I_DRIVER_APB_CLK,
	CLK_BLK_HSI1_UID_PCIE_GEN4_0_IPCLKPORT_PCIE_003_PCIE_SUB_CTRL_INST_0_PHY_REFCLK_IN,
	GOUT_BLK_HSI1_UID_PCIE_GEN4_0_IPCLKPORT_PCIE_003_G4X2_DWC_PCIE_CTL_INST_0_DBI_ACLK_UG,
	GOUT_BLK_HSI1_UID_PCIE_GEN4_0_IPCLKPORT_PCIE_003_G4X2_DWC_PCIE_CTL_INST_0_MSTR_ACLK_UG,
	GOUT_BLK_HSI1_UID_PCIE_GEN4_0_IPCLKPORT_PCIE_003_G4X2_DWC_PCIE_CTL_INST_0_SLV_ACLK_UG,
	GOUT_BLK_HSI1_UID_PCIE_GEN4_0_IPCLKPORT_PCS_PMA_INST_0_PIPE_PAL_PCIE_INST_0_I_APB_PCLK,
	GOUT_BLK_HSI1_UID_PCIE_GEN4_0_IPCLKPORT_PCS_PMA_INST_0_SF_PCIEPHY210X2_LN05LPE_QCH_TM_WRAPPER_INST_0_I_APB_PCLK,
	GOUT_BLK_HSI1_UID_PCIE_GEN4_0_IPCLKPORT_PCS_PMA_INST_0_PHY_UDBG_I_APB_PCLK,
};
enum clk_id cmucal_vclk_ip_pcie_ia_gen4_0[] = {
	GOUT_BLK_HSI1_UID_PCIE_IA_GEN4_0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ufs_embd[] = {
	GOUT_BLK_HSI1_UID_UFS_EMBD_IPCLKPORT_I_ACLK,
	GOUT_BLK_HSI1_UID_UFS_EMBD_IPCLKPORT_I_FMP_CLK,
	GOUT_BLK_HSI1_UID_UFS_EMBD_IPCLKPORT_I_CLK_UNIPRO,
};
enum clk_id cmucal_vclk_ip_mmc_card[] = {
	GOUT_BLK_HSI1_UID_MMC_CARD_IPCLKPORT_I_ACLK,
	GOUT_BLK_HSI1_UID_MMC_CARD_IPCLKPORT_SDCLKIN,
};
enum clk_id cmucal_vclk_ip_itp_cmu_itp[] = {
	CLK_BLK_ITP_UID_ITP_CMU_ITP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_itp[] = {
	GOUT_BLK_ITP_UID_SYSREG_ITP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_itp[] = {
	GOUT_BLK_ITP_UID_D_TZPC_ITP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_ctl_itpdns[] = {
	GOUT_BLK_ITP_UID_LHS_AST_CTL_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_itp[] = {
	GOUT_BLK_ITP_UID_ITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf4_itpdns[] = {
	GOUT_BLK_ITP_UID_LHS_AST_OTF4_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_ctl_dnsitp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_CTL_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf4_dnsitp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_OTF4_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf3_dnsitp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_OTF3_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf5_dnsitp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_OTF5_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf6_dnsitp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_OTF6_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf7_dnsitp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_OTF7_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf8_dnsitp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_OTF8_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf9_dnsitp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_OTF9_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf2_dnsitp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_OTF2_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_itp[] = {
	GOUT_BLK_ITP_UID_LHM_AXI_P_ITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf0_dnsitp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_OTF0_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf1_dnsitp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_OTF1_DNSITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf1_itpdns[] = {
	GOUT_BLK_ITP_UID_LHS_AST_OTF1_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf2_itpdns[] = {
	GOUT_BLK_ITP_UID_LHS_AST_OTF2_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf3_itpdns[] = {
	GOUT_BLK_ITP_UID_LHS_AST_OTF3_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_itp0[] = {
	GOUT_BLK_ITP_UID_AD_APB_ITP0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf0_itpdns[] = {
	GOUT_BLK_ITP_UID_LHS_AST_OTF0_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_itpdns[] = {
	GOUT_BLK_ITP_UID_LHS_AXI_P_ITPDNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf_mcfp1itp[] = {
	GOUT_BLK_ITP_UID_LHM_AST_OTF_MCFP1ITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf_itpmcsc[] = {
	GOUT_BLK_ITP_UID_LHS_AST_OTF_ITPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lme_cmu_lme[] = {
	CLK_BLK_LME_UID_LME_CMU_LME_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d_lme[] = {
	GOUT_BLK_LME_UID_SYSMMU_D_LME_IPCLKPORT_CLK_S2,
	GOUT_BLK_LME_UID_SYSMMU_D_LME_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_sysreg_lme[] = {
	GOUT_BLK_LME_UID_SYSREG_LME_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_lme[] = {
	GOUT_BLK_LME_UID_D_TZPC_LME_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lme[] = {
	GOUT_BLK_LME_UID_LME_IPCLKPORT_C2CLK,
	GOUT_BLK_LME_UID_LME_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_lme[] = {
	GOUT_BLK_LME_UID_PPMU_LME_IPCLKPORT_PCLK,
	GOUT_BLK_LME_UID_PPMU_LME_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_lme[] = {
	GOUT_BLK_LME_UID_LHS_AXI_D_LME_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_lme[] = {
	GOUT_BLK_LME_UID_AD_APB_LME_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_vgen_lite_lme[] = {
	GOUT_BLK_LME_UID_VGEN_LITE_LME_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_lme[] = {
	GOUT_BLK_LME_UID_LHM_AXI_P_LME_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_lme[] = {
	GOUT_BLK_LME_UID_XIU_D_LME_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_m2m_cmu_m2m[] = {
	CLK_BLK_M2M_UID_M2M_CMU_M2M_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d_m2m[] = {
	GOUT_BLK_M2M_UID_LHS_ACEL_D_M2M_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_as_apb_m2m[] = {
	GOUT_BLK_M2M_UID_AS_APB_M2M_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_sysmmu_d_m2m[] = {
	GOUT_BLK_M2M_UID_SYSMMU_D_M2M_IPCLKPORT_CLK_S2,
	GOUT_BLK_M2M_UID_SYSMMU_D_M2M_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_xiu_d_m2m[] = {
	GOUT_BLK_M2M_UID_XIU_D_M2M_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_jpeg0[] = {
	GOUT_BLK_M2M_UID_QE_JPEG0_IPCLKPORT_PCLK,
	GOUT_BLK_M2M_UID_QE_JPEG0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_m2m[] = {
	GOUT_BLK_M2M_UID_D_TZPC_M2M_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_m2m[] = {
	GOUT_BLK_M2M_UID_M2M_IPCLKPORT_ACLK,
	GOUT_BLK_M2M_UID_M2M_IPCLKPORT_ACLK_VOTF,
	GOUT_BLK_M2M_UID_M2M_IPCLKPORT_ACLK_VOTF,
	GOUT_BLK_M2M_UID_M2M_IPCLKPORT_ACLK_2X1,
	GOUT_BLK_M2M_UID_M2M_IPCLKPORT_ACLK_2X1,
};
enum clk_id cmucal_vclk_ip_qe_jsqz[] = {
	GOUT_BLK_M2M_UID_QE_JSQZ_IPCLKPORT_PCLK,
	GOUT_BLK_M2M_UID_QE_JSQZ_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_m2m[] = {
	GOUT_BLK_M2M_UID_QE_M2M_IPCLKPORT_PCLK,
	GOUT_BLK_M2M_UID_QE_M2M_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_m2m[] = {
	GOUT_BLK_M2M_UID_LHM_AXI_P_M2M_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_qe_astc[] = {
	GOUT_BLK_M2M_UID_QE_ASTC_IPCLKPORT_PCLK,
	GOUT_BLK_M2M_UID_QE_ASTC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_m2m[] = {
	GOUT_BLK_M2M_UID_VGEN_LITE_M2M_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d_m2m[] = {
	GOUT_BLK_M2M_UID_PPMU_D_M2M_IPCLKPORT_PCLK,
	GOUT_BLK_M2M_UID_PPMU_D_M2M_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_m2m[] = {
	GOUT_BLK_M2M_UID_SYSREG_M2M_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_astc[] = {
	GOUT_BLK_M2M_UID_ASTC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_jpeg0[] = {
	GOUT_BLK_M2M_UID_JPEG0_IPCLKPORT_I_SMFC_CLK,
};
enum clk_id cmucal_vclk_ip_jsqz[] = {
	GOUT_BLK_M2M_UID_JSQZ_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_jpeg1[] = {
	GOUT_BLK_M2M_UID_JPEG1_IPCLKPORT_I_SMFC_CLK,
};
enum clk_id cmucal_vclk_ip_qe_jpeg1[] = {
	GOUT_BLK_M2M_UID_QE_JPEG1_IPCLKPORT_ACLK,
	GOUT_BLK_M2M_UID_QE_JPEG1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mcfp0_cmu_mcfp0[] = {
	CLK_BLK_MCFP0_UID_MCFP0_CMU_MCFP0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_LHS_AXI_D0_MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_LHM_AXI_P_MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf0_mcfp0mcfp1[] = {
	GOUT_BLK_MCFP0_UID_LHS_AST_OTF0_MCFP0MCFP1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf1_mcfp0mcfp1[] = {
	GOUT_BLK_MCFP0_UID_LHS_AST_OTF1_MCFP0MCFP1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf0_mcfp1mcfp0[] = {
	GOUT_BLK_MCFP0_UID_LHM_AST_OTF0_MCFP1MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_SYSREG_MCFP0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_D_TZPC_MCFP0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_VGEN_LITE_MCFP0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d0_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_PPMU_D0_MCFP0_IPCLKPORT_PCLK,
	GOUT_BLK_MCFP0_UID_PPMU_D0_MCFP0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d0_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_SYSMMU_D0_MCFP0_IPCLKPORT_CLK_S1,
	GOUT_BLK_MCFP0_UID_SYSMMU_D0_MCFP0_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_apb_async_mcfp0_0[] = {
	GOUT_BLK_MCFP0_UID_APB_ASYNC_MCFP0_0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_MCFP0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_LHS_AXI_D1_MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf1_mcfp1mcfp0[] = {
	GOUT_BLK_MCFP0_UID_LHM_AST_OTF1_MCFP1MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d1_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_PPMU_D1_MCFP0_IPCLKPORT_PCLK,
	GOUT_BLK_MCFP0_UID_PPMU_D1_MCFP0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_SYSMMU_D1_MCFP0_IPCLKPORT_CLK_S1,
	GOUT_BLK_MCFP0_UID_SYSMMU_D1_MCFP0_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_mcfp0mcfp1[] = {
	GOUT_BLK_MCFP0_UID_LHS_AXI_P_MCFP0MCFP1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf2_mcfp1mcfp0[] = {
	GOUT_BLK_MCFP0_UID_LHM_AST_OTF2_MCFP1MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf3_mcfp1mcfp0[] = {
	GOUT_BLK_MCFP0_UID_LHM_AST_OTF3_MCFP1MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_qe_d0_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_QE_D0_MCFP0_IPCLKPORT_ACLK,
	GOUT_BLK_MCFP0_UID_QE_D0_MCFP0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_d1_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_QE_D1_MCFP0_IPCLKPORT_ACLK,
	GOUT_BLK_MCFP0_UID_QE_D1_MCFP0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_d2_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_QE_D2_MCFP0_IPCLKPORT_ACLK,
	GOUT_BLK_MCFP0_UID_QE_D2_MCFP0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_d3_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_QE_D3_MCFP0_IPCLKPORT_ACLK,
	GOUT_BLK_MCFP0_UID_QE_D3_MCFP0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d0_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_XIU_D0_MCFP0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d2_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_PPMU_D2_MCFP0_IPCLKPORT_ACLK,
	GOUT_BLK_MCFP0_UID_PPMU_D2_MCFP0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d3_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_PPMU_D3_MCFP0_IPCLKPORT_ACLK,
	GOUT_BLK_MCFP0_UID_PPMU_D3_MCFP0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d2_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_SYSMMU_D2_MCFP0_IPCLKPORT_CLK_S1,
	GOUT_BLK_MCFP0_UID_SYSMMU_D2_MCFP0_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_d3_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_SYSMMU_D3_MCFP0_IPCLKPORT_CLK_S1,
	GOUT_BLK_MCFP0_UID_SYSMMU_D3_MCFP0_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d2_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_LHS_AXI_D2_MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d3_mcfp0[] = {
	GOUT_BLK_MCFP0_UID_LHS_AXI_D3_MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_ctl_mcfp1mcfp0[] = {
	GOUT_BLK_MCFP0_UID_LHM_AST_CTL_MCFP1MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_ctl_mcfp0mcfp1[] = {
	GOUT_BLK_MCFP0_UID_LHS_AST_CTL_MCFP0MCFP1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mcfp1_cmu_mcfp1[] = {
	CLK_BLK_MCFP1_UID_MCFP1_CMU_MCFP1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_mcfp1_0[] = {
	GOUT_BLK_MCFP1_UID_AD_APB_MCFP1_0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_qe_d2_orbmch[] = {
	GOUT_BLK_MCFP1_UID_QE_D2_ORBMCH_IPCLKPORT_PCLK,
	GOUT_BLK_MCFP1_UID_QE_D2_ORBMCH_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_d0_mcfp1[] = {
	GOUT_BLK_MCFP1_UID_VGEN_LITE_D0_MCFP1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_orbmch[] = {
	GOUT_BLK_MCFP1_UID_PPMU_ORBMCH_IPCLKPORT_PCLK,
	GOUT_BLK_MCFP1_UID_PPMU_ORBMCH_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_d0_orbmch[] = {
	GOUT_BLK_MCFP1_UID_QE_D0_ORBMCH_IPCLKPORT_PCLK,
	GOUT_BLK_MCFP1_UID_QE_D0_ORBMCH_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf1_mcfp0mcfp1[] = {
	GOUT_BLK_MCFP1_UID_LHM_AST_OTF1_MCFP0MCFP1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_mcfp0mcfp1[] = {
	GOUT_BLK_MCFP1_UID_LHM_AXI_P_MCFP0MCFP1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mcfp1[] = {
	GOUT_BLK_MCFP1_UID_MCFP1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_mcfp1[] = {
	GOUT_BLK_MCFP1_UID_LHS_AXI_D_MCFP1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf_mcfp1itp[] = {
	GOUT_BLK_MCFP1_UID_LHS_AST_OTF_MCFP1ITP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_vo_taamcfp1[] = {
	GOUT_BLK_MCFP1_UID_LHM_AST_VO_TAAMCFP1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf_mcfp1dns[] = {
	GOUT_BLK_MCFP1_UID_LHS_AST_OTF_MCFP1DNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_orbmch0[] = {
	GOUT_BLK_MCFP1_UID_ORBMCH0_IPCLKPORT_C2CLK,
	GOUT_BLK_MCFP1_UID_ORBMCH0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mcfp1[] = {
	GOUT_BLK_MCFP1_UID_SYSREG_MCFP1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_vo_mcfp1taa[] = {
	GOUT_BLK_MCFP1_UID_LHS_AST_VO_MCFP1TAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d_mcfp1[] = {
	GOUT_BLK_MCFP1_UID_SYSMMU_D_MCFP1_IPCLKPORT_CLK_S2,
	GOUT_BLK_MCFP1_UID_SYSMMU_D_MCFP1_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mcfp1[] = {
	GOUT_BLK_MCFP1_UID_D_TZPC_MCFP1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_mcfp1[] = {
	GOUT_BLK_MCFP1_UID_XIU_D_MCFP1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_d1_orbmch[] = {
	GOUT_BLK_MCFP1_UID_QE_D1_ORBMCH_IPCLKPORT_PCLK,
	GOUT_BLK_MCFP1_UID_QE_D1_ORBMCH_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf0_mcfp1mcfp0[] = {
	GOUT_BLK_MCFP1_UID_LHS_AST_OTF0_MCFP1MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf0_mcfp0mcfp1[] = {
	GOUT_BLK_MCFP1_UID_LHM_AST_OTF0_MCFP0MCFP1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf1_mcfp1mcfp0[] = {
	GOUT_BLK_MCFP1_UID_LHS_AST_OTF1_MCFP1MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_orbmch0[] = {
	GOUT_BLK_MCFP1_UID_AD_APB_ORBMCH0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf2_mcfp1mcfp0[] = {
	GOUT_BLK_MCFP1_UID_LHS_AST_OTF2_MCFP1MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf3_mcfp1mcfp0[] = {
	GOUT_BLK_MCFP1_UID_LHS_AST_OTF3_MCFP1MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_qe_d3_orbmch[] = {
	GOUT_BLK_MCFP1_UID_QE_D3_ORBMCH_IPCLKPORT_ACLK,
	GOUT_BLK_MCFP1_UID_QE_D3_ORBMCH_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_ctl_mcfp0mcfp1[] = {
	GOUT_BLK_MCFP1_UID_LHM_AST_CTL_MCFP0MCFP1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_ctl_mcfp1mcfp0[] = {
	GOUT_BLK_MCFP1_UID_LHS_AST_CTL_MCFP1MCFP0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_orbmch1[] = {
	GOUT_BLK_MCFP1_UID_ORBMCH1_IPCLKPORT_ACLK,
	GOUT_BLK_MCFP1_UID_ORBMCH1_IPCLKPORT_C2CLK,
};
enum clk_id cmucal_vclk_ip_qe_d4_orbmch[] = {
	GOUT_BLK_MCFP1_UID_QE_D4_ORBMCH_IPCLKPORT_ACLK,
	GOUT_BLK_MCFP1_UID_QE_D4_ORBMCH_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_d5_orbmch[] = {
	GOUT_BLK_MCFP1_UID_QE_D5_ORBMCH_IPCLKPORT_ACLK,
	GOUT_BLK_MCFP1_UID_QE_D5_ORBMCH_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_d1_mcfp1[] = {
	GOUT_BLK_MCFP1_UID_VGEN_LITE_D1_MCFP1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_mcsc_cmu_mcsc[] = {
	CLK_BLK_MCSC_UID_MCSC_CMU_MCSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_mcsc[] = {
	GOUT_BLK_MCSC_UID_LHM_AXI_P_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mcsc[] = {
	GOUT_BLK_MCSC_UID_SYSREG_MCSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_mcsc[] = {
	GOUT_BLK_MCSC_UID_MCSC_IPCLKPORT_CLK,
	GOUT_BLK_MCSC_UID_MCSC_IPCLKPORT_C2W_CLK,
	GOUT_BLK_MCSC_UID_MCSC_IPCLKPORT_C2R_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_d0_mcsc[] = {
	GOUT_BLK_MCSC_UID_PPMU_D0_MCSC_IPCLKPORT_PCLK,
	GOUT_BLK_MCSC_UID_PPMU_D0_MCSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mcsc[] = {
	GOUT_BLK_MCSC_UID_D_TZPC_MCSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_d0_mcsc[] = {
	GOUT_BLK_MCSC_UID_VGEN_LITE_D0_MCSC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_mcsc_0[] = {
	GOUT_BLK_MCSC_UID_AD_APB_MCSC_0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ppmu_d1_mcsc[] = {
	GOUT_BLK_MCSC_UID_PPMU_D1_MCSC_IPCLKPORT_PCLK,
	GOUT_BLK_MCSC_UID_PPMU_D1_MCSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_gdc[] = {
	GOUT_BLK_MCSC_UID_GDC_IPCLKPORT_CLK,
	GOUT_BLK_MCSC_UID_GDC_IPCLKPORT_C2CLK_M,
	GOUT_BLK_MCSC_UID_GDC_IPCLKPORT_C2CLK_S,
};
enum clk_id cmucal_vclk_ip_ad_apb_gdc[] = {
	GOUT_BLK_MCSC_UID_AD_APB_GDC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_ppmu_d2_mcsc[] = {
	GOUT_BLK_MCSC_UID_PPMU_D2_MCSC_IPCLKPORT_PCLK,
	GOUT_BLK_MCSC_UID_PPMU_D2_MCSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d2_mcsc[] = {
	GOUT_BLK_MCSC_UID_SYSMMU_D2_MCSC_IPCLKPORT_CLK_S1,
	GOUT_BLK_MCSC_UID_SYSMMU_D2_MCSC_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_d0_mcsc[] = {
	GOUT_BLK_MCSC_UID_SYSMMU_D0_MCSC_IPCLKPORT_CLK_S1,
	GOUT_BLK_MCSC_UID_SYSMMU_D0_MCSC_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d2_mcsc[] = {
	GOUT_BLK_MCSC_UID_LHS_AXI_D2_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d0_mcsc[] = {
	GOUT_BLK_MCSC_UID_LHS_ACEL_D0_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf_yuvppmcsc[] = {
	GOUT_BLK_MCSC_UID_LHM_AST_OTF_YUVPPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_hpm_mcsc[] = {
	CLK_BLK_MCSC_UID_HPM_MCSC_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_busif_add_mcsc[] = {
	GOUT_BLK_MCSC_UID_BUSIF_ADD_MCSC_IPCLKPORT_PCLK,
	GOUT_BLK_MCSC_UID_BUSIF_ADD_MCSC_IPCLKPORT_CLK_CORE,
	GOUT_BLK_MCSC_UID_BUSIF_ADD_MCSC_IPCLKPORT_CLK_CORE,
};
enum clk_id cmucal_vclk_ip_busif_ddd_mcsc[] = {
	GOUT_BLK_MCSC_UID_BUSIF_DDD_MCSC_IPCLKPORT_PCLK,
	GOUT_BLK_MCSC_UID_BUSIF_DDD_MCSC_IPCLKPORT_CK_IN,
};
enum clk_id cmucal_vclk_ip_add_mcsc[] = {
	CLK_BLK_MCSC_UID_ADD_MCSC_IPCLKPORT_CH_CLK,
	CLK_BLK_MCSC_UID_ADD_MCSC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_busif_hpm_mcsc[] = {
	GOUT_BLK_MCSC_UID_BUSIF_HPM_MCSC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d2_mcsc[] = {
	GOUT_BLK_MCSC_UID_XIU_D2_MCSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_d1_mcsc[] = {
	GOUT_BLK_MCSC_UID_VGEN_LITE_D1_MCSC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d0_mcsc[] = {
	GOUT_BLK_MCSC_UID_XIU_D0_MCSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ddd_mcsc[] = {
	CLK_BLK_MCSC_UID_DDD_MCSC_IPCLKPORT_CK_IN,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_yuvppmcsc[] = {
	GOUT_BLK_MCSC_UID_LHM_AXI_D_YUVPPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_d1_mcsc[] = {
	GOUT_BLK_MCSC_UID_XIU_D1_MCSC_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mcsc[] = {
	GOUT_BLK_MCSC_UID_LHS_AXI_D1_MCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_mcsc[] = {
	GOUT_BLK_MCSC_UID_SYSMMU_D1_MCSC_IPCLKPORT_CLK_S1,
	GOUT_BLK_MCSC_UID_SYSMMU_D1_MCSC_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf_itpmcsc[] = {
	GOUT_BLK_MCSC_UID_LHM_AST_OTF_ITPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mfc0_cmu_mfc0[] = {
	CLK_BLK_MFC0_UID_MFC0_CMU_MFC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_as_apb_mfc0[] = {
	GOUT_BLK_MFC0_UID_AS_APB_MFC0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_sysreg_mfc0[] = {
	GOUT_BLK_MFC0_UID_SYSREG_MFC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_mfc0[] = {
	GOUT_BLK_MFC0_UID_LHS_AXI_D0_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mfc0[] = {
	GOUT_BLK_MFC0_UID_LHS_AXI_D1_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_mfc0[] = {
	GOUT_BLK_MFC0_UID_LHM_AXI_P_MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_mfc0d0[] = {
	GOUT_BLK_MFC0_UID_SYSMMU_MFC0D0_IPCLKPORT_CLK_S1,
	GOUT_BLK_MFC0_UID_SYSMMU_MFC0D0_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_mfc0d1[] = {
	GOUT_BLK_MFC0_UID_SYSMMU_MFC0D1_IPCLKPORT_CLK_S1,
	GOUT_BLK_MFC0_UID_SYSMMU_MFC0D1_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_ppmu_mfc0d0[] = {
	GOUT_BLK_MFC0_UID_PPMU_MFC0D0_IPCLKPORT_ACLK,
	GOUT_BLK_MFC0_UID_PPMU_MFC0D0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mfc0d1[] = {
	GOUT_BLK_MFC0_UID_PPMU_MFC0D1_IPCLKPORT_ACLK,
	GOUT_BLK_MFC0_UID_PPMU_MFC0D1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_as_axi_wfd[] = {
	GOUT_BLK_MFC0_UID_AS_AXI_WFD_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_ppmu_wfd[] = {
	GOUT_BLK_MFC0_UID_PPMU_WFD_IPCLKPORT_PCLK,
	GOUT_BLK_MFC0_UID_PPMU_WFD_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_mfc0[] = {
	GOUT_BLK_MFC0_UID_XIU_D_MFC0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_vgen_mfc0[] = {
	GOUT_BLK_MFC0_UID_VGEN_MFC0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_mfc0[] = {
	GOUT_BLK_MFC0_UID_MFC0_IPCLKPORT_ACLK,
	GOUT_BLK_MFC0_UID_MFC0_IPCLKPORT_C2CLK,
};
enum clk_id cmucal_vclk_ip_wfd[] = {
	GOUT_BLK_MFC0_UID_WFD_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lh_atb_mfc0[] = {
	GOUT_BLK_MFC0_UID_LH_ATB_MFC0_IPCLKPORT_I_CLK_MI,
	GOUT_BLK_MFC0_UID_LH_ATB_MFC0_IPCLKPORT_I_CLK_SI,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mfc0[] = {
	GOUT_BLK_MFC0_UID_D_TZPC_MFC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_as_apb_wfd_ns[] = {
	GOUT_BLK_MFC0_UID_AS_APB_WFD_NS_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf0_mfc1mfc0[] = {
	GOUT_BLK_MFC0_UID_LHM_AST_OTF0_MFC1MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf1_mfc1mfc0[] = {
	GOUT_BLK_MFC0_UID_LHM_AST_OTF1_MFC1MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf2_mfc1mfc0[] = {
	GOUT_BLK_MFC0_UID_LHM_AST_OTF2_MFC1MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf3_mfc1mfc0[] = {
	GOUT_BLK_MFC0_UID_LHM_AST_OTF3_MFC1MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf0_mfc0mfc1[] = {
	GOUT_BLK_MFC0_UID_LHS_AST_OTF0_MFC0MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf1_mfc0mfc1[] = {
	GOUT_BLK_MFC0_UID_LHS_AST_OTF1_MFC0MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf2_mfc0mfc1[] = {
	GOUT_BLK_MFC0_UID_LHS_AST_OTF2_MFC0MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf3_mfc0mfc1[] = {
	GOUT_BLK_MFC0_UID_LHS_AST_OTF3_MFC0MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ads_apb_mfc0mfc1[] = {
	GOUT_BLK_MFC0_UID_ADS_APB_MFC0MFC1_IPCLKPORT_PCLKS,
};
enum clk_id cmucal_vclk_ip_mfc1_cmu_mfc1[] = {
	CLK_BLK_MFC1_UID_MFC1_CMU_MFC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_as_apb_mfc1[] = {
	GOUT_BLK_MFC1_UID_AS_APB_MFC1_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_mfc1[] = {
	GOUT_BLK_MFC1_UID_MFC1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mfc1d1[] = {
	GOUT_BLK_MFC1_UID_PPMU_MFC1D1_IPCLKPORT_PCLK,
	GOUT_BLK_MFC1_UID_PPMU_MFC1D1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_mfc1d0[] = {
	GOUT_BLK_MFC1_UID_PPMU_MFC1D0_IPCLKPORT_PCLK,
	GOUT_BLK_MFC1_UID_PPMU_MFC1D0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_mfc1[] = {
	GOUT_BLK_MFC1_UID_LHS_AXI_D0_MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_mfc1[] = {
	GOUT_BLK_MFC1_UID_VGEN_MFC1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_mfc1[] = {
	GOUT_BLK_MFC1_UID_LHM_AXI_P_MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_mfc1d0[] = {
	GOUT_BLK_MFC1_UID_SYSMMU_MFC1D0_IPCLKPORT_CLK_S2,
	GOUT_BLK_MFC1_UID_SYSMMU_MFC1D0_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_sysmmu_mfc1d1[] = {
	GOUT_BLK_MFC1_UID_SYSMMU_MFC1D1_IPCLKPORT_CLK_S2,
	GOUT_BLK_MFC1_UID_SYSMMU_MFC1D1_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_sysreg_mfc1[] = {
	GOUT_BLK_MFC1_UID_SYSREG_MFC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mfc1[] = {
	GOUT_BLK_MFC1_UID_D_TZPC_MFC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_mfc1[] = {
	GOUT_BLK_MFC1_UID_LHS_AXI_D1_MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_adm_apb_mfc0mfc1[] = {
	GOUT_BLK_MFC1_UID_ADM_APB_MFC0MFC1_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf0_mfc0mfc1[] = {
	GOUT_BLK_MFC1_UID_LHM_AST_OTF0_MFC0MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf1_mfc0mfc1[] = {
	GOUT_BLK_MFC1_UID_LHM_AST_OTF1_MFC0MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf2_mfc0mfc1[] = {
	GOUT_BLK_MFC1_UID_LHM_AST_OTF2_MFC0MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf3_mfc0mfc1[] = {
	GOUT_BLK_MFC1_UID_LHM_AST_OTF3_MFC0MFC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf0_mfc1mfc0[] = {
	GOUT_BLK_MFC1_UID_LHS_AST_OTF0_MFC1MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf1_mfc1mfc0[] = {
	GOUT_BLK_MFC1_UID_LHS_AST_OTF1_MFC1MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf2_mfc1mfc0[] = {
	GOUT_BLK_MFC1_UID_LHS_AST_OTF2_MFC1MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf3_mfc1mfc0[] = {
	GOUT_BLK_MFC1_UID_LHS_AST_OTF3_MFC1MFC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mif_cmu_mif[] = {
	CLK_BLK_MIF_UID_MIF_CMU_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ddrphy[] = {
	GOUT_BLK_MIF_UID_DDRPHY_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_mif[] = {
	GOUT_BLK_MIF_UID_SYSREG_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_mif[] = {
	GOUT_BLK_MIF_UID_LHM_AXI_P_MIF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_apbbr_ddrphy[] = {
	GOUT_BLK_MIF_UID_APBBR_DDRPHY_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_apbbr_dmc[] = {
	GOUT_BLK_MIF_UID_APBBR_DMC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmc[] = {
	GOUT_BLK_MIF_UID_DMC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qch_adapter_ppc_debug[] = {
	GOUT_BLK_MIF_UID_QCH_ADAPTER_PPC_DEBUG_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_mif[] = {
	GOUT_BLK_MIF_UID_D_TZPC_MIF_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ppc_debug[] = {
	CLK_BLK_MIF_UID_PPC_DEBUG_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_npu_cmu_npu[] = {
	CLK_BLK_NPU_UID_NPU_CMU_NPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ip_npucore[] = {
	GOUT_BLK_NPU_UID_IP_NPUCORE_IPCLKPORT_I_PCLK,
	GOUT_BLK_NPU_UID_IP_NPUCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AXI_D1_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_rq_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AXI_D_RQ_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AXI_P_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AXI_D0_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_npu[] = {
	GOUT_BLK_NPU_UID_D_TZPC_NPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_cmdq_npu[] = {
	GOUT_BLK_NPU_UID_LHS_AXI_D_CMDQ_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_npu[] = {
	GOUT_BLK_NPU_UID_SYSREG_NPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_ctrl_npu[] = {
	GOUT_BLK_NPU_UID_LHM_AXI_D_CTRL_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_npu01_cmu_npu[] = {
	CLK_BLK_NPU01_UID_NPU_CMU_NPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ip_npu01core[] = {
	GOUT_BLK_NPU01_UID_IP_NPUCORE_IPCLKPORT_I_PCLK,
	GOUT_BLK_NPU01_UID_IP_NPUCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_npu01[] = {
	GOUT_BLK_NPU01_UID_LHM_AXI_D1_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_rq_npu01[] = {
	GOUT_BLK_NPU01_UID_LHS_AXI_D_RQ_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_npu01[] = {
	GOUT_BLK_NPU01_UID_LHM_AXI_P_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_npu01[] = {
	GOUT_BLK_NPU01_UID_LHM_AXI_D0_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_npu01[] = {
	GOUT_BLK_NPU01_UID_D_TZPC_NPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_cmdq_npu01[] = {
	GOUT_BLK_NPU01_UID_LHS_AXI_D_CMDQ_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_npu01[] = {
	GOUT_BLK_NPU01_UID_SYSREG_NPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_ctrl_npu01[] = {
	GOUT_BLK_NPU01_UID_LHM_AXI_D_CTRL_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_npu10_cmu_npu[] = {
	CLK_BLK_NPU10_UID_NPU_CMU_NPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ip_npu10core[] = {
	GOUT_BLK_NPU10_UID_IP_NPUCORE_IPCLKPORT_I_PCLK,
	GOUT_BLK_NPU10_UID_IP_NPUCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d1_npu10[] = {
	GOUT_BLK_NPU10_UID_LHM_AXI_D1_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_rq_npu10[] = {
	GOUT_BLK_NPU10_UID_LHS_AXI_D_RQ_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_npu10[] = {
	GOUT_BLK_NPU10_UID_LHM_AXI_P_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d0_npu10[] = {
	GOUT_BLK_NPU10_UID_LHM_AXI_D0_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_npu10[] = {
	GOUT_BLK_NPU10_UID_D_TZPC_NPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_cmdq_npu10[] = {
	GOUT_BLK_NPU10_UID_LHS_AXI_D_CMDQ_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_npu10[] = {
	GOUT_BLK_NPU10_UID_SYSREG_NPU_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_ctrl_npu10[] = {
	GOUT_BLK_NPU10_UID_LHM_AXI_D_CTRL_NPU_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_npus_cmu_npus[] = {
	CLK_BLK_NPUS_UID_NPUS_CMU_NPUS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d0_npus[] = {
	GOUT_BLK_NPUS_UID_SYSMMU_D0_NPUS_IPCLKPORT_CLK_S2,
	GOUT_BLK_NPUS_UID_SYSMMU_D0_NPUS_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_sysmmu_d2_npus[] = {
	GOUT_BLK_NPUS_UID_SYSMMU_D2_NPUS_IPCLKPORT_CLK_S2,
	GOUT_BLK_NPUS_UID_SYSMMU_D2_NPUS_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_sysmmu_d1_npus[] = {
	GOUT_BLK_NPUS_UID_SYSMMU_D1_NPUS_IPCLKPORT_CLK_S2,
	GOUT_BLK_NPUS_UID_SYSMMU_D1_NPUS_IPCLKPORT_CLK_S1,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_npu10[] = {
	GOUT_BLK_NPUS_UID_LHS_AXI_D0_NPU10_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_npus[] = {
	GOUT_BLK_NPUS_UID_LHS_AXI_D0_NPUS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_ctrl_npu10[] = {
	GOUT_BLK_NPUS_UID_LHS_AXI_D_CTRL_NPU10_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_npu01[] = {
	GOUT_BLK_NPUS_UID_LHS_AXI_D0_NPU01_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_npus[] = {
	GOUT_BLK_NPUS_UID_VGEN_LITE_NPUS_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d0_npu00[] = {
	GOUT_BLK_NPUS_UID_LHS_AXI_D0_NPU00_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_npus[] = {
	GOUT_BLK_NPUS_UID_LHM_AXI_P_NPUS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_cmdq_npu01[] = {
	GOUT_BLK_NPUS_UID_LHM_AXI_D_CMDQ_NPU01_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_cmdq_npu00[] = {
	GOUT_BLK_NPUS_UID_LHM_AXI_D_CMDQ_NPU00_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_ctrl_npu00[] = {
	GOUT_BLK_NPUS_UID_LHS_AXI_D_CTRL_NPU00_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_npus[] = {
	GOUT_BLK_NPUS_UID_LHS_AXI_D1_NPUS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_ctrl_npu01[] = {
	GOUT_BLK_NPUS_UID_LHS_AXI_D_CTRL_NPU01_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_rq_npu10[] = {
	GOUT_BLK_NPUS_UID_LHM_AXI_D_RQ_NPU10_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_npu00[] = {
	GOUT_BLK_NPUS_UID_LHS_AXI_D1_NPU00_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_npu01[] = {
	GOUT_BLK_NPUS_UID_LHS_AXI_D1_NPU01_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_cmdq_npu10[] = {
	GOUT_BLK_NPUS_UID_LHM_AXI_D_CMDQ_NPU10_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_npus[] = {
	GOUT_BLK_NPUS_UID_SYSREG_NPUS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_npus[] = {
	GOUT_BLK_NPUS_UID_D_TZPC_NPUS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ip_npus[] = {
	GOUT_BLK_NPUS_UID_IP_NPUS_IPCLKPORT_I_PCLK,
	GOUT_BLK_NPUS_UID_IP_NPUS_IPCLKPORT_I_ACLK,
	GOUT_BLK_NPUS_UID_IP_NPUS_IPCLKPORT_I_DBGCLK,
	GOUT_BLK_NPUS_UID_IP_NPUS_IPCLKPORT_I_C2A0CLK,
	GOUT_BLK_NPUS_UID_IP_NPUS_IPCLKPORT_I_C2A1CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d2_npus[] = {
	GOUT_BLK_NPUS_UID_LHS_AXI_D2_NPUS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_npus_2[] = {
	GOUT_BLK_NPUS_UID_PPMU_NPUS_2_IPCLKPORT_PCLK,
	GOUT_BLK_NPUS_UID_PPMU_NPUS_2_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_npus_1[] = {
	GOUT_BLK_NPUS_UID_PPMU_NPUS_1_IPCLKPORT_PCLK,
	GOUT_BLK_NPUS_UID_PPMU_NPUS_1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_npus_0[] = {
	GOUT_BLK_NPUS_UID_PPMU_NPUS_0_IPCLKPORT_PCLK,
	GOUT_BLK_NPUS_UID_PPMU_NPUS_0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_rq_npu00[] = {
	GOUT_BLK_NPUS_UID_LHM_AXI_D_RQ_NPU00_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_rq_npu01[] = {
	GOUT_BLK_NPUS_UID_LHM_AXI_D_RQ_NPU01_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d1_npu10[] = {
	GOUT_BLK_NPUS_UID_LHS_AXI_D1_NPU10_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_p0_npus[] = {
	GOUT_BLK_NPUS_UID_AD_APB_P0_NPUS_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_int_npus[] = {
	GOUT_BLK_NPUS_UID_LHS_AXI_P_INT_NPUS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_int_npus[] = {
	GOUT_BLK_NPUS_UID_LHM_AXI_P_INT_NPUS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_hpm_npus[] = {
	CLK_BLK_NPUS_UID_HPM_NPUS_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_add_npus[] = {
	CLK_BLK_NPUS_UID_ADD_NPUS_IPCLKPORT_CH_CLK,
	CLK_BLK_NPUS_UID_ADD_NPUS_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_busif_hpm_npus[] = {
	GOUT_BLK_NPUS_UID_BUSIF_HPM_NPUS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_add_npus[] = {
	GOUT_BLK_NPUS_UID_BUSIF_ADD_NPUS_IPCLKPORT_PCLK,
	GOUT_BLK_NPUS_UID_BUSIF_ADD_NPUS_IPCLKPORT_CLK_CORE,
	GOUT_BLK_NPUS_UID_BUSIF_ADD_NPUS_IPCLKPORT_CLK_CORE,
};
enum clk_id cmucal_vclk_ip_busif_ddd_npus[] = {
	GOUT_BLK_NPUS_UID_BUSIF_DDD_NPUS_IPCLKPORT_PCLK,
	GOUT_BLK_NPUS_UID_BUSIF_DDD_NPUS_IPCLKPORT_CK_IN,
};
enum clk_id cmucal_vclk_ip_htu_npus[] = {
	GOUT_BLK_NPUS_UID_HTU_NPUS_IPCLKPORT_I_PCLK,
	GOUT_BLK_NPUS_UID_HTU_NPUS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ddd_npus[] = {
	CLK_BLK_NPUS_UID_DDD_NPUS_IPCLKPORT_CK_IN,
};
enum clk_id cmucal_vclk_ip_adm_dap_npus[] = {
	GOUT_BLK_NPUS_UID_ADM_DAP_NPUS_IPCLKPORT_DAPCLKM,
};
enum clk_id cmucal_vclk_ip_gpio_peric0[] = {
	GOUT_BLK_PERIC0_UID_GPIO_PERIC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_peric0[] = {
	GOUT_BLK_PERIC0_UID_SYSREG_PERIC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_peric0_cmu_peric0[] = {
	CLK_BLK_PERIC0_UID_PERIC0_CMU_PERIC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_peric0[] = {
	GOUT_BLK_PERIC0_UID_LHM_AXI_P_PERIC0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_peric0[] = {
	GOUT_BLK_PERIC0_UID_D_TZPC_PERIC0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_peric0_top0[] = {
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_4,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_4,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_5,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_6,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_7,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_8,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_9,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_10,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_11,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_12,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_13,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_14,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_PCLK_15,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_5,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_6,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_7,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_8,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_9,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_10,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_11,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_12,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_13,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_14,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP0_IPCLKPORT_IPCLK_15,
};
enum clk_id cmucal_vclk_ip_peric0_top1[] = {
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_PCLK_0,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_PCLK_3,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_PCLK_4,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_PCLK_5,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_PCLK_6,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_PCLK_7,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_PCLK_8,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_PCLK_15,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_IPCLK_0,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_IPCLK_3,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_IPCLK_4,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_IPCLK_5,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_IPCLK_6,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_IPCLK_7,
	GOUT_BLK_PERIC0_UID_PERIC0_TOP1_IPCLKPORT_IPCLK_8,
};
enum clk_id cmucal_vclk_ip_gpio_peric1[] = {
	GOUT_BLK_PERIC1_UID_GPIO_PERIC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_peric1[] = {
	GOUT_BLK_PERIC1_UID_SYSREG_PERIC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_peric1_cmu_peric1[] = {
	CLK_BLK_PERIC1_UID_PERIC1_CMU_PERIC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_peric1[] = {
	GOUT_BLK_PERIC1_UID_LHM_AXI_P_PERIC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_peric1[] = {
	GOUT_BLK_PERIC1_UID_D_TZPC_PERIC1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_peric1_top0[] = {
	GOUT_BLK_PERIC1_UID_PERIC1_TOP0_IPCLKPORT_PCLK_4,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP0_IPCLKPORT_IPCLK_4,
};
enum clk_id cmucal_vclk_ip_peric1_top1[] = {
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_4,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_5,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_6,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_7,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_9,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_10,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_4,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_5,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_6,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_7,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_9,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_10,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_12,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_12,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_13,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_14,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_PCLK_15,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_13,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_14,
	GOUT_BLK_PERIC1_UID_PERIC1_TOP1_IPCLKPORT_IPCLK_15,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_csisperic1[] = {
	GOUT_BLK_PERIC1_UID_LHM_AXI_P_CSISPERIC1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_p_peric1[] = {
	GOUT_BLK_PERIC1_UID_XIU_P_PERIC1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_usi16_i3c[] = {
	GOUT_BLK_PERIC1_UID_USI16_I3C_IPCLKPORT_I_PCLK,
	GOUT_BLK_PERIC1_UID_USI16_I3C_IPCLKPORT_I_SCLK,
};
enum clk_id cmucal_vclk_ip_usi17_i3c[] = {
	GOUT_BLK_PERIC1_UID_USI17_I3C_IPCLKPORT_I_SCLK,
	GOUT_BLK_PERIC1_UID_USI17_I3C_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_peric2_top0[] = {
	GOUT_BLK_PERIC2_UID_PERIC2_TOP0_IPCLKPORT_PCLK_11,
	GOUT_BLK_PERIC2_UID_PERIC2_TOP0_IPCLKPORT_PCLK_10,
	GOUT_BLK_PERIC2_UID_PERIC2_TOP0_IPCLKPORT_PCLK_13,
	GOUT_BLK_PERIC2_UID_PERIC2_TOP0_IPCLKPORT_PCLK_12,
	GOUT_BLK_PERIC2_UID_PERIC2_TOP0_IPCLKPORT_IPCLK_15,
	GOUT_BLK_PERIC2_UID_PERIC2_TOP0_IPCLKPORT_IPCLK_14,
	GOUT_BLK_PERIC2_UID_PERIC2_TOP0_IPCLKPORT_PCLK_15,
	GOUT_BLK_PERIC2_UID_PERIC2_TOP0_IPCLKPORT_IPCLK_13,
	GOUT_BLK_PERIC2_UID_PERIC2_TOP0_IPCLKPORT_PCLK_14,
	GOUT_BLK_PERIC2_UID_PERIC2_TOP0_IPCLKPORT_IPCLK_12,
	GOUT_BLK_PERIC2_UID_PERIC2_TOP0_IPCLKPORT_IPCLK_11,
	GOUT_BLK_PERIC2_UID_PERIC2_TOP0_IPCLKPORT_IPCLK_10,
};
enum clk_id cmucal_vclk_ip_sysreg_peric2[] = {
	GOUT_BLK_PERIC2_UID_SYSREG_PERIC2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_peric2[] = {
	GOUT_BLK_PERIC2_UID_D_TZPC_PERIC2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_peric2[] = {
	GOUT_BLK_PERIC2_UID_LHM_AXI_P_PERIC2_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_gpio_peric2[] = {
	GOUT_BLK_PERIC2_UID_GPIO_PERIC2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_peric2_cmu_peric2[] = {
	CLK_BLK_PERIC2_UID_PERIC2_CMU_PERIC2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_peric2_top1[] = {
	GOUT_BLK_PERIC2_UID_PERIC2_TOP1_IPCLKPORT_IPCLK_0,
	GOUT_BLK_PERIC2_UID_PERIC2_TOP1_IPCLKPORT_PCLK_0,
	GOUT_BLK_PERIC2_UID_PERIC2_TOP1_IPCLKPORT_IPCLK_1,
	GOUT_BLK_PERIC2_UID_PERIC2_TOP1_IPCLKPORT_PCLK_1,
	GOUT_BLK_PERIC2_UID_PERIC2_TOP1_IPCLKPORT_IPCLK_2,
	GOUT_BLK_PERIC2_UID_PERIC2_TOP1_IPCLKPORT_PCLK_2,
	GOUT_BLK_PERIC2_UID_PERIC2_TOP1_IPCLKPORT_IPCLK_3,
	GOUT_BLK_PERIC2_UID_PERIC2_TOP1_IPCLKPORT_PCLK_3,
};
enum clk_id cmucal_vclk_ip_sysreg_peris[] = {
	GOUT_BLK_PERIS_UID_SYSREG_PERIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt1[] = {
	GOUT_BLK_PERIS_UID_WDT1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt0[] = {
	GOUT_BLK_PERIS_UID_WDT0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_peris_cmu_peris[] = {
	CLK_BLK_PERIS_UID_PERIS_CMU_PERIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_otp_con_bira[] = {
	GOUT_BLK_PERIS_UID_OTP_CON_BIRA_IPCLKPORT_PCLK,
	CLK_BLK_PERIS_UID_OTP_CON_BIRA_IPCLKPORT_I_OSCCLK,
};
enum clk_id cmucal_vclk_ip_gic[] = {
	GOUT_BLK_PERIS_UID_GIC_IPCLKPORT_GICCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_peris[] = {
	GOUT_BLK_PERIS_UID_LHM_AXI_P_PERIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mct[] = {
	GOUT_BLK_PERIS_UID_MCT_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_otp_con_top[] = {
	GOUT_BLK_PERIS_UID_OTP_CON_TOP_IPCLKPORT_PCLK,
	CLK_BLK_PERIS_UID_OTP_CON_TOP_IPCLKPORT_I_OSCCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_peris[] = {
	GOUT_BLK_PERIS_UID_D_TZPC_PERIS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_tmu_sub[] = {
	GOUT_BLK_PERIS_UID_TMU_SUB_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_tmu_top[] = {
	GOUT_BLK_PERIS_UID_TMU_TOP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_perisgic[] = {
	GOUT_BLK_PERIS_UID_LHM_AXI_P_PERISGIC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_bc_emul[] = {
	GOUT_BLK_PERIS_UID_BC_EMUL_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_icc_cpugic_cluster0[] = {
	GOUT_BLK_PERIS_UID_LHM_AST_ICC_CPUGIC_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_iri_giccpu_cluster0[] = {
	GOUT_BLK_PERIS_UID_LHS_AST_IRI_GICCPU_CLUSTER0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_otp_con_bisr[] = {
	GOUT_BLK_PERIS_UID_OTP_CON_BISR_IPCLKPORT_PCLK,
	CLK_BLK_PERIS_UID_OTP_CON_BISR_IPCLKPORT_I_OSCCLK,
};
enum clk_id cmucal_vclk_ip_s2d_cmu_s2d[] = {
	CLK_BLK_S2D_UID_S2D_CMU_S2D_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_bis_s2d[] = {
	GOUT_BLK_S2D_UID_BIS_S2D_IPCLKPORT_CLK,
	CLK_BLK_S2D_UID_BIS_S2D_IPCLKPORT_SCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_g_scan2dram[] = {
	GOUT_BLK_S2D_UID_LHM_AXI_G_SCAN2DRAM_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_sysmmu_rtic[] = {
	GOUT_BLK_SSP_UID_AD_APB_SYSMMU_RTIC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_sysmmu_rtic[] = {
	GOUT_BLK_SSP_UID_SYSMMU_RTIC_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_rtic[] = {
	GOUT_BLK_SSP_UID_RTIC_IPCLKPORT_I_ACLK,
	GOUT_BLK_SSP_UID_RTIC_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d_ssp[] = {
	GOUT_BLK_SSP_UID_XIU_D_SSP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d_ssp[] = {
	GOUT_BLK_SSP_UID_LHS_ACEL_D_SSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_puf[] = {
	GOUT_BLK_SSP_UID_AD_APB_PUF_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_puf[] = {
	GOUT_BLK_SSP_UID_PUF_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sss[] = {
	GOUT_BLK_SSP_UID_SSS_IPCLKPORT_I_ACLK,
	GOUT_BLK_SSP_UID_SSS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_ssp[] = {
	GOUT_BLK_SSP_UID_LHM_AXI_P_SSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_ssp[] = {
	GOUT_BLK_SSP_UID_D_TZPC_SSP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_sss[] = {
	GOUT_BLK_SSP_UID_BAAW_SSS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_sspcore[] = {
	GOUT_BLK_SSP_UID_LHM_AXI_D_SSPCORE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_ssp[] = {
	GOUT_BLK_SSP_UID_PPMU_SSP_IPCLKPORT_ACLK,
	GOUT_BLK_SSP_UID_PPMU_SSP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_uss_sspcore[] = {
	GOUT_BLK_SSP_UID_USS_SSPCORE_IPCLKPORT_SS_SSPCORE_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_qe_rtic[] = {
	GOUT_BLK_SSP_UID_QE_RTIC_IPCLKPORT_ACLK,
	GOUT_BLK_SSP_UID_QE_RTIC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_sspcore[] = {
	GOUT_BLK_SSP_UID_QE_SSPCORE_IPCLKPORT_ACLK,
	GOUT_BLK_SSP_UID_QE_SSPCORE_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_sss[] = {
	GOUT_BLK_SSP_UID_QE_SSS_IPCLKPORT_ACLK,
	GOUT_BLK_SSP_UID_QE_SSS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_rtic[] = {
	GOUT_BLK_SSP_UID_VGEN_LITE_RTIC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_sspctrl[] = {
	GOUT_BLK_SSP_UID_SYSREG_SSPCTRL_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sweeper_d_ssp[] = {
	GOUT_BLK_SSP_UID_SWEEPER_D_SSP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_bps_axi_p_ssp[] = {
	GOUT_BLK_SSP_UID_BPS_AXI_P_SSP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_adm_dap_sss[] = {
	GOUT_BLK_SSP_UID_ADM_DAP_SSS_IPCLKPORT_DAPCLKM,
};
enum clk_id cmucal_vclk_ip_ssp_cmu_ssp[] = {
	CLK_BLK_SSP_UID_SSP_CMU_SSP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_taa[] = {
	GOUT_BLK_TAA_UID_LHS_AXI_D_TAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_taa[] = {
	GOUT_BLK_TAA_UID_LHM_AXI_P_TAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_taa[] = {
	GOUT_BLK_TAA_UID_SYSREG_TAA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_taa_cmu_taa[] = {
	CLK_BLK_TAA_UID_TAA_CMU_TAA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf_taadns[] = {
	GOUT_BLK_TAA_UID_LHS_AST_OTF_TAADNS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_taa[] = {
	GOUT_BLK_TAA_UID_D_TZPC_TAA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf0_csistaa[] = {
	GOUT_BLK_TAA_UID_LHM_AST_OTF0_CSISTAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sipu_taa[] = {
	GOUT_BLK_TAA_UID_SIPU_TAA_IPCLKPORT_CLK,
	GOUT_BLK_TAA_UID_SIPU_TAA_IPCLKPORT_CLK_C2COM_STAT,
	GOUT_BLK_TAA_UID_SIPU_TAA_IPCLKPORT_CLK_C2COM_STAT,
	GOUT_BLK_TAA_UID_SIPU_TAA_IPCLKPORT_CLK_C2COM_YDS,
	GOUT_BLK_TAA_UID_SIPU_TAA_IPCLKPORT_CLK_C2COM_YDS,
};
enum clk_id cmucal_vclk_ip_ad_apb_taa[] = {
	GOUT_BLK_TAA_UID_AD_APB_TAA_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhs_ast_zotf0_taacsis[] = {
	GOUT_BLK_TAA_UID_LHS_AST_ZOTF0_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_zotf1_taacsis[] = {
	GOUT_BLK_TAA_UID_LHS_AST_ZOTF1_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_zotf2_taacsis[] = {
	GOUT_BLK_TAA_UID_LHS_AST_ZOTF2_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_taa[] = {
	GOUT_BLK_TAA_UID_PPMU_TAA_IPCLKPORT_ACLK,
	GOUT_BLK_TAA_UID_PPMU_TAA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d_taa[] = {
	GOUT_BLK_TAA_UID_SYSMMU_D_TAA_IPCLKPORT_CLK_S1,
	GOUT_BLK_TAA_UID_SYSMMU_D_TAA_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_vgen_lite_taa0[] = {
	GOUT_BLK_TAA_UID_VGEN_LITE_TAA0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf1_csistaa[] = {
	GOUT_BLK_TAA_UID_LHM_AST_OTF1_CSISTAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf2_csistaa[] = {
	GOUT_BLK_TAA_UID_LHM_AST_OTF2_CSISTAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_sotf0_taacsis[] = {
	GOUT_BLK_TAA_UID_LHS_AST_SOTF0_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_sotf1_taacsis[] = {
	GOUT_BLK_TAA_UID_LHS_AST_SOTF1_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_sotf2_taacsis[] = {
	GOUT_BLK_TAA_UID_LHS_AST_SOTF2_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_otf3_csistaa[] = {
	GOUT_BLK_TAA_UID_LHM_AST_OTF3_CSISTAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_sotf3_taacsis[] = {
	GOUT_BLK_TAA_UID_LHS_AST_SOTF3_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_zotf3_taacsis[] = {
	GOUT_BLK_TAA_UID_LHS_AST_ZOTF3_TAACSIS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_ast_vo_mcfp1taa[] = {
	GOUT_BLK_TAA_UID_LHM_AST_VO_MCFP1TAA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_vo_taamcfp1[] = {
	GOUT_BLK_TAA_UID_LHS_AST_VO_TAAMCFP1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_hpm_taa[] = {
	CLK_BLK_TAA_UID_HPM_TAA_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_busif_hpm_taa[] = {
	GOUT_BLK_TAA_UID_BUSIF_HPM_TAA_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_add_taa[] = {
	CLK_BLK_TAA_UID_ADD_TAA_IPCLKPORT_CH_CLK,
	CLK_BLK_TAA_UID_ADD_TAA_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_busif_add_taa[] = {
	GOUT_BLK_TAA_UID_BUSIF_ADD_TAA_IPCLKPORT_PCLK,
	GOUT_BLK_TAA_UID_BUSIF_ADD_TAA_IPCLKPORT_CLK_CORE,
	GOUT_BLK_TAA_UID_BUSIF_ADD_TAA_IPCLKPORT_CLK_CORE,
};
enum clk_id cmucal_vclk_ip_busif_ddd_taa[] = {
	GOUT_BLK_TAA_UID_BUSIF_DDD_TAA_IPCLKPORT_PCLK,
	GOUT_BLK_TAA_UID_BUSIF_DDD_TAA_IPCLKPORT_CK_IN,
};
enum clk_id cmucal_vclk_ip_vgen_lite_taa1[] = {
	GOUT_BLK_TAA_UID_VGEN_LITE_TAA1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ddd_taa[] = {
	CLK_BLK_TAA_UID_DDD_TAA_IPCLKPORT_CK_IN,
};
enum clk_id cmucal_vclk_ip_xiu_d_taa[] = {
	GOUT_BLK_TAA_UID_XIU_D_TAA_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_vpc_cmu_vpc[] = {
	CLK_BLK_VPC_UID_VPC_CMU_VPC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_vpc[] = {
	GOUT_BLK_VPC_UID_D_TZPC_VPC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_vpcvpd0[] = {
	GOUT_BLK_VPC_UID_LHS_AXI_P_VPCVPD0_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_vpcvpd1[] = {
	GOUT_BLK_VPC_UID_LHS_AXI_P_VPCVPD1_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_vpcvpd0_sfr[] = {
	GOUT_BLK_VPC_UID_LHS_AXI_D_VPCVPD0_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_vpc[] = {
	GOUT_BLK_VPC_UID_LHM_AXI_P_VPC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_vpd0vpc_sfr[] = {
	GOUT_BLK_VPC_UID_LHM_AXI_D_VPD0VPC_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_vpd1vpc_sfr[] = {
	GOUT_BLK_VPC_UID_LHM_AXI_D_VPD1VPC_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ppmu_vpc0[] = {
	GOUT_BLK_VPC_UID_PPMU_VPC0_IPCLKPORT_PCLK,
	GOUT_BLK_VPC_UID_PPMU_VPC0_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_vpc1[] = {
	GOUT_BLK_VPC_UID_PPMU_VPC1_IPCLKPORT_PCLK,
	GOUT_BLK_VPC_UID_PPMU_VPC1_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_vpc2[] = {
	GOUT_BLK_VPC_UID_PPMU_VPC2_IPCLKPORT_PCLK,
	GOUT_BLK_VPC_UID_PPMU_VPC2_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ip_vpc[] = {
	GOUT_BLK_VPC_UID_IP_VPC_IPCLKPORT_CLK,
	GOUT_BLK_VPC_UID_IP_VPC_IPCLKPORT_DAP_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_vpc[] = {
	GOUT_BLK_VPC_UID_SYSREG_VPC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_vpcvpd1_sfr[] = {
	GOUT_BLK_VPC_UID_LHS_AXI_D_VPCVPD1_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_p_vpc_200[] = {
	GOUT_BLK_VPC_UID_LHS_AXI_P_VPC_200_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_vpc_800[] = {
	GOUT_BLK_VPC_UID_LHM_AXI_P_VPC_800_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_vpc2[] = {
	GOUT_BLK_VPC_UID_SYSMMU_VPC2_IPCLKPORT_CLK_S1,
	GOUT_BLK_VPC_UID_SYSMMU_VPC2_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d2_vpc[] = {
	GOUT_BLK_VPC_UID_LHS_ACEL_D2_VPC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_vpd1vpc_cache[] = {
	GOUT_BLK_VPC_UID_LHM_AXI_D_VPD1VPC_CACHE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_vpd0vpc_cache[] = {
	GOUT_BLK_VPC_UID_LHM_AXI_D_VPD0VPC_CACHE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_vpc0[] = {
	GOUT_BLK_VPC_UID_AD_APB_VPC0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_sysmmu_vpc0[] = {
	GOUT_BLK_VPC_UID_SYSMMU_VPC0_IPCLKPORT_CLK_S1,
	GOUT_BLK_VPC_UID_SYSMMU_VPC0_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_sysmmu_vpc1[] = {
	GOUT_BLK_VPC_UID_SYSMMU_VPC1_IPCLKPORT_CLK_S1,
	GOUT_BLK_VPC_UID_SYSMMU_VPC1_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d0_vpc[] = {
	GOUT_BLK_VPC_UID_LHS_ACEL_D0_VPC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_acel_d1_vpc[] = {
	GOUT_BLK_VPC_UID_LHS_ACEL_D1_VPC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_vpcvpd0_dma[] = {
	GOUT_BLK_VPC_UID_LHS_AXI_D_VPCVPD0_DMA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_vpcvpd1_dma[] = {
	GOUT_BLK_VPC_UID_LHS_AXI_D_VPCVPD1_DMA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_hpm_vpc[] = {
	CLK_BLK_VPC_UID_HPM_VPC_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_add_vpc[] = {
	CLK_BLK_VPC_UID_ADD_VPC_IPCLKPORT_CH_CLK,
	CLK_BLK_VPC_UID_ADD_VPC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_busif_hpm_vpc[] = {
	GOUT_BLK_VPC_UID_BUSIF_HPM_VPC_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_busif_add_vpc[] = {
	GOUT_BLK_VPC_UID_BUSIF_ADD_VPC_IPCLKPORT_PCLK,
	GOUT_BLK_VPC_UID_BUSIF_ADD_VPC_IPCLKPORT_CLK_CORE,
	GOUT_BLK_VPC_UID_BUSIF_ADD_VPC_IPCLKPORT_CLK_CORE,
};
enum clk_id cmucal_vclk_ip_busif_ddd_vpc[] = {
	GOUT_BLK_VPC_UID_BUSIF_DDD_VPC_IPCLKPORT_PCLK,
	GOUT_BLK_VPC_UID_BUSIF_DDD_VPC_IPCLKPORT_CK_IN,
};
enum clk_id cmucal_vclk_ip_adm_dap_vpc[] = {
	GOUT_BLK_VPC_UID_ADM_DAP_VPC_IPCLKPORT_DAPCLKM,
};
enum clk_id cmucal_vclk_ip_htu_vpc[] = {
	GOUT_BLK_VPC_UID_HTU_VPC_IPCLKPORT_I_CLK,
	GOUT_BLK_VPC_UID_HTU_VPC_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_ddd_vpc[] = {
	CLK_BLK_VPC_UID_DDD_VPC_IPCLKPORT_CK_IN,
};
enum clk_id cmucal_vclk_ip_vgen_lite_vpc[] = {
	GOUT_BLK_VPC_UID_VGEN_LITE_VPC_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_xiu_vpc_votf[] = {
	GOUT_BLK_VPC_UID_XIU_VPC_VOTF_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_vpd_cmu_vpd[] = {
	CLK_BLK_VPD_UID_VPD_CMU_VPD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_vpd[] = {
	GOUT_BLK_VPD_UID_SYSREG_VPD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_vpdvpc_sfr[] = {
	GOUT_BLK_VPD_UID_LHS_AXI_D_VPDVPC_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ip_vpd[] = {
	GOUT_BLK_VPD_UID_IP_VPD_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_vpdvpc_cache[] = {
	GOUT_BLK_VPD_UID_LHS_AXI_D_VPDVPC_CACHE_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_vpcvpd[] = {
	GOUT_BLK_VPD_UID_LHM_AXI_P_VPCVPD_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_vpcvpd_sfr[] = {
	GOUT_BLK_VPD_UID_LHM_AXI_D_VPCVPD_SFR_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_vpd[] = {
	GOUT_BLK_VPD_UID_D_TZPC_VPD_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_vpcvpd_dma[] = {
	GOUT_BLK_VPD_UID_LHM_AXI_D_VPCVPD_DMA_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ahb_busmatrix[] = {
	GOUT_BLK_VTS_UID_AHB_BUSMATRIX_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_dmic_if0[] = {
	GOUT_BLK_VTS_UID_DMIC_IF0_IPCLKPORT_DMIC_IF_CLK,
	CLK_BLK_VTS_UID_DMIC_IF0_IPCLKPORT_DMIC_IF_DIV2_CLK,
	GOUT_BLK_VTS_UID_DMIC_IF0_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_sysreg_vts[] = {
	GOUT_BLK_VTS_UID_SYSREG_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vts_cmu_vts[] = {
	CLK_BLK_VTS_UID_VTS_CMU_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_vts[] = {
	GOUT_BLK_VTS_UID_LHM_AXI_P_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_gpio_vts[] = {
	GOUT_BLK_VTS_UID_GPIO_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_wdt_vts[] = {
	GOUT_BLK_VTS_UID_WDT_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb0[] = {
	GOUT_BLK_VTS_UID_DMIC_AHB0_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_AHB0_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb1[] = {
	GOUT_BLK_VTS_UID_DMIC_AHB1_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_AHB1_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_asyncinterrupt[] = {
	GOUT_BLK_VTS_UID_ASYNCINTERRUPT_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_ss_vts_glue[] = {
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_ACLK_CPU,
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_AUD_PAD_CLK0,
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_IF_PAD_CLK0,
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_AUD_PAD_CLK1,
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_AUD_PAD_CLK2,
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_IF_PAD_CLK1,
	GOUT_BLK_VTS_UID_SS_VTS_GLUE_IPCLKPORT_DMIC_IF_PAD_CLK2,
};
enum clk_id cmucal_vclk_ip_cortexm4integration[] = {
	GOUT_BLK_VTS_UID_CORTEXM4INTEGRATION_IPCLKPORT_FCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_vts[] = {
	GOUT_BLK_VTS_UID_LHS_AXI_D_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_d_tzpc_vts[] = {
	GOUT_BLK_VTS_UID_D_TZPC_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite[] = {
	GOUT_BLK_VTS_UID_VGEN_LITE_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_bps_lp_vts[] = {
	GOUT_BLK_VTS_UID_BPS_LP_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_bps_p_vts[] = {
	GOUT_BLK_VTS_UID_BPS_P_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sweeper_d_vts[] = {
	GOUT_BLK_VTS_UID_SWEEPER_D_VTS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_baaw_d_vts[] = {
	GOUT_BLK_VTS_UID_BAAW_D_VTS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_mailbox_abox_vts[] = {
	GOUT_BLK_VTS_UID_MAILBOX_ABOX_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb2[] = {
	GOUT_BLK_VTS_UID_DMIC_AHB2_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_AHB2_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb3[] = {
	GOUT_BLK_VTS_UID_DMIC_AHB3_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_AHB3_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_dmic_if2[] = {
	GOUT_BLK_VTS_UID_DMIC_IF2_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_IF2_IPCLKPORT_DMIC_IF_CLK,
	GOUT_BLK_VTS_UID_DMIC_IF2_IPCLKPORT_DMIC_IF_DIV2_CLK,
};
enum clk_id cmucal_vclk_ip_mailbox_apm_vts1[] = {
	GOUT_BLK_VTS_UID_MAILBOX_APM_VTS1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_timer[] = {
	GOUT_BLK_VTS_UID_TIMER_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_pdma_vts[] = {
	GOUT_BLK_VTS_UID_PDMA_VTS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb4[] = {
	GOUT_BLK_VTS_UID_DMIC_AHB4_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_AHB4_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_dmic_ahb5[] = {
	GOUT_BLK_VTS_UID_DMIC_AHB5_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_AHB5_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_dmic_aud0[] = {
	GOUT_BLK_VTS_UID_DMIC_AUD0_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_AUD0_IPCLKPORT_DMIC_AUD_CLK,
	GOUT_BLK_VTS_UID_DMIC_AUD0_IPCLKPORT_DMIC_AUD_DIV2_CLK,
};
enum clk_id cmucal_vclk_ip_dmic_if1[] = {
	GOUT_BLK_VTS_UID_DMIC_IF1_IPCLKPORT_DMIC_IF_CLK,
	GOUT_BLK_VTS_UID_DMIC_IF1_IPCLKPORT_DMIC_IF_DIV2_CLK,
	GOUT_BLK_VTS_UID_DMIC_IF1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmic_aud1[] = {
	GOUT_BLK_VTS_UID_DMIC_AUD1_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_AUD1_IPCLKPORT_DMIC_AUD_CLK,
	GOUT_BLK_VTS_UID_DMIC_AUD1_IPCLKPORT_DMIC_AUD_DIV2_CLK,
};
enum clk_id cmucal_vclk_ip_dmic_aud2[] = {
	GOUT_BLK_VTS_UID_DMIC_AUD2_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMIC_AUD2_IPCLKPORT_DMIC_AUD_CLK,
	GOUT_BLK_VTS_UID_DMIC_AUD2_IPCLKPORT_DMIC_AUD_DIV2_CLK,
};
enum clk_id cmucal_vclk_ip_serial_lif[] = {
	GOUT_BLK_VTS_UID_SERIAL_LIF_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_SERIAL_LIF_IPCLKPORT_BCLK,
	GOUT_BLK_VTS_UID_SERIAL_LIF_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_SERIAL_LIF_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_timer1[] = {
	GOUT_BLK_VTS_UID_TIMER1_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_timer2[] = {
	GOUT_BLK_VTS_UID_TIMER2_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_serial_lif_debug_vt[] = {
	GOUT_BLK_VTS_UID_SERIAL_LIF_DEBUG_VT_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_SERIAL_LIF_DEBUG_VT_IPCLKPORT_BCLK,
	GOUT_BLK_VTS_UID_SERIAL_LIF_DEBUG_VT_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_serial_lif_debug_us[] = {
	GOUT_BLK_VTS_UID_SERIAL_LIF_DEBUG_US_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_SERIAL_LIF_DEBUG_US_IPCLKPORT_BCLK,
	GOUT_BLK_VTS_UID_SERIAL_LIF_DEBUG_US_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_d_audvts[] = {
	GOUT_BLK_VTS_UID_LHM_AXI_D_AUDVTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_mailbox_ap_vts[] = {
	GOUT_BLK_VTS_UID_MAILBOX_AP_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_baaw_c_vts[] = {
	GOUT_BLK_VTS_UID_BAAW_C_VTS_IPCLKPORT_I_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_c_vts[] = {
	GOUT_BLK_VTS_UID_LHS_AXI_C_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sweeper_c_vts[] = {
	GOUT_BLK_VTS_UID_SWEEPER_C_VTS_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic0[] = {
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC0_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC0_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC0_IPCLKPORT_HCLK_BUS,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic1[] = {
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC1_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC1_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC1_IPCLKPORT_HCLK_BUS,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic2[] = {
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC2_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC2_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC2_IPCLKPORT_HCLK_BUS,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic3[] = {
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC3_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC3_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC3_IPCLKPORT_HCLK_BUS,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic4[] = {
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC4_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC4_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC4_IPCLKPORT_HCLK_BUS,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_dmic5[] = {
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC5_IPCLKPORT_HCLK,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC5_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_DMIC5_IPCLKPORT_HCLK_BUS,
};
enum clk_id cmucal_vclk_ip_asyncahb0[] = {
	GOUT_BLK_VTS_UID_ASYNCAHB0_IPCLKPORT_HCLKS,
	GOUT_BLK_VTS_UID_ASYNCAHB0_IPCLKPORT_HCLKM,
};
enum clk_id cmucal_vclk_ip_asyncahb1[] = {
	GOUT_BLK_VTS_UID_ASYNCAHB1_IPCLKPORT_HCLKS,
	GOUT_BLK_VTS_UID_ASYNCAHB1_IPCLKPORT_HCLKM,
};
enum clk_id cmucal_vclk_ip_asyncahb2[] = {
	GOUT_BLK_VTS_UID_ASYNCAHB2_IPCLKPORT_HCLKS,
	GOUT_BLK_VTS_UID_ASYNCAHB2_IPCLKPORT_HCLKM,
};
enum clk_id cmucal_vclk_ip_asyncahb3[] = {
	GOUT_BLK_VTS_UID_ASYNCAHB3_IPCLKPORT_HCLKS,
	GOUT_BLK_VTS_UID_ASYNCAHB3_IPCLKPORT_HCLKM,
};
enum clk_id cmucal_vclk_ip_asyncahb4[] = {
	GOUT_BLK_VTS_UID_ASYNCAHB4_IPCLKPORT_HCLKS,
	GOUT_BLK_VTS_UID_ASYNCAHB4_IPCLKPORT_HCLKM,
};
enum clk_id cmucal_vclk_ip_asyncahb5[] = {
	GOUT_BLK_VTS_UID_ASYNCAHB5_IPCLKPORT_HCLKS,
	GOUT_BLK_VTS_UID_ASYNCAHB5_IPCLKPORT_HCLKM,
};
enum clk_id cmucal_vclk_ip_hpm_vts[] = {
	CLK_BLK_VTS_UID_HPM_VTS_IPCLKPORT_HPM_TARGETCLK_C,
};
enum clk_id cmucal_vclk_ip_async_apb_vts[] = {
	GOUT_BLK_VTS_UID_ASYNC_APB_VTS_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_busif_hpm_vts[] = {
	GOUT_BLK_VTS_UID_BUSIF_HPM_VTS_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_dmailbox_test[] = {
	GOUT_BLK_VTS_UID_DMAILBOX_TEST_IPCLKPORT_PCLK,
	GOUT_BLK_VTS_UID_DMAILBOX_TEST_IPCLKPORT_ACLK,
	GOUT_BLK_VTS_UID_DMAILBOX_TEST_IPCLKPORT_CLK,
	GOUT_BLK_VTS_UID_DMAILBOX_TEST_IPCLKPORT_CCLK,
};
enum clk_id cmucal_vclk_ip_hwacg_sys_serial_lif[] = {
	GOUT_BLK_VTS_UID_HWACG_SYS_SERIAL_LIF_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_SERIAL_LIF_IPCLKPORT_HCLK_BUS,
	GOUT_BLK_VTS_UID_HWACG_SYS_SERIAL_LIF_IPCLKPORT_HCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_lp_vts[] = {
	GOUT_BLK_VTS_UID_LHM_AXI_LP_VTS_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_yuvpp_cmu_yuvpp[] = {
	CLK_BLK_YUVPP_UID_YUVPP_CMU_YUVPP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_yuvpp0[] = {
	GOUT_BLK_YUVPP_UID_AD_APB_YUVPP0_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_d_tzpc_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_D_TZPC_YUVPP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhm_axi_p_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_LHM_AXI_P_YUVPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_sysreg_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_SYSREG_YUVPP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_yuvpp0[] = {
	GOUT_BLK_YUVPP_UID_VGEN_LITE_YUVPP0_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_sysmmu_d_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_SYSMMU_D_YUVPP_IPCLKPORT_CLK_S1,
	GOUT_BLK_YUVPP_UID_SYSMMU_D_YUVPP_IPCLKPORT_CLK_S2,
};
enum clk_id cmucal_vclk_ip_qe_d0_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_QE_D0_YUVPP_IPCLKPORT_PCLK,
	GOUT_BLK_YUVPP_UID_QE_D0_YUVPP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ppmu_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_PPMU_YUVPP_IPCLKPORT_PCLK,
	GOUT_BLK_YUVPP_UID_PPMU_YUVPP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_xiu_d0_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_XIU_D0_YUVPP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_LHS_AXI_D_YUVPP_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_qe_d1_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_QE_D1_YUVPP_IPCLKPORT_ACLK,
	GOUT_BLK_YUVPP_UID_QE_D1_YUVPP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_yuvpp_top[] = {
	GOUT_BLK_YUVPP_UID_YUVPP_TOP_IPCLKPORT_I_CLK,
	GOUT_BLK_YUVPP_UID_YUVPP_TOP_IPCLKPORT_I_CLK_C2COM,
	GOUT_BLK_YUVPP_UID_YUVPP_TOP_IPCLKPORT_I_CLK_C2COM,
};
enum clk_id cmucal_vclk_ip_qe_d2_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_QE_D2_YUVPP_IPCLKPORT_ACLK,
	GOUT_BLK_YUVPP_UID_QE_D2_YUVPP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_d3_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_QE_D3_YUVPP_IPCLKPORT_ACLK,
	GOUT_BLK_YUVPP_UID_QE_D3_YUVPP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_d4_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_QE_D4_YUVPP_IPCLKPORT_ACLK,
	GOUT_BLK_YUVPP_UID_QE_D4_YUVPP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_d5_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_QE_D5_YUVPP_IPCLKPORT_ACLK,
	GOUT_BLK_YUVPP_UID_QE_D5_YUVPP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_lhs_ast_otf_yuvppmcsc[] = {
	GOUT_BLK_YUVPP_UID_LHS_AST_OTF_YUVPPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_frc_mc[] = {
	GOUT_BLK_YUVPP_UID_FRC_MC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_vgen_lite_yuvpp1[] = {
	GOUT_BLK_YUVPP_UID_VGEN_LITE_YUVPP1_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_qe_d6_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_QE_D6_YUVPP_IPCLKPORT_ACLK,
	GOUT_BLK_YUVPP_UID_QE_D6_YUVPP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_d7_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_QE_D7_YUVPP_IPCLKPORT_ACLK,
	GOUT_BLK_YUVPP_UID_QE_D7_YUVPP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_d8_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_QE_D8_YUVPP_IPCLKPORT_ACLK,
	GOUT_BLK_YUVPP_UID_QE_D8_YUVPP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_d9_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_QE_D9_YUVPP_IPCLKPORT_ACLK,
	GOUT_BLK_YUVPP_UID_QE_D9_YUVPP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_xiu_d1_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_XIU_D1_YUVPP_IPCLKPORT_ACLK,
};
enum clk_id cmucal_vclk_ip_ad_apb_frc_mc[] = {
	GOUT_BLK_YUVPP_UID_AD_APB_FRC_MC_IPCLKPORT_PCLKM,
};
enum clk_id cmucal_vclk_ip_lhs_axi_d_yuvppmcsc[] = {
	GOUT_BLK_YUVPP_UID_LHS_AXI_D_YUVPPMCSC_IPCLKPORT_I_CLK,
};
enum clk_id cmucal_vclk_ip_ad_axi_frc_mc[] = {
	GOUT_BLK_YUVPP_UID_AD_AXI_FRC_MC_IPCLKPORT_ACLKM,
};
enum clk_id cmucal_vclk_ip_vgen_lite_yuvpp2[] = {
	GOUT_BLK_YUVPP_UID_VGEN_LITE_YUVPP2_IPCLKPORT_CLK,
};
enum clk_id cmucal_vclk_ip_qe_d10_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_QE_D10_YUVPP_IPCLKPORT_ACLK,
	GOUT_BLK_YUVPP_UID_QE_D10_YUVPP_IPCLKPORT_PCLK,
};
enum clk_id cmucal_vclk_ip_qe_d11_yuvpp[] = {
	GOUT_BLK_YUVPP_UID_QE_D11_YUVPP_IPCLKPORT_ACLK,
	GOUT_BLK_YUVPP_UID_QE_D11_YUVPP_IPCLKPORT_PCLK,
};

/* DVFS VCLK -> LUT List */
struct vclk_lut cmucal_vclk_vdd_mif_lut[] = {
	{4266000, vdd_mif_od_lut_params},
	{3732000, vdd_mif_nm_lut_params},
	{2704000, vdd_mif_ud_lut_params},
	{1420000, vdd_mif_sud_lut_params},
	{710000, vdd_mif_uud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_cam_lut[] = {
	{1200000, vdd_cam_nm_lut_params},
	{1200000, vdd_cam_sud_lut_params},
	{1200000, vdd_cam_ud_lut_params},
	{1200000, vdd_cam_uud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_cpucl2_lut[] = {
	{2950000, vdd_cpucl2_sod_lut_params},
	{2500000, vdd_cpucl2_od_lut_params},
	{2000000, vdd_cpucl2_nm_lut_params},
	{1400000, vdd_cpucl2_ud_lut_params},
	{700000, vdd_cpucl2_sud_lut_params},
	{300000, vdd_cpucl2_uud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_cpucl0_lut[] = {
	{2300000, vdd_cpucl0_sod_lut_params},
	{2050000, vdd_cpucl0_od_lut_params},
	{1800000, vdd_cpucl0_nm_lut_params},
	{1300000, vdd_cpucl0_ud_lut_params},
	{700000, vdd_cpucl0_sud_lut_params},
	{300000, vdd_cpucl0_uud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_cpucl1_lut[] = {
	{2800000, vdd_cpucl1_sod_lut_params},
	{2400000, vdd_cpucl1_od_lut_params},
	{2000000, vdd_cpucl1_nm_lut_params},
	{1400000, vdd_cpucl1_ud_lut_params},
	{700000, vdd_cpucl1_sud_lut_params},
	{300000, vdd_cpucl1_uud_lut_params},
};
struct vclk_lut cmucal_vclk_vdd_int_cmu_lut[] = {
	{2028000, vdd_int_cmu_nm_lut_params},
	{2028000, vdd_int_cmu_od_lut_params},
	{2028000, vdd_int_cmu_sod_lut_params},
	{2028000, vdd_int_cmu_ud_lut_params},
	{1690000, vdd_int_cmu_sud_lut_params},
	{842000, vdd_int_cmu_uud_lut_params},
};

/* SPECIAL VCLK -> LUT List */
struct vclk_lut cmucal_vclk_mux_clk_alive_i3c_pmic_lut[] = {
	{429900, mux_clk_alive_i3c_pmic_nm_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_alive_bus_lut[] = {
	{533000, clkcmu_alive_bus_sod_lut_params},
	{400000, clkcmu_alive_bus_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_aud_dsif_lut[] = {
	{600000, mux_clk_aud_dsif_nm_lut_params},
	{533000, mux_clk_aud_dsif_uud_lut_params},
	{50000, mux_clk_aud_dsif_od_lut_params},
};
struct vclk_lut cmucal_vclk_mux_bus0_cmuref_lut[] = {
	{200000, mux_bus0_cmuref_nm_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_cmu_boost_lut[] = {
	{400000, mux_clkcmu_cmu_boost_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_bus1_cmuref_lut[] = {
	{200000, mux_bus1_cmuref_nm_lut_params},
};
struct vclk_lut cmucal_vclk_mux_bus2_cmuref_lut[] = {
	{200000, mux_bus2_cmuref_nm_lut_params},
};
struct vclk_lut cmucal_vclk_clk_cmgp_adc_lut[] = {
	{30000, clk_cmgp_adc_nm_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cmgp_adc_lut[] = {
	{30000, clkcmu_cmgp_adc_nm_lut_params},
};
struct vclk_lut cmucal_vclk_mux_core_cmuref_lut[] = {
	{200000, mux_core_cmuref_nm_lut_params},
};
struct vclk_lut cmucal_vclk_mux_cpucl0_cmuref_lut[] = {
	{200000, mux_cpucl0_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_cmu_boost_cpu_lut[] = {
	{400000, mux_clkcmu_cmu_boost_cpu_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_cpucl1_cmuref_lut[] = {
	{200000, mux_cpucl1_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_cpucl2_cmuref_lut[] = {
	{200000, mux_cpucl2_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_dsu_cmuref_lut[] = {
	{200000, mux_dsu_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clk_hsi0_usb31drd_lut[] = {
	{20000, mux_clk_hsi0_usb31drd_nm_lut_params},
};
struct vclk_lut cmucal_vclk_mux_mif_cmuref_lut[] = {
	{200000, mux_mif_cmuref_uud_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_cmu_boost_mif_lut[] = {
	{400000, mux_clkcmu_cmu_boost_mif_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_g3d_shader_lut[] = {
	{950000, clkcmu_g3d_shader_sod_lut_params},
	{750000, clkcmu_g3d_shader_ud_lut_params},
	{570000, clkcmu_g3d_shader_sud_lut_params},
	{220000, clkcmu_g3d_shader_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_alive_dbgcore_uart_lut[] = {
	{214950, div_clk_alive_dbgcore_uart_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_i2c0_lut[] = {
	{214950, div_clk_cmgp_i2c0_nm_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cmgp_peri_lut[] = {
	{400000, clkcmu_cmgp_peri_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_usi1_lut[] = {
	{429900, div_clk_cmgp_usi1_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_usi0_lut[] = {
	{429900, div_clk_cmgp_usi0_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_usi2_lut[] = {
	{429900, div_clk_cmgp_usi2_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_usi3_lut[] = {
	{429900, div_clk_cmgp_usi3_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_i2c1_lut[] = {
	{214950, div_clk_cmgp_i2c1_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_i2c2_lut[] = {
	{214950, div_clk_cmgp_i2c2_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_i2c3_lut[] = {
	{214950, div_clk_cmgp_i2c3_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_i3c_lut[] = {
	{214950, div_clk_cmgp_i3c_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cmgp_xx_lut[] = {
	{400000, div_clk_cmgp_400_lut_params},
	{200000, div_clk_cmgp_200_lut_params},
	{100000, div_clk_cmgp_100_lut_params},
};
struct vclk_lut cmucal_vclk_mux_clkcmu_hpm_lut[] = {
	{400000, mux_clkcmu_hpm_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk0_lut[] = {
	{100000, clkcmu_cis_clk0_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk1_lut[] = {
	{100000, clkcmu_cis_clk1_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk2_lut[] = {
	{100000, clkcmu_cis_clk2_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk3_lut[] = {
	{100000, clkcmu_cis_clk3_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk4_lut[] = {
	{100000, clkcmu_cis_clk4_uud_lut_params},
};
struct vclk_lut cmucal_vclk_clkcmu_cis_clk5_lut[] = {
	{100000, clkcmu_cis_clk5_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cpucl0_shortstop_core_lut[] = {
	{1150000, div_clk_cpucl0_shortstop_core_sod_lut_params},
	{1025000, div_clk_cpucl0_shortstop_core_od_lut_params},
	{900000, div_clk_cpucl0_shortstop_core_nm_lut_params},
	{650000, div_clk_cpucl0_shortstop_core_ud_lut_params},
	{350000, div_clk_cpucl0_shortstop_core_sud_lut_params},
	{150000, div_clk_cpucl0_shortstop_core_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cpucl1_shortstop_core_lut[] = {
	{1400000, div_clk_cpucl1_shortstop_core_sod_lut_params},
	{1200000, div_clk_cpucl1_shortstop_core_od_lut_params},
	{1000000, div_clk_cpucl1_shortstop_core_nm_lut_params},
	{700000, div_clk_cpucl1_shortstop_core_ud_lut_params},
	{350000, div_clk_cpucl1_shortstop_core_sud_lut_params},
	{150000, div_clk_cpucl1_shortstop_core_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cpucl1_htu_lut[] = {
	{700000, div_clk_cpucl1_htu_sod_lut_params},
	{600000, div_clk_cpucl1_htu_od_lut_params},
	{500000, div_clk_cpucl1_htu_nm_lut_params},
	{350000, div_clk_cpucl1_htu_ud_lut_params},
	{175000, div_clk_cpucl1_htu_sud_lut_params},
	{75000, div_clk_cpucl1_htu_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cpucl2_shortstop_core_lut[] = {
	{1475000, div_clk_cpucl2_shortstop_core_sod_lut_params},
	{1250000, div_clk_cpucl2_shortstop_core_od_lut_params},
	{1000000, div_clk_cpucl2_shortstop_core_nm_lut_params},
	{700000, div_clk_cpucl2_shortstop_core_ud_lut_params},
	{350000, div_clk_cpucl2_shortstop_core_sud_lut_params},
	{150000, div_clk_cpucl2_shortstop_core_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_cpucl2_htu_lut[] = {
	{737500, div_clk_cpucl2_htu_sod_lut_params},
	{625000, div_clk_cpucl2_htu_od_lut_params},
	{500000, div_clk_cpucl2_htu_nm_lut_params},
	{350000, div_clk_cpucl2_htu_ud_lut_params},
	{175000, div_clk_cpucl2_htu_sud_lut_params},
	{75000, div_clk_cpucl2_htu_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_dsu_shortstop_cluster_lut[] = {
	{1000000, div_clk_dsu_shortstop_cluster_sod_lut_params},
	{875000, div_clk_dsu_shortstop_cluster_od_lut_params},
	{700000, div_clk_dsu_shortstop_cluster_nm_lut_params},
	{500000, div_clk_dsu_shortstop_cluster_ud_lut_params},
	{250000, div_clk_dsu_shortstop_cluster_sud_lut_params},
	{90000, div_clk_dsu_shortstop_cluster_uud_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric0_usi00_usi_lut[] = {
	{400000, div_clk_peric0_usi00_usi_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric0_usi13_usi_lut[] = {
	{400000, div_clk_peric0_usi13_usi_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric1_uart_bt_lut[] = {
	{200000, div_clk_peric1_uart_bt_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric1_usi18_usi_lut[] = {
	{400000, div_clk_peric1_usi18_usi_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric2_usi08_usi_lut[] = {
	{400000, div_clk_peric2_usi08_usi_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_peric2_usi09_usi_lut[] = {
	{400000, div_clk_peric2_usi09_usi_nm_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_pericx_usixx_usi_lut[] = {
	{400000, div_clk_peric_400_lut_params},
	{200000, div_clk_peric_200_lut_params},
	{133000, div_clk_peric_133_lut_params},
	{100000, div_clk_peric_100_lut_params},
	{66000, div_clk_peric_66_lut_params},
	{50000, div_clk_peric_50_lut_params},
	{26000, div_clk_peric_26_lut_params},
	{13000, div_clk_peric_13_lut_params},
	{8600, div_clk_peric_8_lut_params},
	{6500, div_clk_peric_6_lut_params},
};
struct vclk_lut cmucal_vclk_div_clk_top_hsi0_bus_lut[] = {
	{266500, div_clk_top_hsi0_bus_266_params},
	{177666, div_clk_top_hsi0_bus_177_params},
	{106600, div_clk_top_hsi0_bus_106_params},
	{80000, div_clk_top_hsi0_bus_80_params},
	{66666, div_clk_top_hsi0_bus_66_params},
};

/* COMMON VCLK -> LUT List */
struct vclk_lut cmucal_vclk_blk_aud_lut[] = {
	{300000, blk_aud_sud_lut_params},
	{300000, blk_aud_uud_lut_params},
	{26000, blk_aud_od_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cmu_lut[] = {
	{2028000, blk_cmu_ud_lut_params},
	{1690000, blk_cmu_sud_lut_params},
	{1066000, blk_cmu_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_s2d_lut[] = {
	{200000, blk_s2d_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_alive_lut[] = {
	{400000, blk_alive_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cpucl0_lut[] = {
	{2300000, blk_cpucl0_sod_lut_params},
	{2050000, blk_cpucl0_od_lut_params},
	{1800000, blk_cpucl0_nm_lut_params},
	{1300000, blk_cpucl0_ud_lut_params},
	{700000, blk_cpucl0_sud_lut_params},
	{300000, blk_cpucl0_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cpucl1_lut[] = {
	{2800000, blk_cpucl1_sod_lut_params},
	{2400000, blk_cpucl1_od_lut_params},
	{2000000, blk_cpucl1_nm_lut_params},
	{1400000, blk_cpucl1_ud_lut_params},
	{700000, blk_cpucl1_sud_lut_params},
	{300000, blk_cpucl1_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cpucl2_lut[] = {
	{2950000, blk_cpucl2_sod_lut_params},
	{2500000, blk_cpucl2_od_lut_params},
	{2000000, blk_cpucl2_nm_lut_params},
	{1400000, blk_cpucl2_ud_lut_params},
	{700000, blk_cpucl2_sud_lut_params},
	{300000, blk_cpucl2_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_dsu_lut[] = {
	{2000000, blk_dsu_sod_lut_params},
	{1750000, blk_dsu_od_lut_params},
	{1400000, blk_dsu_nm_lut_params},
	{1000000, blk_dsu_ud_lut_params},
	{500000, blk_dsu_sud_lut_params},
	{180000, blk_dsu_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_g3d_lut[] = {
	{237500, blk_g3d_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_hsi0_lut[] = {
	{266500, blk_hsi0_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_peric0_lut[] = {
	{200000, blk_peric0_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_peric1_lut[] = {
	{200000, blk_peric1_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_peric2_lut[] = {
	{200000, blk_peric2_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_vts_lut[] = {
	{429900, blk_vts_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_bus0_lut[] = {
	{266667, blk_bus0_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_bus1_lut[] = {
	{331500, blk_bus1_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_bus2_lut[] = {
	{331500, blk_bus2_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_core_lut[] = {
	{312000, blk_core_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_cpucl0_glb_lut[] = {
	{400000, blk_cpucl0_glb_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_csis_lut[] = {
	{331500, blk_csis_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_dns_lut[] = {
	{331500, blk_dns_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_dpub_lut[] = {
	{331500, blk_dpub_uud_lut_params},
};
struct vclk_lut cmucal_vclk_blk_dpuf0_lut[] = {
	{331500, blk_dpuf0_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_dpuf1_lut[] = {
	{331500, blk_dpuf1_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_itp_lut[] = {
	{331500, blk_itp_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_lme_lut[] = {
	{331500, blk_lme_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_m2m_lut[] = {
	{331500, blk_m2m_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_mcfp0_lut[] = {
	{331500, blk_mcfp0_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_mcfp1_lut[] = {
	{331500, blk_mcfp1_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_mcsc_lut[] = {
	{331500, blk_mcsc_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_mfc0_lut[] = {
	{178750, blk_mfc0_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_mfc1_lut[] = {
	{178750, blk_mfc1_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_npu_lut[] = {
	{234000, blk_npu_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_npu01_lut[] = {
	{234000, blk_npu01_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_npu10_lut[] = {
	{234000, blk_npu10_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_npus_lut[] = {
	{200000, blk_npus_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_peris_lut[] = {
	{100000, blk_peris_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_ssp_lut[] = {
	{200000, blk_ssp_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_taa_lut[] = {
	{331500, blk_taa_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_vpc_lut[] = {
	{200000, blk_vpc_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_vpd_lut[] = {
	{468000, blk_vpd_nm_lut_params},
};
struct vclk_lut cmucal_vclk_blk_yuvpp_lut[] = {
	{331500, blk_yuvpp_nm_lut_params},
};

/* Switch VCLK -> LUT Parameter List */
struct switch_lut mux_clk_aud_cpu_lut[] = {
	{1066000, 0, 0},
	{800000, 2, 0},
	{400000, 7, 0},
	{133250, 5, 3},
};
struct switch_lut mux_clk_aud_bus_lut[] = {
	{663000, 0, 0},
	{533000, 1, 0},
	{266500, 1, 1},
	{200000, 3, 1},
	{66667, 3, 5},
};
/*================================ SWPLL List =================================*/
struct vclk_switch switch_vdd_cam[] = {
	{MUX_CLK_AUD_CPU, MUX_CLKCMU_AUD_CPU, CLKCMU_AUD_CPU, GATE_CLKCMU_AUD_CPU, MUX_CLKCMU_AUD_CPU_USER, mux_clk_aud_cpu_lut, 4},
	{MUX_CLK_AUD_BUS, MUX_CLKCMU_AUD_BUS, CLKCMU_AUD_BUS, GATE_CLKCMU_AUD_BUS, MUX_CLKCMU_AUD_BUS_USER, mux_clk_aud_bus_lut, 5},
};

/*================================ VCLK List =================================*/
unsigned int cmucal_vclk_size = 1190;
struct vclk cmucal_vclk_list[] = {

/* DVFS VCLK*/
	CMUCAL_VCLK(VCLK_VDD_MIF, cmucal_vclk_vdd_mif_lut, cmucal_vclk_vdd_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_CAM, cmucal_vclk_vdd_cam_lut, cmucal_vclk_vdd_cam, NULL, switch_vdd_cam),
	CMUCAL_VCLK(VCLK_VDD_CPUCL2, cmucal_vclk_vdd_cpucl2_lut, cmucal_vclk_vdd_cpucl2, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_CPUCL0, cmucal_vclk_vdd_cpucl0_lut, cmucal_vclk_vdd_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_CPUCL1, cmucal_vclk_vdd_cpucl1_lut, cmucal_vclk_vdd_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_VDD_INT_CMU, cmucal_vclk_vdd_int_cmu_lut, cmucal_vclk_vdd_int_cmu, NULL, NULL),

/* SPECIAL VCLK*/
	CMUCAL_VCLK(VCLK_MUX_CLK_ALIVE_I3C_PMIC, cmucal_vclk_mux_clk_alive_i3c_pmic_lut, cmucal_vclk_mux_clk_alive_i3c_pmic, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_ALIVE_BUS, cmucal_vclk_clkcmu_alive_bus_lut, cmucal_vclk_clkcmu_alive_bus, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_AUD_DSIF, cmucal_vclk_mux_clk_aud_dsif_lut, cmucal_vclk_mux_clk_aud_dsif, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_BUS0_CMUREF, cmucal_vclk_mux_bus0_cmuref_lut, cmucal_vclk_mux_bus0_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_CMU_BOOST, cmucal_vclk_mux_clkcmu_cmu_boost_lut, cmucal_vclk_mux_clkcmu_cmu_boost, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_BUS1_CMUREF, cmucal_vclk_mux_bus1_cmuref_lut, cmucal_vclk_mux_bus1_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_BUS2_CMUREF, cmucal_vclk_mux_bus2_cmuref_lut, cmucal_vclk_mux_bus2_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLK_CMGP_ADC, cmucal_vclk_clk_cmgp_adc_lut, cmucal_vclk_clk_cmgp_adc, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CMGP_ADC, cmucal_vclk_clkcmu_cmgp_adc_lut, cmucal_vclk_clkcmu_cmgp_adc, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CORE_CMUREF, cmucal_vclk_mux_core_cmuref_lut, cmucal_vclk_mux_core_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CPUCL0_CMUREF, cmucal_vclk_mux_cpucl0_cmuref_lut, cmucal_vclk_mux_cpucl0_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_CMU_BOOST_CPU, cmucal_vclk_mux_clkcmu_cmu_boost_cpu_lut, cmucal_vclk_mux_clkcmu_cmu_boost_cpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CPUCL1_CMUREF, cmucal_vclk_mux_cpucl1_cmuref_lut, cmucal_vclk_mux_cpucl1_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CPUCL2_CMUREF, cmucal_vclk_mux_cpucl2_cmuref_lut, cmucal_vclk_mux_cpucl2_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_DSU_CMUREF, cmucal_vclk_mux_dsu_cmuref_lut, cmucal_vclk_mux_dsu_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLK_HSI0_USB31DRD, cmucal_vclk_mux_clk_hsi0_usb31drd_lut, cmucal_vclk_mux_clk_hsi0_usb31drd, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_MIF_CMUREF, cmucal_vclk_mux_mif_cmuref_lut, cmucal_vclk_mux_mif_cmuref, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_CMU_BOOST_MIF, cmucal_vclk_mux_clkcmu_cmu_boost_mif_lut, cmucal_vclk_mux_clkcmu_cmu_boost_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_G3D_SHADER, cmucal_vclk_clkcmu_g3d_shader_lut, cmucal_vclk_clkcmu_g3d_shader, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_ALIVE_DBGCORE_UART, cmucal_vclk_div_clk_alive_dbgcore_uart_lut, cmucal_vclk_div_clk_alive_dbgcore_uart, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_I2C0, cmucal_vclk_div_clk_cmgp_xx_lut, cmucal_vclk_div_clk_cmgp_i2c0, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CMGP_PERI, cmucal_vclk_clkcmu_cmgp_peri_lut, cmucal_vclk_clkcmu_cmgp_peri, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI1, cmucal_vclk_div_clk_cmgp_xx_lut, cmucal_vclk_div_clk_cmgp_usi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI0, cmucal_vclk_div_clk_cmgp_xx_lut, cmucal_vclk_div_clk_cmgp_usi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI2, cmucal_vclk_div_clk_cmgp_xx_lut, cmucal_vclk_div_clk_cmgp_usi2, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_USI3, cmucal_vclk_div_clk_cmgp_xx_lut, cmucal_vclk_div_clk_cmgp_usi3, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_I2C1, cmucal_vclk_div_clk_cmgp_xx_lut, cmucal_vclk_div_clk_cmgp_i2c1, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_I2C2, cmucal_vclk_div_clk_cmgp_xx_lut, cmucal_vclk_div_clk_cmgp_i2c2, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_I2C3, cmucal_vclk_div_clk_cmgp_xx_lut, cmucal_vclk_div_clk_cmgp_i2c3, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CMGP_I3C, cmucal_vclk_div_clk_cmgp_xx_lut, cmucal_vclk_div_clk_cmgp_i3c, NULL, NULL),
	CMUCAL_VCLK(VCLK_MUX_CLKCMU_HPM, cmucal_vclk_mux_clkcmu_hpm_lut, cmucal_vclk_mux_clkcmu_hpm, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK0, cmucal_vclk_clkcmu_cis_clk0_lut, cmucal_vclk_clkcmu_cis_clk0, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK1, cmucal_vclk_clkcmu_cis_clk1_lut, cmucal_vclk_clkcmu_cis_clk1, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK2, cmucal_vclk_clkcmu_cis_clk2_lut, cmucal_vclk_clkcmu_cis_clk2, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK3, cmucal_vclk_clkcmu_cis_clk3_lut, cmucal_vclk_clkcmu_cis_clk3, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK4, cmucal_vclk_clkcmu_cis_clk4_lut, cmucal_vclk_clkcmu_cis_clk4, NULL, NULL),
	CMUCAL_VCLK(VCLK_CLKCMU_CIS_CLK5, cmucal_vclk_clkcmu_cis_clk5_lut, cmucal_vclk_clkcmu_cis_clk5, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CPUCL0_SHORTSTOP_CORE, cmucal_vclk_div_clk_cpucl0_shortstop_core_lut, cmucal_vclk_div_clk_cpucl0_shortstop_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CPUCL1_SHORTSTOP_CORE, cmucal_vclk_div_clk_cpucl1_shortstop_core_lut, cmucal_vclk_div_clk_cpucl1_shortstop_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CPUCL1_HTU, cmucal_vclk_div_clk_cpucl1_htu_lut, cmucal_vclk_div_clk_cpucl1_htu, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CPUCL2_SHORTSTOP_CORE, cmucal_vclk_div_clk_cpucl2_shortstop_core_lut, cmucal_vclk_div_clk_cpucl2_shortstop_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_CPUCL2_HTU, cmucal_vclk_div_clk_cpucl2_htu_lut, cmucal_vclk_div_clk_cpucl2_htu, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_DSU_SHORTSTOP_CLUSTER, cmucal_vclk_div_clk_dsu_shortstop_cluster_lut, cmucal_vclk_div_clk_dsu_shortstop_cluster, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_USI00_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_usi00_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_USI01_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_usi01_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_USI02_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_usi02_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_USI03_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_usi03_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_USI04_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_usi04_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_USI05_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_usi05_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_UART_DBG, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_uart_dbg, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_USI13_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_usi13_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_USI14_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_usi14_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC0_USI15_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric0_usi15_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_UART_BT, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric1_uart_bt, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_USI11_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric1_usi11_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_USI12_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric1_usi12_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_USI16_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric1_usi16_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_USI17_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric1_usi17_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC1_USI18_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric1_usi18_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC2_USI06_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric2_usi06_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC2_USI07_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric2_usi07_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC2_USI08_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric2_usi08_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC2_USI09_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric2_usi09_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_PERIC2_USI10_USI, cmucal_vclk_div_clk_pericx_usixx_usi_lut, cmucal_vclk_div_clk_peric2_usi10_usi, NULL, NULL),
	CMUCAL_VCLK(VCLK_DIV_CLK_TOP_HSI0_BUS, cmucal_vclk_div_clk_top_hsi0_bus_lut, cmucal_vclk_div_clk_top_hsi0_bus, NULL, NULL),

/* COMMON VCLK*/
	CMUCAL_VCLK(VCLK_BLK_AUD, cmucal_vclk_blk_aud_lut, cmucal_vclk_blk_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CMU, cmucal_vclk_blk_cmu_lut, cmucal_vclk_blk_cmu, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_S2D, cmucal_vclk_blk_s2d_lut, cmucal_vclk_blk_s2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_ALIVE, cmucal_vclk_blk_alive_lut, cmucal_vclk_blk_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL0, cmucal_vclk_blk_cpucl0_lut, cmucal_vclk_blk_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL1, cmucal_vclk_blk_cpucl1_lut, cmucal_vclk_blk_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL2, cmucal_vclk_blk_cpucl2_lut, cmucal_vclk_blk_cpucl2, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DSU, cmucal_vclk_blk_dsu_lut, cmucal_vclk_blk_dsu, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_G3D, cmucal_vclk_blk_g3d_lut, cmucal_vclk_blk_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_HSI0, cmucal_vclk_blk_hsi0_lut, cmucal_vclk_blk_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_PERIC0, cmucal_vclk_blk_peric0_lut, cmucal_vclk_blk_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_PERIC1, cmucal_vclk_blk_peric1_lut, cmucal_vclk_blk_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_PERIC2, cmucal_vclk_blk_peric2_lut, cmucal_vclk_blk_peric2, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_VTS, cmucal_vclk_blk_vts_lut, cmucal_vclk_blk_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_BUS0, cmucal_vclk_blk_bus0_lut, cmucal_vclk_blk_bus0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_BUS1, cmucal_vclk_blk_bus1_lut, cmucal_vclk_blk_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_BUS2, cmucal_vclk_blk_bus2_lut, cmucal_vclk_blk_bus2, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CORE, cmucal_vclk_blk_core_lut, cmucal_vclk_blk_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CPUCL0_GLB, cmucal_vclk_blk_cpucl0_glb_lut, cmucal_vclk_blk_cpucl0_glb, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_CSIS, cmucal_vclk_blk_csis_lut, cmucal_vclk_blk_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DNS, cmucal_vclk_blk_dns_lut, cmucal_vclk_blk_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DPUB, cmucal_vclk_blk_dpub_lut, cmucal_vclk_blk_dpub, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DPUF0, cmucal_vclk_blk_dpuf0_lut, cmucal_vclk_blk_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_DPUF1, cmucal_vclk_blk_dpuf1_lut, cmucal_vclk_blk_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_ITP, cmucal_vclk_blk_itp_lut, cmucal_vclk_blk_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_LME, cmucal_vclk_blk_lme_lut, cmucal_vclk_blk_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_M2M, cmucal_vclk_blk_m2m_lut, cmucal_vclk_blk_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_MCFP0, cmucal_vclk_blk_mcfp0_lut, cmucal_vclk_blk_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_MCFP1, cmucal_vclk_blk_mcfp1_lut, cmucal_vclk_blk_mcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_MCSC, cmucal_vclk_blk_mcsc_lut, cmucal_vclk_blk_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_MFC0, cmucal_vclk_blk_mfc0_lut, cmucal_vclk_blk_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_MFC1, cmucal_vclk_blk_mfc1_lut, cmucal_vclk_blk_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_NPU, cmucal_vclk_blk_npu_lut, cmucal_vclk_blk_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_NPU01, cmucal_vclk_blk_npu01_lut, cmucal_vclk_blk_npu01, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_NPU10, cmucal_vclk_blk_npu10_lut, cmucal_vclk_blk_npu10, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_NPUS, cmucal_vclk_blk_npus_lut, cmucal_vclk_blk_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_PERIS, cmucal_vclk_blk_peris_lut, cmucal_vclk_blk_peris, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_SSP, cmucal_vclk_blk_ssp_lut, cmucal_vclk_blk_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_TAA, cmucal_vclk_blk_taa_lut, cmucal_vclk_blk_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_VPC, cmucal_vclk_blk_vpc_lut, cmucal_vclk_blk_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_VPD, cmucal_vclk_blk_vpd_lut, cmucal_vclk_blk_vpd, NULL, NULL),
	CMUCAL_VCLK(VCLK_BLK_YUVPP, cmucal_vclk_blk_yuvpp_lut, cmucal_vclk_blk_yuvpp, NULL, NULL),

/* GATE VCLK*/
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_APM, NULL, cmucal_vclk_ip_lhs_axi_d_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_APM, NULL, cmucal_vclk_ip_lhm_axi_p_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_ALIVE, NULL, cmucal_vclk_ip_wdt_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_ALIVE, NULL, cmucal_vclk_ip_sysreg_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_AP, NULL, cmucal_vclk_ip_mailbox_apm_ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_PMU_ALIVE, NULL, cmucal_vclk_ip_apbif_pmu_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_INTMEM, NULL, cmucal_vclk_ip_intmem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_G_SCAN2DRAM, NULL, cmucal_vclk_ip_lhs_axi_g_scan2dram, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PMU_INTR_GEN, NULL, cmucal_vclk_ip_pmu_intr_gen, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PEM, NULL, cmucal_vclk_ip_pem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP_ALIVE, NULL, cmucal_vclk_ip_xiu_dp_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ALIVE_CMU_ALIVE, NULL, cmucal_vclk_ip_alive_cmu_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GREBEINTEGRATION, NULL, cmucal_vclk_ip_grebeintegration, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_ALIVE, NULL, cmucal_vclk_ip_gpio_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_TOP_RTC, NULL, cmucal_vclk_ip_apbif_top_rtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SS_DBGCORE, NULL, cmucal_vclk_ip_ss_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DTZPC_ALIVE, NULL, cmucal_vclk_ip_dtzpc_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_DBGCORE, NULL, cmucal_vclk_ip_mailbox_ap_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_LP_VTS, NULL, cmucal_vclk_ip_lhs_axi_lp_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_G_DBGCORE, NULL, cmucal_vclk_ip_lhs_axi_g_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_RTC, NULL, cmucal_vclk_ip_apbif_rtc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_C_CMGP, NULL, cmucal_vclk_ip_lhs_axi_c_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_ALIVE, NULL, cmucal_vclk_ip_vgen_lite_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ROM_CRC32_HOST, NULL, cmucal_vclk_ip_rom_crc32_host, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C_PMIC, NULL, cmucal_vclk_ip_i3c_pmic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_C_MODEM, NULL, cmucal_vclk_ip_lhm_axi_c_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_CP, NULL, cmucal_vclk_ip_mailbox_apm_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_CP, NULL, cmucal_vclk_ip_mailbox_ap_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_CP_S, NULL, cmucal_vclk_ip_mailbox_ap_cp_s, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_C_VTS, NULL, cmucal_vclk_ip_lhm_axi_c_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_SYSREG_VGPIO2AP, NULL, cmucal_vclk_ip_apbif_sysreg_vgpio2ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_SYSREG_VGPIO2APM, NULL, cmucal_vclk_ip_apbif_sysreg_vgpio2apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_SYSREG_VGPIO2PMU, NULL, cmucal_vclk_ip_apbif_sysreg_vgpio2pmu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_P_ALIVE, NULL, cmucal_vclk_ip_sweeper_p_alive, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CLKMON, NULL, cmucal_vclk_ip_clkmon, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DBGCORE_UART, NULL, cmucal_vclk_ip_dbgcore_uart, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DOUBLE_IP_BATCHER, NULL, cmucal_vclk_ip_double_ip_batcher, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HW_SCANDUMP_CLKSTOP_CTRL, NULL, cmucal_vclk_ip_hw_scandump_clkstop_ctrl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AUD_CMU_AUD, NULL, cmucal_vclk_ip_aud_cmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_AUD, NULL, cmucal_vclk_ip_lhs_axi_d_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_AUD, NULL, cmucal_vclk_ip_ppmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_AUD, NULL, cmucal_vclk_ip_sysreg_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ABOX, NULL, cmucal_vclk_ip_abox, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_AUD, NULL, cmucal_vclk_ip_lhm_axi_p_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERI_AXI_ASB, NULL, cmucal_vclk_ip_peri_axi_asb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_AUD, NULL, cmucal_vclk_ip_wdt_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SMMU_AUD, NULL, cmucal_vclk_ip_smmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SMMU_AUD, NULL, cmucal_vclk_ip_ad_apb_smmu_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SMMU_AUD_S, NULL, cmucal_vclk_ip_ad_apb_smmu_aud_s, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_AUD, NULL, cmucal_vclk_ip_vgen_lite_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SMMU_AUD_NS1, NULL, cmucal_vclk_ip_ad_apb_smmu_aud_ns1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AUD0, NULL, cmucal_vclk_ip_mailbox_aud0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AUD1, NULL, cmucal_vclk_ip_mailbox_aud1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_AUD, NULL, cmucal_vclk_ip_d_tzpc_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_HSI0AUD, NULL, cmucal_vclk_ip_lhm_axi_d_hsi0aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_AUDHSI0, NULL, cmucal_vclk_ip_lhs_axi_d_audhsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_AUDVTS, NULL, cmucal_vclk_ip_lhs_axi_d_audvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AXI_US_32TO128, NULL, cmucal_vclk_ip_axi_us_32to128, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_AUD, NULL, cmucal_vclk_ip_trex_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AUD2, NULL, cmucal_vclk_ip_mailbox_aud2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AUD3, NULL, cmucal_vclk_ip_mailbox_aud3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_D_AUDVTS, NULL, cmucal_vclk_ip_baaw_d_audvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUS0_CMU_BUS0, NULL, cmucal_vclk_ip_bus0_cmu_bus0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_BUS0, NULL, cmucal_vclk_ip_sysreg_bus0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D0_BUS0, NULL, cmucal_vclk_ip_trex_d0_bus0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MIF0, NULL, cmucal_vclk_ip_lhs_axi_p_mif0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MIF1, NULL, cmucal_vclk_ip_lhs_axi_p_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MIF2, NULL, cmucal_vclk_ip_lhs_axi_p_mif2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MIF3, NULL, cmucal_vclk_ip_lhs_axi_p_mif3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_NPU10, NULL, cmucal_vclk_ip_lhs_axi_p_npu10, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_VPC, NULL, cmucal_vclk_ip_lhs_axi_p_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_NPUS, NULL, cmucal_vclk_ip_lhs_axi_p_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_NPU01, NULL, cmucal_vclk_ip_lhs_axi_p_npu01, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_PERISGIC, NULL, cmucal_vclk_ip_lhs_axi_p_perisgic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_NPU00, NULL, cmucal_vclk_ip_lhs_axi_p_npu00, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_P_BUS0, NULL, cmucal_vclk_ip_trex_p_bus0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D1_BUS0, NULL, cmucal_vclk_ip_trex_d1_bus0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D1_VPC, NULL, cmucal_vclk_ip_lhm_acel_d1_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D2_VPC, NULL, cmucal_vclk_ip_lhm_acel_d2_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_NPUS, NULL, cmucal_vclk_ip_lhm_axi_d0_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_BUS0, NULL, cmucal_vclk_ip_d_tzpc_bus0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_DBG_G_BUS0, NULL, cmucal_vclk_ip_lhs_dbg_g_bus0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D2_NPUS, NULL, cmucal_vclk_ip_lhm_axi_d2_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D0_VPC, NULL, cmucal_vclk_ip_lhm_acel_d0_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_NPUS, NULL, cmucal_vclk_ip_lhm_axi_d1_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_CMUTOPC, NULL, cmucal_vclk_ip_busif_cmutopc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CACHEAID_BUS0, NULL, cmucal_vclk_ip_cacheaid_bus0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_VPC, NULL, cmucal_vclk_ip_baaw_p_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_PERIC0, NULL, cmucal_vclk_ip_lhs_axi_p_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_PERIC2, NULL, cmucal_vclk_ip_lhs_axi_p_peric2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCSFR_WR_SMC, NULL, cmucal_vclk_ip_asyncsfr_wr_smc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUS1_CMU_BUS1, NULL, cmucal_vclk_ip_bus1_cmu_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DIT, NULL, cmucal_vclk_ip_ad_apb_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_BUS1, NULL, cmucal_vclk_ip_d_tzpc_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DIT, NULL, cmucal_vclk_ip_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_DPUB, NULL, cmucal_vclk_ip_lhs_axi_p_dpub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_HSI0, NULL, cmucal_vclk_ip_lhs_axi_p_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_VTS, NULL, cmucal_vclk_ip_lhs_axi_p_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_DBG_G_BUS1, NULL, cmucal_vclk_ip_lhs_dbg_g_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDMA, NULL, cmucal_vclk_ip_qe_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PDMA, NULL, cmucal_vclk_ip_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_SPDMA, NULL, cmucal_vclk_ip_qe_spdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SPDMA, NULL, cmucal_vclk_ip_spdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_BUS1, NULL, cmucal_vclk_ip_sysreg_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_BUS1, NULL, cmucal_vclk_ip_trex_d_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_P_BUS1, NULL, cmucal_vclk_ip_trex_p_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_RB_BUS1, NULL, cmucal_vclk_ip_trex_rb_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D0_BUS1, NULL, cmucal_vclk_ip_xiu_d0_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_DPUF0, NULL, cmucal_vclk_ip_lhm_axi_d0_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D_HSI0, NULL, cmucal_vclk_ip_lhm_acel_d_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_DPUF0, NULL, cmucal_vclk_ip_lhm_axi_d1_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_APM, NULL, cmucal_vclk_ip_lhm_axi_d_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_VTS, NULL, cmucal_vclk_ip_lhm_axi_d_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_VGEN_PDMA, NULL, cmucal_vclk_ip_ad_apb_vgen_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_PDMA, NULL, cmucal_vclk_ip_ad_apb_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SPDMA, NULL, cmucal_vclk_ip_ad_apb_spdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_ACVPS, NULL, cmucal_vclk_ip_ad_apb_sysmmu_acvps, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_DIT, NULL, cmucal_vclk_ip_ad_apb_sysmmu_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_SBIC, NULL, cmucal_vclk_ip_ad_apb_sysmmu_sbic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_BUS1, NULL, cmucal_vclk_ip_vgen_lite_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_P_VTS, NULL, cmucal_vclk_ip_baaw_p_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_S2_ACVPS, NULL, cmucal_vclk_ip_sysmmu_s2_acvps, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_S2_DIT, NULL, cmucal_vclk_ip_sysmmu_s2_dit, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_S2_SBIC, NULL, cmucal_vclk_ip_sysmmu_s2_sbic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_DPUF0, NULL, cmucal_vclk_ip_lhs_axi_p_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_PDMA, NULL, cmucal_vclk_ip_vgen_pdma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_SBIC, NULL, cmucal_vclk_ip_lhm_axi_d_sbic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_SBIC, NULL, cmucal_vclk_ip_lhs_axi_d_sbic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SBIC, NULL, cmucal_vclk_ip_ad_apb_sbic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SBIC, NULL, cmucal_vclk_ip_sbic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_DPUF1, NULL, cmucal_vclk_ip_lhm_axi_d0_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_DPUF1, NULL, cmucal_vclk_ip_lhm_axi_d1_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_DPUF1, NULL, cmucal_vclk_ip_lhs_axi_p_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUS2_CMU_BUS2, NULL, cmucal_vclk_ip_bus2_cmu_bus2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_ITP, NULL, cmucal_vclk_ip_lhs_axi_p_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_LME, NULL, cmucal_vclk_ip_lhm_axi_d_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_BUS2, NULL, cmucal_vclk_ip_trex_d_bus2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_MCFP0, NULL, cmucal_vclk_ip_lhm_axi_d0_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_YUVPP, NULL, cmucal_vclk_ip_lhm_axi_d_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_DNS, NULL, cmucal_vclk_ip_lhm_axi_d0_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MCFP0, NULL, cmucal_vclk_ip_lhs_axi_p_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MCSC, NULL, cmucal_vclk_ip_lhs_axi_p_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_P_BUS2, NULL, cmucal_vclk_ip_trex_p_bus2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_MCFP0, NULL, cmucal_vclk_ip_lhm_axi_d1_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_LME, NULL, cmucal_vclk_ip_lhs_axi_p_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_MCSC, NULL, cmucal_vclk_ip_lhm_axi_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_DBG_G_BUS2, NULL, cmucal_vclk_ip_lhs_dbg_g_bus2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_TAA, NULL, cmucal_vclk_ip_lhm_axi_d_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_BUS2, NULL, cmucal_vclk_ip_d_tzpc_bus2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_BUS2, NULL, cmucal_vclk_ip_sysreg_bus2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_CSIS, NULL, cmucal_vclk_ip_lhs_axi_p_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_CSIS, NULL, cmucal_vclk_ip_lhm_axi_d1_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_HSI1, NULL, cmucal_vclk_ip_lhs_axi_p_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D0_MCSC, NULL, cmucal_vclk_ip_lhm_acel_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_TAA, NULL, cmucal_vclk_ip_lhs_axi_p_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D_HSI1, NULL, cmucal_vclk_ip_lhm_acel_d_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_CSIS, NULL, cmucal_vclk_ip_lhm_axi_d0_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_YUVPP, NULL, cmucal_vclk_ip_lhs_axi_p_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D2_CSIS, NULL, cmucal_vclk_ip_lhm_axi_d2_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_MCFP1, NULL, cmucal_vclk_ip_lhm_axi_d_mcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_DNS, NULL, cmucal_vclk_ip_lhm_axi_d1_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D2_MCSC, NULL, cmucal_vclk_ip_lhm_axi_d2_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_MFC0, NULL, cmucal_vclk_ip_lhm_axi_d0_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_MFC1, NULL, cmucal_vclk_ip_lhm_axi_d0_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_MFC0, NULL, cmucal_vclk_ip_lhm_axi_d1_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_MFC1, NULL, cmucal_vclk_ip_lhm_axi_d1_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D2_MCFP0, NULL, cmucal_vclk_ip_lhm_axi_d2_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D3_CSIS, NULL, cmucal_vclk_ip_lhm_axi_d3_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D3_MCFP0, NULL, cmucal_vclk_ip_lhm_axi_d3_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_M2M, NULL, cmucal_vclk_ip_lhs_axi_p_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MFC0, NULL, cmucal_vclk_ip_lhs_axi_p_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MFC1, NULL, cmucal_vclk_ip_lhs_axi_p_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_SSP, NULL, cmucal_vclk_ip_lhs_axi_p_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D_SSP, NULL, cmucal_vclk_ip_lhm_acel_d_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D_M2M, NULL, cmucal_vclk_ip_lhm_acel_d_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_PERIC1, NULL, cmucal_vclk_ip_lhs_axi_p_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CMGP_CMU_CMGP, NULL, cmucal_vclk_ip_cmgp_cmu_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADC_CMGP, NULL, cmucal_vclk_ip_adc_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_CMGP, NULL, cmucal_vclk_ip_gpio_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP0, NULL, cmucal_vclk_ip_i2c_cmgp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP1, NULL, cmucal_vclk_ip_i2c_cmgp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP2, NULL, cmucal_vclk_ip_i2c_cmgp2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I2C_CMGP3, NULL, cmucal_vclk_ip_i2c_cmgp3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP, NULL, cmucal_vclk_ip_sysreg_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP0, NULL, cmucal_vclk_ip_usi_cmgp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP1, NULL, cmucal_vclk_ip_usi_cmgp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP2, NULL, cmucal_vclk_ip_usi_cmgp2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI_CMGP3, NULL, cmucal_vclk_ip_usi_cmgp3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2PMU_AP, NULL, cmucal_vclk_ip_sysreg_cmgp2pmu_ap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CMGP, NULL, cmucal_vclk_ip_d_tzpc_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_C_CMGP, NULL, cmucal_vclk_ip_lhm_axi_c_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2APM, NULL, cmucal_vclk_ip_sysreg_cmgp2apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_I3C_CMGP, NULL, cmucal_vclk_ip_i3c_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CMGP2CP, NULL, cmucal_vclk_ip_sysreg_cmgp2cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBIF_GPIO_CMGP, NULL, cmucal_vclk_ip_apbif_gpio_cmgp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CORE_CMU_CORE, NULL, cmucal_vclk_ip_core_cmu_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CORE, NULL, cmucal_vclk_ip_sysreg_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MPACE2AXI_0, NULL, cmucal_vclk_ip_mpace2axi_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MPACE2AXI_1, NULL, cmucal_vclk_ip_mpace2axi_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_DEBUG_CCI, NULL, cmucal_vclk_ip_ppc_debug_cci, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_P0_CORE, NULL, cmucal_vclk_ip_trex_p0_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ATB_T_BDU, NULL, cmucal_vclk_ip_lhs_atb_t_bdu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BDU, NULL, cmucal_vclk_ip_bdu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_P1_CORE, NULL, cmucal_vclk_ip_trex_p1_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_G3D, NULL, cmucal_vclk_ip_lhs_axi_p_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_CPUCL0, NULL, cmucal_vclk_ip_lhs_axi_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACE_D0_G3D, NULL, cmucal_vclk_ip_lhm_ace_d0_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACE_D1_G3D, NULL, cmucal_vclk_ip_lhm_ace_d1_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACE_D2_G3D, NULL, cmucal_vclk_ip_lhm_ace_d2_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACE_D3_G3D, NULL, cmucal_vclk_ip_lhm_ace_d3_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_D_CORE, NULL, cmucal_vclk_ip_trex_d_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPCFW_G3D, NULL, cmucal_vclk_ip_ppcfw_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_APM, NULL, cmucal_vclk_ip_lhs_axi_p_apm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CORE, NULL, cmucal_vclk_ip_d_tzpc_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_G3D0, NULL, cmucal_vclk_ip_ppc_g3d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_G3D1, NULL, cmucal_vclk_ip_ppc_g3d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_G3D2, NULL, cmucal_vclk_ip_ppc_g3d2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_G3D3, NULL, cmucal_vclk_ip_ppc_g3d3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_IRPS0, NULL, cmucal_vclk_ip_ppc_irps0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_IRPS1, NULL, cmucal_vclk_ip_ppc_irps1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MPACE_ASB_D0_MIF, NULL, cmucal_vclk_ip_mpace_asb_d0_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MPACE_ASB_D1_MIF, NULL, cmucal_vclk_ip_mpace_asb_d1_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MPACE_ASB_D2_MIF, NULL, cmucal_vclk_ip_mpace_asb_d2_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MPACE_ASB_D3_MIF, NULL, cmucal_vclk_ip_mpace_asb_d3_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AXI_ASB_CSSYS, NULL, cmucal_vclk_ip_axi_asb_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_CSSYS, NULL, cmucal_vclk_ip_lhm_axi_g_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CCI, NULL, cmucal_vclk_ip_cci, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_G3D0, NULL, cmucal_vclk_ip_ppmu_g3d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_G3D1, NULL, cmucal_vclk_ip_ppmu_g3d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_G3D2, NULL, cmucal_vclk_ip_ppmu_g3d2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_G3D3, NULL, cmucal_vclk_ip_ppmu_g3d3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_G3D0, NULL, cmucal_vclk_ip_sysmmu_g3d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_G3D1, NULL, cmucal_vclk_ip_sysmmu_g3d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_G3D2, NULL, cmucal_vclk_ip_sysmmu_g3d2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_G3D3, NULL, cmucal_vclk_ip_sysmmu_g3d3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_CORE, NULL, cmucal_vclk_ip_xiu_d_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APB_ASYNC_SYSMMU_G3D0, NULL, cmucal_vclk_ip_apb_async_sysmmu_g3d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ACE_SLICE_G3D0, NULL, cmucal_vclk_ip_ace_slice_g3d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ACE_SLICE_G3D1, NULL, cmucal_vclk_ip_ace_slice_g3d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ACE_SLICE_G3D2, NULL, cmucal_vclk_ip_ace_slice_g3d2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ACE_SLICE_G3D3, NULL, cmucal_vclk_ip_ace_slice_g3d3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_MODEM, NULL, cmucal_vclk_ip_lhm_axi_d0_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_MODEM, NULL, cmucal_vclk_ip_lhm_axi_d1_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACEL_D2_MODEM, NULL, cmucal_vclk_ip_lhm_acel_d2_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_AUD, NULL, cmucal_vclk_ip_lhm_axi_d_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_AUD, NULL, cmucal_vclk_ip_lhs_axi_p_aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MODEM, NULL, cmucal_vclk_ip_lhs_axi_p_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_IRPS2, NULL, cmucal_vclk_ip_ppc_irps2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_IRPS3, NULL, cmucal_vclk_ip_ppc_irps3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_CPUCL0_0, NULL, cmucal_vclk_ip_ppc_cpucl0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_CPUCL0_1, NULL, cmucal_vclk_ip_ppc_cpucl0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_CPUCL0_0, NULL, cmucal_vclk_ip_ppmu_cpucl0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_CPUCL0_1, NULL, cmucal_vclk_ip_ppmu_cpucl0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACE_D0_CLUSTER0, NULL, cmucal_vclk_ip_lhm_ace_d0_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ACE_D1_CLUSTER0, NULL, cmucal_vclk_ip_lhm_ace_d1_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_CP, NULL, cmucal_vclk_ip_baaw_cp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_MODEM, NULL, cmucal_vclk_ip_sysmmu_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_PERIS, NULL, cmucal_vclk_ip_lhs_axi_p_peris, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MIF0, NULL, cmucal_vclk_ip_ppmu_mif0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MIF1, NULL, cmucal_vclk_ip_ppmu_mif1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MIF2, NULL, cmucal_vclk_ip_ppmu_mif2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MIF3, NULL, cmucal_vclk_ip_ppmu_mif3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_MODEM, NULL, cmucal_vclk_ip_vgen_lite_modem, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL0_CMU_CPUCL0, NULL, cmucal_vclk_ip_cpucl0_cmu_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADD_CPUCL0_0, NULL, cmucal_vclk_ip_add_cpucl0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_ADD_CPUCL0_0, NULL, cmucal_vclk_ip_busif_add_cpucl0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_DDD_CPUCL0_0, NULL, cmucal_vclk_ip_busif_ddd_cpucl0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_STR_CPUCL0_0, NULL, cmucal_vclk_ip_busif_str_cpucl0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_CPUCL0, NULL, cmucal_vclk_ip_htu_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_BUSIF_DDD_CPUCL0_0, NULL, cmucal_vclk_ip_hwacg_busif_ddd_cpucl0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDD_CPUCL0_0, NULL, cmucal_vclk_ip_ddd_cpucl0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_APB_G_CLUSTER0, NULL, cmucal_vclk_ip_adm_apb_g_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_INT_ETR, NULL, cmucal_vclk_ip_lhm_axi_g_int_etr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_G_CSSYS, NULL, cmucal_vclk_ip_lhs_axi_g_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_INT_STM, NULL, cmucal_vclk_ip_lhm_axi_g_int_stm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_CPUCL0, NULL, cmucal_vclk_ip_lhm_axi_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TREX_CPUCL0, NULL, cmucal_vclk_ip_trex_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SECJTAG, NULL, cmucal_vclk_ip_secjtag, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_CPUCL0, NULL, cmucal_vclk_ip_bps_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_INT_CSSYS, NULL, cmucal_vclk_ip_lhm_axi_g_int_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_T_BDU, NULL, cmucal_vclk_ip_lhm_atb_t_bdu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_P_CPUCL0, NULL, cmucal_vclk_ip_xiu_p_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CPUCL0, NULL, cmucal_vclk_ip_d_tzpc_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CPUCL0, NULL, cmucal_vclk_ip_sysreg_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_DBGCORE, NULL, cmucal_vclk_ip_lhm_axi_g_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPM_CPUCL0, NULL, cmucal_vclk_ip_busif_hpm_cpucl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_DP_CSSYS, NULL, cmucal_vclk_ip_xiu_dp_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_G_INT_STM, NULL, cmucal_vclk_ip_lhs_axi_g_int_stm, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_INT_DBGCORE, NULL, cmucal_vclk_ip_lhm_axi_g_int_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APB_ASYNC_P_CSSYS_0, NULL, cmucal_vclk_ip_apb_async_p_cssys_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSSYS, NULL, cmucal_vclk_ip_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_G_INT_CSSYS, NULL, cmucal_vclk_ip_lhs_axi_g_int_cssys, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_G_INT_ETR, NULL, cmucal_vclk_ip_lhs_axi_g_int_etr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_G_INT_DBGCORE, NULL, cmucal_vclk_ip_lhs_axi_g_int_dbgcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL0_GLB_CMU_CPUCL0_GLB, NULL, cmucal_vclk_ip_cpucl0_glb_cmu_cpucl0_glb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_CPUCL0_0, NULL, cmucal_vclk_ip_hpm_cpucl0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_CPUCL0_1, NULL, cmucal_vclk_ip_hpm_cpucl0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_CPUCL0_2, NULL, cmucal_vclk_ip_hpm_cpucl0_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_T0_CLUSTER0, NULL, cmucal_vclk_ip_lhm_atb_t0_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_T1_CLUSTER0, NULL, cmucal_vclk_ip_lhm_atb_t1_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_T2_CLUSTER0, NULL, cmucal_vclk_ip_lhm_atb_t2_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_T3_CLUSTER0, NULL, cmucal_vclk_ip_lhm_atb_t3_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_T4_CLUSTER0, NULL, cmucal_vclk_ip_lhm_atb_t4_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_T5_CLUSTER0, NULL, cmucal_vclk_ip_lhm_atb_t5_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_T6_CLUSTER0, NULL, cmucal_vclk_ip_lhm_atb_t6_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_ATB_T7_CLUSTER0, NULL, cmucal_vclk_ip_lhm_atb_t7_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL1_CMU_CPUCL1, NULL, cmucal_vclk_ip_cpucl1_cmu_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADD_CPUCL0_1, NULL, cmucal_vclk_ip_add_cpucl0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_ADD_CPUCL0_1, NULL, cmucal_vclk_ip_busif_add_cpucl0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_DDD_CPUCL0_2, NULL, cmucal_vclk_ip_busif_ddd_cpucl0_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_DDD_CPUCL0_3, NULL, cmucal_vclk_ip_busif_ddd_cpucl0_3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_DDD_CPUCL0_4, NULL, cmucal_vclk_ip_busif_ddd_cpucl0_4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_STR_CPUCL0_1, NULL, cmucal_vclk_ip_busif_str_cpucl0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_CPUCL1, NULL, cmucal_vclk_ip_htu_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_BUSIF_DDD_CPUCL0_2, NULL, cmucal_vclk_ip_hwacg_busif_ddd_cpucl0_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_BUSIF_DDD_CPUCL0_4, NULL, cmucal_vclk_ip_hwacg_busif_ddd_cpucl0_4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL1, NULL, cmucal_vclk_ip_cpucl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_BUSIF_DDD_CPUCL0_3, NULL, cmucal_vclk_ip_hwacg_busif_ddd_cpucl0_3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CPUCL2_CMU_CPUCL2, NULL, cmucal_vclk_ip_cpucl2_cmu_cpucl2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_STR_CPUCL0_2, NULL, cmucal_vclk_ip_busif_str_cpucl0_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADD_CPUCL0_2, NULL, cmucal_vclk_ip_add_cpucl0_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_ADD_CPUCL0_2, NULL, cmucal_vclk_ip_busif_add_cpucl0_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_DDD_CPUCL0_1, NULL, cmucal_vclk_ip_busif_ddd_cpucl0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_CPUCL2, NULL, cmucal_vclk_ip_htu_cpucl2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_BUSIF_DDD_CPUCL0_1, NULL, cmucal_vclk_ip_hwacg_busif_ddd_cpucl0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDD_CPUCL0_1, NULL, cmucal_vclk_ip_ddd_cpucl0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MIPI_PHY_LINK_WRAP, NULL, cmucal_vclk_ip_mipi_phy_link_wrap, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PDP_TOP, NULL, cmucal_vclk_ip_pdp_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDP_STAT_IMG0, NULL, cmucal_vclk_ip_qe_pdp_stat_img0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_CSIS_DMA3, NULL, cmucal_vclk_ip_qe_csis_dma3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDP_STAT_AF0, NULL, cmucal_vclk_ip_qe_pdp_stat_af0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDP_AF1, NULL, cmucal_vclk_ip_qe_pdp_af1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDP_STAT_IMG1, NULL, cmucal_vclk_ip_qe_pdp_stat_img1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_STRP1, NULL, cmucal_vclk_ip_qe_strp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_STRP2, NULL, cmucal_vclk_ip_qe_strp2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_ZSL1, NULL, cmucal_vclk_ip_qe_zsl1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_ZSL2, NULL, cmucal_vclk_ip_qe_zsl2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_PDP_CORE, NULL, cmucal_vclk_ip_ad_apb_pdp_core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_D1, NULL, cmucal_vclk_ip_vgen_lite_d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_D2, NULL, cmucal_vclk_ip_vgen_lite_d2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF0_CSISTAA, NULL, cmucal_vclk_ip_lhs_ast_otf0_csistaa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF1_CSISTAA, NULL, cmucal_vclk_ip_lhs_ast_otf1_csistaa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF2_CSISTAA, NULL, cmucal_vclk_ip_lhs_ast_otf2_csistaa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_INT_VO_PDPCSIS, NULL, cmucal_vclk_ip_lhs_ast_int_vo_pdpcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSIS_CMU_CSIS, NULL, cmucal_vclk_ip_csis_cmu_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_CSIS, NULL, cmucal_vclk_ip_lhs_axi_d0_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_CSIS, NULL, cmucal_vclk_ip_lhs_axi_d1_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_CSIS, NULL, cmucal_vclk_ip_d_tzpc_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CSISX6, NULL, cmucal_vclk_ip_csisx6, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D0, NULL, cmucal_vclk_ip_ppmu_d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D1, NULL, cmucal_vclk_ip_ppmu_d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_CSIS, NULL, cmucal_vclk_ip_sysmmu_d0_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_CSIS, NULL, cmucal_vclk_ip_sysmmu_d1_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_CSIS, NULL, cmucal_vclk_ip_sysreg_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_D0, NULL, cmucal_vclk_ip_vgen_lite_d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_CSIS, NULL, cmucal_vclk_ip_lhm_axi_p_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_INT_VO_PDPCSIS, NULL, cmucal_vclk_ip_lhm_ast_int_vo_pdpcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_ZOTF0_TAACSIS, NULL, cmucal_vclk_ip_lhm_ast_zotf0_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_ZOTF1_TAACSIS, NULL, cmucal_vclk_ip_lhm_ast_zotf1_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_ZOTF2_TAACSIS, NULL, cmucal_vclk_ip_lhm_ast_zotf2_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_CSIS0, NULL, cmucal_vclk_ip_ad_apb_csis0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D0_CSIS, NULL, cmucal_vclk_ip_xiu_d0_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D1_CSIS, NULL, cmucal_vclk_ip_xiu_d1_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_CSIS_DMA1, NULL, cmucal_vclk_ip_qe_csis_dma1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_ZSL0, NULL, cmucal_vclk_ip_qe_zsl0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_STRP0, NULL, cmucal_vclk_ip_qe_strp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_SOTF0_TAACSIS, NULL, cmucal_vclk_ip_lhm_ast_sotf0_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_SOTF1_TAACSIS, NULL, cmucal_vclk_ip_lhm_ast_sotf1_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_SOTF2_TAACSIS, NULL, cmucal_vclk_ip_lhm_ast_sotf2_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D2, NULL, cmucal_vclk_ip_ppmu_d2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OIS_MCU_TOP, NULL, cmucal_vclk_ip_ois_mcu_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_OIS_MCU_TOP, NULL, cmucal_vclk_ip_ad_axi_ois_mcu_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_CSIS_DMA0, NULL, cmucal_vclk_ip_qe_csis_dma0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_CSISPERIC1, NULL, cmucal_vclk_ip_lhs_axi_p_csisperic1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF3_CSISTAA, NULL, cmucal_vclk_ip_lhs_ast_otf3_csistaa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_ZOTF3_TAACSIS, NULL, cmucal_vclk_ip_lhm_ast_zotf3_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_SOTF3_TAACSIS, NULL, cmucal_vclk_ip_lhm_ast_sotf3_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_INT_VO_CSISPDP, NULL, cmucal_vclk_ip_lhm_ast_int_vo_csispdp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_INT_VO_CSISPDP, NULL, cmucal_vclk_ip_lhs_ast_int_vo_csispdp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D2_CSIS, NULL, cmucal_vclk_ip_xiu_d2_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D3, NULL, cmucal_vclk_ip_ppmu_d3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D2_CSIS, NULL, cmucal_vclk_ip_sysmmu_d2_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D3_CSIS, NULL, cmucal_vclk_ip_sysmmu_d3_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_CSIS_DMA2, NULL, cmucal_vclk_ip_qe_csis_dma2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D2_CSIS, NULL, cmucal_vclk_ip_lhs_axi_d2_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D3_CSIS, NULL, cmucal_vclk_ip_lhs_axi_d3_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDP_STAT_IMG2, NULL, cmucal_vclk_ip_qe_pdp_stat_img2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_PDP_AF2, NULL, cmucal_vclk_ip_qe_pdp_af2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D4_CSIS, NULL, cmucal_vclk_ip_xiu_d4_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D3_CSIS, NULL, cmucal_vclk_ip_xiu_d3_csis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_STRP, NULL, cmucal_vclk_ip_ad_axi_strp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_INT_OTF0_CSISPDP, NULL, cmucal_vclk_ip_lhm_ast_int_otf0_csispdp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_INT_OTF1_CSISPDP, NULL, cmucal_vclk_ip_lhm_ast_int_otf1_csispdp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_INT_OTF2_CSISPDP, NULL, cmucal_vclk_ip_lhm_ast_int_otf2_csispdp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_INT_OTF3_CSISPDP, NULL, cmucal_vclk_ip_lhm_ast_int_otf3_csispdp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_INT_OTF0_CSISPDP, NULL, cmucal_vclk_ip_lhs_ast_int_otf0_csispdp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_INT_OTF1_CSISPDP, NULL, cmucal_vclk_ip_lhs_ast_int_otf1_csispdp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_INT_OTF2_CSISPDP, NULL, cmucal_vclk_ip_lhs_ast_int_otf2_csispdp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_INT_OTF3_CSISPDP, NULL, cmucal_vclk_ip_lhs_ast_int_otf3_csispdp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_INT_OTF0_PDPCSIS, NULL, cmucal_vclk_ip_lhm_ast_int_otf0_pdpcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_INT_OTF1_PDPCSIS, NULL, cmucal_vclk_ip_lhm_ast_int_otf1_pdpcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_INT_OTF2_PDPCSIS, NULL, cmucal_vclk_ip_lhm_ast_int_otf2_pdpcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_INT_OTF3_PDPCSIS, NULL, cmucal_vclk_ip_lhm_ast_int_otf3_pdpcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_INT_OTF0_PDPCSIS, NULL, cmucal_vclk_ip_lhs_ast_int_otf0_pdpcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_INT_OTF1_PDPCSIS, NULL, cmucal_vclk_ip_lhs_ast_int_otf1_pdpcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_INT_OTF2_PDPCSIS, NULL, cmucal_vclk_ip_lhs_ast_int_otf2_pdpcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_INT_OTF3_PDPCSIS, NULL, cmucal_vclk_ip_lhs_ast_int_otf3_pdpcsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_STRP3, NULL, cmucal_vclk_ip_qe_strp3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_ZSL3, NULL, cmucal_vclk_ip_qe_zsl3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DNS_CMU_DNS, NULL, cmucal_vclk_ip_dns_cmu_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF4_ITPDNS, NULL, cmucal_vclk_ip_lhm_ast_otf4_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D0_DNS, NULL, cmucal_vclk_ip_xiu_d0_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF_TAADNS, NULL, cmucal_vclk_ip_lhm_ast_otf_taadns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_DNS, NULL, cmucal_vclk_ip_sysmmu_d1_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_ITPDNS, NULL, cmucal_vclk_ip_lhm_axi_p_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF3_ITPDNS, NULL, cmucal_vclk_ip_lhm_ast_otf3_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DNS, NULL, cmucal_vclk_ip_d_tzpc_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF_MCFP1DNS, NULL, cmucal_vclk_ip_lhm_ast_otf_mcfp1dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF2_ITPDNS, NULL, cmucal_vclk_ip_lhm_ast_otf2_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DNS0, NULL, cmucal_vclk_ip_ad_apb_dns0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DNS, NULL, cmucal_vclk_ip_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF0_ITPDNS, NULL, cmucal_vclk_ip_lhm_ast_otf0_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF1_ITPDNS, NULL, cmucal_vclk_ip_lhm_ast_otf1_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D1_DNS, NULL, cmucal_vclk_ip_xiu_d1_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF9_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_otf9_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF8_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_otf8_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DNS, NULL, cmucal_vclk_ip_sysreg_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_DNS, NULL, cmucal_vclk_ip_lhs_axi_d0_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D0_DNS, NULL, cmucal_vclk_ip_ppmu_d0_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF5_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_otf5_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF4_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_otf4_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF6_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_otf6_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF0_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_otf0_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF1_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_otf1_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF2_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_otf2_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF3_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_otf3_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF7_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_otf7_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_DNS, NULL, cmucal_vclk_ip_lhs_axi_d1_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_CTL_ITPDNS, NULL, cmucal_vclk_ip_lhm_ast_ctl_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D1_DNS, NULL, cmucal_vclk_ip_ppmu_d1_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_DNS, NULL, cmucal_vclk_ip_sysmmu_d0_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_CTL_DNSITP, NULL, cmucal_vclk_ip_lhs_ast_ctl_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_D0_DNS, NULL, cmucal_vclk_ip_vgen_lite_d0_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_D1_DNS, NULL, cmucal_vclk_ip_vgen_lite_d1_dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPUB_CMU_DPUB, NULL, cmucal_vclk_ip_dpub_cmu_dpub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_DPUB, NULL, cmucal_vclk_ip_lhm_axi_p_dpub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DPUB, NULL, cmucal_vclk_ip_d_tzpc_dpub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DPUB, NULL, cmucal_vclk_ip_sysreg_dpub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPUB, NULL, cmucal_vclk_ip_dpub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DECON_MAIN, NULL, cmucal_vclk_ip_ad_apb_decon_main, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPUF0_CMU_DPUF0, NULL, cmucal_vclk_ip_dpuf0_cmu_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DPUF0, NULL, cmucal_vclk_ip_sysreg_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DPUF0D0, NULL, cmucal_vclk_ip_sysmmu_dpuf0d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_DPUF0, NULL, cmucal_vclk_ip_lhm_axi_p_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_DPUF0, NULL, cmucal_vclk_ip_lhs_axi_d1_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DPUF0_DMA, NULL, cmucal_vclk_ip_ad_apb_dpuf0_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DPUF0D1, NULL, cmucal_vclk_ip_sysmmu_dpuf0d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DPUF0D0, NULL, cmucal_vclk_ip_ppmu_dpuf0d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DPUF0D1, NULL, cmucal_vclk_ip_ppmu_dpuf0d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_DPUF0, NULL, cmucal_vclk_ip_lhs_axi_d0_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPUF0, NULL, cmucal_vclk_ip_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DPUF0, NULL, cmucal_vclk_ip_d_tzpc_dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_DPUF1DPUF0, NULL, cmucal_vclk_ip_lhm_axi_d_dpuf1dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPUF1_CMU_DPUF1, NULL, cmucal_vclk_ip_dpuf1_cmu_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_DPUF1_DMA, NULL, cmucal_vclk_ip_ad_apb_dpuf1_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_DPUF1, NULL, cmucal_vclk_ip_lhs_axi_d1_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DPUF1, NULL, cmucal_vclk_ip_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DPUF1D0, NULL, cmucal_vclk_ip_ppmu_dpuf1d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_DPUF1D1, NULL, cmucal_vclk_ip_ppmu_dpuf1d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_DPUF1, NULL, cmucal_vclk_ip_d_tzpc_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_DPUF1, NULL, cmucal_vclk_ip_lhs_axi_d0_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_DPUF1, NULL, cmucal_vclk_ip_lhm_axi_p_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DPUF1D0, NULL, cmucal_vclk_ip_sysmmu_dpuf1d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_DPUF1D1, NULL, cmucal_vclk_ip_sysmmu_dpuf1d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_DPUF1, NULL, cmucal_vclk_ip_sysreg_dpuf1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_DPUF1DPUF0, NULL, cmucal_vclk_ip_lhs_axi_d_dpuf1dpuf0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CLUSTER0, NULL, cmucal_vclk_ip_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DSU_CMU_DSU, NULL, cmucal_vclk_ip_dsu_cmu_dsu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_IRI_GICCPU_CLUSTER0, NULL, cmucal_vclk_ip_lhm_ast_iri_giccpu_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_ICC_CPUGIC_CLUSTER0, NULL, cmucal_vclk_ip_lhs_ast_icc_cpugic_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ATB_T0_CLUSTER0, NULL, cmucal_vclk_ip_lhs_atb_t0_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ATB_T1_CLUSTER0, NULL, cmucal_vclk_ip_lhs_atb_t1_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ATB_T2_CLUSTER0, NULL, cmucal_vclk_ip_lhs_atb_t2_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ATB_T3_CLUSTER0, NULL, cmucal_vclk_ip_lhs_atb_t3_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ATB_T4_CLUSTER0, NULL, cmucal_vclk_ip_lhs_atb_t4_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ATB_T5_CLUSTER0, NULL, cmucal_vclk_ip_lhs_atb_t5_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ATB_T6_CLUSTER0, NULL, cmucal_vclk_ip_lhs_atb_t6_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ATB_T7_CLUSTER0, NULL, cmucal_vclk_ip_lhs_atb_t7_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_STR_CPUCL0_3, NULL, cmucal_vclk_ip_busif_str_cpucl0_3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_DSU, NULL, cmucal_vclk_ip_htu_dsu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_INSTRRET_CLUSTER0_0, NULL, cmucal_vclk_ip_ppc_instrret_cluster0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_INSTRRET_CLUSTER0_1, NULL, cmucal_vclk_ip_ppc_instrret_cluster0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_INSTRRUN_CLUSTER0_0, NULL, cmucal_vclk_ip_ppc_instrrun_cluster0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_INSTRRUN_CLUSTER0_1, NULL, cmucal_vclk_ip_ppc_instrrun_cluster0_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ACE_US_128TO256_D0_CLUSTER0, NULL, cmucal_vclk_ip_ace_us_128to256_d0_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ACE_US_128TO256_D1_CLUSTER0, NULL, cmucal_vclk_ip_ace_us_128to256_d1_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACE_D0_CLUSTER0, NULL, cmucal_vclk_ip_lhs_ace_d0_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACE_D1_CLUSTER0, NULL, cmucal_vclk_ip_lhs_ace_d1_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_G3D, NULL, cmucal_vclk_ip_lhm_axi_p_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPMG3D, NULL, cmucal_vclk_ip_busif_hpmg3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_G3D, NULL, cmucal_vclk_ip_hpm_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_G3D, NULL, cmucal_vclk_ip_sysreg_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_G3D_CMU_G3D, NULL, cmucal_vclk_ip_g3d_cmu_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_INT_G3D, NULL, cmucal_vclk_ip_lhs_axi_p_int_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_G3D, NULL, cmucal_vclk_ip_vgen_lite_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_INT_G3D, NULL, cmucal_vclk_ip_lhm_axi_p_int_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GRAY2BIN_G3D, NULL, cmucal_vclk_ip_gray2bin_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_G3D, NULL, cmucal_vclk_ip_d_tzpc_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADD_APBIF_G3D, NULL, cmucal_vclk_ip_add_apbif_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADD_G3D, NULL, cmucal_vclk_ip_add_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDD_APBIF_G3D, NULL, cmucal_vclk_ip_ddd_apbif_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPU, NULL, cmucal_vclk_ip_gpu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_STR_G3D, NULL, cmucal_vclk_ip_busif_str_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDD_G3D, NULL, cmucal_vclk_ip_ddd_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_G3D, NULL, cmucal_vclk_ip_htu_g3d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USB31DRD, NULL, cmucal_vclk_ip_usb31drd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DP_LINK, NULL, cmucal_vclk_ip_dp_link, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_HSI0_BUS1, NULL, cmucal_vclk_ip_ppmu_hsi0_bus1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D_HSI0, NULL, cmucal_vclk_ip_lhs_acel_d_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_HSI0, NULL, cmucal_vclk_ip_vgen_lite_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_HSI0, NULL, cmucal_vclk_ip_d_tzpc_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_HSI0, NULL, cmucal_vclk_ip_lhm_axi_p_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_USB, NULL, cmucal_vclk_ip_sysmmu_usb, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_HSI0, NULL, cmucal_vclk_ip_sysreg_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HSI0_CMU_HSI0, NULL, cmucal_vclk_ip_hsi0_cmu_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D1_HSI0, NULL, cmucal_vclk_ip_xiu_d1_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_AUDHSI0, NULL, cmucal_vclk_ip_lhm_axi_d_audhsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_HSI0AUD, NULL, cmucal_vclk_ip_lhs_axi_d_hsi0aud, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D0_HSI0, NULL, cmucal_vclk_ip_xiu_d0_hsi0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_HSI1, NULL, cmucal_vclk_ip_sysmmu_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HSI1_CMU_HSI1, NULL, cmucal_vclk_ip_hsi1_cmu_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PCIE_GEN2, NULL, cmucal_vclk_ip_pcie_gen2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_HSI1, NULL, cmucal_vclk_ip_sysreg_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_HSI1, NULL, cmucal_vclk_ip_gpio_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D_HSI1, NULL, cmucal_vclk_ip_lhs_acel_d_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_HSI1, NULL, cmucal_vclk_ip_lhm_axi_p_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_HSI1, NULL, cmucal_vclk_ip_xiu_d_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_P_HSI1, NULL, cmucal_vclk_ip_xiu_p_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_HSI1, NULL, cmucal_vclk_ip_ppmu_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_HSI1, NULL, cmucal_vclk_ip_vgen_lite_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PCIE_IA_GEN2, NULL, cmucal_vclk_ip_pcie_ia_gen2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_HSI1, NULL, cmucal_vclk_ip_d_tzpc_hsi1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PCIE_GEN4_0, NULL, cmucal_vclk_ip_pcie_gen4_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PCIE_IA_GEN4_0, NULL, cmucal_vclk_ip_pcie_ia_gen4_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_UFS_EMBD, NULL, cmucal_vclk_ip_ufs_embd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MMC_CARD, NULL, cmucal_vclk_ip_mmc_card, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ITP_CMU_ITP, NULL, cmucal_vclk_ip_itp_cmu_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_ITP, NULL, cmucal_vclk_ip_sysreg_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_ITP, NULL, cmucal_vclk_ip_d_tzpc_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_CTL_ITPDNS, NULL, cmucal_vclk_ip_lhs_ast_ctl_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ITP, NULL, cmucal_vclk_ip_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF4_ITPDNS, NULL, cmucal_vclk_ip_lhs_ast_otf4_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_CTL_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_ctl_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF4_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_otf4_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF3_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_otf3_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF5_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_otf5_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF6_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_otf6_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF7_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_otf7_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF8_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_otf8_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF9_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_otf9_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF2_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_otf2_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_ITP, NULL, cmucal_vclk_ip_lhm_axi_p_itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF0_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_otf0_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF1_DNSITP, NULL, cmucal_vclk_ip_lhm_ast_otf1_dnsitp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF1_ITPDNS, NULL, cmucal_vclk_ip_lhs_ast_otf1_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF2_ITPDNS, NULL, cmucal_vclk_ip_lhs_ast_otf2_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF3_ITPDNS, NULL, cmucal_vclk_ip_lhs_ast_otf3_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_ITP0, NULL, cmucal_vclk_ip_ad_apb_itp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF0_ITPDNS, NULL, cmucal_vclk_ip_lhs_ast_otf0_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_ITPDNS, NULL, cmucal_vclk_ip_lhs_axi_p_itpdns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF_MCFP1ITP, NULL, cmucal_vclk_ip_lhm_ast_otf_mcfp1itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF_ITPMCSC, NULL, cmucal_vclk_ip_lhs_ast_otf_itpmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LME_CMU_LME, NULL, cmucal_vclk_ip_lme_cmu_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D_LME, NULL, cmucal_vclk_ip_sysmmu_d_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_LME, NULL, cmucal_vclk_ip_sysreg_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_LME, NULL, cmucal_vclk_ip_d_tzpc_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LME, NULL, cmucal_vclk_ip_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_LME, NULL, cmucal_vclk_ip_ppmu_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_LME, NULL, cmucal_vclk_ip_lhs_axi_d_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_LME, NULL, cmucal_vclk_ip_ad_apb_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_LME, NULL, cmucal_vclk_ip_vgen_lite_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_LME, NULL, cmucal_vclk_ip_lhm_axi_p_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_LME, NULL, cmucal_vclk_ip_xiu_d_lme, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_M2M_CMU_M2M, NULL, cmucal_vclk_ip_m2m_cmu_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D_M2M, NULL, cmucal_vclk_ip_lhs_acel_d_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_M2M, NULL, cmucal_vclk_ip_as_apb_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D_M2M, NULL, cmucal_vclk_ip_sysmmu_d_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_M2M, NULL, cmucal_vclk_ip_xiu_d_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_JPEG0, NULL, cmucal_vclk_ip_qe_jpeg0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_M2M, NULL, cmucal_vclk_ip_d_tzpc_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_M2M, NULL, cmucal_vclk_ip_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_JSQZ, NULL, cmucal_vclk_ip_qe_jsqz, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_M2M, NULL, cmucal_vclk_ip_qe_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_M2M, NULL, cmucal_vclk_ip_lhm_axi_p_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_ASTC, NULL, cmucal_vclk_ip_qe_astc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_M2M, NULL, cmucal_vclk_ip_vgen_lite_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D_M2M, NULL, cmucal_vclk_ip_ppmu_d_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_M2M, NULL, cmucal_vclk_ip_sysreg_m2m, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASTC, NULL, cmucal_vclk_ip_astc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_JPEG0, NULL, cmucal_vclk_ip_jpeg0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_JSQZ, NULL, cmucal_vclk_ip_jsqz, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_JPEG1, NULL, cmucal_vclk_ip_jpeg1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_JPEG1, NULL, cmucal_vclk_ip_qe_jpeg1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCFP0_CMU_MCFP0, NULL, cmucal_vclk_ip_mcfp0_cmu_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_MCFP0, NULL, cmucal_vclk_ip_lhs_axi_d0_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MCFP0, NULL, cmucal_vclk_ip_lhm_axi_p_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF0_MCFP0MCFP1, NULL, cmucal_vclk_ip_lhs_ast_otf0_mcfp0mcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF1_MCFP0MCFP1, NULL, cmucal_vclk_ip_lhs_ast_otf1_mcfp0mcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF0_MCFP1MCFP0, NULL, cmucal_vclk_ip_lhm_ast_otf0_mcfp1mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MCFP0, NULL, cmucal_vclk_ip_sysreg_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MCFP0, NULL, cmucal_vclk_ip_d_tzpc_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_MCFP0, NULL, cmucal_vclk_ip_vgen_lite_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D0_MCFP0, NULL, cmucal_vclk_ip_ppmu_d0_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_MCFP0, NULL, cmucal_vclk_ip_sysmmu_d0_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APB_ASYNC_MCFP0_0, NULL, cmucal_vclk_ip_apb_async_mcfp0_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCFP0, NULL, cmucal_vclk_ip_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MCFP0, NULL, cmucal_vclk_ip_lhs_axi_d1_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF1_MCFP1MCFP0, NULL, cmucal_vclk_ip_lhm_ast_otf1_mcfp1mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D1_MCFP0, NULL, cmucal_vclk_ip_ppmu_d1_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_MCFP0, NULL, cmucal_vclk_ip_sysmmu_d1_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_MCFP0MCFP1, NULL, cmucal_vclk_ip_lhs_axi_p_mcfp0mcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF2_MCFP1MCFP0, NULL, cmucal_vclk_ip_lhm_ast_otf2_mcfp1mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF3_MCFP1MCFP0, NULL, cmucal_vclk_ip_lhm_ast_otf3_mcfp1mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D0_MCFP0, NULL, cmucal_vclk_ip_qe_d0_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D1_MCFP0, NULL, cmucal_vclk_ip_qe_d1_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D2_MCFP0, NULL, cmucal_vclk_ip_qe_d2_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D3_MCFP0, NULL, cmucal_vclk_ip_qe_d3_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D0_MCFP0, NULL, cmucal_vclk_ip_xiu_d0_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D2_MCFP0, NULL, cmucal_vclk_ip_ppmu_d2_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D3_MCFP0, NULL, cmucal_vclk_ip_ppmu_d3_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D2_MCFP0, NULL, cmucal_vclk_ip_sysmmu_d2_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D3_MCFP0, NULL, cmucal_vclk_ip_sysmmu_d3_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D2_MCFP0, NULL, cmucal_vclk_ip_lhs_axi_d2_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D3_MCFP0, NULL, cmucal_vclk_ip_lhs_axi_d3_mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_CTL_MCFP1MCFP0, NULL, cmucal_vclk_ip_lhm_ast_ctl_mcfp1mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_CTL_MCFP0MCFP1, NULL, cmucal_vclk_ip_lhs_ast_ctl_mcfp0mcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCFP1_CMU_MCFP1, NULL, cmucal_vclk_ip_mcfp1_cmu_mcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_MCFP1_0, NULL, cmucal_vclk_ip_ad_apb_mcfp1_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D2_ORBMCH, NULL, cmucal_vclk_ip_qe_d2_orbmch, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_D0_MCFP1, NULL, cmucal_vclk_ip_vgen_lite_d0_mcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_ORBMCH, NULL, cmucal_vclk_ip_ppmu_orbmch, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D0_ORBMCH, NULL, cmucal_vclk_ip_qe_d0_orbmch, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF1_MCFP0MCFP1, NULL, cmucal_vclk_ip_lhm_ast_otf1_mcfp0mcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MCFP0MCFP1, NULL, cmucal_vclk_ip_lhm_axi_p_mcfp0mcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCFP1, NULL, cmucal_vclk_ip_mcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_MCFP1, NULL, cmucal_vclk_ip_lhs_axi_d_mcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF_MCFP1ITP, NULL, cmucal_vclk_ip_lhs_ast_otf_mcfp1itp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_VO_TAAMCFP1, NULL, cmucal_vclk_ip_lhm_ast_vo_taamcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF_MCFP1DNS, NULL, cmucal_vclk_ip_lhs_ast_otf_mcfp1dns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ORBMCH0, NULL, cmucal_vclk_ip_orbmch0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MCFP1, NULL, cmucal_vclk_ip_sysreg_mcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_VO_MCFP1TAA, NULL, cmucal_vclk_ip_lhs_ast_vo_mcfp1taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D_MCFP1, NULL, cmucal_vclk_ip_sysmmu_d_mcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MCFP1, NULL, cmucal_vclk_ip_d_tzpc_mcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_MCFP1, NULL, cmucal_vclk_ip_xiu_d_mcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D1_ORBMCH, NULL, cmucal_vclk_ip_qe_d1_orbmch, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF0_MCFP1MCFP0, NULL, cmucal_vclk_ip_lhs_ast_otf0_mcfp1mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF0_MCFP0MCFP1, NULL, cmucal_vclk_ip_lhm_ast_otf0_mcfp0mcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF1_MCFP1MCFP0, NULL, cmucal_vclk_ip_lhs_ast_otf1_mcfp1mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_ORBMCH0, NULL, cmucal_vclk_ip_ad_apb_orbmch0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF2_MCFP1MCFP0, NULL, cmucal_vclk_ip_lhs_ast_otf2_mcfp1mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF3_MCFP1MCFP0, NULL, cmucal_vclk_ip_lhs_ast_otf3_mcfp1mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D3_ORBMCH, NULL, cmucal_vclk_ip_qe_d3_orbmch, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_CTL_MCFP0MCFP1, NULL, cmucal_vclk_ip_lhm_ast_ctl_mcfp0mcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_CTL_MCFP1MCFP0, NULL, cmucal_vclk_ip_lhs_ast_ctl_mcfp1mcfp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ORBMCH1, NULL, cmucal_vclk_ip_orbmch1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D4_ORBMCH, NULL, cmucal_vclk_ip_qe_d4_orbmch, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D5_ORBMCH, NULL, cmucal_vclk_ip_qe_d5_orbmch, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_D1_MCFP1, NULL, cmucal_vclk_ip_vgen_lite_d1_mcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCSC_CMU_MCSC, NULL, cmucal_vclk_ip_mcsc_cmu_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MCSC, NULL, cmucal_vclk_ip_lhm_axi_p_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MCSC, NULL, cmucal_vclk_ip_sysreg_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCSC, NULL, cmucal_vclk_ip_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D0_MCSC, NULL, cmucal_vclk_ip_ppmu_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MCSC, NULL, cmucal_vclk_ip_d_tzpc_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_D0_MCSC, NULL, cmucal_vclk_ip_vgen_lite_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_MCSC_0, NULL, cmucal_vclk_ip_ad_apb_mcsc_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D1_MCSC, NULL, cmucal_vclk_ip_ppmu_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GDC, NULL, cmucal_vclk_ip_gdc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_GDC, NULL, cmucal_vclk_ip_ad_apb_gdc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_D2_MCSC, NULL, cmucal_vclk_ip_ppmu_d2_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D2_MCSC, NULL, cmucal_vclk_ip_sysmmu_d2_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_MCSC, NULL, cmucal_vclk_ip_sysmmu_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D2_MCSC, NULL, cmucal_vclk_ip_lhs_axi_d2_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D0_MCSC, NULL, cmucal_vclk_ip_lhs_acel_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF_YUVPPMCSC, NULL, cmucal_vclk_ip_lhm_ast_otf_yuvppmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_MCSC, NULL, cmucal_vclk_ip_hpm_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_ADD_MCSC, NULL, cmucal_vclk_ip_busif_add_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_DDD_MCSC, NULL, cmucal_vclk_ip_busif_ddd_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADD_MCSC, NULL, cmucal_vclk_ip_add_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPM_MCSC, NULL, cmucal_vclk_ip_busif_hpm_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D2_MCSC, NULL, cmucal_vclk_ip_xiu_d2_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_D1_MCSC, NULL, cmucal_vclk_ip_vgen_lite_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D0_MCSC, NULL, cmucal_vclk_ip_xiu_d0_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDD_MCSC, NULL, cmucal_vclk_ip_ddd_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_YUVPPMCSC, NULL, cmucal_vclk_ip_lhm_axi_d_yuvppmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D1_MCSC, NULL, cmucal_vclk_ip_xiu_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MCSC, NULL, cmucal_vclk_ip_lhs_axi_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_MCSC, NULL, cmucal_vclk_ip_sysmmu_d1_mcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF_ITPMCSC, NULL, cmucal_vclk_ip_lhm_ast_otf_itpmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFC0_CMU_MFC0, NULL, cmucal_vclk_ip_mfc0_cmu_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_MFC0, NULL, cmucal_vclk_ip_as_apb_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MFC0, NULL, cmucal_vclk_ip_sysreg_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_MFC0, NULL, cmucal_vclk_ip_lhs_axi_d0_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MFC0, NULL, cmucal_vclk_ip_lhs_axi_d1_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MFC0, NULL, cmucal_vclk_ip_lhm_axi_p_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_MFC0D0, NULL, cmucal_vclk_ip_sysmmu_mfc0d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_MFC0D1, NULL, cmucal_vclk_ip_sysmmu_mfc0d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MFC0D0, NULL, cmucal_vclk_ip_ppmu_mfc0d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MFC0D1, NULL, cmucal_vclk_ip_ppmu_mfc0d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_AXI_WFD, NULL, cmucal_vclk_ip_as_axi_wfd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_WFD, NULL, cmucal_vclk_ip_ppmu_wfd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_MFC0, NULL, cmucal_vclk_ip_xiu_d_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_MFC0, NULL, cmucal_vclk_ip_vgen_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFC0, NULL, cmucal_vclk_ip_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WFD, NULL, cmucal_vclk_ip_wfd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LH_ATB_MFC0, NULL, cmucal_vclk_ip_lh_atb_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MFC0, NULL, cmucal_vclk_ip_d_tzpc_mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_WFD_NS, NULL, cmucal_vclk_ip_as_apb_wfd_ns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF0_MFC1MFC0, NULL, cmucal_vclk_ip_lhm_ast_otf0_mfc1mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF1_MFC1MFC0, NULL, cmucal_vclk_ip_lhm_ast_otf1_mfc1mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF2_MFC1MFC0, NULL, cmucal_vclk_ip_lhm_ast_otf2_mfc1mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF3_MFC1MFC0, NULL, cmucal_vclk_ip_lhm_ast_otf3_mfc1mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF0_MFC0MFC1, NULL, cmucal_vclk_ip_lhs_ast_otf0_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF1_MFC0MFC1, NULL, cmucal_vclk_ip_lhs_ast_otf1_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF2_MFC0MFC1, NULL, cmucal_vclk_ip_lhs_ast_otf2_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF3_MFC0MFC1, NULL, cmucal_vclk_ip_lhs_ast_otf3_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADS_APB_MFC0MFC1, NULL, cmucal_vclk_ip_ads_apb_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFC1_CMU_MFC1, NULL, cmucal_vclk_ip_mfc1_cmu_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AS_APB_MFC1, NULL, cmucal_vclk_ip_as_apb_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MFC1, NULL, cmucal_vclk_ip_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MFC1D1, NULL, cmucal_vclk_ip_ppmu_mfc1d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_MFC1D0, NULL, cmucal_vclk_ip_ppmu_mfc1d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_MFC1, NULL, cmucal_vclk_ip_lhs_axi_d0_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_MFC1, NULL, cmucal_vclk_ip_vgen_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MFC1, NULL, cmucal_vclk_ip_lhm_axi_p_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_MFC1D0, NULL, cmucal_vclk_ip_sysmmu_mfc1d0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_MFC1D1, NULL, cmucal_vclk_ip_sysmmu_mfc1d1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MFC1, NULL, cmucal_vclk_ip_sysreg_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MFC1, NULL, cmucal_vclk_ip_d_tzpc_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_MFC1, NULL, cmucal_vclk_ip_lhs_axi_d1_mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_APB_MFC0MFC1, NULL, cmucal_vclk_ip_adm_apb_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF0_MFC0MFC1, NULL, cmucal_vclk_ip_lhm_ast_otf0_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF1_MFC0MFC1, NULL, cmucal_vclk_ip_lhm_ast_otf1_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF2_MFC0MFC1, NULL, cmucal_vclk_ip_lhm_ast_otf2_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF3_MFC0MFC1, NULL, cmucal_vclk_ip_lhm_ast_otf3_mfc0mfc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF0_MFC1MFC0, NULL, cmucal_vclk_ip_lhs_ast_otf0_mfc1mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF1_MFC1MFC0, NULL, cmucal_vclk_ip_lhs_ast_otf1_mfc1mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF2_MFC1MFC0, NULL, cmucal_vclk_ip_lhs_ast_otf2_mfc1mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF3_MFC1MFC0, NULL, cmucal_vclk_ip_lhs_ast_otf3_mfc1mfc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MIF_CMU_MIF, NULL, cmucal_vclk_ip_mif_cmu_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDRPHY, NULL, cmucal_vclk_ip_ddrphy, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_MIF, NULL, cmucal_vclk_ip_sysreg_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_MIF, NULL, cmucal_vclk_ip_lhm_axi_p_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBBR_DDRPHY, NULL, cmucal_vclk_ip_apbbr_ddrphy, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_APBBR_DMC, NULL, cmucal_vclk_ip_apbbr_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMC, NULL, cmucal_vclk_ip_dmc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QCH_ADAPTER_PPC_DEBUG, NULL, cmucal_vclk_ip_qch_adapter_ppc_debug, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_MIF, NULL, cmucal_vclk_ip_d_tzpc_mif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPC_DEBUG, NULL, cmucal_vclk_ip_ppc_debug, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NPU_CMU_NPU, NULL, cmucal_vclk_ip_npu_cmu_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IP_NPUCORE, NULL, cmucal_vclk_ip_ip_npucore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_NPU, NULL, cmucal_vclk_ip_lhm_axi_d1_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_RQ_NPU, NULL, cmucal_vclk_ip_lhs_axi_d_rq_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_NPU, NULL, cmucal_vclk_ip_lhm_axi_p_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_NPU, NULL, cmucal_vclk_ip_lhm_axi_d0_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_NPU, NULL, cmucal_vclk_ip_d_tzpc_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_CMDQ_NPU, NULL, cmucal_vclk_ip_lhs_axi_d_cmdq_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_NPU, NULL, cmucal_vclk_ip_sysreg_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_CTRL_NPU, NULL, cmucal_vclk_ip_lhm_axi_d_ctrl_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NPU01_CMU_NPU, NULL, cmucal_vclk_ip_npu01_cmu_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IP_NPU01CORE, NULL, cmucal_vclk_ip_ip_npu01core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_NPU01, NULL, cmucal_vclk_ip_lhm_axi_d1_npu01, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_RQ_NPU01, NULL, cmucal_vclk_ip_lhs_axi_d_rq_npu01, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_NPU01, NULL, cmucal_vclk_ip_lhm_axi_p_npu01, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_NPU01, NULL, cmucal_vclk_ip_lhm_axi_d0_npu01, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_NPU01, NULL, cmucal_vclk_ip_d_tzpc_npu01, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_CMDQ_NPU01, NULL, cmucal_vclk_ip_lhs_axi_d_cmdq_npu01, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_NPU01, NULL, cmucal_vclk_ip_sysreg_npu01, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_CTRL_NPU01, NULL, cmucal_vclk_ip_lhm_axi_d_ctrl_npu01, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NPU10_CMU_NPU, NULL, cmucal_vclk_ip_npu10_cmu_npu, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IP_NPU10CORE, NULL, cmucal_vclk_ip_ip_npu10core, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D1_NPU10, NULL, cmucal_vclk_ip_lhm_axi_d1_npu10, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_RQ_NPU10, NULL, cmucal_vclk_ip_lhs_axi_d_rq_npu10, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_NPU10, NULL, cmucal_vclk_ip_lhm_axi_p_npu10, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D0_NPU10, NULL, cmucal_vclk_ip_lhm_axi_d0_npu10, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_NPU10, NULL, cmucal_vclk_ip_d_tzpc_npu10, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_CMDQ_NPU10, NULL, cmucal_vclk_ip_lhs_axi_d_cmdq_npu10, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_NPU10, NULL, cmucal_vclk_ip_sysreg_npu10, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_CTRL_NPU10, NULL, cmucal_vclk_ip_lhm_axi_d_ctrl_npu10, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_NPUS_CMU_NPUS, NULL, cmucal_vclk_ip_npus_cmu_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D0_NPUS, NULL, cmucal_vclk_ip_sysmmu_d0_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D2_NPUS, NULL, cmucal_vclk_ip_sysmmu_d2_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D1_NPUS, NULL, cmucal_vclk_ip_sysmmu_d1_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_NPU10, NULL, cmucal_vclk_ip_lhs_axi_d0_npu10, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_NPUS, NULL, cmucal_vclk_ip_lhs_axi_d0_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_CTRL_NPU10, NULL, cmucal_vclk_ip_lhs_axi_d_ctrl_npu10, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_NPU01, NULL, cmucal_vclk_ip_lhs_axi_d0_npu01, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_NPUS, NULL, cmucal_vclk_ip_vgen_lite_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D0_NPU00, NULL, cmucal_vclk_ip_lhs_axi_d0_npu00, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_NPUS, NULL, cmucal_vclk_ip_lhm_axi_p_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_CMDQ_NPU01, NULL, cmucal_vclk_ip_lhm_axi_d_cmdq_npu01, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_CMDQ_NPU00, NULL, cmucal_vclk_ip_lhm_axi_d_cmdq_npu00, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_CTRL_NPU00, NULL, cmucal_vclk_ip_lhs_axi_d_ctrl_npu00, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_NPUS, NULL, cmucal_vclk_ip_lhs_axi_d1_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_CTRL_NPU01, NULL, cmucal_vclk_ip_lhs_axi_d_ctrl_npu01, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_RQ_NPU10, NULL, cmucal_vclk_ip_lhm_axi_d_rq_npu10, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_NPU00, NULL, cmucal_vclk_ip_lhs_axi_d1_npu00, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_NPU01, NULL, cmucal_vclk_ip_lhs_axi_d1_npu01, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_CMDQ_NPU10, NULL, cmucal_vclk_ip_lhm_axi_d_cmdq_npu10, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_NPUS, NULL, cmucal_vclk_ip_sysreg_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_NPUS, NULL, cmucal_vclk_ip_d_tzpc_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IP_NPUS, NULL, cmucal_vclk_ip_ip_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D2_NPUS, NULL, cmucal_vclk_ip_lhs_axi_d2_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_NPUS_2, NULL, cmucal_vclk_ip_ppmu_npus_2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_NPUS_1, NULL, cmucal_vclk_ip_ppmu_npus_1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_NPUS_0, NULL, cmucal_vclk_ip_ppmu_npus_0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_RQ_NPU00, NULL, cmucal_vclk_ip_lhm_axi_d_rq_npu00, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_RQ_NPU01, NULL, cmucal_vclk_ip_lhm_axi_d_rq_npu01, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D1_NPU10, NULL, cmucal_vclk_ip_lhs_axi_d1_npu10, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_P0_NPUS, NULL, cmucal_vclk_ip_ad_apb_p0_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_INT_NPUS, NULL, cmucal_vclk_ip_lhs_axi_p_int_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_INT_NPUS, NULL, cmucal_vclk_ip_lhm_axi_p_int_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_NPUS, NULL, cmucal_vclk_ip_hpm_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADD_NPUS, NULL, cmucal_vclk_ip_add_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPM_NPUS, NULL, cmucal_vclk_ip_busif_hpm_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_ADD_NPUS, NULL, cmucal_vclk_ip_busif_add_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_DDD_NPUS, NULL, cmucal_vclk_ip_busif_ddd_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_NPUS, NULL, cmucal_vclk_ip_htu_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDD_NPUS, NULL, cmucal_vclk_ip_ddd_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_DAP_NPUS, NULL, cmucal_vclk_ip_adm_dap_npus, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_PERIC0, NULL, cmucal_vclk_ip_gpio_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_PERIC0, NULL, cmucal_vclk_ip_sysreg_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIC0_CMU_PERIC0, NULL, cmucal_vclk_ip_peric0_cmu_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_PERIC0, NULL, cmucal_vclk_ip_lhm_axi_p_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_PERIC0, NULL, cmucal_vclk_ip_d_tzpc_peric0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIC0_TOP0, NULL, cmucal_vclk_ip_peric0_top0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIC0_TOP1, NULL, cmucal_vclk_ip_peric0_top1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_PERIC1, NULL, cmucal_vclk_ip_gpio_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_PERIC1, NULL, cmucal_vclk_ip_sysreg_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIC1_CMU_PERIC1, NULL, cmucal_vclk_ip_peric1_cmu_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_PERIC1, NULL, cmucal_vclk_ip_lhm_axi_p_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_PERIC1, NULL, cmucal_vclk_ip_d_tzpc_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIC1_TOP0, NULL, cmucal_vclk_ip_peric1_top0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIC1_TOP1, NULL, cmucal_vclk_ip_peric1_top1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_CSISPERIC1, NULL, cmucal_vclk_ip_lhm_axi_p_csisperic1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_P_PERIC1, NULL, cmucal_vclk_ip_xiu_p_peric1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI16_I3C, NULL, cmucal_vclk_ip_usi16_i3c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USI17_I3C, NULL, cmucal_vclk_ip_usi17_i3c, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIC2_TOP0, NULL, cmucal_vclk_ip_peric2_top0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_PERIC2, NULL, cmucal_vclk_ip_sysreg_peric2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_PERIC2, NULL, cmucal_vclk_ip_d_tzpc_peric2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_PERIC2, NULL, cmucal_vclk_ip_lhm_axi_p_peric2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_PERIC2, NULL, cmucal_vclk_ip_gpio_peric2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIC2_CMU_PERIC2, NULL, cmucal_vclk_ip_peric2_cmu_peric2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIC2_TOP1, NULL, cmucal_vclk_ip_peric2_top1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_PERIS, NULL, cmucal_vclk_ip_sysreg_peris, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT1, NULL, cmucal_vclk_ip_wdt1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT0, NULL, cmucal_vclk_ip_wdt0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PERIS_CMU_PERIS, NULL, cmucal_vclk_ip_peris_cmu_peris, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OTP_CON_BIRA, NULL, cmucal_vclk_ip_otp_con_bira, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GIC, NULL, cmucal_vclk_ip_gic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_PERIS, NULL, cmucal_vclk_ip_lhm_axi_p_peris, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MCT, NULL, cmucal_vclk_ip_mct, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OTP_CON_TOP, NULL, cmucal_vclk_ip_otp_con_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_PERIS, NULL, cmucal_vclk_ip_d_tzpc_peris, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TMU_SUB, NULL, cmucal_vclk_ip_tmu_sub, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TMU_TOP, NULL, cmucal_vclk_ip_tmu_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_PERISGIC, NULL, cmucal_vclk_ip_lhm_axi_p_perisgic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BC_EMUL, NULL, cmucal_vclk_ip_bc_emul, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_ICC_CPUGIC_CLUSTER0, NULL, cmucal_vclk_ip_lhm_ast_icc_cpugic_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_IRI_GICCPU_CLUSTER0, NULL, cmucal_vclk_ip_lhs_ast_iri_giccpu_cluster0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_OTP_CON_BISR, NULL, cmucal_vclk_ip_otp_con_bisr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_S2D_CMU_S2D, NULL, cmucal_vclk_ip_s2d_cmu_s2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BIS_S2D, NULL, cmucal_vclk_ip_bis_s2d, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_G_SCAN2DRAM, NULL, cmucal_vclk_ip_lhm_axi_g_scan2dram, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_SYSMMU_RTIC, NULL, cmucal_vclk_ip_ad_apb_sysmmu_rtic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_RTIC, NULL, cmucal_vclk_ip_sysmmu_rtic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_RTIC, NULL, cmucal_vclk_ip_rtic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_SSP, NULL, cmucal_vclk_ip_xiu_d_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D_SSP, NULL, cmucal_vclk_ip_lhs_acel_d_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_PUF, NULL, cmucal_vclk_ip_ad_apb_puf, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PUF, NULL, cmucal_vclk_ip_puf, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SSS, NULL, cmucal_vclk_ip_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_SSP, NULL, cmucal_vclk_ip_lhm_axi_p_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_SSP, NULL, cmucal_vclk_ip_d_tzpc_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_SSS, NULL, cmucal_vclk_ip_baaw_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_SSPCORE, NULL, cmucal_vclk_ip_lhm_axi_d_sspcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_SSP, NULL, cmucal_vclk_ip_ppmu_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_USS_SSPCORE, NULL, cmucal_vclk_ip_uss_sspcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_RTIC, NULL, cmucal_vclk_ip_qe_rtic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_SSPCORE, NULL, cmucal_vclk_ip_qe_sspcore, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_SSS, NULL, cmucal_vclk_ip_qe_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_RTIC, NULL, cmucal_vclk_ip_vgen_lite_rtic, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_SSPCTRL, NULL, cmucal_vclk_ip_sysreg_sspctrl, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_D_SSP, NULL, cmucal_vclk_ip_sweeper_d_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_AXI_P_SSP, NULL, cmucal_vclk_ip_bps_axi_p_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_DAP_SSS, NULL, cmucal_vclk_ip_adm_dap_sss, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SSP_CMU_SSP, NULL, cmucal_vclk_ip_ssp_cmu_ssp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_TAA, NULL, cmucal_vclk_ip_lhs_axi_d_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_TAA, NULL, cmucal_vclk_ip_lhm_axi_p_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_TAA, NULL, cmucal_vclk_ip_sysreg_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TAA_CMU_TAA, NULL, cmucal_vclk_ip_taa_cmu_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF_TAADNS, NULL, cmucal_vclk_ip_lhs_ast_otf_taadns, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_TAA, NULL, cmucal_vclk_ip_d_tzpc_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF0_CSISTAA, NULL, cmucal_vclk_ip_lhm_ast_otf0_csistaa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SIPU_TAA, NULL, cmucal_vclk_ip_sipu_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_TAA, NULL, cmucal_vclk_ip_ad_apb_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_ZOTF0_TAACSIS, NULL, cmucal_vclk_ip_lhs_ast_zotf0_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_ZOTF1_TAACSIS, NULL, cmucal_vclk_ip_lhs_ast_zotf1_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_ZOTF2_TAACSIS, NULL, cmucal_vclk_ip_lhs_ast_zotf2_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_TAA, NULL, cmucal_vclk_ip_ppmu_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D_TAA, NULL, cmucal_vclk_ip_sysmmu_d_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_TAA0, NULL, cmucal_vclk_ip_vgen_lite_taa0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF1_CSISTAA, NULL, cmucal_vclk_ip_lhm_ast_otf1_csistaa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF2_CSISTAA, NULL, cmucal_vclk_ip_lhm_ast_otf2_csistaa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_SOTF0_TAACSIS, NULL, cmucal_vclk_ip_lhs_ast_sotf0_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_SOTF1_TAACSIS, NULL, cmucal_vclk_ip_lhs_ast_sotf1_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_SOTF2_TAACSIS, NULL, cmucal_vclk_ip_lhs_ast_sotf2_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_OTF3_CSISTAA, NULL, cmucal_vclk_ip_lhm_ast_otf3_csistaa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_SOTF3_TAACSIS, NULL, cmucal_vclk_ip_lhs_ast_sotf3_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_ZOTF3_TAACSIS, NULL, cmucal_vclk_ip_lhs_ast_zotf3_taacsis, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AST_VO_MCFP1TAA, NULL, cmucal_vclk_ip_lhm_ast_vo_mcfp1taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_VO_TAAMCFP1, NULL, cmucal_vclk_ip_lhs_ast_vo_taamcfp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_TAA, NULL, cmucal_vclk_ip_hpm_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPM_TAA, NULL, cmucal_vclk_ip_busif_hpm_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADD_TAA, NULL, cmucal_vclk_ip_add_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_ADD_TAA, NULL, cmucal_vclk_ip_busif_add_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_DDD_TAA, NULL, cmucal_vclk_ip_busif_ddd_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_TAA1, NULL, cmucal_vclk_ip_vgen_lite_taa1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDD_TAA, NULL, cmucal_vclk_ip_ddd_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D_TAA, NULL, cmucal_vclk_ip_xiu_d_taa, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VPC_CMU_VPC, NULL, cmucal_vclk_ip_vpc_cmu_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_VPC, NULL, cmucal_vclk_ip_d_tzpc_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_VPCVPD0, NULL, cmucal_vclk_ip_lhs_axi_p_vpcvpd0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_VPCVPD1, NULL, cmucal_vclk_ip_lhs_axi_p_vpcvpd1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_VPCVPD0_SFR, NULL, cmucal_vclk_ip_lhs_axi_d_vpcvpd0_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_VPC, NULL, cmucal_vclk_ip_lhm_axi_p_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_VPD0VPC_SFR, NULL, cmucal_vclk_ip_lhm_axi_d_vpd0vpc_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_VPD1VPC_SFR, NULL, cmucal_vclk_ip_lhm_axi_d_vpd1vpc_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_VPC0, NULL, cmucal_vclk_ip_ppmu_vpc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_VPC1, NULL, cmucal_vclk_ip_ppmu_vpc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_VPC2, NULL, cmucal_vclk_ip_ppmu_vpc2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IP_VPC, NULL, cmucal_vclk_ip_ip_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_VPC, NULL, cmucal_vclk_ip_sysreg_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_VPCVPD1_SFR, NULL, cmucal_vclk_ip_lhs_axi_d_vpcvpd1_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_P_VPC_200, NULL, cmucal_vclk_ip_lhs_axi_p_vpc_200, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_VPC_800, NULL, cmucal_vclk_ip_lhm_axi_p_vpc_800, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_VPC2, NULL, cmucal_vclk_ip_sysmmu_vpc2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D2_VPC, NULL, cmucal_vclk_ip_lhs_acel_d2_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_VPD1VPC_CACHE, NULL, cmucal_vclk_ip_lhm_axi_d_vpd1vpc_cache, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_VPD0VPC_CACHE, NULL, cmucal_vclk_ip_lhm_axi_d_vpd0vpc_cache, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_VPC0, NULL, cmucal_vclk_ip_ad_apb_vpc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_VPC0, NULL, cmucal_vclk_ip_sysmmu_vpc0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_VPC1, NULL, cmucal_vclk_ip_sysmmu_vpc1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D0_VPC, NULL, cmucal_vclk_ip_lhs_acel_d0_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_ACEL_D1_VPC, NULL, cmucal_vclk_ip_lhs_acel_d1_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_VPCVPD0_DMA, NULL, cmucal_vclk_ip_lhs_axi_d_vpcvpd0_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_VPCVPD1_DMA, NULL, cmucal_vclk_ip_lhs_axi_d_vpcvpd1_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_VPC, NULL, cmucal_vclk_ip_hpm_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADD_VPC, NULL, cmucal_vclk_ip_add_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPM_VPC, NULL, cmucal_vclk_ip_busif_hpm_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_ADD_VPC, NULL, cmucal_vclk_ip_busif_add_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_DDD_VPC, NULL, cmucal_vclk_ip_busif_ddd_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ADM_DAP_VPC, NULL, cmucal_vclk_ip_adm_dap_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HTU_VPC, NULL, cmucal_vclk_ip_htu_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DDD_VPC, NULL, cmucal_vclk_ip_ddd_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_VPC, NULL, cmucal_vclk_ip_vgen_lite_vpc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_VPC_VOTF, NULL, cmucal_vclk_ip_xiu_vpc_votf, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VPD_CMU_VPD, NULL, cmucal_vclk_ip_vpd_cmu_vpd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_VPD, NULL, cmucal_vclk_ip_sysreg_vpd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_VPDVPC_SFR, NULL, cmucal_vclk_ip_lhs_axi_d_vpdvpc_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_IP_VPD, NULL, cmucal_vclk_ip_ip_vpd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_VPDVPC_CACHE, NULL, cmucal_vclk_ip_lhs_axi_d_vpdvpc_cache, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_VPCVPD, NULL, cmucal_vclk_ip_lhm_axi_p_vpcvpd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_VPCVPD_SFR, NULL, cmucal_vclk_ip_lhm_axi_d_vpcvpd_sfr, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_VPD, NULL, cmucal_vclk_ip_d_tzpc_vpd, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_VPCVPD_DMA, NULL, cmucal_vclk_ip_lhm_axi_d_vpcvpd_dma, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AHB_BUSMATRIX, NULL, cmucal_vclk_ip_ahb_busmatrix, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_IF0, NULL, cmucal_vclk_ip_dmic_if0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_VTS, NULL, cmucal_vclk_ip_sysreg_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VTS_CMU_VTS, NULL, cmucal_vclk_ip_vts_cmu_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_VTS, NULL, cmucal_vclk_ip_lhm_axi_p_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_GPIO_VTS, NULL, cmucal_vclk_ip_gpio_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_WDT_VTS, NULL, cmucal_vclk_ip_wdt_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB0, NULL, cmucal_vclk_ip_dmic_ahb0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB1, NULL, cmucal_vclk_ip_dmic_ahb1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCINTERRUPT, NULL, cmucal_vclk_ip_asyncinterrupt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SS_VTS_GLUE, NULL, cmucal_vclk_ip_ss_vts_glue, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_CORTEXM4INTEGRATION, NULL, cmucal_vclk_ip_cortexm4integration, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_VTS, NULL, cmucal_vclk_ip_lhs_axi_d_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_VTS, NULL, cmucal_vclk_ip_d_tzpc_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE, NULL, cmucal_vclk_ip_vgen_lite, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_LP_VTS, NULL, cmucal_vclk_ip_bps_lp_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BPS_P_VTS, NULL, cmucal_vclk_ip_bps_p_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_D_VTS, NULL, cmucal_vclk_ip_sweeper_d_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_D_VTS, NULL, cmucal_vclk_ip_baaw_d_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_ABOX_VTS, NULL, cmucal_vclk_ip_mailbox_abox_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB2, NULL, cmucal_vclk_ip_dmic_ahb2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB3, NULL, cmucal_vclk_ip_dmic_ahb3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_IF2, NULL, cmucal_vclk_ip_dmic_if2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_APM_VTS1, NULL, cmucal_vclk_ip_mailbox_apm_vts1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TIMER, NULL, cmucal_vclk_ip_timer, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PDMA_VTS, NULL, cmucal_vclk_ip_pdma_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB4, NULL, cmucal_vclk_ip_dmic_ahb4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AHB5, NULL, cmucal_vclk_ip_dmic_ahb5, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AUD0, NULL, cmucal_vclk_ip_dmic_aud0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_IF1, NULL, cmucal_vclk_ip_dmic_if1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AUD1, NULL, cmucal_vclk_ip_dmic_aud1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMIC_AUD2, NULL, cmucal_vclk_ip_dmic_aud2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SERIAL_LIF, NULL, cmucal_vclk_ip_serial_lif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TIMER1, NULL, cmucal_vclk_ip_timer1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_TIMER2, NULL, cmucal_vclk_ip_timer2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SERIAL_LIF_DEBUG_VT, NULL, cmucal_vclk_ip_serial_lif_debug_vt, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SERIAL_LIF_DEBUG_US, NULL, cmucal_vclk_ip_serial_lif_debug_us, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_D_AUDVTS, NULL, cmucal_vclk_ip_lhm_axi_d_audvts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_MAILBOX_AP_VTS, NULL, cmucal_vclk_ip_mailbox_ap_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BAAW_C_VTS, NULL, cmucal_vclk_ip_baaw_c_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_C_VTS, NULL, cmucal_vclk_ip_lhs_axi_c_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SWEEPER_C_VTS, NULL, cmucal_vclk_ip_sweeper_c_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC0, NULL, cmucal_vclk_ip_hwacg_sys_dmic0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC1, NULL, cmucal_vclk_ip_hwacg_sys_dmic1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC2, NULL, cmucal_vclk_ip_hwacg_sys_dmic2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC3, NULL, cmucal_vclk_ip_hwacg_sys_dmic3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC4, NULL, cmucal_vclk_ip_hwacg_sys_dmic4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_DMIC5, NULL, cmucal_vclk_ip_hwacg_sys_dmic5, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCAHB0, NULL, cmucal_vclk_ip_asyncahb0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCAHB1, NULL, cmucal_vclk_ip_asyncahb1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCAHB2, NULL, cmucal_vclk_ip_asyncahb2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCAHB3, NULL, cmucal_vclk_ip_asyncahb3, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCAHB4, NULL, cmucal_vclk_ip_asyncahb4, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNCAHB5, NULL, cmucal_vclk_ip_asyncahb5, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HPM_VTS, NULL, cmucal_vclk_ip_hpm_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_ASYNC_APB_VTS, NULL, cmucal_vclk_ip_async_apb_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_BUSIF_HPM_VTS, NULL, cmucal_vclk_ip_busif_hpm_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_DMAILBOX_TEST, NULL, cmucal_vclk_ip_dmailbox_test, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_HWACG_SYS_SERIAL_LIF, NULL, cmucal_vclk_ip_hwacg_sys_serial_lif, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_LP_VTS, NULL, cmucal_vclk_ip_lhm_axi_lp_vts, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_YUVPP_CMU_YUVPP, NULL, cmucal_vclk_ip_yuvpp_cmu_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_YUVPP0, NULL, cmucal_vclk_ip_ad_apb_yuvpp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_D_TZPC_YUVPP, NULL, cmucal_vclk_ip_d_tzpc_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHM_AXI_P_YUVPP, NULL, cmucal_vclk_ip_lhm_axi_p_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSREG_YUVPP, NULL, cmucal_vclk_ip_sysreg_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_YUVPP0, NULL, cmucal_vclk_ip_vgen_lite_yuvpp0, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_SYSMMU_D_YUVPP, NULL, cmucal_vclk_ip_sysmmu_d_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D0_YUVPP, NULL, cmucal_vclk_ip_qe_d0_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_PPMU_YUVPP, NULL, cmucal_vclk_ip_ppmu_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D0_YUVPP, NULL, cmucal_vclk_ip_xiu_d0_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_YUVPP, NULL, cmucal_vclk_ip_lhs_axi_d_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D1_YUVPP, NULL, cmucal_vclk_ip_qe_d1_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_YUVPP_TOP, NULL, cmucal_vclk_ip_yuvpp_top, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D2_YUVPP, NULL, cmucal_vclk_ip_qe_d2_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D3_YUVPP, NULL, cmucal_vclk_ip_qe_d3_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D4_YUVPP, NULL, cmucal_vclk_ip_qe_d4_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D5_YUVPP, NULL, cmucal_vclk_ip_qe_d5_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AST_OTF_YUVPPMCSC, NULL, cmucal_vclk_ip_lhs_ast_otf_yuvppmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_FRC_MC, NULL, cmucal_vclk_ip_frc_mc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_YUVPP1, NULL, cmucal_vclk_ip_vgen_lite_yuvpp1, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D6_YUVPP, NULL, cmucal_vclk_ip_qe_d6_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D7_YUVPP, NULL, cmucal_vclk_ip_qe_d7_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D8_YUVPP, NULL, cmucal_vclk_ip_qe_d8_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D9_YUVPP, NULL, cmucal_vclk_ip_qe_d9_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_XIU_D1_YUVPP, NULL, cmucal_vclk_ip_xiu_d1_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_APB_FRC_MC, NULL, cmucal_vclk_ip_ad_apb_frc_mc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_LHS_AXI_D_YUVPPMCSC, NULL, cmucal_vclk_ip_lhs_axi_d_yuvppmcsc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_AD_AXI_FRC_MC, NULL, cmucal_vclk_ip_ad_axi_frc_mc, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_VGEN_LITE_YUVPP2, NULL, cmucal_vclk_ip_vgen_lite_yuvpp2, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D10_YUVPP, NULL, cmucal_vclk_ip_qe_d10_yuvpp, NULL, NULL),
	CMUCAL_VCLK(VCLK_IP_QE_D11_YUVPP, NULL, cmucal_vclk_ip_qe_d11_yuvpp, NULL, NULL),
};

