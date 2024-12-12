#ifndef _UFS_VS_HANDLE_H_
#define _UFS_VS_HANDLE_H_

/* If not matched with ufs-cal-if, compling would fail */
#define UFS_VS_MMIO_VER 1

struct ufs_vs_handle {
	void *std;	/* Need to care conditions */
	void *hci;
	void *ufsp;
	void *unipro;
	void *pma;
	void *cport;
	void (*udelay)(u32 us);
	void *private;
};

#define EXYNOS_UFS_MMIO_FUNC(name)					\
static inline void name##_writel(struct ufs_vs_handle *handle, u32 val, u32 ofs)	\
{									\
	writel(val, handle->name + ofs);				\
}									\
static inline u32 name##_readl(struct ufs_vs_handle *handle, u32 ofs)	\
{									\
	return readl(handle->name + ofs);				\
}

EXYNOS_UFS_MMIO_FUNC(std);
EXYNOS_UFS_MMIO_FUNC(hci);
EXYNOS_UFS_MMIO_FUNC(ufsp);
EXYNOS_UFS_MMIO_FUNC(unipro);
EXYNOS_UFS_MMIO_FUNC(cport);

#if defined(__UFS_CAL_FW__)

#undef CLKSTOP_CTRL
#undef MPHY_APBCLK_STOP

#define	CLKSTOP_CTRL		0x11B0
#define	MPHY_APBCLK_STOP	(1<<3)

static inline void pma_writel(struct ufs_vs_handle *handle, u32 val, u32 ofs)
{
	u32 clkstop_ctrl = readl(handle->hci + CLKSTOP_CTRL);

	writel(clkstop_ctrl & ~MPHY_APBCLK_STOP, handle->hci + CLKSTOP_CTRL);
	writel(val, handle->pma + ofs);
	writel(clkstop_ctrl | MPHY_APBCLK_STOP, handle->hci + CLKSTOP_CTRL);
}

static inline u32 pma_readl(struct ufs_vs_handle *handle, u32 ofs)
{
	u32 clkstop_ctrl = readl(handle->hci + CLKSTOP_CTRL);
	u32 val;

	writel(clkstop_ctrl & ~MPHY_APBCLK_STOP, handle->hci + CLKSTOP_CTRL);
	val = readl(handle->pma + ofs);
	writel(clkstop_ctrl | MPHY_APBCLK_STOP, handle->hci + CLKSTOP_CTRL);
	return val;
}
#else
EXYNOS_UFS_MMIO_FUNC(pma);
#endif

#endif /* _UFS_VS_HANDLE_H_ */
