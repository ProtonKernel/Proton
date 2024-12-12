/*
 * ncp.h
 *
 *  Created on: 2017. 7. 18.
 *      Author: kilyeon.im
 */

#ifndef NCP_H_
#define NCP_H_
/* Already defined in include/base/type.h */
#if 0
typedef	signed char		s8;
typedef	signed short		s16;
typedef	signed int		s32;
typedef	unsigned char		u8;
typedef	unsigned short		u16;
typedef	unsigned int		u32;
typedef unsigned long		ulong;
typedef unsigned long long	u64;
#endif
#define NCP_MAGIC1		0x0C0FFEE0
#define NCP_MAGIC2		0xC0DEC0DE

#define NCP_VERSION		24

/**
 * @brief an element of address vector table
 */
struct address_vector {
	/**
	 * @brief index in address vector table
	 * @details producer : compiler
	 * @n consumer : driver, firmware
	 * @n description : this index is required for partial update of address vector(0 ~ n)
	 */
	u32	index;
	/**
	 * @brief master address (KITT side)
	 * @details producer : compiler, driver
	 * @n consumer : driver, firmware
	 * @n description : device virtual address or offset.
	 * this address can point feature map, weight, golden etc.
	 * offset is provided by compiler in case of weight, feature map and golden.
	 * driver should replace this offset to device virtual address.
	 * offset value means the offset from start of ncp header.
	 */
	u32 	m_addr;
	/**
	 * @brief slave address (TURING side)
	 * @details producer : driver
	 * @n consumer : firmware
	 * @n description :
	 */
	u32 	s_addr;
	/**
	 * @brief size in byte
	 * @details producer : compiler
	 * @n consumer : driver
	 * @n description : io feature map and intermediate feature map should have not zero size
	 */
	u32 	size;
};

enum ncp_memory_type {
	MEMORY_TYPE_IN_FMAP, /* input feature map */
	MEMORY_TYPE_OT_FMAP, /* output feature map */
	MEMORY_TYPE_IM_FMAP, /* intermediate feature map */
	MEMORY_TYPE_OT_PIX0,
	MEMORY_TYPE_OT_PIX1,
	MEMORY_TYPE_OT_PIX2,
	MEMORY_TYPE_OT_PIX3,
	MEMORY_TYPE_WEIGHT,
	MEMORY_TYPE_WMASK,
	MEMORY_TYPE_LUT,
	MEMORY_TYPE_NCP,
	MEMORY_TYPE_GOLDEN,
	MEMORY_TYPE_CUCODE,
	MEMORY_TYPE_MAX
};

/**
 * @brief an element of memory vector table
 */
struct memory_vector {
	/**
	 * @brief memory type
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : feature map, weight, weight mask, lut, ncp body. bias is included to ncp body.
	 */
	u32 	type;
	/**
	 * @brief pixsel format
	 * @details producer : compiler
	 * @n consumer : driver
	 * @n description : required pixel format (FOUR CC)
	 */
	u32 	pixel_format;
	/**
	 * @brief pixel width
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description :
	 */
	u32 	width;
	/**
	 * @brief pixel height
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description :
	 */
	u32 	height;
	/**
	 * @brief channel count
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description :
	 */
	u32 	channels;
	/**
	 * @brief width stride
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : this stride value can be input or output stride
	 */
	u32 	wstride;
	/**
	 * @brief channel stride
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : this stride value can be input or output stride
	 */
	u32 	cstride;
	/**
	 * @brief address vector index
	 * @details producer : framework
	 * @n consumer : firmware
	 * @n description : index of array of struct address_vector
	 */
	u32 	address_vector_index;
};

enum group_type {
	GROUP_TYPE_THREAD,
	GROUP_TYPE_SLOT,
	GROUP_TYPE_DUMMY,
	GROUP_TYPE_END
};

enum group_status {
	GROUP_STATUS_STANDBY, /* not started yet */
	GROUP_STATUS_START, /* right before start */
	GROUP_STATUS_STOP, /* stopped on work */
	GROUP_STATUE_DONE, /* process done */
	GROUP_STATUS_END
};

enum group_flags {
	GROUP_FLAG_SKIP, /* this group should be skipped to process */
	GROUP_FLAG_NOTIFY, /* notify flag look forward to notifying a completion of group process by status update */
	GROUP_FLAG_COMPARE, /* this flag look forward to comparing between output and golden */
	GROUP_FLAG_LAST, /* last flag means last group in chunk it's contained */
	GROUP_FLAG_WFS, /* group have to wait for standby status(WFS) */
	GROUP_FLAG_END
};

/**
 * @brief an element of group vector table
 */
struct group_vector {
	/**
	 * @brief group index
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : this index should be monotonously increased by one
	 * range : 0 <= index < ncp_header->group_vector_cnt
	 */
	u32 	index;
	/**
	 * @brief group id
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : this id should be monotonously increased by one
	 * range : 0 <= id < maximum group id in a thread
	 */
	u32 	id;
	/**
	 * @brief group type
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : thread, slot
	 */
	u32 	type;
	/**
	 * @brief the size of group
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description :
	 */
	u32 	size;
	/**
	 * @brief the status of group
	 * @details producer : firmware
	 * @n consumer : driver
	 * @n description : this shows progress of group
	 */
	u32 	status;
	/**
	 * @brief flags
	 * @details producer : all
	 * @n consumer : firmware
	 * @n description : bit flag can be orred, each bit corresponds to each flag.
	 */
	u32 	flags;
	/**
	 * @brief process count
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : process count is used for repeatation of isa only
	 * if this value is zero then just be done one time.
	 */
	u32 	batch;
	/**
	 * @brief the offset of intrinsic
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : instrinsic address =  body + intrinsic offset
	 */
	u32 	intrinsic_offset;
	/**
	 * @brief the size of intrinsic set
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description :
	 */
	u32 	intrinsic_size;
	/**
	 * @brief the offset of ISA
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : isa address =  body + isa offset
	 */
	u32 	isa_offset;
	/**
	 * @brief the size of command set
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description :
	 */
	u32 	isa_size;
};

/**
 * @brief an element of thread vector table
 */
struct thread_vector {
	/**
	 * @brief vector index
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : this index should be monotonously increased by one
	 * range : 0 <= index < ncp_header->thread_vector_cnt
	 */
	u32 	index;
	/**
	 * @brief group start index
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : this points the start index of group vector
	 * range : 0 <= index < ncp_header-group_vector_cnt
	 */
	u32	group_str_idx;
	/**
	 * @brief group end index
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : this points the end index of group vector
	 * range : 0 <= index < ncp_header->group_vector_cnt
	 */
	u32	group_end_idx;
	/**
	 * @brief workload
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : this value is a propotion(0 ~ 100) that a thread occupies on the total workload
	 */
	u32	workload;
};

/**
 * @brief ncp header to describe image structure
 */
struct ncp_header {
	/**
	 * @brief magic number at start
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : for detecting memory invasion
	 */
	u32 	magic_number1;
	/**
	 * @brief ncp header version
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : the format of version number is decimal
	 */
	u32 	hdr_version;
	/**
	 * @brief ncp header size
	 * @details producer : compiler & driver
	 * @n consumer : all
	 * @n description : total ncp header size in byte, this size includes address, memory, group, chunk vector
	 */
	u32 	hdr_size;
	/**
	 * @brief intrinsic version
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : firmware should check intrinsic version before doing ncp
	 */
	u32 	intrinsic_version;
	/**
	 * @brief network id
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : network id is a fixed number before
	 */
	u32 	net_id;
	/**
	 * @brief unique id
	 * @details producer : driver
	 * @n consumer : all
	 * @n description : a unique number is made by driver and reported to framework
	 */
	u32 	unique_id;
	/**
	 * @brief priority
	 * @details producer : framework
	 * @n consumer : firmware
	 * @n description : the process order
	 */
	u32 	priority;
	/**
	 * @brief flags
	 * @details producer : framework
	 * @n consumer : all
	 * @n description : debug flag, and son on
	 */
	u32 	flags;
	/**
	 * @brief period time
	 * @details producer : framework
	 * @n consumer : driver, firmware
	 * @n description : duty time for done in micro second
	 */
	u32 	period;
	/**
	 * @brief workload
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : computational complexity of operation in cycle
	 */
	u32 	workload;
	/**
	 * @brief total flc transfer size
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : total flc transfer size in byte
	 */
	u32 	total_flc_transfer_size;
	/**
	 * @brief total sdma transfer size
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : total sdma transfer size in byte
	 */
	u32 	total_sdma_transfer_size;
	/**
	 * @brief address vector offset
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : address vector offset in byte
	 */
	u32 	address_vector_offset;
	/**
	 * @brief the number of address vector
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : n
	 */
	u32 	address_vector_cnt;
	/**
	 * @brief memory vector offset
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : memory vector offset in byte
	 */
	u32 	memory_vector_offset;
	/**
	 * @brief the number of memory vector
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : n
	 */
	u32 	memory_vector_cnt;
	/**
	 * @brief group vector offset
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : group vector offset in byte
	 */
	u32 	group_vector_offset;
	/**
	 * @brief the number of group vector
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : n
	 */
	u32 	group_vector_cnt;
	/**
	 * @brief thread vector offset
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : thread vector offset in byte
	 */
	u32 	thread_vector_offset;
	/**
	 * @brief the number of thread vector
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : n
	 */
	u32 	thread_vector_cnt;
	/**
	 * @brief ncp body version
	 * @details producer : compiler
	 * @n consumer : driver, firmware
	 * @n description :
	 */
	u32 	body_version;
	/**
	 * @brief ncp body offset
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : ncp body offset in byte
	 */
	u32 	body_offset;
	/**
	 * @brief ncp body size
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : ncp body size in byte
	 */
	u32 	body_size;
	/**
	 * @brief io vector offset
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : io vector offset in byte
	 */
	u32 	io_vector_offset;
	/**
	 * @brief the number of io vector
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description :
	 */
	u32 	io_vector_cnt;
	/**
	 * @brief rq vector offset
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : rq vector offset should be aligned 64 bytes
	 */
	u32 	rq_vector_offset;
	/**
	 * @brief rq vector size
	 * @details producer : compiler
	 * @n consumer : firmware
	 * @n description : this size can be calculated by the number of io vector * 360 bytes
	 */
	u32 	rq_vector_size;
	/**
	 * @brief reserved field
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : compatibility for the future
	 */
	u32 	reserved[8];
	/**
	 * @brief magic number
	 * @details producer : compiler
	 * @n consumer : all
	 * @n description : correction for header version
	 */
	u32 	magic_number2;
};

#endif /* NCP_H_ */
