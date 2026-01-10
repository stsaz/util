
#include <ffsys/path.h>
static inline void ffpath_split3_str(ffstr fullname, ffstr *path, ffstr *name, ffstr *ext)
{
	ffstr nm;
	ffpath_splitpath_str(fullname, path, &nm);
	ffpath_splitname_str(nm, name, ext);
}

/** Treat ".ext" file name as just an extension without a name */
static inline void ffpath_split3_output(ffstr fullname, ffstr *path, ffstr *name, ffstr *ext)
{
	ffstr nm;
	ffpath_splitpath_str(fullname, path, &nm);
	ffstr_rsplitby(&nm, '.', name, ext);
}

#ifdef FF_WIN
#define ffpath_rfindslash(path, len)  ffs_rfindany(path, len, "/\\", 2)

#else // UNIX:
/** Find the last slash in path. */
#define ffpath_rfindslash(path, len)  ffs_rfindchar(path, len, '/')
#endif

/** Get max. shared parent directory (without the last slash).
p1,p2: Normalized paths
dir: [output] points to segment inside `p1`
Return !=0 if the paths don't have a shared parent. */
static inline int ffpath_parent(ffstr p1, ffstr p2, ffstr *dir)
{
	size_t n = ffmin(p1.len, p2.len);
	if (n == 0)
		return -1; // empty path
	ssize_t i = ffs_cmp_n(p1.ptr, p2.ptr, n);
	if (i == 0) {
		if (p1.len == p2.len)
			goto done; // "/a" & "/a"

		const char *s = (n == p1.len) ? p2.ptr : p1.ptr;
		if (ffpath_slash(s[n]))
			goto done; // "/a" & "/a/b"

		// "/a" & "/ab" => "/a"
	} else if (i < 0) {
		n = -i - 1; // "/ab" & "/ac" => "/a"
	} else {
		n = i - 1; // "/ac" & "/ab" => "/a"
	}

	if ((i = ffpath_rfindslash(p1.ptr, n)) < 0)
		return -1;
	n = i;

done:
	ffstr_set(dir, p1.ptr, n);
	return 0;
}

/** Replace uncommon/invalid characters in file name component
Return N bytes written */
static inline ffsize ffpath_makename(char *dst, ffsize dstcap, ffstr src, char replace_char, const uint *char_bitmask)
{
	ffstr_skipchar(&src, ' ');
	ffstr_rskipchar(&src, ' ');
	ffsize len = ffmin(src.len, dstcap);

	for (ffsize i = 0;  i < len;  i++) {
		dst[i] = src.ptr[i];
		if (!ffbit_test_array32(char_bitmask, (ffbyte)src.ptr[i]))
			dst[i] = replace_char;
	}
	return len;
}

/** Return TRUE if 'parent' is a parent directory of 'file'.
Both paths must be normalized. */
static inline int path_isparent(ffstr parent, ffstr file)
{
	return (file.len > parent.len && ffpath_slash(file.ptr[parent.len])
		&& ffstr_match2(&file, &parent));
}
