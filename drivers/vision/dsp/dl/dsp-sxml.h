/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DL_DSP_SXML_H__
#define __DL_DSP_SXML_H__

enum dsp_sxml_err {
	SXML_ERROR_XMLINVALID = -1,
	SXML_SUCCESS = 0,
	SXML_ERROR_BUFFERDRY = 1,
	SXML_ERROR_TOKENSFULL = 2
};

struct dsp_sxml {
	unsigned int bufferpos;
	unsigned int ntokens;
	unsigned int taglevel;
};

enum dsp_sxml_type {
	SXML_STARTTAG,
	SXML_ENDTAG,

	SXML_CHARACTER,
	SXML_CDATA,

	SXML_INSTRUCTION,
	SXML_DOCTYPE,
	SXML_COMMENT
};

struct dsp_sxml_tok {
	unsigned short type;
	unsigned short size;

	unsigned int startpos;
	unsigned int endpos;
};

void dsp_sxml_init(struct dsp_sxml *parser);
enum dsp_sxml_err dsp_sxml_parse(struct dsp_sxml *parser, const char *buffer,
	unsigned int bufferlen, struct dsp_sxml_tok *tokens,
	unsigned int num_tokens);

#endif
