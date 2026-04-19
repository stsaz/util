/**
Simon Zolin, 2021 */

#include "zstd-ff.h"
#include <zstd.h>

const char* zstd_error(int code)
{
	return ZSTD_getErrorName((ssize_t)code);
}


struct zstd_decoder {
	ZSTD_DStream *z;
};

int zstd_decode_init(zstd_decoder **pdec, zstd_dec_conf *conf)
{
	struct zstd_decoder *dec = *pdec;
	if (dec == NULL) {
		if (NULL == (dec = calloc(1, sizeof(struct zstd_decoder))))
			return -1;
		if (NULL == (dec->z = ZSTD_createDStream())) {
			free(dec);
			return -1;
		}
	}

	ZSTD_DCtx_reset(dec->z, ZSTD_reset_session_and_parameters);

	if (conf != NULL) {
		if (conf->max_mem_kb != 0) {
			unsigned shift = 32 - (__builtin_clz(conf->max_mem_kb * 1024)+1);
			(void)ZSTD_DCtx_setParameter(dec->z, ZSTD_d_windowLogMax, shift);
		}
	}

	*pdec = dec;
	return 0;
}

void zstd_decode_free(zstd_decoder *dec)
{
	if (dec == NULL)
		return;

	ZSTD_freeDStream(dec->z);
	free(dec);
}

int zstd_decode(zstd_decoder *dec, zstd_buf *_in, zstd_buf *_out)
{
	ZSTD_inBuffer in = {
		.src = _in->ptr,
		.size = _in->len,
	};
	ZSTD_outBuffer out = {
		.dst = _out->ptr,
		.size = _out->len,
	};
	int r = (ssize_t)ZSTD_decompressStream(dec->z, &out, &in);
	_in->pos = in.pos;
	_out->pos = out.pos;
	return r;
}


struct zstd_encoder {
	ZSTD_CStream *z;
	int done;
};

int zstd_encode_init(zstd_encoder **penc, zstd_enc_conf *conf)
{
	struct zstd_encoder *enc = *penc;
	if (enc == NULL) {
		if (NULL == (enc = calloc(1, sizeof(struct zstd_encoder))))
			return -1;
		if (NULL == (enc->z = ZSTD_createCStream())) {
			free(enc);
			return -1;
		}
	}

	ZSTD_CCtx_reset(enc->z, ZSTD_reset_session_and_parameters);

	if (conf != NULL) {
		(void)ZSTD_CCtx_setParameter(enc->z, ZSTD_c_compressionLevel, conf->level);

		if (conf->workers == 0)
			conf->workers = 1;
		(void)ZSTD_CCtx_setParameter(enc->z, ZSTD_c_nbWorkers, conf->workers);

		conf->max_block_size = ZSTD_CStreamOutSize();
	}

	enc->done = 0;
	*penc = enc;
	return 0;
}

void zstd_encode_free(zstd_encoder *enc)
{
	if (enc == NULL)
		return;

	ZSTD_freeCStream(enc->z);
	free(enc);
}

int zstd_encode(zstd_encoder *enc, zstd_buf *_in, zstd_buf *_out, unsigned int flags)
{
	if (enc->done) {
		_in->pos = 0;
		_out->pos = 0;
		return 0;
	}

	ZSTD_inBuffer in = {
		.src = _in->ptr,
		.size = _in->len,
	};
	ZSTD_outBuffer out = {
		.dst = _out->ptr,
		.size = _out->len,
	};

	unsigned f = ZSTD_e_continue;
	if (flags & ZSTD_FFLUSH)
		f = ZSTD_e_flush;
	if (flags & ZSTD_FFINISH)
		f = ZSTD_e_end;

	int r;
	for (;;) {
		r = (ssize_t)ZSTD_compressStream2(enc->z, &out, &in, f);
		if (out.pos == 0 && out.pos < out.size && in.pos < in.size)
			continue; // zstd consumed some input but didn't return any output data
		break;
	}

	_in->pos = in.pos;
	_out->pos = out.pos;
	if (f == ZSTD_e_end && r == 0 && in.pos == in.size)
		enc->done = 1; // zstd consumed all input and returned all output
	return r;
}
