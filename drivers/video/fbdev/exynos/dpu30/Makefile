#
# Copyright (c) 2013 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

obj-$(CONFIG_EXYNOS_DPU30) += dpu.o

dpu-y += decon_core.o decon_dsi.o decon_wb.o helper.o win_update.o cursor.o fence.o event_log.o format.o dpu_llc.o
dpu-y += dpp_drv.o dsim_drv.o
dpu-$(CONFIG_UML) += ./cal_2100/dsim_reg.o ./cal_2100/dpp_reg.o ./cal_2100/decon_reg.o
dpu-$(CONFIG_SOC_EXYNOS2100) += ./cal_2100/dsim_reg.o ./cal_2100/dpp_reg.o ./cal_2100/decon_reg.o
dpu-$(CONFIG_SOC_EXYNOS9830) += ./cal_9830/dsim_reg.o ./cal_9830/dpp_reg.o ./cal_9830/decon_reg.o
dpu-$(CONFIG_SOC_EXYNOS9820) += ./cal_9820/dsim_reg.o ./cal_9820/dpp_reg.o ./cal_9820/decon_reg.o
dpu-$(CONFIG_SOC_EXYNOS9610) += ./cal_9610/dsim_reg.o ./cal_9610/dpp_reg.o ./cal_9610/decon_reg.o
ifdef CONFIG_EXYNOS_DISPLAYPORT_MODULE
dpu-y += displayport_drv.o displayport_edid.o displayport_hdcp13.o decon_displayport.o displayport_hdcp22_if.o displayport_aux_client.o displayport_topology.o
dpu-$(CONFIG_UML) += ./cal_2100/displayport_reg.o
dpu-$(CONFIG_UML) += dp_unit_test.o
dpu-$(CONFIG_SOC_EXYNOS2100) += ./cal_2100/displayport_reg.o
dpu-$(CONFIG_SOC_EXYNOS9830) += ./cal_9830/displayport_reg.o
dpu-$(CONFIG_SOC_EXYNOS9820) += ./cal_9820/displayport_reg.o
dpu-$(CONFIG_SOC_EXYNOS9610) += ./cal_9610/displayport_reg.o
GCOV_PROFILE_displayport_edid.o		:=y
GCOV_PROFILE_displayport_drv.o		:=y
endif
ifdef CONFIG_EXYNOS_DECON_DQE
dpu-y += dqe_drv.o
dpu-$(CONFIG_UML) += ./cal_2100/dqe_reg.o
dpu-$(CONFIG_SOC_EXYNOS2100) += ./cal_2100/dqe_reg.o
dpu-$(CONFIG_SOC_EXYNOS9830) += ./cal_9830/dqe_reg.o
endif
ifdef CONFIG_EXYNOS_DMA_DSIMFC
dpu-y += fcmd_drv.o
dpu-$(CONFIG_UML) += ./cal_2100/fcmd_reg.o
dpu-$(CONFIG_SOC_EXYNOS2100) += ./cal_2100/fcmd_reg.o
endif
dpu-$(CONFIG_EXYNOS_BTS) += bts.o
dpu-$(CONFIG_MCDHDR) += ./mcdhdr/mcdhdr_drv.o ./mcdhdr/mcdhdr_help.o

ifneq ($(filter y m,$(CONFIG_MCD_PANEL)),)
dpu-$(CONFIG_MCD_PANEL) += ./panels/common_panel_drv.o\
				./panels/common_mipi_lcd.o\
                            ./mcd_decon.o\
                            ./mcd_dsim.o\
                            ./mcd_dpp.o\
                            ./mcd_helper.o
dpu-$(CONFIG_PANEL_DISPLAY_MODE) += ./panels/exynos_panel_modes.o
else
dpu-$(CONFIG_EXYNOS_COMMON_PANEL) += ./panels/exynos_panel_drv.o\
									./panels/panel-samsung-s6e3hab.o\
									./panels/panel-samsung-s6e3ha9.o\
									./panels/panel-samsung-s6e3ha8.o\
									./panels/panel-samsung-s6e3fa0.o\
									./panels/panel-samsung-s6e3fab.o\
									./panels/panel-samsung-s6e3had.o\
									./panels/panel-samsung-emul.o
endif
