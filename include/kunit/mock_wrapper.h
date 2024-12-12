/* SPDX-License-Identifier: GPL-2.0 */
/*
 * add macros and functions to support 5.10 Kunit
 *
 */

#define KUNIT_EXPECT_CALL(expectation_call) EXPECT_CALL(expectation_call)

#define kunit_any(test) any((struct test *)test)

/*
 * Matches different types of integers, the argument is compared to the
 * `expected` field, based on the comparison defined.
 */
#define kunit_u8_eq(test, ...) u8_eq((struct test *)test, ##__VA_ARGS__)
#define kunit_u8_ne(test, ...) u8_ne((struct test *)test, ##__VA_ARGS__)
#define kunit_u8_le(test, ...) u8_le((struct test *)test, ##__VA_ARGS__)
#define kunit_u8_lt(test, ...) u8_lt((struct test *)test, ##__VA_ARGS__)
#define kunit_u8_ge(test, ...) u8_ge((struct test *)test, ##__VA_ARGS__)
#define kunit_u8_gt(test, ...) u8_gt((struct test *)test, ##__VA_ARGS__)

#define kunit_u16_eq(test, ...) u16_eq((struct test *)test, ##__VA_ARGS__)
#define kunit_u16_ne(test, ...) u16_ne((struct test *)test, ##__VA_ARGS__)
#define kunit_u16_le(test, ...) u16_le((struct test *)test, ##__VA_ARGS__)
#define kunit_u16_lt(test, ...) u16_lt((struct test *)test, ##__VA_ARGS__)
#define kunit_u16_ge(test, ...) u16_ge((struct test *)test, ##__VA_ARGS__)
#define kunit_u16_gt(test, ...) u16_gt((struct test *)test, ##__VA_ARGS__)

#define kunit_u32_eq(test, ...) u32_eq((struct test *)test, ##__VA_ARGS__)
#define kunit_u32_ne(test, ...) u32_ne((struct test *)test, ##__VA_ARGS__)
#define kunit_u32_le(test, ...) u32_le((struct test *)test, ##__VA_ARGS__)
#define kunit_u32_lt(test, ...) u32_lt((struct test *)test, ##__VA_ARGS__)
#define kunit_u32_ge(test, ...) u32_ge((struct test *)test, ##__VA_ARGS__)
#define kunit_u32_gt(test, ...) u32_gt((struct test *)test, ##__VA_ARGS__)

#define kunit_u64_eq(test, ...) u64_eq((struct test *)test, ##__VA_ARGS__)
#define kunit_u64_ne(test, ...) u64_ne((struct test *)test, ##__VA_ARGS__)
#define kunit_u64_le(test, ...) u64_le((struct test *)test, ##__VA_ARGS__)
#define kunit_u64_lt(test, ...) u64_lt((struct test *)test, ##__VA_ARGS__)
#define kunit_u64_ge(test, ...) u64_ge((struct test *)test, ##__VA_ARGS__)
#define kunit_u64_gt(test, ...) u64_gt((struct test *)test, ##__VA_ARGS__)

#define kunit_char_eq(test, ...) char_eq((struct test *)test, ##__VA_ARGS__)
#define kunit_char_ne(test, ...) char_ne((struct test *)test, ##__VA_ARGS__)
#define kunit_char_le(test, ...) char_le((struct test *)test, ##__VA_ARGS__)
#define kunit_char_lt(test, ...) char_lt((struct test *)test, ##__VA_ARGS__)
#define kunit_char_ge(test, ...) char_ge((struct test *)test, ##__VA_ARGS__)
#define kunit_char_gt(test, ...) char_gt((struct test *)test, ##__VA_ARGS__)

#define kunit_uchar_eq(test, ...) uchar_eq((struct test *)test, ##__VA_ARGS__)
#define kunit_uchar_ne(test, ...) uchar_ne((struct test *)test, ##__VA_ARGS__)
#define kunit_uchar_le(test, ...) uchar_le((struct test *)test, ##__VA_ARGS__)
#define kunit_uchar_lt(test, ...) uchar_lt((struct test *)test, ##__VA_ARGS__)
#define kunit_uchar_ge(test, ...) uchar_ge((struct test *)test, ##__VA_ARGS__)
#define kunit_uchar_gt(test, ...) uchar_gt((struct test *)test, ##__VA_ARGS__)

#define kunit_schar_eq(test, ...) schar_eq((struct test *)test, ##__VA_ARGS__)
#define kunit_schar_ne(test, ...) schar_ne((struct test *)test, ##__VA_ARGS__)
#define kunit_schar_le(test, ...) schar_le((struct test *)test, ##__VA_ARGS__)
#define kunit_schar_lt(test, ...) schar_lt((struct test *)test, ##__VA_ARGS__)
#define kunit_schar_ge(test, ...) schar_ge((struct test *)test, ##__VA_ARGS__)
#define kunit_schar_gt(test, ...) schar_gt((struct test *)test, ##__VA_ARGS__)

#define kunit_short_eq(test, ...) short_eq((struct test *)test, ##__VA_ARGS__)
#define kunit_short_ne(test, ...) short_ne((struct test *)test, ##__VA_ARGS__)
#define kunit_short_le(test, ...) short_le((struct test *)test, ##__VA_ARGS__)
#define kunit_short_lt(test, ...) short_lt((struct test *)test, ##__VA_ARGS__)
#define kunit_short_ge(test, ...) short_ge((struct test *)test, ##__VA_ARGS__)
#define kunit_short_gt(test, ...) short_gt((struct test *)test, ##__VA_ARGS__)

#define kunit_ushort_eq(test, ...) ushort_eq((struct test *)test, ##__VA_ARGS__)
#define kunit_ushort_ne(test, ...) ushort_ne((struct test *)test, ##__VA_ARGS__)
#define kunit_ushort_le(test, ...) ushort_le((struct test *)test, ##__VA_ARGS__)
#define kunit_ushort_lt(test, ...) ushort_lt((struct test *)test, ##__VA_ARGS__)
#define kunit_ushort_ge(test, ...) ushort_ge((struct test *)test, ##__VA_ARGS__)
#define kunit_ushort_gt(test, ...) ushort_gt((struct test *)test, ##__VA_ARGS__)

#define kunit_int_eq(test, ...) int_eq((struct test *)test, ##__VA_ARGS__)
#define kunit_int_ne(test, ...) int_ne((struct test *)test, ##__VA_ARGS__)
#define kunit_int_lt(test, ...) int_lt((struct test *)test, ##__VA_ARGS__)
#define kunit_int_le(test, ...) int_le((struct test *)test, ##__VA_ARGS__)
#define kunit_int_gt(test, ...) int_gt((struct test *)test, ##__VA_ARGS__)
#define kunit_int_ge(test, ...) int_ge((struct test *)test, ##__VA_ARGS__)

#define kunit_uint_eq(test, ...) uint_eq((struct test *)test, ##__VA_ARGS__)
#define kunit_uint_ne(test, ...) uint_ne((struct test *)test, ##__VA_ARGS__)
#define kunit_uint_lt(test, ...) uint_lt((struct test *)test, ##__VA_ARGS__)
#define kunit_uint_le(test, ...) uint_le((struct test *)test, ##__VA_ARGS__)
#define kunit_uint_gt(test, ...) uint_gt((struct test *)test, ##__VA_ARGS__)
#define kunit_uint_ge(test, ...) uint_ge((struct test *)test, ##__VA_ARGS__)

#define kunit_long_eq(test, ...) long_eq((struct test *)test, ##__VA_ARGS__)
#define kunit_long_ne(test, ...) long_ne((struct test *)test, ##__VA_ARGS__)
#define kunit_long_le(test, ...) long_le((struct test *)test, ##__VA_ARGS__)
#define kunit_long_lt(test, ...) long_lt((struct test *)test, ##__VA_ARGS__)
#define kunit_long_ge(test, ...) long_ge((struct test *)test, ##__VA_ARGS__)
#define kunit_long_gt(test, ...) long_gt((struct test *)test, ##__VA_ARGS__)

#define kunit_ulong_eq(test, ...) ulong_eq((struct test *)test, ##__VA_ARGS__)
#define kunit_ulong_ne(test, ...) ulong_ne((struct test *)test, ##__VA_ARGS__)
#define kunit_ulong_le(test, ...) ulong_le((struct test *)test, ##__VA_ARGS__)
#define kunit_ulong_lt(test, ...) ulong_lt((struct test *)test, ##__VA_ARGS__)
#define kunit_ulong_ge(test, ...) ulong_ge((struct test *)test, ##__VA_ARGS__)
#define kunit_ulong_gt(test, ...) ulong_gt((struct test *)test, ##__VA_ARGS__)

#define kunit_longlong_eq(test, ...) longlong_eq((struct test *)test, ##__VA_ARGS__)
#define kunit_longlong_ne(test, ...) longlong_ne((struct test *)test, ##__VA_ARGS__)
#define kunit_longlong_le(test, ...) longlong_le((struct test *)test, ##__VA_ARGS__)
#define kunit_longlong_lt(test, ...) longlong_lt((struct test *)test, ##__VA_ARGS__)
#define kunit_longlong_ge(test, ...) longlong_ge((struct test *)test, ##__VA_ARGS__)
#define kunit_longlong_gt(test, ...) longlong_gt((struct test *)test, ##__VA_ARGS__)

#define kunit_ulonglong_eq(test, ...) ulonglong_eq((struct test *)test, ##__VA_ARGS__)
#define kunit_ulonglong_ne(test, ...) ulonglong_ne((struct test *)test, ##__VA_ARGS__)
#define kunit_ulonglong_le(test, ...) ulonglong_le((struct test *)test, ##__VA_ARGS__)
#define kunit_ulonglong_lt(test, ...) ulonglong_lt((struct test *)test, ##__VA_ARGS__)
#define kunit_ulonglong_ge(test, ...) ulonglong_ge((struct test *)test, ##__VA_ARGS__)
#define kunit_ulonglong_gt(test, ...) ulonglong_gt((struct test *)test, ##__VA_ARGS__)

/* Matches pointers. */
#define kunit_ptr_eq(test, ...) ptr_eq((struct test *)test, ##__VA_ARGS__)
#define kunit_ptr_ne(test, ...) ptr_eq((struct test *)test, ##__VA_ARGS__)
#define kunit_ptr_lt(test, ...) ptr_lt((struct test *)test, ##__VA_ARGS__)
#define kunit_ptr_le(test, ...) ptr_le((struct test *)test, ##__VA_ARGS__)
#define kunit_ptr_gt(test, ...) ptr_gt((struct test *)test, ##__VA_ARGS__)
#define kunit_ptr_ge(test, ...) ptr_ge((struct test *)test, ##__VA_ARGS__)

/* Matches memory sections and strings. */
#define kunit_memeq(test, ...) memeq((struct test *)test, ##__VA_ARGS__)
#define kunit_streq(test, ...) streq((struct test *)test, ##__VA_ARGS__)

#define kunit_str_contains(test, ...) str_contains((struct test *)test, ##__VA_ARGS__)

/* Matches var-arg arguments. */
#define kunit_va_format_cmp(test, ...) va_format_cmp((struct test *)test, ##__VA_ARGS__)

#define kunit_bool_return(test, ...) bool_return((struct test *)test, ##__VA_ARGS__)
#define kunit_u8_return(test, ...) u8_return((struct test *)test, ##__VA_ARGS__)
#define kunit_u16_return(test, ...) u16_return((struct test *)test, ##__VA_ARGS__)
#define kunit_u32_return(test, ...) u32_return((struct test *)test, ##__VA_ARGS__)
#define kunit_u64_return(test, ...) u64_return((struct test *)test, ##__VA_ARGS__)
#define kunit_char_return(test, ...) char_return((struct test *)test, ##__VA_ARGS__)
#define kunit_uchar_return(test, ...) uchar_return((struct test *)test, ##__VA_ARGS__)
#define kunit_schar_return(test, ...) schar_return((struct test *)test, ##__VA_ARGS__)
#define kunit_short_return(test, ...) short_return((struct test *)test, ##__VA_ARGS__)
#define kunit_ushort_return(test, ...) ushort_return((struct test *)test, ##__VA_ARGS__)
#define kunit_int_return(test, ...) int_return((struct test *)test, ##__VA_ARGS__)
#define kunit_uint_return(test, ...) uint_return((struct test *)test, ##__VA_ARGS__)
#define kunit_long_return(test, ...) long_return((struct test *)test, ##__VA_ARGS__)
#define kunit_ulong_return(test, ...) ulong_return((struct test *)test, ##__VA_ARGS__)
#define kunit_longlong_return(test, ...) longlong_return((struct test *)test, ##__VA_ARGS__)
#define kunit_ulonglong_return(test, ...) ulonglong_return((struct test *)test, ##__VA_ARGS__)
#define kunit_ptr_return(test, ...) ptr_return((struct test *)test, ##__VA_ARGS__)
