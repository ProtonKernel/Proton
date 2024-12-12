#include "fips140.h"

__section(.rodata)
const __s8 builtime_crypto_hmac[FIPS_HMAC_SIZE] = {0};

__section(.rodata)
const struct first_last integrity_crypto_addrs[FIPS_CRYPTO_ADDRS_SIZE] = {{0},};

__section(.rodata)
const __u64 crypto_buildtime_address = 10;
