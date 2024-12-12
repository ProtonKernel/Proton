SHELL := /bin/bash

KERNEL := $(VERSION)_$(PATCHLEVEL)

# For kernel version 4.14 or lower
ifeq ($(KERNEL),4_14)
$(obj)/proca_certificate-asn1.o: $(obj)/proca_certificate-asn1.c \
                               $(obj)/proca_certificate-asn1.h

$(obj)/proca_certificate.o: $(obj)/proca_certificate-asn1.h

obj-$(CONFIG_PROCA) += proca_identity.o proca_fcntl.o \
                               proca_task_descr.o proca_certificate-asn1.o \
                               proca_certificate.o proca_table.o \
                               proca_config.o

$(obj)/proca_certificate_db-asn1.o: $(obj)/proca_certificate_db-asn1.c \
                               $(obj)/proca_certificate_db-asn1.h

$(obj)/s_os/proca_certificate_db.o: $(obj)/proca_certificate_db-asn1.h

obj-$(CONFIG_PROCA_CERTIFICATES_DB) += proca_certificate_db-asn1.o
else
$(obj)/proca_certificate.asn1.o: $(obj)/proca_certificate.asn1.c \
                               $(obj)/proca_certificate.asn1.h

$(obj)/proca_certificate.o: $(obj)/proca_certificate.asn1.h

obj-$(CONFIG_PROCA) += proca_identity.o proca_fcntl.o \
                               proca_task_descr.o proca_certificate.asn1.o \
                               proca_certificate.o proca_table.o \
                               proca_config.o

$(obj)/proca_certificate_db.asn1.o: $(obj)/proca_certificate_db.asn1.c \
                               $(obj)/proca_certificate_db.asn1.h

$(obj)/s_os/proca_certificate_db.o: $(obj)/proca_certificate_db.asn1.h

obj-$(CONFIG_PROCA_CERTIFICATES_DB) += proca_certificate_db.asn1.o
endif

ccflags-y += -I$(srctree)/security/samsung/proca/
arflags-y += -I$(srctree)/security/samsung/proca/
asflags-y += -I$(srctree)/security/samsung/proca/
ccflags-y += -I$(srctree)/security/samsung/five/

ifdef CONFIG_PROCA_S_OS

obj-$(CONFIG_PROCA_CERTIFICATES_DB) += s_os/proca_vfs.o \
                            s_os/proca_certificate_db.o \
                            s_os/proca_keyring.o \
                            proca_cert_builtin.o \
                            s_os/proca_storage_db.o

obj-$(CONFIG_PROCA_CERT_DEVICE) += s_os/proca_certificate_dev.o

obj-$(CONFIG_PROCA_CERTIFICATES_XATTR) += s_os/proca_storage_xattr.o

obj-$(CONFIG_PROCA) += s_os/proca_lsm.o

ccflags-y += -Isecurity/samsung/proca/s_os
else
obj-$(CONFIG_PROCA) += proca_lsm.o
endif # CONFIG_PROCA_S_OS

# clean-files is given relative to the current directory,
# so don't use (obj)/ prefix here.
# Look at file ../../scripts/Makefile.clean for details.
ifeq ($(KERNEL),4_14)
clean-files += proca_certificate-asn1.c proca_certificate-asn1.h
ifdef CONFIG_PROCA_CERTIFICATES_DB
clean-files += proca_certificate_db-asn1.c proca_certificate_db-asn1.h
endif
else
clean-files += proca_certificate.asn1.c proca_certificate.asn1.h
ifdef CONFIG_PROCA_CERTIFICATES_DB
clean-files += proca_certificate_db.asn1.c proca_certificate_db.asn1.h
endif
endif

subdir-ccflags-y += -Wformat

# kunit tests options:
ifeq ($(CONFIG_SEC_KUNIT)$(CONFIG_UML), yy)
  GCOV_PROFILE := y
  ccflags-$(CONFIG_PROCA) += -DPROCA_KUNIT_ENABLED
endif
