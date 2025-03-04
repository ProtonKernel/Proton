#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import mmap
import sys
import json

LOG_DEBUG = 5
LOG_INFO = 1
LOG_ERROR = 0

CURRENT_LOG_LEVEL = LOG_INFO

def log_e(*args):
	if CURRENT_LOG_LEVEL < LOG_ERROR:
		return
	print(args)

def log_i(*args):
	if CURRENT_LOG_LEVEL < LOG_INFO:
		return
	print(args)

def log_d(*args):
	if CURRENT_LOG_LEVEL < LOG_DEBUG:
		return
	print(args)

def debug_print(data):
	if CURRENT_LOG_LEVEL < LOG_DEBUG:
		return
	if isinstance(data, list):
		for i in data:
			print(" {}".format(i))
	elif isinstance(data, dict):
		for key, value in data.items():
			print(key, value)
	elif isinstance(data, bytearray):
		i = 0
		width  = 16
		while i < len(data):
			print(data[i:  i + width].hex())
			i = i + width
	else:
		print(data)

class Utils():
	@staticmethod
	def file_write_data(file_name: str, data: bytearray, pos: int):
		with open(file_name, "r+b") as file:
			mm_hndl = mmap.mmap(file.fileno(), 0)
			mm_hndl.seek(pos)
			mm_hndl.write(bytes(data))

	@staticmethod
	def file_wr_data(file_name: str, data):
		with open(file_name, "w") as file:
			if isinstance(data, list):
				for i in data:
					file.write(i)
					file.write("\n")
			elif isinstance(data, str):
				file.write(data)

	@staticmethod
	def file_append_data(file_name: str, data):
		with open(file_name, "a") as file:
			if isinstance(data, list):
				for i in data:
					file.write(i)
					file.write("\n")
			elif isinstance(data, str):
				file.write(data)
			elif isinstance(data, dict):
				json.dump(data, file)
			else:
				raise RuntimeError(" ERROR: File append data type cant be handled")

	@staticmethod
	def file_read_dict(file_name: str) -> dict:
		with open(file_name, "r") as file:
			return json.load(file)

	@staticmethod
	def custom_print_to_file(data, filename: str):
		orig_stdout = sys.stdout
		with open(filename, 'w') as f_out:
			sys.stdout = f_out
			if isinstance(data, list):
				for i in data:
					print(" {}".format(i))
			elif isinstance(data, dict):
				for key, value in data.items():
					print(key, value)
			elif isinstance(data, bytearray):
				i = 0
				width  = 16
				while i < len(data):
					print(data[i:  i + width].hex())
					i = i + width
			else:
				print(data)

			sys.stdout = orig_stdout
