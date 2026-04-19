/** ffpack: libzstd wrapper
Simon Zolin, 2021 */

#pragma once
#include <stdlib.h>

#ifdef WIN32
#define EXP  __declspec(dllexport)
#else
#define EXP  __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

EXP const char* zstd_error(int code);

typedef struct zstd_buf {
	void *ptr;
	size_t len;
	size_t pos;
} zstd_buf;

static inline void zstd_buf_set(zstd_buf *b, void *ptr, size_t len)
{
	b->ptr = ptr;
	b->len = len;
}


typedef struct zstd_decoder zstd_decoder;

typedef struct zstd_dec_conf {
	unsigned int max_mem_kb; // default:134mb
} zstd_dec_conf;

/** Initialize decoder
dec: NULL: create new context;
 !=NULL: reset existing context
conf: settings (optional)
Return 0 on success */
EXP int zstd_decode_init(zstd_decoder **dec, zstd_dec_conf *conf);

EXP void zstd_decode_free(zstd_decoder *dec);

/** Decode data
Return 0: success, complete block;
 >0: success, incomplete block;
 <0: error */
EXP int zstd_decode(zstd_decoder *dec, zstd_buf *in, zstd_buf *out);


typedef struct zstd_encoder zstd_encoder;

typedef struct zstd_enc_conf {
	int level; // -7..22; default:3
	unsigned int workers; // default:1
	size_t max_block_size;
} zstd_enc_conf;

/**
enc: NULL: create new context;
 !=NULL: reset existing context
conf: settings (optional)
Return 0 on success */
EXP int zstd_encode_init(zstd_encoder **enc, zstd_enc_conf *conf);

EXP void zstd_encode_free(zstd_encoder *enc);

enum ZSTD_FLAGS {
	ZSTD_FFLUSH = 1,
	ZSTD_FFINISH = 2,
};

/**
flags: enum ZSTD_FLAGS
Return 0: success, complete block;
 >0: success, incomplete block;
 <0: error */
EXP int zstd_encode(zstd_encoder *enc, zstd_buf *in, zstd_buf *out, unsigned int flags);

#ifdef __cplusplus
}
#endif

#undef EXP
