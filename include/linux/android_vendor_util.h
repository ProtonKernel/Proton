/* SPDX-License-Identifier: GPL-2.0 */
/*
 * android_vendor_util.h - utility for Android vendor data
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 */
#ifndef _ANDROID_VENDOR_UTIL_H
#define _ANDROID_VENDOR_UTIL_H

#include <linux/compiler.h>
#include <linux/android_vendor.h>

/*
 * Worker macros, don't use these, use the ones without a leading '_'
 */

#define __ANDROID_VENDOR_CHECK_SIZE_ALIGN(_orig, _new)				\
	union {									\
		_Static_assert(sizeof(struct{_new;}) <= sizeof(struct{_orig;}),	\
			       __FILE__ ":" __stringify(__LINE__) ": "		\
			       __stringify(_new)				\
			       " is larger than "				\
			       __stringify(_orig) );				\
		_Static_assert(__alignof__(struct{_new;}) <= __alignof__(struct{_orig;}),	\
			       __FILE__ ":" __stringify(__LINE__) ": "		\
			       __stringify(_orig)				\
			       " is not aligned the same as "			\
			       __stringify(_new) );				\
	}

#ifdef __GENKSYMS__

#define _ANDROID_VENDOR_REPLACE(_orig, _new)		_orig

#else

#define _ANDROID_VENDOR_REPLACE(_orig, _new)			\
	union {							\
		_new;						\
		struct {					\
			_orig;					\
		} __UNIQUE_ID(android_vendor_hide);		\
		__ANDROID_VENDOR_CHECK_SIZE_ALIGN(_orig, _new);	\
	}

#endif /* __GENKSYMS__ */

/*
 * ANDROID_VENDOR_USE(number, _new)
 *   Use a previous padding entry that was defined with ANDROID_VENDOR_RESERVE
 *   number: the previous "number" of the padding variable
 *   _new: the variable to use now instead of the padding variable
 */
#define ANDROID_VENDOR_USE(number, _new)		\
	_ANDROID_VENDOR_REPLACE(ANDROID_VENDOR_DATA(number), _new)

/*
 * ANDROID_VENDOR_USE2(number, _new1, _new2)
 *   Use a previous padding entry that was defined with ANDROID_VENDOR_RESERVE for
 *   two new variables that fit into 64 bits.  This is good for when you do not
 *   want to "burn" a 64bit padding variable for a smaller variable size if not
 *   needed.
 */
#define ANDROID_VENDOR_USE2(number, _new1, _new2)			\
	_ANDROID_VENDOR_REPLACE(ANDROID_VENDOR_DATA(number), struct{ _new1; _new2; })

#endif /* _ANDROID_VENDOR_UTIL_H */
