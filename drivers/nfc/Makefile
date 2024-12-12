# SPDX-License-Identifier: GPL-2.0
#
# Makefile for nfc devices
#
ifneq ($(SEC_BUILD_CONF_USE_ESE_TZ), false)
	ccflags-y += -DENABLE_ESE_SPI_SECURED
endif

ifeq ($(CONFIG_SAMSUNG_NFC),m)
obj-$(CONFIG_SAMSUNG_NFC)		+= nfc_sec.o
nfc_sec-$(CONFIG_NFC_PN547_ESE_SUPPORT)	+= p61.o
nfc_sec-$(CONFIG_NFC_FEATURE_SN100U)	+= cold_reset.o
nfc_sec-$(CONFIG_SEC_NFC_LOGGER)	+= nfc_logger/nfc_logger.o
nfc_sec-$(CONFIG_NFC_PN547)		+= pn547.o
else
obj-$(CONFIG_NFC_PN547)			+= pn547.o
obj-$(CONFIG_NFC_PN547_ESE_SUPPORT)	+= p61.o
obj-$(CONFIG_NFC_FEATURE_SN100U)	+= cold_reset.o
obj-$(CONFIG_SEC_NFC)			+= sec_nfc.o
obj-$(CONFIG_ESE_P3_LSI)		+= ese_p3.o
obj-$(CONFIG_SEC_NFC_LOGGER)		+= nfc_logger/nfc_logger.o
endif
ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG

ifeq ($(CONFIG_SEC_STAR), m)
obj-$(CONFIG_SEC_STAR) = sec-star/sec_star.o

sec_star-y += \
                        sec-star/sec_star.o \
                        sec-star/protocol/ese_data.o \
                        sec-star/protocol/ese_iso7816_t1.o \
                        sec-star/hal/ese_i2c.o \
                        sec-star/hal/ese_spi.o \
                        sec-star/hal/ese_hal.o

sec_star-$(CONFIG_STAR_K250A) += sec-star/sec_k250a.o
else
obj-$(CONFIG_SEC_STAR) += sec-star/sec_star.o \
                        sec-star/protocol/ese_data.o \
                        sec-star/protocol/ese_memory.o \
                        sec-star/protocol/ese_iso7816_t1.o \
                        sec-star/hal/ese_i2c.o \
                        sec-star/hal/ese_spi.o \
                        sec-star/hal/ese_hal.o

obj-$(CONFIG_STAR_K250A) += sec-star/sec_k250a.o
endif
