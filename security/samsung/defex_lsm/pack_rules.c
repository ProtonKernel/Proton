/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "include/defex_rules.h"

#define SAFE_STRCOPY(dst, src) do { strncpy(dst, src, sizeof(dst)); dst[sizeof(dst) - 1] = 0; } while (0)

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif /* ARRAY_SIZE */

const struct feature_match_entry feature_match[] = {
	{"feature_safeplace_path", feature_safeplace_path},
	{"feature_ped_exception", feature_ped_exception},
	{"feature_immutable_path_open", feature_immutable_path_open},
	{"feature_immutable_path_write", feature_immutable_path_write},
	{"feature_immutable_src_exception", feature_immutable_src_exception},
	{"feature_immutable_dst_exception", feature_immutable_dst_exception},
	{"feature_umhbin_path", feature_umhbin_path},
	{"feature_integrity_check", feature_integrity_check},
};

struct file_list_item {
	char file_name[PATH_MAX];
#ifdef DEFEX_INTEGRITY_ENABLE
	char integrity[INTEGRITY_LENGTH * 2 + 1];
#endif /* DEFEX_INTEGRITY_ENABLE */
	int is_recovery;
};

struct rule_item_struct *defex_packed_rules;
int packfiles_count, packfiles_size;

struct file_list_item *file_list = NULL;
int file_list_count = 0;

#ifndef DEFEX_DEBUG_ENABLE
int debug_ifdef_is_active = 0;
void process_debug_ifdef(const char *src_str);
#endif

/* Show rules vars */
const char header_name[16] = {"DEFEX_RULES_FILE"};
static char work_path[512];
static int global_data_size;

/* Suplementary functions for packing rules */
struct rule_item_struct *create_file_item(const char *name, int l);
struct rule_item_struct *add_file_item(struct rule_item_struct *base, const char *name, int l);
struct rule_item_struct *lookup_dir(struct rule_item_struct *base, const char *name, int l, int for_recovery);
struct rule_item_struct *add_file_path(const char *file_path, int for_recovery);
void add_rule(const char *part1, const char *part2, int for_recovery,
		enum feature_types feature, const char *integrity);
void addline2tree(char *src_line, enum feature_types feature);
char *extract_rule_text(const char *src_line);
int lookup_tree(const char *file_path, int attribute, int for_recovery);
int store_tree(FILE *f, FILE *f_bin);

#ifdef DEFEX_INTEGRITY_ENABLE
/* Transfer string to hex */
char null_integrity[INTEGRITY_LENGTH * 2 + 1];
unsigned char ascii_to_hex(char input);
int string_to_hex(const char *input, size_t inputLen, unsigned char *output);
#endif /* DEFEX_INTEGRITY_ENABLE */

/* Suplementary functions for reducing rules */
int str_to_feature(const char *str);
int remove_substr(char *str, const char *part);
void trim_cr_lf(char *str);
char* remove_redundant_chars(char *str);
int check_path_in_use(const char *path);
int load_file_list(const char *name);
int lookup_file_list(const char *rule, int for_recovery);

/* Suplementary functions for showing rules */
void feature_to_str(char *str, unsigned short flags);
int check_array_size(struct rule_item_struct *ptr);
int parse_items(struct rule_item_struct *base, int path_length, int level);
int defex_show_structure(void *packed_rules, int rules_size);

/* Main processing functions */
int reduce_rules(const char *source_rules_file, const char *reduced_rules_file, const char *list_file);
int pack_rules(const char *source_rules_file, const char *packed_rules_file, const char *packed_rules_binfile);
int parse_packed_bin_file(const char *source_bin_file);

int str_to_feature(const char *str)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(feature_match); i++) {
		if (strstr(str, feature_match[i].feature_name))
			return feature_match[i].feature_num;
	}
	return 0;
}

void feature_to_str(char *str, unsigned short flags)
{
	int i;

	str[0] = 0;
	for (i = 0; i < ARRAY_SIZE(feature_match); i++)
		if (flags & feature_match[i].feature_num) {
			if (str[0])
				strcat(str, ", ");
			strcat(str, feature_match[i].feature_name);
		}
	if (flags & feature_for_recovery) {
		if (str[0])
			strcat(str, ", ");
		strcat(str, "feature_for_recovery");
	}
}

int remove_substr(char *str, const char *part)
{
	int l, part_l, found = 0;
	char *ptr;

	l = strnlen(str, PATH_MAX - 1);
	ptr = strstr(str, part);
	if (ptr) {
		part_l = strnlen(part, 32) - 1;
		memmove(ptr, ptr + part_l, l - (ptr - str) - part_l + 1);
		found = 1;
	}
	return found;
}

void trim_cr_lf(char *str)
{
	char *ptr;
	/* remove CR or LF at the end */
	ptr = strchr(str, '\r');
	if (ptr)
		*ptr = 0;
	ptr = strchr(str, '\n');
	if (ptr)
		*ptr = 0;
}

char *remove_redundant_chars(char *str)
{
	int l;

	/* skip hash values in the begin */
	str += 65;
	trim_cr_lf(str);
	l = strnlen(str, PATH_MAX - 1);
	/* remove starting dot or space */
	while (l && (*str == '.' || *str == ' '))
		str++;
	return str;
}

int check_path_in_use(const char *path)
{
	int i;
	static const char * const path_list[] = {
		"/root/",
		"/product",
		"/recovery/",
		"/system/",
		"/tmp/",
		"/vendor/",
#if defined(DEFEX_FACTORY_ENABLE)
		"/data/",
#endif
		"/apex/",
		"/system_ext/",
		"/postinstall/"
	};

	for (i = 0; i < ARRAY_SIZE(path_list); i++) {
		if (!strncmp(path, path_list[i], strnlen(path_list[i], PATH_MAX)))
			return 1;
	}
	return 0;
}

struct rule_item_struct *create_file_item(const char *name, int l)
{
	struct rule_item_struct *item;
	unsigned int offset;

	if (!name)
		l = 0;
	offset = packfiles_size;
	packfiles_size += (sizeof(struct rule_item_struct) + l);
	packfiles_count++;
	item = GET_ITEM_PTR(offset, defex_packed_rules);
	item->next_file = 0;
	item->next_level = 0;
	item->feature_type = 0;
	item->size = l;
#ifdef DEFEX_INTEGRITY_ENABLE
	memset(item->integrity, 0, INTEGRITY_LENGTH);
#endif /* DEFEX_INTEGRITY_ENABLE */
	if (l)
		memcpy(item->name, name, l);
	return item;
}

struct rule_item_struct *add_file_item(struct rule_item_struct *base, const char *name, int l)
{
	struct rule_item_struct *item, *new_item = NULL;

	if (!base)
		return new_item;

	new_item = create_file_item(name, l);
	if (!base->next_level) {
		base->next_level = GET_ITEM_OFFSET(new_item, defex_packed_rules);
	} else {
		item = GET_ITEM_PTR(base->next_level, defex_packed_rules);
		while (item->next_file)
			item = GET_ITEM_PTR(item->next_file, defex_packed_rules);
		item->next_file = GET_ITEM_OFFSET(new_item, defex_packed_rules);
	}
	return new_item;
}

struct rule_item_struct *lookup_dir(struct rule_item_struct *base, const char *name, int l, int for_recovery)
{
	struct rule_item_struct *item = NULL;
	unsigned int offset;

	if (!base || !base->next_level)
		return item;
	item = GET_ITEM_PTR(base->next_level, defex_packed_rules);
	do {
		if ((!(item->feature_type & feature_is_file)
			|| (!!(item->feature_type & feature_for_recovery)) == for_recovery)
			&& item->size == l
			&& !memcmp(name, item->name, l)) return item;
		offset = item->next_file;
		item = GET_ITEM_PTR(offset, defex_packed_rules);
	} while (offset);
	return NULL;
}

struct rule_item_struct *add_file_path(const char *file_path, int for_recovery)
{
	const char *ptr, *next_separator;
	struct rule_item_struct *base, *cur_item = NULL;
	int l;

	if (!file_path || *file_path != '/')
		return NULL;
	if (!defex_packed_rules) {
		packfiles_count = 0;
		packfiles_size = 0;
		defex_packed_rules = calloc(sizeof(struct rule_item_struct),  100 * 1024);
		if (!defex_packed_rules) {
			printf("WARNING: Can not create the new item!\n");
			exit(-1);
		}
		create_file_item(header_name, sizeof(header_name));
	}
	base = defex_packed_rules;
	ptr = file_path + 1;
	do {
		next_separator = strchr(ptr, '/');
		if (!next_separator)
			l = strlen(ptr);
		else
			l = next_separator - ptr;
		if (!l)
			return NULL; /* two slashes in sequence */
		cur_item = lookup_dir(base, ptr, l, for_recovery);
		if (!cur_item) {
			cur_item = add_file_item(base, ptr, l);
			/* slash wasn't found, it's a file */
			if (!next_separator) {
				cur_item->feature_type |= feature_is_file;
				if (for_recovery)
					cur_item->feature_type |= feature_for_recovery;
			}
		}
		base = cur_item;
		ptr += l;
		if (next_separator)
			ptr++;
	} while (*ptr);
	return cur_item;
}

int lookup_tree(const char *file_path, int attribute, int for_recovery)
{
	const char *ptr, *next_separator;
	struct rule_item_struct *base, *cur_item = NULL;
	int l;

	if (!file_path || *file_path != '/' || !defex_packed_rules)
		return 0;
	base = defex_packed_rules;
	ptr = file_path + 1;
	do {
		next_separator = strchr(ptr, '/');
		if (!next_separator)
			l = strlen(ptr);
		else
			l = next_separator - ptr;
		if (!l)
			return 0;
		cur_item = lookup_dir(base, ptr, l, for_recovery);
		if (!cur_item)
			break;
		if (cur_item->feature_type & attribute)
			return 1;
		base = cur_item;
		ptr += l;
		if (next_separator)
			ptr++;
	} while (*ptr);
	return 0;
}

char *extract_rule_text(const char *src_line)
{
	char *start_ptr, *end_ptr;

	start_ptr = strchr(src_line, '\"');
	if (start_ptr) {
		start_ptr++;
		end_ptr = strchr(start_ptr, '\"');
		if (end_ptr) {
			*end_ptr = 0;
			return start_ptr;
		}
	}
	return NULL;
}

#ifdef DEFEX_INTEGRITY_ENABLE
unsigned char ascii_to_hex(char input)
{
	if (input >= 0x30 && input <= 0x39)
		return (unsigned char)input - 0x30;
	else if (input >= 0x41 && input <= 0x46)
		return (unsigned char)input - 0x37;
	else if (input >= 0x61 && input <= 0x66)
		return (unsigned char)input - 0x57;
	return 0xFF;
}

int string_to_hex(const char *input, size_t inputLen, unsigned char *output)
{
	unsigned char convert1, convert2;
	size_t i;

	if (input == NULL || output == NULL)
		return 0;

	/* Check input is a paired value. */
	if (inputLen % 2 != 0)
		return 0;

	/* divide length by 2 */
	inputLen >>= 1;
	memset(output, 0, inputLen);

	/* Convert ascii code to hexa. */
	for (i = 0; i < inputLen; i++) {
		convert1 = ascii_to_hex(input[2*i]);
		convert2 = ascii_to_hex(input[2*i+1]);

		if (convert1 == 0xFF || convert2 == 0xFF)
			return 0;

		output[i] = (char)((convert1 << 4) | convert2);
	}
	return 1;
}
#endif /* DEFEX_INTEGRITY_ENABLE */

void add_rule(const char *part1, const char *part2, int for_recovery, enum feature_types feature, const char *integrity)
{
	struct rule_item_struct *item_part1 = NULL, *item_part2;

	(void)integrity;
	if (part1) {
		item_part1 = add_file_path(part1, for_recovery);
		if (item_part1) {
			item_part1->feature_type |= feature;
			if (part2 && item_part1->feature_type & feature_is_file) {
				item_part2 = add_file_path(part2, 0);
				if (item_part2)
					item_part2->feature_type |= feature_immutable_dst_exception;

				/* add the reference from part1 to part2 */
				if (item_part2)
					item_part1->next_level = GET_ITEM_OFFSET(item_part2, defex_packed_rules);
			}
#ifdef DEFEX_INTEGRITY_ENABLE
			if (integrity)
				string_to_hex(integrity + 1, INTEGRITY_LENGTH * 2, item_part1->integrity);
#endif /* DEFEX_INTEGRITY_ENABLE */
		}
	}
}

void addline2tree(char *src_line, enum feature_types feature)
{
	char *part1, *part2;
	char *n_sign = NULL, *r_sign = NULL;
	char *integrity;

	(void)integrity;
	/* extract the first part */
	part1 = extract_rule_text(src_line);
	if (!part1)
		return;
	/* extract the second part after ':' symbol (immutable_dst_exception rule) */
	part2 = strchr(part1, ':');
	if (part2) {
		*part2 = 0;
		++part2;
	}

#ifdef DEFEX_INTEGRITY_ENABLE
	integrity = strchr((part2) ? part2 : part1, '\0') + 1;
	integrity = extract_rule_text(integrity);
	if (integrity) {
		n_sign = strchr(integrity, 'N');
		r_sign = strchr(integrity, 'R');
	}
#endif /* DEFEX_INTEGRITY_ENABLE */

	if (n_sign || !r_sign)
		add_rule(part1, part2, 0, feature, n_sign);

	if (r_sign)
		add_rule(part1, part2, 1, feature, r_sign);
}

int store_tree(FILE *f, FILE *f_bin)
{
	unsigned char *ptr = (unsigned char *)defex_packed_rules;
	static char work_str[4096];
	int i, offset = 0, index = 0;

	if (packfiles_size)
		defex_packed_rules->data_size = packfiles_size;

	work_str[0] = 0;
	fprintf(f, "#ifndef DEFEX_RAMDISK_ENABLE\n\n");
	fprintf(f, "const unsigned char defex_packed_rules[] = {\n");
	for (i = 0; i < packfiles_size; i++) {
		if (index)
			offset += snprintf(work_str + offset, sizeof(work_str) - offset, ", ");
		offset += snprintf(work_str + offset, sizeof(work_str) - offset, "0x%02x", ptr[i]);
		index++;
		if (index == 16) {
			fprintf(f, "\t%s,\n", work_str);
			index = 0;
			offset = 0;
		}
	}
	if (index)
		fprintf(f, "\t%s\n", work_str);
	fprintf(f, "};\n");
	fprintf(f, "\n#endif /* DEFEX_RAMDISK_ENABLE */\n\n");
	fprintf(f, "#define DEFEX_RULES_ARRAY_SIZE\t\t%d\n", packfiles_size);
	if (f_bin)
		fwrite(defex_packed_rules, 1, packfiles_size, f_bin);
	return 0;
}

int load_file_list(const char *name)
{
	int found;
	char *str;
	FILE *lst_file = NULL;
	struct file_list_item *file_list_new;
	static char work_str[PATH_MAX*2];

	lst_file = fopen(name, "r");
	if (!lst_file)
		return -1;

	while (!feof(lst_file)) {
		if (!fgets(work_str, sizeof(work_str), lst_file))
			break;
		str = remove_redundant_chars(work_str);
		if (*str == '/' && check_path_in_use(str)) {
			remove_substr(str, "/root/");
			found = remove_substr(str, "/recovery/");
			file_list_count++;
			file_list_new = realloc(file_list, sizeof(struct file_list_item) * file_list_count);
			if (!file_list_new) {
				free(file_list);
				printf("WARNING: Can not allocate the filelist item!\n");
				exit(-1);
			}
			file_list = file_list_new;
#ifdef DEFEX_INTEGRITY_ENABLE
			strncpy(file_list[file_list_count - 1].integrity, work_str, INTEGRITY_LENGTH * 2);
			file_list[file_list_count - 1].integrity[INTEGRITY_LENGTH * 2] = 0;
#endif /* DEFEX_INTEGRITY_ENABLE */
			SAFE_STRCOPY(file_list[file_list_count - 1].file_name, str);
			file_list[file_list_count - 1].is_recovery = found;
		}
	}
	fclose(lst_file);
	return 0;
}

int lookup_file_list(const char *rule, int for_recovery)
{
	int i;

	for (i = 0; i < file_list_count; i++) {
		if (file_list[i].is_recovery == for_recovery
			&& !strncmp(file_list[i].file_name, rule, strnlen(rule, PATH_MAX))
			&& !strncmp(file_list[i].file_name, rule, strnlen(file_list[i].file_name, PATH_MAX)))
			return i+1;
	}
	return 0;
}

#ifndef DEFEX_DEBUG_ENABLE
void process_debug_ifdef(const char *src_str)
{
	char *ptr;
	ptr = strstr(src_str, "#ifdef DEFEX_DEBUG_ENABLE");
	if (ptr) {
		while (ptr > src_str) {
			ptr--;
			if (*ptr != ' ' && *ptr != '\t')
				return;
		}
		debug_ifdef_is_active = 1;
		return;
	}
	ptr = strstr(src_str, "#endif");
	if (ptr && debug_ifdef_is_active) {
		while (ptr > src_str) {
			ptr--;
			if (*ptr != ' ' && *ptr != '\t')
				return;
		}
		debug_ifdef_is_active = 0;
		return;
	}
}
#endif

int reduce_rules(const char *source_rules_file, const char *reduced_rules_file, const char *list_file)
{
	int ret_val = -1;
	int found_normal = 0, found_recovery = 0;
	char *rule, *colon_ptr;
	static char work_str[PATH_MAX*2], tmp_str[PATH_MAX*2], rule_part1[PATH_MAX*2];
	FILE *src_file = NULL, *dst_file = NULL;
#ifdef DEFEX_INTEGRITY_ENABLE
	char *line_end, *integrity_normal;
	char *integrity_recovery;
#endif /* DEFEX_INTEGRITY_ENABLE */

	src_file = fopen(source_rules_file, "r");
	if (!src_file)
		return -1;
	dst_file = fopen(reduced_rules_file, "wt");
	if (!dst_file)
		goto do_close2;

	if (load_file_list(list_file) != 0)
		goto do_close1;

#ifdef DEFEX_INTEGRITY_ENABLE
	memset(null_integrity, '0', sizeof(null_integrity) - 1);
	null_integrity[sizeof(null_integrity) - 1] = 0;
#endif /* DEFEX_INTEGRITY_ENABLE */

	while (!feof(src_file)) {
		if (!fgets(work_str, sizeof(work_str), src_file))
			break;

		if (str_to_feature(work_str)) {
			trim_cr_lf(work_str);
			SAFE_STRCOPY(tmp_str, work_str);
			rule = extract_rule_text(tmp_str);
			SAFE_STRCOPY(rule_part1, rule);
			colon_ptr = strchr(rule_part1, ':');
			if (colon_ptr)
				*colon_ptr = 0;
			found_normal = lookup_file_list(rule_part1, 0);
			found_recovery = lookup_file_list(rule_part1, 1);
			if (rule && !found_normal && !found_recovery && !strstr(work_str, "/* DEFAULT */")) {
				printf("removed rule: %s\n", rule);
				continue;
			}
#ifdef DEFEX_INTEGRITY_ENABLE
			integrity_normal = null_integrity;
			integrity_recovery = null_integrity;
			if (found_normal)
				integrity_normal = file_list[found_normal - 1].integrity;
			if (found_recovery)
				integrity_recovery = file_list[found_recovery - 1].integrity;

			line_end = strstr(work_str, "},");
			if (line_end) {
				*line_end = 0;
				line_end += 2;
			}

			/* Add hash vale after each file path */
			if (found_normal || (!found_normal && !found_recovery))
				printf("remained rule: %s, %s %s\n",
					rule, integrity_normal, (line_end != NULL)?line_end:"");
			if (found_recovery)
				printf("remained rule: %s, %s %s (R)\n",
					rule, integrity_recovery, (line_end != NULL)?line_end:"");

			fprintf(dst_file, "%s,\"", work_str);
			if (found_normal)
				fprintf(dst_file, "N%s", integrity_normal);
			if (found_recovery)
				fprintf(dst_file, "R%s", integrity_recovery);
			
			fprintf(dst_file, "\"}, %s\n", (line_end != NULL)?line_end:"");

#else
			printf("remained rule: %s\n", work_str);
			fputs(work_str, dst_file);
			fputs("\n", dst_file);
#endif /* DEFEX_INTEGRITY_ENABLE */
		} else
			fputs(work_str, dst_file);
	}
	ret_val = 0;
do_close1:
	fclose(dst_file);
do_close2:
	fclose(src_file);
	return ret_val;
}

int pack_rules(const char *source_rules_file, const char *packed_rules_file, const char *packed_rules_binfile)
{
	int ret_val = -1;
	int feature;
	FILE *src_file = NULL, *dst_file = NULL, *dst_binfile = NULL;
	static char work_str[PATH_MAX*2];

	src_file = fopen(source_rules_file, "r");
	if (!src_file) {
		printf("Failed to open %s, %s\n", source_rules_file, strerror(errno));
		return -1;
	}
	dst_file = fopen(packed_rules_file, "wt");
	if (!dst_file) {
		printf("Failed to open %s, %s\n", packed_rules_file, strerror(errno));
		goto do_close2;
	}
	if (packed_rules_binfile) {
		dst_binfile = fopen(packed_rules_binfile, "wt");
		if (!dst_binfile)
			printf("Failed to open %s, %s - Ignore\n", packed_rules_binfile, strerror(errno));
	}

	while (!feof(src_file)) {
		if (!fgets(work_str, sizeof(work_str), src_file))
			break;
#ifndef DEFEX_DEBUG_ENABLE
		process_debug_ifdef(work_str);
		if (!debug_ifdef_is_active) {
#endif
		feature = str_to_feature(work_str);
		if (feature) {
			addline2tree(work_str, feature);
			continue;
		}
#ifndef DEFEX_DEBUG_ENABLE
		}
#endif
	}
	store_tree(dst_file, dst_binfile);
	if (!packfiles_count)
		printf("WARNING: Defex packed rules tree is empty!\n");
	ret_val = 0;
	if (dst_binfile)
		fclose(dst_binfile);
	fclose(dst_file);
do_close2:
	fclose(src_file);
	return ret_val;
}

int check_array_size(struct rule_item_struct *ptr)
{
	unsigned long offset = (unsigned long)ptr - (unsigned long)defex_packed_rules;
	int min_size = (global_data_size < packfiles_size)?global_data_size:packfiles_size;

	offset += sizeof(struct rule_item_struct);

	if (offset > min_size)
		return 1;

	offset += ptr->size;
	if (offset > min_size)
		return 2;
	return 0;
}

int parse_items(struct rule_item_struct *base, int path_length, int level)
{
	int l, err, ret = 0, is_rule_part2;
	unsigned int offset;
	struct rule_item_struct *child_item;
	static char feature_list[128];

	if (level > 8) {
		printf("Level is too deep\n");
		return -1;

	}
	if (path_length > (sizeof(work_path) - 128)) {
		printf("Work path is too long\n");
		return -1;
	}
	while (base) {
		err = check_array_size(base);
		if (err) {
			printf("%s/<?> - out of array bounds\n", work_path);
			return -1;
		}
		l = base->size;
		if (!l) {
			printf("WARNING: Name field is incorrect, structure error!\n");
			return -1;

		}

		memcpy(work_path + path_length, base->name, l);
		l += path_length;
		work_path[l] = 0;
		offset = base->next_level;
		/* check whether the rule has the second part (immutable_dst_exception) */
		is_rule_part2 = (offset && (base->feature_type & feature_is_file) &&
				(base->feature_type & feature_immutable_src_exception));

		if (offset && !is_rule_part2) {
			if (base->feature_type & feature_is_file) {
				printf("%s - is a file, but has children, structure error!\n", work_path);
				ret = -1;
			} else if (base->feature_type != 0) {
				feature_to_str(feature_list, base->feature_type);
				printf("%s%c - %s\n", work_path,
					((base->feature_type & feature_is_file)?' ':'/'), feature_list);
			}
			child_item = GET_ITEM_PTR(offset, defex_packed_rules);
			work_path[l++] = '/';
			work_path[l] = 0;
			err = check_array_size(child_item);
			if (!err) {
				err = parse_items(child_item, l, level + 1);
				if (err != 0)
					return err;
			} else {
				printf("%s/<?> - out of array bounds\n", work_path);
				ret = -1;
			}
		} else {
			feature_to_str(feature_list, base->feature_type);
			printf("%s%c%s - %s\n", work_path,
				((base->feature_type & feature_is_file)?' ':'/'),
				(is_rule_part2)?":<SECOND PART>":"", feature_list);
		}
		work_path[path_length] = 0;
		offset = base->next_file;
		base = (offset)?GET_ITEM_PTR(offset, defex_packed_rules):NULL;
	}
	return ret;
}

int defex_show_structure(void *packed_rules, int rules_size)
{
	struct rule_item_struct *base;
	int res, offset;
	int first_item_size = sizeof(struct rule_item_struct) + sizeof(header_name);

	defex_packed_rules = (struct rule_item_struct *)packed_rules;

	work_path[0] = '/';
	work_path[1] = 0;

	packfiles_size = rules_size;
	global_data_size = defex_packed_rules->data_size;

	printf("Rules binary size: %d\n", packfiles_size);
	printf("Rules internal data size: %d\n", global_data_size);

	if (global_data_size > packfiles_size)
		printf("WARNING: Internal size is bigger than binary size, possible structure error!\n");

	if (packfiles_size < first_item_size) {
		printf("ERROR: Too short binary size, can't continue!\n");
		return -1;
	}

	if (global_data_size < first_item_size)
		printf("WARNING: Too short data size, possible structure error!\n");

	if ((int)(defex_packed_rules->size) != sizeof(header_name))
		printf("WARNING: incorrect size field (%d), possible structure error!\n",
			(int)defex_packed_rules->size);


	if (memcmp(header_name, defex_packed_rules->name, sizeof(header_name)) != 0)
		printf("WARNING: incorrect name field, possible structure error!\n");

	printf("File List:\n");
	offset = defex_packed_rules->next_level;
	base = (offset)?GET_ITEM_PTR(offset, defex_packed_rules):NULL;

	if ((long unsigned int)base < (long unsigned int)packed_rules ||
			(long unsigned int)base > (long unsigned int)base + rules_size) {
		printf("- empty list\n");
		return 0;
	} else if (check_array_size(base)) {
		printf("- list is out of array bounds!\n");
		return -1;
	}

	res = parse_items(base, 1, 1);
	printf("== End of File List ==\n");
	return res;
}

int parse_packed_bin_file(const char *source_bin_file)
{
	struct stat sb;
	FILE *policy_file = NULL;
	int policy_size;
	unsigned char *policy_data = NULL;

	if (stat(source_bin_file, &sb) == -1) {
		perror("Error");
		return -1;
	}

	policy_size = sb.st_size;

	printf("Try to parse file: %s\n", source_bin_file);

	policy_file = fopen(source_bin_file, "r");
	if (policy_file == NULL) {
		perror("Error");
		return -1;
	}

	policy_data = malloc(policy_size);

	if (policy_data == NULL) {
		perror("Error");
		goto exit;
	}

	if ((fread(policy_data, policy_size, 1, policy_file)) != 1) {
		perror("Error");
		goto exit;
	}

	defex_show_structure((void *)policy_data, policy_size);

exit:
	free(policy_data);
	fclose(policy_file);
	return 0;
}

int main(int argc, char **argv)
{
	static char param[4][PATH_MAX];
	char *src_file = NULL, *packed_file = NULL, *packed_bin_file = NULL;
	char *reduced_file = NULL, *list_file = NULL;
	int i;

	if (argc == 3) {
		if (!strncmp(argv[1], "-s", 2)) {
			SAFE_STRCOPY(param[0], argv[2]);
			src_file = param[0];
			parse_packed_bin_file(src_file);
			return 0;
		}
	}

	if (argc < 4 || argc > 5) {
		printf("Invalid number of arguments\n");
		goto show_help;
	}

	for (i = 0; i < (argc - 2); i++) {
		SAFE_STRCOPY(param[i], argv[i + 2]);
		switch(i) {
			case 0:
				src_file = param[i];
				break;
			case 1:
				packed_file = reduced_file = param[i];
				break;
			case 2:
				packed_bin_file = list_file = param[i];
				break;
		}
	}

	if (!strncmp(argv[1], "-p", 2)) {
		if (pack_rules(src_file, packed_file, packed_bin_file) != 0)
			goto show_help;
		return 0;

	} else if (!strncmp(argv[1], "-r", 2) && list_file) {
		if (reduce_rules(src_file, reduced_file, list_file) != 0)
			goto show_help;
		return 0;
	}
	printf("Invalid command\n");

show_help:
	printf("Defex rules processing utility.\nUSAGE:\n%s <CMD> <PARAMS>\n"
		"Commands:\n"
		"  -p - Pack rules file to the tree. Params: <SOURCE_FILE> <PACKED_FILE> [PACKED_BIN_FILE]\n"
		"  -r - Reduce rules file (remove unexistent files). Params: <SOURCE_FILE> <REDUCED_FILE> <FILE_LIST>\n"
		"  -s - Show rules binary file content. Params: <PACKED_BIN_FILE>\n",
		argv[0]);
	return -1;
}
