
#pragma once

extern int phy_exynos_usbdp_g2_v4_enable(struct exynos_usbphy_info *info);
extern void phy_exynos_usbdp_g2_v4_disable(struct exynos_usbphy_info *info);
extern void phy_exynos_usbdp_g2_v4_eom(struct exynos_usbphy_info *info, struct usb_eom_result_s *eom_result, u32 cmn_rate);
extern int phy_exynos_usb3p1_rewa_enable(struct exynos_usbphy_info *info);
extern void phy_exynos_usbdp_g2_v4_tune(struct exynos_usbphy_info *info);
