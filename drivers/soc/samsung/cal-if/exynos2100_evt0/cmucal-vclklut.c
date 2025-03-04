#include "../cmucal.h"
#include "cmucal-vclklut.h"


/* DVFS VCLK -> LUT Parameter List */
unsigned int vdd_mif_od_lut_params[] = {
	6400000, 6400000, 
};
unsigned int vdd_mif_nm_lut_params[] = {
	4266000, 4266000, 
};
unsigned int vdd_mif_ud_lut_params[] = {
	3078000, 3078000, 
};
unsigned int vdd_mif_sud_lut_params[] = {
	1690000, 1690000, 
};
unsigned int vdd_mif_uud_lut_params[] = {
	842000, 842000, 
};
unsigned int vdd_cam_nm_lut_params[] = {
	1, 1200000, 4, 58, 0, 7, 
};
unsigned int vdd_cam_sud_lut_params[] = {
	5, 1200000, 0, 0, 0, 7, 
};
unsigned int vdd_cam_ud_lut_params[] = {
	3, 1200000, 0, 0, 0, 7, 
};
unsigned int vdd_cam_uud_lut_params[] = {
	7, 1200000, 4, 58, 11, 3, 
};
unsigned int vdd_cpucl2_sod_lut_params[] = {
	2500000, 
};
unsigned int vdd_cpucl2_od_lut_params[] = {
	2100000, 
};
unsigned int vdd_cpucl2_nm_lut_params[] = {
	1600000, 
};
unsigned int vdd_cpucl2_ud_lut_params[] = {
	1000000, 
};
unsigned int vdd_cpucl2_sud_lut_params[] = {
	600000, 
};
unsigned int vdd_cpucl2_uud_lut_params[] = {
	300000, 
};
unsigned int vdd_cpucl0_sod_lut_params[] = {
	2200000, 1800000, 1, 
};
unsigned int vdd_cpucl0_od_lut_params[] = {
	1700000, 1500000, 1, 
};
unsigned int vdd_cpucl0_nm_lut_params[] = {
	1200000, 1100000, 0, 
};
unsigned int vdd_cpucl0_ud_lut_params[] = {
	800000, 715000, 0, 
};
unsigned int vdd_cpucl0_sud_lut_params[] = {
	480000, 420000, 0, 
};
unsigned int vdd_cpucl0_uud_lut_params[] = {
	175000, 175000, 0, 
};
unsigned int vdd_cpucl1_sod_lut_params[] = {
	2500000, 
};
unsigned int vdd_cpucl1_od_lut_params[] = {
	1950000, 
};
unsigned int vdd_cpucl1_nm_lut_params[] = {
	1500000, 
};
unsigned int vdd_cpucl1_ud_lut_params[] = {
	950000, 
};
unsigned int vdd_cpucl1_sud_lut_params[] = {
	550000, 
};
unsigned int vdd_cpucl1_uud_lut_params[] = {
	300000, 
};
unsigned int vdd_int_cmu_nm_lut_params[] = {
	0, 2, 3, 1, 3, 3, 0, 2, 2, 0, 1, 1, 1, 2, 1, 1, 2, 1, 1, 19, 2, 1, 1, 3, 2, 1, 1, 2, 2, 2, 1, 2, 2, 7, 2, 1, 7, 1, 2, 1, 7, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 860000, 808000, 663000, 0, 0, 0, 0, 0, 2028000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 
};
unsigned int vdd_int_cmu_od_lut_params[] = {
	1, 0, 0, 0, 2, 0, 1, 1, 1, 1, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 1, 2, 1, 7, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 860000, 808000, 663000, 0, 0, 0, 0, 0, 2028000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 
};
unsigned int vdd_int_cmu_sod_lut_params[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 860000, 808000, 663000, 0, 0, 0, 0, 0, 2028000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 
};
unsigned int vdd_int_cmu_ud_lut_params[] = {
	1, 5, 2, 3, 2, 2, 1, 2, 2, 0, 3, 3, 3, 2, 3, 1, 2, 3, 3, 19, 2, 1, 1, 3, 2, 3, 3, 2, 2, 2, 3, 4, 2, 7, 2, 3, 7, 1, 2, 1, 7, 0, 3, 3, 1, 1, 1, 1, 1, 3, 3, 3, 3, 3, 1, 3, 4, 0, 0, 3, 0, 0, 1, 4, 0, 2, 0, 0, 0, 0, 0, 0, 1, 0, 2, 0, 0, 2, 0, 0, 0, 0, 0, 0, 1, 0, 1, 3, 0, 2, 0, 0, 3, 0, 0, 3, 0, 0, 675000, 808000, 663000, 0, 0, 2, 2, 0, 2028000, 0, 0, 1, 0, 0, 0, 0, 2, 1, 0, 3, 4, 2, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 
};
unsigned int vdd_int_cmu_sud_lut_params[] = {
	3, 1, 2, 3, 2, 2, 2, 2, 2, 0, 2, 2, 3, 2, 2, 1, 2, 2, 2, 19, 2, 1, 1, 3, 2, 2, 2, 2, 2, 2, 2, 3, 2, 7, 2, 2, 7, 1, 2, 1, 7, 0, 2, 2, 1, 1, 1, 1, 1, 0, 2, 2, 2, 1, 1, 3, 4, 2, 1, 3, 1, 1, 3, 4, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 3, 3, 1, 1, 1, 1, 3, 1, 1, 3, 1, 1, 360000, 404000, 663000, 1, 1, 2, 1, 1, 1420000, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 
};
unsigned int vdd_int_cmu_uud_lut_params[] = {
	15, 2, 2, 3, 2, 0, 15, 2, 2, 0, 2, 3, 3, 2, 3, 1, 11, 2, 2, 19, 2, 1, 1, 3, 2, 2, 2, 2, 2, 2, 2, 4, 2, 15, 2, 3, 15, 7, 11, 1, 15, 3, 2, 2, 1, 1, 1, 3, 1, 5, 2, 2, 2, 1, 1, 15, 0, 7, 3, 3, 7, 7, 1, 4, 4, 2, 4, 4, 7, 7, 3, 1, 1, 3, 2, 4, 7, 1, 7, 3, 7, 7, 7, 1, 15, 7, 15, 3, 7, 2, 3, 3, 3, 7, 7, 3, 7, 7, 95000, 101000, 663000, 0, 7, 2, 1, 7, 842000, 7, 7, 15, 7, 6, 3, 3, 2, 1, 4, 2, 3, 0, 2, 2, 3, 1, 1, 7, 3, 3, 7, 7, 3, 3, 3, 1, 3, 7, 
};

/* SPECIAL VCLK -> LUT Parameter List */
unsigned int mux_clk_alive_i3c_pmic_nm_lut_params[] = {
	0, 0, 
};
unsigned int clkcmu_alive_bus_sod_lut_params[] = {
	0, 
};
unsigned int clkcmu_alive_bus_uud_lut_params[] = {
	0, 
};
unsigned int mux_clk_aud_dsif_nm_lut_params[] = {
	0, 2, 0, 23, 7, 0, 2, 7, 0, 7, 0, 7, 0, 1, 7, 0, 7, 0, 7, 0, 7, 7, 0, 
};
unsigned int mux_clk_aud_dsif_ud_lut_params[] = {
	0, 2, 0, 23, 7, 0, 2, 7, 0, 7, 0, 7, 0, 1, 7, 0, 7, 0, 7, 0, 7, 7, 0, 
};
unsigned int mux_clk_aud_dsif_od_lut_params[] = {
	0, 2, 0, 0, 7, 0, 2, 7, 0, 7, 0, 7, 0, 1, 7, 0, 7, 0, 7, 0, 7, 7, 0, 
};
unsigned int mux_clk_aud_dsif_uud_lut_params[] = {
	0, 2, 0, 23, 7, 0, 2, 7, 0, 7, 0, 7, 0, 1, 7, 0, 7, 0, 7, 0, 7, 7, 0, 
};
unsigned int mux_bus0_cmuref_nm_lut_params[] = {
	1, 
};
unsigned int mux_clkcmu_cmu_boost_uud_lut_params[] = {
	2, 
};
unsigned int mux_bus1_cmuref_nm_lut_params[] = {
	1, 
};
unsigned int mux_bus2_cmuref_nm_lut_params[] = {
	1, 
};
unsigned int clk_cmgp_adc_nm_lut_params[] = {
	1, 
};
unsigned int clkcmu_cmgp_adc_nm_lut_params[] = {
	1, 0, 
};
unsigned int mux_core_cmuref_nm_lut_params[] = {
	1, 
};
unsigned int mux_cpucl0_cmuref_uud_lut_params[] = {
	1, 
};
unsigned int mux_clkcmu_cmu_boost_cpu_uud_lut_params[] = {
	2, 
};
unsigned int mux_cpucl1_cmuref_uud_lut_params[] = {
	1, 
};
unsigned int mux_cpucl2_cmuref_uud_lut_params[] = {
	1, 
};
unsigned int mux_dsu_cmuref_uud_lut_params[] = {
	1, 
};
unsigned int mux_clk_hsi0_usb31drd_nm_lut_params[] = {
	0, 
};
unsigned int mux_mif_cmuref_uud_lut_params[] = {
	1, 
};
unsigned int mux_clkcmu_cmu_boost_mif_uud_lut_params[] = {
	2, 
};
unsigned int clkcmu_g3d_shader_sod_lut_params[] = {
	0, 
};
unsigned int clkcmu_g3d_shader_ud_lut_params[] = {
	0, 
};
unsigned int clkcmu_g3d_shader_sud_lut_params[] = {
	0, 
};
unsigned int clkcmu_g3d_shader_uud_lut_params[] = {
	0, 
};
unsigned int div_clk_alive_dbgcore_uart_nm_lut_params[] = {
	1, 
};
unsigned int div_clk_cmgp_i2c0_nm_lut_params[] = {
	1, 1, 
};
unsigned int clkcmu_cmgp_peri_nm_lut_params[] = {
	0, 2,
};
unsigned int div_clk_cmgp_usi1_nm_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_cmgp_usi0_nm_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_cmgp_usi2_nm_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_cmgp_usi3_nm_lut_params[] = {
	0, 1, 
};
unsigned int div_clk_cmgp_i2c1_nm_lut_params[] = {
	1, 1, 
};
unsigned int div_clk_cmgp_i2c2_nm_lut_params[] = {
	1, 1, 
};
unsigned int div_clk_cmgp_i2c3_nm_lut_params[] = {
	1, 1, 
};
unsigned int div_clk_cmgp_i3c_nm_lut_params[] = {
	1, 1, 
};
/* parent clock source(DIV_CLKCMU_CMGP_PERI) -  IP0: 400Mhz */
unsigned int div_clk_cmgp_400_lut_params[] = {
	0, 1,
};
unsigned int div_clk_cmgp_200_lut_params[] = {
	1, 1,
};
unsigned int div_clk_cmgp_100_lut_params[] = {
	3, 1,
};
unsigned int mux_clkcmu_hpm_uud_lut_params[] = {
	2, 
};
unsigned int clkcmu_cis_clk0_uud_lut_params[] = {
	3, 1, 
};
unsigned int clkcmu_cis_clk1_uud_lut_params[] = {
	3, 1, 
};
unsigned int clkcmu_cis_clk2_uud_lut_params[] = {
	3, 1, 
};
unsigned int clkcmu_cis_clk3_uud_lut_params[] = {
	3, 1, 
};
unsigned int clkcmu_cis_clk4_uud_lut_params[] = {
	3, 1, 
};
unsigned int clkcmu_cis_clk5_uud_lut_params[] = {
	3, 1, 
};
unsigned int div_clk_cpucl0_shortstop_core_sod_lut_params[] = {
	1, 
};
unsigned int div_clk_cpucl0_shortstop_core_od_lut_params[] = {
	1, 
};
unsigned int div_clk_cpucl0_shortstop_core_nm_lut_params[] = {
	1, 
};
unsigned int div_clk_cpucl0_shortstop_core_ud_lut_params[] = {
	1, 
};
unsigned int div_clk_cpucl0_shortstop_core_sud_lut_params[] = {
	1, 
};
unsigned int div_clk_cpucl0_shortstop_core_uud_lut_params[] = {
	1, 
};
unsigned int div_clk_cpucl1_shortstop_core_sod_lut_params[] = {
	1, 
};
unsigned int div_clk_cpucl1_shortstop_core_od_lut_params[] = {
	1, 
};
unsigned int div_clk_cpucl1_shortstop_core_nm_lut_params[] = {
	1, 
};
unsigned int div_clk_cpucl1_shortstop_core_ud_lut_params[] = {
	1, 
};
unsigned int div_clk_cpucl1_shortstop_core_sud_lut_params[] = {
	1, 
};
unsigned int div_clk_cpucl1_shortstop_core_uud_lut_params[] = {
	1, 
};
unsigned int div_clk_cpucl1_htu_sod_lut_params[] = {
	3, 
};
unsigned int div_clk_cpucl1_htu_od_lut_params[] = {
	3, 
};
unsigned int div_clk_cpucl1_htu_nm_lut_params[] = {
	3, 
};
unsigned int div_clk_cpucl1_htu_ud_lut_params[] = {
	3, 
};
unsigned int div_clk_cpucl1_htu_sud_lut_params[] = {
	3, 
};
unsigned int div_clk_cpucl1_htu_uud_lut_params[] = {
	3, 
};
unsigned int div_clk_cpucl2_shortstop_core_sod_lut_params[] = {
	1, 
};
unsigned int div_clk_cpucl2_shortstop_core_od_lut_params[] = {
	1, 
};
unsigned int div_clk_cpucl2_shortstop_core_nm_lut_params[] = {
	1, 
};
unsigned int div_clk_cpucl2_shortstop_core_ud_lut_params[] = {
	1, 
};
unsigned int div_clk_cpucl2_shortstop_core_sud_lut_params[] = {
	1, 
};
unsigned int div_clk_cpucl2_shortstop_core_uud_lut_params[] = {
	1, 
};
unsigned int div_clk_cpucl2_htu_sod_lut_params[] = {
	3, 
};
unsigned int div_clk_cpucl2_htu_od_lut_params[] = {
	3, 
};
unsigned int div_clk_cpucl2_htu_nm_lut_params[] = {
	3, 
};
unsigned int div_clk_cpucl2_htu_ud_lut_params[] = {
	3, 
};
unsigned int div_clk_cpucl2_htu_sud_lut_params[] = {
	3, 
};
unsigned int div_clk_cpucl2_htu_uud_lut_params[] = {
	3, 
};
unsigned int div_clk_dsu_shortstop_cluster_sod_lut_params[] = {
	1, 
};
unsigned int div_clk_dsu_shortstop_cluster_od_lut_params[] = {
	1, 
};
unsigned int div_clk_dsu_shortstop_cluster_nm_lut_params[] = {
	1, 
};
unsigned int div_clk_dsu_shortstop_cluster_ud_lut_params[] = {
	1, 
};
unsigned int div_clk_dsu_shortstop_cluster_sud_lut_params[] = {
	1, 
};
unsigned int div_clk_dsu_shortstop_cluster_uud_lut_params[] = {
	1, 
};
unsigned int div_clk_peric0_usi00_usi_nm_lut_params[] = {
	0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 
};
unsigned int div_clk_peric0_usi13_usi_nm_lut_params[] = {
	0, 1, 0, 1, 0, 1, 
};
unsigned int div_clk_peric1_uart_bt_nm_lut_params[] = {
	1, 1, 
};
unsigned int div_clk_peric1_usi18_usi_nm_lut_params[] = {
	0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 
};
unsigned int div_clk_peric2_usi08_usi_nm_lut_params[] = {
	0, 1, 1, 1, 0, 1, 0, 1, 
};
/* parent clock source(DIV_CLKCMU_PERICx_IP) -  IP0: 400Mhz // IP1: 200MHz  */
unsigned int div_clk_peric_400_lut_params[] = { //IP0
	0, 1,
};
unsigned int div_clk_peric_200_lut_params[] = {
	0, 2,
};
unsigned int div_clk_peric_133_lut_params[] = { //IP0
	2, 1,
};
unsigned int div_clk_peric_100_lut_params[] = {
	1, 2,
};
unsigned int div_clk_peric_66_lut_params[] = {
	2, 2,
};
unsigned int div_clk_peric_50_lut_params[] = {
	3, 2,
};
unsigned int div_clk_peric_26_lut_params[] = {
	0, 0,
};
unsigned int div_clk_peric_13_lut_params[] = {
	1, 0,
};
unsigned int div_clk_peric_8_lut_params[] = {
	2, 0,
};
unsigned int div_clk_peric_6_lut_params[] = {
	3, 0,
};
unsigned int div_clk_top_hsi0_bus_266_params[] = {
	1, 0,
};
unsigned int div_clk_top_hsi0_bus_177_params[] = {
	2, 0,
};
unsigned int div_clk_top_hsi0_bus_106_params[] = {
	4, 0,
};
unsigned int div_clk_top_hsi0_bus_80_params[] = {
	4, 1,
};
unsigned int div_clk_top_hsi0_bus_66_params[] = {
	5, 1,
};

/* COMMON VCLK -> LUT Parameter List */
unsigned int blk_aud_sud_lut_params[] = {
	70000, 2, 0, 3, 
};
unsigned int blk_aud_uud_lut_params[] = {
	69000, 2, 0, 3, 
};
unsigned int blk_aud_od_lut_params[] = {
	26000, 2, 0, 3, 
};
unsigned int blk_cmu_ud_lut_params[] = {
	1066000, 936000, 800000, 715000, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 
};
unsigned int blk_cmu_sud_lut_params[] = {
	1066000, 936000, 800000, 715000, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 
};
unsigned int blk_cmu_uud_lut_params[] = {
	1066000, 936000, 800000, 715000, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 
};
unsigned int blk_s2d_nm_lut_params[] = {
	800000, 1, 
};
unsigned int blk_alive_nm_lut_params[] = {
	0, 2, 0, 2, 0, 2,
};
unsigned int blk_cpucl0_sod_lut_params[] = {
	0, 0, 0, 
};
unsigned int blk_cpucl0_od_lut_params[] = {
	0, 0, 0, 
};
unsigned int blk_cpucl0_nm_lut_params[] = {
	0, 0, 0, 
};
unsigned int blk_cpucl0_ud_lut_params[] = {
	0, 0, 0, 
};
unsigned int blk_cpucl0_sud_lut_params[] = {
	0, 0, 0, 
};
unsigned int blk_cpucl0_uud_lut_params[] = {
	0, 0, 0, 
};
unsigned int blk_cpucl1_sod_lut_params[] = {
	0, 
};
unsigned int blk_cpucl1_od_lut_params[] = {
	0, 
};
unsigned int blk_cpucl1_nm_lut_params[] = {
	0, 
};
unsigned int blk_cpucl1_ud_lut_params[] = {
	0, 
};
unsigned int blk_cpucl1_sud_lut_params[] = {
	0, 
};
unsigned int blk_cpucl1_uud_lut_params[] = {
	0, 
};
unsigned int blk_cpucl2_sod_lut_params[] = {
	0, 
};
unsigned int blk_cpucl2_od_lut_params[] = {
	0, 
};
unsigned int blk_cpucl2_nm_lut_params[] = {
	0, 
};
unsigned int blk_cpucl2_ud_lut_params[] = {
	0, 
};
unsigned int blk_cpucl2_sud_lut_params[] = {
	0, 
};
unsigned int blk_cpucl2_uud_lut_params[] = {
	0, 
};
unsigned int blk_dsu_nm_lut_params[] = {
	0, 0, 0, 3, 1, 3, 0, 
};
unsigned int blk_dsu_sod_lut_params[] = {
	0, 0, 0, 3, 1, 3, 0, 
};
unsigned int blk_dsu_od_lut_params[] = {
	0, 0, 0, 3, 1, 3, 0, 
};
unsigned int blk_dsu_ud_lut_params[] = {
	0, 0, 0, 3, 1, 3, 0, 
};
unsigned int blk_dsu_sud_lut_params[] = {
	0, 0, 0, 3, 1, 3, 0, 
};
unsigned int blk_dsu_uud_lut_params[] = {
	0, 0, 0, 3, 1, 3, 0, 
};
unsigned int blk_g3d_nm_lut_params[] = {
	0, 3, 
};
unsigned int blk_hsi0_nm_lut_params[] = {
	0, 
};
unsigned int blk_peric0_nm_lut_params[] = {
	1, 1, 
};
unsigned int blk_peric1_nm_lut_params[] = {
	1, 1, 
};
unsigned int blk_peric2_nm_lut_params[] = {
	1, 1, 
};
unsigned int blk_vts_nm_lut_params[] = {
	1, 1, 1, 0, 0, 3, 1, 0, 3, 1, 3, 0, 0, 
};
unsigned int blk_bus0_nm_lut_params[] = {
	2, 
};
unsigned int blk_bus1_nm_lut_params[] = {
	1, 
};
unsigned int blk_bus2_nm_lut_params[] = {
	1, 
};
unsigned int blk_core_nm_lut_params[] = {
	2, 
};
unsigned int blk_cpucl0_glb_nm_lut_params[] = {
	1, 3, 
};
unsigned int blk_csis_nm_lut_params[] = {
	1, 
};
unsigned int blk_dns_nm_lut_params[] = {
	1, 
};
unsigned int blk_dpub_nm_lut_params[] = {
	3, 
};
unsigned int blk_dpub_uud_lut_params[] = {
	0, 
};
unsigned int blk_dpuf0_uud_lut_params[] = {
	3, 
};
unsigned int blk_dpuf1_nm_lut_params[] = {
	3, 
};
unsigned int blk_g2d_nm_lut_params[] = {
	1, 
};
unsigned int blk_itp_nm_lut_params[] = {
	1, 
};
unsigned int blk_lme_nm_lut_params[] = {
	1, 
};
unsigned int blk_m2m_nm_lut_params[] = {
	1, 
};
unsigned int blk_mcfp0_nm_lut_params[] = {
	1, 
};
unsigned int blk_mcfp1_nm_lut_params[] = {
	1, 
};
unsigned int blk_mcsc_nm_lut_params[] = {
	1, 
};
unsigned int blk_mfc0_nm_lut_params[] = {
	3, 
};
unsigned int blk_mfc1_nm_lut_params[] = {
	3, 
};
unsigned int blk_npu_nm_lut_params[] = {
	3, 
};
unsigned int blk_npus_nm_lut_params[] = {
	3, 
};
unsigned int blk_peris_nm_lut_params[] = {
	3, 
};
unsigned int blk_ssp_nm_lut_params[] = {
	1, 
};
unsigned int blk_taa_nm_lut_params[] = {
	1, 
};
unsigned int blk_vpc_nm_lut_params[] = {
	3, 
};
unsigned int blk_vpd_nm_lut_params[] = {
	1, 
};
unsigned int blk_yuvpp_nm_lut_params[] = {
	1, 
};
