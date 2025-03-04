/* sound/soc/samsung/vts/vts_util.h
 *
 * ALSA SoC - Samsung VTS utility
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SND_SOC_VTS_UTIL_H
#define __SND_SOC_VTS_UTIL_H

#include <sound/pcm.h>
#include <linux/firmware.h>

/**
 * ioremap to virtual address but not request
 * @param[in]	pdev		pointer to platform device structure
 * @param[in]	name		name of resource
 * @param[out]	phys_addr	physical address of the resource
 * @param[out]	size		size of the resource
 * @return	virtual address
 */
extern void __iomem *vts_devm_get_ioremap(struct platform_device *pdev,
		const char *name, phys_addr_t *phys_addr, size_t *size);

/**
 * Request memory resource and map to virtual address
 * @param[in]	pdev		pointer to platform device structure
 * @param[in]	name		name of resource
 * @param[out]	phys_addr	physical address of the resource
 * @param[out]	size		size of the resource
 * @return	virtual address
 */
extern void __iomem *vts_devm_get_request_ioremap(struct platform_device *pdev,
		const char *name, phys_addr_t *phys_addr, size_t *size);

/**
 * Request clock and prepare
 * @param[in]	pdev		pointer to platform device structure
 * @param[in]	name		name of clock
 * @return	pointer to clock
 */
extern struct clk *vts_devm_clk_get_and_prepare(struct platform_device *pdev,
		const char *name);

/**
 * Get property value with samsung, prefix
 * @param[in]	dev		pointer to device invoking this API
 * @param[in]	np		device node
 * @param[in]	propname	name of the property
 * @param[out]	out_value	pointer to return value
 * @return	error code
 */
extern int vts_of_samsung_property_read_u32(struct device *dev,
		const struct device_node *np,
		const char *propname, u32 *out_value);

/**
 * Get property value with samsung, prefix
 * @param[in]	dev		pointer to device invoking this API
 * @param[in]	np		device node
 * @param[in]	propname	name of the property
 * @param[out]	out_values	pointer to return value
 * @param[in]	sz		number of array elements to read
 * @return	error code
 */
extern int vts_of_samsung_property_read_u32_array(struct device *dev,
		const struct device_node *np,
		const char *propname, u32 *out_values, size_t sz);

/**
 * Get property value with samsung, prefix
 * @param[in]	dev		pointer to device invoking this API
 * @param[in]	np		device node
 * @param[in]	propname	name of the property
 * @param[out]	out_string	pointer to return value
 * @return	error code
 */
extern int vts_of_samsung_property_read_string(struct device *dev,
		const struct device_node *np,
		const char *propname, const char **out_string);
#endif /* __SND_SOC_ABOX_UTIL_H */
