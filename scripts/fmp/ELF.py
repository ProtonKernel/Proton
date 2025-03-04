#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import struct
import re
from math import ceil
from Utils import Utils
from Utils import log_e, log_d

# Symbol types
STT_NOTYPE = 0
STT_OBJECT = 1
STT_FUNC = 2
STT_SECTION = 3
STT_FILE = 4
STT_COMMON = 5
STT_LOOS = 10
STT_HIOS = 12
STT_LOPROC = 13
STT_SPARC_REGISTER = 13
STT_HIPROC = 15

# Symbol bind
STB_LOCAL = 0
STB_GLOBAL = 1
STB_WEAK = 2
STB_LOOS = 10
STB_HIOS = 12
STB_LOPROC = 13
STB_HIPROC = 15

# Symbol visibility
STV_DEFAULT = 0
STV_INTERNAL = 1
STV_HIDDEN = 2
STV_PROTECTED = 3
STV_EXPORTED = 4
STV_SINGLETON = 5
STV_ELIMINATE = 6

# defines from /arch/arm64/include/asm/elf.h
R_ARM_NONE = 0
R_AARCH64_NONE = 256

# Data.
R_AARCH64_ABS64 = 257
R_AARCH64_ABS32 = 258
R_AARCH64_ABS16 = 259
R_AARCH64_PREL64 = 260
R_AARCH64_PREL32 = 261
R_AARCH64_PREL16 = 262

# Instructions.
R_AARCH64_MOVW_UABS_G0 = 263
R_AARCH64_MOVW_UABS_G0_NC = 264
R_AARCH64_MOVW_UABS_G1 = 265
R_AARCH64_MOVW_UABS_G1_NC = 266
R_AARCH64_MOVW_UABS_G2 = 267
R_AARCH64_MOVW_UABS_G2_NC = 268
R_AARCH64_MOVW_UABS_G3 = 269
R_AARCH64_MOVW_SABS_G0 = 270
R_AARCH64_MOVW_SABS_G1 = 271
R_AARCH64_MOVW_SABS_G2 = 272
R_AARCH64_LD_PREL_LO19 = 273
R_AARCH64_ADR_PREL_LO21 = 274
R_AARCH64_ADR_PREL_PG_HI21 = 275
R_AARCH64_ADR_PREL_PG_HI21_NC = 276
R_AARCH64_ADD_ABS_LO12_NC = 277
R_AARCH64_LDST8_ABS_LO12_NC	= 278
R_AARCH64_TSTBR14 = 279
R_AARCH64_CONDBR19 = 280
R_AARCH64_JUMP26 = 282
R_AARCH64_CALL26 = 283
R_AARCH64_LDST16_ABS_LO12_NC = 284
R_AARCH64_LDST32_ABS_LO12_NC = 285
R_AARCH64_LDST64_ABS_LO12_NC = 286
R_AARCH64_LDST128_ABS_LO12_NC = 299
R_AARCH64_MOVW_PREL_G0 = 287
R_AARCH64_MOVW_PREL_G0_NC = 288
R_AARCH64_MOVW_PREL_G1 = 289
R_AARCH64_MOVW_PREL_G1_NC = 290
R_AARCH64_MOVW_PREL_G2 = 291
R_AARCH64_MOVW_PREL_G2_NC = 292
R_AARCH64_MOVW_PREL_G3 = 293
R_AARCH64_RELATIVE = 1027

SHT_NULL = 0
SHT_PROGBITS = 1
SHT_SYMTAB = 2
SHT_STRTAB = 3
SHT_RELA = 4
SHT_HASH = 5
SHT_DYNAMIC = 6
SHT_NOTE = 7
SHT_NOBITS = 8
SHT_REL = 9
SHT_SHLIB = 10
SHT_DYNSYM = 11
SHT_NUM = 12

ARCH_SHF_SMALL = 0
SHF_WRITE =	0x1
SHF_ALLOC =	0x2
SHF_EXECINSTR =	0x4
SHF_RELA_LIVEPATCH = 0x00100000
SHF_RO_AFTER_INIT = 0x00200000
SHF_MASKPROC = 0xf0000000

DEFAULT_NAME_SECTION_SYMTAB = ".symtab"
DEFAULT_NAME_SECTION_SHSTRTAB = ".shstrtab"
DEFAULT_NAME_SECTION_STRTAB = ".strtab"
DEFAULT_NAME_SECTION_ALTINSTR = ".altinstructions"
DEFAULT_RELA_PREFIX = ".rela"
DEFAULT_CFI_ENDING = ".__cfi_check"
DEFAULT_NAME_SECTION_JUMPTBL = "__jump_table"
PREFIX_SECTION_DEBUG = ".debug"

CFI_MARKER_NAME_FUNCTION = "__cfi_check"

DEFAULT_ARM_INSTRUCTION_LEN = 4
#DEFAULT_RELA_GAP_LEN = 4

SAMPLE_NAME_SECTION_SYMBOL_MARKER = "I_m_section_marker_symbol_"

def string_to_bytearray(__string: str):
	test = bytearray(__string, "utf-8")
	test.append(0x00)
	return test

def find_pattern(blob, pattern: str):
	"""
	return list of offsets
	"""
	idx_list = []
	idx = 0
	while idx != -1:
		idx = blob[idx: -1].find(pattern)
		if idx != -1:
			idx_list.append(idx)
			idx = idx + 1
	return idx_list

def remove_prefix(string : str, prefix: str) -> str:
	if string.startswith(prefix):
		return string[len(prefix):]
	return None

# support class for altinstructions handling
class Sec_Altinstructions_Data:
	orig_sec_idx = -1
	orig_offset = -1
	orig_len = -1
	alt_sec_idx = -1
	alt_offset = -1
	alt_len = -1
	cpufeature = -1

class Sec_Jumptable_Data:
	target_sec_idx = -1
	target_offset = -1
	code = -1
	key = -1

class Sec_Symtable_data:
	st_name = -1 # Symbol name, index in string tbl
	st_info = -1 # Type and binding attributes
	st_other = -1 # No defined meaning, 0
	st_shndx = -1 # Associated section index
	st_value = -1 # Value of the symbol
	st_size = -1	# Associated symbol size
	sym_name_str = ""	# symbol name string

class Elf_Section:
	bin_img = None

	def __init__(self, img: bytearray):
		self.bin_img = bytearray(img)

	def find_bin_pattern(self, byte_pattern):
		return find_pattern(self.bin_img, byte_pattern)

	def find_symbols(self, *argv):
		return

	def write_img_by_offset(self, offset: int, data: bytearray):
		self.bin_img[offset: offset + len(data)] = data

class Elf64_Rela(Elf_Section):
	"""
	typedef struct elf64_rela {
	Elf64_Addr r_offset;	/* Location at which to apply the action */
	Elf64_Xword r_info;	/* index and type of relocation */
	Elf64_Sxword r_addend;	/* Constant addend used to compute value */
	} Elf64_Rela;
	"""

	rela_struct_format = 'QQQ'
	section = None

	# list of lists [ target_sec_offset, target_len, ref_sec_idx, ref_sec_offset ]
	rela_info = None

	def __init__(self, img: bytearray):
		super().__init__(img)
		self.derive_rela_info(img)

	def derive_rela_info(self, img: bytearray):
		self.rela_info = []
		__rela_struct_size = struct.calcsize(self.rela_struct_format)
		for i in range(ceil(len(img)/__rela_struct_size)):
			__begin = i * __rela_struct_size
			__end = i * __rela_struct_size + __rela_struct_size
			[ r_offset,
			r_info,
			r_addend ] = list(struct.unpack(self.rela_struct_format,
											self.bin_img[__begin: __end]))
			__rela_type = r_info & 0x00000000ffffffff
			__ref_sec_ndx = r_info >> 32  # to be added extra handling
			__ref_sec_offset = r_addend  # to be added extra handling
			self.rela_info.append([ r_offset,
									self.get_gap_len(__rela_type),
									__ref_sec_ndx,
									__ref_sec_offset])
			# Note: here __ref_sec_ndx __ref_sec_offset actually contain
			# sumbol Num and offset/addent respectively

	def zeroize_mapping_relocations_info(self, sym_sec):
		"""
		rela_info.ref_sec_idx contains section index as number in symbol section
		rela_info.ref_sec_offset contains offset from the symbol position
		more convenient to operate direct section index and offset from section zero,
		the function updates the fields properly
		"""
		for __ri in self.rela_info:
			__ref_sym_idx = __ri[2]
			__ri[2] = sym_sec.section.get_sec_idx_by_num(__ri[2])
			__ri[3] = sym_sec.section.get_sym_offset_by_num(__ref_sym_idx) + __ri[3]
			# offset of "data source for relocation" from the section zero is
			# sum of referenced symbol offset and addend

	def get_section_gaps(self) -> list:
		ret_list = []
		for i in self.rela_info:
			ret_list.append([i[0], i[1]])

		return ret_list

	def get_full_rela_info(self) -> list:
		return self.rela_info

	def get_gap_len(self, rela_type: int) -> int:
		if rela_type in (
			R_AARCH64_MOVW_UABS_G0, 	R_AARCH64_MOVW_UABS_G0_NC,	R_AARCH64_MOVW_UABS_G1,
			R_AARCH64_MOVW_UABS_G1_NC,	R_AARCH64_MOVW_UABS_G2,		R_AARCH64_MOVW_UABS_G2_NC,
			R_AARCH64_MOVW_UABS_G3,		R_AARCH64_MOVW_SABS_G0,		R_AARCH64_MOVW_SABS_G1,
			R_AARCH64_MOVW_SABS_G2,		R_AARCH64_LD_PREL_LO19,		R_AARCH64_ADR_PREL_LO21,
			R_AARCH64_ADR_PREL_PG_HI21,	R_AARCH64_ADR_PREL_PG_HI21_NC,	R_AARCH64_ADD_ABS_LO12_NC,
			R_AARCH64_LDST8_ABS_LO12_NC,R_AARCH64_TSTBR14,			R_AARCH64_CONDBR19,
			R_AARCH64_JUMP26,			R_AARCH64_CALL26,			R_AARCH64_LDST16_ABS_LO12_NC,
			R_AARCH64_LDST32_ABS_LO12_NC,	R_AARCH64_LDST64_ABS_LO12_NC, R_AARCH64_LDST128_ABS_LO12_NC,
			R_AARCH64_MOVW_PREL_G0,		R_AARCH64_MOVW_PREL_G0_NC,	R_AARCH64_MOVW_PREL_G1,
			R_AARCH64_MOVW_PREL_G1_NC,	R_AARCH64_MOVW_PREL_G2,		R_AARCH64_MOVW_PREL_G2_NC,
			R_AARCH64_MOVW_PREL_G3):
			return 4
		if rela_type in (
			R_AARCH64_ABS64,			R_AARCH64_ABS32,			R_AARCH64_ABS16,
			R_AARCH64_PREL64,			R_AARCH64_PREL32,			R_AARCH64_PREL16,
			R_AARCH64_RELATIVE):
			return 8
		return 4

class ELF64_jumptable(Elf_Section):
	"""
	__jumptable section item described in jump_label.h
	struct jump_entry {
	s32 code;
	s32 target;
	long key;	// key may be far away from the core kernel under KASLR
	};

	class Sec_Jumptable_Data:
		target_sec_idx = -1,
		target_offset = -1
		code = -1,
		key = -1
	"""

	jumptable_struct_format = '<iiQ'
	section = None
	jumptable_rec = None  # list of instances 'class Sec_Jumptable_Data'

	def __init__(self, img: bytearray):
		self.jumptable_rec = []
		super().__init__(img)
		__jumptable_struct_size = struct.calcsize(self.jumptable_struct_format)
		for i in range(ceil(len(self.bin_img)/__jumptable_struct_size)):
			__jt_rec = Sec_Jumptable_Data()
			__begin = i * __jumptable_struct_size
			__end = __begin + __jumptable_struct_size

			[ __jt_rec.code,
			__jt_rec.target_offset,
			__jt_rec.key ] = list(struct.unpack(self.jumptable_struct_format,
									self.bin_img[__begin: __end]))
			self.jumptable_rec.append(__jt_rec)

	def set_records_value_by_offset(self, offset_to_write: int, sec_idx: int, sec_offset: int):
		"""
		Set 'target_offset' and 'target_sec_id' on in-section offset
		"""
		__jt_struct_size = struct.calcsize(self.jumptable_struct_format)
		__rec_idx = offset_to_write // __jt_struct_size
		__rec_value_offset = offset_to_write % __jt_struct_size

		# We need to feel only fields target_offset and target_sec_idx the both say
		# where instructions should be replaced, other fields are currentl useless.
		if __rec_value_offset == 0:
			self.jumptable_rec[__rec_idx].target_sec_idx = sec_idx
			self.jumptable_rec[__rec_idx].target_offset = sec_offset

	def show_class_content(self):
		log_d("Jumptable section content")
		for __jt in self.jumptable_rec:
			log_d(__jt.code, __jt.target_sec_idx, __jt.target_offset, __jt.key)

	def get_jumptable_gaps_for_section_idx(self, sec_idx: int) -> list:
		ret_list = []
		for i in self.jumptable_rec:
			if i.target_sec_idx == sec_idx:
				ret_list.append([i.target_offset, DEFAULT_ARM_INSTRUCTION_LEN])
		return ret_list

class Elf64_altinstructions(Elf_Section):
	"""
	.altinstructions section contains an array of struct alt_instr.
	As instance, for kernel 4.14 from /arch/arm64/include/asm/alternative.h
	struct alt_instr {
		s32 orig_offset;    /* offset to original instruction */
		s32 alt_offset;     /* offset to replacement instruction */
		u16 cpufeature;     /* cpufeature bit set for replacement */
		u8  orig_len;       /* size of original instruction(s) */
		u8  alt_len;        /* size of new instruction(s), <= orig_len */
	};

	class section_altinstructions_data:
		orig_sec_idx = -1,
		orig_offset = -1,
		orig_len = -1,
		alt_sec_idx = -1,
		alt_offset = -1,
		alt_len = -1,
		cpufeature = -1
	"""

	altinstructions_struct_format = '<iiHBB'
	section = None
	altinstruction_rec = None  # dict of instances 'class Sec_Altinstructions_Data'

	def __init__(self, img: bytearray):
		self.altinstruction_rec = {}
		super().__init__(img)
		__altinstr_struct_size = struct.calcsize(self.altinstructions_struct_format)
		for i in range(ceil(len(self.bin_img)/__altinstr_struct_size)):
			__alt_rec = Sec_Altinstructions_Data()
			__begin = i * __altinstr_struct_size
			__end = __begin + __altinstr_struct_size
			[ __alt_rec.orig_offset,
			__alt_rec.alt_offset,
			__alt_rec.cpufeature,
			__alt_rec.orig_len,
			__alt_rec.alt_len ] = list(struct.unpack(self.altinstructions_struct_format,
											self.bin_img[__begin: __end]))
			self.altinstruction_rec[i] = __alt_rec

	def set_records_value_by_offset(self, offset_to_write: int, sec_idx: int, offset: int):
		"""
		Set 'alt_inst' or 'orig_instr' fields in record based on in-section offset
		"""
		__altinstr_struct_size = struct.calcsize(self.altinstructions_struct_format)
		__rec_idx = offset_to_write // __altinstr_struct_size
		__rec_value_offset = offset_to_write % __altinstr_struct_size

		if __rec_value_offset == 0:
			self.altinstruction_rec[__rec_idx].orig_offset = offset
			self.altinstruction_rec[__rec_idx].orig_sec_idx = sec_idx
		elif __rec_value_offset == 4:
			self.altinstruction_rec[__rec_idx].alt_offset = offset
			self.altinstruction_rec[__rec_idx].alt_sec_idx = sec_idx
		else:
			raise RuntimeError(" Wrong record offset")

	def show_class_content(self):
		log_d("Altinstructions section content")
		for key, value in self.altinstruction_rec.items():
			log_d(key,
				value.orig_sec_idx,
				value.orig_offset,
				value.orig_len,
				value.alt_sec_idx,
				value.alt_offset,
				value.alt_len,
				value.cpufeature)

	def get_altinst_gaps_for_section_idx(self, sec_idx: int) -> list:
		ret_list = []
		for _, value in self.altinstruction_rec.items():
			if value.orig_sec_idx == sec_idx:
				ret_list.append([value.orig_offset, value.orig_len])
		return ret_list

class Elf64_Sym(Elf_Section):
	"""
	structure mappinng based on /include/uapi/linux/elf.h
	typedef struct elf64_sym {
	  Elf64_Word st_name;		/* Symbol name, index in string tbl */
	  unsigned char	st_info;	/* Type and binding attributes */
	  unsigned char	st_other;	/* No defined meaning, 0 */
	  Elf64_Half st_shndx;		/* Associated section index */
	  Elf64_Addr st_value;		/* Value of the symbol */
	  Elf64_Xword st_size;		/* Associated symbol size */
	} Elf64_Sym;
	"""

	str_sym_table_format = 'IBBHQQ'
	section = None
	syms = None  # list of instances of Sec_Symtable_data

	def __init__(self, img: bytearray):
		self.syms = []
		super().__init__(img)

	def find_symbols(self, strtab: bytearray):
		__struct_size = struct.calcsize(self.str_sym_table_format)
		for i in range(int(len(self.bin_img)/__struct_size)):
			__temp_sym = Sec_Symtable_data()
			[ __temp_sym.st_name,
			__temp_sym.st_info,
			__temp_sym.st_other,
			__temp_sym.st_shndx,
			__temp_sym.st_value,
			__temp_sym.st_size ] = list(struct.unpack(self.str_sym_table_format,
											self.bin_img[i * __struct_size: i * __struct_size + __struct_size]))
			__temp_sym.sym_name_str = strtab[__temp_sym.st_name:].split(b'\x00')[0].decode("utf-8")

			# look at specific section symbols
			if __temp_sym.st_info == STT_SECTION:
				__temp_sym.sym_name_str = SAMPLE_NAME_SECTION_SYMBOL_MARKER + str(__temp_sym.st_shndx)

			# look at mapping symbols $x $d $t ... which can be
			# met several times in the same section
			if (__temp_sym.sym_name_str.startswith('$x') or
				__temp_sym.sym_name_str.startswith('$d') or
				__temp_sym.sym_name_str.startswith('$t') or
				__temp_sym.sym_name_str.startswith('$a') or
				__temp_sym.sym_name_str.startswith('$v')):
				__temp_sym.sym_name_str = __temp_sym.sym_name_str + "_sec_" + str(__temp_sym.st_shndx)
				uniq_id = 0
				while __temp_sym.sym_name_str + "_" + str(uniq_id) in (_l.sym_name_str for _l in self.syms):
					uniq_id = uniq_id + 1

				__temp_sym.sym_name_str = __temp_sym.sym_name_str + "_" + str(uniq_id)
			self.syms.append(__temp_sym)
		self.show_class_content()

	def lookup_symbol_name(self, sym_name_pattern: str) -> list:
		ret_lst = []
		__r = re.compile(sym_name_pattern)
		for _l in self.syms:
			if len(__r.findall(_l.sym_name_str)) > 0:
				ret_lst.append(_l.sym_name_str)
		# to provide original symbol name at the list head, sort it
		return sorted(ret_lst)

	def get_sym_by_name(self, sym_name: str):
		for _l in self.syms:
			if _l.sym_name_str == sym_name:
				return _l
		return None

	def get_sym_offset_by_name(self, sym_name: str) -> int:
		return self.get_sym_by_name(sym_name).st_value

	def get_sec_idx_by_name(self, sym_name: str) -> int:
		return self.get_sym_by_name(sym_name).st_shndx

	def get_sym_size_by_name(self, sym_name: str) -> int:
		return self.get_sym_by_name(sym_name).st_size

	def get_sym_type_by_name(self, sym_name: str) -> int:
		return self.get_sym_by_name(sym_name).st_info & 0x0f

	def get_sym_bind_by_name(self, sym_name: str) -> int:
		return self.get_sym_by_name(sym_name).st_info >> 4

	def get_sym_vis_by_name(self, sym_name: str) -> int:
		return self.get_sym_by_name(sym_name).st_other & 0x03

	def get_sec_idx_by_num(self, num: int) -> int:
		return self.syms[num].st_shndx

	def get_sym_offset_by_num(self, num: int) -> int:
		return self.syms[num].st_value

	def get_all_sym_names_by_sec_idx(self, sec_idx: int) -> list:
		return [ __item.sym_name_str for __item in self.syms if __item.st_shndx == sec_idx ]

	def show_class_content(self):
		log_d(" Symbols -------- ")
		for i, _l in zip(range(len(self.syms)), self.syms):
			log_d(" {:4} {:65} {:6} {:6} {:6} {:6} {:10} {:6}".format(
			i,
			_l.sym_name_str,
			hex(_l.st_name),
			hex(_l.st_info),
			hex(_l.st_other),
			hex(_l.st_shndx),
			hex(_l.st_value),
			hex(_l.st_size)))

class Elf64_Shdr:
	"""
	structure mappinng based on /include/uapi/linux/elf.h
	or general ELF specification
	"""
	shdr_struct_format = 'IIQQQQIIQQ'

	section = None
	section_name_str = None
	section_gaps = None
	section_is_init = False

	def __init__(self, shdr_img):
		self.section_gaps = []
		sh_struct_len = struct.calcsize(Elf64_Shdr.shdr_struct_format)
		[ self.sh_name,
		self.sh_type,
		self.sh_flags,
		self.sh_addr,
		self.sh_offset,
		self.sh_size,
		self.sh_link,
		self.sh_info,
		self.sh_addralign,
		self.sh_entsize ] = list(struct.unpack(self.shdr_struct_format,
								shdr_img[0: sh_struct_len]))

	def sh_set_name(self, sh_strtab_sec: bytearray):
		__name = sh_strtab_sec[self.sh_name:].split(b'\x00')
		self.section_name_str = __name[0].decode()

	def sh_populate(self, sec_bin: bytearray):
		if self.sh_type == SHT_SYMTAB:
			self.section = Elf64_Sym(sec_bin)
		elif self.sh_type == SHT_RELA:
			self.section = Elf64_Rela(sec_bin)
		elif self.section_name_str == DEFAULT_NAME_SECTION_ALTINSTR:
			self.section = Elf64_altinstructions(sec_bin)
		elif self.section_name_str == DEFAULT_NAME_SECTION_JUMPTBL:
			self.section = ELF64_jumptable(sec_bin)
		else:
			self.section = Elf_Section(sec_bin)

	def sh_find_symbols(self, img_strtab: bytearray):
		self.section.find_symbols(img_strtab)

	def get_all_sym_names_by_sec_idx(self, sec_idx: int) -> list:
		return self.section.get_all_sym_names_by_sec_idx(sec_idx)

	def get_section_gaps(self) -> list:
		return self.section.get_section_gaps()

	def get_type(self):
		return self.sh_type

	def get_flags(self):
		return self.sh_flags

	def sh_lookup_symbol_name(self, sym_name_pattern : str) -> list:
		return self.section.lookup_symbol_name(sym_name_pattern)


class Elf64_Ehdr:
	"""
	typedef struct elf64_hdr {
	  unsigned char	e_ident[EI_NIDENT];	/* ELF "magic number" */
	  Elf64_Half e_type;
	  Elf64_Half e_machine;
	  Elf64_Word e_version;
	  Elf64_Addr e_entry;		/* Entry point virtual address */
	  Elf64_Off e_phoff;		/* Program header table file offset */
	  Elf64_Off e_shoff;		/* Section header table file offset */
	  Elf64_Word e_flags;
	  Elf64_Half e_ehsize;
	  Elf64_Half e_phentsize;
	  Elf64_Half e_phnum;
	  Elf64_Half e_shentsize;
	  Elf64_Half e_shnum;
	  Elf64_Half e_shstrndx;
	} Elf64_Ehdr;
	"""
	ehdr_struct_format = '16shhIQQQIhhhhhh'

	def __init__(self, img):
		__elf_hdr_len = struct.calcsize(self.ehdr_struct_format)
		[ self.e_ident,
		self.e_type,
		self.e_machine,
		self.e_version,
		self.e_entry,
		self.e_phoff,
		self.e_shoff,
		self.e_flags,
		self.e_ehsize,
		self.e_phentsize,
		self.e_phnum,
		self.e_shentsize,
		self.e_shnum,
		self.e_shstrndx ] =	list(struct.unpack(self.ehdr_struct_format,
								img[0: __elf_hdr_len]))
		log_d("\tSection headers num : {}".format(self.e_shnum))

class ELF:
	elf_file_name = None
	binary_img = None
	ehdr = None
	shdr = None

	def __init__(self, file_name):
		self.shdr = []
		self.elf_file_name = file_name
		with open(file_name, "rb") as file:
			self.binary_img = file.read()

		self.ehdr = Elf64_Ehdr(self.binary_img)
		__sh_off = self.ehdr.e_shoff
		for _ in range(0, self.ehdr.e_shnum):
			__shdr = Elf64_Shdr(self.binary_img[__sh_off: __sh_off + self.ehdr.e_shentsize])
			__sh_off = __sh_off + self.ehdr.e_shentsize
			self.shdr.append(__shdr)

		# section names and sections data
		self.find_section_names()
		for __sh in self.shdr:
			__sh.sh_populate(self.binary_img[__sh.sh_offset: __sh.sh_offset + __sh.sh_size])

		# symbols
		__strtab = self.get_section_by_name(DEFAULT_NAME_SECTION_STRTAB)
		for __sh in self.shdr:
			__sh.sh_find_symbols(__strtab.section.bin_img)

		# alternation instructions
		self.apply_relocation_for_alternation_instructions()

		# jumptable
		self.apply_relocation_for_jumptable_instructions()

	def calc_syms_info(self):
		__strtab = self.get_section_by_name(DEFAULT_NAME_SECTION_STRTAB)
		for __shdr in self.shdr:
			__shdr.calc_syms_info(__strtab)

	def find_section_names(self):
		__str_offset_sechdr_lst = []
		for __shdr in self.shdr:
			if __shdr.sh_type == SHT_STRTAB:
				__start_bin_idx = __shdr.sh_offset
				__end_bin_idx = __shdr.sh_offset + __shdr.sh_size
				__temp_lst = find_pattern(self.binary_img[__start_bin_idx: __end_bin_idx],
										  string_to_bytearray(DEFAULT_NAME_SECTION_SHSTRTAB))
				if len(__temp_lst) > 0:
					__str_offset_sechdr_lst.append([__shdr, __temp_lst])

		if len(__str_offset_sechdr_lst) != 1 or len(__str_offset_sechdr_lst[0][1]) != 1:
			log_e(" ERROR: something wrong with section name string search ")
			raise RuntimeError

		__sh_strtab = __str_offset_sechdr_lst[0][0]
		for __shdr in self.shdr:
			__shdr.sh_set_name(self.binary_img[__sh_strtab.sh_offset:
											__sh_strtab.sh_offset + __sh_strtab.sh_size ])

	def get_section_by_name(self, name: str) -> Elf64_Shdr:
		for i in self.shdr:
			if i.section_name_str == name:
				return i
		return None

	def get_section_idx_by_name(self, name: str) -> int:
		idx = 0
		for i in self.shdr:
			if i.section_name_str == name:
				return idx
			idx = idx + 1
		return idx

	def get_rela_gaps_for_section(self, sec_name: str) -> list:
		ret_list = []

		rela_prefix = DEFAULT_RELA_PREFIX + sec_name
		for __sec in self.shdr:
			if __sec.section_name_str.startswith(rela_prefix):
				ret_list = ret_list + __sec.get_section_gaps()
		return ret_list

	def get_altinstr_gaps_for_section(self, sec_name: str) -> list:
		altinstr_sec = self.get_section_by_name(DEFAULT_NAME_SECTION_ALTINSTR)
		if altinstr_sec is None:
			return []
		__sec_idx = self.get_section_idx_by_name(sec_name)
		return altinstr_sec.section.get_altinst_gaps_for_section_idx(__sec_idx)

	def get_jump_table_gaps_for_section(self, sec_name: str) -> list:
		jumptbl_sec = self.get_section_by_name(DEFAULT_NAME_SECTION_JUMPTBL)
		if jumptbl_sec is None:
			return []
		__sec_idx = self.get_section_idx_by_name(sec_name)
		return jumptbl_sec.section.get_jumptable_gaps_for_section_idx(__sec_idx)

	def get_section_owner_of_sym(self, sym_name: str) -> Elf64_Shdr:
		__sec_hdr = self.get_section_by_name(DEFAULT_NAME_SECTION_SYMTAB)
		__sh_idx = __sec_hdr.section.get_sec_idx_by_name(sym_name)
		return self.shdr[__sh_idx]

	def get_sections_all_names(self) -> list:
		ret_list = []
		for i in self.shdr:
			ret_list.append(i.section_name_str)
		return ret_list

	def get_all_sym_names_by_sec_idx(self, sec_idx: int) -> list:
		__sec_hdr = self.get_section_by_name(DEFAULT_NAME_SECTION_SYMTAB)
		return __sec_hdr.get_all_sym_names_by_sec_idx(sec_idx)

	def get_symbol_offset_in_sec(self, sym_name: str) -> int:
		__sec_hdr = self.get_section_by_name(DEFAULT_NAME_SECTION_SYMTAB)
		return __sec_hdr.section.get_sym_offset_by_name(sym_name)

	def get_symbol_by_voluntary_search(self, sym_name_pattern: str) -> list:
		__sec_hdr = self.get_section_by_name(DEFAULT_NAME_SECTION_SYMTAB)
		return __sec_hdr.sh_lookup_symbol_name(sym_name_pattern)

	def get_symbol_size(self, sym_name: str) -> int:
		__sec_hdr = self.get_section_by_name(DEFAULT_NAME_SECTION_SYMTAB)
		return __sec_hdr.section.get_sym_size_by_name(sym_name)

	def get_symbol_type(self, sym_name: str) -> int:
		__sec_hdr = self.get_section_by_name(DEFAULT_NAME_SECTION_SYMTAB)
		return __sec_hdr.section.get_sym_type_by_name(sym_name)

	def get_symbol_vis(self, sym_name: str) -> int:
		__sec_hdr = self.get_section_by_name(DEFAULT_NAME_SECTION_SYMTAB)
		return __sec_hdr.section.get_sym_vis_by_name(sym_name)

	def get_symbol_bind(self, sym_name: str) -> int:
		__sec_hdr = self.get_section_by_name(DEFAULT_NAME_SECTION_SYMTAB)
		return __sec_hdr.section.get_sym_bind_by_name(sym_name)

	def get_section_size_where_sym_lives(self, sym_name: str) -> int:
		__sec_hdr = self.get_section_by_name(DEFAULT_NAME_SECTION_SYMTAB)
		__sh_idx = __sec_hdr.section.get_sec_idx_by_name(sym_name)
		return self.shdr[__sh_idx].sh_size

	def get_symbol_offset_from_elf_begin(self, sym_name: str) -> int:
		__sec_hdr = self.get_section_owner_of_sym(sym_name)
		symbol_offset_in_sec = self.get_symbol_offset_in_sec(sym_name)
		section_ofset_in_file = __sec_hdr.sh_offset
		return symbol_offset_in_sec + section_ofset_in_file

	def write_symbol_value(self, sym_name: str, data: bytearray):
		# write to section img
		__sec_offset = self.get_symbol_offset_in_sec(sym_name)
		__sec_sym_owner = self.get_section_owner_of_sym(sym_name)
		__sec_sym_owner.section.write_img_by_offset(__sec_offset, data)
		__file_offset = self.get_symbol_offset_from_elf_begin(sym_name)
		Utils().file_write_data(self.elf_file_name, data, __file_offset)

	def  apply_relocation_for_alternation_instructions(self):
		"""
		Should poppulate instance of class Elf64_altinstructions with data
		"""
		altinstr_sec = self.get_section_by_name(DEFAULT_NAME_SECTION_ALTINSTR)
		if altinstr_sec is None:
			return

		self.apply_relocations_for_section(altinstr_sec)
		altinstr_sec.section.show_class_content()

	def apply_relocation_for_jumptable_instructions(self):
		"""
		Should poppulate instance of class Elf64_jumptable with data:
		"""
		jumptable_sec = self.get_section_by_name(DEFAULT_NAME_SECTION_JUMPTBL)
		if jumptable_sec is None:
			return

		self.apply_relocations_for_section(jumptable_sec)
		jumptable_sec.section.show_class_content()

	def apply_relocations_for_section(self, target_sec):
		rela_info = None

		rela_target_sec = self.get_section_by_name(DEFAULT_RELA_PREFIX + target_sec.section_name_str)
		if rela_target_sec is not None:
			symtab_sec = self.get_section_by_name(DEFAULT_NAME_SECTION_SYMTAB)
			rela_target_sec.section.zeroize_mapping_relocations_info(symtab_sec)

			# rela info list of lists [ dest_sec_offset, dest_len, src_sec_idx, src_sec_offset ]
			rela_info = rela_target_sec.section.get_full_rela_info()

			# fill target section records
			for __rela_item in rela_info:
				offset_to_wr = __rela_item[0]
				sec_idx = __rela_item[2]
				sec_offset = __rela_item[3]
				target_sec.section.set_records_value_by_offset(offset_to_wr, sec_idx, sec_offset)
