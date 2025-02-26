#
# Copyright (c) 2016 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

gdc-objs := camerapp-gdc-core.o \
		camerapp-gdc-grid.o

obj-$(CONFIG_VIDEO_EXYNOS_CAMERA_POSTPROCESS_GDC)	+= gdc.o
gdc-$(CONFIG_CAMERA_PP_GDC_V1_0_0_OBJ)	+= camerapp-hw-api-gdc-v100.o
gdc-$(CONFIG_CAMERA_PP_GDC_V2_0_0_OBJ)	+= camerapp-hw-api-gdc-v200.o
gdc-$(CONFIG_CAMERA_PP_GDC_V2_1_0_OBJ)	+= camerapp-hw-api-gdc-v210.o
gdc-$(CONFIG_CAMERA_PP_GDC_V3_0_0_OBJ)	+= camerapp-hw-api-gdc-v300.o
gdc-$(CONFIG_CAMERA_PP_GDC_V3_1_0_OBJ)  += camerapp-hw-api-gdc-v301.o
gdc-$(CONFIG_CAMERA_PP_GDC_V3_10_0_OBJ)	+= camerapp-hw-api-gdc-v310.o
gdc-$(CONFIG_CAMERA_PP_GDC_V4_1_0_OBJ)	+= camerapp-hw-api-gdc-v401.o

ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera-pp/
ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/include
