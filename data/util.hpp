/** C++ utility functions
2023, Simon Zolin */

#include <ffsys/path.h>
#include <ffsys/time.h>
#include <ffsys/file.h>
#include <ffbase/string.h>
#include <ffbase/vector.h>
#include <new>

struct xxstr : ffstr {
	xxstr() { ptr = NULL;  len = 0; }
	xxstr(ffstr s) { ptr = s.ptr;  len = s.len; }
	xxstr(const char *sz) { ptr = (char*)sz;  len = ffsz_len(sz); }
	xxstr(const char *s, size_t n) { ptr = (char*)s;  len = n; }
	void	set(const char *s, size_t n) { ptr = (char*)s;  len = n; }
	void	operator=(const char *sz) { ptr = (char*)sz, len = ffsz_len(sz); }
	void	reset() { ptr = NULL;  len = 0; }
	xxstr&	shift(ffsize n) { ffstr_shift(this, n); return *this; }
	void	free() { ffmem_free(ptr);  ptr = NULL;  len = 0; }

	bool	equals(xxstr s) const { return ffstr_eq2(this, &s); }
	bool	equals_i(xxstr s) const { return ffstr_ieq(this, s.ptr, s.len); }
	char	at(size_t i) const { FF_ASSERT(i < len); return ptr[i]; }
	ffssize	split_by(char by, xxstr *left, xxstr *right) const { return ffstr_splitby(this, by, left, right); }
	ffssize	split_at(ffssize index, xxstr *left, xxstr *right) const { return ffstr_split(this, index, left, right); }
	ffssize	find_char(char c) const { return ffstr_findchar(this, c); }
	ffssize	find_str(xxstr s) const { return ffstr_findstr(this, &s); }
	ffssize	find_str_i(xxstr s) const { return ffstr_ifindstr(this, &s); }
	ffssize	match_f(const char *fmt, ...) const {
		va_list va;
		va_start(va, fmt);
		ffssize r = ffstr_matchfmtv(this, fmt, va);
		va_end(va);
		return r;
	}
	short int16(short _default) const {
		short n;
		if (!ffstr_toint(this, &n, FFS_INT16 | FFS_INTSIGN))
			n = _default;
		return n;
	}
	ushort uint16(ushort _default) const {
		ushort n;
		if (!ffstr_toint(this, &n, FFS_INT16))
			n = _default;
		return n;
	}
	uint uint32(uint _default) const {
		uint n;
		if (!ffstr_toint(this, &n, FFS_INT32))
			n = _default;
		return n;
	}
};

template<uint N> struct xxstr_buf : ffstr {
	char buf[N];
	xxstr_buf() { ptr = buf;  len = 0; }
	const char* zfmt(const char *fmt, ...) {
		va_list va;
		va_start(va, fmt);
		len = ffsz_formatv(ptr, N, fmt, va);
		va_end(va);
		return ptr;
	}
	ffstr& fmt(const char *fmt, ...) {
		va_list va;
		va_start(va, fmt);
		len = ffs_formatv(ptr, N, fmt, va);
		va_end(va);
		return *this;
	}
	xxstr_buf<N>& add_char(char ch) {
		ffstr_addchar(this, N - len, ch);
		return *this;
	}
};

template<uint N> struct xxwstr_buf {
	size_t len;
	wchar_t *ptr;
	wchar_t buf[N];
	xxwstr_buf() { ptr = buf;  len = 0; }
	const wchar_t* utow(const char *s) {
		len = N;
		return (ptr = ffs_utow(buf, &len, s, -1));
	}
};

struct xxvec : ffvec {
	xxvec() { ffvec_null(this); }
	xxvec(ffstr s) {
		ptr = s.ptr, len = s.len, cap = !(s.len == 0 && s.ptr != NULL) ? s.len : 1;
	}
	xxvec(const char *sz) {
		ptr = (char*)sz, len = (ptr != NULL) ? ffsz_len(sz) : 0, cap = (ptr != NULL) ? len + 1 : 0;
	}
	xxvec(ffslice s) {
		ptr = s.ptr, len = s.len, cap = !(s.len == 0 && s.ptr != NULL) ? s.len : 1;
	}
	~xxvec() { ffvec_free(this); }
	void free() { ffvec_free(this); }
	void reset() { ffvec_null(this); }
	xxvec& set(const char *sz) {
		ffvec_free(this);
		ptr = (void*)sz, len = ffsz_len(sz);
		return *this;
	}
	xxvec& acquire(ffstr s) {
		ffvec_free(this);
		ptr = s.ptr, len = s.len, cap = !(s.len == 0 && s.ptr != NULL) ? s.len : 1;
		return *this;
	}
	xxvec& acquire(const char *sz) {
		ffvec_free(this);
		ptr = (char*)sz, len = (ptr) ? ffsz_len(sz) : 0, cap = (ptr) ? len + 1 : 0;
		return *this;
	}
	xxvec& copy(ffstr s) {
		len = 0;
		ffvec_addstr(this, &s);
		return *this;
	}
	xxvec& add(xxstr s) {
		ffvec_addstr(this, &s);
		return *this;
	}
	xxvec& add_f(const char *fmt, ...) {
		va_list va;
		va_start(va, fmt);
		ffstr_growfmtv((ffstr*)this, &cap, fmt, va);
		va_end(va);
		return *this;
	}
	template<class T> xxvec& rm_swap(T *iter, ffsize n) {
		ffslice_rmswap((ffslice*)this, iter - (T*)ptr, n, sizeof(T));
		return *this;
	}
	template<class T> xxvec& rm_swap(ffsize i, ffsize n) {
		ffslice_rmswap((ffslice*)this, i, n, sizeof(T));
		return *this;
	}
	template<class T> T* at(ffsize i) { FF_ASSERT(i < len); return ffslice_itemT(this, i, T); }
	template<class T> T* alloc(ffsize n) { return ffvec_allocT(this, n, T); }
	template<class T> T* push() { return ffvec_pushT(this, T); }
	template<class T> T* push_z() { return ffvec_zpushT(this, T); }
	const xxstr& str() const { return *(xxstr*)this; }
	const ffslice& slice() const { return *(ffslice*)this; }
	char* sz() { return (char*)ptr; }
	char* strz() {
		if (len && ((char*)ptr)[len-1] != '\0')
			ffvec_addchar(this, '\0');
		return (char*)ptr;
	}
};

/** Take rvalue */
template<typename T> T& xxrval(T &&t) { return t; }

struct xxpath {
	xxstr data;
	xxpath(const char *sz) : data(sz) {}
	xxpath(ffstr s) : data(s) {}
	ffstr	path() const {
		ffstr path;
		ffpath_splitpath(data.ptr, data.len, &path, NULL);
		return path;
	}
	ffstr	name() const {
		ffstr name;
		ffpath_splitpath(data.ptr, data.len, NULL, &name);
		return name;
	}
};

struct xxtime : fftime {
	xxtime() { sec = 0, nsec = 0; }
	xxtime(const fftime &t) { sec = t.sec, nsec = t.nsec; }
	xxtime(const ffdatetime &dt) { fftime_join1(this, &dt); }
	xxtime&	since1970() {
		sec -= FFTIME_1970_SECONDS;
		return *this;
	}
	ffuint64 to_msec() const { return fftime_to_msec(this); }
	ffuint64 to_usec() const { return fftime_to_usec(this); }
};

struct xxfileinfo {
	fffileinfo info = {};

	bool	dir() const { return fffile_isdir(fffileinfo_attr(&info)); }
	xxtime	mtime() const { return fffileinfo_mtime(&info); }
	xxtime	mtime1() const { return fffileinfo_mtime1(&info); }
	uint64	size() const { return fffileinfo_size(&info); }
	uint	attr() const { return fffileinfo_attr(&info); }
};

struct xxfile {
	fffd f = FFFILE_NULL;

	~xxfile() { fffile_close(f); }
	xxfile& open(const char *name, uint flags) {
		f = fffile_open(name, flags);
		return *this;
	}
	ffssize read(void *buf, ffsize size) { return fffile_read(f, buf, size); }
	ffint64 seek(ffuint64 pos, int method) { return fffile_seek(f, pos, method); }

	bool null() const { return f == FFFILE_NULL; }
	xxfileinfo info() const {
		xxfileinfo fi;
		fffile_info(f, &fi.info);
		return fi;
	}

	static xxfileinfo info(const char *fn) {
		xxfileinfo fi;
		fffile_info_path(fn, &fi.info);
		return fi;
	}
};
