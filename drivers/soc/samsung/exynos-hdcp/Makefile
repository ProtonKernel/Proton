#
# Exynos HDCP drivers
#

# HDCP
hdcp2-objs := exynos-hdcp2.o exynos-hdcp2-teeif.o exynos-hdcp2-crypto.o exynos-hdcp2-session.o exynos-hdcp2-misc.o exynos-hdcp2-encrypt.o exynos-hdcp2-protocol-msg.o iia_link/exynos-hdcp2-iia-auth.o iia_link/exynos-hdcp2-iia.o dp_link/exynos-hdcp2-dplink-inter.o dp_link/exynos-hdcp2-dplink.o dp_link/exynos-hdcp2-dplink-if.o dp_link/exynos-hdcp2-dplink-auth.o dp_link/exynos-hdcp2-dplink-protocol-msg.o iia_link/exynos-hdcp2-iia-selftest.o

obj-$(CONFIG_EXYNOS_HDCP2)     += hdcp2.o

exynos-hdcp2-$(HDCP2_EMULATION_MODE) += dp_link/exynos-hdcp2-dplink-selftest.o iia_link/exynos-hdcp2-iia-selftest.o
