#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
sys.dont_write_bytecode = True
import os
import re
from Utils import Utils
from Utils import log_d, log_i, debug_print
from ELF import ELF
from ELF import STV_DEFAULT, STT_FUNC, STT_OBJECT, STB_GLOBAL

pattern_sec_name_ic_target = [".text", ".rodata"]
pattern_sec_name_not_allowed = [".plt", ".rela", ".debug", "cfi", "ftrace", ".init", ".exit"]

NAME_FILE_SECTION_HANDLED_ALREADY = "section_is_handled_already"
PREFIX_ANCHOR_VAR = "fips140"
PREFIX_NAME_RET_FUNC = "ret_addr"
MIN_SYM_SIZE_BYTES = 4
PREFIX_ELF_OBJECTS = "elf_"
PREFIX_PATCHED_SOURCES = "fipsed_"

class CodeGenerator:
	sec_sym_file_dict = {}  # {section_name: [ symbol_name, file_where_symbol_is ]}

	def analize_elf_patch_source(self, __elf: ELF, source_file: str):
		available_target_sec = self.lookup_ci_target_sections(__elf)
		targets_sym_list = self.lookup_ci_target_symbols(__elf, available_target_sec) # ret [[sec_idx, sec_name, sym_name] ... ]
		self.patch_source_with_addr_ret_function(source_file, targets_sym_list)
		for lst in targets_sym_list:
			self.sec_sym_file_dict[lst[1]] = [lst[2], source_file]

	def is_section_handled_already(self, sc_name: str) -> bool:
		return bool(sc_name in self.sec_sym_file_dict)

	def is_section_name_match(self, sc_name: str) -> bool:
		for __s in pattern_sec_name_ic_target:
			if __s in sc_name:
				return True
		return False

	def is_section_name_allowed(self, sc_name: str) -> bool:
		for __na in	pattern_sec_name_not_allowed:
			if __na in sc_name:
				return False
		return True

	def lookup_ci_target_sections(self, __elf: ELF) -> list:
		"""
		Returns list of sections
		"""
		elf_sec_list = []
		res_sec_list = []

		elf_sec_list = __elf.get_sections_all_names()

		for __s_name in elf_sec_list:
			# check if the section name seems such
			# we need to cover with integrity check
			if not self.is_section_name_match(__s_name):
				continue

			# check the section name doesn't contain
			# not allowed substrings
			if not self.is_section_name_allowed(__s_name):
				continue

			# check if the section anchor is already
			# handled
			if self.is_section_handled_already(__s_name):
				continue

			res_sec_list.append(__s_name)

		debug_print(" found section(s) : {}".format(res_sec_list))
		return res_sec_list

	def lookup_ci_target_symbols(self, __elf: ELF, sec_lst: list) -> list:
		"""
		Returns list of [ section index, section name, symbol_name ]
		"""
		sym_list = []
		res_list = []
		for __sec_name in sec_lst:
			__sec_idx = __elf.get_section_idx_by_name(__sec_name)
			sym_list = __elf.get_all_sym_names_by_sec_idx(__sec_idx)
			for __sym in sym_list:
				__size = __elf.get_symbol_size(__sym)
				__type = __elf.get_symbol_type(__sym)
				__bind = __elf.get_symbol_bind(__sym)
				__vis = __elf.get_symbol_vis(__sym)
				if (__size > MIN_SYM_SIZE_BYTES
				and __vis == STV_DEFAULT
				and ((__type == STT_FUNC and __bind == STB_GLOBAL)
				  or (__type == STT_OBJECT))):
					# it is possible to add something here to choose most convenient
					# symbol.. but, actually the just 1st one is good enough
					res_list.append([__sec_idx, __sec_name, __sym])
					break

		log_d(" chosen pair : {}".format(res_list))
		return res_list

	def generate_support_source_file(self, file_name: str, sec_sym_file_dict: dict):
		sc_names = []
		sym_names = []
		header_name = os.path.basename(file_name).replace(".c", ".h")
		for k, v in sec_sym_file_dict.items():
			sc_names.append(k)
			sym_names.append(v[0])
		support_src = []
		support_src.append("#include \"stdint.h\"")
		support_src.append("#include \"{}\"".format(header_name))
		support_src.append("")
		support_src.append("#pragma clang optimize off")
		support_src.append("")
		support_src.append("#define FIPS_HMAC_SIZE 32")
		support_src.append("#define FIPS_CHUNKS_MAX_AMOUNT 2048")
		support_src.append("#define FIPS_SECTIONS_MAX_AMOUNT 48")
		support_src.append("#define FIPS_ANCHOR_SYMS_OFFS_MAX_AMOUNT FIPS_SECTIONS_MAX_AMOUNT")
		support_src.append("")
		for i, j in zip(sc_names, sym_names):
			support_src.append("extern void *{}{}(void); // {}".format(PREFIX_NAME_RET_FUNC, i.replace(".", "_"), j))
		support_src.append("")
		# anchor variables addrs array
		support_src.append("void *fips140_get_anchor_addr(uint32_t i)")
		support_src.append("{")
		support_src.append("	switch(i) {")
		for __sc, __i in zip(sc_names, range(len(sc_names))):
			support_src.append("	case {}: return (void *){}{}();".format(__i,
														PREFIX_NAME_RET_FUNC,
														__sc.replace(".", "_")))
		support_src.append("	default: return (void*)0;")
		support_src.append("	};")
		support_src.append("}")
		support_src.append("")
		# room for anchor symbols build-time in-section offsets. TO BE EMBEDDED
		support_src.append("__attribute__ ((section(\".rodata\"), unused))")
		support_src.append("uint32_t {}_anchor_offset[FIPS_SECTIONS_MAX_AMOUNT];"
														.format(PREFIX_ANCHOR_VAR))
		support_src.append("")
		# room for variable sections amount. TO BE EMBEDDED
		support_src.append("__attribute__ ((section(\".rodata\"), unused))")
		support_src.append("uint32_t {}_sec_amount;".format(PREFIX_ANCHOR_VAR))
		support_src.append("")
		# room for gaps info. TO BE EMBEDDED
		support_src.append("__attribute__ ((section(\".rodata\"), unused))")
		support_src.append("struct chunks_info {}_chunks_info[FIPS_CHUNKS_MAX_AMOUNT];"
														.format(PREFIX_ANCHOR_VAR))
		support_src.append("")
		# room for variable chunk amount. TO BE EMBEDDED
		support_src.append("__attribute__ ((section(\".rodata\"), unused))")
		support_src.append("uint32_t {}_chunk_amount;".format(PREFIX_ANCHOR_VAR))
		support_src.append("")
		# room for HMAC. TO BE EMBEDDED
		support_src.append("__attribute__ ((section(\".rodata\"), unused))")
		support_src.append("uint8_t {}_builtime_hmac[FIPS_HMAC_SIZE];".format(PREFIX_ANCHOR_VAR))
		support_src.append("")
		support_src.append("#pragma clang optimize on")
		support_src.append("")
		Utils().file_wr_data(file_name, support_src)

	def generate_support_file(self, source_file: str):
		self.generate_support_source_file(source_file, self.sec_sym_file_dict)

	def patch_source_with_addr_ret_function(self, file_name: str, sec_sym_lst: list):
		src = []
		for __i in sec_sym_lst:
			section_name = __i[1]
			symbol_name = __i[2]
			log_i(" Source: {} to be patched: {} {} ".format(os.path.basename(file_name),
															section_name, symbol_name))
			src.append("")
			src.append("/* Here and below auto-generated part */")
			src.append("#pragma clang optimize off ")
			src.append("void *{}{}(void)".format(PREFIX_NAME_RET_FUNC, section_name.replace(".", "_")))
			src.append("{")
			src.append(" 	return (void*)&{};".format(symbol_name))
			src.append("}")
			src.append("#pragma clang optimize on ")
			src.append("")
		Utils().file_append_data(file_name, src)


def derive_source_name_from_elf_object(object_name: str):
	"""
	Derives source name from object file name.
	hope we`ll never use cpp file
	"""
	__lst = re.split("(.*)elf_(.*).o", object_name)
	__naked_file_name = __lst[-2]
	__path = __lst[-3]
	c_file_name = os.path.join(__path, PREFIX_PATCHED_SOURCES + __naked_file_name + ".c")
	return c_file_name

def get_sec_sym_list_ordered(ic_support_file: str) -> list:
	"""
	Determines sections order by parsing ic_support_file
	"""
	res_sec_sym_list = []

	if not os.path.isfile(ic_support_file):
		raise RuntimeError(" ERROR: file not found. Ordered Sections/symbols list failed")
	with open(ic_support_file, "r") as file:
		lines = file.readlines()
		for __l in lines:
			anchor_var_pattern = "^extern void \\*{}(.*)\\(void\\); // (.*)$".format(PREFIX_NAME_RET_FUNC)
			__p = re.compile(anchor_var_pattern, re.MULTILINE)
			res = __p.findall(__l)
			if len(res) == 1 and len(res[0]) == 2:
				res_sec_sym_list.append([res[0][0].replace("_", "."), res[0][1]])

	log_i(res_sec_sym_list)
	if len(res_sec_sym_list) == 0:
		raise RuntimeError(" ERROR: sections <> symbols pairs list is not found")
	return res_sec_sym_list

if __name__ == "__main__":
	cmd_args = sys.argv
	if len(cmd_args) < 3:
		log_i("Usage " + sys.argv[0] + " source_root_directory" + " obj_file")
		sys.exit(-1)

	o_files = cmd_args[3:]
	c_support_file = cmd_args[1]
	sec_sym_file = cmd_args[2]

	c_file_to_generate = c_support_file
	cg = CodeGenerator()
	for _o_file in o_files:
		c_file_to_patch = derive_source_name_from_elf_object(_o_file)
		if not os.path.isfile(c_file_to_patch):
			raise RuntimeError(" ERROR: Source to be patched {} was not found".format(c_file_to_patch))
		elf = ELF(_o_file)
		cg.analize_elf_patch_source(elf, c_file_to_patch)

	cg.generate_support_file(c_support_file)
	sys.exit(0)
