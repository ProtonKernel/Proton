obj-$(CONFIG_PABLO_V8_10_0) += setup-is.o
obj-$(CONFIG_PABLO_V8_10_0) += setup-is-sensor.o
obj-$(CONFIG_PABLO_V8_10_0) += is-hw-pwr.o
obj-$(CONFIG_PABLO_V8_10_0) += is-hw-chain.o
ifeq ($(CONFIG_PM_DEVFREQ),y)
obj-$(CONFIG_PABLO_V8_10_0) += is-hw-dvfs.o
endif

obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-3aa.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-3ac.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-3ap.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-3af.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-3ag.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-isp.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-ixc.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-ixp.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-ixg.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-ixt.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-ixv.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-ixw.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-mexc.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-orbxc.o

obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-mcs.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-mcsp.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-vra.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-paf.o

obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-clh.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-clxc.o

ifeq ($(CONFIG_USE_SENSOR_GROUP),y)
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-sensor.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-sensor_vc0.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-sensor_vc1.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-sensor_vc2.o
obj-$(CONFIG_PABLO_V8_10_0) += is-subdev-sensor_vc3.o
endif

ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera
ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera/include
ccflags-y += -I$(srctree)/drivers/media/platform/exynos/camera-pp
