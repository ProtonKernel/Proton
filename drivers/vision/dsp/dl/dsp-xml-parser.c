// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dl/dsp-xml-parser.h"
#include "dl/dsp-sxml.h"
#include "dl/dsp-common.h"
#include "dl/dsp-string-tree.h"

#define COUNT(arr)	(sizeof(arr) / sizeof((arr)[0]))
#define TOKEN_END	(-255)
#define TOKEN_LIST_NUM	(64)
#define TOKEN_MAX	(64)
#define KERNEL_CNT_MAX	(256)

static struct dsp_string_tree_node *xml_str;
struct dsp_xml_lib_table *xml_libs;
static char *token_value;

static const char *const xml_strs[] = {
	"libs",
	"count",
	"lib",
	"name",
	"kernel_count",
	"kernel",
	"id",
	"pre",
	"exe",
	"post"
};

static void __dsp_xml_lib_table_init(void)
{
	xml_libs->lib_cnt = 0;
	dsp_hash_tab_init(&xml_libs->lib_hash);
}

void dsp_xml_parser_init(void)
{
	int idx;

	xml_str = (struct dsp_string_tree_node *)dsp_dl_malloc(
			sizeof(struct dsp_string_tree_node),
			"XML String tree");
	xml_libs = (struct dsp_xml_lib_table *)dsp_dl_malloc(
			sizeof(struct dsp_xml_lib_table),
			"XML lib table");
	token_value = (char *)dsp_dl_malloc(TOKEN_MAX, "Token value");

	__dsp_xml_lib_table_init();

	dsp_string_tree_init(xml_str);

	for (idx = 0; idx < TOKEN_NUM; idx++)
		dsp_string_tree_push(xml_str, xml_strs[idx], idx);

}

static void __dsp_xml_lib_free(struct dsp_xml_lib *lib)
{
	unsigned int idx;

	DL_DEBUG("XML lib free\n");
	dsp_dl_free(lib->name);

	for (idx = 0; idx < lib->kernel_cnt; idx++) {
		struct dsp_xml_kernel_table *kernel_table = &lib->kernels[idx];

		if (kernel_table->pre) {
			DL_DEBUG("free pre(%s)\n", kernel_table->pre);
			dsp_dl_free(kernel_table->pre);
		}

		if (kernel_table->exe) {
			DL_DEBUG("free exe(%s)\n", kernel_table->exe);
			dsp_dl_free(kernel_table->exe);
		}

		if (kernel_table->post) {
			DL_DEBUG("free post(%s)\n", kernel_table->post);
			dsp_dl_free(kernel_table->post);
		}
	}

	dsp_dl_free(lib->kernels);
}

static void __dsp_xml_lib_table_free(void)
{
	int idx;

	for (idx = 0; idx < DSP_HASH_MAX; idx++) {
		struct dsp_list_node *cur, *next;

		cur = (&xml_libs->lib_hash.list[idx])->next;

		while (cur != NULL) {
			struct dsp_hash_node *hash_node =
				container_of(cur, struct dsp_hash_node, node);
			struct dsp_xml_lib *lib =
				(struct dsp_xml_lib *)hash_node->value;

			next = cur->next;
			__dsp_xml_lib_free(lib);
			cur = next;
		}
	}

	dsp_hash_free(&xml_libs->lib_hash, 1);
}

void dsp_xml_parser_free(void)
{
	dsp_string_tree_free(xml_str);
	__dsp_xml_lib_table_free();
	dsp_dl_free(xml_str);
	dsp_dl_free(xml_libs);
	dsp_dl_free(token_value);
}

static int __get_next_token(unsigned short *type, char *buf,
	unsigned int buf_len)
{
	static int need_parse_token = 1;
	static int token_remained = 1;
	static int parse_end;
	static unsigned int idx;
	static unsigned int token_num;
	static struct dsp_sxml parser;
	static struct dsp_sxml_tok tokens[TOKEN_LIST_NUM];
	struct dsp_sxml_tok token;
	size_t token_value_size;

	if (buf == NULL) {
		need_parse_token = 1;
		token_remained = 1;
		parse_end = 0;
		idx = 0;
		token_num = 0;
		dsp_sxml_init(&parser);
		return 0;
	}

	if (idx == token_num) {
		if (token_remained)
			need_parse_token = 1;
		else
			parse_end = 1;
	}

	if (parse_end)
		return TOKEN_END;

	if (need_parse_token) {
		enum dsp_sxml_err err = dsp_sxml_parse(&parser, buf, buf_len,
				tokens, COUNT(tokens));

		DL_DEBUG("Parse num : %u\n", parser.ntokens);

		switch (err) {
		case SXML_SUCCESS:
			token_remained = 0;
			need_parse_token = 0;
			token_num = parser.ntokens;
			idx = 0;
			break;
		case SXML_ERROR_TOKENSFULL:
			token_remained = 1;
			need_parse_token = 0;
			token_num = parser.ntokens;
			idx = 0;

			parser.ntokens = 0;
			break;
		case SXML_ERROR_BUFFERDRY:
			DL_ERROR("Larger than buf size\n");
			return TOKEN_END;
		case SXML_ERROR_XMLINVALID:
			DL_ERROR("XML is invalid\n");
			return TOKEN_END;
		default:
			DL_ERROR("Error code is invalid\n");
			return TOKEN_END;
		}
	}

	token = tokens[idx++];
	token_value_size = token.endpos - token.startpos;
	if (token_value_size >= TOKEN_MAX) {
		DL_ERROR("Token_value_size is too big(%zd/%u)\n",
				token_value_size, TOKEN_MAX);
		return TOKEN_END;
	}

	memcpy(token_value, buf + token.startpos, token_value_size);
	token_value[token_value_size] = '\0';

	*type = token.type;
	DL_DEBUG("Token value : %s, type : %d\n", token_value, *type);

	return dsp_string_tree_get(xml_str, token_value);
}

static void __dsp_xml_kernel_table_init(struct dsp_xml_kernel_table *kt)
{
	kt->pre = NULL;
	kt->post = NULL;
	kt->exe = NULL;
}

static int __create_kernel_table(struct dsp_xml_lib *lib, char *buf,
	unsigned int buf_len)
{
	unsigned short type;
	int ret;
	unsigned int idx = 0xFFFFFFFF;
	struct dsp_xml_kernel_table kernel_table;
	int skip_token = 0;

	__dsp_xml_kernel_table_init(&kernel_table);

	do {
		if (!skip_token)
			ret = __get_next_token(&type, buf, buf_len);
		else
			skip_token = 0;

		if (ret == TOKEN_END) {
			DL_ERROR("token end before create xml structure\n");
			return -1;
		} else if (ret == KERNEL && type == SXML_ENDTAG) {
			if (!lib->kernels) {
				DL_ERROR("no memory for kernel tables\n");
				return -1;
			}

			if (idx == 0xFFFFFFFF) {
				DL_ERROR("no kernel idx\n");
				return -1;
			} else if (idx >= lib->kernel_cnt) {
				DL_ERROR("invalid kernel idx\n");
				return -1;
			}
			lib->kernels[idx] = kernel_table;
			return 0;
		} else if (ret == ID && type == SXML_CDATA) {
			ret = __get_next_token(&type, buf, buf_len);
			if (ret == TOKEN_END) {
				DL_ERROR("token end before create xml\n");
				return -1;
			} else if (type == SXML_CHARACTER) {
				ret = kstrtouint(token_value, 10, &idx);
				if (ret)
					DL_ERROR("Failed to change str(%d)\n",
						ret);

				DL_DEBUG("kernel idx : %d\n", idx);
			} else {
				DL_ERROR("id token is not character\n");
				return -1;
			}
		} else if (ret == PRE && type == SXML_CDATA) {
			ret = __get_next_token(&type, buf, buf_len);
			if (ret == TOKEN_END) {
				DL_ERROR("token end before create xml\n");
				return -1;
			} else if (type == SXML_CHARACTER) {
				kernel_table.pre =
					(char *)dsp_dl_malloc(
						strlen(token_value) + 1,
						"kernel pre");
				strcpy(kernel_table.pre, token_value);
				DL_DEBUG("kernel pre : %s\n", kernel_table.pre);
			} else {
				DL_DEBUG("No pre kernel\n");
				skip_token = 1;
			}
		} else if (ret == EXE && type == SXML_CDATA) {
			ret = __get_next_token(&type, buf, buf_len);
			if (ret == TOKEN_END) {
				DL_ERROR("token end before create xml\n");
				return -1;
			} else if (type == SXML_CHARACTER) {
				kernel_table.exe =
					(char *)dsp_dl_malloc(
						strlen(token_value) + 1,
						"kernel exe");
				strcpy(kernel_table.exe, token_value);
				DL_DEBUG("kernel exe : %s\n", kernel_table.exe);
			} else {
				DL_ERROR("exe token is not character\n");
				return -1;
			}
		} else if (ret == POST && type == SXML_CDATA) {
			ret = __get_next_token(&type, buf, buf_len);
			if (ret == TOKEN_END) {
				DL_ERROR("token end before create xml\n");
				return -1;
			} else if (type == SXML_CHARACTER) {
				kernel_table.post =
					(char *)dsp_dl_malloc(
						strlen(token_value) + 1,
						"kernel post");
				strcpy(kernel_table.post, token_value);
				DL_DEBUG("kernel post : %s\n",
					kernel_table.post);
			} else {
				DL_DEBUG("No post kernel\n");
				skip_token = 1;
			}
		}
	} while (1);
}

static int __create_lib(char *buf, unsigned int buf_len)
{
	unsigned short type;
	int ret;
	struct dsp_xml_lib *lib =
		(struct dsp_xml_lib *)dsp_dl_malloc(
			sizeof(*lib), "XML library");

	lib->kernels = NULL;

	do {
		ret = __get_next_token(&type, buf, buf_len);
		if (ret == TOKEN_END) {
			if (lib->kernels)
				dsp_dl_free(lib->kernels);

			dsp_dl_free(lib);
			DL_ERROR("token end before create xml structure\n");
			return -1;
		} else if (ret == LIB && type == SXML_ENDTAG
				&& lib->name != NULL) {
			dsp_hash_push(&xml_libs->lib_hash, lib->name, lib);
			return 0;
		} else if (ret == NAME && type == SXML_CDATA) {
			ret = __get_next_token(&type, buf, buf_len);
			if (ret == TOKEN_END) {
				if (lib->kernels)
					dsp_dl_free(lib->kernels);

				dsp_dl_free(lib);
				DL_ERROR("token end before create xml\n");
				return -1;
			} else if (type == SXML_CHARACTER) {

				//TODO: Roll back tmp lib path gen code
				token_value[strlen(token_value) - 6] = '\0';

				lib->name = (char *)dsp_dl_malloc(
						strlen(token_value) + 1,
						"lib name");
				strcpy(lib->name, token_value);
				DL_DEBUG("lib name : %s\n", lib->name);
			} else {
				if (lib->kernels)
					dsp_dl_free(lib->kernels);

				dsp_dl_free(lib);
				DL_ERROR("name token is not character\n");
				return -1;
			}
		} else if (ret == KERNEL_COUNT && type == SXML_CDATA) {
			ret = __get_next_token(&type, buf, buf_len);
			if (ret == TOKEN_END) {
				if (lib->kernels)
					dsp_dl_free(lib->kernels);

				dsp_dl_free(lib);
				DL_ERROR("token end before create xml\n");
				return -1;
			} else if (type == SXML_CHARACTER) {
				ret = kstrtouint(token_value, 10,
						&lib->kernel_cnt);
				if (ret) {
					DL_ERROR("Failed to change str(%d)\n",
						ret);
					dsp_dl_free(lib);
					return -1;
				}

				if (!lib->kernel_cnt) {
					DL_ERROR("invalid kernel cnt\n");
					dsp_dl_free(lib);
					return -1;
				}
				DL_DEBUG("lib kernel cnt : %u\n",
					lib->kernel_cnt);

				if (lib->kernel_cnt > KERNEL_CNT_MAX) {
					DL_ERROR("kernel_cnt(%u) is over(%d)\n",
							lib->kernel_cnt,
							KERNEL_CNT_MAX);
					dsp_dl_free(lib);
					return -1;
				}

				lib->kernels = (struct dsp_xml_kernel_table *)
					dsp_dl_malloc(
						sizeof(*lib->kernels) *
						lib->kernel_cnt,
						"XML kernel table");
				if (!lib->kernels) {
					DL_ERROR("Failed to alloc XML kt\n");
					dsp_dl_free(lib);
					return -1;
				}
			} else {
				if (lib->kernels)
					dsp_dl_free(lib->kernels);

				dsp_dl_free(lib);
				DL_ERROR("kernel cnt token is not character\n");
				return -1;
			}
		} else if (ret == KERNEL && type == SXML_STARTTAG) {
			if (__create_kernel_table(lib, buf, buf_len) == -1) {
				if (lib->kernels)
					dsp_dl_free(lib->kernels);

				dsp_dl_free(lib);
				return -1;
			}
		}
	} while (1);
}

static int __create_lib_table(char *buf, unsigned int buf_len)
{
	unsigned short type;
	int ret;

	do {
		ret = __get_next_token(&type, buf, buf_len);
		if (ret == TOKEN_END) {
			DL_ERROR("token end before create xml structure\n");
			return -1;
		} else if (ret == LIBS && type == SXML_ENDTAG)
			return 0;

		else if (ret == COUNT && type == SXML_CDATA) {
			ret = __get_next_token(&type, buf, buf_len);
			if (ret == TOKEN_END) {
				DL_ERROR("token end before create xml\n");
				return -1;
			} else if (type == SXML_CHARACTER) {
				ret = kstrtouint(token_value, 10,
						&xml_libs->lib_cnt);
				if (ret)
					DL_ERROR("Failed to change str(%d)\n",
						ret);

				DL_DEBUG("libs count : %u\n",
					xml_libs->lib_cnt);
			} else {
				DL_ERROR("count token is not character\n");
				return -1;
			}
		} else if (ret == LIB && type == SXML_STARTTAG) {
			if (__create_lib(buf, buf_len) == -1)
				return -1;
		}
	} while (1);
}

static int __create_xml_structure(char *buf, unsigned int buf_len)
{
	unsigned short type;
	int ret;

	ret = __get_next_token(NULL, NULL, 0);
	if (ret != 0) {
		DL_ERROR("Failed to init token\n");
		return -1;
	}

	do {
		ret = __get_next_token(&type, buf, buf_len);
		if (ret == TOKEN_END) {
			DL_ERROR("token end before create xml structure\n");
			return -1;
		} else if (ret == LIBS && type == SXML_STARTTAG)
			return __create_lib_table(buf, buf_len);
	} while (1);
}

int dsp_xml_parser_parse(struct dsp_dl_lib_file *file)
{
	dsp_dl_lib_file_reset(file);
	return __create_xml_structure((char *)file->mem, file->size);
}
