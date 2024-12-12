#
# Makefile for the dsp common driver
#

ccflags-y := -Wformat
subdir-ccflags-y := -Wformat

ccflags-y += -I$(srctree)/drivers/vision/dsp
ccflags-y += -I$(srctree)/drivers/vision/dsp/utils

dsp-objs			:= dsp-device.o
dsp-objs			+= dsp-context.o
dsp-objs			+= dsp-core.o
dsp-objs			+= dsp-ioctl.o
ifeq ($(CONFIG_COMPAT),y)
dsp-objs			+= dsp-compat-ioctl.o
endif
dsp-objs			+= dsp-debug.o
dsp-objs			+= dsp-binary.o
dsp-objs			+= dsp-task.o
dsp-objs			+= dsp-kernel.o
dsp-objs			+= dsp-graph.o
dsp-objs			+= dsp-util.o
dsp-objs			+= dsp-time.o

dsp-objs			+= dsp-npu.o

dsp-objs			+= dl/dsp-common.o
dsp-objs			+= dl/dsp-dl-engine.o
dsp-objs			+= dl/dsp-dl-linker.o
dsp-objs			+= dl/dsp-dl-out-manager.o
dsp-objs			+= dl/dsp-elf-loader.o
dsp-objs			+= dl/dsp-gpt-manager.o
dsp-objs			+= dl/dsp-hash.o
dsp-objs			+= dl/dsp-lib-manager.o
dsp-objs			+= dl/dsp-list.o
dsp-objs			+= dl/dsp-llstack.o
dsp-objs			+= dl/dsp-pm-manager.o
dsp-objs			+= dl/dsp-rule-reader.o
dsp-objs			+= dl/dsp-string-tree.o
dsp-objs			+= dl/dsp-sxml.o
dsp-objs			+= dl/dsp-tlsf-allocator.o
dsp-objs			+= dl/dsp-xml-parser.o

dsp-objs			+= hardware/dsp-system.o
dsp-objs			+= hardware/dsp-pm.o
dsp-objs			+= hardware/dsp-clk.o
dsp-objs			+= hardware/dsp-bus.o
dsp-objs			+= hardware/dsp-llc.o
dsp-objs			+= hardware/dsp-interface.o
dsp-objs			+= hardware/dsp-debug.o
dsp-objs			+= hardware/dsp-ctrl.o
dsp-objs			+= hardware/dsp-memory.o
dsp-objs			+= hardware/dsp-mailbox.o
dsp-objs			+= hardware/dsp-governor.o
dsp-objs			+= hardware/dsp-dump.o

dsp-objs			+= hardware/dummy/dsp-hw-dummy-system.o
dsp-objs			+= hardware/dummy/dsp-hw-dummy-pm.o
dsp-objs			+= hardware/dummy/dsp-hw-dummy-clk.o
dsp-objs			+= hardware/dummy/dsp-hw-dummy-bus.o
dsp-objs			+= hardware/dummy/dsp-hw-dummy-llc.o
dsp-objs			+= hardware/dummy/dsp-hw-dummy-interface.o
dsp-objs			+= hardware/dummy/dsp-hw-dummy-debug.o
dsp-objs			+= hardware/dummy/dsp-hw-dummy-ctrl.o
dsp-objs			+= hardware/dummy/dsp-hw-dummy-memory.o
dsp-objs			+= hardware/dummy/dsp-hw-dummy-mailbox.o
dsp-objs			+= hardware/dummy/dsp-hw-dummy-governor.o
dsp-objs			+= hardware/dummy/dsp-hw-dummy-dump.o

dsp-objs			+= hardware/common/dsp-hw-common-system.o
dsp-objs			+= hardware/common/dsp-hw-common-pm.o
dsp-objs			+= hardware/common/dsp-hw-common-clk.o
dsp-objs			+= hardware/common/dsp-hw-common-bus.o
dsp-objs			+= hardware/common/dsp-hw-common-llc.o
dsp-objs			+= hardware/common/dsp-hw-common-interface.o
dsp-objs			+= hardware/common/dsp-hw-common-debug.o
dsp-objs			+= hardware/common/dsp-hw-common-ctrl.o
dsp-objs			+= hardware/common/dsp-hw-common-memory.o
dsp-objs			+= hardware/common/dsp-hw-common-mailbox.o
dsp-objs			+= hardware/common/dsp-hw-common-governor.o
dsp-objs			+= hardware/common/dsp-hw-common-dump.o

dsp-objs			+= hardware/O1/dsp-hw-o1-system.o
dsp-objs			+= hardware/O1/dsp-hw-o1-pm.o
dsp-objs			+= hardware/O1/dsp-hw-o1-clk.o
dsp-objs			+= hardware/O1/dsp-hw-o1-bus.o
dsp-objs			+= hardware/O1/dsp-hw-o1-llc.o
dsp-objs			+= hardware/O1/dsp-hw-o1-interface.o
dsp-objs			+= hardware/O1/dsp-hw-o1-debug.o
dsp-objs			+= hardware/O1/dsp-hw-o1-ctrl.o
dsp-objs			+= hardware/O1/dsp-hw-o1-memory.o
dsp-objs			+= hardware/O1/dsp-hw-o1-mailbox.o
dsp-objs			+= hardware/O1/dsp-hw-o1-governor.o
dsp-objs			+= hardware/O1/dsp-hw-o1-dump.o

obj-$(CONFIG_EXYNOS_DSP)	+= dsp.o
