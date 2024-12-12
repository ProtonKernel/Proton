#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <sound/pcm.h>

#include "vts_util.h"

void __iomem *vts_devm_get_ioremap(struct platform_device *pdev,
		const char *name, phys_addr_t *phys_addr, size_t *size)
{
	struct resource *res;
	void __iomem *ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (IS_ERR_OR_NULL(res)) {
		dev_err(&pdev->dev, "Failed to get %s\n", name);
		return ERR_PTR(-EINVAL);
	}
	if (phys_addr)
		*phys_addr = res->start;
	if (size)
		*size = resource_size(res);

	ret = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR_OR_NULL(ret)) {
		dev_err(&pdev->dev, "Failed to map %s\n", name);
		return ERR_PTR(-EFAULT);
	}

	return ret;
}

void __iomem *vts_devm_get_request_ioremap(struct platform_device *pdev,
		const char *name, phys_addr_t *phys_addr, size_t *size)
{
	struct resource *res;
	void __iomem *ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (IS_ERR_OR_NULL(res)) {
		dev_err(&pdev->dev, "Failed to get %s\n", name);
		return ERR_PTR(-EINVAL);
	}
	if (phys_addr)
		*phys_addr = res->start;
	if (size)
		*size = resource_size(res);

	res = devm_request_mem_region(&pdev->dev, res->start,
			resource_size(res), name);
	if (IS_ERR_OR_NULL(res)) {
		dev_err(&pdev->dev, "Failed to request %s\n", name);
		return ERR_PTR(-EFAULT);
	}

	ret = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR_OR_NULL(ret)) {
		dev_err(&pdev->dev, "Failed to map %s\n", name);
		return ERR_PTR(-EFAULT);
	}

	return ret;
}

struct clk *vts_devm_clk_get_and_prepare(struct platform_device *pdev,
		const char *name)
{
	struct device *dev = &pdev->dev;
	struct clk *clk;
	int ret;

	clk = devm_clk_get(dev, name);
	if (IS_ERR(clk)) {
		dev_err(dev, "Failed to get clock %s\n", name);
		goto error;
	}

	ret = clk_prepare(clk);
	if (ret < 0) {
		dev_err(dev, "Failed to prepare clock %s\n", name);
		goto error;
	}

error:
	return clk;
}

int vts_of_samsung_property_read_u32(struct device *dev,
		const struct device_node *np,
		const char *propname, u32 *out_value)
{
	char name[SZ_64];
	int ret;

	snprintf(name, sizeof(name), "samsung,%s", propname);
	ret = of_property_read_u32(np, name, out_value);
	if (ret < 0)
		dev_dbg(dev, "Failed to read %s: %d\n", name, ret);

	return ret;
}

int vts_of_samsung_property_read_u32_array(struct device *dev,
		const struct device_node *np,
		const char *propname, u32 *out_values, size_t sz)
{
	char name[SZ_64];
	int ret;

	snprintf(name, sizeof(name), "samsung,%s", propname);
	ret = of_property_read_u32_array(np, name, out_values, sz);
	if (ret < 0)
		dev_dbg(dev, "Failed to read %s: %d\n", name, ret);

	return ret;
}

int vts_of_samsung_property_read_string(struct device *dev,
		const struct device_node *np,
		const char *propname, const char **out_string)
{
	char name[SZ_64];
	int ret;

	snprintf(name, sizeof(name), "samsung,%s", propname);
	ret = of_property_read_string(np, name, out_string);
	if (ret < 0)
		dev_warn(dev, "Failed to read %s: %d\n", name, ret);

	return ret;
}
