ifeq ($(CONFIG_SND_SOC_SAMSUNG_ABOX_V1),y)
snd-soc-samsung-abox-$(CONFIG_SND_SOC_SAMSUNG_ABOX) += abox_soc_3.o abox_cmpnt_3.o
else ifeq ($(CONFIG_SND_SOC_SAMSUNG_ABOX_V2),y)
snd-soc-samsung-abox-$(CONFIG_SND_SOC_SAMSUNG_ABOX) += abox_soc_3.o abox_cmpnt_3.o
else ifeq ($(CONFIG_SND_SOC_SAMSUNG_ABOX_V3),y)
snd-soc-samsung-abox-$(CONFIG_SND_SOC_SAMSUNG_ABOX) += abox_soc_3.o abox_cmpnt_3.o
else
snd-soc-samsung-abox-$(CONFIG_SND_SOC_SAMSUNG_ABOX) += abox_soc.o abox_cmpnt.o
endif

snd-soc-samsung-abox-$(CONFIG_SND_SOC_SAMSUNG_ABOX_UDMA) += abox_udma.o

snd-soc-samsung-abox-gic-$(CONFIG_SND_SOC_SAMSUNG_ABOX) += abox_gic.o abox_util.o

snd-soc-samsung-abox-$(CONFIG_SND_SOC_SAMSUNG_ABOX) += abox_util.o \
	abox_dbg.o abox_dump.o abox_log.o abox.o abox_core.o abox_rdma.o\
	abox_wdma.o abox_if.o abox_effect.o abox_vss.o abox_failsafe.o \
	abox_vdma.o abox_msg.o abox_ipc.o abox_qos.o abox_ion.o abox_tplg.o \
	abox_dma.o abox_pci.o abox_shm.o abox_atune.o abox_proc.o

obj-$(CONFIG_SND_SOC_SAMSUNG_ABOX) += snd-soc-samsung-abox-gic.o snd-soc-samsung-abox.o

obj-$(CONFIG_SEC_SND_ADAPTATION) += abox_adaptation.o

GCOV_PROFILE_abox.o	:=y
GCOV_PROFILE_abox_shm.o	:=y
GCOV_PROFILE_abox_cmpnt.o	:=y
