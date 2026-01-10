
enum FILE_CMP_F {
	FILE_CMP_FAST = 1, // Compare just 3 chunks of data (beginning, middle, end)
};

/** Compare files content.
window: buffer size
flags: enum FILE_CMP_F'
Return 0 if equal;
 1 if not equal;
 <0 on error */
static inline int file_cmp(const char *fn1, const char *fn2, ffsize window, ffuint flags)
{
	int rc = -1;
	char *buf = NULL;
	fffd f1 = FFFILE_NULL, f2 = FFFILE_NULL;
	ffssize r1, r2;
	ffuint64 sz, off = 0, offsets[3];
	ffuint i = 0;

	if (FFFILE_NULL == (f1 = fffile_open(fn1, FFFILE_READONLY))
		|| FFFILE_NULL == (f2 = fffile_open(fn2, FFFILE_READONLY)))
		goto end;

	sz = fffile_size(f1);
	if (sz != (ffuint64)fffile_size(f2)) {
		rc = 1;
		goto end;
	}
	offsets[0] = 0;
	offsets[1] = (sz > window) ? sz / 2 : ~0ULL;
	offsets[2] = sz - window;

	if (!(buf = (char*)ffmem_align(2 * window, 4096)))
		goto end;

	for (;;) {
		if (flags & FILE_CMP_FAST) {
			if (i == 3)
				break;
			off = offsets[i++];
			if ((ffint64)off < 0)
				break;
		}

		r1 = fffile_readat(f1, buf, window, off);
		r2 = fffile_readat(f2, buf + window, window, off);
		off += window;
		if (r1 < 0 || r2 < 0)
			goto end;
		if (r1 != r2 || ffmem_cmp(buf, buf + window, r1)) {
			rc = 1;
			goto end;
		}
		if ((ffsize)r1 < window)
			break;
	}

	rc = 0;

end:
	fffile_close(f1);
	fffile_close(f2);
	ffmem_free(buf);
	return rc;
}

static inline int file_copydata(fffd src, ffuint64 offsrc, fffd dst, ffuint64 offdst, ffuint64 size)
{
	int rc = -1, r;
	ffvec v = {};
	ffvec_alloc(&v, 8*1024*1024, 1);

	while (size != 0) {
		if (0 >= (r = fffile_readat(src, v.ptr, ffmin(size, v.cap), offsrc)))
			goto end;
		offsrc += r;
		if (0 > (r = fffile_writeat(dst, v.ptr, r, offdst)))
			goto end;
		offdst += r;
		size -= r;
	}

	rc = 0;

end:
	ffvec_free(&v);
	return rc;
}
