/*
 * Created in Samsung Ukraine R&D Center (SRK) under a contract between
 * LLC "Samsung Electronics Ukraine Company" (Kyiv, Ukraine)
 * and "Samsung Electronics Co", Ltd (Seoul, Republic of Korea)
 * Copyright: (c) Samsung Electronics Co, Ltd 2020. All rights reserved.
 */
#ifndef _FMP_FIPS140_IC_SUPPORT_H_
#define _FMP_FIPS140_IC_SUPPORT_H_

struct chunks_info {
	uint32_t sec_no;
	uint32_t chunk_start;
	uint32_t chunk_end;
};

extern struct chunks_info fips140_chunks_info[];// to be embedded
extern uint32_t fips140_anchor_offset[];		// to be embedded
extern uint32_t fips140_sec_amount;			// to be embedded
extern uint32_t fips140_chunk_amount;		// to be embedded
extern uint8_t fips140_builtime_hmac[];		// to be embedded

void *fips140_get_anchor_addr(uint32_t i);

#endif // _FMP_FIPS140_IC_SUPPORT_H_
