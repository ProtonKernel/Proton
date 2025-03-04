#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
sys.dont_write_bytecode = True
import binascii
import hmac
import hashlib
import struct
from Utils import Utils
from Utils import log_d, log_i
from ELF import ELF
from ELF import STT_FUNC
from IntegrityCheckCodeGen import get_sec_sym_list_ordered
from IntegrityCheckCodeGen import PREFIX_ANCHOR_VAR

SECTION_NAME_WHERE_EMBEDDED = ".rodata"
SYMBOL_NAME_BUILDTIME_HMAC = PREFIX_ANCHOR_VAR + "_builtime_hmac"

CFI_SUBST_SUFFIX = ".cfi_jt"
LLVM_SUBST_GLOBAL_SUFFIX = ".llvm."

class IntegrityCheckProvider:
	elf = None
	sec_lst = None
	sym_lst = None
	hmac_key = "The quick brown fox jumps over the lazy dog"
	gaps = {}

	format_chunk_amount = 'I'
	format_chunk_arr = 'III' # sec_no, start, end
	format_sec_amount = 'I'
	format_insection_offset = 'I'

	def __init__(self, __elf: ELF, __sec_sym_lst: list):
		self.elf = __elf
		self.sec_lst = [ __s[0] for __s in __sec_sym_lst ]
		self.sym_lst = [ __s[1] for __s in __sec_sym_lst ]

		# store gaps from elf it is necessary because
		# different types of gaps should be handled and
		# instance of the class should be able to add gaps
		for __sc in self.sec_lst:

			self.gaps[__sc] = self.elf.get_rela_gaps_for_section(__sc)
			self.gaps[__sc] += self.elf.get_altinstr_gaps_for_section(__sc)
			self.gaps[__sc] += self.elf.get_jump_table_gaps_for_section(__sc)
			self.gaps[__sc].sort()

		# skip build_HMAC carrier symbol, add it as gap
		hmac_sym_offset = self.elf.get_symbol_offset_in_sec(SYMBOL_NAME_BUILDTIME_HMAC)
		hmac_sym_len = self.elf.get_symbol_size(SYMBOL_NAME_BUILDTIME_HMAC)
		hmac_rec = [hmac_sym_offset, hmac_sym_len]

		if SECTION_NAME_WHERE_EMBEDDED not in self.gaps.keys():
			self.gaps[SECTION_NAME_WHERE_EMBEDDED] = []
			self.gaps[SECTION_NAME_WHERE_EMBEDDED].append(hmac_rec)
			return

		__g_idx_hmac_to_be_inserted_before = -1
		__gaps = self.gaps[SECTION_NAME_WHERE_EMBEDDED]
		for g_idx in range(len(__gaps)):
			if __gaps[g_idx][0] > hmac_rec[0]:
				__g_idx_hmac_to_be_inserted_before = g_idx
				break

		if __g_idx_hmac_to_be_inserted_before == -1:
			__gaps.append(hmac_rec)
		else:
			__gaps.insert(__g_idx_hmac_to_be_inserted_before, hmac_rec)

	def get_gaps_for_section(self, sec_name: str) -> list:
		return self.gaps[sec_name]

	def get_substitute_sym_name(self, sym_name: str) -> str:
		# pretty possible the symbol is renamed due to LLVM activity
		# look at symbols starting with 'sym_name'
		__symbol_search_pattern = "^" + sym_name
		sym_lst = self.elf.get_symbol_by_voluntary_search(__symbol_search_pattern)

		if len(sym_lst) == 1 and sym_lst[0] == sym_name:
			# default routine, no surpises
			return sym_name

		if len(sym_lst) > 1 and sym_lst[0] == sym_name:
			if self.elf.get_symbol_type(sym_name) == STT_FUNC:
			# CFI substitution is possible for executable items on such case
			# two symbols are available sym_name and sym_name + ".cfi_jt"
				cfi_sym_name = sym_name + CFI_SUBST_SUFFIX
				sym_lst = self.elf.get_symbol_by_voluntary_search(cfi_sym_name)
				if len(sym_lst) == 1 and sym_lst[0] == cfi_sym_name:
					return sym_lst[0]
				raise RuntimeError(" ERROR: something is wrong with CFI version of chosen symbol ..")
		# as extra option here are similar name local symbols like xxx.[:digits:], to be checked later
		raise RuntimeError(" ERROR: something is wrong with symbols search..")

	def embed_anchor_offset_array(self):
		__offset_arr = bytearray()
		for __sym_name in self.sym_lst:
			sym_name_checked = self.get_substitute_sym_name(__sym_name)
			__o = self.elf.get_symbol_offset_in_sec(sym_name_checked)
			log_d(" anchor symbol {} offset: {}".format(sym_name_checked, __o))
			__offset_arr.extend(struct.pack(self.format_insection_offset, __o))

		array_name = PREFIX_ANCHOR_VAR + "_anchor_offset"
		self.elf.write_symbol_value(array_name, __offset_arr)

	def embed_section_amount_var(self):
		__sec_amount = bytearray()
		var_name = PREFIX_ANCHOR_VAR + "_sec_amount"
		__sec_amount = struct.pack(self.format_sec_amount, len(self.sec_lst))
		self.elf.write_symbol_value(var_name, __sec_amount)

	def embed_buildtime_hmac(self, buildtime_hmac: bytearray):
		self.elf.write_symbol_value(SYMBOL_NAME_BUILDTIME_HMAC,  buildtime_hmac)

	def embed_chunks_info(self, glob_chunks_info: list):
		__chunk_arr = bytearray()
		__chunk_amount = bytearray()

		for __gci in glob_chunks_info:
			__gc = struct.pack(self.format_chunk_arr, __gci[0],  __gci[1],  __gci[2])
			__chunk_arr.extend(__gc)

		array_name = PREFIX_ANCHOR_VAR + "_chunks_info"
		self.elf.write_symbol_value(array_name, __chunk_arr)

		__chunk_amount = struct.pack(self.format_chunk_amount, len(glob_chunks_info))
		var_name = PREFIX_ANCHOR_VAR + "_chunk_amount"
		self.elf.write_symbol_value(var_name, __chunk_amount)

	def get_global_chunks_arr(self) -> list:
		glob_chunks_arr = list()

		for __i, __sc in zip(range(len(self.sec_lst)), self.sec_lst):
			__sec_size = self.elf.get_section_by_name(__sc).sh_size
			__gaps = self.get_gaps_for_section(__sc)

			# provide chunks array HMAC will be calculated over
			a_arr = self.get_areas(__gaps, __sec_size)
			for __a in a_arr:
				glob_chunks_arr.append([__i, __a[0], __a[1]])

		return glob_chunks_arr

	def get_areas(self, gaps: list, area_len) -> list:
		# works with sorted non overlapped gaps ranges
		area = [[0, area_len - 1]]
		__r_lst_item = 0
		for __g in gaps:
			__curr_gap_begin = __g[0]
			__curr_gap_end = __g[0] + __g[1] - 1

			if area[__r_lst_item][0] == __curr_gap_begin and area[__r_lst_item][1] == __curr_gap_end:
				del area[__r_lst_item]
				break

			if area[__r_lst_item][0] == __curr_gap_begin and area[__r_lst_item][1] > __curr_gap_end:
				area[__r_lst_item][0] = __curr_gap_end + 1
				continue

			if area[__r_lst_item][0] < __curr_gap_begin and area[__r_lst_item][1] > __curr_gap_end:
				area.append([__curr_gap_end + 1, area[__r_lst_item][1]])
				area[__r_lst_item][1] = __curr_gap_begin - 1
				__r_lst_item = __r_lst_item + 1
				continue

			if area[__r_lst_item][1] == __curr_gap_end:
				area[__r_lst_item][1] = __curr_gap_begin - 1
				continue
		return area

	def calculate_global_hmac(self) -> bytearray:
		__digest = hmac.new(bytearray(self.hmac_key.encode("utf-8")), digestmod=hashlib.sha256)

		for __i, __sc in zip(range(len(self.sec_lst)), self.sec_lst):
			__sec_size = self.elf.get_section_by_name(__sc).sh_size
			__img = self.elf.get_section_by_name(__sc).section.bin_img
			__gaps = self.get_gaps_for_section(__sc)

			hmac_covered_dump =bytearray()

			a_arr = self.get_areas(__gaps, __sec_size)
			for __i in a_arr:
				hmac_covered_dump.extend(__img[__i[0]: __i[1] + 1])
				__digest.update(__img[__i[0]: __i[1] + 1])

			Utils().custom_print_to_file(hmac_covered_dump, "hmac_chunks_dump_"+__sc)
		__hmac = __digest.digest()
		log_i(" HMAC : {}".format(binascii.hexlify(__hmac)))
		return __hmac

if __name__ == "__main__":
	cmd_args = sys.argv

	ko_file = cmd_args[1]
	ic_support_file = cmd_args[2]
	sec_sym_lst = get_sec_sym_list_ordered(ic_support_file)

	elf = ELF(ko_file)
	ic_provider = IntegrityCheckProvider(elf, sec_sym_lst)

	global_chunks_info = ic_provider.get_global_chunks_arr()
	ic_provider.embed_chunks_info(global_chunks_info)
	ic_provider.embed_anchor_offset_array()
	ic_provider.embed_section_amount_var()
	global_hmac = ic_provider.calculate_global_hmac()
	ic_provider.embed_buildtime_hmac(global_hmac)
	sys.exit(0)
