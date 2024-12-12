/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/syscalls.h>
#include <linux/vmalloc.h>
#include <asm/cacheflush.h>

#include "npu-binary.h"
#include "npu-log.h"
#include "npu-device.h"
#include "npu-scheduler.h"
#include "npu-common.h"
#ifdef CONFIG_NPU_FW_HEADER_FILE
#include "npu-firmware.h"
#endif

/*
static noinline_for_stack long __get_file_size(struct file *file)
{
	if (!S_ISREG(file->f_inode->i_mode)) {
		return -1;
	}
	return i_size_read(file->f_inode);
}
*/

int npu_imgloader_probe(struct npu_binary *binary, struct device *dev)
{
	int ret = 0;

	binary->imgloader.dev = dev;
	binary->imgloader.owner = THIS_MODULE;
	binary->imgloader.ops = &npu_imgloader_ops;
	binary->imgloader.fw_name = NPU_FW_NAME;
	binary->imgloader.name = "NPUS";
	binary->imgloader.fw_id = 0;

	ret = imgloader_desc_init(&binary->imgloader);
	if (ret)
		probe_err("imgloader_desc_init is fail(%d)\n", ret);

	return ret;
}

int npu_binary_init(struct npu_binary *binary,
	struct device *dev,
	char *fpath1,
	char *fpath2,
	char *fname)
{
	BUG_ON(!binary);
	BUG_ON(!dev);

	binary->dev = dev;
	binary->image_size = 0;
	snprintf(binary->fpath1, sizeof(binary->fpath1), "%s%s", fpath1, fname);
	snprintf(binary->fpath2, sizeof(binary->fpath2), "%s%s", fpath2, fname);

	return 0;
}

int npu_binary_g_size(struct npu_binary *binary, size_t *size)
{
	npu_err("This function is not support for gki, remove disk IO");
	*size = 0;
	return -ENOTSUPP;
/*
	int ret;
	mm_segment_t old_fs;
	struct file *fp = NULL;
	size_t fsize = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	fp = // remove open
	if (IS_ERR_OR_NULL(fp)) {
		set_fs(old_fs);
		ret = -EINVAL;
		fp = NULL;
		goto p_err;
	}

	fsize = __get_file_size(fp);
	if (fsize <= 0) {
		npu_err("fail(%ld) in __get_file_size\n", fsize);
		ret = -EINVAL;
		goto p_err;
	}
	ret = 0;

p_err:
	if (fp)
		remove close
	set_fs(old_fs);
	*size = fsize;
	return ret;
*/
}

#define MAX_SIGNATURE_LEN       128
#define FW_SIGNATURE_LEN        528
#define SIGNATURE_HEADING       "NPU Firmware signature : "
static void print_fw_signature(char *fw_addr, size_t fw_size)
{
	char buf[MAX_SIGNATURE_LEN];            /* Buffer to hold the signature */
	char *p_fw;             /* Pointer over fw_addr */
	char *p_buf;            /* Pointer over buf */

	/* Put pointer at last position */
	buf[MAX_SIGNATURE_LEN - 1] = '\0';
	p_fw = (fw_addr + fw_size) - FW_SIGNATURE_LEN - 1;
	p_buf = &buf[MAX_SIGNATURE_LEN - 2];

	if (*p_fw-- != ']') {
		/* No signature */
		npu_info(SIGNATURE_HEADING "none\n");
		return;
	}

	/* Extract signature */
	for (; p_fw > fw_addr && p_buf > buf && *p_fw != '['; p_buf--, p_fw--)
		*p_buf = *p_fw;

	/* Print out */
	/* *p_buf == '[' if everything was OK */
	if (p_buf > buf)
		npu_info(SIGNATURE_HEADING "%s\n", p_buf + 1);
	else
		npu_info(SIGNATURE_HEADING "invalid\n");
}

static long __npu_binary_read(struct npu_binary *binary, void *target, size_t target_size, int mode)
{
	npu_err("This function is not support for gki, remove disk IO");
	return -ENOTSUPP;
/*
	int ret = 0;
	const struct firmware *fw_blob;
	u8 *buf = NULL;
	struct file *fp;
	mm_segment_t old_fs;
	long fsize;
	char *fpath = NULL;
	char *npu_fw_path = NULL;

	BUG_ON(!binary);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fpath = binary->fpath1;
	fp = // remove open
	if (IS_ERR_OR_NULL(fp)) {
		fpath = binary->fpath2;
		fp = // remove open
		if (IS_ERR_OR_NULL(fp)) {
			set_fs(old_fs);
			goto fw_header_file;
		}
	}

	fsize = __get_file_size(fp);
	if (fsize <= 0) {
		npu_err("fail(%ld) in __get_file_size\n", fsize);
		ret = -EBADF;
		goto p_err;
	}

	buf = vmalloc(fsize);
	if (!buf) {
		npu_err("fail in vmalloc\n");
		ret = -ENOMEM;
		goto p_err;
	}

	// To Do  add read image - driver should never be writing directly to the disk
	//	npu_err("fail(%ld != %ld) in \n", nread, fsize);
	//	ret = -EIO;
	//	goto p_err;
	//}

	if (fsize > target_size) {
		npu_err("size over(%ld > %ld) for image\n", fsize, target_size);
		ret = -EIO;
		goto p_err;
	}

	binary->image_size = fsize;
	// no cache operation, because target is sram of vpu
#if 1//def CONFIG_EXYNOS_VPU_HARDWARE
	memcpy(target, (void *)buf, fsize);
#endif
	npu_info("success of fw(%s, %ld) apply.\n", fpath, fsize);
	ret = fsize;

	// Cache invalidation
	//__dma_map_area(target, fsize, DMA_TO_DEVICE);

p_err:
	vfree(buf);
	// remove close
	set_fs(old_fs);

	return ret;
fw_header_file:
#ifdef CONFIG_NPU_FW_HEADER_FILE
furture work
        switch (mode) {
        case NPU_PERF_MODE_NPU_BOOST:
                npu_fw_path = (char *)NPU_perf_bin;
                fsize = (long)NPU_perf_bin_len;
                break;
        case NPU_PERF_MODE_NPU_DN:
                npu_fw_path = (char *)NPU_dn_bin;
                fsize = (long)NPU_dn_bin_len;
                break;
        default:
                npu_fw_path = (char *)NPU_bin;
                fsize = (long)NPU_bin_len;
                break;
        }

        binary->image_size = fsize;
        memcpy(target, (void *)npu_fw_path, fsize);
        ret = fsize;

//request_fw:
        binary->image_size = NPU_bin_len;
        memcpy(target, (void *)NPU_bin, NPU_bin_len);
        ret = NPU_bin_len;

        npu_info("success of fw_header_file(NPU.bin, %ld) apply.\n", NPU_bin_len);
        return ret;
#endif
	switch (mode) {
	case NPU_PERF_MODE_NPU_BOOST:
		npu_fw_path = NPU_FW_PATH4 NPU_PERF_FW_NAME;
		break;
	case NPU_PERF_MODE_NPU_DN:
		npu_fw_path = NPU_FW_PATH5 NPU_DN_FW_NAME;
		break;
	default:
		npu_fw_path = NPU_FW_PATH3 NPU_FW_NAME;
		break;
	}
	ret = request_firmware(&fw_blob, npu_fw_path, binary->dev);
	if (ret) {
		npu_err("fail(%d) in request_firmware(%s)", ret, npu_fw_path);
		ret = -EINVAL;
		goto request_err;
	}

	if (!fw_blob) {
		npu_err("null in fw_blob\n");
		ret = -EINVAL;
		goto request_err;
	}

	if (!fw_blob->data) {
		npu_err("null in fw_blob->data\n");
		ret = -EINVAL;
		goto request_err;
	}

	if (fw_blob->size > target_size) {
		npu_err("image size over(%ld > %ld)\n", binary->image_size, target_size);
		ret = -EIO;
		goto p_err;
	}

	binary->image_size = fw_blob->size;
#ifdef CONFIG_EXYNOS_VPU_HARDWARE
	memcpy(target, fw_blob->data, fw_blob->size);
#else
	memcpy(target, fw_blob->data, fw_blob->size);
#endif

	npu_info("success of binary(%s, %ld) apply.\n", npu_fw_path, fw_blob->size);
	ret = fw_blob->size;

request_err:
	release_firmware(fw_blob);
	return ret;
*/
}


int npu_binary_read(struct npu_binary *binary, void *target, size_t target_size)
{
	long ret = __npu_binary_read(binary, target, target_size, 0);

	if (ret > 0)
		return 0;

	/* Error case */
	return ret;
}

int npu_firmware_file_read(struct npu_binary *binary, void *target, size_t target_size, int mode)
{
	long ret = __npu_binary_read(binary, target, target_size, mode);

	if (ret > 0) {
		/* Print firmware signature on success */
		print_fw_signature(target, ret);
		return 0;
	}

	/* Error case */
	return ret;

}

int npu_firmware_file_read_signature(struct npu_binary *binary)
{
	int ret = 0;

	ret = imgloader_boot(&binary->imgloader);
	if (ret) {
		npu_err("fail(%d) in imgloader_boot\n", ret);
	}

	return ret;
}

int npu_binary_write(struct npu_binary *binary,
	void *target,
	size_t target_size)
{
	npu_err("This function is not support for gki, remove disk IO");
	return -ENOTSUPP;
/*
	int ret = 0;
	struct file *fp;
	mm_segment_t old_fs;
	//long nwrite;

	BUG_ON(!binary);

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	fp = // remove open
	if (IS_ERR_OR_NULL(fp)) {
		set_fs(old_fs);
		npu_err("fail(%pK) in pen\n", fp);
		ret = -EBADF;
		goto p_err;
	}

	// TODO : write file

	binary->image_size = target_size;

	npu_info("success of binary(%s, %ld) apply.\n", binary->fpath1, target_size);

	// remove close
	set_fs(old_fs);

p_err:
	return ret;
*/
}

static int npu_imgloader_memcpy(struct imgloader_desc *imgloader, const u8 *metadata, size_t size,
									phys_addr_t *fw_phys_base, size_t *fw_bin_size, size_t *fw_mem_size)
{
	int ret = 0;
	struct npu_binary *binary;
	struct npu_system *system;
	struct npu_memory_buffer *fwmem;

	binary = container_of(imgloader, struct npu_binary, imgloader);
	system = container_of(binary, struct npu_system, binary);

	fwmem = npu_get_mem_area(system, "fwmemory");
	memcpy(fwmem->vaddr, metadata, size);
	//npu_info("checking firmware head MAGIC(0x%08x)\n", *(u32 *)fwmem->vaddr);
	npu_info("checking firmware phys(%#llx), bin_size(%#zx), daddr(%#x++%#zx)\n",
			fwmem->paddr, size, fwmem->daddr, fwmem->size);

	*fw_phys_base = fwmem->paddr;
	*fw_bin_size = size;
	*fw_mem_size = fwmem->size;

	print_fw_signature(fwmem->vaddr, size);

	return ret;
}

static struct imgloader_ops npu_imgloader_ops = {
	.mem_setup = npu_imgloader_memcpy,
	.verify_fw = NULL,
	.blk_pwron = NULL,
	.init_image = NULL,
	.deinit_image = NULL,
	.shutdown = NULL,
};
