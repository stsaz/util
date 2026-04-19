/** libz interface for FF.
Simon Zolin, 2016 */

#pragma once

#include <stdlib.h>


#ifdef WIN32
#define EXP  __declspec(dllexport)
#else
#define EXP  __attribute__((visibility("default")))
#endif

typedef struct z_ctx z_ctx;

#ifndef Z_EXP
/* zlib.h */
enum Z_FLAGS {
	Z_SYNC_FLUSH = 2,
	Z_FINISH = 4,
};

enum {
	Z_BEST_COMPRESSION = 9,
};
#endif

typedef void* (*z_alloc_func) (void* opaque, unsigned int items, unsigned int size);
typedef void (*z_free_func) (void* opaque, void* address);

/**
All parameters are optional. */
typedef struct z_conf {
	int level; //1..9
	int mem; //2..256 (in kbytes)

	z_alloc_func zalloc;
	z_free_func zfree;
	void *opaque;
} z_conf;

enum Z_ERR {
	Z_DONE = -2,
	//any other code is an error
};

#ifdef __cplusplus
extern "C" {
#endif

EXP const char* z_errstr(z_ctx *z);

/**
Return 0 on success. */
EXP int z_deflate_init(z_ctx **z, z_conf *conf);

EXP void z_deflate_free(z_ctx *z);

EXP void z_deflate_reset(z_ctx *z);

/**
flags: enum Z_FLAGS
Return the number of bytes written;
	0 if more data is needed;
	enum Z_ERR on error */
EXP int z_deflate(z_ctx *z, const char *data, size_t *len, char *dst, size_t cap, unsigned int flags);


/**
Return 0 on success. */
EXP int z_inflate_init(z_ctx **z, z_conf *conf);

EXP void z_inflate_free(z_ctx *z);

EXP void z_inflate_reset(z_ctx *z);

/**
flags: enum Z_FLAGS
Return the number of bytes written;
	0 if more data is needed;
	enum Z_ERR on error */
EXP int z_inflate(z_ctx *z, const char *data, size_t *len, char *dst, size_t cap, unsigned int flags);

#ifdef __cplusplus
}
#endif

#undef EXP
