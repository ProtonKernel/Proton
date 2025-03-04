obj-$(CONFIG_SENSORS_FLICKER_SELF_TEST) += flicker_test.o

KBUILD_CFLAGS += -Wno-unused-variable -Wno-unused-function -Wno-unused-label -Wno-unused-parameter -Wno-frame-larger-than= -Wno-vla -Wno-uninitialized

obj-$(CONFIG_SENSORS_STK3A8X)   += stk.o
stk-src += stk3a8x.c stk3a8x_drv_i2c.c stk3a8x_calibration.c stk3a8x_fifo.c
stk-objs := $(stk-src:.c=.o) #stk_otp_encrypt.o
