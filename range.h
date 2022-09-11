/** alphahttpd: memory region pointer
2022, Simon Zolin
*/

// ffstr requires 16 bytes; range16 requires only 4 bytes
typedef struct range16 {
	ffushort off, len;
} range16;

static inline void range16_set(range16 *r, uint off, uint len)
{
	r->off = off;
	r->len = len;
}

static inline ffstr range16_tostr(range16 *r, const char *base)
{
	ffstr s = FFSTR_INITN(base + r->off, r->len);
	return s;
}
