/*
 * Created in Samsung Ukraine R&D Center (SRK) under a contract between
 * LLC "Samsung Electronics Ukraine Company" (Kyiv, Ukraine)
 * and "Samsung Electronics Co", Ltd (Seoul, Republic of Korea)
 * Copyright: (c) Samsung Electronics Co, Ltd 2020. All rights reserved.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <crypto/fmp.h>
#include "hmac-sha256.h"
#include "fmp_fips_info.h"
#include "fips140_ic_support.h"

#define FIPS_HMAC_SIZE 32

#define FIPS_CHECK_INTEGRITY
#undef FIPS_DEBUG_INTEGRITY

/* Same as build time */
static const unsigned char *integrity_check_key =
	"The quick brown fox jumps over the lazy dog";

#ifdef FIPS_DEBUG_INTEGRITY
void show_emebedded_info(void)
{
	int i = 0;
	int j = 0;

	for (i = 0; i < fips140_sec_amount; i++) {
		pr_info("Section #%d\n", i);
		pr_info(" anchor symbols addr/offset %llx/%d\n",
			fips140_get_anchor_addr(i), fips140_anchor_offset[i]);
	}

	pr_info(" Chunks amount: %d\n", fips140_chunk_amount);
	for (j = 0; j < fips140_chunk_amount; j++) {
		pr_info("[ chunk #%d, sec: %d start: %d end: %d ]",
		j,
		fips140_chunks_info[j].sec_no,
		fips140_chunks_info[j].chunk_start,
		fips140_chunks_info[j].chunk_end);
	}
}
#endif

int do_fmp_integrity_check(struct exynos_fmp *fmp)
{
	int err = 0;
	uint8_t *start_addr = NULL;
	unsigned char runtime_hmac[FIPS_HMAC_SIZE];
	uint32_t sec_idx = 0;
	uint8_t *sec_zero_addr = NULL;
	uint32_t ch = 0;
	uint32_t size = 0;
	struct hmac_sha256_ctx ctx;
	const char *builtime_hmac = 0;
	struct exynos_fmp_fips_test_vops *test_vops;

#ifndef FIPS_CHECK_INTEGRITY
	return 0;
#endif

#ifdef FIPS_DEBUG_INTEGRITY
	show_emebedded_info();
#endif

	if (!fmp || !fmp->dev) {
		pr_err("%s: invalid fmp device\n", __func__);
		return -EINVAL;
	}

	memset(runtime_hmac, 0x00, 32);

	err = hmac_sha256_init(&ctx,
				integrity_check_key,
				strlen(integrity_check_key));
	if (err) {
		pr_err("FIPS(%s): init_hash failed\n", __func__);
		return -1;
	}

	for (sec_idx = 0; sec_idx < fips140_sec_amount; sec_idx++) {
		sec_zero_addr = (uint8_t *)(fips140_get_anchor_addr(sec_idx)
				- fips140_anchor_offset[sec_idx]);

		for (ch = 0; ch < fips140_chunk_amount; ch++) {
			if (fips140_chunks_info[ch].sec_no != sec_idx)
				continue;

			start_addr = fips140_chunks_info[ch].chunk_start
						+ sec_zero_addr;
			size = fips140_chunks_info[ch].chunk_end
				- fips140_chunks_info[ch].chunk_start
				+ 1;
#ifdef FIPS_DEBUG_INTEGRITY
			print_hex_dump(KERN_ERR, " FIPS_chunks >>> : ",
			DUMP_PREFIX_ADDRESS, 16, 1, start_addr, size, false);
#endif
			err = hmac_sha256_update(&ctx, start_addr, size);
			if (err) {
				pr_err("FIPS(%s): Error to update hash",
					__func__);
				return -1;
			}
		}
	}

	test_vops = (struct exynos_fmp_fips_test_vops *)fmp->test_vops;

	if (test_vops) {
		// Weird code here and in fmp_func_test_integrity
		// in fmp_fips_func_test.c. To be revised
		unsigned long ic_fail_data = 0x5a5a5a5a;

		err = test_vops->integrity(&ctx, &ic_fail_data);
		if (err) {
			pr_err("FIPS(%s): Error to update hash for func test\n",
				__func__);
			hmac_sha256_ctx_cleanup(&ctx);
			return -1;
		}
	}

	err = hmac_sha256_final(&ctx, runtime_hmac);
	if (err) {
		pr_err("FIPS(%s): Error in finalize", __func__);
		hmac_sha256_ctx_cleanup(&ctx);
		return -1;
	}

	hmac_sha256_ctx_cleanup(&ctx);
	builtime_hmac = fips140_builtime_hmac;

#ifdef FIPS_DEBUG_INTEGRITY
	print_hex_dump(KERN_INFO, "FIPS FMP RUNTIME : runtime hmac  = ",
	DUMP_PREFIX_NONE, 16, 1, runtime_hmac, sizeof(runtime_hmac), false);
	print_hex_dump(KERN_INFO, "FIPS FMP RUNTIME : builtime_hmac = ",
	DUMP_PREFIX_NONE, 16, 1, builtime_hmac, sizeof(runtime_hmac), false);
#endif

	if (!memcmp(builtime_hmac, runtime_hmac, sizeof(runtime_hmac))) {
		pr_info("FIPS(%s): Integrity Check Passed", __func__);
		return 0;
	}

	pr_err("FIPS(%s): Integrity Check Failed", __func__);
	return -1;
}
