#
# Copyright (c) 2019 Samsung Electronics Co., Ltd.
#		http://www.samsung.com
#
# Licensed under GPLv2
#

votf-objs := camerapp-votf.o
votf-objs += camerapp-hw-api-votf.o

obj-$(CONFIG_VIDEO_EXYNOS_CAMERA_POSTPROCESS_VOTF)	+= votf.o

ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera-pp/
ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/include
