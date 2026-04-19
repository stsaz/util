/**
Simon Zolin, 2016 */

#include "lzma-ff.h"
#include <common.h>
#include <lzma.h>
#include <common/block_decoder.h>


static const char* const errs[] = {
	"LZMA_OK",
	"LZMA_STREAM_END",
	"LZMA_NO_CHECK",
	"LZMA_UNSUPPORTED_CHECK",
	"LZMA_GET_CHECK",
	"LZMA_MEM_ERROR",
	"LZMA_MEMLIMIT_ERROR",
	"LZMA_FORMAT_ERROR",
	"LZMA_OPTIONS_ERROR",
	"LZMA_DATA_ERROR",
	"LZMA_BUF_ERROR",
	"LZMA_PROG_ERROR",
};

const char* lzma_errstr(int e)
{
	e = -e;
	if ((unsigned int)e > sizeof(errs) / sizeof(*errs))
		return "unknown error";
	return errs[e];
}


struct lzma_decoder {
	lzma_next_coder blk_dec;
	lzma_block blk;

	const lzma_coder_ctx *coder;
	lzma_simple_coder_t simple_decoder;
	char buf[8];
	unsigned int nbuf;
	char ctx[0];
};

extern const lzma_coder_ctx lzma_x86_ctx;

static const lzma_coder_ctx* coders[] = {
	&lzma_x86_ctx,
};

static const lzma_coder_ctx* find_coder(unsigned int method)
{
	for (unsigned int i = 0;  i != sizeof(coders) / sizeof(*coders);  i++) {
		if (method == coders[i]->method)
			return coders[i];
	}
	return NULL;
}

int lzma_decode_init(lzma_decoder **pdec, unsigned int check_method, const lzma_filter_props *fp, unsigned int nfilt)
{
	const lzma_coder_ctx *coder;

	if (nfilt == 1 && NULL != (coder = find_coder(fp[0].id))) {

		int r;
		lzma_decoder *dec;
		if (NULL == (dec = calloc(1, sizeof(lzma_decoder) + coder->ctxsize))) {
			r = LZMA_MEM_ERROR;
			goto err;
		}
		dec->simple_decoder = coder->simple_decoder;
		dec->coder = coder;

		*pdec = dec;
		return 0;

err:
		free(dec);
		return -r;
	}

	int r;
	unsigned int i;

	lzma_filter filters[LZMA_FILTERS_MAX + 1] = {0};
	for (i = 0;  i != nfilt;  i++) {
		filters[i].id = fp[i].id;
		filters[i].options = NULL;
		r = lzma_properties_decode(&filters[i], NULL, (void*)fp[i].props, fp[i].prop_len);
		if (r != LZMA_OK)
			goto end;
	}
	filters[i].id = LZMA_VLI_UNKNOWN;
	filters[i].options = NULL;

	lzma_block blk = {0};
	blk.header_size = LZMA_BLOCK_HEADER_SIZE_MIN;
	blk.check = check_method;
	blk.version = 1;
	blk.ignore_check = 0;
	blk.compressed_size = LZMA_VLI_UNKNOWN;
	blk.uncompressed_size = LZMA_VLI_UNKNOWN;
	blk.filters = filters;

	lzma_decoder *dec;
	if (NULL == (dec = calloc(1, sizeof(lzma_decoder)))) {
		r = LZMA_MEM_ERROR;
		goto end;
	}

	lzma_next_coder blk_dec = LZMA_NEXT_CODER_INIT;
	dec->blk_dec = blk_dec;
	dec->blk = blk;
	r = lzma_block_decoder_init(&dec->blk_dec, NULL, &dec->blk);

	if (r != LZMA_OK) {
		lzma_decode_free(dec);
		goto end;
	}
	*pdec = dec;
	r = 0;

end:
	for (i = 0;  i != nfilt;  i++)
		lzma_free(filters[i].options, NULL);
	return -r;
}

void lzma_decode_free(lzma_decoder *dec)
{
	if (dec == NULL)
		return;
	lzma_next_end(&dec->blk_dec, NULL);
	free(dec);
}

size_t lzma_decode_bufsize(lzma_decoder *dec, size_t in_bufsize)
{
	if (dec->coder != NULL)
		return in_bufsize + dec->coder->max_unprocessed;
	return in_bufsize;
}

static size_t ffmin(size_t a, size_t b)
{
	return (a < b) ? a : b;
}

/*
. Copy stored input data to output buffer
. Append input data to output buffer
. Call coder
. Store unprocessed input data
*/
static int call_simple_decoder(lzma_decoder *dec, const char *data, size_t *len, char *dst, size_t cap)
{
	size_t n, valid = 0, unprocessed;

	if (cap < dec->nbuf)
		return -LZMA_PROG_ERROR;
	n = ffmin(dec->nbuf, cap);
	memcpy(dst, dec->buf, n);
	valid += n;

	if (*len == (size_t)-1) {
		if (dec->nbuf == 0)
			return LZMA_DONE;
		dec->nbuf = 0;
		*len = 0;
		return valid;
	}

	n = ffmin(*len, cap - valid);
	memcpy(dst + valid, data, n);
	valid += n;
	*len = n;

	n = dec->simple_decoder(dec->ctx, (void*)dst, valid);

	unprocessed = valid - n;
	if (unprocessed > sizeof(dec->buf))
		return -LZMA_PROG_ERROR;
	memcpy(dec->buf, dst + n, unprocessed);
	dec->nbuf = unprocessed;

	return n;
}

int lzma_decode(lzma_decoder *dec, const char *data, size_t *len, char *dst, size_t cap)
{
	if (dec->simple_decoder != NULL)
		return call_simple_decoder(dec, data, len, dst, cap);

	size_t inpos = 0, outpos = 0;
	int r = dec->blk_dec.code(dec->blk_dec.coder, NULL
		, (void*)data, &inpos, *len, (void*)dst, &outpos, cap
		, LZMA_FINISH);
	*len = inpos;

	switch (r) {
	case LZMA_STREAM_END:
		if (outpos == 0) {
			lzma_next_end(&dec->blk_dec, NULL);
			return LZMA_DONE;
		}
		// fallthrough

	case LZMA_OK:
		return outpos;
	}

	return -r;
}

lzma_ret lzma_stream_decoder_init(lzma_next_coder *next, const lzma_allocator *allocator, uint64_t memlimit, uint32_t flags){}
