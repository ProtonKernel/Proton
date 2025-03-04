obj-$(CONFIG_EXYNOS_DSP)	+= dsp/

#
# Makefile for the npu device driver
#

ccflags-y += -I$(srctree)/drivers/vision/vision-core/include -Wformat
ccflags-y += -I$(srctree)/drivers/vision/npu/core -Wformat
ccflags-y += -I$(srctree)/drivers/vision/npu/core/include -Wformat
ccflags-y += -I$(srctree)/drivers/vision/npu/soc/1000/include -Wformat

ifeq ($(CONFIG_NPU_HARDWARE),y)
ccflags-y += -I$(srctree)/drivers/vision/npu/core/interface/hardware
else # CONFIG NPU_LOOPBACK
ccflags-y += -I$(srctree)/drivers/vision/npu/core/interface/loopback
endif

ifeq ($(CONFIG_NPU_STM),y)
ccflags-y += -I$(srctree)/drivers/hwtracing/coresight
endif

npu-objs				+= vision-core/vision-dev.o
npu-objs				+= vision-core/vision-compat-ioctl32.o
npu-objs				+= vision-core/vision-ioctl.o
npu-objs				+= vision-core/vision-buffer.o
npu-objs				+= vision-core/vision-log.o

npu-objs				+= npu/core/npu-util-autosleepthr.o
npu-objs				+= npu/core/npu-util-liststatemgr.o
npu-objs				+= npu/core/npu-util-msgidgen.o
npu-objs				+= npu/core/npu-util-regs.o
npu-objs				+= npu/core/npu-util-llq.o
npu-objs				+= npu/core/npu-core.o
npu-objs				+= npu/core/npu-clock.o
npu-objs				+= npu/core/npu-util-common.o
npu-objs				+= npu/core/npu-llc.o
npu-objs				+= npu/core/npu-log.o
npu-objs				+= npu/core/npu-ver.o
npu-objs				+= npu/core/npu-debug.o
npu-objs				+= npu/core/npu-device.o
npu-objs				+= npu/core/npu-vertex.o
npu-objs				+= npu/core/npu-binary.o
npu-objs				+= npu/core/npu-queue.o
npu-objs				+= npu/core/npu-qos.o
npu-objs				+= npu/core/npu-system.o
npu-objs				+= npu/core/npu-memory.o
npu-objs				+= npu/core/npu-protodrv.o
npu-objs				+= npu/core/npu-session.o
npu-objs				+= npu/core/npu-fw-test-handler.o
npu-objs				+= npu/core/npu-queue.o
npu-objs				+= npu/core/npu-if-session-protodrv.o
npu-objs				+= npu/core/npu-if-protodrv-mbox2.o
npu-objs				+= npu/core/npu-sessionmgr.o
npu-objs				+= npu/core/npu-config.o

ifeq ($(CONFIG_NPU_INTERFACE),y)
ifeq ($(CONFIG_NPU_HARDWARE),y)
npu-objs				+= npu/core/interface/hardware/npu-interface.o
npu-objs				+= npu/core/interface/hardware/mailbox_ipc.o
else # CONFIG_NPU_LOOPBACK
npu-objs				+= npu/core/interface/loopback/npu-interface.o
npu-objs				+= npu/core/interface/loopback/mailbox_ipc.o
endif
endif

ifeq ($(CONFIG_NPU_GOLDEN_MATCH),y)
npu-objs			        += npu/core/npu-golden.o
npu-objs				+= npu/core/npu-util-statekeeper.o
npu-objs				+= npu/core/npu-util-comparator.o
endif

ifeq ($(CONFIG_FIRMWARE_SRAM_DUMP_DEBUGFS),y)
npu-objs				+= npu/core/npu-util-memdump.o
endif

ifeq ($(CONFIG_NPU_SCHEDULER_GOVERNOR),y)
npu-objs				+= npu/core/npu-scheduler.o
npu-objs				+= npu/core/npu-scheduler-governor.o
ifeq ($(CONFIG_NPU_GOVERNOR_SIMPLE_EXYNOS),y)
npu-objs				+= npu/core/npu-scheduler-governor-simple-exynos.o
endif
ifeq ($(CONFIG_NPU_GOVERNOR_EXYNOS_INTERACTIVE),y)
npu-objs				+= npu/core/npu-scheduler-governor-exynos-interactive.o
endif
ifeq ($(CONFIG_NPU_GOVERNOR_USERSPACE),y)
npu-objs				+= npu/core/npu-scheduler-governor-userspace.o
endif
endif

ifeq ($(CONFIG_NPU_USE_SPROFILER),y)
npu-objs				+= npu/core/npu-profile.o
endif

ifeq ($(CONFIG_EXYNOS_NPU_PUBLISH_NPU_BUILD_VER),y)
npu-objs				+= npu/core/generated/npu-ver-info.o
endif

ifeq ($(CONFIG_NPU_STM),y)
npu-objs				+= npu/core/npu-stm.o
endif

obj-$(CONFIG_EXYNOS_NPU)		+= npu.o
