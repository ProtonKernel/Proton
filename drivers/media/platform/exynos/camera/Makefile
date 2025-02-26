fimc-is-objs	:= is-core.o \
		is-spi.o \
		is-debug.o \
		is-pipe.o \
		is-framemgr.o \
		is-groupmgr.o \
		is-votfmgr.o \
		is-resourcemgr.o \
		is-devicemgr.o \
		is-video.o \
		is-video-sensor.o \
		is-video-3aa.o \
		is-video-3aac.o \
		is-video-3aap.o \
		is-video-3aaf.o \
		is-video-3aag.o \
		is-video-3aao.o \
		is-video-3aal.o \
		is-video-lme.o \
		is-video-lmes.o \
		is-video-lmec.o \
		is-video-isp.o \
		is-video-ispc.o \
		is-video-ispp.o \
		is-video-ispt.o \
		is-video-ispg.o \
		is-video-ispv.o \
		is-video-ispw.o \
		is-video-mec.o \
		is-video-orbc.o \
		is-video-mcs.o \
		is-video-mcsp.o \
		is-video-vra.o \
		is-video-clh.o \
		is-video-clhc.o \
		is-video-ssvc0.o \
		is-video-ssvc1.o \
		is-video-ssvc2.o \
		is-video-ssvc3.o \
		is-video-pafrdma.o \
		is-video-ypp.o \
		is-subdev-ctrl.o \
		is-device-ischain.o \
		is-interface-wrap.o \
		is-device-sensor_v2.o \
		is-time.o \
		is-dt.o \
		is-binary.o \
		is-i2c-config.o \
		is-device-camif-dma.o \
		is-uh_call.o

# lib
fimc-is-objs += lib/is-work.o \
		lib/is-fpsimdmacro.o \
		lib/is-fpsimd.o \
		lib/is-irq.o

ifeq ($(CONFIG_EXYNOS_DEVICE_MIPI_CSIS_VER4),y)
fimc-is-objs += is-device-csi_v4.o
endif

ifeq ($(CONFIG_VIDEO_EXYNOS_PABLO_ISP),m)
fimc-is-objs += is-mem_v2.o
else
fimc-is-objs += is-mem.o
endif

ifeq ($(CONFIG_CAMERA_DVFS_V2),y)
fimc-is-objs += is-dvfs_v2.o
else
fimc-is-objs += is-dvfs.o
endif

obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += fimc-is.o
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += sensor/
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += ischain/
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += vendor/
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += hardware/
obj-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += interface/

# hardware
fimc-is-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += hardware/is-hw-control.o \
		hardware/is-hw-3aa.o \
		hardware/is-hw-isp.o \
		hardware/is-hw-dm.o

fimc-is-$(CONFIG_PABLO_V8_1_0) += hardware/is-hw-mcscaler-v3.o \
		hardware/is-hw-djag-v2.o \
		hardware/is-hw-vra.o \
		hardware/is-hw-clahe.o \
		hardware/is-hw-cac.o
fimc-is-$(CONFIG_PABLO_V8_10_0) += hardware/is-hw-mcscaler-v3.o \
		hardware/is-hw-djag-v2.o \
		hardware/is-hw-vra.o \
		hardware/is-hw-clahe.o \
		hardware/is-hw-cac.o
fimc-is-$(CONFIG_PABLO_V8_20_0) += hardware/is-hw-mcscaler-v3.o \
		hardware/is-hw-djag-v2.o \
		hardware/is-hw-vra-v2.o \
		hardware/is-hw-clahe.o \
		hardware/is-hw-cac.o
fimc-is-$(CONFIG_PABLO_V9_1_0) += hardware/is-hw-mcscaler-v3.o \
		hardware/is-hw-djag-v2.o \
		hardware/is-hw-lme.o \
		hardware/is-hw-yuvpp.o \
		hardware/is-hw-common-dma.o \
		hardware/is-hw-cac.o

fimc-is-$(CONFIG_CAMERA_PDP) += hardware/is-hw-pdp.o

# hardware/api
fimc-is-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += hardware/api/is-hw-api-common.o

fimc-is-$(CONFIG_MC_SCALER_V5_0) += hardware/api/is-hw-api-mcscaler-v5_0.o \
		hardware/api/is-hw-api-vra.o
fimc-is-$(CONFIG_MC_SCALER_V8_0) += hardware/api/is-hw-api-mcscaler-v8_0.o \
		hardware/api/is-hw-api-vra.o
fimc-is-$(CONFIG_MC_SCALER_V8_1) += hardware/api/is-hw-api-mcscaler-v8_1.o \
		hardware/api/is-hw-api-vra.o
fimc-is-$(CONFIG_MC_SCALER_V8_10) += hardware/api/is-hw-api-mcscaler-v8_10.o \
		hardware/api/is-hw-api-vra.o
fimc-is-$(CONFIG_MC_SCALER_V8_20) += hardware/api/is-hw-api-mcscaler-v8_20-v8_21.o \
		hardware/api/is-hw-api-vra-v3_2.o
fimc-is-$(CONFIG_MC_SCALER_V9_1) += hardware/api/is-hw-api-mcscaler-v9_1.o \
		hardware/api/is-hw-api-vra.o

fimc-is-$(CONFIG_PDP_V1_0) += hardware/api/is-hw-api-pdp-v1_0.o
fimc-is-$(CONFIG_PDP_V1_0_0) += hardware/api/is-hw-api-pdp-v1_0_0.o
fimc-is-$(CONFIG_PDP_V3_1) += hardware/api/is-hw-api-pdp-v3_1.o
fimc-is-$(CONFIG_YUVPP_V2_1) += hardware/api/is-hw-api-yuvpp-v2_1.o

# interface
fimc-is-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP) += interface/is-interface-ischain.o \
		interface/is-interface-library.o \
		interface/is-interface-ddk.o

fimc-is-$(CONFIG_PABLO_V8_1_0) += interface/is-interface-vra.o
fimc-is-$(CONFIG_PABLO_V8_10_0) += interface/is-interface-vra.o
fimc-is-$(CONFIG_PABLO_V8_20_0) += interface/is-interface-vra-v2.o
fimc-is-$(CONFIG_PABLO_V9_1_0) += interface/is-interface-vra.o

# ischain
fimc-is-$(CONFIG_EXYNOS_PABLO_ISP) += ischain/setup-is-module.o

fimc-is-$(CONFIG_PABLO_V9_1_0) += ischain/is-v9_1_0/setup-is.o \
		ischain/is-v9_1_0/setup-is-sensor.o \
		ischain/is-v9_1_0/is-hw-pwr.o \
		ischain/is-v9_1_0/is-hw-chain.o \
		ischain/is-v9_1_0/is-hw-dvfs.o \
		ischain/is-v9_1_0/is-subdev-3aa.o \
		ischain/is-v9_1_0/is-subdev-3ac.o \
		ischain/is-v9_1_0/is-subdev-3ap.o \
		ischain/is-v9_1_0/is-subdev-3af.o \
		ischain/is-v9_1_0/is-subdev-3ag.o \
		ischain/is-v9_1_0/is-subdev-3ao.o \
		ischain/is-v9_1_0/is-subdev-3al.o \
		ischain/is-v9_1_0/is-subdev-isp.o \
		ischain/is-v9_1_0/is-subdev-ixc.o \
		ischain/is-v9_1_0/is-subdev-ixp.o \
		ischain/is-v9_1_0/is-subdev-ixg.o \
		ischain/is-v9_1_0/is-subdev-ixt.o \
		ischain/is-v9_1_0/is-subdev-ixv.o \
		ischain/is-v9_1_0/is-subdev-ixw.o \
		ischain/is-v9_1_0/is-subdev-mexc.o \
		ischain/is-v9_1_0/is-subdev-orbxc.o \
		ischain/is-v9_1_0/is-subdev-mcs.o \
		ischain/is-v9_1_0/is-subdev-mcsp.o \
		ischain/is-v9_1_0/is-subdev-paf.o \
		ischain/is-v9_1_0/is-subdev-ypp.o \
		ischain/is-v9_1_0/is-subdev-lme.o \
		ischain/is-v9_1_0/is-subdev-lmes.o \
		ischain/is-v9_1_0/is-subdev-lmec.o \
		ischain/is-v9_1_0/is-subdev-sensor.o \
		ischain/is-v9_1_0/is-subdev-sensor_vc0.o \
		ischain/is-v9_1_0/is-subdev-sensor_vc1.o \
		ischain/is-v9_1_0/is-subdev-sensor_vc2.o \
		ischain/is-v9_1_0/is-subdev-sensor_vc3.o \
		ischain/is-v9_1_0/is-votf.o

# sensor/csi2
fimc-is-$(CONFIG_CSIS_V1_2) += sensor/csi2/is-hw-csi-v1_2.o
fimc-is-$(CONFIG_CSIS_V5_2) += sensor/csi2/is-hw-csi-v5_2.o
fimc-is-$(CONFIG_CSIS_V5_3) += sensor/csi2/is-hw-csi-v5_3.o
fimc-is-$(CONFIG_CSIS_V5_4) += sensor/csi2/is-hw-csi-v5_4.o

# sensor/module_framework
fimc-is-$(CONFIG_VIDEO_EXYNOS_PABLO_ISP)	+= sensor/is-helper-i2c.o \
						   sensor/module_framework/is-interface-sensor.o \
						   sensor/module_framework/is-interface-actuator.o \
						   sensor/module_framework/is-control-sensor.o \
						   sensor/module_framework/is-control-actuator.o \
						   sensor/module_framework/i2c_dummy/is-i2c-dummy.o \
						   sensor/module_framework/is-device-sensor-peri.o \
						   sensor/module_framework/modules/is-device-module-base.o \
						   sensor/module_framework/cis/is-cis.o

# vendor
fimc-is-$(CONFIG_CAMERA_VENDER_DEFAULT) += vendor/default/is-vender.o \
				    vendor/default/is-sysfs.o

subdir-ccflags-y := -I$(srctree)/drivers/media/platform/exynos/camera
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/include
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/interface
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/hardware
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/hardware/api
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/vendor
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/sensor/csi
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/sensor/module_framework
subdir-ccflags-$(CONFIG_PABLO_V8_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v8_1_0
subdir-ccflags-$(CONFIG_PABLO_V8_10_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v8_10_0
subdir-ccflags-$(CONFIG_PABLO_V8_20_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v8_20_0
subdir-ccflags-$(CONFIG_PABLO_V9_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/ischain/is-v9_1_0

# For using vendor specific code, please rename default to xxx.
subdir-ccflags-$(CONFIG_CAMERA_VENDER_DEFAULT) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default
ifeq ($(CONFIG_CAMERA_VENDER_DEFAULT),y)
subdir-ccflags-$(CONFIG_PABLO_V8_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default/v8_1_0
subdir-ccflags-$(CONFIG_PABLO_V8_10_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default/v8_10_0
subdir-ccflags-$(CONFIG_PABLO_V8_20_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default/v8_20_0
subdir-ccflags-$(CONFIG_PABLO_V9_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/default/v9_1_0
endif

fimc-is-$(CONFIG_CAMERA_VENDER_MCD) += vendor/mcd/is-vender.o \
				vendor/mcd/is-vender-caminfo.o \
				vendor/mcd/is-sec-define.o \
				vendor/mcd/is-notifier.o \
				vendor/mcd/crc32.o \
				vendor/mcd/is-sysfs.o

fimc-is-$(CONFIG_OIS_USE)			+= vendor/mcd/is-device-ois_common.o
fimc-is-$(CONFIG_AF_HOST_CONTROL)		+= vendor/mcd/is-device-af.o
fimc-is-$(CONFIG_CAMERA_USE_INTERNAL_MCU) 	+= vendor/mcd/is-ois-mcu.o \
						vendor/mcd/is-hw-api-ois-mcu-v1_1_1.o

ifeq ($(CONFIG_CAMERA_VENDER_MCD),y)
subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/mcd
subdir-ccflags-$(CONFIG_PABLO_V8_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/mcd/v8_1_0
subdir-ccflags-$(CONFIG_PABLO_V8_10_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/mcd/v8_10_0
subdir-ccflags-$(CONFIG_PABLO_V8_20_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/mcd/v8_20_0
subdir-ccflags-$(CONFIG_PABLO_V9_1_0) += -I$(srctree)/drivers/media/platform/exynos/camera/vendor/mcd/v9_1_0
endif

subdir-ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera-pp
