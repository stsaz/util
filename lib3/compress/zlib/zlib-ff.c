/**
Simon Zolin, 2016 */

#include "zlib-ff.h"
#include <zlib.h>


struct z_ctx {
	z_stream stm;
};


#if !defined _MSC_VER && !defined __MINGW64__
#define bit_ffs32(i)  __builtin_ffs(i)

#else
#include <windows.h>
static unsigned int bit_ffs32(unsigned int i)
{
	DWORD idx;
	if (!BitScanForward(&idx, i))
		return 0;
	return idx + 1;
}
#endif


const char* z_errstr(z_ctx *z)
{
	return (z->stm.msg != NULL) ? z->stm.msg : "";
}


/** Get window bits number (8..15).
window = 1 << (window_bits+2)
@window: 1k..128k */
#define wbits(window) \
	((bit_ffs32(window) - 1) - 2)

/** Get memory level number (1..9).
mem = 1 << (mem_level+9)
@mem: 1k..256k */
#define memlevel(mem) \
	((bit_ffs32(mem) - 1) - 9)

int z_deflate_init(z_ctx **pz, z_conf *conf)
{
	z_ctx *z;

	if (NULL == (z = calloc(1, sizeof(z_ctx))))
		return -1;

	z->stm.zalloc = conf->zalloc;
	z->stm.zfree = conf->zfree;
	z->stm.opaque = conf->opaque;
	if (conf->level == 0)
		conf->level = 6;
	if (conf->mem == 0)
		conf->mem = 256;
	if (0 != deflateInit2(&z->stm, conf->level, Z_DEFLATED, -wbits(conf->mem * 1024 / 2), memlevel(conf->mem * 1024 / 2), Z_DEFAULT_STRATEGY)) {
		free(z);
		return -1;
	}

	*pz = z;
	return 0;
}

void z_deflate_free(z_ctx *z)
{
	deflateEnd(&z->stm);
	free(z);
}

void z_deflate_reset(z_ctx *z)
{
	deflateReset(&z->stm);
}

int z_deflate(z_ctx *z, const char *data, size_t *len, char *dst, size_t cap, unsigned int flags)
{
	z->stm.next_in = (void*)data;
	z->stm.avail_in = *len;
	z->stm.next_out = dst;
	z->stm.avail_out = cap;
	int r = deflate(&z->stm, flags);
	*len -= z->stm.avail_in;
	cap -= z->stm.avail_out;

	switch (r) {
	case Z_STREAM_END:
		if (cap == 0)
			return Z_DONE;
		// break

	case Z_OK:
		return cap;

	case Z_BUF_ERROR:
		if (*len == 0)
			return 0;
	}
	return -1;
}


int z_inflate_init(z_ctx **pz, z_conf *conf)
{
	z_ctx *z;

	if (NULL == (z = calloc(1, sizeof(z_ctx))))
		return -1;

	if (conf) {
		z->stm.zalloc = conf->zalloc;
		z->stm.zfree = conf->zfree;
		z->stm.opaque = conf->opaque;
	}

	if (0 != inflateInit2(&z->stm, -15)) {
		free(z);
		return -1;
	}

	*pz = z;
	return 0;
}

void z_inflate_free(z_ctx *z)
{
	if (z == NULL)
		return;
	inflateEnd(&z->stm);
	free(z);
}

void z_inflate_reset(z_ctx *z)
{
	inflateReset(&z->stm);
}

int z_inflate(z_ctx *z, const char *data, size_t *len, char *dst, size_t cap, unsigned int flags)
{
	z->stm.next_in = (void*)data;
	z->stm.avail_in = *len;
	z->stm.next_out = dst;
	z->stm.avail_out = cap;
	int r = inflate(&z->stm, flags);
	*len -= z->stm.avail_in;
	cap -= z->stm.avail_out;

	switch (r) {
	case Z_STREAM_END:
		if (cap == 0)
			return Z_DONE;
		// break

	case Z_OK:
		return cap;

	case Z_BUF_ERROR:
		if (*len == 0)
			return 0;
	}
	return -1;
}
