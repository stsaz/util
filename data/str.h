
static inline char* ffstr_dup_str0(ffstr *dst, ffstr src)
{
	if (src.len == 0) {
		FF_ASSERT(dst->ptr == NULL);
		dst->ptr = NULL;
		dst->len = 0;
		return NULL;
	}

	return ffstr_dup(dst, src.ptr, src.len);
}

static inline char* ffstrz_dup_str0(ffstr *dst, ffstr src)
{
	if (src.len == 0) {
		FF_ASSERT(dst->ptr == NULL);
		dst->ptr = NULL;
		dst->len = 0;
		return NULL;
	}

	if (NULL == ffstr_alloc(dst, src.len + 1))
		return NULL;
	*(char*)ffmem_copy(dst->ptr, src.ptr, src.len) = '\0';
	dst->len = src.len;
	return dst->ptr;
}

/** Compare strings.
Return
	* 0 if equal
	* position +1 where the strings differ */
static inline ffssize ffs_cmp_n(const char *a, const char *b, ffsize n)
{
	for (ffsize i = 0;  i < n;  i++) {
		if (a[i] != b[i])
			return ((ffbyte)a[i] < (ffbyte)b[i]) ? -(ffssize)(i+1) : (ffssize)(i+1);
	}
	return 0;
}

static inline ffssize ffcharr_findsorted_padding(const void *ar, ffsize n, ffsize elsize, ffsize padding, const char *search, ffsize search_len)
{
	if (search_len > elsize)
		return -1; // the string's too large for this array

	ffsize start = 0;
	while (start != n) {
		ffsize i = start + (n - start) / 2;
		const char *ptr = (char*)ar + i * (elsize + padding);
		int r = ffmem_cmp(search, ptr, search_len);

		if (r == 0
			&& search_len != elsize
			&& ptr[search_len] != '\0')
			r = -1; // found "01" in {0,1,2}

		if (r == 0)
			return i;
		else if (r < 0)
			n = i;
		else
			start = i + 1;
	}
	return -1;
}

static inline int ffbit_test_array32(const uint *ar, uint bit)
{
	return ffbit_test32(&ar[bit / 32], bit % 32);
}


/** Free aligned memory region. */
static inline void ffvec_free_align(ffvec *v)
{
	if (v->cap != 0) {
		FF_ASSERT(v->ptr != NULL);
		FF_ASSERT(v->len <= v->cap);
		ffmem_alignfree(v->ptr);
		v->cap = 0;
	}
	v->ptr = NULL;
	v->len = 0;
}

/** Allocate aligned memory region; call ffvec_free_align() to free.
WARNING: don't call ffvec_free() */
static inline void* ffvec_alloc_align(ffvec *v, ffsize n, ffsize align, ffsize elsize)
{
	ffvec_free_align(v);

	if (n == 0)
		n = 1;
	ffsize bytes;
	if (__builtin_mul_overflow(n, elsize, &bytes))
		return NULL;

	if (NULL == (v->ptr = ffmem_align(bytes, align)))
		return NULL;

	v->cap = n;
	return v->ptr;
}

#define ffvec_alloc_alignT(v, n, align, T)  ((T*)ffvec_alloc_align(v, n, align, sizeof(T)))


/** Process input string of the format "...text...$var...text...".
out: either text chunk or variable name */
static inline int ffstr_var_next(ffstr *in, ffstr *out, char c)
{
	if (in->ptr[0] != c) {
		ffssize pos = ffstr_findchar(in, c);
		if (pos < 0)
			pos = in->len;
		ffstr_set(out, in->ptr, pos);
		ffstr_shift(in, pos);
		return 't';
	}

	ffstr_shift(in, 1);
	ffssize i = ffs_skip_ranges(in->ptr, in->len, "\x30\x39\x41\x5a\x5f\x5f\x61\x7a", 8); // "0-9A-Z_a-z"
	if (i < 0)
		i = in->len;
	ffstr_set(out, in->ptr - 1, i + 1);
	ffstr_shift(in, i);
	return 'v';
}
