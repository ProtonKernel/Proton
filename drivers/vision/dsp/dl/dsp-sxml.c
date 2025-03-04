// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/string.h>

#include "dl/dsp-sxml.h"

static const char *str_findchr(const char *start, const char *end, int c)
{
	const char *it;

	it = (const char *)memchr(start, c, end - start);
	return (it != NULL) ? it : end;
}

static const char *str_findstr(const char *start, const char *end,
	const char *needle)
{
	size_t needlelen;
	int first;

	needlelen = strlen(needle);
	first = (unsigned char)needle[0];

	while (start + needlelen <= end) {
		const char *it = (const char *)memchr(start, first,
				(end - start) - (needlelen - 1));

		if (it == NULL)
			break;

		if (memcmp(it, needle, needlelen) == 0)
			return it;

		start = it + 1;
	}

	return end;
}

static bool str_startswith(const char *start, const char *end,
	const char *prefix)
{
	long nbytes;

	nbytes = strlen(prefix);
	if (end - start < nbytes)
		return false;

	return memcmp(prefix, start, nbytes) == 0;
}

static bool __white_space(int c)
{
	switch (c) {
	case ' ':
	case '\t':
	case '\r':
	case '\n':
		return true;
	}

	return false;
}

static bool __name_start_char(int c)
{
	if (c >= 0x80)
		return true;

	return c == ':' || ('A' <= c && c <= 'Z') || c == '_' ||
		('a' <= c && c <= 'z');
}

static bool __name_char(int c)
{
	return __name_start_char(c) ||
		c == '-' || c == '.' || (c >= '0' && c <= '9') ||
		c == 0xB7 || (c >= 0x0300 && c <= 0x036F) ||
		(c >= 0x203F && c <= 0x2040);
}

#define ISSPACE(c)	(__white_space(((unsigned char)(c))))
#define ISALPHA(c)	(__name_start_char(((unsigned char)(c))))
#define ISALNUM(c)	(__name_char(((unsigned char)(c))))

static const char *str_ltrim(const char *start, const char *end)
{
	const char *it;

	for (it = start; it != end && ISSPACE(*it); it++)
		;

	return it;
}

static const char *str_rtrim(const char *start, const char *end)
{
	const char *it, *prev;

	for (it = end; start != it; it = prev) {
		prev = it - 1;

		if (!ISSPACE(*prev))
			return it;
	}

	return start;
}

static const char *str_find_notalnum(const char *start, const char *end)
{
	const char *it;

	for (it = start; it != end && ISALNUM(*it); it++)
		;

	return it;
}

struct dsp_sxml_args {
	const char *buffer;
	unsigned int bufferlen;
	struct dsp_sxml_tok *tokens;
	unsigned int num_tokens;
};

#define buffer_fromoffset(args, idx)	((args)->buffer + (idx))
#define buffer_tooffset(args, ptr)	(unsigned int)((ptr) - (args)->buffer)
#define buffer_getend(args)		((args)->buffer + (args)->bufferlen)

static bool state_pushtoken(struct dsp_sxml *state, struct dsp_sxml_args *args,
	enum dsp_sxml_type type, const char *start, const char *end)
{
	struct dsp_sxml_tok *token;
	unsigned int idx = state->ntokens++;

	if (args->num_tokens < state->ntokens)
		return false;

	token = &args->tokens[idx];
	token->type = type;
	token->startpos = buffer_tooffset(args, start);
	token->endpos = buffer_tooffset(args, end);
	token->size = 0;

	switch (type) {
	case SXML_STARTTAG:
		state->taglevel++;
		break;
	case SXML_ENDTAG:
		state->taglevel--;
		break;
	default:
		break;
	}

	return true;
}

static enum dsp_sxml_err state_setpos(struct dsp_sxml *state,
	const struct dsp_sxml_args *args, const char *ptr)
{
	state->bufferpos = buffer_tooffset(args, ptr);
	return (state->ntokens <= args->num_tokens) ?
		SXML_SUCCESS : SXML_ERROR_TOKENSFULL;
}

#define state_commit(dest, src)	memcpy((dest), (src), sizeof(struct dsp_sxml))

#define SXML_ERROR_XMLSTRICT	SXML_ERROR_XMLINVALID

#define ENTITY_MAXLEN		(8)
#define MIN(a, b)		((a) < (b) ? (a) : (b))

static enum dsp_sxml_err parse_characters(struct dsp_sxml *state,
	struct dsp_sxml_args *args, const char *end)
{
	const char *start = buffer_fromoffset(args, state->bufferpos);
	const char *limit, *colon, *ampr = str_findchr(start, end, '&');

	if (ampr != start)
		state_pushtoken(state, args, SXML_CHARACTER, start, ampr);

	if (ampr == end)
		return state_setpos(state, args, ampr);

	limit = MIN(ampr + ENTITY_MAXLEN, end);
	colon = str_findchr(ampr, limit, ';');
	if (colon == limit)
		return (limit == end) ?
			SXML_ERROR_BUFFERDRY : SXML_ERROR_XMLINVALID;

	start = colon + 1;
	state_pushtoken(state, args, SXML_CHARACTER, ampr, start);
	return state_setpos(state, args, start);
}

static enum dsp_sxml_err parse_attrvalue(struct dsp_sxml *state,
	struct dsp_sxml_args *args, const char *end)
{
	while (buffer_fromoffset(args, state->bufferpos) != end) {
		enum dsp_sxml_err err = parse_characters(state, args, end);

		if (err != SXML_SUCCESS)
			return err;
	}

	return SXML_SUCCESS;
}

static enum dsp_sxml_err parse_attributes(struct dsp_sxml *state,
	struct dsp_sxml_args *args)
{
	const char *start = buffer_fromoffset(args, state->bufferpos);
	const char *end = buffer_getend(args);
	const char *name = str_ltrim(start, end);

	unsigned int ntokens = state->ntokens;

	while (name != end && ISALPHA(*name)) {
		const char *eq, *space, *quot, *value;
		enum dsp_sxml_err err;

		eq = str_findchr(name, end, '=');

		if (eq == end)
			return SXML_ERROR_BUFFERDRY;

		space = str_rtrim(name, eq);
		state_pushtoken(state, args, SXML_CDATA, name, space);

		quot = str_ltrim(eq + 1, end);

		if (quot == end)
			return SXML_ERROR_BUFFERDRY;
		else if (*quot != '\'' && *quot != '"')
			return SXML_ERROR_XMLINVALID;

		value = quot + 1;
		quot = str_findchr(value, end, *quot);

		if (quot == end)
			return SXML_ERROR_BUFFERDRY;

		state_setpos(state, args, value);
		err = parse_attrvalue(state, args, quot);

		if (err != SXML_SUCCESS)
			return err;

		/* --- */

		name = str_ltrim(quot + 1, end);
	}

	{
		struct dsp_sxml_tok *token = args->tokens + (ntokens - 1);

		token->size = (unsigned short)(state->ntokens - ntokens);
	}

	return state_setpos(state, args, name);
}

/* --- */

#define TAG_LEN(str)	(sizeof(str) - 1)
#define TAG_MINSIZE	(3)

static enum dsp_sxml_err parse_comment(struct dsp_sxml *state,
	struct dsp_sxml_args *args)
{
	static const char STARTTAG[] = "<!--";
	static const char ENDTAG[] = "-->";

	const char *dash;
	const char *start = buffer_fromoffset(args, state->bufferpos);
	const char *end = buffer_getend(args);

	if (end - start < (int)TAG_LEN(STARTTAG))
		return SXML_ERROR_BUFFERDRY;

	if (!str_startswith(start, end, STARTTAG))
		return SXML_ERROR_XMLINVALID;

	start += TAG_LEN(STARTTAG);
	dash = str_findstr(start, end, ENDTAG);
	if (dash == end)
		return SXML_ERROR_BUFFERDRY;

	state_pushtoken(state, args, SXML_COMMENT, start, dash);
	return state_setpos(state, args, dash + TAG_LEN(ENDTAG));
}

static enum dsp_sxml_err parse_instruction(struct dsp_sxml *state,
	struct dsp_sxml_args *args)
{
	static const char STARTTAG[] = "<?";
	static const char ENDTAG[] = "?>";

	enum dsp_sxml_err err;
	const char *quest, *space;
	const char *start = buffer_fromoffset(args, state->bufferpos);
	const char *end = buffer_getend(args);

	if (!str_startswith(start, end, STARTTAG))
		return SXML_ERROR_XMLINVALID;

	start += TAG_LEN(STARTTAG);
	space = str_find_notalnum(start, end);
	if (space == end)
		return SXML_ERROR_BUFFERDRY;

	state_pushtoken(state, args, SXML_INSTRUCTION, start, space);

	state_setpos(state, args, space);
	err = parse_attributes(state, args);
	if (err != SXML_SUCCESS)
		return err;

	quest = buffer_fromoffset(args, state->bufferpos);

	if (end - quest < (int)TAG_LEN(ENDTAG))
		return SXML_ERROR_BUFFERDRY;

	if (!str_startswith(quest, end, ENDTAG))
		return SXML_ERROR_XMLINVALID;

	return state_setpos(state, args, quest + TAG_LEN(ENDTAG));
}

static enum dsp_sxml_err parse_doctype(struct dsp_sxml *state,
	struct dsp_sxml_args *args)
{
	static const char STARTTAG[] = "<!DOCTYPE";
	static const char ENDTAG[] = "]>";

	const char *bracket;
	const char *start = buffer_fromoffset(args, state->bufferpos);
	const char *end = buffer_getend(args);

	if (end - start < (int)TAG_LEN(STARTTAG))
		return SXML_ERROR_BUFFERDRY;

	if (!str_startswith(start, end, STARTTAG))
		return SXML_ERROR_BUFFERDRY;

	start += TAG_LEN(STARTTAG);
	bracket = str_findstr(start, end, ENDTAG);

	if (bracket == end)
		return SXML_ERROR_BUFFERDRY;

	state_pushtoken(state, args, SXML_DOCTYPE, start, bracket);
	return state_setpos(state, args, bracket + TAG_LEN(ENDTAG));
}

static enum dsp_sxml_err parse_start(struct dsp_sxml *state,
	struct dsp_sxml_args *args)
{
	enum dsp_sxml_err err;
	const char *gt, *name, *space;
	const char *start = buffer_fromoffset(args, state->bufferpos);
	const char *end = buffer_getend(args);

	if (!(start[0] == '<' && ISALPHA(start[1])))
		return SXML_ERROR_XMLINVALID;

	/* --- */

	name = start + 1;
	space = str_find_notalnum(name, end);
	if (space == end)
		return SXML_ERROR_BUFFERDRY;

	state_pushtoken(state, args, SXML_STARTTAG, name, space);

	state_setpos(state, args, space);
	err = parse_attributes(state, args);
	if (err != SXML_SUCCESS)
		return err;

	/* --- */

	gt = buffer_fromoffset(args, state->bufferpos);

	if (gt != end && *gt == '/') {
		state_pushtoken(state, args, SXML_ENDTAG, name, space);
		gt++;
	}

	if (gt == end)
		return SXML_ERROR_BUFFERDRY;

	if (*gt != '>')
		return SXML_ERROR_XMLINVALID;

	return state_setpos(state, args, gt + 1);
}

static enum dsp_sxml_err parse_end(struct dsp_sxml *state,
	struct dsp_sxml_args *args)
{
	const char *gt, *space;
	const char *start = buffer_fromoffset(args, state->bufferpos);
	const char *end = buffer_getend(args);

	if (!(str_startswith(start, end, "</") && ISALPHA(start[2])))
		return SXML_ERROR_XMLINVALID;

	start += 2;
	gt = str_findchr(start, end, '>');
	if (gt == end)
		return SXML_ERROR_BUFFERDRY;

	space = str_find_notalnum(start, gt);
	if (str_ltrim(space, gt) != gt)
		return SXML_ERROR_XMLSTRICT;

	state_pushtoken(state, args, SXML_ENDTAG, start, space);
	return state_setpos(state, args, gt + 1);
}

static enum dsp_sxml_err parse_cdata(struct dsp_sxml *state,
	struct dsp_sxml_args *args)
{
	static const char STARTTAG[] = "<![CDATA[";
	static const char ENDTAG[] = "]]>";

	const char *bracket;
	const char *start = buffer_fromoffset(args, state->bufferpos);
	const char *end = buffer_getend(args);

	if (end - start < (int)TAG_LEN(STARTTAG))
		return SXML_ERROR_BUFFERDRY;

	if (!str_startswith(start, end, STARTTAG))
		return SXML_ERROR_XMLINVALID;

	start += TAG_LEN(STARTTAG);
	bracket = str_findstr(start, end, ENDTAG);
	if (bracket == end)
		return SXML_ERROR_BUFFERDRY;

	state_pushtoken(state, args, SXML_CDATA, start, bracket);
	return state_setpos(state, args, bracket + TAG_LEN(ENDTAG));
}

void dsp_sxml_init(struct dsp_sxml *state)
{
	state->bufferpos = 0;
	state->ntokens = 0;
	state->taglevel = 0;
}

#define ROOT_FOUND(state)	(0 < (state)->taglevel)
#define ROOT_PARSED(state)	((state)->taglevel == 0)

enum dsp_sxml_err dsp_sxml_parse(struct dsp_sxml *state, const char *buffer,
	unsigned int bufferlen, struct dsp_sxml_tok tokens[],
	unsigned int num_tokens)
{
	struct dsp_sxml temp = *state;
	const char *end = buffer + bufferlen;

	struct dsp_sxml_args args;

	args.buffer = buffer;
	args.bufferlen = bufferlen;
	args.tokens = tokens;
	args.num_tokens = num_tokens;

	/* --- */

	while (!ROOT_FOUND(&temp)) {
		enum dsp_sxml_err err;
		const char *start = buffer_fromoffset(&args, temp.bufferpos);
		const char *lt = str_ltrim(start, end);

		state_setpos(&temp, &args, lt);
		state_commit(state, &temp);

		if (end - lt < TAG_MINSIZE)
			return SXML_ERROR_BUFFERDRY;

		/* --- */

		if (*lt != '<')
			return SXML_ERROR_XMLINVALID;

		switch (lt[1]) {
		case '?':
			err = parse_instruction(&temp, &args);
			break;
		case '!':
			err = parse_doctype(&temp, &args);
			break;
		default:
			err = parse_start(&temp, &args);
			break;
		}

		if (err != SXML_SUCCESS)
			return err;

		state_commit(state, &temp);
	}

	/* --- */

	while (!ROOT_PARSED(&temp)) {
		enum dsp_sxml_err err;
		const char *start = buffer_fromoffset(&args, temp.bufferpos);
		const char *lt = str_findchr(start, end, '<');

		while (buffer_fromoffset(&args, temp.bufferpos) != lt) {
			enum dsp_sxml_err err =
				parse_characters(&temp, &args, lt);

			if (err != SXML_SUCCESS)
				return err;

			state_commit(state, &temp);
		}

		/* --- */

		if (end - lt < TAG_MINSIZE)
			return SXML_ERROR_BUFFERDRY;

		switch (lt[1]) {
		case '?':
			err = parse_instruction(&temp, &args);
			break;
		case '/':
			err = parse_end(&temp, &args);
			break;
		case '!':
			err = (lt[2] == '-') ? parse_comment(&temp, &args) :
				parse_cdata(&temp, &args);
			break;
		default:
			err = parse_start(&temp, &args);
			break;
		}

		if (err != SXML_SUCCESS)
			return err;

		state_commit(state, &temp);
	}

	return SXML_SUCCESS;
}
