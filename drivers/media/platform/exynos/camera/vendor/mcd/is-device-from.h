/*
 * Samsung Exynos5 SoC series FIMC-IS-FROM driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_FROM_H
#define IS_FROM_H

int is_spi_write(struct is_spi *spi, u32 addr, u8 *data, size_t size);
int is_spi_write_enable(struct is_spi *spi);
int is_spi_write_disable(struct is_spi *spi);
int is_spi_erase_sector(struct is_spi *spi, u32 addr);
int is_spi_erase_block(struct is_spi *spi, u32 addr);
int is_spi_read_status_bit(struct is_spi *spi, u8 *buf);
int is_spi_read_module_id(struct is_spi *spi, void *buf, u16 addr, size_t size);
#endif
