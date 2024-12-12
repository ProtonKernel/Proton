TARGET = synaptics_ts

ifeq ($(CONFIG_SAMSUNG_PRODUCT_SHIP), y)
$(TARGET)-objs := synaptics_core.o synaptics_interrupt.o synaptics_fn.o synaptics_fw.o synaptics_dump.o synaptics_cmd.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS) += $(TARGET).o
else
$(TARGET)-objs := synaptics_core.o synaptics_interrupt.o synaptics_fn.o synaptics_fw.o synaptics_dump.o synaptics_cmd.o synaptics_vendor.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS) += $(TARGET).o
endif
ccflags-y += -Wformat
