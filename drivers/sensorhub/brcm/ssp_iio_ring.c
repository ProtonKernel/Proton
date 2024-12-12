/*
 *  Copyright (C) 2011, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>

#include "ssp.h"
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/buffer_impl.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/sysfs.h>

struct iio_kfifo {
	struct iio_buffer buffer;
	struct kfifo kf;
	struct mutex user_lock;
	int update_needed;
};

#define iio_to_kfifo(r) container_of(r, struct iio_kfifo, buffer)

static inline int __iio_allocate_kfifo(struct iio_kfifo *buf,
			size_t bytes_per_datum, unsigned int length)
{
	if ((length == 0) || (bytes_per_datum == 0))
		return -EINVAL;

	/*
	 * Make sure we don't overflow an unsigned int after kfifo rounds up to
	 * the next power of 2.
	 */
	if (roundup_pow_of_two(length) > UINT_MAX / bytes_per_datum)
		return -EINVAL;

	return __kfifo_alloc((struct __kfifo *)&buf->kf, length,
			     bytes_per_datum, GFP_KERNEL);
}

static int iio_request_update_kfifo(struct iio_buffer *r)
{
	int ret = 0;
	struct iio_kfifo *buf = iio_to_kfifo(r);

	mutex_lock(&buf->user_lock);
	if (buf->update_needed) {
		kfifo_free(&buf->kf);
		ret = __iio_allocate_kfifo(buf, buf->buffer.bytes_per_datum,
				   buf->buffer.length);
		if (ret >= 0)
			buf->update_needed = false;
	} else {
		kfifo_reset_out(&buf->kf);
	}
	mutex_unlock(&buf->user_lock);

	return ret;
}

static int iio_mark_update_needed_kfifo(struct iio_buffer *r)
{
	struct iio_kfifo *kf = iio_to_kfifo(r);
	kf->update_needed = true;
	return 0;
}

static int iio_set_bytes_per_datum_kfifo(struct iio_buffer *r, size_t bpd)
{
	/*
	pr_info("[SSP-IIO]: %s, r->bytes_per_datum=%d bpd=%d",__func__, r->bytes_per_datum,bpd);

	if (r->bytes_per_datum != bpd) {
		r->bytes_per_datum = bpd;
		iio_mark_update_needed_kfifo(r);
	}
	*/
	return 0;
}

static int iio_set_length_kfifo(struct iio_buffer *r, unsigned int length)
{
	/* Avoid an invalid state */
	if (length < 2)
		length = 2;
	if (r->length != length) {
		r->length = length;
		iio_mark_update_needed_kfifo(r);
	}
	return 0;
}

static int iio_store_to_kfifo(struct iio_buffer *r,
			      const void *data)
{
	int ret;
	struct iio_kfifo *kf = iio_to_kfifo(r);
	ret = kfifo_in(&kf->kf, data, 1);
	if (ret != 1)
		return -EBUSY;
	return 0;
}

static int iio_read_first_n_kfifo(struct iio_buffer *r,
			   size_t n, char __user *buf)
{
	int ret, copied;
	struct iio_kfifo *kf = iio_to_kfifo(r);

	if (mutex_lock_interruptible(&kf->user_lock))
		return -ERESTARTSYS;

	if (!kfifo_initialized(&kf->kf) || n < kfifo_esize(&kf->kf))
		ret = -EINVAL;
	else
		ret = kfifo_to_user(&kf->kf, buf, n, &copied);
	
	//pr_err("[SSP_IIO] size=%d, kfifo_init=%d, kfifo_esize=%d", n, kfifo_initialized(&kf->kf), kfifo_esize(&kf->kf));
	
	mutex_unlock(&kf->user_lock);
	if (ret < 0)
		return ret;

	return copied;
}

static size_t iio_kfifo_buf_data_available(struct iio_buffer *r)
{
	struct iio_kfifo *kf = iio_to_kfifo(r);
	size_t samples;

	mutex_lock(&kf->user_lock);
	samples = kfifo_len(&kf->kf);
	mutex_unlock(&kf->user_lock);

	return samples;
}

static void iio_kfifo_buffer_release(struct iio_buffer *buffer)
{
	struct iio_kfifo *kf = iio_to_kfifo(buffer);

	mutex_destroy(&kf->user_lock);
	kfifo_free(&kf->kf);
	kfree(kf);
}

static const struct iio_buffer_access_funcs kfifo_access_funcs = {
	.store_to = &iio_store_to_kfifo,
	.read_first_n = &iio_read_first_n_kfifo,
	.data_available = iio_kfifo_buf_data_available,
	.request_update = &iio_request_update_kfifo,
	.set_bytes_per_datum = &iio_set_bytes_per_datum_kfifo,
	.set_length = &iio_set_length_kfifo,
	.release = &iio_kfifo_buffer_release,

	.modes = INDIO_BUFFER_SOFTWARE | INDIO_BUFFER_TRIGGERED,
};

struct iio_buffer *iio_kfifo_allocate(void)
{
	struct iio_kfifo *kf;

	kf = kzalloc(sizeof(*kf), GFP_KERNEL);
	if (!kf)
		return NULL;

	kf->update_needed = true;
	iio_buffer_init(&kf->buffer);
	kf->buffer.access = &kfifo_access_funcs;
	kf->buffer.length = 2;
	mutex_init(&kf->user_lock);

	return &kf->buffer;
}

void iio_kfifo_free(struct iio_buffer *r)
{
	iio_buffer_put(r);
}

void ssp_iio_unconfigure_ring(struct iio_dev *indio_dev)
{
	iio_kfifo_free(indio_dev->buffer);
}

static int ssp_predisable(struct iio_dev *indio_dev)
{
	return 0;
}

static int ssp_preenable(struct iio_dev *indio_dev)
{
	return 0;
}


static const struct iio_buffer_setup_ops ssp_iio_ring_setup_ops = {
	.preenable = &ssp_preenable,
	.predisable = &ssp_predisable,
};

int ssp_iio_configure_ring(struct iio_dev *indio_dev, int bytes)
{
	struct iio_buffer *ring;

	ring = iio_kfifo_allocate();
	if (!ring)
		return -ENOMEM;

	/* setup ring buffer */
	ring->bytes_per_datum = bytes;
	ring->scan_timestamp = true;
	iio_device_attach_buffer(indio_dev, ring);

	indio_dev->setup_ops = &ssp_iio_ring_setup_ops;
	/*
	 *scan count double count timestamp. should subtract 1. but
	 *number of channels still includes timestamp
	 */

	indio_dev->modes |= INDIO_BUFFER_SOFTWARE;

	return 0;
}

