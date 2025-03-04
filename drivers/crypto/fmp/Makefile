# Exynos FMP makefile
CONFIG_EXYNOS_FMP_FIPS_FUNC_TEST=n

ifeq ($(CONFIG_EXYNOS_FMP_FIPS), m)
FIPS_PR = fipsed_
endif

ccflags-$(CONFIG_EXYNOS_FMP_FIPS_FUNC_TEST) += -DCONFIG_EXYNOS_FMP_FIPS_FUNC_TEST

obj-$(CONFIG_EXYNOS_FMP)   += fmp-core.o
fmp-core-$(CONFIG_EXYNOS_FMP) += $(FIPS_PR)fmp.o
fmp-core-$(CONFIG_EXYNOS_FMP_FIPS) += $(FIPS_PR)fmp_test.o \
				$(FIPS_PR)fmp_fips_main.o \
				$(FIPS_PR)fmp_fips_fops.o \
				$(FIPS_PR)fmp_fips_selftest.o \
				$(FIPS_PR)fmp_fips_integrity.o \
				$(FIPS_PR)hmac-sha256.o \
				$(FIPS_PR)fmp_fips_cipher.o \
				$(FIPS_PR)sha256.o

ifeq ($(CONFIG_EXYNOS_FMP_FIPS), m)
fmp-core-$(CONFIG_EXYNOS_FMP_FIPS) += fips140_ic_support.o
endif

fmp-core-$(CONFIG_EXYNOS_FMP_FIPS_FUNC_TEST) += fmp_fips_func_test.o

ifeq ($(CONFIG_EXYNOS_FMP_FIPS), m)
FMP_OUT_DIR        := $(objtree)/drivers/crypto/fmp/
FMP_SRC_DIR        := $(srctree)/drivers/crypto/fmp/
FMP_OBJS_LIST      := $(patsubst $(FIPS_PR)%,%, $(filter-out fips140_ic_support.o,$(fmp-core-m)))
FMP_SRCS_LIST      := $(patsubst %.o,%.c, $(FMP_OBJS_LIST))
FMP_SRCS_LIST_PATH := $(addprefix $(FMP_SRC_DIR), $(FMP_SRCS_LIST))
FIPS_OBJS_ELF_LIST      := $(addprefix elf_, $(FMP_OBJS_LIST))
FIPS_OBJS_ELF_LIST_PATH := $(addprefix $(FMP_OUT_DIR), $(FIPS_OBJS_ELF_LIST))
FIPS_FIPSED_SRCS_LIST   := $(addprefix $(FIPS_PR), $(FMP_SRCS_LIST))
FIPS_FIPSED_SRCS_LIST_PATH  := $(addprefix $(src)/, $(FIPS_FIPSED_SRCS_LIST))

$(FIPS_FIPSED_SRCS_LIST_PATH): $(src)/fips140_ic_support.c

elf_%.o: %.c
	$(eval FIPS_FLAGS := $(filter-out -flto=thin,$(c_flags)))
	$(eval FIPS_FLAGS := $(filter-out -fsanitize=cfi,$(FIPS_FLAGS)))
	@echo " FIPS generate ELF object for $*.c"
	@$(CC) $(FIPS_FLAGS) -c -o $@ $<

$(src)/fips140_ic_support.c : $(FIPS_OBJS_ELF_LIST_PATH)
	@for _S in $(FMP_SRCS_LIST); do									\
		cp -f $(FMP_SRC_DIR)/$$_S $(FMP_OUT_DIR)/$(FIPS_PR)$$_S;	\
		chmod a+w $(FMP_OUT_DIR)/$(FIPS_PR)$$_S;					\
	done
	@$(srctree)/scripts/fmp/IntegrityCheckCodeGen.py $(FMP_OUT_DIR)/fips140_ic_support.c	\
													$(FIPS_OBJS_ELF_LIST_PATH)
endif

.PHONY: fips_clean
fips_clean:
	@echo " Clean FIPS module artefacts .... "
	@rm -f $(objtree)/drivers/crypto/fmp/fipsed_*.c
	@rm -f $(objtree)/drivers/crypto/fmp/fips140_ic_support.c
	@rm -f $(objtree)/drivers/crypto/fmp/*.o
	@sync
	@echo " Clean FIPS module artefacts completed "