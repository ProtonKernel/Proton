# NXP audio amplifier - TFA98xx

TFA_VERSION              =tfa9874

TFADSP_DSP_BUFFER_POOL=1
TFA_EXCEPTION_AT_TRANSITION=1
TFA_USE_DEVICE_SPECIFIC_CONTROL=1
TFA_PROFILE_ON_DEVICE    =1
TFA_SOFTDSP              =1
TFA_SRC_DIR              =$(srctree)/$(src)

# cc flags
ccflags-y               := -DDEBUG
ccflags-y               += -DTFA98XX_GIT_VERSIONS
ccflags-y               += -I$(TFA_SRC_DIR)/inc
ccflags-y               += -Werror
#ccflags-y               += -DTFA_DEBUG

EXTRA_CFLAGS += -I$(TFA_SRC_DIR)/inc

snd-soc-tfa98xx-objs += src/tfa98xx.o
snd-soc-tfa98xx-objs += src/tfa_container.o
snd-soc-tfa98xx-objs += src/tfa_dsp.o
snd-soc-tfa98xx-objs += src/tfa_init.o

# built-in driver
obj-$(CONFIG_SND_SOC_TFA9874)	+= snd-soc-tfa98xx.o
