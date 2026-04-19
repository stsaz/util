/** liblzma interface for FF.
Simon Zolin, 2016 */

#pragma once

#include <stdlib.h>


#ifdef WIN32
#define EXP  __declspec(dllexport)
#else
#define EXP  __attribute__((visibility("default")))
#endif

typedef size_t (*lzma_simple_coder_t)(void *ctx, char *buf, size_t size);

typedef struct lzma_coder_ctx {
	unsigned int method;
	unsigned int ctxsize;
	unsigned int max_unprocessed;
	lzma_simple_coder_t simple_decoder;
} lzma_coder_ctx;

typedef struct lzma_decoder lzma_decoder;

enum LZMA_FILT {
	LZMA_FILT_LZMA1 = 0x4000000000000001,
	LZMA_FILT_LZMA2 = 0x21,
	LZMA_FILT_X86 = 0x04,
};

typedef struct lzma_filter_props {
	unsigned long long id; //enum LZMA_FILT
	unsigned long long prop_len;
	const char *props;
} lzma_filter_props;

enum LZMA_ERR {
	LZMA_DONE = -0x100,
	//any other code is an error
};

#ifdef __cplusplus
extern "C" {
#endif

EXP const char* lzma_errstr(int e);


/** Initialize block decoder.
@fi: filter properties from block header.
Return 0 on success. */
EXP int lzma_decode_init(lzma_decoder **dec, unsigned int check_method, const lzma_filter_props *fi, unsigned int nfilt);

EXP void lzma_decode_free(lzma_decoder *dec);

/** Get the best output buffer capacity. */
EXP size_t lzma_decode_bufsize(lzma_decoder *dec, size_t in_bufsize);

/** Decode 1 block.
Return the number of bytes written;  0 if more data is needed;  enum LZMA_ERR on error. */
EXP int lzma_decode(lzma_decoder *dec, const char *data, size_t *len, char *dst, size_t cap);

#ifdef __cplusplus
}
#endif

#undef EXP
