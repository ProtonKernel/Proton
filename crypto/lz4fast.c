// SPDX-License-Identifier: GPL-2.0-only
/*
 * Cryptographic API.
 *
 * Copyright (c) 2013 Chanho Min <chanho.min@lge.com>
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/crypto.h>
#include <linux/vmalloc.h>
#include <linux/lz4.h>
#include <crypto/internal/scompress.h>
struct lz4fast_ctx {
	void *lz4fast_comp_mem;
};
static void *lz4fast_alloc_ctx(struct crypto_scomp *tfm)
{
	void *ctx;
	ctx = vmalloc(LZ4_MEM_COMPRESS);
	if (!ctx)
		return ERR_PTR(-ENOMEM);
	return ctx;
}
static int lz4fast_init(struct crypto_tfm *tfm)
{
	struct lz4fast_ctx *ctx = crypto_tfm_ctx(tfm);
	ctx->lz4fast_comp_mem = lz4fast_alloc_ctx(NULL);
	if (IS_ERR(ctx->lz4fast_comp_mem))
		return -ENOMEM;
	return 0;
}
static void lz4fast_free_ctx(struct crypto_scomp *tfm, void *ctx)
{
	vfree(ctx);
}
static void lz4fast_exit(struct crypto_tfm *tfm)
{
	struct lz4fast_ctx *ctx = crypto_tfm_ctx(tfm);
	lz4fast_free_ctx(NULL, ctx->lz4fast_comp_mem);
}
static int __lz4fast_compress_crypto(const u8 *src, unsigned int slen,
				 u8 *dst, unsigned int *dlen, void *ctx)
{
	int out_len = LZ4_compress_fast(src, dst,
		slen, *dlen, LZ4FAST_DEFAULT_CLEVEL, ctx);
	if (!out_len)
		return -EINVAL;
	*dlen = out_len;
	return 0;
}
static int lz4fast_scompress(struct crypto_scomp *tfm, const u8 *src,
			 unsigned int slen, u8 *dst, unsigned int *dlen,
			 void *ctx)
{
	return __lz4fast_compress_crypto(src, slen, dst, dlen, ctx);
}
static int lz4fast_compress_crypto(struct crypto_tfm *tfm, const u8 *src,
			       unsigned int slen, u8 *dst, unsigned int *dlen)
{
	struct lz4fast_ctx *ctx = crypto_tfm_ctx(tfm);
	return __lz4fast_compress_crypto(src, slen, dst, dlen, ctx->lz4fast_comp_mem);
}
static int __lz4fast_decompress_crypto(const u8 *src, unsigned int slen,
				   u8 *dst, unsigned int *dlen, void *ctx)
{
	int out_len = LZ4_decompress_safe(src, dst, slen, *dlen);
	if (out_len < 0)
		return -EINVAL;
	*dlen = out_len;
	return 0;
}
static int lz4fast_sdecompress(struct crypto_scomp *tfm, const u8 *src,
			   unsigned int slen, u8 *dst, unsigned int *dlen,
			   void *ctx)
{
	return __lz4fast_decompress_crypto(src, slen, dst, dlen, NULL);
}
static int lz4fast_decompress_crypto(struct crypto_tfm *tfm, const u8 *src,
				 unsigned int slen, u8 *dst,
				 unsigned int *dlen)
{
	return __lz4fast_decompress_crypto(src, slen, dst, dlen, NULL);
}
static struct crypto_alg alg_lz4fast = {
	.cra_name		= "lz4fast",
	.cra_driver_name	= "lz4fast-generic",
	.cra_flags		= CRYPTO_ALG_TYPE_COMPRESS,
	.cra_ctxsize		= sizeof(struct lz4fast_ctx),
	.cra_module		= THIS_MODULE,
	.cra_init		= lz4fast_init,
	.cra_exit		= lz4fast_exit,
	.cra_u			= { .compress = {
	.coa_compress		= lz4fast_compress_crypto,
	.coa_decompress		= lz4fast_decompress_crypto } }
};
static struct scomp_alg scomp = {
	.alloc_ctx		= lz4fast_alloc_ctx,
	.free_ctx		= lz4fast_free_ctx,
	.compress		= lz4fast_scompress,
	.decompress		= lz4fast_sdecompress,
	.base			= {
		.cra_name	= "lz4fast",
		.cra_driver_name = "lz4fast-scomp",
		.cra_module	 = THIS_MODULE,
	}
};
static int __init lz4fast_mod_init(void)
{
	int ret;
	ret = crypto_register_alg(&alg_lz4fast);
	if (ret)
		return ret;
	ret = crypto_register_scomp(&scomp);
	if (ret) {
		crypto_unregister_alg(&alg_lz4fast);
		return ret;
	}
	return ret;
}
static void __exit lz4fast_mod_fini(void)
{
	crypto_unregister_alg(&alg_lz4fast);
	crypto_unregister_scomp(&scomp);
}
subsys_initcall(lz4fast_mod_init);
module_exit(lz4fast_mod_fini);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LZ4FAST Compression Algorithm");
MODULE_ALIAS_CRYPTO("lz4fast");