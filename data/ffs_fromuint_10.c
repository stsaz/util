
static inline ffuint ffs_fromuint_10(ffuint i, char *dst, ffsize cap)
{
	char buf[32];
	ffuint k = sizeof(buf);

	ffuint i4 = i;
	do {
		buf[--k] = (ffbyte)(i4 % 10 + '0');
		i4 /= 10;
	} while (i4 != 0);

	ffuint len = sizeof(buf) - k;
	if (len > cap)
		return 0; // not enough space
	ffmem_copy(dst, buf + k, len);
	return len;
}
